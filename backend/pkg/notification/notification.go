// Package notification 提供通知发送接口
package notification

import "context"

// Notifier 通知发送器接口
type Notifier interface {
	// Send 发送通知
	// ctx: 上下文
	// to: 接收方 (手机号/邮箱/设备Token)
	// message: 通知内容
	Send(ctx context.Context, to string, message string) error
}

// NotifierFunc 函数适配器，方便测试
type NotifierFunc func(ctx context.Context, to string, message string) error

func (f NotifierFunc) Send(ctx context.Context, to string, message string) error {
	return f(ctx, to, message)
}

// NoopNotifier 空操作通知器（用于开发测试）
var NoopNotifier Notifier = NoopNotifierFunc{}

type NoopNotifierFunc struct{}

func (NoopNotifierFunc) Send(_ context.Context, _ string, _ string) error {
	return nil
}
