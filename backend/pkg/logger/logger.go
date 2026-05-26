// Package logger 提供结构化日志功能
package logger

import (
	"fmt"
	"log"
	"os"
	"time"
)

// Field 日志字段
type Field struct {
	Key   string
	Value interface{}
}

// String 创建字符串字段
func String(key, val string) Field {
	return Field{Key: key, Value: val}
}

// ErrField 创建错误字段
func ErrField(err error) Field {
	return Field{Key: "error", Value: err.Error()}
}

// Int 创建整数字段
func Int(key string, val int) Field {
	return Field{Key: key, Value: val}
}

// Bool 创建布尔字段
func Bool(key string, val bool) Field {
	return Field{Key: key, Value: val}
}

// logf 输出日志
func logf(level string, msg string, fields ...Field) {
	text := fmt.Sprintf("[%s] %s %s", level, time.Now().Format(time.RFC3339), msg)
	for _, f := range fields {
		text += fmt.Sprintf(" %s=%v", f.Key, f.Value)
	}
	log.Println(text)
}

// Info 输出信息日志
func Info(msg string, fields ...Field) {
	logf("INFO", msg, fields...)
}

// Warn 输出警告日志
func Warn(msg string, fields ...Field) {
	logf("WARN", msg, fields...)
}

// Error 输出错误日志
func Error(msg string, fields ...Field) {
	logf("ERROR", msg, fields...)
	os.Stderr.Sync()
}

// Debug 输出调试日志
func Debug(msg string, fields ...Field) {
	logf("DEBUG", msg, fields...)
}
