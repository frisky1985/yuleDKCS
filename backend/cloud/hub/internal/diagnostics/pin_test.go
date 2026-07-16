// Package diagnostics — unit tests for PIN diagnostics module
package diagnostics

import (
	"context"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"go.uber.org/zap"
)

func TestNewDefaultCollector(t *testing.T) {
	t.Run("with logger and capacity", func(t *testing.T) {
		c := NewDefaultCollector(zap.NewNop(), 100)
		require.NotNil(t, c)
		assert.Equal(t, 100, c.capacity)
	})

	t.Run("nil logger defaults to Nop", func(t *testing.T) {
		c := NewDefaultCollector(nil, 0)
		require.NotNil(t, c)
		assert.Equal(t, 1000, c.capacity) // default capacity
	})
}

func TestDefaultCollector_Collect(t *testing.T) {
	ctx := context.Background()
	c := NewDefaultCollector(zap.NewNop(), 10)

	t.Run("collects log entry", func(t *testing.T) {
		entry := LogEntry{
			Level:   LevelInfo,
			Message: "test log",
			Service: "hub",
			TraceID: "trace-001",
		}
		err := c.Collect(ctx, entry)
		assert.NoError(t, err)
	})

	t.Run("auto-fills timestamp when empty", func(t *testing.T) {
		err := c.Collect(ctx, LogEntry{Level: LevelDebug, Message: "no timestamp"})
		assert.NoError(t, err)
		assert.False(t, c.buffer[1].Timestamp.IsZero())
	})

	t.Run("triggers flush when buffer reaches capacity", func(t *testing.T) {
		small := NewDefaultCollector(zap.NewNop(), 3)
		for i := 0; i < 5; i++ {
			_ = small.Collect(ctx, LogEntry{
				Level: LevelInfo, Message: "entry",
			})
		}
		// After each flush, buffer resets. For capacity=3, collecting 5 should have flushed at 3.
		assert.Less(t, len(small.buffer), 5)
	})
}

func TestDefaultCollector_Query(t *testing.T) {
	ctx := context.Background()
	c := NewDefaultCollector(zap.NewNop(), 100)

	_ = c.Collect(ctx, LogEntry{Level: LevelInfo, Message: "info msg", Service: "hub", TraceID: "t1"})
	_ = c.Collect(ctx, LogEntry{Level: LevelError, Message: "error msg", Service: "dkcs", TraceID: "t2"})
	_ = c.Collect(ctx, LogEntry{Level: LevelInfo, Message: "another info", Service: "hub", TraceID: "t1"})

	t.Run("filter by trace ID", func(t *testing.T) {
		results, err := c.Query(ctx, LogFilter{TraceID: "t1"})
		assert.NoError(t, err)
		assert.Len(t, results, 2)
	})

	t.Run("filter by level", func(t *testing.T) {
		results, err := c.Query(ctx, LogFilter{Level: LevelError})
		assert.NoError(t, err)
		assert.Len(t, results, 1)
	})

	t.Run("filter by service", func(t *testing.T) {
		results, err := c.Query(ctx, LogFilter{Service: "dkcs"})
		assert.NoError(t, err)
		assert.Len(t, results, 1)
	})

	t.Run("limit results", func(t *testing.T) {
		results, err := c.Query(ctx, LogFilter{Limit: 1})
		assert.NoError(t, err)
		assert.Len(t, results, 1)
	})

	t.Run("empty filter returns all", func(t *testing.T) {
		results, err := c.Query(ctx, LogFilter{})
		assert.NoError(t, err)
		assert.Len(t, results, 3)
	})
}

func TestDefaultCollector_Flush(t *testing.T) {
	ctx := context.Background()
	c := NewDefaultCollector(zap.NewNop(), 100)

	_ = c.Collect(ctx, LogEntry{Level: LevelInfo, Message: "flush me"})
	err := c.Flush(ctx)
	assert.NoError(t, err)
	assert.Empty(t, c.buffer)
}

func TestDefaultCollector_FlushEmpty(t *testing.T) {
	ctx := context.Background()
	c := NewDefaultCollector(zap.NewNop(), 100)
	err := c.Flush(ctx)
	assert.NoError(t, err)
}

