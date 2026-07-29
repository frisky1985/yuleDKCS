package logger

import (
	"bytes"
	"context"
	"encoding/json"
	"strings"
	"testing"
	"time"
)

func TestNewLogger(t *testing.T) {
	var buf bytes.Buffer
	cfg := LoggerConfig{
		ServiceName: "test-svc",
		InstanceID:  "inst-1",
		Level:       LevelTrace,
		Output:      &buf,
		EnableJSON:  false,
	}
	l := NewLogger(cfg)
	if l == nil {
		t.Fatal("NewLogger returned nil")
	}
	if l.serviceName != "test-svc" {
		t.Errorf("expected test-svc, got %s", l.serviceName)
	}
}

func TestNewLogger_DefaultOutput(t *testing.T) {
	l := NewLogger(LoggerConfig{Level: LevelInfo})
	if l.output == nil {
		t.Error("expected default output to be set")
	}
}

func TestNewLogger_JSONOutput(t *testing.T) {
	var buf bytes.Buffer
	l := NewLogger(LoggerConfig{
		Level:      LevelInfo,
		Output:     &buf,
		EnableJSON: true,
		ServiceName: "json-svc",
	})
	l.Info("hello world", WithKeyID("key-001"))

	var entry LogEntry
	if err := json.Unmarshal(buf.Bytes(), &entry); err != nil {
		t.Fatalf("failed to parse JSON: %v", err)
	}
	if entry.Message != "hello world" {
		t.Errorf("expected 'hello world', got %s", entry.Message)
	}
	if entry.Level != "INFO" {
		t.Errorf("expected INFO, got %s", entry.Level)
	}
	if entry.Service != "json-svc" {
		t.Errorf("expected json-svc, got %s", entry.Service)
	}
	if entry.Logger != "INIT" {
		t.Errorf("expected INIT tag, got %s", entry.Logger)
	}
	if entry.Fields["key_id"] != "key-001" {
		t.Errorf("expected key-001 in fields, got %v", entry.Fields["key_id"])
	}
}

func TestLevelString(t *testing.T) {
	tests := []struct {
		level Level
		want  string
	}{
		{LevelTrace, "TRACE"},
		{LevelDebug, "DEBUG"},
		{LevelInfo, "INFO"},
		{LevelWarn, "WARN"},
		{LevelError, "ERROR"},
		{LevelFatal, "FATAL"},
		{Level(99), "UNKNOWN"},
	}
	for _, tt := range tests {
		if got := tt.level.String(); got != tt.want {
			t.Errorf("Level(%d).String() = %s, want %s", tt.level, got, tt.want)
		}
	}
}

func TestLogger_SetLevel(t *testing.T) {
	var buf bytes.Buffer
	l := NewLogger(LoggerConfig{
		Level:  LevelError,
		Output: &buf,
	})
	l.Info("should not appear")
	if buf.Len() != 0 {
		t.Error("expected no output when level is ERROR but logging INFO")
	}

	l.SetLevel(LevelTrace)
	l.Info("should appear now")
	if buf.Len() == 0 {
		t.Error("expected output after SetLevel to Trace")
	}
}

func TestLogger_LevelFiltering(t *testing.T) {
	var buf bytes.Buffer
	l := NewLogger(LoggerConfig{Level: LevelWarn, Output: &buf})

	l.Trace("trace")
	l.Debug("debug")
	l.Info("info")
	l.Warn("warn")
	l.Error("error")

	output := buf.String()
	if strings.Contains(output, "TRACE") {
		t.Error("TRACE should be filtered out")
	}
	if strings.Contains(output, "DEBUG") {
		t.Error("DEBUG should be filtered out")
	}
	if strings.Contains(output, "INFO") {
		t.Error("INFO should be filtered out")
	}
	if !strings.Contains(output, "WARN") {
		t.Error("WARN should appear")
	}
	if !strings.Contains(output, "ERROR") {
		t.Error("ERROR should appear")
	}
}

func TestLogger_LogLevels(t *testing.T) {
	var buf bytes.Buffer
	l := NewLogger(LoggerConfig{Level: LevelTrace, Output: &buf})

	l.Trace("trace msg")
	l.Debug("debug msg")
	l.Info("info msg")
	l.Warn("warn msg")
	l.Error("error msg")

	output := buf.String()
	for _, level := range []string{"TRACE", "DEBUG", "INFO", "WARN", "ERROR"} {
		if !strings.Contains(output, level) {
			t.Errorf("expected %s in output", level)
		}
	}
}

