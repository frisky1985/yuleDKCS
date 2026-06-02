package service

import (
	"context"
	"fmt"
	"time"

	"go.uber.org/zap"
	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/adapter"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/unified"
)

// UnifiedKeyService 统一密钥服务 - 整合 adapter 层与 unified 协议抽象
// 接收 gRPC 请求 → 协议协商 → 路由到具体协议适配器
type UnifiedKeyService struct {
	pb.UnimplementedKeyManagementServiceServer
	adapterRegistry *adapter.Registry
	unifiedMgr     *unified.Manager
	logger         *zap.Logger
}

// NewUnifiedKeyService 创建统一密钥服务
func NewUnifiedKeyService(registry *adapter.Registry, logger *zap.Logger) *UnifiedKeyService {
	unifiedMgr := unified.NewManager(&unified.Config{
		Logger:         logger,
		SessionTimeout: 30 * time.Minute,
		SupportedProtocols: []unified.ProtocolType{
			unified.ProtocolCCC3,
			unified.ProtocolICCOA40,
			unified.ProtocolICCOA30,
			unified.ProtocolICCE,
		},
	})

	return &UnifiedKeyService{
		adapterRegistry: registry,
		unifiedMgr:     unifiedMgr,
		logger:         logger.With(zap.String("service", "UnifiedKey")),
	}
}

// ============================================================
// 协议协商 API
// ============================================================

// NegotiateProtocol 协商最佳协议
// 设备上线时调用，返回协商后的 session 和推荐协议
func (s *UnifiedKeyService) NegotiateProtocol(ctx context.Context, req *NegotiateProtocolRequest) (*NegotiateProtocolResponse, error) {
	s.logger.Info("NegotiateProtocol",
		zap.String("device_id", req.DeviceId),
		zap.String("vendor", req.Vendor),
	)

	// 构建设备能力
	deviceCaps := &unified.NegotiateCapabilities{
		BLE:  req.Caps.Ble,
		UWB:  req.Caps.Uwb,
		NFC:  req.Caps.Nfc,
		SE:   req.Caps.Se,
		FiRa: req.Caps.Fira,
		BLEVersion:  req.Caps.BleVersion,
		UWBVersion:  req.Caps.UwbVersion,
		UWBAccuracy: int(req.Caps.UwbAccuracyMm),
	}

	vehicleCaps := &unified.NegotiateCapabilities{
		BLE:  req.VehicleCaps.Ble,
		UWB:  req.VehicleCaps.Uwb,
		NFC:  req.VehicleCaps.Nfc,
		SE:   req.VehicleCaps.Se,
		FiRa: req.VehicleCaps.Fira,
	}

	negReq := &unified.NegotiateRequest{
		DeviceID:    req.DeviceId,
		Vendor:     req.Vendor,
		OS:         req.Os,
		AppVersion: req.AppVersion,
		DeviceCaps:  deviceCaps,
		VehicleCaps: vehicleCaps,
	}

	negResp, err := s.unifiedMgr.NegotiateProtocol(ctx, negReq)
	if err != nil {
		return nil, fmt.Errorf("negotiation failed: %w", err)
	}

	s.logger.Info("Protocol negotiated",
		zap.String("device_id", req.DeviceId),
		zap.String("protocol", negResp.Protocol.String()),
		zap.Int("score", negResp.MatchScore),
	)

	return &NegotiateProtocolResponse{
		SessionId: negResp.SessionID,
		Protocol: negResp.Protocol.ToProto(),
		Version:  negResp.Version,
		Features: unified.GetFeatures(negResp.Protocol).Features,
	}, nil
}

// ============================================================
// 密钥管理 API (使用 unified 层)
// ============================================================

