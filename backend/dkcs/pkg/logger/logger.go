// Stub logger package matching github.com/frisky1985/yuleDKCS/backend/dkcs/pkg/logger
package logger

import "context"

// Config logger configuration
type Config struct {
	Level  string // debug, info, warn, error
	Format string // json, text
	Output string // stdout, stderr, file
	File   string // 日志文件路径
}

// Logger stub for build purposes
type Logger struct{}

func New() *Logger {
	return &Logger{}
}

// NewLogger creates a new logger from config
func NewLogger(cfg *Config) *Logger {
	return &Logger{}
}

func (l *Logger) Info(msg string, fields ...interface{})  {}
func (l *Logger) Error(msg string, fields ...interface{}) {}
func (l *Logger) Debug(msg string, fields ...interface{}) {}
func (l *Logger) Warn(msg string, fields ...interface{})  {}
func (l *Logger) Fatal(msg string, fields ...interface{}) {}
func (l *Logger) Sync() error                             { return nil }
func (l *Logger) WithContext(ctx context.Context) *Logger  { return l }

// Err returns an error field
func Err(err error) interface{} { return err }

// String returns a string field
func String(key, value string) interface{} { return value }

// Int returns an int field
func Int(key string, value int) interface{} { return value }

// Any returns a generic field
func Any(key string, value interface{}) interface{} { return value }
