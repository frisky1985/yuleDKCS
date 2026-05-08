package unified

import (
	"context"
	"fmt"
	"sync"
	"time"

	"go.uber.org/zap"
	
	pb "github.com/digitalkey/hub/api/v1"
)

// Manager 统一协议管理器 - 顶层入口
// 整合协议协商、消息路由、协议转换、会话管理
type Manager struct {
	logger         *zap.Logger
	codecRegistry  *CodecRegistry
	sessionManager *SessionManager
	negotiator     *Negotiator
	adapterRegistry interface{ Get(protocol, vendor string) (interface{ BindKey(ctx context.Context, req *pb.BindKeyRequest) (*pb.BindKeyResponse, error) }, bool) }
	keyService     interface{ GetKey(ctx context.Context, keyID string) (*pb.DigitalKey, error) }
	
	// 协议特定编解码器
	codecs map[ProtocolType]MessageCodec
	
	mu sync.RWMutex
}

// Config Manager 配置
type Config struct {
	Logger           *zap.Logger
	SessionTimeout   time.Duration  // 默认 30 分钟
	SupportedProtocols []ProtocolType
}

// NewManager 创建统一管理器
func NewManager(cfg *Config) *Manager {
	negotiator := NewNegotiator(cfg.SupportedProtocols)
	if negotiator == nil {
		negotiator = NewNegotiator([]ProtocolType{
			ProtocolCCC3, ProtocolICCOA40, ProtocolICCOA30, ProtocolICCE,
		})
	}
	
	m := &Manager{
		logger:    cfg.Logger,
		codecRegistry: NewCodecRegistry(),
		sessionManager: NewSessionManager(cfg.SessionTimeout),
		negotiator:    negotiator,
		codecs:        make(map[ProtocolType]MessageCodec),
	}
	
	// 注册默认编解码器
	m.registerDefaultCodecs()
	
	return m
}

// registerDefaultCodecs 注册默认编解码器
func (m *Manager) registerDefaultCodecs() {
	m.codecs[ProtocolICCOA30] = NewICCOACodec(ProtocolICCOA30)
	m.codecs[ProtocolICCOA40] = NewICCOACodec(ProtocolICCOA40)
	m.codecs[ProtocolICCE]    = NewICCECodec()
	m.codecs[ProtocolCCC3]    = NewCCCCodec()
	
	for proto, codec := range m.codecs {
		m.codecRegistry.Register(proto, codec)
	}
}

// RegisterCodec 注册协议编解码器
func (m *Manager) RegisterCodec(proto ProtocolType, codec MessageCodec) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.codecs[proto] = codec
	m.codecRegistry.Register(proto, codec)
}

// ============================================================
// 核心 API - 统一入口
// ============================================================

// NegotiateProtocol 协议协商 API
// 设备上线时调用，根据设备能力和车辆支持情况协商最佳协议
func (m *Manager) NegotiateProtocol(ctx context.Context, req *NegotiateRequest) (*NegotiateResponse, error) {
	// 1. 构建设备能力集
	deviceCaps := &CapabilitySet{
		BLE:         req.DeviceCaps.BLE,
		UWB:         req.DeviceCaps.UWB,
		NFC:         req.DeviceCaps.NFC,
		SE:          req.DeviceCaps.SE,
		FiRa:        req.DeviceCaps.FiRa,
		BLEVersion:  req.DeviceCaps.BLEVersion,
		UWBVersion:  req.DeviceCaps.UWBVersion,
		UWBAccuracy: req.DeviceCaps.UWBAccuracy,
	}
	
	// 2. 构建车辆能力集
	vehicleCaps := &CapabilitySet{
		BLE:  req.VehicleCaps.BLE,
		UWB:  req.VehicleCaps.UWB,
		NFC:  req.VehicleCaps.NFC,
		SE:   req.VehicleCaps.SE,
		FiRa: req.VehicleCaps.FiRa,
	}
	
	// 3. 协商
	result, err := m.negotiator.Negotiate(deviceCaps, vehicleCaps)
	if err != nil {
		return nil, fmt.Errorf("protocol negotiation failed: %w", err)
	}
	
	// 4. 创建会话
	sessionID := fmt.Sprintf("sess-%s-%d", req.DeviceID, time.Now().UnixMilli())
	device := &Device{
		DeviceID:    req.DeviceID,
		Vendor:      req.Vendor,
		OS:          req.OS,
		Protocol:    result.Protocol,
		Capabilities: deviceCaps,
	}
	session := m.sessionManager.CreateSession(sessionID, device)
	session.Protocol = result.Protocol
	
	return &NegotiateResponse{
		SessionID:   sessionID,
		Protocol:    result.Protocol,
		Version:     result.Version,
		MatchScore:  result.MatchScore,
		Reason:      result.Reason,
	}, nil
}