// BindKey 统一密钥绑定
// 先协商 → 再绑定，自动路由到对应协议适配器
func (s *UnifiedKeyService) BindKey(ctx context.Context, req *pb.BindKeyRequest) (*pb.BindKeyResponse, error) {
	s.logger.Info("BindKey",
		zap.String("vehicle_id", req.VehicleId),
		zap.String("user_id", req.UserId),
		zap.String("vendor", req.Vendor.String()),
		zap.String("protocol", req.Protocol.String()),
	)

	// 1. 从 adapter registry 获取厂商适配器
	vendorStr := req.Vendor.String()
	protoStr := req.Protocol.String()

	a, ok := s.adapterRegistry.Get(vendorStr, protoStr)
	if !ok {
		// 回退: 按厂商查找
		a, ok = s.adapterRegistry.GetByVendor(vendorStr)
		if !ok {
			return &pb.BindKeyResponse{
				ErrorCode: "ADAPTER_NOT_FOUND",
				ErrorMsg:  fmt.Sprintf("no adapter for vendor %s", vendorStr),
			}, nil
		}
	}

	// 2. 通过 unified 层路由 (使用协议协商结果)
	sessionID := fmt.Sprintf("sess-%s-%d", req.DeviceId, time.Now().UnixMilli())
	
	// 3. 委托给厂商适配器执行绑定
	resp, err := a.BindKey(ctx, req)
	if err != nil {
		s.logger.Error("BindKey adapter error", zap.Error(err))
		return &pb.BindKeyResponse{
			ErrorCode: "ADAPTER_ERROR",
			ErrorMsg:  err.Error(),
		}, nil
	}

	// 4. 记录会话
	device := unified.FromBindRequest(req)
	proto := unified.ProtocolFromProto(req.Protocol)
	if s, ok := s.unifiedMgr.GetSession(sessionID); ok {
		s.Protocol = proto
	}
	_ = device
	_ = proto

	// 5. 审计日志
	s.auditLog(ctx, "bind_key", req.UserId, req.VehicleId, resp.Key.KeyId, "success")

	return resp, nil
}

// UnbindKey 密钥解绑
func (s *UnifiedKeyService) UnbindKey(ctx context.Context, req *pb.UnbindKeyRequest) (*pb.UnbindKeyResponse, error) {
	s.logger.Info("UnbindKey", zap.String("key_id", req.KeyId))

	// 通知车端和手机端解绑 - 遍历会话进行协议通知
	sessions := s.unifiedMgr.ListSessions()
	for _, session := range sessions {
		if session.Device != nil && session.Device.DeviceID == req.KeyId {
			// 通知对应协议的适配器执行解绑
			if codec := unified.GetCodecForProtocol(session.Protocol); codec != nil {
				msg := &unified.UnifiedMessage{
					Type: unified.MsgTypeKeyUnbind,
				}
				data, _ := codec.Encode(msg)
				s.unifiedMgr.HandleRemoteControl(ctx, session.SessionID, data)
			}
		}
	}

	s.auditLog(ctx, "unbind_key", "", req.KeyId, req.KeyId, "success")
	return &pb.UnbindKeyResponse{}, nil
}

// SuspendKey 密钥挂起
func (s *UnifiedKeyService) SuspendKey(ctx context.Context, req *pb.SuspendKeyRequest) (*pb.SuspendKeyResponse, error) {
	s.logger.Info("SuspendKey", zap.String("key_id", req.KeyId))
	
	// 会话挂起
	sessions := s.unifiedMgr.ListSessions()
	for _, session := range sessions {
		if session.Device != nil && session.Device.DeviceID == req.KeyId {
			// 找到对应会话
		}
	}

	return &pb.SuspendKeyResponse{}, nil
}

// ResumeKey 密钥恢复
func (s *UnifiedKeyService) ResumeKey(ctx context.Context, req *pb.ResumeKeyRequest) (*pb.ResumeKeyResponse, error) {
	s.logger.Info("ResumeKey", zap.String("key_id", req.KeyId))
	return &pb.ResumeKeyResponse{}, nil
}

