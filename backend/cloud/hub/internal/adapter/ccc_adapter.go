package adapter

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"time"

	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
	"go.uber.org/zap"
)

// CCCAdapter CCC协议适配器 (Apple/Samsung)
// 对接Apple Wallet / Samsung Pass数字钥匙服务
type CCCAdapter struct {
	vendor         string
	logger         *zap.Logger
	mailboxCreator MailboxCreator // 可选：用于创建 Mailbox 做 payload 中继
	httpClient     *http.Client   // 厂商API HTTP客户端 (Apple/Samsung 私有API)
	baseURL        string         // 厂商API基础URL
	demoMode       bool           // 仅测试: 允许合成占位密钥（生产必须为 false）
	// hsm        hsm.Client    // HSM密钥操作
}

func NewCCCAdapter(vendor string, logger *zap.Logger) *CCCAdapter {
	return &CCCAdapter{
		vendor: vendor,
		logger: logger.With(zap.String("vendor", vendor), zap.String("protocol", "ccc_dk3")),
	}
}

// NewCCCAdapterInDemoMode 仅供测试: 厂商后端不可用时合成占位密钥。
// SECURITY: 生产环境禁止使用 — 占位密钥会让所有车辆共用同一"密钥"。
func NewCCCAdapterInDemoMode(vendor string, logger *zap.Logger) *CCCAdapter {
	a := NewCCCAdapter(vendor, logger)
	a.demoMode = true
	return a
}

func (a *CCCAdapter) WithMailboxCreator(mc MailboxCreator) *CCCAdapter {
	a.mailboxCreator = mc
	return a
}

func (a *CCCAdapter) WithHTTPClient(client *http.Client, baseURL string) *CCCAdapter {
	a.httpClient = client
	a.baseURL = baseURL
	return a
}

func (a *CCCAdapter) Vendor() string   { return a.vendor }
func (a *CCCAdapter) Protocol() string { return "ccc_dk3" }

func (a *CCCAdapter) BindKey(ctx context.Context, req *pb.BindKeyRequest) (*pb.BindKeyResponse, error) {
	a.logger.Info("BindKey: start",
		zap.String("vehicle_id", req.VehicleId),
		zap.String("user_id", req.UserId),
	)

	// 生产路径: 必须配置厂商 API (密钥材料只能由 HSM/厂商后端产生)
	if a.httpClient == nil || a.baseURL == "" {
		if a.demoMode {
			return a.demoBindKey(req), nil
		}
		return nil, fmt.Errorf("ccc adapter: vendor API not configured — refusing to synthesize key material")
	}

	bindReq := cccBindKeyRequest{
		DeviceID:     req.DeviceId,
		UserID:       req.UserId,
		DevicePubKey: fmt.Sprintf("%x", req.DevicePubkey),
		AccessLevel:  req.AccessLevel.String(),
		ValidFrom:    req.ValidFrom,
		ValidUntil:   req.ValidUntil,
		VehicleID:    req.VehicleId,
	}

	body, err := json.Marshal(bindReq)
	if err != nil {
		a.logger.Error("BindKey: marshal request failed", zap.Error(err))
		return nil, fmt.Errorf("ccc bind key marshal: %w", err)
	}

	url := a.baseURL + "/passkeys/" + req.VehicleId
	httpReq, err := http.NewRequestWithContext(ctx, http.MethodPost, url, bytes.NewReader(body))
	if err != nil {
		a.logger.Error("BindKey: create request failed", zap.Error(err))
		return nil, fmt.Errorf("ccc bind key request: %w", err)
	}
	httpReq.Header.Set("Content-Type", "application/json")

	httpResp, err := a.httpClient.Do(httpReq)
	if err != nil {
		a.logger.Error("BindKey: vendor API call failed",
			zap.String("vendor", a.vendor),
			zap.Error(err),
		)
		if a.demoMode {
			return a.demoBindKey(req), nil
		}
		return nil, fmt.Errorf("ccc vendor api unavailable: %w", err)
	}
	defer httpResp.Body.Close()

	if httpResp.StatusCode != http.StatusOK && httpResp.StatusCode != http.StatusCreated {
		respBody, _ := io.ReadAll(httpResp.Body)
		a.logger.Warn("BindKey: vendor API returned non-success",
			zap.Int("status", httpResp.StatusCode),
			zap.String("response", string(respBody)),
		)
		return nil, fmt.Errorf("ccc vendor api error: status=%d body=%s", httpResp.StatusCode, string(respBody))
	}

	var bindResp cccBindKeyResponse
	if err := json.NewDecoder(httpResp.Body).Decode(&bindResp); err != nil {
		a.logger.Warn("BindKey: decode vendor response failed", zap.Error(err))
		return nil, fmt.Errorf("ccc bind key decode: %w", err)
	}

	// 密钥材料完整性门禁: 空/全零视为无效，绝不下发。
	if bindResp.KeyID == "" ||
		len(bindResp.VehiclePubKey) == 0 || isZeroBytes([]byte(bindResp.VehiclePubKey)) ||
		len(bindResp.SharedSecret) == 0 || isZeroBytes([]byte(bindResp.SharedSecret)) {
		return nil, fmt.Errorf("ccc vendor response missing valid key material")
	}

	return &pb.BindKeyResponse{
		Key: &pb.DigitalKey{
			KeyId:       bindResp.KeyID,
			VehicleId:   req.VehicleId,
			DeviceId:    req.DeviceId,
			UserId:      req.UserId,
			KeyType:     req.KeyType,
			Protocol:    pb.Protocol_CCC_DK3,
			AccessLevel: req.AccessLevel,
			Status:      pb.KeyStatus_ACTIVE,
			ValidFrom:   req.ValidFrom,
			ValidUntil:  req.ValidUntil,
			CreatedAt:   time.Now().Unix(),
		},
		VehiclePubkey: []byte(bindResp.VehiclePubKey),
		SharedSecret:  []byte(bindResp.SharedSecret),
	}, nil
}

