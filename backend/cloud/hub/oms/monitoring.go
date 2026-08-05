package oms

import (
	"context"
	"errors"
	"time"
)

// ── 监控管理器接口 ─────────────────────────────────────────────────────────
// 对标银基 OMS: 钥匙使用监控与统计
// 提供钥匙使用的实时监控、历史查询和统计分析能力。

var (
	ErrUsageRecordNotFound = errors.New("oms: usage record not found")
)

// MonitoringManager 监控管理器
// 负责：
//   - 采集钥匙使用记录（解锁、闭锁、启动等操作）
//   - 按时间范围聚合统计
//   - 检查使用异常（如短时间内高频操作）
type MonitoringManager interface {
	// RecordUsage 记录一次钥匙使用事件
	// 每把钥匙的每次操作都应调用此方法记录
	RecordUsage(ctx context.Context, usage UsageRecord) error

	// GetUsageStats 获取指定钥匙在指定时间范围内的使用统计
	GetUsageStats(ctx context.Context, keyID string, since, until time.Time) (*UsageStats, error)

	// GetUsageHistory 获取钥匙的详细使用历史（带分页）
	GetUsageHistory(ctx context.Context, keyID string, since, until time.Time, limit, offset int) ([]UsageRecord, error)

	// GetAggregatedStats 获取全局聚合统计
	GetAggregatedStats(ctx context.Context, since, until time.Time, oemID string) (*AggregatedStats, error)
}

// AggregatedStats 全局聚合统计
type AggregatedStats struct {
	TotalKeys       int64            `json:"total_keys"`
	ActiveKeys      int64            `json:"active_keys"`
	SuspendedKeys   int64            `json:"suspended_keys"`
	RevokedKeys     int64            `json:"revoked_keys"`
	TotalActions    int64            `json:"total_actions"`
	ActionsByOEM    map[string]int64 `json:"actions_by_oem,omitempty"`
	PeriodStart     time.Time        `json:"period_start"`
	PeriodEnd       time.Time        `json:"period_end"`
}

// UsageFilter 使用记录查询筛选
type UsageFilter struct {
	KeyID    *string    `json:"key_id,omitempty"`
	DeviceID *string    `json:"device_id,omitempty"`
	Action   *string    `json:"action,omitempty"`
	Result   *string    `json:"result,omitempty"`
	Since    *time.Time `json:"since,omitempty"`
	Until    *time.Time `json:"until,omitempty"`
	Limit    int        `json:"limit,omitempty"`
	Offset   int        `json:"offset,omitempty"`
}
