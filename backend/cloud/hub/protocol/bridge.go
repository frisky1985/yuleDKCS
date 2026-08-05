package protocol

import (
	"context"
)

// DeviceBridge 设备桥接接口
// 设备发现、绑定、通信的桥接层抽象
// 对应 Device HUB 的 bridge 层级 — 连接设备与云端
type DeviceBridge interface {
	// Discover 发现设备
	// 通过 BLE/NFC/UWB 等近场通信发现附近设备
	Discover(ctx context.Context, filter *DiscoveryFilter) ([]*DeviceInfo, error)

	// Bind 绑定设备到用户/车辆
	Bind(ctx context.Context, deviceID string, params *BindParams) (*BindResult, error)

	// Unbind 解绑设备
	Unbind(ctx context.Context, deviceID string) error

	// SendCommand 发送命令到设备
	// 通过已建立的协议通道发送控制指令
	SendCommand(ctx context.Context, deviceID string, command *DeviceCommand) (*CommandResult, error)

	// ReceiveEvent 接收设备事件（阻塞式）
	ReceiveEvent(ctx context.Context, deviceID string) (*DeviceEvent, error)
}

// DiscoveryFilter 设备发现过滤条件
type DiscoveryFilter struct {
	Vendor       string   // 厂商标识过滤
	Protocol     string   // 协议类型过滤
	Capabilities []string // 能力集过滤 ("BLE", "UWB", "NFC", "SE")
	MaxResults   int      // 最大返回数量
	TimeoutMs    int64    // 发现超时(毫秒)
}

// DeviceInfo 设备信息
type DeviceInfo struct {
	DeviceID     string            `json:"device_id"`
	Vendor       string            `json:"vendor"`
	Model        string            `json:"model"`
	OS           string            `json:"os"`
	OSVersion    string            `json:"os_version"`
	AppVersion   string            `json:"app_version"`
	Capabilities map[string]bool   `json:"capabilities"`
	BLEMAC       string            `json:"ble_mac,omitempty"`
	NFCUID       string            `json:"nfc_uid,omitempty"`
	UWBID        string            `json:"uwb_id,omitempty"`
	SEID         string            `json:"se_id,omitempty"`
}

// BindParams 绑定参数
type BindParams struct {
	UserID    string `json:"user_id"`
	VehicleID string `json:"vehicle_id"`
	KeyType   string `json:"key_type"` // owner/friend/valet
	Protocol  string `json:"protocol"` // 绑定使用的协议
}

// BindResult 绑定结果
type BindResult struct {
	BindingID   string `json:"binding_id"`
	Status      string `json:"status"` // bound/pending/rejected
	DeviceID    string `json:"device_id"`
	VehicleID   string `json:"vehicle_id"`
	KeyID       string `json:"key_id,omitempty"`
	SessionID   string `json:"session_id,omitempty"`
	Protocol    string `json:"protocol"`
}

// DeviceCommand 设备命令
type DeviceCommand struct {
	Type      string            `json:"type"`      // lock/unlock/engine_start/etc
	Params    map[string]string `json:"params"`    // 命令参数
	TimeoutMs int64             `json:"timeout_ms"`
}

// CommandResult 命令执行结果
type CommandResult struct {
	CommandID string `json:"command_id"`
	Success   bool   `json:"success"`
	ErrorMsg  string `json:"error_msg,omitempty"`
	Data      []byte `json:"data,omitempty"`
}

// DeviceEvent 设备事件
type DeviceEvent struct {
	DeviceID  string            `json:"device_id"`
	Type      string            `json:"type"`
	Data      map[string]string `json:"data"`
	Timestamp int64             `json:"timestamp"`
}