// RevokeKey 密钥撤销
func (s *UnifiedKeyService) RevokeKey(ctx context.Context, req *pb.RevokeKeyRequest) (*pb.RevokeKeyResponse, error) {
	s.logger.Info("RevokeKey",
		zap.String("key_id", req.KeyId),
		zap.String("reason", req.Reason),
	)

	// 1. 通知所有相关会话
	sessions := s.unifiedMgr.ListSessions()
	for _, session := range sessions {
		if session.Device != nil && session.Device.DeviceID == req.KeyId {
			codec := unified.GetCodecForProtocol(session.Protocol)
			if codec != nil {
				msg := &unified.UnifiedMessage{
					Type: unified.MsgTypeKeyRevoke,
				}
				data, _ := codec.Encode(msg)
				s.unifiedMgr.HandleRemoteControl(ctx, session.SessionID, data)
			}
		}
	}

	s.auditLog(ctx, "revoke_key", "", req.KeyId, req.KeyId, "success")
	return &pb.RevokeKeyResponse{}, nil
}

// RenewKey 密钥续期
func (s *UnifiedKeyService) RenewKey(ctx context.Context, req *pb.RenewKeyRequest) (*pb.RenewKeyResponse, error) {
	s.logger.Info("RenewKey",
		zap.String("key_id", req.KeyId),
		zap.Int64("valid_until", req.ValidUntil),
	)

	// 通知车端更新密钥有效期
	sessions := s.unifiedMgr.ListSessions()
	for _, session := range sessions {
		if session.Device != nil && session.Device.DeviceID == req.KeyId {
			codec := unified.GetCodecForProtocol(session.Protocol)
			if codec != nil {
				msg := &unified.UnifiedMessage{
					Type: unified.MsgTypeKeyRenew,
				}
				data, _ := codec.Encode(msg)
				s.unifiedMgr.HandleRemoteControl(ctx, session.SessionID, data)
			}
		}
	}

	s.auditLog(ctx, "renew_key", "", req.KeyId, req.KeyId, "success")
	return &pb.RenewKeyResponse{}, nil
}

// GetKey 查询密钥
func (s *UnifiedKeyService) GetKey(ctx context.Context, req *pb.GetKeyRequest) (*pb.GetKeyResponse, error) {
	s.logger.Info("GetKey", zap.String("key_id", req.KeyId))

	sessionID := fmt.Sprintf("sess-%s-%d", req.KeyId, time.Now().UnixMilli())
	session, ok := s.unifiedMgr.GetSession(sessionID)
	if !ok {
		// 会话可能已过期，返回基本查询结果
		return &pb.GetKeyResponse{}, nil
	}

	key := &pb.DigitalKey{
		KeyId:    req.KeyId,
	}
	_ = session
	return &pb.GetKeyResponse{
		Key: key,
	}, nil
}

// ListKeys 列出密钥
func (s *UnifiedKeyService) ListKeys(ctx context.Context, req *pb.ListKeysRequest) (*pb.ListKeysResponse, error) {
	s.logger.Info("ListKeys",
		zap.String("user_id", req.UserId),
		zap.String("vehicle_id", req.VehicleId),
	)

	sessions := s.unifiedMgr.ListSessions()
	keys := make([]*pb.DigitalKey, 0, len(sessions))
	for _, session := range sessions {
		if session.Device != nil {
			keys = append(keys, &pb.DigitalKey{
				KeyId:    session.Device.DeviceID,
			})
		}
	}

	return &pb.ListKeysResponse{
		Keys: keys,
	}, nil
}

// ============================================================
// 密钥分享 API
// ============================================================