func TestLogger_WithFields(t *testing.T) {
	var buf bytes.Buffer
	l := NewLogger(LoggerConfig{Level: LevelInfo, Output: &buf})

	l.Info("test", WithUserID("u-1"), WithVehicleID("v-1"), WithDeviceID("d-1"), WithKeyID("k-1"))
	output := buf.String()

	for _, field := range []string{"user_id=u-1", "vehicle_id=v-1", "device_id=d-1", "key_id=k-1"} {
		if !strings.Contains(output, field) {
			t.Errorf("expected field %s in output", field)
		}
	}
}

func TestLogger_WithError(t *testing.T) {
	var buf bytes.Buffer
	l := NewLogger(LoggerConfig{Level: LevelInfo, Output: &buf})

	err := someCustomError("something broke")
	l.Error("error occurred", WithError(err))

	if !strings.Contains(buf.String(), "something broke") {
		t.Error("expected error message in output")
	}
}

func TestLogger_WithErrorCode(t *testing.T) {
	var buf bytes.Buffer
	l := NewLogger(LoggerConfig{Level: LevelInfo, Output: &buf})

	l.Warn("code warning", WithErrorCode(0x0301))
	output := buf.String()

	if !strings.Contains(output, "0x0301") {
		t.Errorf("expected error code 0x0301 in output, got: %s", output)
	}
}

func TestLogger_WithDuration(t *testing.T) {
	var buf bytes.Buffer
	l := NewLogger(LoggerConfig{Level: LevelInfo, Output: &buf})

	l.Info("slow op", WithDuration(1500))
	if !strings.Contains(buf.String(), "1500") {
		t.Error("expected duration in output")
	}
}

func TestLogger_WithField(t *testing.T) {
	var buf bytes.Buffer
	l := NewLogger(LoggerConfig{Level: LevelInfo, Output: &buf})

	l.Info("custom", WithField("custom_key", "custom_value"))
	if !strings.Contains(buf.String(), "custom_value") {
		t.Error("expected custom field value in output")
	}
}

func TestLogger_WithTraceID(t *testing.T) {
	var buf bytes.Buffer
	l := NewLogger(LoggerConfig{Level: LevelInfo, Output: &buf, EnableJSON: true})

	l.Info("traced", WithTraceID("trace-abc-123"))

	var entry LogEntry
	if err := json.Unmarshal(buf.Bytes(), &entry); err != nil {
		t.Fatalf("JSON parse error: %v", err)
	}
	if entry.Fields["trace_id"] != "trace-abc-123" {
		t.Errorf("expected trace_id in fields, got %v", entry.Fields)
	}
}

func TestLogger_JSONWithAllFields(t *testing.T) {
	var buf bytes.Buffer
	l := NewLogger(LoggerConfig{
		Level:       LevelInfo,
		Output:      &buf,
		EnableJSON:  true,
		ServiceName: "svc",
		InstanceID:  "inst-1",
	})
	l.Info("json test", WithField("hello", "world"))

	var entry LogEntry
	if err := json.Unmarshal(buf.Bytes(), &entry); err != nil {
		t.Fatalf("JSON parse error: %v", err)
	}
	if entry.Service != "svc" {
		t.Errorf("expected svc, got %s", entry.Service)
	}
	if entry.Instance != "inst-1" {
		t.Errorf("expected inst-1, got %s", entry.Instance)
	}
	if entry.Message != "json test" {
		t.Errorf("expected 'json test', got %s", entry.Message)
	}
	if entry.Level != "INFO" {
		t.Errorf("expected INFO, got %s", entry.Level)
	}
}

func TestLogger_TextOutput(t *testing.T) {
	var buf bytes.Buffer
	l := NewLogger(LoggerConfig{Level: LevelInfo, Output: &buf, EnableJSON: false})
	l.Info("text output", WithField("key", "val"))

	output := buf.String()
	if !strings.Contains(output, "INFO") {
		t.Error("expected INFO level in text output")
	}
	if !strings.Contains(output, "text output") {
		t.Error("expected message in text output")
	}
}

