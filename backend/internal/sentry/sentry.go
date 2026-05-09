package sentry

import (
	"fmt"
	"time"

	"github.com/getsentry/sentry-go"
)

// Config Sentry 配置
type Config struct {
	DSN              string
	Environment      string
	Release          string
	Debug            bool
	AttachStacktrace bool
}

// InitSentry 初始化 Sentry
func InitSentry(cfg *Config) error {
	if cfg.DSN == "" {
		return nil // 未配置 DSN，跳过初始化
	}

	err := sentry.Init(sentry.ClientOptions{
		DSN:              cfg.DSN,
		Environment:      cfg.Environment,
		Release:          cfg.Release,
		Debug:            cfg.Debug,
		AttachStacktrace: cfg.AttachStacktrace,
		BeforeSend: func(event *sentry.Event, hint *sentry.EventHint) *sentry.Event {
			// 过滤敏感信息
			if event.Request != nil {
				event.Request.Headers["Authorization"] = "[FILTERED]"
				event.Request.Headers["Cookie"] = "[FILTERED]"
			}
			return event
		},
	})

	if err != nil {
		return fmt.Errorf("failed to initialize sentry: %w", err)
	}

	return nil
}

// CaptureException 捕获异常
func CaptureException(err error) {
	if err != nil {
		sentry.CaptureException(err)
	}
}

// CaptureMessage 捕获消息
func CaptureMessage(message string) {
	sentry.CaptureMessage(message)
}

// CaptureEvent 捕获事件
func CaptureEvent(event *sentry.Event) {
	sentry.CaptureEvent(event)
}

// WithScope 使用自定义 scope
func WithScope(f func(scope *sentry.Scope)) {
	sentry.WithScope(f)
}

// ConfigureScope 配置全局 scope
func ConfigureScope(f func(scope *sentry.Scope)) {
	sentry.ConfigureScope(f)
}

// SetUser 设置用户信息
func SetUser(userID, email string) {
	sentry.ConfigureScope(func(scope *sentry.Scope) {
		scope.SetUser(sentry.User{
			ID:    userID,
			Email: email,
		})
	})
}

// SetTag 设置标签
func SetTag(key, value string) {
	sentry.ConfigureScope(func(scope *sentry.Scope) {
		scope.SetTag(key, value)
	})
}

// SetContext 设置上下文
func SetContext(name string, context map[string]interface{}) {
	sentry.ConfigureScope(func(scope *sentry.Scope) {
		scope.SetContext(name, context)
	})
}

// Flush 等待事件发送
func Flush(timeout time.Duration) bool {
	return sentry.Flush(timeout)
}
