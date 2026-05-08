package service

import (
	"context"
	"fmt"
	"time"

	"go.uber.org/zap"
	pb "github.com/digitalkey/hub/api/v1"
	"github.com/digitalkey/hub/internal/adapter"
	"github.com/digitalkey/hub/internal/unified"
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
	session := s.unifiedMgr.GetSession(sessionID)
	if session != nil {
		session.Protocol = proto
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

	// 查找密钥对应的会话
	// TODO: 从 key service 获取密钥元信息 (协议类型)
	
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

	// TODO: 通知车端撤销 + 通知手机端删除
	// 1. 获取密钥协议类型
	// 2. 调用对应 adapter.RevokeNotify()
	// 3. 更新 key service 状态

	return &pb.RevokeKeyResponse{}, nil
}

// RenewKey 密钥续期
func (s *UnifiedKeyService) RenewKey(ctx context.Context, req *pb.RenewKeyRequest) (*pb.RenewKeyResponse, error) {
	s.logger.Info("RenewKey",
		zap.String("key_id", req.KeyId),
		zap.Int64("new_valid_until", req.NewValidUntil),
	)
	return &pb.RenewKeyResponse{}, nil
}

// GetKey 查询密钥
func (s *UnifiedKeyService) GetKey(ctx context.Context, req *pb.GetKeyRequest) (*pb.GetKeyResponse, error) {
	s.logger.Info("GetKey", zap.String("key_id", req.KeyId))
	
	// TODO: 从 key service 查询
	_ = req

	return &pb.GetKeyResponse{}, nil
}

// ListKeys 列出密钥
func (s *UnifiedKeyService) ListKeys(ctx context.Context, req *pb.ListKeysRequest) (*pb.ListKeysResponse, error) {
	s.logger.Info("ListKeys",
		zap.String("user_id", req.UserId),
		zap.String("vehicle_id", req.VehicleId),
	)
	
	// TODO: 从 key service 查询
	
	return &pb.ListKeysResponse{}, nil
}

// ============================================================
// 密钥分享 API
// ============================================================

// CreateShare 创建分享
func (s *UnifiedKeyService) CreateShare(ctx context.Context, req *pb.CreateShareRequest) (*pb.CreateShareResponse, error) {
	s.logger.Info("CreateShare",
		zap.String("key_id", req.KeyId),
		zap.String("recipient_id", req.RecipientId),
	)

	// 通过 unified 层生成分享协议帧
	sessionID := fmt.Sprintf("sess-share-%d", time.Now().UnixMilli())
	shareResp, err := s.unifiedMgr.ShareKey(ctx, sessionID, req)
	if err != nil {
		return nil, fmt.Errorf("share failed: %w", err)
	}

	return &pb.CreateShareResponse{
		ShareId:   shareResp.ShareId,
		ShareCode: shareResp.ShareCode,
	}, nil
}

// AcceptShare 接收分享
func (s *UnifiedKeyService) AcceptShare(ctx context.Context, req *pb.AcceptShareRequest) (*pb.AcceptShareResponse, error) {
	s.logger.Info("AcceptShare", zap.String("share_code", req.ShareCode))
	
	// TODO: 解析分享码 → 找到分享者密钥 → 协商协议 → 创建新密钥
	
	return &pb.AcceptShareResponse{}, nil
}

// CancelShare 取消分享
func (s *UnifiedKeyService) CancelShare(ctx context.Context, req *pb.CancelShareRequest) (*pb.CancelShareResponse, error) {
	s.logger.Info("CancelShare", zap.String("share_id", req.ShareId))
	return &pb.CancelShareResponse{}, nil
}

// GetShare 查询分享
func (s *UnifiedKeyService) GetShare(ctx context.Context, req *pb.GetShareRequest) (*pb.GetShareResponse, error) {
	s.logger.Info("GetShare", zap.String("share_id", req.ShareId))
	return &pb.GetShareResponse{}, nil
}

// ============================================================
// 车辆控制 API
// ============================================================

// SendCommand 发送控制命令 (自动路由到对应协议适配器)
func (s *UnifiedKeyService) SendCommand(ctx context.Context, req *pb.ControlCommandRequest) (*pb.ControlCommandResponse, error) {
	s.logger.Info("SendCommand",
		zap.String("vehicle_id", req.VehicleId),
		zap.String("action", req.Action.String()),
		zap.String("key_id", req.KeyId),
	)

	// 1. 获取会话
	sessionID := req.SessionId
	session, ok := s.unifiedMgr.GetSession(sessionID)
	if !ok {
		return &pb.ControlCommandResponse{
			ErrorCode: "SESSION_NOT_FOUND",
			ErrorMsg:  fmt.Sprintf("session not found: %s", sessionID),
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
			ErrorCode: "CODEC_NOT_FOUND",
			ErrorMsg:  fmt.Sprintf("no codec for protocol: %s", session.Protocol),
		}, nil
	}

	msg := &unified.UnifiedMessage{
		Type:           unified.MsgTypeRemoteControl,
		RemoteControl: rcMsg,
	}

	data, err := codec.Encode(msg)
	if err != nil {
		return nil, fmt.Errorf("encode failed: %w", err)
	}

	// 4. 发送命令
	ok, err = s.unifiedMgr.HandleRemoteControl(ctx, sessionID, data)
	if err != nil {
		return &pb.ControlCommandResponse{
			ErrorCode: "CONTROL_ERROR",
			ErrorMsg:  err.Error(),
		}, nil
	}

	s.auditLog(ctx, "send_command", "", req.VehicleId, req.KeyId, "success")

	return &pb.ControlCommandResponse{
		Success:  ok,
		Result:   data,
	}, nil
}

// StreamStatus 车辆状态流 (通过 unified 层处理不同协议的 TLV 帧)
func (s *UnifiedKeyService) StreamStatus(req *pb.VehicleStatusRequest, stream pb.VehicleControlService_StreamStatusServer) error {
	s.logger.Info("StreamStatus",
		zap.String("vehicle_id", req.VehicleId),
		zap.String("session_id", req.SessionId),
	)

	// TODO: 通过 unified 层解码 ICCOA/ICCE/CCC 不同协议的状态帧
	// 推送原始帧给 unifiedMgr，解析后转发
	
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
		zap.String("vendor", req.Vendor),
		zap.Int("data_len", len(req.Data)),
	)

	vendor := req.Vendor
	a, ok := s.adapterRegistry.GetByVendor(vendor)
	if !ok {
		return &pb.ForwardResponse{
			ErrorCode: "ADAPTER_NOT_FOUND",
			ErrorMsg:  fmt.Sprintf("no adapter for vendor: %s", vendor),
		}, nil
	}

	// 调用适配器的 Notify 方法
	_ = a
	// TODO: 通用转发

	return &pb.ForwardResponse{}, nil
}

