// Package device 定义设备管理层接口
// 对应 Device HUB 的设备管理能力 — 设备注册、状态管理、能力抽象
package device

import "time"

// Device 设备核心模型
type Device struct {
	DeviceID     string            `json:"device_id"`
	UserID       string            `json:"user_id"`
	Vendor       string            `json:"vendor"`        // 手机厂商
	Platform     string            `json:"platform"`      // ios/android
	Model        string            `json:"model"`         // 机型
	OSVersion    string            `json:"os_version"`
	AppVersion   string            `json:"app_version"`
	Capabilities *CapabilitySet    `json:"capabilities"`
	Status       DeviceStatus      `json:"status"`
	LastSeen     time.Time         `json:"last_seen"`
	RegisteredAt time.Time         `json:"registered_at"`
	Tags         map[string]string `json:"tags,omitempty"`
}

// DeviceStatus 设备状态
type DeviceStatus string

const (
	DeviceStatusActive     DeviceStatus = "active"      // 活跃
	DeviceStatusInactive   DeviceStatus = "inactive"    // 不活跃
	DeviceStatusSuspended  DeviceStatus = "suspended"   // 挂起
	DeviceStatusRevoked    DeviceStatus = "revoked"     // 吊销
	DeviceStatusDisabled   DeviceStatus = "disabled"    // 禁用
)

// CapabilitySet 设备能力集
type CapabilitySet struct {
	BLE  bool `json:"ble"`  // BLE 支持
	UWB  bool `json:"uwb"`  // UWB 支持
	NFC  bool `json:"nfc"`  // NFC 支持
	SE   bool `json:"se"`   // Secure Element 支持
	FiRa bool `json:"fira"` // FiRa 协议栈

	BLEVersion string `json:"ble_version,omitempty"` // e.g. "5.0", "5.3"
	UWBVersion string `json:"uwb_version,omitempty"` // e.g. "FiRa 1.0"

	UWBAccuracyMM int `json:"uwb_accuracy_mm,omitempty"` // UWB 定位精度(mm)
}

// DeviceCapability 设备能力枚举
type DeviceCapability string

const (
	CapBLE  DeviceCapability = "BLE"
	CapUWB  DeviceCapability = "UWB"
	CapNFC  DeviceCapability = "NFC"
	CapSE   DeviceCapability = "SE"
	CapFiRa DeviceCapability = "FiRa"
)

// DeviceBinding 设备绑定关系
type DeviceBinding struct {
	BindingID string    `json:"binding_id"`
	DeviceID  string    `json:"device_id"`
	UserID    string    `json:"user_id"`
	VehicleID string    `json:"vehicle_id"`
	KeyID     string    `json:"key_id"`
	Protocol  string    `json:"protocol"`
	Status    string    `json:"status"` // active/revoked/suspended
	BoundAt   time.Time `json:"bound_at"`
	ExpiresAt time.Time `json:"expires_at,omitempty"`
}

// DeviceEvent 设备事件
type DeviceEvent struct {
	EventID   string            `json:"event_id"`
	DeviceID  string            `json:"device_id"`
	Type      string            `json:"type"`
	Data      map[string]string `json:"data"`
	Timestamp time.Time         `json:"timestamp"`
}
