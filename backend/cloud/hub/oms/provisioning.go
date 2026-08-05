package oms

import (
	"context"
	"errors"
)

// ── 预置管理器接口 ─────────────────────────────────────────────────────────
// 对标银基 OMS: 数字钥匙预置（Provisioning）管理
// 负责在钥匙交付前完成安全凭证、密钥材料、配置参数的预置流程。

var (
	ErrProvisioningJobNotFound = errors.New("oms: provisioning job not found")
	ErrProvisioningFailed      = errors.New("oms: provisioning job failed")
	ErrProvisioningExpired     = errors.New("oms: provisioning job expired")
)

// ProvisioningManager 预置管理器
// 数字钥匙在售前/售中阶段需要进行预置操作：
//   - 密钥材料生成与分发
//   - OEM 端安全凭证注入
//   - 车辆端配对信息预配置
type ProvisioningManager interface {
	// CreateProvisioningJob 创建预置任务
	// 每个 KeyRecord 关联一个 ProvisioningJob，在钥匙创建后自动触发
	CreateProvisioningJob(ctx context.Context, job ProvisioningJob) error

	// GetJobStatus 查询预置任务状态
	GetJobStatus(ctx context.Context, jobID string) (*ProvisioningJob, error)

	// CancelProvisioningJob 取消预置任务
	CancelProvisioningJob(ctx context.Context, jobID string) error

	// ListProvisioningJobs 按条件列出预置任务
	ListProvisioningJobs(ctx context.Context, filter ProvisioningJobFilter) ([]ProvisioningJob, error)
}

// ProvisioningJobFilter 预置任务查询筛选条件
type ProvisioningJobFilter struct {
	Status *ProvisioningStatus `json:"status,omitempty"`
	OemID  *string            `json:"oem_id,omitempty"`
	KeyID  *string            `json:"key_id,omitempty"`
	Limit  int                `json:"limit,omitempty"`
	Offset int                `json:"offset,omitempty"`
}
