package unified

import (
	"fmt"
	"sync"
	"time"
)

// SessionState 会话状态
type SessionState int

const (
	StateInit SessionState = iota
	StateNegotiating       // 协议协商中
	StateDeviceVerified    // 设备验证通过
	StateKeyBinding        // 密钥绑定中
	StateActive            // 密钥激活/正常使用
	StateSharing           // 分享中
	StateSuspended         // 挂起
	StateRevoked           // 已撤销
)

func (s SessionState) String() string {
	return [...]string{
		"Init", "Negotiating", "DeviceVerified", "KeyBinding",
		"Active", "Sharing", "Suspended", "Revoked",
	}[s]
}

// IsTerminal 判断是否为终态
func (s SessionState) IsTerminal() bool {
	return s == StateRevoked
}

// Session 会话 - 管理单个设备的完整生命周期
type Session struct {
	SessionID   string
	Device      *Device
	State       SessionState
	Protocol    ProtocolType
	CreatedAt   time.Time
	UpdatedAt   time.Time
	LastActive  time.Time
	ExpiresAt   time.Time // 会话过期时间

	// 协议特定上下文
	ctx map[string]interface{}

	mu sync.RWMutex
}

func NewSession(id string, device *Device) *Session {
	now := time.Now()
	return &Session{
		SessionID:  id,
		Device:    device,
		State:     StateInit,
		CreatedAt: now,
		UpdatedAt: now,
		LastActive: now,
		ctx:       make(map[string]interface{}),
	}
}

func (s *Session) SetState(state SessionState) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.State = state
	s.UpdatedAt = time.Now()
}

func (s *Session) GetState() SessionState {
	s.mu.RLock()
	defer s.mu.RUnlock()
	return s.State
}

func (s *Session) SetContext(key string, val interface{}) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.ctx[key] = val
}

func (s *Session) GetContext(key string) (interface{}, bool) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	v, ok := s.ctx[key]
	return v, ok
}

func (s *Session) Touch() {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.LastActive = time.Now()
}

// Valid 检查会话是否有效
func (s *Session) Valid() bool {
	s.mu.RLock()
	defer s.mu.RUnlock()
	if s.State == StateRevoked || s.State == StateSuspended {
		return false
	}
	if !s.ExpiresAt.IsZero() && time.Now().After(s.ExpiresAt) {
		return false
	}
	return true
}

// StateMachine 状态机 - 定义会话状态转换规则
type StateMachine struct {
	transitions map[SessionState]map[Event]SessionState
}

type Event int

const (
	EventNegotiateStart Event = iota
	EventNegotiateDone
	EventDeviceVerifyPass
	EventDeviceVerifyFail
	EventBindStart
	EventBindSuccess
	EventBindFail
	EventShareStart
	EventShareDone
	EventSuspend
	EventResume
	EventRevoke
	EventExpire
	EventTimeout
)

// NewStateMachine 创建状态机
func NewStateMachine() *StateMachine {
	sm := &StateMachine{
		transitions: make(map[SessionState]map[Event]SessionState),
	}

	transitions := []struct {
		from  SessionState
		event Event
		to    SessionState
	}{
		{StateInit, EventNegotiateStart, StateNegotiating},
		{StateNegotiating, EventNegotiateDone, StateDeviceVerified},
		{StateNegotiating, EventDeviceVerifyFail, StateInit},
		{StateDeviceVerified, EventBindStart, StateKeyBinding},
		{StateDeviceVerified, EventDeviceVerifyFail, StateInit},
		{StateKeyBinding, EventBindSuccess, StateActive},
		{StateKeyBinding, EventBindFail, StateInit},
		{StateActive, EventShareStart, StateSharing},
		{StateSharing, EventShareDone, StateActive},
		{StateActive, EventSuspend, StateSuspended},
		{StateActive, EventRevoke, StateRevoked},
		{StateActive, EventExpire, StateRevoked},
		{StateSuspended, EventResume, StateActive},
		{StateSuspended, EventRevoke, StateRevoked},
		{StateSuspended, EventExpire, StateRevoked},
		{StateInit, EventTimeout, StateRevoked},
		{StateNegotiating, EventTimeout, StateRevoked},
	}

	for _, t := range transitions {
		if sm.transitions[t.from] == nil {
			sm.transitions[t.from] = make(map[Event]SessionState)
		}
		sm.transitions[t.from][t.event] = t.to
	}

	return sm
}