func TestLogger_ModuleLogger(t *testing.T) {
	var buf bytes.Buffer
	l := NewLogger(LoggerConfig{Level: LevelInfo, Output: &buf})

	ml := l.Module("TEST")
	ml.Info("module test")
	if !strings.Contains(buf.String(), "TEST") {
		t.Errorf("expected module tag TEST in output, got: %s", buf.String())
	}
}

func TestLogger_ModuleLevels(t *testing.T) {
	var buf bytes.Buffer
	l := NewLogger(LoggerConfig{Level: LevelTrace, Output: &buf})
	ml := l.Module("MOD")

	ml.Trace("t")
	ml.Debug("d")
	ml.Info("i")
	ml.Warn("w")
	ml.Error("e")

	output := buf.String()
	for _, level := range []string{"TRACE", "DEBUG", "INFO", "WARN", "ERROR"} {
		if !strings.Contains(output, level) {
			t.Errorf("expected %s in module output", level)
		}
	}
}

func TestLogger_PredefinedModules(t *testing.T) {
	var buf bytes.Buffer
	l := NewLogger(LoggerConfig{Level: LevelInfo, Output: &buf})

	l.Init().Info("init test")
	l.KeyMgr().Info("key mgr")
	l.Auth().Info("auth")
	l.BLE().Info("ble")
	l.NFC().Info("nfc")
	l.UWB().Info("uwb")
	l.Sec().Info("sec")
	l.Transport().Info("transport")
	l.Protocol().Info("protocol")
	l.Service().Info("service")
	l.Adapter().Info("adapter")
	l.DB().Info("db")
	l.Cache().Info("cache")
	l.MQTT().Info("mqtt")
	l.GRPC().Info("grpc")

	output := buf.String()
	for _, tag := range []string{"INIT", "KEYMGR", "AUTH", "BLE", "NFC", "UWB", "SEC",
		"TRANSPORT", "PROTO", "SERVICE", "ADAPTER", "DB", "CACHE", "MQTT", "GRPC"} {
		if !strings.Contains(output, tag) {
			t.Errorf("expected module tag %s in output", tag)
		}
	}
}

func TestLogger_WithContext(t *testing.T) {
	var buf bytes.Buffer
	l := NewLogger(LoggerConfig{Level: LevelInfo, Output: &buf})

	ctx := context.Background()
	cl := l.WithContext(ctx)
	if _, ok := cl.(*DefaultLogger); !ok {
		t.Errorf("expected *DefaultLogger from WithContext, got %T", cl)
	}

	cl.Info("with context")
	if !strings.Contains(buf.String(), "with context") {
		t.Error("expected message in output")
	}
}

func TestLogger_Fatal(t *testing.T) {
	var buf bytes.Buffer
	l := NewLogger(LoggerConfig{Level: LevelTrace, Output: &buf})

	// Fatal should log
	l.Fatal("fatal error")
	if !strings.Contains(buf.String(), "FATAL") {
		t.Error("expected FATAL in output")
	}
}

func TestGlobalDefault(t *testing.T) {
	// Reset global for test
	defaultLogger = nil

	d := Default()
	if d == nil {
		t.Fatal("Default() returned nil")
	}

	// Verify default has JSON enabled
	var buf bytes.Buffer
	d.output = &buf
	d.Info("global default")

	if !strings.Contains(buf.String(), "INFO") {
		t.Error("expected INFO in default logger output")
	}
}

func TestGlobalInit(t *testing.T) {
	var buf bytes.Buffer
	Init(LoggerConfig{
		ServiceName: "global-svc",
		Level:       LevelDebug,
		Output:      &buf,
		EnableJSON:  true,
	})

	d := Default()
	if d.serviceName != "global-svc" {
		t.Errorf("expected global-svc, got %s", d.serviceName)
	}
}

func TestGlobalInit_DefaultJSON(t *testing.T) {
	var buf bytes.Buffer
	Init(LoggerConfig{
		Level:  LevelInfo,
		Output: &buf,
	})

	// Init with EnableJSON=false by default
	d := Default()
	d.Info("no-json")
	// Should produce text output since EnableJSON defaults to false
	if strings.Contains(buf.String(), `"level"`) {
		t.Error("expected text output, not JSON")
	}
}

