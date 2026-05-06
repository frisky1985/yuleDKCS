// logger.go - 统一日志接口
// 三端统一日志规范

package logger

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"os"
	"sync"
	"time"
)

// 日志级别
type Level int

const (
	LevelTrace Level = 0
	LevelDebug Level = 1
	LevelInfo  Level = 2
	LevelWarn  Level = 3
	LevelError Level = 4
	LevelFatal Level = 5
)

func (l Level) String() string {
	switch l {
	case LevelTrace:
		return "TRACE"
	case LevelDebug:
		return "DEBUG"
	case LevelInfo:
		return "INFO"
	case LevelWarn:
		return "WARN"
	case LevelError:
		return "ERROR"
	case LevelFatal:
		return "FATAL"
	default:
		return "UNKNOWN"
	}
}

// 日志模块标签
const (
	TagInit     = "INIT"
	TagKeyMgr   = "KEYMGR"
	TagAuth     = "AUTH"
	TagBLE      = "BLE"
	TagNFC      = "NFC"
	TagUWB      = "UWB"
	TagSec      = "SEC"
	TagTransport = "TRANSPORT"
	TagProtocol  = "PROTO"
	TagService   = "SERVICE"
	TagAdapter   = "ADAPTER"
	TagDB        = "DB"
	TagCache     = "CACHE"
	TagMQTT      = "MQTT"
	TagGRPC      = "GRPC"
)

// LogEntry 日志条目
type LogEntry struct {
	Timestamp  string                 `json:"timestamp"`
	Level      string                 `json:"level"`
	Service    string                 `json:"service,omitempty"`
	Instance   string                 `json:"instance,omitempty"`
	TraceID    string                 `json:"trace_id,omitempty"`
	SpanID     string                 `json:"span_id,omitempty"`
	Logger     string                 `json:"logger"`
	Message    string                 `json:"message"`
	Fields     map[string]interface{} `json:"fields,omitempty"`
}

// Field 日志字段
type Field struct {
	Key   string
	Value interface{}
}

// WithTraceID 添加TraceID字段
func WithTraceID(traceID string) Field {
	return Field{Key: "trace_id", Value: traceID}
}

// WithError 添加Error字段
func WithError(err error) Field {
	return Field{Key: "error", Value: err.Error()}
}

// WithErrorCode 添加错误码字段
func WithErrorCode(code uint16) Field {
	return Field{Key: "error_code", Value: fmt.Sprintf("0x%04X", code)}
}

// WithDuration 添加耗时字段
func WithDuration(ms uint64) Field {
	return Field{Key: "duration_ms", Value: ms}
}

// WithKeyID 添加密钥ID字段
func WithKeyID(id string) Field {
	return Field{Key: "key_id", Value: id}
}

// WithVehicleID 添加车辆ID字段
func WithVehicleID(id string) Field {
	return Field{Key: "vehicle_id", Value: id}
}

// WithUserID 添加用户ID字段
func WithUserID(id string) Field {
	return Field{Key: "user_id", Value: id}
}

// WithDeviceID 添加设备ID字段
func WithDeviceID(id string) Field {
	return Field{Key: "device_id", Value: id}
}

// WithField 添加自定义字段
func WithField(key string, value interface{}) Field {
	return Field{Key: key, Value: value}
}

// Logger 日志接口
type Logger interface {
	Trace(msg string, fields ...Field)
	Debug(msg string, fields ...Field)
	Info(msg string, fields ...Field)
	Warn(msg string, fields ...Field)
	Error(msg string, fields ...Field)
	Fatal(msg string, fields ...Field)
	WithContext(ctx context.Context) Logger
}

// DefaultLogger 默认日志实现
type DefaultLogger struct {
	mu           sync.Mutex
	output       io.Writer
	level        Level
	serviceName  string
	instanceID   string
	enableJSON   bool
}

// LoggerConfig 日志配置
type LoggerConfig struct {
	ServiceName string
	InstanceID  string
	Level       Level
	Output      io.Writer
	EnableJSON  bool
}

// NewLogger 创建日志器
func NewLogger(config LoggerConfig) *DefaultLogger {
	if config.Output == nil {
		config.Output = os.Stdout
	}
	return &DefaultLogger{
		output:      config.Output,
		level:       config.Level,
		serviceName: config.ServiceName,
		instanceID:  config.InstanceID,
		enableJSON:  config.EnableJSON,
	}
}

// SetLevel 设置日志级别
func (l *DefaultLogger) SetLevel(level Level) {
	l.level = level
}

// Log 输出日志
func (l *DefaultLogger) Log(level Level, tag string, msg string, fields ...Field) {
	if level < l.level {
		return
	}

	entry := LogEntry{
		Timestamp: time.Now().Format(time.RFC3339Nano),
		Level:    level.String(),
		Service:  l.serviceName,
		Instance: l.instanceID,
		Logger:   tag,
		Message:  msg,
		Fields:   make(map[string]interface{}),
	}

	for _, f := range fields {
		entry.Fields[f.Key] = f.Value
	}

	l.mu.Lock()
	defer l.mu.Unlock()

	if l.enableJSON {
		data, _ := json.Marshal(entry)
		fmt.Fprintln(l.output, string(data))
	} else {
		// 文本格式
		fieldsStr := ""
		for k, v := range entry.Fields {
			fieldsStr += fmt.Sprintf(" %s=%v", k, v)
		}
		fmt.Fprintf(l.output, "[%s][%s][%s] %s%s\n",
			entry.Timestamp, entry.Level, tag, msg, fieldsStr)
	}
}