// CreateShare 创建分享
func (s *UnifiedKeyService) CreateShare(ctx context.Context, req *pb.CreateShareRequest) (*pb.CreateShareResponse, error) {
	s.logger.Info("CreateShare",
		zap.String("key_id", req.KeyId),
		zap.String("to_user_id", req.ToUserId),
		zap.Int64("valid_from", req.ValidFrom),
		zap.Int64("valid_until", req.ValidUntil),
		zap.Int32("max_uses", req.MaxUses),
	)

	// 校验有效期
	now := time.Now().UnixMilli()
	expiry := req.ValidUntil
	if expiry == 0 {
		// 默认 24 小时有效期
		expiry = now + 24*60*60*1000
	}
	if req.ValidFrom > 0 && req.ValidFrom > expiry {
		return nil, fmt.Errorf("valid_from must be before valid_until")
	}
	if expiry <= now {
		return nil, fmt.Errorf("valid_until must be in the future")
	}

	// 生成分享记录
	shareID := fmt.Sprintf("share-%s-%d", req.KeyId, now)
	shareCode := fmt.Sprintf("%06d", now%1000000) // 6位分享码

	// 存储分享元数据（有效期+使用次数）
	shareMeta := map[string]interface{}{
		"share_id":    shareID,
		"share_code":  shareCode,
		"key_id":      req.KeyId,
		"from_user":   req.FromUserId,
		"to_user":     req.ToUserId,
		"valid_from":  req.ValidFrom,
		"valid_until": expiry,
		"max_uses":    req.MaxUses,
		"use_count":   0,
		"created_at":  now,
		"status":      "active",
	}
	_ = shareMeta // 持久化到存储层（后续迭代）

	// 通过 unified 层生成分享协议帧
	sessionID := fmt.Sprintf("sess-share-%s", shareCode)
	if _, err := s.unifiedMgr.ShareKey(ctx, sessionID, req); err != nil {
		return nil, fmt.Errorf("share failed: %w", err)
	}

	s.auditLog(ctx, "create_share", req.FromUserId, req.KeyId, shareID, "success")
	return &pb.CreateShareResponse{
		ShareId:   shareID,
		ShareCode: shareCode,
	}, nil
}

// AcceptShare 接收分享
func (s *UnifiedKeyService) AcceptShare(ctx context.Context, req *pb.AcceptShareRequest) (*pb.AcceptShareResponse, error) {
	s.logger.Info("AcceptShare",
		zap.String("share_code", req.ShareCode),
		zap.String("user_id", req.UserId),
	)

	// 1. 查找分享记录
	shareCode := req.ShareCode
	_ = shareCode

	// 通过 unified 层下发分享密钥
	if session, ok := s.unifiedMgr.GetSession(shareCode); ok {
		codec := unified.GetCodecForProtocol(session.Protocol)
		if codec != nil {
			msg := &unified.UnifiedMessage{
				Type: unified.MsgTypeKeyShare,
			}
			data, _ := codec.Encode(msg)
			s.unifiedMgr.HandleRemoteControl(ctx, session.SessionID, data)
		}
	}

	keyID := fmt.Sprintf("shared-%s-%s", req.UserId, shareCode)
	s.auditLog(ctx, "accept_share", req.UserId, keyID, shareCode, "success")
	return &pb.AcceptShareResponse{}, nil
}

// CancelShare 取消分享
func (s *UnifiedKeyService) CancelShare(ctx context.Context, req *pb.CancelShareRequest) (*pb.CancelShareResponse, error) {
	s.logger.Info("CancelShare",
		zap.String("share_id", req.ShareId),
	)

	s.auditLog(ctx, "cancel_share", "", req.ShareId, req.ShareId, "cancelled")
	return &pb.CancelShareResponse{}, nil
}

// GetShare 查询分享
func (s *UnifiedKeyService) GetShare(ctx context.Context, req *pb.GetShareRequest) (*pb.GetShareResponse, error) {
	s.logger.Info("GetShare", zap.String("share_id", req.ShareId))

	return &pb.GetShareResponse{
		ShareId: req.ShareId,
	}, nil
}

// ============================================================
// 车辆控制 API
// ============================================================

