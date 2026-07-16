// Package-level documentation is in doc.go.
// This file defines ThreatEvent, Alert, EventFilter, and SecurityStats types.
package security

import "time"

// ThreatEventType 安全威胁事件类型
type ThreatEventType string

const (
	EventAuthFailure  ThreatEventType = "auth_failure"   // 认证失败（连续多次）
	EventRelayAttack  ThreatEventType = "relay_attack"   // 中继攻击（UWB 距离异常）
	EventKeyTamper    ThreatEventType = "key_tamper"     // 密钥篡改（完整性校验失败）
	EventReplayAttack ThreatEventType = "replay_attack"  // 重放攻击（Nonce 重复）
	EventDeviceAnomaly ThreatEventType = "device_anomaly" // 设备行为异常
	EventBruteForce   ThreatEventType = "brute_force"    // 暴力破解（短时间内大量请求）
)

// Severity 安全事件严重级别
type Severity string

const (
	SevLow      Severity = "low"      // 低 — 审计记录级
	SevMedium   Severity = "medium"   // 中 — 需要关注
	SevHigh     Severity = "high"     // 高 — 立即告警
	SevCritical Severity = "critical" // 危急 — 自动阻断
)

// ThreatEvent 安全威胁事件
// 对标银基 VSoC 威胁事件模型，用于安全监控和告警溯源。
type ThreatEvent struct {
	EventID     string            `json:"event_id"`
	EventType   ThreatEventType   `json:"event_type"`
	DeviceID    string            `json:"device_id"`
	Severity    Severity          `json:"severity"`
	Timestamp   time.Time         `json:"timestamp"`
	Description string            `json:"description"`
	SourceIP    string            `json:"source_ip,omitempty"`
	Protocol    string            `json:"protocol,omitempty"`    // "ccc", "iccoa", "icce"
	Metadata    map[string]string `json:"metadata,omitempty"`    // 扩展元数据
}

// Alert 安全告警（当威胁事件达到阈值后生成）
type Alert struct {
	AlertID     string            `json:"alert_id"`
	EventID     string            `json:"event_id"`     // 关联的威胁事件
	RuleID      string            `json:"rule_id"`      // 触发告警的规则 ID
	Title       string            `json:"title"`
	Severity    Severity          `json:"severity"`
	CreatedAt   time.Time         `json:"created_at"`
	Acknowledged bool             `json:"acknowledged"`
	Tags        map[string]string `json:"tags,omitempty"`
}

// EventFilter 安全事件筛选条件
type EventFilter struct {
	EventType  ThreatEventType `json:"event_type,omitempty"`
	DeviceID   string          `json:"device_id,omitempty"`
	Severity   Severity        `json:"severity,omitempty"`
	Since      time.Time       `json:"since,omitempty"`
	Until      time.Time       `json:"until,omitempty"`
	Limit      int             `json:"limit,omitempty"`
	Offset     int             `json:"offset,omitempty"`
}

// SecurityStats 安全状态汇总
type SecurityStats struct {
	TotalEvents      int64             `json:"total_events"`
	EventsBySeverity map[Severity]int64 `json:"events_by_severity"`
	EventsByType     map[ThreatEventType]int64 `json:"events_by_type"`
	ActiveAlerts     int64             `json:"active_alerts"`
	LastEventAt      time.Time         `json:"last_event_at"`
	PeriodStart      time.Time         `json:"period_start"`
	PeriodEnd        time.Time         `json:"period_end"`
}
