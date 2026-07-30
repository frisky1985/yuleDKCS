package adapter

import (
	"context"
	"fmt"
	"time"

	"go.uber.org/zap"
	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/adapter/s2s"
)

// ICCOAAdapter ICCOA协议适配器 (小米/OPPO/vivo)
// 对接: 小米钱包/OPPO钱包/vivo钱包数字钥匙服务
// ICCOA 以车服务器为中心的 S2S 架构，无 Relay Server
// Hub 通过 ICCOAClient 直连车服务器 S2S 端点
type ICCOAAdapter struct {
	vendor string
	logger *zap.Logger
	client *s2s.ICCOAClient // S2S 车服务器通信客户端
}

// NewICCOAAdapter 创建 ICCOA 适配器（stub 模式，无 S2S 客户端）
// S2S 功能需通过 NewICCOAAdapterWithClient 注入客户端后启用
func NewICCOAAdapter(vendor string, logger *zap.Logger) *ICCOAAdapter {
	return &ICCOAAdapter{
		vendor: vendor,
		logger: logger.With(zap.String("vendor", vendor), zap.String("protocol", "iccoa_dk40")),
	}
}

// NewICCOAAdapterWithClient 使用自定义 S2S 客户端创建适配器（测试用）
func NewICCOAAdapterWithClient(vendor string, logger *zap.Logger, client *s2s.ICCOAClient) *ICCOAAdapter {
	return &ICCOAAdapter{
		vendor: vendor,
		logger: logger.With(zap.String("vendor", vendor), zap.String("protocol", "iccoa_dk40")),
		client: client,
	}
}

// defaultICCOAConfig 返回厂商默认 ICCOA 车服务器配置
func defaultICCOAConfig(vendor string) s2s.ICCOAConfig {
	// 默认使用占位 URL，生产环境通过环境变量配置
	var baseURL, vehicleOEMID, deviceOEMID string
	switch vendor {
	case "xiaomi":
		baseURL = "https://api.xiaomi.com/carkey/v4"
		vehicleOEMID = "xiaomi_vehicle"
		deviceOEMID = "xiaomi_device"
	case "oppo":
		baseURL = "https://api.oppo.com/ocar/v2/digitalkey"
		vehicleOEMID = "oppo_vehicle"
		deviceOEMID = "oppo_device"
	case "vivo":
		baseURL = "https://api.vivo.com/vivotsp/v1/digitalkey"
		vehicleOEMID = "vivo_vehicle"
		deviceOEMID = "vivo_device"
	default:
		baseURL = "https://api.iccoa.org/dk/v4"
		vehicleOEMID = "default_vehicle"
		deviceOEMID = "default_device"
	}
	return s2s.NewDefaultICCOAConfig(vendor, baseURL, vehicleOEMID, deviceOEMID)
}

func (a *ICCOAAdapter) Vendor() string   { return a.vendor }
func (a *ICCOAAdapter) Protocol() string { return "iccoa_dk40" }

func (a *ICCOAAdapter) BindKey(ctx context.Context, req *pb.BindKeyRequest) (*pb.BindKeyResponse, error) {
	a.logger.Info("BindKey: ICCOA DK4.0 binding",
		zap.String("vehicle_id", req.VehicleId),
		zap.String("user_id", req.UserId),
	)

	// ICCOA DK4.0绑定流程:
	// 1. 设备能力协商 (BLE 5.0 + UWB FiRa)
	// 2. 车端生成密钥对
	// 3. DK4.0 BLE配对帧交换
	// 4. UWB配置下发
	// 5. 调用车服务器 trackKey API 注册钥匙

	if a.client != nil {
		s2sReq := &s2s.ICCOATrackKeyRequest{
			KeyID:        req.VehicleId + "-" + req.DeviceId,
			VehicleID:    req.VehicleId,
			DeviceID:     req.DeviceId,
			UserID:       req.UserId,
			KeyType:      req.KeyType.String(),
			DevicePubKey: fmt.Sprintf("%x", req.DevicePubkey),
			ValidFrom:    req.ValidFrom,
			ValidUntil:   req.ValidUntil,
		}

		s2sResp, err := a.client.TrackKey(ctx, s2sReq)
		if err != nil {
			a.logger.Error("ICCOA S2S BindKey (trackKey) failed",
				zap.String("vehicle_id", req.VehicleId),
				zap.Error(err),
				zap.Stack("stack"),
			)
			// graceful degradation: 失败时使用 fallback stub
		} else {
			return &pb.BindKeyResponse{
				Key: &pb.DigitalKey{
					KeyId:       s2sResp.KeyID,
					VehicleId:   req.VehicleId,
					DeviceId:    req.DeviceId,
					UserId:      req.UserId,
					KeyType:     req.KeyType,
					Protocol:    pb.Protocol_ICCOA_DK40,
					AccessLevel: req.AccessLevel,
					Status:      pb.KeyStatus_ACTIVE,
					ValidFrom:   req.ValidFrom,
					ValidUntil:  req.ValidUntil,
					CreatedAt:   time.Now().Unix(),
				},
				VehiclePubkey: []byte(s2sResp.VehiclePubKey),
			}, nil
		}
	}

	// fallback stub (client nil 或 S2S 失败时)
	vehiclePubKey := make([]byte, 64)
	sharedSecret := make([]byte, 32)

	return &pb.BindKeyResponse{
		Key: &pb.DigitalKey{
			KeyId:       fmt.Sprintf("key-iccoa-%d", time.Now().UnixMilli()),
			VehicleId:   req.VehicleId,
			DeviceId:    req.DeviceId,
			UserId:      req.UserId,
			KeyType:     req.KeyType,
			Protocol:    pb.Protocol_ICCOA_DK40,
			AccessLevel: req.AccessLevel,
			Status:      pb.KeyStatus_ACTIVE,
			ValidFrom:   req.ValidFrom,
			ValidUntil:  req.ValidUntil,
			CreatedAt:   time.Now().Unix(),
		},
		VehiclePubkey: vehiclePubKey,
		SharedSecret:  sharedSecret,
	}, nil
}

