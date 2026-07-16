package security

import "context"

// ── 安全事件存储接口 ──────────────────────────────────────────────────────
// 支持多种后端实现：PostgreSQL / Redis / InMemory
// 生产推荐 PostgreSQL (JSONB) 或 Elasticsearch（全文检索）

// EventStore 安全事件持久化存储接口
type EventStore interface {
	// SaveEvent 保存安全威胁事件
	SaveEvent(ctx context.Context, event ThreatEvent) error

	// QueryEvents 按条件查询安全事件
	QueryEvents(ctx context.Context, filter EventFilter) ([]ThreatEvent, error)

	// DeleteEvents 删除指定时间范围前的事件（数据生命周期管理）
	DeleteEvents(ctx context.Context, beforeTimestamp int64) (int64, error)

	// GetEventCount 获取事件总数
	GetEventCount(ctx context.Context, filter EventFilter) (int64, error)
}

// ── 告警存储接口 ──────────────────────────────────────────────────────────

// AlertStore 告警存储接口
type AlertStore interface {
	// SaveAlert 保存告警
	SaveAlert(ctx context.Context, alert Alert) error

	// GetActiveAlerts 获取当前未确认的告警
	GetActiveAlerts(ctx context.Context) ([]Alert, error)

	// AcknowledgeAlert 确认告警（标记为已处理）
	AcknowledgeAlert(ctx context.Context, alertID string) error

	// ResolveAlert 解决告警（标记为已修复）
	ResolveAlert(ctx context.Context, alertID string) error
}
