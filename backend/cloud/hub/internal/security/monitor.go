package security

import (
	"context"
	"fmt"
	"sync"
	"time"

	"go.uber.org/zap"
)

// ── 安全监控器接口 ─────────────────────────────────────────────────────────
// 对标银基 VSoC: 安全事件采集 → 规则引擎 → 告警 → 响应处置

// Monitor 安全监控器接口
// 提供安全威胁事件的报告、查询和统计能力。
type Monitor interface {
	// ReportEvent 报告安全威胁事件
	ReportEvent(ctx context.Context, event ThreatEvent) error
	// GetEvents 查询安全事件（支持筛选）
	GetEvents(ctx context.Context, filter EventFilter) ([]ThreatEvent, error)
	// GetStats 获取安全状态汇总
	GetStats(ctx context.Context) (*SecurityStats, error)
	// CreateAlert 基于威胁事件创建告警
	CreateAlert(ctx context.Context, alert Alert) error
	// GetAlerts 获取活跃告警列表
	GetAlerts(ctx context.Context, severity Severity) ([]Alert, error)
	// AcknowledgeAlert 确认告警
	AcknowledgeAlert(ctx context.Context, alertID string) error
}

// ── 默认实现 ───────────────────────────────────────────────────────────────

// DefaultMonitor 默认安全监控器实现
// 使用内存存储，适合单实例部署；生产环境应替换为带持久化的实现。
type DefaultMonitor struct {
	mu     sync.RWMutex
	events []ThreatEvent
	alerts []Alert
	store  EventStore
	logger *zap.Logger
}

// NewDefaultMonitor 创建默认安全监控器
func NewDefaultMonitor(store EventStore, logger *zap.Logger) *DefaultMonitor {
	if logger == nil {
		logger = zap.NewNop()
	}
	return &DefaultMonitor{
		store:  store,
		logger: logger,
	}
}

func (m *DefaultMonitor) ReportEvent(ctx context.Context, event ThreatEvent) error {
	if event.EventID == "" {
		event.EventID = fmt.Sprintf("evt-%d", time.Now().UnixNano())
	}
	if event.Timestamp.IsZero() {
		event.Timestamp = time.Now()
	}

	m.mu.Lock()
	m.events = append(m.events, event)
	m.mu.Unlock()

	// 保存到持久化存储
	if m.store != nil {
		if err := m.store.SaveEvent(ctx, event); err != nil {
			m.logger.Error("failed to persist security event",
				zap.String("event_id", event.EventID),
				zap.Error(err),
			)
			return fmt.Errorf("persist event: %w", err)
		}
	}

	m.logger.Warn("security threat event reported",
		zap.String("event_id", event.EventID),
		zap.String("type", string(event.EventType)),
		zap.String("severity", string(event.Severity)),
		zap.String("device_id", event.DeviceID),
		zap.String("description", event.Description),
	)

	return nil
}

func (m *DefaultMonitor) GetEvents(ctx context.Context, filter EventFilter) ([]ThreatEvent, error) {
	if m.store != nil {
		return m.store.QueryEvents(ctx, filter)
	}

	m.mu.RLock()
	defer m.mu.RUnlock()

	var result []ThreatEvent
	for _, e := range m.events {
		if filter.EventType != "" && e.EventType != filter.EventType {
			continue
		}
		if filter.DeviceID != "" && e.DeviceID != filter.DeviceID {
			continue
		}
		if filter.Severity != "" && e.Severity != filter.Severity {
			continue
		}
		if !filter.Since.IsZero() && e.Timestamp.Before(filter.Since) {
			continue
		}
		if !filter.Until.IsZero() && e.Timestamp.After(filter.Until) {
			continue
		}
		result = append(result, e)
	}

	// 应用分页
	if filter.Limit > 0 && len(result) > filter.Limit {
		result = result[:filter.Limit]
	}

	return result, nil
}

func (m *DefaultMonitor) GetStats(ctx context.Context) (*SecurityStats, error) {
	stats := &SecurityStats{
		EventsBySeverity: make(map[Severity]int64),
		EventsByType:     make(map[ThreatEventType]int64),
		PeriodStart:      time.Now().Add(-24 * time.Hour),
		PeriodEnd:        time.Now(),
	}

	events, err := m.GetEvents(ctx, EventFilter{
		Since: stats.PeriodStart,
		Until: stats.PeriodEnd,
	})
	if err != nil {
		return nil, err
	}

	for _, e := range events {
		stats.TotalEvents++
		stats.EventsBySeverity[e.Severity]++
		stats.EventsByType[e.EventType]++
		if e.Timestamp.After(stats.LastEventAt) {
			stats.LastEventAt = e.Timestamp
		}
	}

	// 活跃告警数
	m.mu.RLock()
	for _, a := range m.alerts {
		if !a.Acknowledged {
			stats.ActiveAlerts++
		}
	}
	m.mu.RUnlock()

	return stats, nil
}

func (m *DefaultMonitor) CreateAlert(ctx context.Context, alert Alert) error {
	if alert.AlertID == "" {
		alert.AlertID = fmt.Sprintf("alert-%d", time.Now().UnixNano())
	}
	if alert.CreatedAt.IsZero() {
		alert.CreatedAt = time.Now()
	}

	m.mu.Lock()
	m.alerts = append(m.alerts, alert)
	m.mu.Unlock()

	m.logger.Error("SECURITY ALERT TRIGGERED",
		zap.String("alert_id", alert.AlertID),
		zap.String("title", alert.Title),
		zap.String("severity", string(alert.Severity)),
	)

	return nil
}

func (m *DefaultMonitor) GetAlerts(ctx context.Context, severity Severity) ([]Alert, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()

	var result []Alert
	for _, a := range m.alerts {
		if severity != "" && a.Severity != severity {
			continue
		}
		result = append(result, a)
	}
	return result, nil
}

func (m *DefaultMonitor) AcknowledgeAlert(ctx context.Context, alertID string) error {
	m.mu.Lock()
	defer m.mu.Unlock()

	for i := range m.alerts {
		if m.alerts[i].AlertID == alertID {
			m.alerts[i].Acknowledged = true
			m.logger.Info("alert acknowledged",
				zap.String("alert_id", alertID),
			)
			return nil
		}
	}

	return fmt.Errorf("alert %s not found", alertID)
}
