package diagnostics

import (
	"context"
	"fmt"
	"sync"
	"time"

	"go.uber.org/zap"
)

// ── 日志采集器 ─────────────────────────────────────────────────────────────
// 对标银基 Dting 数据采集层：收集结构化日志并关联到 Trace

// Collector 日志采集器接口
type Collector interface {
	// Collect 采集一条日志
	Collect(ctx context.Context, entry LogEntry) error

	// Query 查询日志（支持按 TraceID / Service / Level 过滤）
	Query(ctx context.Context, filter LogFilter) ([]LogEntry, error)

	// Flush 强制刷新缓冲区
	Flush(ctx context.Context) error
}

// LogFilter 日志查询筛选条件
type LogFilter struct {
	TraceID string   `json:"trace_id,omitempty"`
	SpanID  string   `json:"span_id,omitempty"`
	Service string   `json:"service,omitempty"`
	Level   LogLevel `json:"level,omitempty"`
	Since   time.Time `json:"since,omitempty"`
	Until   time.Time `json:"until,omitempty"`
	Limit   int      `json:"limit,omitempty"`
}

// ── 默认实现 ───────────────────────────────────────────────────────────────

// DefaultCollector 默认日志采集器（内存缓冲区 + 异步写入）
type DefaultCollector struct {
	mu       sync.RWMutex
	buffer   []LogEntry
	logger   *zap.Logger
	capacity int
}

// NewDefaultCollector 创建默认日志采集器
func NewDefaultCollector(logger *zap.Logger, capacity int) *DefaultCollector {
	if logger == nil {
		logger = zap.NewNop()
	}
	if capacity <= 0 {
		capacity = 1000
	}
	return &DefaultCollector{
		buffer:   make([]LogEntry, 0, capacity),
		logger:   logger,
		capacity: capacity,
	}
}

func (c *DefaultCollector) Collect(ctx context.Context, entry LogEntry) error {
	if entry.Timestamp.IsZero() {
		entry.Timestamp = time.Now()
	}

	c.mu.Lock()
	c.buffer = append(c.buffer, entry)

	// 缓冲区满时自动触发 Flush
	if len(c.buffer) >= c.capacity {
		c.mu.Unlock()
		return c.Flush(ctx)
	}
	c.mu.Unlock()

	return nil
}

func (c *DefaultCollector) Query(ctx context.Context, filter LogFilter) ([]LogEntry, error) {
	c.mu.RLock()
	defer c.mu.RUnlock()

	var result []LogEntry
	for _, entry := range c.buffer {
		if filter.TraceID != "" && entry.TraceID != filter.TraceID {
			continue
		}
		if filter.SpanID != "" && entry.SpanID != filter.SpanID {
			continue
		}
		if filter.Service != "" && entry.Service != filter.Service {
			continue
		}
		if filter.Level != "" && entry.Level != filter.Level {
			continue
		}
		if !filter.Since.IsZero() && entry.Timestamp.Before(filter.Since) {
			continue
		}
		if !filter.Until.IsZero() && entry.Timestamp.After(filter.Until) {
			continue
		}
		result = append(result, entry)
	}

	if filter.Limit > 0 && len(result) > filter.Limit {
		result = result[:filter.Limit]
	}

	return result, nil
}

func (c *DefaultCollector) Flush(ctx context.Context) error {
	c.mu.Lock()
	if len(c.buffer) == 0 {
		c.mu.Unlock()
		return nil
	}

	// 将缓冲区写入后端存储（此处模拟：输出到日志）
	entries := make([]LogEntry, len(c.buffer))
	copy(entries, c.buffer)
	c.buffer = c.buffer[:0]
	c.mu.Unlock()

	c.logger.Info("log collector flush",
		zap.Int("entries_flushed", len(entries)),
	)

	// TODO: 生产环境应将 entries 写入 Elasticsearch / Loki / Kafka
	_ = entries

	return nil
}

// ── 辅助函数 ───────────────────────────────────────────────────────────────

// DefaultCollectorFromTracer 创建一个绑定了追踪上下文日志采集辅助函数
func (c *DefaultCollector) LogWithTrace(ctx context.Context, level LogLevel, msg string, fields map[string]any) {
	entry := LogEntry{
		Timestamp: time.Now(),
		Level:     level,
		Message:   msg,
		Fields:    fields,
	}

	tc := GetTraceContext(ctx)
	if tc != nil {
		entry.TraceID = tc.TraceID
		entry.SpanID = tc.CurrentSpanID
		entry.Service = tc.Service
	}

	// 不要阻塞调用路径
	if err := c.Collect(ctx, entry); err != nil {
		fmt.Printf("log collect failed: %v\n", err)
	}
}
