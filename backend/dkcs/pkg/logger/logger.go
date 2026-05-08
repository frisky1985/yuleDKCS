// Stub logger package matching github.com/digitalkey/dkcs/pkg/logger
package logger

import "context"

// Logger stub for build purposes
type Logger struct{}

func New() *Logger {
	return &Logger{}
}

func (l *Logger) Info(msg string, fields ...interface{})  {}
func (l *Logger) Error(msg string, fields ...interface{}) {}
func (l *Logger) Debug(msg string, fields ...interface{}) {}
func (l *Logger) Warn(msg string, fields ...interface{})  {}
func (l *Logger) WithContext(ctx context.Context) *Logger { return l }