// demoBindKey 仅测试用: 合成占位密钥。生产路径不可达 (demoMode=false 时拒绝)。
func (a *CCCAdapter) demoBindKey(req *pb.BindKeyRequest) *pb.BindKeyResponse {
	return &pb.BindKeyResponse{
		Key: &pb.DigitalKey{
			KeyId:       fmt.Sprintf("key-ccc-demo-%d", time.Now().UnixMilli()),
			VehicleId:   req.VehicleId,
			DeviceId:    req.DeviceId,
			UserId:      req.UserId,
			KeyType:     req.KeyType,
			Protocol:    pb.Protocol_CCC_DK3,
			AccessLevel: req.AccessLevel,
			Status:      pb.KeyStatus_ACTIVE,
			ValidFrom:   req.ValidFrom,
			ValidUntil:  req.ValidUntil,
			CreatedAt:   time.Now().Unix(),
		},
		VehiclePubkey: make([]byte, 64),
		SharedSecret:  make([]byte, 32),
	}
}

// isZeroBytes 报告 b 是否全为 0（空切片视为全零）。
func isZeroBytes(b []byte) bool {
	for _, v := range b {
		if v != 0 {
			return false
		}
	}
	return true
}

func (a *CCCAdapter) UnbindKey(ctx context.Context, keyID string) error {
	a.logger.Info("UnbindKey", zap.String("key_id", keyID))

	// 如果有配置 HTTP client，调用厂商API删除数字钥匙
	if a.httpClient != nil && a.baseURL != "" {
		url := a.baseURL + "/passkeys/" + keyID
		httpReq, err := http.NewRequestWithContext(ctx, http.MethodDelete, url, nil)
		if err != nil {
			a.logger.Error("UnbindKey: create request failed", zap.Error(err))
			return fmt.Errorf("ccc unbind key request: %w", err)
		}

		httpResp, err := a.httpClient.Do(httpReq)
		if err != nil {
			a.logger.Error("UnbindKey: vendor API call failed",
				zap.String("vendor", a.vendor),
				zap.String("key_id", keyID),
				zap.Error(err),
			)
			return fmt.Errorf("ccc unbind key failed: %w", err)
		}
		defer httpResp.Body.Close()

		if httpResp.StatusCode != http.StatusOK && httpResp.StatusCode != http.StatusNoContent {
			respBody, _ := io.ReadAll(httpResp.Body)
			a.logger.Warn("UnbindKey: vendor API returned non-success",
				zap.Int("status", httpResp.StatusCode),
				zap.String("response", string(respBody)),
			)
			return fmt.Errorf("ccc vendor unbind error: status=%d body=%s", httpResp.StatusCode, string(respBody))
		}
	}

	// Apple:  DELETE /v1/passkeys/{key_id}
	// Samsung: DELETE /api/v2/digitalkeys/{key_id}
	return nil
}

func (a *CCCAdapter) RevokeNotify(ctx context.Context, keyID string, reason string) error {
	a.logger.Info("RevokeNotify", zap.String("key_id", keyID), zap.String("reason", reason))

	// 如果有配置 HTTP client，调用厂商API撤销钥匙
	if a.httpClient != nil && a.baseURL != "" {
		revokeReq := cccRevokeRequest{Reason: reason}
		body, err := json.Marshal(revokeReq)
		if err != nil {
			a.logger.Error("RevokeNotify: marshal request failed", zap.Error(err))
			return fmt.Errorf("ccc revoke marshal: %w", err)
		}

		url := a.baseURL + "/passkeys/" + keyID + "/revoke"
		httpReq, err := http.NewRequestWithContext(ctx, http.MethodPost, url, bytes.NewReader(body))
		if err != nil {
			a.logger.Error("RevokeNotify: create request failed", zap.Error(err))
			return fmt.Errorf("ccc revoke request: %w", err)
		}
		httpReq.Header.Set("Content-Type", "application/json")

		httpResp, err := a.httpClient.Do(httpReq)
		if err != nil {
			a.logger.Error("RevokeNotify: vendor API call failed",
				zap.String("vendor", a.vendor),
				zap.String("key_id", keyID),
				zap.Error(err),
			)
			return fmt.Errorf("ccc revoke failed: %w", err)
		}
		defer httpResp.Body.Close()

		if httpResp.StatusCode != http.StatusOK && httpResp.StatusCode != http.StatusNoContent {
			respBody, _ := io.ReadAll(httpResp.Body)
			a.logger.Warn("RevokeNotify: vendor API returned non-success",
				zap.Int("status", httpResp.StatusCode),
				zap.String("response", string(respBody)),
			)
			return fmt.Errorf("ccc vendor revoke error: status=%d body=%s", httpResp.StatusCode, string(respBody))
		}
	}

	// Apple:  POST /v1/passkeys/{key_id}/revoke + APNs推送
	// Samsung: POST /api/v2/digitalkeys/{key_id}/revoke + FCM推送
	return nil
}

