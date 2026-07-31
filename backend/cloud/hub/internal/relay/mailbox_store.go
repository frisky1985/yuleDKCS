package relay

import (
	"context"
	"errors"
	"sync"
	"time"
)

// ErrMailboxNotFound 表示邮箱不存在。
var ErrMailboxNotFound = errors.New("mailbox not found")

// MailboxStore 提供 Mailbox 持久化。
// 实现:
//   - InMemoryMailboxStore (默认, 开发/测试)
//   - PostgresMailboxStore (量产, 见 internal/store)
//
// 所有实现必须 goroutine-safe。
type MailboxStore interface {
	// Create 创建邮箱。
	Create(ctx context.Context, mb *Mailbox) error
	// Get 按 ID 读取邮箱。
	Get(ctx context.Context, mailboxID string) (*Mailbox, error)
	// Update 覆盖写入整行（含版本号）。
	Update(ctx context.Context, mb *Mailbox) error
	// Delete 删除邮箱。
	Delete(ctx context.Context, mailboxID string) error
	// ListExpired 返回所有已过期但未标记 Expired 的邮箱。
	ListExpired(ctx context.Context, now time.Time) ([]*Mailbox, error)
}

// ─── InMemory 实现 (默认) ──────────────────────────────────

// InMemoryMailboxStore 是内存版 MailboxStore。
// 数据在进程重启后丢失 — 仅用于开发/测试。
type InMemoryMailboxStore struct {
	mu        sync.RWMutex
	mailboxes map[string]*Mailbox
}

func NewInMemoryMailboxStore() *InMemoryMailboxStore {
	return &InMemoryMailboxStore{
		mailboxes: make(map[string]*Mailbox),
	}
}

func (s *InMemoryMailboxStore) Create(_ context.Context, mb *Mailbox) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	cp := *mb
	s.mailboxes[mb.ID] = &cp
	return nil
}

func (s *InMemoryMailboxStore) Get(_ context.Context, mailboxID string) (*Mailbox, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	mb, ok := s.mailboxes[mailboxID]
	if !ok {
		return nil, ErrMailboxNotFound
	}
	cp := *mb
	return &cp, nil
}

func (s *InMemoryMailboxStore) Update(_ context.Context, mb *Mailbox) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	if _, ok := s.mailboxes[mb.ID]; !ok {
		return ErrMailboxNotFound
	}
	cp := *mb
	s.mailboxes[mb.ID] = &cp
	return nil
}

func (s *InMemoryMailboxStore) Delete(_ context.Context, mailboxID string) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	if _, ok := s.mailboxes[mailboxID]; !ok {
		return ErrMailboxNotFound
	}
	delete(s.mailboxes, mailboxID)
	return nil
}

func (s *InMemoryMailboxStore) ListExpired(_ context.Context, now time.Time) ([]*Mailbox, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	var result []*Mailbox
	for _, mb := range s.mailboxes {
		if now.After(mb.ExpiresAt) && mb.Status != StatusExpired {
			cp := *mb
			result = append(result, &cp)
		}
	}
	return result, nil
}
