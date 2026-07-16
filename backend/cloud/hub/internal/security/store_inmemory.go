// ── InMemory 安全存储实现 ──────────────────────────────────────────────────
// 用于开发与测试，生产环境应替换为 PostgreSQL/Elasticsearch

package security

import (
	"context"
	"fmt"
	"sync"
)

// ── InMemoryEventStore ──────────────────────────────────────────────────

// InMemoryEventStore 内存级安全事件存储
type InMemoryEventStore struct {
	mu     sync.RWMutex
	events []ThreatEvent
}

// NewInMemoryEventStore 创建内存事件存储
func NewInMemoryEventStore() *InMemoryEventStore {
	return &InMemoryEventStore{
		events: make([]ThreatEvent, 0),
	}
}

func (s *InMemoryEventStore) SaveEvent(_ context.Context, event ThreatEvent) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.events = append(s.events, event)
	return nil
}

func (s *InMemoryEventStore) QueryEvents(_ context.Context, filter EventFilter) ([]ThreatEvent, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()

	var result []ThreatEvent
	for _, e := range s.events {
		if filter.EventType != "" && e.EventType != filter.EventType {
			continue
		}
		if filter.DeviceID != "" && e.DeviceID != filter.DeviceID {
			continue
		}
		if filter.Severity != "" && e.Severity != filter.Severity {
			continue
		}
		if !filter.Since.IsZero() && e.Timestamp.Before(filter.Since) {
			continue
		}
		if !filter.Until.IsZero() && e.Timestamp.After(filter.Until) {
			continue
		}
		result = append(result, e)
	}

	// Apply offset and limit
	if filter.Offset > 0 && filter.Offset < len(result) {
		result = result[filter.Offset:]
	}
	if filter.Limit > 0 && filter.Limit < len(result) {
		result = result[:filter.Limit]
	}

	return result, nil
}

func (s *InMemoryEventStore) DeleteEvents(_ context.Context, beforeTimestamp int64) (int64, error) {
	s.mu.Lock()
	defer s.mu.Unlock()

	cut := len(s.events)
	var deleted int64
	for i, e := range s.events {
		if e.Timestamp.UnixMilli() < beforeTimestamp {
			cut = i
			deleted++
			break
		}
	}

	if cut < len(s.events) {
		s.events = append(s.events[:cut], s.events[cut+1:]...)
	}

	return deleted, nil
}

func (s *InMemoryEventStore) GetEventCount(_ context.Context, filter EventFilter) (int64, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()

	var count int64
	for _, e := range s.events {
		if filter.EventType != "" && e.EventType != filter.EventType {
			continue
		}
		if filter.DeviceID != "" && e.DeviceID != filter.DeviceID {
			continue
		}
		count++
	}
	return count, nil
}

// ── InMemoryAlertStore ──────────────────────────────────────────────────

// InMemoryAlertStore 内存级告警存储
type InMemoryAlertStore struct {
	mu     sync.RWMutex
	alerts []Alert
}

// NewInMemoryAlertStore 创建内存告警存储
func NewInMemoryAlertStore() *InMemoryAlertStore {
	return &InMemoryAlertStore{
		alerts: make([]Alert, 0),
	}
}

func (s *InMemoryAlertStore) SaveAlert(_ context.Context, alert Alert) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.alerts = append(s.alerts, alert)
	return nil
}

func (s *InMemoryAlertStore) GetActiveAlerts(_ context.Context) ([]Alert, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()

	var result []Alert
	for _, a := range s.alerts {
		if !a.Acknowledged {
			result = append(result, a)
		}
	}
	return result, nil
}

func (s *InMemoryAlertStore) AcknowledgeAlert(_ context.Context, alertID string) error {
	s.mu.Lock()
	defer s.mu.Unlock()

	for i := range s.alerts {
		if s.alerts[i].AlertID == alertID {
			s.alerts[i].Acknowledged = true
			return nil
		}
	}
	return fmt.Errorf("alert %s not found", alertID)
}

func (s *InMemoryAlertStore) ResolveAlert(_ context.Context, alertID string) error {
	s.mu.Lock()
	defer s.mu.Unlock()

	for i := range s.alerts {
		if s.alerts[i].AlertID == alertID {
			// Mark as acknowledged (resolved = acknowledged)
			s.alerts[i].Acknowledged = true
			return nil
		}
	}
	return fmt.Errorf("alert %s not found", alertID)
}