func TestNewHealthChecker(t *testing.T) {
	h := NewHealthChecker("hub", "1.0.0", zap.NewNop())
	require.NotNil(t, h)
	assert.Equal(t, "hub", h.status.Service)
}

func TestHealthChecker_RegisterAndRun(t *testing.T) {
	ctx := context.Background()
	h := NewHealthChecker("hub", "1.0.0", zap.NewNop())

	h.RegisterCheck("ble_module", func(ctx context.Context) HealthCheck {
		return HealthCheck{Name: "ble_module", Status: "pass", Message: "BLE OK", Latency: 5}
	})
	h.RegisterCheck("uwb_module", func(ctx context.Context) HealthCheck {
		return HealthCheck{Name: "uwb_module", Status: "pass", Message: "UWB OK", Latency: 3}
	})

	status := h.RunChecks(ctx)
	assert.Equal(t, "healthy", status.Status)
	assert.Len(t, status.Checks, 2)
}

func TestHealthChecker_FailureDegradation(t *testing.T) {
	ctx := context.Background()
	h := NewHealthChecker("hub", "1.0.0", zap.NewNop())

	h.RegisterCheck("good_check", func(ctx context.Context) HealthCheck {
		return HealthCheck{Name: "good", Status: "pass", Message: "OK"}
	})
	h.RegisterCheck("bad_check", func(ctx context.Context) HealthCheck {
		return HealthCheck{Name: "bad", Status: "fail", Message: "FAIL"}
	})

	status := h.RunChecks(ctx)
	assert.Equal(t, "degraded", status.Status)
}

func TestHealthChecker_AllFail(t *testing.T) {
	ctx := context.Background()
	h := NewHealthChecker("hub", "1.0.0", zap.NewNop())

	h.RegisterCheck("f1", func(ctx context.Context) HealthCheck {
		return HealthCheck{Name: "f1", Status: "fail", Message: "fail 1"}
	})
	h.RegisterCheck("f2", func(ctx context.Context) HealthCheck {
		return HealthCheck{Name: "f2", Status: "fail", Message: "fail 2"}
	})

	status := h.RunChecks(ctx)
	assert.Equal(t, "unhealthy", status.Status)
}

func TestHealthChecker_GetStatusCache(t *testing.T) {
	h := NewHealthChecker("hub", "1.0.0", zap.NewNop())

	h.RegisterCheck("check1", func(ctx context.Context) HealthCheck {
		return HealthCheck{Name: "check1", Status: "pass"}
	})

	// Run first
	_ = h.RunChecks(context.Background())
	// Get cached
	cached := h.GetStatus()
	assert.NotNil(t, cached)
	assert.Len(t, cached.Checks, 1)
}

func TestHealthChecker_StartStop(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	h := NewHealthChecker("hub", "1.0.0", zap.NewNop())
	h.interval = 100 * time.Millisecond
	h.RegisterCheck("check1", func(ctx context.Context) HealthCheck {
		return HealthCheck{Name: "check1", Status: "pass"}
	})

	h.Start(ctx)
	assert.True(t, h.started)

	// Starting again should be no-op
	h.Start(ctx)

	h.Stop()
	assert.False(t, h.started)
}

func TestDefaultTracer(t *testing.T) {
	t.Run("new tracer", func(t *testing.T) {
		tr := NewDefaultTracer(zap.NewNop())
		require.NotNil(t, tr)
	})

	t.Run("nil logger defaults", func(t *testing.T) {
		tr := NewDefaultTracer(nil)
		require.NotNil(t, tr)
	})
}

func TestDefaultTracer_StartEndSpan(t *testing.T) {
	ctx := context.Background()
	tr := NewDefaultTracer(zap.NewNop())

	ctx, span := tr.StartSpan(ctx, "hub", "unlock", map[string]string{"device": "d1"})
	require.NotNil(t, span)
	assert.Equal(t, "hub", span.Service)
	assert.Equal(t, "unlock", span.Operation)
	assert.False(t, span.StartTime.IsZero())

	tr.EndSpan(ctx, StatusSuccess, "")
	assert.False(t, span.EndTime.IsZero())
	assert.Equal(t, StatusSuccess, span.Status)
}