// BindKey 统一密钥绑定入口
// 根据会话中协商的协议，路由到对应适配器
func (m *Manager) BindKey(ctx context.Context, sessionID string, req *pb.BindKeyRequest) (*pb.BindKeyResponse, error) {
	session, ok := m.sessionManager.GetSession(sessionID)
	if !ok {
		return nil, fmt.Errorf("session not found: %s", sessionID)
	}
	
	if !session.Valid() {
		return nil, fmt.Errorf("session invalid or expired: %s", sessionID)
	}
	
	// 状态转换: 发起绑定
	m.sessionManager.Transition(sessionID, EventBindStart)
	
	proto := session.Protocol
	adapterKey := protoToAdapterKey(proto)
	
	// 从 adapter registry 获取适配器
	// TODO: adapterRegistry 需要实现
	_ = adapterKey
	
	// 根据协议类型路由
	switch proto {
	case ProtocolICCOA40:
		return m.bindKeyICCOA(ctx, req)
	case ProtocolICCOA30:
		return m.bindKeyICCOA(ctx, req)
	case ProtocolICCE:
		return m.bindKeyICCE(ctx, req)
	case ProtocolCCC3:
		return m.bindKeyCCC(ctx, req)
	default:
		return nil, fmt.Errorf("unsupported protocol: %s", proto)
	}
}

func (m *Manager) bindKeyICCOA(ctx context.Context, req *pb.BindKeyRequest) (*pb.BindKeyResponse, error) {
	// ICCOA 协议特定处理
	codec, ok := m.codecs[ProtocolICCOA40]
	if !ok {
		return nil, fmt.Errorf("ICCOA codec not registered")
	}
	
	// 构建统一消息
	msg := &UnifiedMessage{
		Type: MsgTypeKeyBind,
		KeyBind: &KeyBindMessage{
			VehicleID:    req.VehicleId,
			UserID:      req.UserId,
			KeyType:     req.KeyType,
			AccessLevel: req.AccessLevel,
			DevicePubKey: req.DevicePubkey,
			ValidFrom:   req.ValidFrom,
			ValidUntil:  req.ValidUntil,
		},
	}
	
	// 编码为 ICCOA 格式
	data, err := codec.Encode(msg)
	if err != nil {
		return nil, fmt.Errorf("encode failed: %w", err)
	}
	
	m.logger.Info("ICCOA BindKey encoded",
		zap.String("vehicle_id", req.VehicleId),
		zap.ByteString("data", data),
	)
	
	// TODO: 发送到 ICCOA 适配器
	// adapter.BindingFlow(ctx, data)
	
	return &pb.BindKeyResponse{
		Key: &pb.DigitalKey{
			KeyId:     fmt.Sprintf("key-iccoa-%d", time.Now().UnixMilli()),
			VehicleId: req.VehicleId,
			DeviceId:  req.DeviceId,
			UserId:    req.UserId,
			Protocol:  pb.Protocol_ICCOA_DK40,
			Status:    pb.KeyStatus_ACTIVE,
			CreatedAt: time.Now().Unix(),
		},
	}, nil
}

