// Package protocol 定义设备厂商协议适配层接口
// 这是 Device HUB 级别设备厂商集成平台的核心抽象层,
// 对应银基科技 Device HUB 的厂商适配接口规范.
package protocol

import (
	"context"
)

// ProtocolAdapter 协议适配器接口
// 每个手机厂商/设备厂商实现此接口以对接不同协议栈
// 对应 Device HUB 的 driver/adapter 层级
type ProtocolAdapter interface {
	// Connect 建立与设备/厂商后端的连接
	// vendor: 厂商标识 (apple/samsung/xiaomi/oppo/vivo/huawei)
	// endpoint: 厂商后端端点 URL
	Connect(ctx context.Context, vendor string, endpoint string) error

	// Disconnect 断开与设备/厂商后端的连接
	Disconnect(ctx context.Context, vendor string) error

	// Send 发送协议消息到设备/厂商后端
	// 返回厂商后端的原始响应数据
	Send(ctx context.Context, vendor string, message *ProtocolMessage) (*ProtocolMessage, error)

	// Receive 从设备/厂商后端接收协议消息（阻塞式）
	Receive(ctx context.Context, vendor string) (*ProtocolMessage, error)

	// HealthCheck 检查与厂商后端的连接健康状态
	HealthCheck(ctx context.Context, vendor string) (*HealthStatus, error)

	// Vendor 返回厂商标识
	Vendor() string

	// Protocol 返回协议名称
	Protocol() string
}

// ProtocolMessage 协议消息的通用容器
// 包含协议层的元数据和原始载荷
type ProtocolMessage struct {
	Type      MessageType // 消息类型
	Vendor    string      // 厂商标识
	SessionID string      // 会话ID
	Payload   []byte      // 协议原始载荷
	Sequence  uint64      // 序列号
	Timestamp int64       // Unix毫秒时间戳
	TraceID   string      // 链路追踪ID
}

// MessageType 协议消息类型枚举
type MessageType int

const (
	MsgTypeUnknown      MessageType = iota
	MsgTypeDeviceInfo               // 设备信息上报
	MsgTypeCapability               // 能力协商
	MsgTypeKeyBind                  // 密钥绑定
	MsgTypeKeyUnbind                // 密钥解绑
	MsgTypeKeyShare                 // 密钥分享
	MsgTypeKeyAccept                // 接收分享
	MsgTypeKeyRevoke                // 密钥撤销
	MsgTypeRemoteControl            // 远程控制
	MsgTypeVehicleStatus            // 车辆状态
	MsgTypeHeartbeat                // 心跳保活
)

// HealthStatus 健康检查状态
type HealthStatus struct {
	Vendor      string `json:"vendor"`
	Protocol    string `json:"protocol"`
	Healthy     bool   `json:"healthy"`
	ErrorMsg    string `json:"error_msg,omitempty"`
	LastCheckMs int64  `json:"last_check_ms"`
	LatencyMs   int64  `json:"latency_ms,omitempty"`
}
