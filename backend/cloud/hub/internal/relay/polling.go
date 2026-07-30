package relay

import "time"

// ─── PollingStrategy ────────────────────────────────────────────

// PollingStrategy 实现 CCC Digital Key v4.0 §11.3.1 规定的
// 轮询降级策略：当 Push 通知不可用或 Token 未配置时，
// 推荐递增的轮询间隔以降低服务器和客户端负载。
//
// 阶段1: age ≤ 30s        → 每 5s 轮询
// 阶段2: 30s < age ≤ 2m   → 每 10s 轮询
// 阶段3: 2m < age ≤ 10m   → 每 30s 轮询
// 阶段4: age > 10m        → 每 60s 轮询
type PollingStrategy struct{}

// NextInterval 根据 mailbox 已存在时间返回推荐的轮询间隔。
// age: mailbox 从创建到现在的存活时长。
// 返回: 推荐的轮询间隔（time.Duration）。
func (p *PollingStrategy) NextInterval(age time.Duration) time.Duration {
	switch {
	case age <= 30*time.Second:
		return 5 * time.Second
	case age <= 2*time.Minute:
		return 10 * time.Second
	case age <= 10*time.Minute:
		return 30 * time.Second
	default:
		return 60 * time.Second
	}
}