func TestDefaultTracer_GetTrace(t *testing.T) {
	ctx := context.Background()
	tr := NewDefaultTracer(zap.NewNop())

	ctx, _ = tr.StartSpan(ctx, "hub", "unlock", nil)
	tr.EndSpan(ctx, StatusSuccess, "")

	tc := GetTraceContext(ctx)
	require.NotNil(t, tc)

	spans, err := tr.GetTrace(ctx, tc.TraceID)
	assert.NoError(t, err)
	assert.Len(t, spans, 1)
}

func TestDefaultTracer_GetTraceNotFound(t *testing.T) {
	ctx := context.Background()
	tr := NewDefaultTracer(zap.NewNop())
	_, err := tr.GetTrace(ctx, "non-existent")
	assert.Error(t, err)
}

func TestDefaultTracer_QuerySpans(t *testing.T) {
	ctx := context.Background()
	tr := NewDefaultTracer(zap.NewNop())

	ctx1, _ := tr.StartSpan(ctx, "hub", "unlock", nil)
	tr.EndSpan(ctx1, StatusSuccess, "")

	ctx2, _ := tr.StartSpan(ctx, "dkcs", "key_issue", nil)
	tr.EndSpan(ctx2, StatusError, "timeout")

	t.Run("filter by service", func(t *testing.T) {
		results, err := tr.QuerySpans(ctx, TraceFilter{Service: "hub"})
		assert.NoError(t, err)
		assert.Len(t, results, 1)
	})

	t.Run("filter by operation", func(t *testing.T) {
		results, err := tr.QuerySpans(ctx, TraceFilter{Operation: "key_issue"})
		assert.NoError(t, err)
		assert.Len(t, results, 1)
	})

	t.Run("filter by status", func(t *testing.T) {
		results, err := tr.QuerySpans(ctx, TraceFilter{Status: StatusError})
		assert.NoError(t, err)
		assert.Len(t, results, 1)
	})

	t.Run("limit", func(t *testing.T) {
		results, err := tr.QuerySpans(ctx, TraceFilter{Limit: 1})
		assert.NoError(t, err)
		assert.Len(t, results, 1)
	})
}

func TestTraceContext(t *testing.T) {
	ctx := context.Background()
	tc := &TraceContext{TraceID: "trace-001", RootSpanID: "root-001", Service: "hub"}

	ctx = WithTraceContext(ctx, tc)
	retrieved := GetTraceContext(ctx)
	require.NotNil(t, retrieved)
	assert.Equal(t, "trace-001", retrieved.TraceID)
}

func TestLogLevels(t *testing.T) {
	assert.Equal(t, LogLevel("debug"), LevelDebug)
	assert.Equal(t, LogLevel("info"), LevelInfo)
	assert.Equal(t, LogLevel("warn"), LevelWarn)
	assert.Equal(t, LogLevel("error"), LevelError)
	assert.Equal(t, LogLevel("fatal"), LevelFatal)
}

func TestTraceStatus(t *testing.T) {
	assert.Equal(t, TraceStatus("success"), StatusSuccess)
	assert.Equal(t, TraceStatus("error"), StatusError)
	assert.Equal(t, TraceStatus("timeout"), StatusTimeout)
}

func TestDefaultCollector_LogWithTrace(t *testing.T) {
	ctx := context.Background()
	tc := &TraceContext{TraceID: "trace-001", CurrentSpanID: "span-001", Service: "hub"}
	ctx = WithTraceContext(ctx, tc)

	c := NewDefaultCollector(zap.NewNop(), 100)
	c.LogWithTrace(ctx, LevelInfo, "message with trace", map[string]any{"key": "value"})

	assert.Len(t, c.buffer, 1)
	assert.Equal(t, "trace-001", c.buffer[0].TraceID)
	assert.Equal(t, "span-001", c.buffer[0].SpanID)
}

func TestDefaultCollector_LogWithTraceNoContext(t *testing.T) {
	ctx := context.Background()
	c := NewDefaultCollector(zap.NewNop(), 100)
	c.LogWithTrace(ctx, LevelInfo, "no trace context", nil)

	assert.Len(t, c.buffer, 1)
	assert.Empty(t, c.buffer[0].TraceID)
}
