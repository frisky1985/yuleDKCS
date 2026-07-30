package relay

import "context"

// ─── Push 通知消息 ────────────────────────────────────────────

// PushMessage 推送通知负载
type PushMessage struct {
	Title     string            // 通知标题
	Body      string            // 通知正文
	MailboxID string            // 关联的邮箱 ID
	Token     string            // 设备推送 token
	Vendor    string            // "apple" / "android"
	Data      map[string]string // 透传数据
}

// ─── PushNotifier 接口 ────────────────────────────────────────

// PushNotifier 向手机设备发送推送通知
// 实现：NoopPusher（默认空操作）、MockPusher（测试）、FCMPusher（Android）、APNsPusher（iOS）
type PushNotifier interface {
	Notify(ctx context.Context, msg PushMessage) error
}

// ─── NoopPusher（默认：什么都不做） ──────────────────────────────

// NoopPusher 空操作实现，确保不配置 Push 时系统也能正常工作
type NoopPusher struct{}

func (n *NoopPusher) Notify(_ context.Context, _ PushMessage) error {
	return nil
}

// ─── CompositePusher（多路广播：同时发给多个 Push 后端） ─────────

// CompositePusher 将通知同时发送给多个 PushNotifier
// 适合同时配置了 FCM (Android) + APNs (iOS) 的场景
type CompositePusher struct {
	pushers []PushNotifier
}

func NewCompositePusher(pushers ...PushNotifier) *CompositePusher {
	return &CompositePusher{pushers: pushers}
}

func (c *CompositePusher) Notify(ctx context.Context, msg PushMessage) error {
	for _, p := range c.pushers {
		_ = p.Notify(ctx, msg) // 单个失败不影响其他渠道
	}
	return nil
}
