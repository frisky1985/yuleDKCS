package relay

import (
	"context"
	"sync"
)

// MockPusher 测试用 Push 模拟器
// 记录所有发送的通知，方便测试断言
type MockPusher struct {
	mu   sync.Mutex
	Sent []PushMessage
}

func NewMockPusher() *MockPusher {
	return &MockPusher{}
}

func (m *MockPusher) Notify(_ context.Context, msg PushMessage) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.Sent = append(m.Sent, msg)
	return nil
}

// Messages 返回所有已发送通知的副本（线程安全）
func (m *MockPusher) Messages() []PushMessage {
	m.mu.Lock()
	defer m.mu.Unlock()
	cp := make([]PushMessage, len(m.Sent))
	copy(cp, m.Sent)
	return cp
}

// Reset 清空记录
func (m *MockPusher) Reset() {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.Sent = nil
}
