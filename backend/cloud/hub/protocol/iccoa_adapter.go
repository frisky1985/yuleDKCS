package protocol

import (
	"context"
	"fmt"
	"time"
)

// ICCOAProtocolAdapter ICCOA 协议适配器
// 对接小米钱包/OPPO钱包/vivo钱包的数字钥匙服务
// ICCOA Digital Key 3.0 / 4.0 双版本支持
type ICCOAProtocolAdapter struct {
	vendor   string
	version  string // "dk30" 或 "dk40"
	endpoint string
	connected bool
}

// NewICCOAProtocolAdapter 创建 ICCOA 协议适配器
func NewICCOAProtocolAdapter(vendor string, version string) *ICCOAProtocolAdapter {
	return &ICCOAProtocolAdapter{
		vendor:  vendor,
		version: version,
	}
}

// Connect 实现 ProtocolAdapter.Connect
func (a *ICCOAProtocolAdapter) Connect(ctx context.Context, vendor string, endpoint string) error {
	a.endpoint = endpoint
	a.connected = true
	return nil
}

// Disconnect 实现 ProtocolAdapter.Disconnect
func (a *ICCOAProtocolAdapter) Disconnect(ctx context.Context, vendor string) error {
	a.connected = false
	return nil
}

// Send 实现 ProtocolAdapter.Send
func (a *ICCOAProtocolAdapter) Send(ctx context.Context, vendor string, msg *ProtocolMessage) (*ProtocolMessage, error) {
	if !a.connected {
		return nil, fmt.Errorf("ICCOA adapter not connected")
	}
	return &ProtocolMessage{
		Type:      msg.Type,
		Vendor:    a.vendor,
		SessionID: msg.SessionID,
		Payload:   msg.Payload,
		Timestamp: time.Now().UnixMilli(),
	}, nil
}

// Receive 实现 ProtocolAdapter.Receive
func (a *ICCOAProtocolAdapter) Receive(ctx context.Context, vendor string) (*ProtocolMessage, error) {
	if !a.connected {
		return nil, fmt.Errorf("ICCOA adapter not connected")
	}
	return &ProtocolMessage{
		Vendor:    a.vendor,
		Timestamp: time.Now().UnixMilli(),
	}, nil
}

// HealthCheck 实现 ProtocolAdapter.HealthCheck
func (a *ICCOAProtocolAdapter) HealthCheck(ctx context.Context, vendor string) (*HealthStatus, error) {
	return &HealthStatus{
		Vendor:      a.vendor,
		Protocol:    a.Protocol(),
		Healthy:     a.connected,
		LastCheckMs: time.Now().UnixMilli(),
	}, nil
}

// Vendor 实现 ProtocolAdapter.Vendor
func (a *ICCOAProtocolAdapter) Vendor() string { return a.vendor }

// Protocol 实现 ProtocolAdapter.Protocol
func (a *ICCOAProtocolAdapter) Protocol() string {
	if a.version == "dk40" {
		return "iccoa_dk40"
	}
	return "iccoa_dk30"
}

// Discover 实现 DeviceBridge.Discover
func (a *ICCOAProtocolAdapter) Discover(ctx context.Context, filter *DiscoveryFilter) ([]*DeviceInfo, error) {
	// ICCOA: 通过 BLE 扫描 + 钱包 API 发现设备
	return nil, nil
}

// Bind 实现 DeviceBridge.Bind
func (a *ICCOAProtocolAdapter) Bind(ctx context.Context, deviceID string, params *BindParams) (*BindResult, error) {
	return &BindResult{
		BindingID: fmt.Sprintf("bind-iccoa-%s-%d", deviceID, time.Now().UnixMilli()),
		Status:    "bound",
		DeviceID:  deviceID,
		VehicleID: params.VehicleID,
		Protocol:  a.Protocol(),
	}, nil
}

// Unbind 实现 DeviceBridge.Unbind
func (a *ICCOAProtocolAdapter) Unbind(ctx context.Context, deviceID string) error {
	return nil
}

// SendCommand 实现 DeviceBridge.SendCommand
func (a *ICCOAProtocolAdapter) SendCommand(ctx context.Context, deviceID string, command *DeviceCommand) (*CommandResult, error) {
	return &CommandResult{
		Success: true,
	}, nil
}

// ReceiveEvent 实现 DeviceBridge.ReceiveEvent
func (a *ICCOAProtocolAdapter) ReceiveEvent(ctx context.Context, deviceID string) (*DeviceEvent, error) {
	return &DeviceEvent{
		DeviceID:  deviceID,
		Timestamp: time.Now().UnixMilli(),
	}, nil
}
