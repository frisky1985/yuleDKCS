package protocol

import (
	"context"
	"fmt"
	"time"
)

// ICCEProtocolAdapter ICCE 协议适配器
// 对接华为钱包的数字钥匙服务
// ICCE 数字钥匙标准（华为生态）
type ICCEProtocolAdapter struct {
	vendor   string
	endpoint string
	connected bool
}

// NewICCEProtocolAdapter 创建 ICCE 协议适配器
func NewICCEProtocolAdapter(vendor string) *ICCEProtocolAdapter {
	return &ICCEProtocolAdapter{
		vendor: vendor,
	}
}

// Connect 实现 ProtocolAdapter.Connect
func (a *ICCEProtocolAdapter) Connect(ctx context.Context, vendor string, endpoint string) error {
	a.endpoint = endpoint
	a.connected = true
	return nil
}

// Disconnect 实现 ProtocolAdapter.Disconnect
func (a *ICCEProtocolAdapter) Disconnect(ctx context.Context, vendor string) error {
	a.connected = false
	return nil
}

// Send 实现 ProtocolAdapter.Send
func (a *ICCEProtocolAdapter) Send(ctx context.Context, vendor string, msg *ProtocolMessage) (*ProtocolMessage, error) {
	if !a.connected {
		return nil, fmt.Errorf("ICCE adapter not connected")
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
func (a *ICCEProtocolAdapter) Receive(ctx context.Context, vendor string) (*ProtocolMessage, error) {
	if !a.connected {
		return nil, fmt.Errorf("ICCE adapter not connected")
	}
	return &ProtocolMessage{
		Vendor:    a.vendor,
		Timestamp: time.Now().UnixMilli(),
	}, nil
}

// HealthCheck 实现 ProtocolAdapter.HealthCheck
func (a *ICCEProtocolAdapter) HealthCheck(ctx context.Context, vendor string) (*HealthStatus, error) {
	return &HealthStatus{
		Vendor:      a.vendor,
		Protocol:    "icce",
		Healthy:     a.connected,
		LastCheckMs: time.Now().UnixMilli(),
	}, nil
}

// Vendor 实现 ProtocolAdapter.Vendor
func (a *ICCEProtocolAdapter) Vendor() string { return a.vendor }

// Protocol 实现 ProtocolAdapter.Protocol
func (a *ICCEProtocolAdapter) Protocol() string { return "icce" }

// Discover 实现 DeviceBridge.Discover
func (a *ICCEProtocolAdapter) Discover(ctx context.Context, filter *DiscoveryFilter) ([]*DeviceInfo, error) {
	return nil, nil
}

// Bind 实现 DeviceBridge.Bind
func (a *ICCEProtocolAdapter) Bind(ctx context.Context, deviceID string, params *BindParams) (*BindResult, error) {
	return &BindResult{
		BindingID: fmt.Sprintf("bind-icce-%s-%d", deviceID, time.Now().UnixMilli()),
		Status:    "bound",
		DeviceID:  deviceID,
		VehicleID: params.VehicleID,
		Protocol:  "icce",
	}, nil
}

// Unbind 实现 DeviceBridge.Unbind
func (a *ICCEProtocolAdapter) Unbind(ctx context.Context, deviceID string) error {
	return nil
}

// SendCommand 实现 DeviceBridge.SendCommand
func (a *ICCEProtocolAdapter) SendCommand(ctx context.Context, deviceID string, command *DeviceCommand) (*CommandResult, error) {
	return &CommandResult{
		Success: true,
	}, nil
}

// ReceiveEvent 实现 DeviceBridge.ReceiveEvent
func (a *ICCEProtocolAdapter) ReceiveEvent(ctx context.Context, deviceID string) (*DeviceEvent, error) {
	return &DeviceEvent{
		DeviceID:  deviceID,
		Timestamp: time.Now().UnixMilli(),
	}, nil
}