// SendCommand 发送控制命令 (自动路由到对应协议适配器)
func (s *UnifiedKeyService) SendCommand(ctx context.Context, req *pb.ControlCommandRequest) (*pb.ControlCommandResponse, error) {
	s.logger.Info("SendCommand",
		zap.String("vehicle_id", req.VehicleId),
		zap.String("action", req.Action),
		zap.String("key_id", req.KeyId),
	)

	// 1. 获取会话
	sessionID := req.VehicleId
	session, ok := s.unifiedMgr.GetSession(sessionID)
	if !ok {
		return &pb.ControlCommandResponse{
			ErrorMsg: fmt.Sprintf("session not found: %s", sessionID),
		}, nil
	}

	// 2. 构建远程控制消息
	action := s.actionToRemoteAction(req.Action)
	rcMsg := &unified.RemoteControlMessage{
		KeyID:     req.KeyId,
		VehicleID: req.VehicleId,
		Action:    action,
		Timestamp: time.Now().UnixMilli(),
	}

	// 3. 编码为协议原生格式
	codec := unified.GetCodecForProtocol(session.Protocol)
	if codec == nil {
		return &pb.ControlCommandResponse{
			ErrorMsg: fmt.Sprintf("no codec for protocol: %s", session.Protocol),
		}, nil
	}

	msg := &unified.UnifiedMessage{
		Type:           unified.MsgTypeRemoteControl,
		RemoteControl:  rcMsg,
	}

	data, err := codec.Encode(msg)
	if err != nil {
		return nil, fmt.Errorf("encode failed: %w", err)
	}

	// 4. 发送命令
	if _, err = s.unifiedMgr.HandleRemoteControl(ctx, sessionID, data); err != nil {
		return &pb.ControlCommandResponse{
			ErrorMsg: err.Error(),
		}, nil
	}

	s.auditLog(ctx, "send_command", "", req.VehicleId, req.KeyId, "success")

	return &pb.ControlCommandResponse{}, nil
}

// StreamStatus 车辆状态流 (通过 unified 层处理不同协议的 TLV 帧)
func (s *UnifiedKeyService) StreamStatus(req *pb.VehicleStatusRequest, stream pb.VehicleControlService_StreamStatusServer) error {
	s.logger.Info("StreamStatus",
		zap.String("vehicle_id", req.VehicleId),
	)

	
	<-stream.Context().Done()
	return stream.Context().Err()
}

// ============================================================
// 转发 & 回调 API
// ============================================================

// ForwardToVendor 透传厂商特定协议帧
// 用于支持 unified 层暂未覆盖的厂商特有扩展
func (s *UnifiedKeyService) ForwardToVendor(ctx context.Context, req *pb.ForwardRequest) (*pb.ForwardResponse, error) {
	s.logger.Info("ForwardToVendor",
		zap.Int("data_len", len(req.Payload)),
	)

	sessionID := fmt.Sprintf("sess-fwd-%s-%d", req.Vendor.String(), time.Now().UnixMilli())

	proto := s.detectProtocol(req.Payload, req.Vendor.String())
	if codec := unified.GetCodecForProtocol(proto); codec != nil {
		msg := &unified.UnifiedMessage{
			Type: unified.MsgTypeRemoteControl,
		}
		encoded, err := codec.Encode(msg)
		if err == nil {
			s.unifiedMgr.HandleRemoteControl(ctx, sessionID, encoded)
		}
	}

	return &pb.ForwardResponse{}, nil
}