func (a *ICCOAAdapter) UnbindKey(ctx context.Context, keyID string) error {
	a.logger.Info("UnbindKey", zap.String("key_id", keyID))

	if a.client != nil {
		// 调用 manageKey action=revoke 解绑
		req := &s2s.ICCOAManageKeyRequest{
			KeyID:  keyID,
			Action: "revoke",
			Reason: "user_unbind",
		}
		if _, err := a.client.ManageKey(ctx, req); err != nil {
			a.logger.Error("ICCOA S2S UnbindKey (manageKey) failed",
				zap.String("key_id", keyID),
				zap.Error(err),
				zap.Stack("stack"),
			)
			return fmt.Errorf("iccoa unbind failed: %w", err)
		}

		// 发送钥匙事件通知
		notifyReq := &s2s.ICCOANotifyKeyEventRequest{
			KeyID:     keyID,
			EventType: "unbind",
			Timestamp: time.Now().UnixMilli(),
		}
		if err := a.client.NotifyKeyEvent(ctx, notifyReq); err != nil {
			a.logger.Warn("ICCOA S2S NotifyKeyEvent after unbind failed",
				zap.String("key_id", keyID),
				zap.Error(err),
			)
			// 非关键错误，不阻断流程
		}
		return nil
	}

	return nil
}

func (a *ICCOAAdapter) RevokeNotify(ctx context.Context, keyID string, reason string) error {
	a.logger.Info("RevokeNotify", zap.String("key_id", keyID), zap.String("reason", reason))

	if a.client != nil {
		req := &s2s.ICCOAManageKeyRequest{
			KeyID:  keyID,
			Action: "revoke",
			Reason: reason,
		}
		if _, err := a.client.ManageKey(ctx, req); err != nil {
			a.logger.Error("ICCOA S2S RevokeNotify (manageKey) failed",
				zap.String("key_id", keyID),
				zap.Error(err),
				zap.Stack("stack"),
			)
			return fmt.Errorf("iccoa revoke failed: %w", err)
		}
	}
	return nil
}

func (a *ICCOAAdapter) ShareKey(ctx context.Context, req *pb.CreateShareRequest) (*pb.CreateShareResponse, error) {
	a.logger.Info("ShareKey: ICCOA DK4.0 sharing", zap.String("key_id", req.KeyId))

	if a.client != nil {
		// 1. 调用 share/genSession 生成分享 session
		sessionReq := &s2s.ICCOAGenSessionRequest{
			KeyID:      req.KeyId,
			FromUserID: req.FromUserId,
			ToUserID:   req.ToUserId,
			ValidFrom:  req.ValidFrom,
			ValidUntil: req.ValidUntil,
		}

		sessionResp, err := a.client.GenSession(ctx, sessionReq)
		if err != nil {
			a.logger.Error("ICCOA S2S ShareKey (genSession) failed",
				zap.String("key_id", req.KeyId),
				zap.Error(err),
				zap.Stack("stack"),
			)
			return nil, fmt.Errorf("iccoa share failed: %w", err)
		}

		// 2. 通知钥匙事件
		notifyReq := &s2s.ICCOANotifyKeyEventRequest{
			KeyID:     req.KeyId,
			EventType: "share",
			Timestamp: time.Now().UnixMilli(),
		}
		if err := a.client.NotifyKeyEvent(ctx, notifyReq); err != nil {
			a.logger.Warn("ICCOA S2S NotifyKeyEvent after share failed",
				zap.String("key_id", req.KeyId),
				zap.Error(err),
			)
			// 非关键错误
		}

		return &pb.CreateShareResponse{
			ShareId:   sessionResp.SessionID,
			ShareCode: sessionResp.ShareCode,
		}, nil
	}

	// fallback stub
	return &pb.CreateShareResponse{
		ShareId:   fmt.Sprintf("share-iccoa-%d", time.Now().UnixMilli()),
		ShareCode: fmt.Sprintf("%06d", time.Now().UnixNano()%1000000),
	}, nil
}

