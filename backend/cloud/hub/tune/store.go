// yuleTUNE — 标定校准平台 (Calibration Hub)
// Store — 标定数据持久化接口。

package tune

import "context"

// RecordStore 标定记录持久化存储接口。
// 生产环境可实现为 MongoDB / MySQL / 或 gRPC 远端存储。
type RecordStore interface {
	// SaveRecord 保存一条标定记录
	SaveRecord(ctx context.Context, record *CalibrationRecord) error

	// GetRecords 查询某型号某类型的标定记录
	GetRecords(ctx context.Context, modelID string, calibType CalibrationType, limit int) ([]CalibrationRecord, error)

	// DeleteRecords 删除某型号的标定记录（例如重标定时清空历史）
	DeleteRecords(ctx context.Context, modelID string, calibType CalibrationType) error
}

// ProfileStore 标定档案持久化存储接口。
type ProfileStore interface {
	// SaveProfile 保存/更新档案
	SaveProfile(ctx context.Context, profile *CalibrationProfile) error

	// GetProfile 获取档案
	GetProfile(ctx context.Context, modelID string, calibType CalibrationType) (*CalibrationProfile, error)
}

// ModelStore 手机型号配置持久化存储接口。
type ModelStore interface {
	// SaveModel 保存手机型号配置
	SaveModel(ctx context.Context, model *DeviceModel) error

	// GetModel 获取手机型号配置
	GetModel(ctx context.Context, modelID string) (*DeviceModel, error)

	// ListModels 列出型号（可按厂商筛选）
	ListModels(ctx context.Context, manufacturer string) ([]DeviceModel, error)

	// DeleteModel 删除型号
	DeleteModel(ctx context.Context, modelID string) error
}

// Store 聚合统一的存储接口，方便在生产代码中注入。
type Store interface {
	RecordStore
	ProfileStore
	ModelStore
}
