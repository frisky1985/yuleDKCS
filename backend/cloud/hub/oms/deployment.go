package oms

import (
	"context"
	"errors"
)

// ── 部署管理器接口 ─────────────────────────────────────────────────────────
// 对标银基 OMS: OEM 端系统部署管理
// 负责跟踪和管理 OEM 侧的钥匙系统版本升级、配置变更和灰度发布。

var (
	ErrDeploymentNotFound   = errors.New("oms: deployment not found")
	ErrDeploymentInProgress = errors.New("oms: deployment already in progress")
	ErrDeploymentCompleted  = errors.New("oms: deployment already completed")
)

// DeploymentManager 部署管理器
// 数字钥匙的 OTA 升级或配置变更需要在 OEM 端进行部署管理：
//   - 灰度发布（canary release）支持
//   - 部署版本追踪
//   - 一键回滚能力
type DeploymentManager interface {
	// CreateDeployment 创建部署计划
	CreateDeployment(ctx context.Context, deploy DeploymentRecord) error

	// RollbackDeployment 回滚部署到上一个版本
	// 只有在 in_progress 或 completed 状态的部署可以回滚
	RollbackDeployment(ctx context.Context, deployID string) error

	// GetDeploymentStatus 查询部署状态
	GetDeploymentStatus(ctx context.Context, deployID string) (*DeploymentRecord, error)

	// ListDeployments 列出 OEM 的部署历史
	ListDeployments(ctx context.Context, oemID string, limit, offset int) ([]DeploymentRecord, error)

	// UpdateDeploymentProgress 更新部署进度
	// 用于灰度发布中按阶段推进 rollout_pct
	UpdateDeploymentProgress(ctx context.Context, deployID string, rolloutPct int) error
}

// ── 部署状态常量 ──────────────────────────────────────────────────────────

const (
	DeployStatusPlanning    = "planning"
	DeployStatusInProgress  = "in_progress"
	DeployStatusCompleted   = "completed"
	DeployStatusRolledBack  = "rolled_back"
	DeployStatusFailed      = "failed"
	DeployStatusCancelled   = "cancelled"
)
