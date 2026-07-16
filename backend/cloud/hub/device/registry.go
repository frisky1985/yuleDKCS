package device

import (
	"context"
)

// Registry 设备注册与发现接口
// 提供设备生命周期管理和查询能力
type Registry interface {
	// Register 注册新设备
	// 设备首次上线时调用，记录设备信息和能力
	Register(ctx context.Context, device *Device) error

	// Unregister 注销设备
	// 设备被移除时调用
	Unregister(ctx context.Context, deviceID string) error

	// Get 获取单个设备信息
	Get(ctx context.Context, deviceID string) (*Device, error)

	// ListByUser 查询用户所有设备
	ListByUser(ctx context.Context, userID string) ([]*Device, error)

	// ListByCapability 按能力查询设备
	// 用于协议协商时筛选支持特定能力的设备
	ListByCapability(ctx context.Context, caps []DeviceCapability) ([]*Device, error)

	// UpdateStatus 更新设备状态
	UpdateStatus(ctx context.Context, deviceID string, status DeviceStatus) error

	// UpdateLastSeen 更新设备最后活跃时间
	UpdateLastSeen(ctx context.Context, deviceID string) error

	// CountByUser 统计用户设备数
	CountByUser(ctx context.Context, userID string) (int, error)
}

// Discovery 设备发现接口
// 用于 BLE/NFC/UWB 近场发现
type Discovery interface {
	// Scan 启动设备扫描
	Scan(ctx context.Context, filter *DiscoveryFilter) (<-chan *Device, error)

	// StopScan 停止设备扫描
	StopScan(ctx context.Context) error
}

// DiscoveryFilter 发现过滤条件
type DiscoveryFilter struct {
	Vendors      []string           // 厂商白名单
	Capabilities []DeviceCapability // 能力需求
	Timeout      int                // 扫描超时(秒)
	MaxResults   int                // 最大结果数
}
