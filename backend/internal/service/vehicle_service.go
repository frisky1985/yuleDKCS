package service

import "context"

// VehicleService 车辆服务接口
// 提供 FriendSharingService 所需的车辆端操作
type VehicleService struct {
	// 当前为最小实现，后续扩展需添加实际业务逻辑
}

// NewVehicleService 创建车辆服务实例
func NewVehicleService() *VehicleService {
	return &VehicleService{}
}

// ConfigureTemporaryKey 配置临时钥匙到车辆
func (s *VehicleService) ConfigureTemporaryKey(ctx context.Context, vehicleID, friendID string, tempKey string, permissions string) error {
	// TODO: 调用车辆端接口配置临时钥匙（BLE/UWB写入）
	// 实际实现需要与 embedded 端通信
	return nil
}

// RevokeTemporaryKey 撤销车辆上的临时钥匙
func (s *VehicleService) RevokeTemporaryKey(ctx context.Context, vehicleID, friendID string) error {
	// TODO: 通知车辆端删除临时钥匙
	return nil
}

// UpdateKeyPermissions 更新车辆端的钥匙权限
func (s *VehicleService) UpdateKeyPermissions(ctx context.Context, vehicleID, friendID string, permissions []string) error {
	// TODO: 通知车辆端更新权限
	return nil
}
