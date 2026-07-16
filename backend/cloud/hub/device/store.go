package device

import (
	"context"
)

// Store 设备持久化存储接口
// 提供设备数据的 CRUD 操作
type Store interface {
	// SaveDevice 保存/更新设备信息
	SaveDevice(ctx context.Context, device *Device) error

	// GetDevice 获取设备信息
	GetDevice(ctx context.Context, deviceID string) (*Device, error)

	// DeleteDevice 删除设备
	DeleteDevice(ctx context.Context, deviceID string) error

	// ListDevicesByUser 查询用户设备列表
	ListDevicesByUser(ctx context.Context, userID string) ([]*Device, error)

	// SaveBinding 保存设备绑定关系
	SaveBinding(ctx context.Context, binding *DeviceBinding) error

	// GetBinding 获取绑定关系
	GetBinding(ctx context.Context, bindingID string) (*DeviceBinding, error)

	// ListBindingsByDevice 查询设备的所有绑定
	ListBindingsByDevice(ctx context.Context, deviceID string) ([]*DeviceBinding, error)

	// ListBindingsByVehicle 查询车辆的所有绑定设备
	ListBindingsByVehicle(ctx context.Context, vehicleID string) ([]*DeviceBinding, error)

	// DeleteBinding 删除绑定关系
	DeleteBinding(ctx context.Context, bindingID string) error
}
