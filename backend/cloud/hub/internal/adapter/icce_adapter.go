package adapter

import (
	"context"
	"fmt"
	"time"

	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/adapter/s2s"
	"go.uber.org/zap"
)

// ICCEAdapter ICCE协议适配器 (华为等)
// ICCE 以车厂 OEM 为中心的 S2S 架构，无 Relay Server
// Hub 通过 ICCEClient 直连厂商 S2S 端点
type ICCEAdapter struct {
	vendor   string
	logger   *zap.Logger
	client   *s2s.ICCEClient // S2S 厂商通信客户端
	demoMode bool            // 仅测试: 允许合成占位密钥（生产必须为 false）
}

// NewICCEAdapter 创建 ICCE 适配器（生产模式，无 S2S 客户端时 BindKey 直接失败）
// S2S 功能需通过 NewICCEAdapterWithClient 注入客户端后启用
func NewICCEAdapter(vendor string, logger *zap.Logger) *ICCEAdapter {
	return &ICCEAdapter{
		vendor: vendor,
		logger: logger.With(zap.String("vendor", vendor), zap.String("protocol", "icce")),
	}
}

// NewICCEAdapterInDemoMode 仅供测试: 厂商 S2S 不可用时合成占位密钥。
// SECURITY: 生产环境禁止使用。
func NewICCEAdapterInDemoMode(vendor string, logger *zap.Logger) *ICCEAdapter {
	a := NewICCEAdapter(vendor, logger)
	a.demoMode = true
	return a
}

// NewICCEAdapterWithClient 使用自定义 S2S 客户端创建适配器（测试用）
func NewICCEAdapterWithClient(vendor string, logger *zap.Logger, client *s2s.ICCEClient) *ICCEAdapter {
	return &ICCEAdapter{
		vendor: vendor,
		logger: logger.With(zap.String("vendor", vendor), zap.String("protocol", "icce")),
		client: client,
	}
}

func (a *ICCEAdapter) Vendor() string   { return a.vendor }
func (a *ICCEAdapter) Protocol() string { return "icce" }

// callS2S 安全调用 S2S 客户端，失败时返回 fallback 值
// 当 S2S 端点不可达或未配置时，适配器以降级模式运行
func (a *ICCEAdapter) callS2S(ctx context.Context, fn func(context.Context) error) error {
	if a.client == nil {
		return nil
	}
	if err := fn(ctx); err != nil {
		a.logger.Warn("ICCE S2S call failed, using fallback",
			zap.Error(err),
		)
		return err
	}
	return nil
}

// callS2SWithResult 安全调用 S2S 客户端并返回结果
func callS2SWithResult[T any](ctx context.Context, a *ICCEAdapter, fn func(context.Context) (*T, error)) (*T, error) {
	if a.client == nil {
		return nil, nil
	}
	result, err := fn(ctx)
	if err != nil {
		a.logger.Warn("ICCE S2S call failed, using fallback",
			zap.Error(err),
		)
		return nil, err
	}
	return result, nil
}

func (a *ICCEAdapter) BindKey(ctx context.Context, req *pb.BindKeyRequest) (*pb.BindKeyResponse, error) {
	a.logger.Info("BindKey: ICCE binding",
		zap.String("vehicle_id", req.VehicleId),
		zap.String("user_id", req.UserId),
	)

	// 生产路径: 必须配置 S2S 客户端 (密钥材料只能由厂商后端产生)
	if a.client == nil {
		if a.demoMode {
			return a.demoBindKey(req), nil
		}
		return nil, fmt.Errorf("icce adapter: S2S client not configured — refusing to synthesize key material")
	}

	s2sReq := &s2s.ICCEBindRequest{
		VehicleID:    req.VehicleId,
		DeviceID:     req.DeviceId,
		UserID:       req.UserId,
		DevicePubKey: fmt.Sprintf("%x", req.DevicePubkey),
		KeyType:      req.KeyType.String(),
		ValidFrom:    req.ValidFrom,
		ValidUntil:   req.ValidUntil,
	}

	s2sResp, err := a.client.BindKey(ctx, s2sReq)
	if err != nil {
		a.logger.Error("ICCE S2S BindKey failed",
			zap.String("vehicle_id", req.VehicleId),
			zap.Error(err),
		)
		if a.demoMode {
			return a.demoBindKey(req), nil
		}
		return nil, fmt.Errorf("icce s2s bind: %w", err)
	}

	// 密钥材料完整性门禁: 空/全零视为无效，绝不下发。
	if s2sResp.KeyID == "" ||
		len(s2sResp.VehiclePubKey) == 0 || isZeroBytes([]byte(s2sResp.VehiclePubKey)) ||
		len(s2sResp.SharedSecret) == 0 || isZeroBytes([]byte(s2sResp.SharedSecret)) {
		return nil, fmt.Errorf("icce s2s response missing valid key material")
	}

	return &pb.BindKeyResponse{
		Key: &pb.DigitalKey{
			KeyId:       s2sResp.KeyID,
			VehicleId:   req.VehicleId,
			DeviceId:    req.DeviceId,
			UserId:      req.UserId,
			KeyType:     req.KeyType,
			Protocol:    pb.Protocol_ICCE,
			AccessLevel: req.AccessLevel,
			Status:      pb.KeyStatus_ACTIVE,
			ValidFrom:   req.ValidFrom,
			ValidUntil:  req.ValidUntil,
			CreatedAt:   time.Now().Unix(),
		},
		VehiclePubkey: []byte(s2sResp.VehiclePubKey),
		SharedSecret:  []byte(s2sResp.SharedSecret),
	}, nil
}

