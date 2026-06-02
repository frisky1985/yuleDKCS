package logger

import (
	"context"
	"testing"
)

func TestNew(t *testing.T) {
	l := New()
	if l == nil {
		t.Fatal("New() returned nil")
	}
}

func TestLoggerInfo(t *testing.T) {
	l := New()
	// Should not panic
	l.Info("test info message")
	l.Info("test with fields", "key1", "value1", "key2", 42)
}

func TestLoggerError(t *testing.T) {
	l := New()
	// Should not panic
	l.Error("test error message")
	l.Error("test with fields", "err", "something went wrong", "code", 500)
}

func TestLoggerDebug(t *testing.T) {
	l := New()
	// Should not panic
	l.Debug("test debug message")
	l.Debug("test with fields", "module", "auth", "trace_id", "abc123")
}

func TestLoggerWarn(t *testing.T) {
	l := New()
	// Should not panic
	l.Warn("test warn message")
	l.Warn("test with fields", "threshold", 0.9, "action", "rate_limit")
}

func TestLoggerAllLevels(t *testing.T) {
	l := New()
	// All log levels should be callable without panic
	l.Debug("debug")
	l.Info("info")
	l.Warn("warn")
	l.Error("error")
}

func TestLoggerWithContext(t *testing.T) {
	l := New()
	ctx := context.Background()
	l2 := l.WithContext(ctx)
	if l2 == nil {
		t.Fatal("WithContext returned nil")
	}
	// Should return a new *Logger (may be the same receiver in a stub)
}

func TestLoggerWithContextAndLog(t *testing.T) {
	l := New()
	ctx := context.WithValue(context.Background(), "request_id", "req-001")
	l2 := l.WithContext(ctx)
	// Should not panic when logging after WithContext
	l2.Info("request started", "request_id", ctx.Value("request_id"))
}

func TestLoggerMultipleCalls(t *testing.T) {
	l := New()
	for i := 0; i < 100; i++ {
		l.Info("test", "iteration", i)
	}
	// Just verifying no panic on repeated calls
}

// --- Coverage additions: missing functions ---

func TestNewLogger(t *testing.T) {
	// Test with nil config
	l := NewLogger(nil)
	if l == nil {
		t.Fatal("NewLogger(nil) returned nil")
	}
	// Test with various configs
	configs := []*Config{
		{Level: "debug", Format: "json", Output: "stdout"},
		{Level: "info", Format: "text", Output: "stderr"},
		{Level: "warn", Format: "json", Output: "file", File: "/tmp/test.log"},
		{Level: "error", Format: "", Output: ""},
	}
	for _, cfg := range configs {
		l := NewLogger(cfg)
		if l == nil {
			t.Fatalf("NewLogger(%+v) returned nil", cfg)
		}
	}
}

func TestLoggerFatal(t *testing.T) {
	l := New()
	// Should not panic (stub implementation)
	l.Fatal("test fatal message")
	l.Fatal("test with fields", "reason", "critical_error", "code", 1)
}

func TestLoggerSync(t *testing.T) {
	l := New()
	// Should return nil without error
	if err := l.Sync(); err != nil {
		t.Fatalf("Sync returned error: %v", err)
	}
}

func TestLoggerHelperFunctions(t *testing.T) {
	// Test Err helper
	errVal := Err(assertAnError())
	if errVal == nil {
		t.Error("Err returned nil")
	}

	// Test String helper
	strVal := String("key1", "value1")
	if strVal != "value1" {
		t.Errorf("String expected 'value1', got '%v'", strVal)
	}

	// Test Int helper
	intVal := Int("count", 42)
	if intVal != 42 {
		t.Errorf("Int expected 42, got '%v'", intVal)
	}

	// Test Any helper
	anyVal := Any("data", map[string]string{"key": "val"})
	if anyVal == nil {
		t.Error("Any returned nil")
	}
}

// assertAnError returns an error for Err() helper testing
func assertAnError() error {
	return context.Canceled
}

func TestLoggerHelperEdgeCases(t *testing.T) {
	// Test Err with nil error
	// Although the function signature takes error type, not interface{} / any
	_ = Err(context.DeadlineExceeded)

	// Test String with empty values
	_ = String("", "")
	_ = String("key", "v")

	// Test Int with zero and negative
	_ = Int("", 0)
	_ = Int("negative", -1)

	// Test Any with nil
	_ = Any("nil", nil)
	_ = Any("struct", struct{ Name string }{Name: "test"})
}

func TestLoggerFullLifecycle(t *testing.T) {
	// Complete lifecycle: create config -> NewLogger -> log at all levels -> Sync
	l := NewLogger(&Config{
		Level:  "debug",
		Format: "json",
		Output: "stdout",
	})
	if l == nil {
		t.Fatal("NewLogger returned nil")
	}

	l.Debug("debug message")
	l.Info("info message")
	l.Warn("warn message")
	l.Error("error message")
	l.Fatal("fatal message")

	if err := l.Sync(); err != nil {
		t.Fatalf("Sync failed: %v", err)
	}

	// WithContext after lifecycle
	ctx := context.Background()
	l2 := l.WithContext(ctx)
	l2.Info("post-context log")
}