// Next 获取下一个状态
func (sm *StateMachine) Next(current SessionState, event Event) (SessionState, error) {
	if nextStates, ok := sm.transitions[current]; ok {
		if next, ok := nextStates[event]; ok {
			return next, nil
		}
	}
	return current, fmt.Errorf("invalid transition: %s + %s", current, event)
}

// CanTransition 判断是否可以转换
func (sm *StateMachine) CanTransition(current SessionState, event Event) bool {
	_, err := sm.Next(current, event)
	return err == nil
}

// SessionManager 会话管理器
type SessionManager struct {
	sessions map[string]*Session
	mu       sync.RWMutex
	sm       *StateMachine
	timeout  time.Duration
}

func NewSessionManager(timeout time.Duration) *SessionManager {
	return &SessionManager{
		sessions: make(map[string]*Session),
		sm:       NewStateMachine(),
		timeout:  timeout,
	}
}

// CreateSession 创建新会话
func (m *SessionManager) CreateSession(id string, device *Device) *Session {
	m.mu.Lock()
	defer m.mu.Unlock()
	s := NewSession(id, device)
	s.ExpiresAt = time.Now().Add(m.timeout)
	m.sessions[id] = s
	return s
}

// GetSession 获取会话
func (m *SessionManager) GetSession(id string) (*Session, bool) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	s, ok := m.sessions[id]
	return s, ok
}

// GetOrCreate 获取或创建会话
func (m *SessionManager) GetOrCreate(id string, device *Device) *Session {
	m.mu.Lock()
	defer m.mu.Unlock()
	if s, ok := m.sessions[id]; ok && s.Valid() {
		return s
	}
	s := NewSession(id, device)
	s.ExpiresAt = time.Now().Add(m.timeout)
	m.sessions[id] = s
	return s
}

// Transition 状态转换
func (m *SessionManager) Transition(sessionID string, event Event) error {
	m.mu.Lock()
	defer m.mu.Unlock()

	s, ok := m.sessions[sessionID]
	if !ok {
		return fmt.Errorf("session not found: %s", sessionID)
	}

	next, err := m.sm.Next(s.State, event)
	if err != nil {
		return err
	}

	s.State = next
	s.UpdatedAt = time.Now()
	return nil
}

// ExpireSession 使会话过期
func (m *SessionManager) ExpireSession(id string) error {
	return m.Transition(id, EventExpire)
}

// ListActive 列出活跃会话
func (m *SessionManager) ListActive() []*Session {
	m.mu.RLock()
	defer m.mu.RUnlock()

	var active []*Session
	now := time.Now()
	for _, s := range m.sessions {
		if s.Valid() && s.UpdatedAt.After(now.Add(-m.timeout)) {
			active = append(active, s)
		}
	}
	return active
}

// Cleanup 清理过期会话
func (m *SessionManager) Cleanup() int {
	m.mu.Lock()
	defer m.mu.Unlock()

	now := time.Now()
	removed := 0
	for id, s := range m.sessions {
		if !s.Valid() || s.UpdatedAt.Before(now.Add(-2*m.timeout)) {
			delete(m.sessions, id)
			removed++
		}
	}
	return removed
}

// Remove 删除会话
func (m *SessionManager) Remove(id string) {
	m.mu.Lock()
	defer m.mu.Unlock()
	delete(m.sessions, id)
}
