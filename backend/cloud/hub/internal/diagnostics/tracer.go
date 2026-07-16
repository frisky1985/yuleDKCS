package diagnostics

import (
	"context"
	"fmt"
	"sync"
	"time"

	"go.uber.org/zap"
)

// ── 追踪器 ─────────────────────────────────────────────────────────────────
// 实现银基 Dting 全链路追踪概念：Trace → Span Tree
// 每个 TraceSpan 对应一次跨服务调用中的一个处理步骤。

// Tracer 追踪器接口
type Tracer interface {
	// StartSpan 启动一个新的追踪跨度
	StartSpan(ctx context.Context, service, operation string, tags map[string]string) (context.Context, *TraceSpan)

	// EndSpan 结束当前跨度（记录结束时间和状态）
	EndSpan(ctx context.Context, status TraceStatus, errMsg string)

	// GetTrace 获取指定 TraceID 的完整 Span 链
	GetTrace(ctx context.Context, traceID string) ([]TraceSpan, error)

	// QuerySpans 按条件查询追踪跨度
	QuerySpans(ctx context.Context, filter TraceFilter) ([]TraceSpan, error)
}

// ── Context Key ────────────────────────────────────────────────────────────

type contextKey string

const (
	ctxKeyTraceContext = contextKey("trace_context")
	ctxKeyActiveSpan  = contextKey("active_span")
)

// WithTraceContext 将追踪上下文注入 context
func WithTraceContext(ctx context.Context, tc *TraceContext) context.Context {
	return context.WithValue(ctx, ctxKeyTraceContext, tc)
}

// GetTraceContext 从 context 中提取追踪上下文
func GetTraceContext(ctx context.Context) *TraceContext {
	tc, _ := ctx.Value(ctxKeyTraceContext).(*TraceContext)
	return tc
}

// ── 默认实现 ───────────────────────────────────────────────────────────────

// DefaultTracer 默认全链路追踪器（内存实现）
type DefaultTracer struct {
	mu     sync.RWMutex
	spans  map[string][]TraceSpan // traceID → spans
	logger *zap.Logger
}

// NewDefaultTracer 创建默认追踪器
func NewDefaultTracer(logger *zap.Logger) *DefaultTracer {
	if logger == nil {
		logger = zap.NewNop()
	}
	return &DefaultTracer{
		spans:  make(map[string][]TraceSpan),
		logger: logger,
	}
}

func (t *DefaultTracer) StartSpan(ctx context.Context, service, operation string, tags map[string]string) (context.Context, *TraceSpan) {
	spanID := fmt.Sprintf("span-%d-%d", time.Now().UnixNano(), len(t.spans))
	var traceID string
	var parentID string

	// 从 context 中获取父追踪上下文
	parentCtx := GetTraceContext(ctx)
	if parentCtx != nil {
		traceID = parentCtx.TraceID
		parentID = parentCtx.CurrentSpanID
	} else {
		// 根 span — 生成新的 TraceID
		traceID = fmt.Sprintf("trace-%d", time.Now().UnixNano())
	}

	span := &TraceSpan{
		SpanID:    spanID,
		ParentID:  parentID,
		Service:   service,
		Operation: operation,
		StartTime: time.Now(),
		Status:    StatusSuccess,
		Tags:      tags,
	}

	// 创建新的追踪上下文
	tc := &TraceContext{
		TraceID:       traceID,
		RootSpanID:    spanID,
		CurrentSpanID: spanID,
		Service:       service,
	}

	// 保留根 span ID
	if parentCtx != nil {
		tc.RootSpanID = parentCtx.RootSpanID
	}

	newCtx := context.WithValue(ctx, ctxKeyTraceContext, tc)
	newCtx = context.WithValue(newCtx, ctxKeyActiveSpan, span)

	t.logger.Debug("span started",
		zap.String("trace_id", traceID),
		zap.String("span_id", spanID),
		zap.String("service", service),
		zap.String("operation", operation),
	)

	return newCtx, span
}

func (t *DefaultTracer) EndSpan(ctx context.Context, status TraceStatus, errMsg string) {
	activeSpan, ok := ctx.Value(ctxKeyActiveSpan).(*TraceSpan)
	if !ok || activeSpan == nil {
		t.logger.Warn("EndSpan called without active span in context")
		return
	}

	activeSpan.EndTime = time.Now()
	activeSpan.Duration = activeSpan.EndTime.Sub(activeSpan.StartTime)
	activeSpan.Status = status
	activeSpan.ErrorMsg = errMsg

	tc := GetTraceContext(ctx)
	if tc == nil {
		t.logger.Warn("EndSpan called without trace context")
		return
	}

	t.mu.Lock()
	t.spans[tc.TraceID] = append(t.spans[tc.TraceID], *activeSpan)
	t.mu.Unlock()

	t.logger.Debug("span ended",
		zap.String("trace_id", tc.TraceID),
		zap.String("span_id", activeSpan.SpanID),
		zap.Duration("duration", activeSpan.Duration),
		zap.String("status", string(status)),
	)
}

func (t *DefaultTracer) GetTrace(ctx context.Context, traceID string) ([]TraceSpan, error) {
	t.mu.RLock()
	defer t.mu.RUnlock()

	spans, ok := t.spans[traceID]
	if !ok {
		return nil, fmt.Errorf("trace %s not found", traceID)
	}

	result := make([]TraceSpan, len(spans))
	copy(result, spans)
	return result, nil
}

func (t *DefaultTracer) QuerySpans(ctx context.Context, filter TraceFilter) ([]TraceSpan, error) {
	t.mu.RLock()
	defer t.mu.RUnlock()

	var result []TraceSpan
	for _, spans := range t.spans {
		for _, s := range spans {
			if filter.TraceID != "" && filter.TraceID != getTraceIDFromKey(ctx, t.spans, &s) {
				continue
			}
			if filter.Service != "" && s.Service != filter.Service {
				continue
			}
			if filter.Operation != "" && s.Operation != filter.Operation {
				continue
			}
			if filter.Status != "" && s.Status != filter.Status {
				continue
			}
			if !filter.Since.IsZero() && s.StartTime.Before(filter.Since) {
				continue
			}
			if !filter.Until.IsZero() && s.StartTime.After(filter.Until) {
				continue
			}
			result = append(result, s)
		}
	}

	if filter.Limit > 0 && len(result) > filter.Limit {
		result = result[:filter.Limit]
	}

	return result, nil
}

// getTraceIDFromKey 辅助函数：从 spans map 的 key 获取 traceID
func getTraceIDFromKey(ctx context.Context, spans map[string][]TraceSpan, s *TraceSpan) string {
	for traceID, sp := range spans {
		for _, sp2 := range sp {
			if sp2.SpanID == s.SpanID {
				return traceID
			}
		}
	}
	return ""
}