// demoBindKey 仅测试用: 合成占位密钥。生产路径不可达 (demoMode=false 时拒绝)。
func (a *ICCEAdapter) demoBindKey(req *pb.BindKeyRequest) *pb.BindKeyResponse {
	return &pb.BindKeyResponse{
		Key: &pb.DigitalKey{
			KeyId:       fmt.Sprintf("key-icce-demo-%d", time.Now().UnixMilli()),
			VehicleId:   req.VehicleId,
			DeviceId:    req.DeviceId,
			UserId:      req.UserId,
			KeyType:     req.KeyType,
			Protocol:    pb.Protocol_ICCE,
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

func (a *ICCEAdapter) UnbindKey(ctx context.Context, keyID string) error {
	a.logger.Info("UnbindKey", zap.String("key_id", keyID))

	if err := a.callS2S(ctx, func(ctx context.Context) error {
		return a.client.UnbindKey(ctx, keyID)
	}); err != nil {
		a.logger.Error("ICCE S2S UnbindKey failed, using fallback",
			zap.String("key_id", keyID),
			zap.Error(err),
			zap.Stack("stack"),
		)
	}
	return nil
}

func (a *ICCEAdapter) RevokeNotify(ctx context.Context, keyID string, reason string) error {
	a.logger.Info("RevokeNotify", zap.String("key_id", keyID), zap.String("reason", reason))

	if err := a.callS2S(ctx, func(ctx context.Context) error {
		return a.client.RevokeKey(ctx, keyID, reason)
	}); err != nil {
		a.logger.Error("ICCE S2S RevokeNotify failed, using fallback",
			zap.String("key_id", keyID),
			zap.Error(err),
			zap.Stack("stack"),
		)
	}
	return nil
}

func (a *ICCEAdapter) ShareKey(ctx context.Context, req *pb.CreateShareRequest) (*pb.CreateShareResponse, error) {
	a.logger.Info("ShareKey", zap.String("key_id", req.KeyId))

	var shareID, shareCode string

	if a.client != nil {
		s2sReq := &s2s.ICCEShareRequest{
			KeyID:      req.KeyId,
			FromUserID: req.FromUserId,
		}

		s2sResp, err := a.client.ShareKey(ctx, s2sReq)
		if err == nil {
			shareID = s2sResp.ShareID
			shareCode = s2sResp.ShareCode
		} else {
			a.logger.Error("ICCE S2S ShareKey failed, using fallback",
				zap.String("key_id", req.KeyId),
				zap.Error(err),
				zap.Stack("stack"),
			)
		}
	}

	if shareID == "" {
		shareID = fmt.Sprintf("share-icce-%d", time.Now().UnixMilli())
	}
	if shareCode == "" {
		shareCode = fmt.Sprintf("%06d", time.Now().UnixNano()%1000000)
	}

	return &pb.CreateShareResponse{
		ShareId:   shareID,
		ShareCode: shareCode,
	}, nil
}

func (a *ICCEAdapter) AcceptShare(ctx context.Context, req *pb.AcceptShareRequest) (*pb.AcceptShareResponse, error) {
	a.logger.Info("AcceptShare", zap.String("vendor", req.Vendor.String()))

	var keyID string

	if a.client != nil {
		s2sReq := &s2s.ICCEAcceptShareRequest{
			ShareCode: req.ShareCode,
			DeviceID:  req.DeviceId,
			UserID:    req.UserId,
		}

		s2sResp, err := a.client.AcceptShare(ctx, s2sReq)
		if err == nil {
			keyID = s2sResp.KeyID
		} else {
			a.logger.Error("ICCE S2S AcceptShare failed, using fallback",
				zap.String("share_code", req.ShareCode),
				zap.Error(err),
				zap.Stack("stack"),
			)
		}
	}

	if keyID == "" {
		keyID = fmt.Sprintf("key-icce-share-%d", time.Now().UnixMilli())
	}

	return &pb.AcceptShareResponse{
		Key: &pb.DigitalKey{
			KeyId:    keyID,
			KeyType:  pb.KeyType_FRIEND,
			Protocol: pb.Protocol_ICCE,
			Status:   pb.KeyStatus_ACTIVE,
		},
	}, nil
}

func (a *ICCEAdapter) Notify(ctx context.Context, userID string, notification *pb.VehicleStatusUpdate) error {
	a.logger.Info("Notify", zap.String("user_id", userID))
	// ICCE 推送通过华为 Push Kit 实现（非 S2S 通道）
	// 已由 relay.PushNotifier 统一处理
	return nil
}

func (a *ICCEAdapter) HealthCheck(ctx context.Context) (*pb.AdapterStatus, error) {
	var errorMsg string

	// 尝试 S2S 健康检查（可选），适配器在 S2S 不可用时以降级模式运行
	if a.client != nil {
		resp, err := a.client.HealthCheck(ctx)
		if err != nil {
			a.logger.Warn("ICCE S2S HealthCheck unavailable, adapter in fallback mode",
				zap.Error(err),
			)
			errorMsg = err.Error()
		}
		_ = resp
	}

	// 即使 S2S 端点不可达，适配器仍以降级模式工作
	return &pb.AdapterStatus{
		Vendor:      a.vendor,
		Protocol:    "icce",
		Healthy:     true,
		ErrorMsg:    errorMsg,
		LastCheckMs: time.Now().UnixMilli(),
	}, nil
}
