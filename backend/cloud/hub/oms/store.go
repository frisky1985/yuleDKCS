package oms

import "context"

// ── 持久化存储接口 ─────────────────────────────────────────────────────────
// 对标银基 OMS: 全生命周期数据的持久化层
// 支持多种后端实现：PostgreSQL (JSONB) / MongoDB / InMemory
// 生产环境推荐 PostgreSQL + Redis 缓存组合

// KeyStore 数字钥匙持久化存储
type KeyStore interface {
	// SaveKey 保存钥匙记录（创建或更新）
	SaveKey(ctx context.Context, key KeyRecord) error

	// GetKey 按 KeyID 查询钥匙记录
	GetKey(ctx context.Context, keyID string) (*KeyRecord, error)

	// ListKeys 按筛选条件列出钥匙
	ListKeys(ctx context.Context, filter KeyFilter) ([]KeyRecord, error)

	// DeleteKey 软删除钥匙记录（标记为 deleted 状态）
	DeleteKey(ctx context.Context, keyID string) error

	// CountKeys 统计钥匙数量
	CountKeys(ctx context.Context, filter KeyFilter) (int64, error)
}

// ProvisioningStore 预置任务持久化存储
type ProvisioningStore interface {
	// SaveJob 保存预置任务
	SaveJob(ctx context.Context, job ProvisioningJob) error

	// GetJob 按 JobID 查询预置任务
	GetJob(ctx context.Context, jobID string) (*ProvisioningJob, error)

	// ListJobs 按筛选条件列出预置任务
	ListJobs(ctx context.Context, filter ProvisioningJobFilter) ([]ProvisioningJob, error)

	// UpdateJobStatus 更新预置任务状态
	UpdateJobStatus(ctx context.Context, jobID string, status ProvisioningStatus, errMsg string) error
}

// DeploymentStore 部署记录持久化存储
type DeploymentStore interface {
	// SaveDeployment 保存部署记录
	SaveDeployment(ctx context.Context, deploy DeploymentRecord) error

	// GetDeployment 查询部署记录
	GetDeployment(ctx context.Context, deployID string) (*DeploymentRecord, error)

	// ListDeployments 列出 OEM 的部署历史
	ListDeployments(ctx context.Context, oemID string, limit, offset int) ([]DeploymentRecord, error)

	// UpdateDeploymentStatus 更新部署状态
	UpdateDeploymentStatus(ctx context.Context, deployID string, status string) error
}

// UsageStore 使用记录持久化存储
type UsageStore interface {
	// SaveUsage 保存使用记录
	SaveUsage(ctx context.Context, usage UsageRecord) error

	// QueryUsage 查询使用记录
	QueryUsage(ctx context.Context, filter UsageFilter) ([]UsageRecord, error)

	// GetUsageStats 聚合统计
	GetUsageStats(ctx context.Context, keyID string, since, until int64) (*UsageStats, error)

	// GetAggregatedStats 全局聚合
	GetAggregatedStats(ctx context.Context, since, until int64, oemID string) (*AggregatedStats, error)

	// PurgeOldRecords 清理过期使用记录
	PurgeOldRecords(ctx context.Context, beforeTimestamp int64) (int64, error)
}