// Trace 跟踪日志
func (l *DefaultLogger) Trace(msg string, fields ...Field) {
	l.Log(LevelTrace, TagInit, msg, fields...)
}

// Debug 调试日志
func (l *DefaultLogger) Debug(msg string, fields ...Field) {
	l.Log(LevelDebug, TagInit, msg, fields...)
}

// Info 信息日志
func (l *DefaultLogger) Info(msg string, fields ...Field) {
	l.Log(LevelInfo, TagInit, msg, fields...)
}

// Warn 警告日志
func (l *DefaultLogger) Warn(msg string, fields ...Field) {
	l.Log(LevelWarn, TagInit, msg, fields...)
}

// Error 错误日志
func (l *DefaultLogger) Error(msg string, fields ...Field) {
	l.Log(LevelError, TagInit, msg, fields...)
}

// Fatal 致命错误日志
func (l *DefaultLogger) Fatal(msg string, fields ...Field) {
	l.Log(LevelFatal, TagInit, msg, fields...)
}

// WithContext 获取带上下文的日志器
func (l *DefaultLogger) WithContext(ctx context.Context) Logger {
	// 从context提取trace_id等信息
	return l
}

// ModuleLogger 模块日志器
type ModuleLogger struct {
	logger *DefaultLogger
	module string
}

// Module 获取模块日志器
func (l *DefaultLogger) Module(module string) *ModuleLogger {
	return &ModuleLogger{
		logger: l,
		module: module,
	}
}

// Trace 跟踪日志
func (m *ModuleLogger) Trace(msg string, fields ...Field) {
	m.logger.Log(LevelTrace, m.module, msg, fields...)
}

// Debug 调试日志
func (m *ModuleLogger) Debug(msg string, fields ...Field) {
	m.logger.Log(LevelDebug, m.module, msg, fields...)
}

// Info 信息日志
func (m *ModuleLogger) Info(msg string, fields ...Field) {
	m.logger.Log(LevelInfo, m.module, msg, fields...)
}

// Warn 警告日志
func (m *ModuleLogger) Warn(msg string, fields ...Field) {
	m.logger.Log(LevelWarn, m.module, msg, fields...)
}

// Error 错误日志
func (m *ModuleLogger) Error(msg string, fields ...Field) {
	m.logger.Log(LevelError, m.module, msg, fields...)
}

// Fatal 致命错误日志
func (m *ModuleLogger) Fatal(msg string, fields ...Field) {
	m.logger.Log(LevelFatal, m.module, msg, fields...)
}

// 预定义的模块日志器
func (l *DefaultLogger) Init() *ModuleLogger     { return l.Module(TagInit) }
func (l *DefaultLogger) KeyMgr() *ModuleLogger   { return l.Module(TagKeyMgr) }
func (l *DefaultLogger) Auth() *ModuleLogger     { return l.Module(TagAuth) }
func (l *DefaultLogger) BLE() *ModuleLogger      { return l.Module(TagBLE) }
func (l *DefaultLogger) NFC() *ModuleLogger      { return l.Module(TagNFC) }
func (l *DefaultLogger) UWB() *ModuleLogger      { return l.Module(TagUWB) }
func (l *DefaultLogger) Sec() *ModuleLogger      { return l.Module(TagSec) }
func (l *DefaultLogger) Transport() *ModuleLogger { return l.Module(TagTransport) }
func (l *DefaultLogger) Protocol() *ModuleLogger { return l.Module(TagProtocol) }
func (l *DefaultLogger) Service() *ModuleLogger   { return l.Module(TagService) }
func (l *DefaultLogger) Adapter() *ModuleLogger   { return l.Module(TagAdapter) }
func (l *DefaultLogger) DB() *ModuleLogger        { return l.Module(TagDB) }
func (l *DefaultLogger) Cache() *ModuleLogger     { return l.Module(TagCache) }
func (l *DefaultLogger) MQTT() *ModuleLogger      { return l.Module(TagMQTT) }
func (l *DefaultLogger) GRPC() *ModuleLogger     { return l.Module(TagGRPC) }

// 全局日志器
var defaultLogger *DefaultLogger

// Init 初始化全局日志器
func Init(config LoggerConfig) {
	defaultLogger = NewLogger(config)
}

// Default 获取默认日志器
func Default() *DefaultLogger {
	if defaultLogger == nil {
		defaultLogger = NewLogger(LoggerConfig{
			Level:      LevelInfo,
			Output:     os.Stdout,
			EnableJSON: true,
		})
	}
	return defaultLogger
}

// 便捷函数
func Trace(tag string, msg string, fields ...Field) { Default().Log(LevelTrace, tag, msg, fields...) }
func Debug(tag string, msg string, fields ...Field) { Default().Log(LevelDebug, tag, msg, fields...) }
func Info(tag string, msg string, fields ...Field) { Default().Log(LevelInfo, tag, msg, fields...) }
func Warn(tag string, msg string, fields ...Field) { Default().Log(LevelWarn, tag, msg, fields...) }
func Error(tag string, msg string, fields ...Field) { Default().Log(LevelError, tag, msg, fields...) }
func Fatal(tag string, msg string, fields ...Field) { Default().Log(LevelFatal, tag, msg, fields...) }