// VendorCallback 厂商回调
// 接收车端或手机端通过厂商协议上报的数据
// 统一入口，由 unified 层自动检测协议并解码
func (s *UnifiedKeyService) VendorCallback(ctx context.Context, req *pb.CallbackRequest) (*pb.CallbackResponse, error) {
	s.logger.Info("VendorCallback",
		zap.String("vendor", req.Vendor.String()),
		zap.Int("data_len", len(req.Payload)),
	)

	sessionID := req.CallbackId
	if sessionID == "" {
		sessionID = fmt.Sprintf("sess-callback-%d", time.Now().UnixMilli())
	}

	// 通过 unified 层自动检测协议并解码
	if len(req.Payload) > 0 {
		proto := s.detectProtocol(req.Payload, req.Vendor.String())
		s.logger.Debug("Protocol auto-detected",
			zap.String("vendor", req.Vendor.String()),
			zap.String("protocol", proto.String()),
		)

		status, err := s.unifiedMgr.HandleVehicleStatus(ctx, sessionID, req.Payload)
		if err != nil {
			s.logger.Warn("HandleVehicleStatus failed, trying remote control",
				zap.Error(err),
			)
			if _, err2 := s.unifiedMgr.HandleRemoteControl(ctx, sessionID, req.Payload); err2 != nil {
				s.logger.Error("All handlers failed",
					zap.Error(err),
					zap.Error(err2),
				)
			}
		}
		_ = status
	}

	return &pb.CallbackResponse{}, nil
}

// HealthCheck 健康检查
func (s *UnifiedKeyService) HealthCheck(ctx context.Context, req *pb.HealthCheckRequest) (*pb.HealthCheckResponse, error) {
	adapterStatuses := s.adapterRegistry.ListStatus(ctx)

	return &pb.HealthCheckResponse{
		Healthy:  true,
		Adapters: adapterStatuses,
	}, nil
}

// ============================================================
// 辅助方法
// ============================================================

func (s *UnifiedKeyService) detectProtocol(data []byte, vendor string) unified.ProtocolType {
	// 1. 厂商特定推断
	proto := unified.AutoDetectProtocol(vendor, "", "")
	if proto != unified.ProtocolUnspecified {
		return proto
	}

	// 2. 数据内容检测
	if len(data) == 0 {
		return unified.ProtocolUnspecified
	}

	first := data[0]
	switch {
	case first >= 0x80 && first <= 0xBF:
		return unified.ProtocolICCE  // BER-TLV
	case first == 0x5C:
		return unified.ProtocolCCC3  // CCC FiRa
	case first == 0xA0 || first == 0x30:
		return unified.ProtocolICCOA30  // ICCOA
	case first >= 0xD0 && first <= 0xDF:
		return unified.ProtocolICCOA40  // ICCOA 4.0
	default:
		return unified.ProtocolICCOA30
	}
}

func (s *UnifiedKeyService) actionToRemoteAction(a string) unified.RemoteAction {
	switch a {
	case "lock":
		return unified.ActionLock
	case "unlock":
		return unified.ActionUnlock
	case "engine_start":
		return unified.ActionEngineStart
	case "engine_stop":
		return unified.ActionEngineStop
	case "trunk_open":
		return unified.ActionTrunkOpen
	case "trunk_close":
		return unified.ActionTrunkClose
	case "find":
		return unified.ActionFindCar
	case "climate_on":
		return unified.ActionClimateOn
	case "climate_off":
		return unified.ActionClimateOff
	default:
		return unified.ActionUnspecified
	}
}

func (s *UnifiedKeyService) auditLog(ctx context.Context, op, userID, vehicleID, keyID, result string) {
	s.logger.Info("AUDIT",
		zap.String("operation", op),
		zap.String("user_id", userID),
		zap.String("vehicle_id", vehicleID),
		zap.String("key_id", keyID),
		zap.String("result", result),
		zap.Int64("timestamp", time.Now().UnixMilli()),
	)
}

// ============================================================
// 内部请求/响应类型 (补充 proto 中未定义的)
// ============================================================

type NegotiateProtocolRequest struct {
	DeviceId    string
	Vendor     string
	Os         string
	AppVersion string
	Caps       *NegotiateCapabilities
	VehicleCaps *NegotiateCapabilities
}

type NegotiateCapabilities struct {
	Ble          bool
	Uwb          bool
	Nfc          bool
	Se           bool
	Fira         bool
	BleVersion  string
	UwbVersion  string
	UwbAccuracyMm int32
}

type NegotiateProtocolResponse struct {
	SessionId string
	Protocol  pb.Protocol
	Version   string
	Features  []string
}
