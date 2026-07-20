package device

import (
	"go.uber.org/zap"
)

// Service 设备管理服务
type Service struct {
	logger *zap.Logger
}

func NewService(logger *zap.Logger) *Service {
	return &Service{
		logger: logger.With(zap.String("service", "DeviceMgmt")),
	}
}

// RegisterDevice 注册新设备 (绑定前调用)
// VerifyAttestation 验证设备SE证明
// UpdateDeviceStatus 更新设备状态
// GetDeviceByBLEMAC 通过BLE MAC查找设备