func (m *Manager) bindKeyICCE(ctx context.Context, req *pb.BindKeyRequest) (*pb.BindKeyResponse, error) {
	codec, ok := m.codecs[ProtocolICCE]
	if !ok {
		return nil, fmt.Errorf("ICCE codec not registered")
	}
	
	msg := &UnifiedMessage{
		Type: MsgTypeKeyBind,
		KeyBind: &KeyBindMessage{
			VehicleID:    req.VehicleId,
			UserID:      req.UserId,
			DevicePubKey: req.DevicePubkey,
			ValidFrom:   req.ValidFrom,
			ValidUntil:  req.ValidUntil,
		},
	}
	
	data, err := codec.Encode(msg)
	if err != nil {
		return nil, fmt.Errorf("encode failed: %w", err)
	}
	
	m.logger.Info("ICCE BindKey encoded",
		zap.String("vehicle_id", req.VehicleId),
		zap.ByteString("data", data),
	)
	
	return &pb.BindKeyResponse{
		Key: &pb.DigitalKey{
			KeyId:     fmt.Sprintf("key-icce-%d", time.Now().UnixMilli()),
			VehicleId: req.VehicleId,
			DeviceId:  req.DeviceId,
			UserId:    req.UserId,
			Protocol:  pb.Protocol_ICCE,
			Status:    pb.KeyStatus_ACTIVE,
			CreatedAt: time.Now().Unix(),
		},
	}, nil
}

func (m *Manager) bindKeyCCC(ctx context.Context, req *pb.BindKeyRequest) (*pb.BindKeyResponse, error) {
	codec, ok := m.codecs[ProtocolCCC3]
	if !ok {
		return nil, fmt.Errorf("CCC codec not registered")
	}
	
	msg := &UnifiedMessage{
		Type: MsgTypeKeyBind,
		KeyBind: &KeyBindMessage{
			VehicleID:    req.VehicleId,
			UserID:      req.UserId,
			DevicePubKey: req.DevicePubkey,
		},
	}
	
	data, err := codec.Encode(msg)
	if err != nil {
		return nil, fmt.Errorf("encode failed: %w", err)
	}
	
	m.logger.Info("CCC BindKey encoded",
		zap.String("vehicle_id", req.VehicleId),
		zap.ByteString("data", data),
	)
	
	return &pb.BindKeyResponse{
		Key: &pb.DigitalKey{
			KeyId:     fmt.Sprintf("key-ccc-%d", time.Now().UnixMilli()),
			VehicleId: req.VehicleId,
			DeviceId:  req.DeviceId,
			UserId:    req.UserId,
			Protocol:  pb.Protocol_CCC_DK3,
			Status:    pb.KeyStatus_ACTIVE,
			CreatedAt: time.Now().Unix(),
		},
	}, nil
}

// ShareKey 统一密钥分享入口
func (m *Manager) ShareKey(ctx context.Context, sessionID string, req *pb.CreateShareRequest) (*pb.CreateShareResponse, error) {
	session, ok := m.sessionManager.GetSession(sessionID)
	if !ok {
		return nil, fmt.Errorf("session not found")
	}
	
	// 获取密钥信息
	key, err := m.getKeyFromService(ctx, req.KeyId)
	if err != nil {
		return nil, fmt.Errorf("key not found: %s", req.KeyId)
	}
	
	// 根据协议生成分享协议帧
	proto := ProtocolFromProto(key.Protocol)
	codec, ok := m.codecs[proto]
	if !ok {
		return nil, fmt.Errorf("codec not found for protocol: %s", key.Protocol)
	}
	
	msg := &UnifiedMessage{
		Type: MsgTypeKeyShare,
		KeyShare: &KeyShareMessage{
			KeyID:       req.KeyId,
			RecipientID: req.RecipientId,
			AccessLevel: req.AccessLevel,
			ValidUntil:  req.ValidUntil,
		},
	}
	
	data, err := codec.Encode(msg)
	if err != nil {
		return nil, fmt.Errorf("encode failed: %w", err)
	}
	
	m.logger.Info("ShareKey encoded",
		zap.String("key_id", req.KeyId),
		zap.String("recipient", req.RecipientId),
		zap.String("protocol", proto.String()),
		zap.ByteString("data", data),
	)
	
	return &pb.CreateShareResponse{
		ShareId:   fmt.Sprintf("share-%d", time.Now().UnixMilli()),
		ShareCode: fmt.Sprintf("%06d", time.Now().UnixNano()%1000000),
	}, nil
}