func TestGlobalFunctions(t *testing.T) {
	var buf bytes.Buffer
	Init(LoggerConfig{Level: LevelTrace, Output: &buf, EnableJSON: false})

	Trace("TAG", "trace global")
	Debug("TAG", "debug global")
	Info("TAG", "info global")
	Warn("TAG", "warn global")
	Error("TAG", "error global")
	Fatal("TAG", "fatal global")

	output := buf.String()
	for _, level := range []string{"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"} {
		if !strings.Contains(output, level) {
			t.Errorf("expected %s in global function output", level)
		}
	}
}

func TestGlobalFunctions_WithFields(t *testing.T) {
	var buf bytes.Buffer
	Init(LoggerConfig{Level: LevelInfo, Output: &buf})

	Info("SVC", "user action", WithUserID("user-1"), WithKeyID("key-a"))
	if !strings.Contains(buf.String(), "user-1") {
		t.Error("expected user-1 in global output")
	}
}

func TestLogger_TimestampFormat(t *testing.T) {
	var buf bytes.Buffer
	l := NewLogger(LoggerConfig{Level: LevelInfo, Output: &buf, EnableJSON: true})

	l.Info("time check")

	var entry LogEntry
	if err := json.Unmarshal(buf.Bytes(), &entry); err != nil {
		t.Fatalf("JSON parse error: %v", err)
	}
	if entry.Timestamp == "" {
		t.Error("expected non-empty timestamp")
	}

	// Verify it's parseable
	_, err := time.Parse(time.RFC3339Nano, entry.Timestamp)
	if err != nil {
		t.Errorf("timestamp %s is not RFC3339Nano: %v", entry.Timestamp, err)
	}
}

func TestLogger_LogConcurrency(t *testing.T) {
	var buf bytes.Buffer
	l := NewLogger(LoggerConfig{Level: LevelTrace, Output: &buf})

	done := make(chan bool, 50)
	for i := 0; i < 50; i++ {
		go func(n int) {
			l.Log(LevelInfo, "TAG", "concurrent test")
			done <- true
		}(i)
	}
	for i := 0; i < 50; i++ {
		<-done
	}

	lines := strings.Split(strings.TrimSpace(buf.String()), "\n")
	if len(lines) != 50 {
		t.Errorf("expected 50 log lines, got %d", len(lines))
	}
}

func TestModuleLogger_LogConcurrency(t *testing.T) {
	var buf bytes.Buffer
	l := NewLogger(LoggerConfig{Level: LevelTrace, Output: &buf})
	ml := l.Module("CONC")

	done := make(chan bool, 30)
	for i := 0; i < 30; i++ {
		go func() {
			ml.Info("concurrent module")
			done <- true
		}()
	}
	for i := 0; i < 30; i++ {
		<-done
	}

	lines := strings.Split(strings.TrimSpace(buf.String()), "\n")
	if len(lines) != 30 {
		t.Errorf("expected 30 module log lines, got %d", len(lines))
	}
}

func TestLogger_EmptyMessage(t *testing.T) {
	var buf bytes.Buffer
	l := NewLogger(LoggerConfig{Level: LevelInfo, Output: &buf})

	l.Info("")
	if buf.Len() == 0 {
		t.Error("expected output even with empty message")
	}
}

func TestLogger_LevelConstants(t *testing.T) {
	if LevelTrace >= LevelDebug {
		t.Error("TRACE should be < DEBUG")
	}
	if LevelDebug >= LevelInfo {
		t.Error("DEBUG should be < INFO")
	}
	if LevelInfo >= LevelWarn {
		t.Error("INFO should be < WARN")
	}
	if LevelWarn >= LevelError {
		t.Error("WARN should be < ERROR")
	}
	if LevelError >= LevelFatal {
		t.Error("ERROR should be < FATAL")
	}
}

func TestModuleLogger_EmptyMessage(t *testing.T) {
	var buf bytes.Buffer
	l := NewLogger(LoggerConfig{Level: LevelInfo, Output: &buf})
	ml := l.Module("TEST")

	ml.Warn("")
	if buf.Len() == 0 {
		t.Error("expected output from ModuleLogger even with empty message")
	}
}

func TestDefaultLogger_InitWithError(t *testing.T) {
	_ = NewLogger(LoggerConfig{
		Level: LevelError,
	})
	// Just ensure no panic
}

type someCustomError string

func (e someCustomError) Error() string { return string(e) }
