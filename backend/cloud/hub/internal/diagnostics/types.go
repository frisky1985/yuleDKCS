// Package-level documentation is in doc.go.
// This file defines TraceSpan, TraceContext, LogEntry, HealthStatus types.
package diagnostics

import "time"

// TraceStatus 追踪跨度状态
type TraceStatus string

const (
	StatusSuccess TraceStatus = "success" // 正常完成
	StatusError   TraceStatus = "error"   // 异常结束
	StatusTimeout TraceStatus = "timeout" // 超时
)

// TraceSpan 追踪跨度
// 记录一次分布式调用中单个服务的处理时间段。
// ParentID 为空表示根 span（调用起始端）。
type TraceSpan struct {
	SpanID    string            `json:"span_id"`
	ParentID  string            `json:"parent_id"`
	Service   string            `json:"service"`   // "hub", "dkcs", "app", "vehicle"
	Operation string            `json:"operation"` // "unlock", "start", "share", "bind"
	StartTime time.Time         `json:"start_time"`
	EndTime   time.Time         `json:"end_time"`
	Duration  time.Duration     `json:"duration_ms"`
	Status    TraceStatus       `json:"status"`
	Tags      map[string]string `json:"tags,omitempty"`
	ErrorMsg  string            `json:"error_msg,omitempty"`
}

// TraceContext 追踪上下文（用于跨服务传递）
type TraceContext struct {
	TraceID      string `json:"trace_id"`
	RootSpanID   string `json:"root_span_id"`
	CurrentSpanID string `json:"current_span_id"`
	Service      string `json:"service"`
}

// LogLevel 日志级别
type LogLevel string

const (
	LevelDebug LogLevel = "debug"
	LevelInfo  LogLevel = "info"
	LevelWarn  LogLevel = "warn"
	LevelError LogLevel = "error"
	LevelFatal LogLevel = "fatal"
)

// LogEntry 结构化日志条目
type LogEntry struct {
	Timestamp   time.Time         `json:"timestamp"`
	Level       LogLevel          `json:"level"`
	Service     string            `json:"service"`
	Message     string            `json:"message"`
	TraceID     string            `json:"trace_id,omitempty"`
	SpanID      string            `json:"span_id,omitempty"`
	Fields      map[string]any    `json:"fields,omitempty"`
}

// HealthStatus 服务健康状态
type HealthStatus struct {
	Service    string            `json:"service"`
	Status     string            `json:"status"` // "healthy", "degraded", "unhealthy"
	Uptime     time.Duration     `json:"uptime"`
	Version    string            `json:"version"`
	Checks     []HealthCheck     `json:"checks"`
	LastCheck  time.Time         `json:"last_check"`
}

// HealthCheck 健康检查项
type HealthCheck struct {
	Name    string `json:"name"`
	Status  string `json:"status"` // "pass", "fail"
	Message string `json:"message,omitempty"`
	Latency int64  `json:"latency_ms"`
}

// TraceFilter 追踪查询筛选条件
type TraceFilter struct {
	TraceID   string    `json:"trace_id,omitempty"`
	Service   string    `json:"service,omitempty"`
	Operation string    `json:"operation,omitempty"`
	Status    TraceStatus `json:"status,omitempty"`
	Since     time.Time   `json:"since,omitempty"`
	Until     time.Time   `json:"until,omitempty"`
	Limit     int         `json:"limit,omitempty"`
}