// HandleVehicleStatus 处理车辆状态上报
// 统一入口，解码后根据会话关联的协议进行对应处理
func (m *Manager) HandleVehicleStatus(ctx context.Context, sessionID string, data []byte) (*pb.VehicleStatusUpdate, error) {
	session, ok := m.sessionManager.GetSession(sessionID)
	if !ok {
		return nil, fmt.Errorf("session not found")
	}
	
	proto := session.Protocol
	codec, ok := m.codecs[proto]
	if !ok {
		return nil, fmt.Errorf("codec not found for protocol: %s", proto)
	}
	
	msg, err := codec.Decode(data)
	if err != nil {
		return nil, fmt.Errorf("decode failed: %w", err)
	}
	
	vs := msg.VehicleStatus
	if vs == nil {
		return nil, fmt.Errorf("invalid vehicle status message")
	}
	
	m.logger.Info("VehicleStatus received",
		zap.String("vehicle_id", vs.VehicleID),
		zap.Bool("doors_locked", vs.DoorsLocked),
		zap.Bool("engine_on", vs.EngineOn),
		zap.String("protocol", proto.String()),
	)
	
	return &pb.VehicleStatusUpdate{
		VehicleId:   vs.VehicleID,
		KeyId:       vs.KeyID,
		DoorsLocked: vs.DoorsLocked,
		EngineOn:    vs.EngineOn,
		Battery:     int32(vs.BatteryLevel),
	}, nil
}

// HandleRemoteControl 解码并处理远程控制命令
func (m *Manager) HandleRemoteControl(ctx context.Context, sessionID string, data []byte) (bool, error) {
	session, ok := m.sessionManager.GetSession(sessionID)
	if !ok {
		return false, fmt.Errorf("session not found")
	}
	
	proto := session.Protocol
	codec, ok := m.codecs[proto]
	if !ok {
		return false, fmt.Errorf("codec not found for protocol: %s", proto)
	}
	
	msg, err := codec.Decode(data)
	if err != nil {
		return false, fmt.Errorf("decode failed: %w", err)
	}
	
	rc := msg.RemoteControl
	if rc == nil {
		return false, fmt.Errorf("invalid remote control message")
	}
	
	m.logger.Info("RemoteControl received",
		zap.String("vehicle_id", rc.VehicleID),
		zap.String("action", rc.Action.String()),
		zap.String("protocol", proto.String()),
	)
	
	// TODO: 发送到车辆控制服务
	
	return true, nil
}

// GetSession 获取会话信息
func (m *Manager) GetSession(sessionID string) (*Session, bool) {
	return m.sessionManager.GetSession(sessionID)
}

// ListSessions 列出所有活跃会话
func (m *Manager) ListSessions() []*Session {
	return m.sessionManager.ListActive()
}

// Cleanup 清理过期会话
func (m *Manager) Cleanup() int {
	return m.sessionManager.Cleanup()
}

// ---- Request/Response Types ----

// NegotiateRequest 协议协商请求
type NegotiateRequest struct {
	DeviceID    string
	Vendor      string  // xiaomi/oppo/vivo/huawei/samsung/apple
	OS          string  // ios/android
	AppVersion  string
	DeviceCaps  *NegotiateCapabilities
	VehicleCaps *NegotiateCapabilities
}

// NegotiateCapabilities 协商能力
type NegotiateCapabilities struct {
	BLE         bool
	UWB         bool
	NFC         bool
	SE          bool
	FiRa        bool
	BLEVersion  string
	UWBVersion  string
	UWBAccuracy int  // mm
}

// NegotiateResponse 协议协商响应
type NegotiateResponse struct {
	SessionID  string
	Protocol   ProtocolType
	Version    string
	MatchScore int
	Reason     string
}

// ---- Helpers ----

func protoToAdapterKey(proto ProtocolType) string {
	switch proto {
	case ProtocolCCC3:
		return "vendor:ccc"
	case ProtocolICCOA30:
		return "vendor:iccoa30"
	case ProtocolICCOA40:
		return "vendor:iccoa40"
	case ProtocolICCE:
		return "vendor:icce"
	default:
		return "vendor:unknown"
	}
}

func (m *Manager) getKeyFromService(ctx context.Context, keyID string) (*pb.DigitalKey, error) {
	// TODO: 实际调用 key service
	return &pb.DigitalKey{
		KeyId: keyID,
	}, nil
}

// MarshalJSON 为 NegotiateResponse 实现 JSON 序列化
func (r *NegotiateResponse) MarshalJSON() ([]byte, error) {
	type alias NegotiateResponse
	return []byte(fmt.Sprintf(`{"session_id":"%s","protocol":"%s","version":"%s","score":%d,"reason":"%s"}`,
		r.SessionID, r.Protocol, r.Version, r.MatchScore, r.Reason)), nil
}