// VendorCallback 厂商回调
// 接收车端或手机端通过厂商协议上报的数据
// 统一入口，由 unified 层自动检测协议并解码
func (s *UnifiedKeyService) VendorCallback(ctx context.Context, req *pb.CallbackRequest) (*pb.CallbackResponse, error) {
	s.logger.Info("VendorCallback",
		zap.String("session_id", req.SessionId),
		zap.String("vendor", req.Vendor),
		zap.Int("data_len", len(req.Data)),
	)

	sessionID := req.SessionId
	if sessionID == "" {
		sessionID = fmt.Sprintf("sess-callback-%d", time.Now().UnixMilli())
	}

	// 通过 unified 层自动检测协议并解码
	if len(req.Data) > 0 {
		// 协议自动检测
		proto := s.detectProtocol(req.Data, req.Vendor)
		s.logger.Debug("Protocol auto-detected",
			zap.String("vendor", req.Vendor),
			zap.String("protocol", proto.String()),
		)

		// 解码并处理
		status, err := s.unifiedMgr.HandleVehicleStatus(ctx, sessionID, req.Data)
		if err != nil {
			s.logger.Warn("HandleVehicleStatus failed, trying remote control",
				zap.Error(err),
			)
			ok, err2 := s.unifiedMgr.HandleRemoteControl(ctx, sessionID, req.Data)
			if err2 != nil {
				s.logger.Error("All handlers failed",
					zap.Error(err),
					zap.Error(err2),
				)
			}
			_ = ok
		}
		_ = status
	}

	return &pb.CallbackResponse{
		Success:   true,
		Timestamp: time.Now().UnixMilli(),
	}, nil
}

// HealthCheck 健康检查
func (s *UnifiedKeyService) HealthCheck(ctx context.Context, req *pb.HealthCheckRequest) (*pb.HealthCheckResponse, error) {
	adapterStatuses := s.adapterRegistry.ListStatus(ctx)
	activeSessions := s.unifiedMgr.ListSessions()

	return &pb.HealthCheckResponse{
		Status:      "healthy",
		Timestamp:   time.Now().UnixMilli(),
		SessionCount: int32(len(activeSessions)),
		AdapterCount: int32(len(adapterStatuses)),
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

func (s *UnifiedKeyService) actionToRemoteAction(a pb.RemoteAction) unified.RemoteAction {
	switch a {
	case pb.RemoteAction_LOCK:
		return unified.ActionLock
	case pb.RemoteAction_UNLOCK:
		return unified.ActionUnlock
	case pb.RemoteAction_ENGINE_START:
		return unified.ActionEngineStart
	case pb.RemoteAction_ENGINE_STOP:
		return unified.ActionEngineStop
	case pb.RemoteAction_TRUNK_OPEN:
		return unified.ActionTrunkOpen
	case pb.RemoteAction_TRUNK_CLOSE:
		return unified.ActionTrunkClose
	case pb.RemoteAction_FIND_CAR:
		return unified.ActionFindCar
	case pb.RemoteAction_CLIMATE_ON:
		return unified.ActionClimateOn
	case pb.RemoteAction_CLIMATE_OFF:
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