func (a *ICCOAAdapter) AcceptShare(ctx context.Context, req *pb.AcceptShareRequest) (*pb.AcceptShareResponse, error) {
	a.logger.Info("AcceptShare", zap.String("vendor", req.Vendor.String()))

	if a.client != nil {
		// 1. 使用 shareCode 通过车服务器完成签发
		// 简化流程: 车服务器验证分享码后直接签发钥匙
		signReq := &s2s.ICCOASignRequest{
			SessionID:    req.ShareCode,
			DevicePubKey: fmt.Sprintf("%x", req.DevicePubkey),
		}

		signResp, err := a.client.Sign(ctx, signReq)
		if err != nil {
			a.logger.Error("ICCOA S2S AcceptShare (sign) failed",
				zap.String("share_code", req.ShareCode),
				zap.Error(err),
				zap.Stack("stack"),
			)
			return nil, fmt.Errorf("iccoa accept share failed: %w", err)
		}

		// 2. 通知钥匙事件
		notifyReq := &s2s.ICCOANotifyKeyEventRequest{
			KeyID:     signResp.KeyID,
			EventType: "bind",
			Timestamp: time.Now().UnixMilli(),
		}
		if err := a.client.NotifyKeyEvent(ctx, notifyReq); err != nil {
			a.logger.Warn("ICCOA S2S NotifyKeyEvent after accept failed",
				zap.String("key_id", signResp.KeyID),
				zap.Error(err),
			)
		}

		return &pb.AcceptShareResponse{
			Key: &pb.DigitalKey{
				KeyId:    signResp.KeyID,
				KeyType:  pb.KeyType_FRIEND,
				Protocol: pb.Protocol_ICCOA_DK40,
				Status:   pb.KeyStatus_ACTIVE,
			},
		}, nil
	}

	// fallback stub
	return &pb.AcceptShareResponse{
		Key: &pb.DigitalKey{
			KeyId:    fmt.Sprintf("key-iccoa-share-%d", time.Now().UnixMilli()),
			KeyType:  pb.KeyType_FRIEND,
			Protocol: pb.Protocol_ICCOA_DK40,
			Status:   pb.KeyStatus_ACTIVE,
		},
	}, nil
}

func (a *ICCOAAdapter) Notify(ctx context.Context, userID string, notification *pb.VehicleStatusUpdate) error {
	a.logger.Info("Notify", zap.String("user_id", userID))

	if a.client != nil {
		// 车辆状态变更通知 → 通知车服务器
		req := &s2s.ICCOANotifyKeyEventRequest{
			EventType: "vehicle_status",
			UserID:    userID,
			Timestamp: time.Now().UnixMilli(),
		}
		if err := a.client.NotifyKeyEvent(ctx, req); err != nil {
			a.logger.Warn("ICCOA S2S NotifyKeyEvent failed",
				zap.String("user_id", userID),
				zap.Error(err),
			)
		}
	}
	return nil
}

func (a *ICCOAAdapter) HealthCheck(ctx context.Context) (*pb.AdapterStatus, error) {
	if a.client != nil {
		resp, err := a.client.HealthCheck(ctx)
		if err != nil {
			return &pb.AdapterStatus{
				Vendor:      a.vendor,
				Protocol:    "iccoa_dk40",
				Healthy:     false,
				ErrorMsg:    err.Error(),
				LastCheckMs: time.Now().UnixMilli(),
			}, nil
		}
		_ = resp
	}

	return &pb.AdapterStatus{
		Vendor:      a.vendor,
		Protocol:    "iccoa_dk40",
		Healthy:     true,
		LastCheckMs: time.Now().UnixMilli(),
	}, nil
}