func (a *CCCAdapter) ShareKey(ctx context.Context, req *pb.CreateShareRequest) (*pb.CreateShareResponse, error) {
	a.logger.Info("ShareKey", zap.String("key_id", req.KeyId))

	// CCC分享流程:
	// 1. 车主授权分享码
	// 2. 接收方通过厂商钱包App领取
	// 3. 生成受限密钥 (sub-key)
	// Apple:  POST /v1/passkeys/{key_id}/share
	// Samsung: POST /api/v2/digitalkeys/{key_id}/share

	// 生成 share ID
	shareID := fmt.Sprintf("share-ccc-%d", time.Now().UnixMilli())

	// 创建 Mailbox（如果配置了 Creator）
	var sharingURL string
	if a.mailboxCreator != nil {
		mailboxID, url, err := a.mailboxCreator.CreateMailbox(ctx, req.KeyId, "apple", req.FromUserId, shareID)
		if err != nil {
			a.logger.Warn("CreateMailbox failed, sharing continues without mailbox",
				zap.String("share_id", shareID),
				zap.Error(err),
			)
		} else {
			sharingURL = url
			a.logger.Info("Mailbox created for CCC share",
				zap.String("share_id", shareID),
				zap.String("mailbox_id", mailboxID),
				zap.String("sharing_url", sharingURL),
			)
		}
	}

	return &pb.CreateShareResponse{
		ShareId:    shareID,
		ShareCode:  fmt.Sprintf("%06d", time.Now().UnixNano()%1000000),
		SharingUrl: sharingURL,
	}, nil
}

func (a *CCCAdapter) AcceptShare(ctx context.Context, req *pb.AcceptShareRequest) (*pb.AcceptShareResponse, error) {
	a.logger.Info("AcceptShare", zap.String("vendor", req.Vendor.String()))

	// 接收方设备SE验证 + 密钥写入
	return &pb.AcceptShareResponse{
		Key: &pb.DigitalKey{
			KeyId:    fmt.Sprintf("key-ccc-share-%d", time.Now().UnixMilli()),
			KeyType:  pb.KeyType_FRIEND,
			Protocol: pb.Protocol_CCC_DK3,
			Status:   pb.KeyStatus_ACTIVE,
		},
	}, nil
}

func (a *CCCAdapter) Notify(ctx context.Context, userID string, notification *pb.VehicleStatusUpdate) error {
	a.logger.Info("Notify", zap.String("user_id", userID))
	// Apple: APNs push
	// Samsung: FCM push
	return nil
}

func (a *CCCAdapter) HealthCheck(ctx context.Context) (*pb.AdapterStatus, error) {
	// 调用厂商健康检查端点
	// Apple:  GET /v1/health
	// Samsung: GET /api/v2/health
	return &pb.AdapterStatus{
		Vendor:      a.vendor,
		Protocol:    "ccc_dk3",
		Healthy:     true,
		LastCheckMs: time.Now().UnixMilli(),
	}, nil
}

// ============================================================================
// CCC厂商API请求/响应结构体
// ============================================================================

// cccBindKeyRequest 绑钥匙请求体
// Apple:  POST /v1/passkeys/{vehicle_id}
// Samsung: POST /api/v2/digitalkeys
type cccBindKeyRequest struct {
	DeviceID     string `json:"device_id"`
	UserID       string `json:"user_id"`
	DevicePubKey string `json:"device_pubkey"`
	AccessLevel  string `json:"access_level"`
	ValidFrom    int64  `json:"valid_from,omitempty"`
	ValidUntil   int64  `json:"valid_until,omitempty"`
	VehicleID    string `json:"vehicle_id"`
}

// cccBindKeyResponse 绑钥匙响应体
type cccBindKeyResponse struct {
	KeyID         string `json:"key_id"`
	VehiclePubKey string `json:"vehicle_pubkey"`
	SharedSecret  string `json:"shared_secret"`
}

// cccRevokeRequest 撤销钥匙请求体
// Apple:  POST /v1/passkeys/{key_id}/revoke
// Samsung: POST /api/v2/digitalkeys/{key_id}/revoke
type cccRevokeRequest struct {
	Reason string `json:"reason"`
}
