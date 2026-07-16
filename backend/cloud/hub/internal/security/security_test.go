// Package security — unit tests for security monitoring module
package security

import (
	"context"
	"fmt"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"go.uber.org/zap"
)

func TestNewDefaultMonitor(t *testing.T) {
	t.Run("with store and logger", func(t *testing.T) {
		store := NewInMemoryEventStore()
		m := NewDefaultMonitor(store, zap.NewNop())
		require.NotNil(t, m)
		assert.NotNil(t, m.logger)
		assert.NotNil(t, m.store)
	})

	t.Run("nil logger defaults to Nop", func(t *testing.T) {
		m := NewDefaultMonitor(nil, nil)
		require.NotNil(t, m)
		assert.NotNil(t, m.logger)
	})

	t.Run("nil store still works", func(t *testing.T) {
		m := NewDefaultMonitor(nil, zap.NewNop())
		require.NotNil(t, m)
	})
}

func TestDefaultMonitor_ReportEvent(t *testing.T) {
	ctx := context.Background()

	t.Run("reports and auto-generates event ID", func(t *testing.T) {
		store := NewInMemoryEventStore()
		m := NewDefaultMonitor(store, zap.NewNop())

		err := m.ReportEvent(ctx, ThreatEvent{
			EventType:   EventAuthFailure,
			DeviceID:    "dev-001",
			Severity:    SevHigh,
			Description: "auth failure test",
		})
		assert.NoError(t, err)
		assert.Len(t, m.events, 1)
		assert.NotEmpty(t, m.events[0].EventID)
	})

	t.Run("preserves provided event ID", func(t *testing.T) {
		m := NewDefaultMonitor(nil, nil)
		err := m.ReportEvent(ctx, ThreatEvent{
			EventID:   "custom-id-001",
			EventType: EventRelayAttack,
			DeviceID:  "dev-002",
			Severity:  SevCritical,
		})
		assert.NoError(t, err)
		assert.Equal(t, "custom-id-001", m.events[0].EventID)
	})

	t.Run("reports multiple events", func(t *testing.T) {
		m := NewDefaultMonitor(nil, nil)
		for i := 0; i < 5; i++ {
			err := m.ReportEvent(ctx, ThreatEvent{
				EventType: EventAuthFailure,
				DeviceID:  "dev-001",
				Severity:  SevLow,
			})
			assert.NoError(t, err)
		}
		assert.Len(t, m.events, 5)
	})
}

func TestDefaultMonitor_GetEvents(t *testing.T) {
	ctx := context.Background()

	t.Run("filters by event type", func(t *testing.T) {
		m := NewDefaultMonitor(nil, nil)
		_ = m.ReportEvent(ctx, ThreatEvent{EventType: EventAuthFailure, DeviceID: "d1", Severity: SevLow})
		_ = m.ReportEvent(ctx, ThreatEvent{EventType: EventRelayAttack, DeviceID: "d1", Severity: SevHigh})
		_ = m.ReportEvent(ctx, ThreatEvent{EventType: EventAuthFailure, DeviceID: "d2", Severity: SevMedium})

		events, err := m.GetEvents(ctx, EventFilter{EventType: EventAuthFailure})
		assert.NoError(t, err)
		assert.Len(t, events, 2)
	})

	t.Run("filters by device ID", func(t *testing.T) {
		m := NewDefaultMonitor(nil, nil)
		_ = m.ReportEvent(ctx, ThreatEvent{EventType: EventRelayAttack, DeviceID: "d1", Severity: SevLow})
		_ = m.ReportEvent(ctx, ThreatEvent{EventType: EventRelayAttack, DeviceID: "d2", Severity: SevLow})

		events, err := m.GetEvents(ctx, EventFilter{DeviceID: "d1"})
		assert.NoError(t, err)
		assert.Len(t, events, 1)
		assert.Equal(t, "d1", events[0].DeviceID)
	})

	t.Run("filters by severity", func(t *testing.T) {
		m := NewDefaultMonitor(nil, nil)
		_ = m.ReportEvent(ctx, ThreatEvent{EventType: EventAuthFailure, DeviceID: "d1", Severity: SevLow})
		_ = m.ReportEvent(ctx, ThreatEvent{EventType: EventRelayAttack, DeviceID: "d1", Severity: SevHigh})

		events, err := m.GetEvents(ctx, EventFilter{Severity: SevHigh})
		assert.NoError(t, err)
		assert.Len(t, events, 1)
		assert.Equal(t, SevHigh, events[0].Severity)
	})

	t.Run("returns all events with empty filter", func(t *testing.T) {
		m := NewDefaultMonitor(nil, nil)
		_ = m.ReportEvent(ctx, ThreatEvent{EventType: EventAuthFailure, DeviceID: "d1", Severity: SevLow})
		_ = m.ReportEvent(ctx, ThreatEvent{EventType: EventKeyTamper, DeviceID: "d2", Severity: SevLow})

		events, err := m.GetEvents(ctx, EventFilter{})
		assert.NoError(t, err)
		assert.Len(t, events, 2)
	})

	t.Run("uses store when available", func(t *testing.T) {
		store := NewInMemoryEventStore()
		m := NewDefaultMonitor(store, zap.NewNop())
		_ = m.ReportEvent(ctx, ThreatEvent{EventType: EventAuthFailure, DeviceID: "d1", Severity: SevLow})

		events, err := m.GetEvents(ctx, EventFilter{EventType: EventAuthFailure})
		assert.NoError(t, err)
		assert.Len(t, events, 1)
	})
}

func TestDefaultMonitor_GetStats(t *testing.T) {
	ctx := context.Background()

	t.Run("returns stats with empty state", func(t *testing.T) {
		m := NewDefaultMonitor(nil, nil)
		stats, err := m.GetStats(ctx)
		assert.NoError(t, err)
		assert.Equal(t, int64(0), stats.TotalEvents)
		assert.Equal(t, int64(0), stats.ActiveAlerts)
	})

	t.Run("returns accurate stats after events with timestamps", func(t *testing.T) {
		m := NewDefaultMonitor(nil, nil)
		now := time.Now()
		_ = m.ReportEvent(ctx, ThreatEvent{
			EventType: EventAuthFailure, DeviceID: "d1", Severity: SevHigh,
			Timestamp: now,
		})
		_ = m.ReportEvent(ctx, ThreatEvent{
			EventType: EventRelayAttack, DeviceID: "d2", Severity: SevCritical,
			Timestamp: now,
		})

		stats, err := m.GetStats(ctx)
		assert.NoError(t, err)
		assert.Equal(t, int64(2), stats.TotalEvents)
		assert.Equal(t, int64(1), stats.EventsBySeverity[SevHigh])
		assert.Equal(t, int64(1), stats.EventsBySeverity[SevCritical])
		assert.Equal(t, int64(1), stats.EventsByType[EventAuthFailure])
		assert.Equal(t, int64(1), stats.EventsByType[EventRelayAttack])
	})

	t.Run("counts active alerts", func(t *testing.T) {
		m := NewDefaultMonitor(nil, nil)
		_ = m.CreateAlert(ctx, Alert{AlertID: "a1", Severity: SevHigh})
		_ = m.CreateAlert(ctx, Alert{AlertID: "a2", Severity: SevLow})
		_ = m.AcknowledgeAlert(ctx, "a2")

		stats, err := m.GetStats(ctx)
		assert.NoError(t, err)
		assert.Equal(t, int64(1), stats.ActiveAlerts)
	})
}

func TestDefaultMonitor_CreateAndGetAlerts(t *testing.T) {
	ctx := context.Background()

	t.Run("creates and retrieves alerts", func(t *testing.T) {
		m := NewDefaultMonitor(nil, nil)
		err := m.CreateAlert(ctx, Alert{
			AlertID:  "alert-001",
			EventID:  "evt-001",
			RuleID:   "rule-high-failures",
			Title:    "High auth failure rate",
			Severity: SevHigh,
		})
		assert.NoError(t, err)

		alerts, err := m.GetAlerts(ctx, SevHigh)
		assert.NoError(t, err)
		assert.Len(t, alerts, 1)
		assert.Equal(t, "alert-001", alerts[0].AlertID)
	})

	t.Run("auto-generates alert ID when empty", func(t *testing.T) {
		m := NewDefaultMonitor(nil, nil)
		err := m.CreateAlert(ctx, Alert{
			Title:    "Auto ID test",
			Severity: SevMedium,
		})
		assert.NoError(t, err)
		assert.NotEmpty(t, m.alerts[0].AlertID)
	})

	t.Run("filters by severity", func(t *testing.T) {
		m := NewDefaultMonitor(nil, nil)
		_ = m.CreateAlert(ctx, Alert{AlertID: "a1", Title: "Low", Severity: SevLow})
		_ = m.CreateAlert(ctx, Alert{AlertID: "a2", Title: "High", Severity: SevHigh})

		alerts, err := m.GetAlerts(ctx, SevLow)
		assert.NoError(t, err)
		assert.Len(t, alerts, 1)
		assert.Equal(t, "a1", alerts[0].AlertID)
	})

	t.Run("empty severity returns all", func(t *testing.T) {
		m := NewDefaultMonitor(nil, nil)
		_ = m.CreateAlert(ctx, Alert{AlertID: "a1", Severity: SevLow})
		_ = m.CreateAlert(ctx, Alert{AlertID: "a2", Severity: SevHigh})

		alerts, err := m.GetAlerts(ctx, "")
		assert.NoError(t, err)
		assert.Len(t, alerts, 2)
	})
}

func TestDefaultMonitor_AcknowledgeAlert(t *testing.T) {
	ctx := context.Background()

	t.Run("acknowledges existing alert", func(t *testing.T) {
		m := NewDefaultMonitor(nil, nil)
		_ = m.CreateAlert(ctx, Alert{AlertID: "alert-001", Severity: SevHigh})

		err := m.AcknowledgeAlert(ctx, "alert-001")
		assert.NoError(t, err)

		alerts, _ := m.GetAlerts(ctx, SevHigh)
		for _, a := range alerts {
			if a.AlertID == "alert-001" {
				assert.True(t, a.Acknowledged)
			}
		}
	})

	t.Run("acknowledges non-existent alert returns error", func(t *testing.T) {
		m := NewDefaultMonitor(nil, nil)
		err := m.AcknowledgeAlert(ctx, "non-existent")
		assert.Error(t, err)
	})
}

func TestDefaultMonitor_Concurrency(t *testing.T) {
	ctx := context.Background()
	m := NewDefaultMonitor(nil, nil)

	t.Run("concurrent event reporting", func(t *testing.T) {
		done := make(chan struct{}, 2)
		go func() {
			for i := 0; i < 10; i++ {
				_ = m.ReportEvent(ctx, ThreatEvent{EventType: EventAuthFailure, DeviceID: "d1", Severity: SevLow})
			}
			done <- struct{}{}
		}()
		go func() {
			for i := 0; i < 10; i++ {
				_, _ = m.GetEvents(ctx, EventFilter{})
			}
			done <- struct{}{}
		}()

		<-done
		<-done
		assert.GreaterOrEqual(t, len(m.events), 10)
	})
}

// ── InMemory stores tests ──────────────────────────────────────────────

func TestInMemoryEventStore(t *testing.T) {
	ctx := context.Background()

	t.Run("SaveEvent and QueryEvents", func(t *testing.T) {
		s := NewInMemoryEventStore()
		err := s.SaveEvent(ctx, ThreatEvent{EventID: "e1", EventType: EventAuthFailure, DeviceID: "d1"})
		assert.NoError(t, err)

		events, err := s.QueryEvents(ctx, EventFilter{EventType: EventAuthFailure})
		assert.NoError(t, err)
		assert.Len(t, events, 1)
	})

	t.Run("DeleteEvents removes old events", func(t *testing.T) {
		s := NewInMemoryEventStore()
		_ = s.SaveEvent(ctx, ThreatEvent{
			EventID: "e1", EventType: EventAuthFailure,
			Timestamp: time.Now().Add(-48 * time.Hour),
		})
		_ = s.SaveEvent(ctx, ThreatEvent{
			EventID: "e2", EventType: EventRelayAttack,
			Timestamp: time.Now(),
		})

		cut := time.Now().Add(-24 * time.Hour).UnixMilli()
		deleted, err := s.DeleteEvents(ctx, cut)
		assert.NoError(t, err)
		assert.Equal(t, int64(1), deleted)
	})

	t.Run("GetEventCount", func(t *testing.T) {
		s := NewInMemoryEventStore()
		_ = s.SaveEvent(ctx, ThreatEvent{EventID: "e1", EventType: EventAuthFailure})
		_ = s.SaveEvent(ctx, ThreatEvent{EventID: "e2", EventType: EventRelayAttack})

		count, err := s.GetEventCount(ctx, EventFilter{})
		assert.NoError(t, err)
		assert.Equal(t, int64(2), count)
	})

	t.Run("QueryEvents with offset and limit", func(t *testing.T) {
		s := NewInMemoryEventStore()
		for i := 0; i < 5; i++ {
			_ = s.SaveEvent(ctx, ThreatEvent{
				EventID: fmt.Sprintf("e%d", i), EventType: EventAuthFailure,
			})
		}

		events, err := s.QueryEvents(ctx, EventFilter{Limit: 2, Offset: 0})
		assert.NoError(t, err)
		assert.Len(t, events, 2)
	})
}

func TestInMemoryAlertStore(t *testing.T) {
	ctx := context.Background()

	t.Run("SaveAlert and GetActiveAlerts", func(t *testing.T) {
		s := NewInMemoryAlertStore()
		err := s.SaveAlert(ctx, Alert{AlertID: "a1", Severity: SevHigh})
		assert.NoError(t, err)

		alerts, err := s.GetActiveAlerts(ctx)
		assert.NoError(t, err)
		assert.Len(t, alerts, 1)
	})

	t.Run("AcknowledgeAlert marks as acknowledged", func(t *testing.T) {
		s := NewInMemoryAlertStore()
		_ = s.SaveAlert(ctx, Alert{AlertID: "a1", Severity: SevHigh})

		err := s.AcknowledgeAlert(ctx, "a1")
		assert.NoError(t, err)

		alerts, err := s.GetActiveAlerts(ctx)
		assert.NoError(t, err)
		assert.Len(t, alerts, 0)
	})

	t.Run("ResolveAlert marks as resolved", func(t *testing.T) {
		s := NewInMemoryAlertStore()
		_ = s.SaveAlert(ctx, Alert{AlertID: "a1", Severity: SevHigh})

		err := s.ResolveAlert(ctx, "a1")
		assert.NoError(t, err)

		alerts, err := s.GetActiveAlerts(ctx)
		assert.NoError(t, err)
		assert.Len(t, alerts, 0)
	})

	t.Run("acknowledge non-existent returns error", func(t *testing.T) {
		s := NewInMemoryAlertStore()
		err := s.AcknowledgeAlert(ctx, "nonexistent")
		assert.Error(t, err)
	})

	t.Run("resolve non-existent returns error", func(t *testing.T) {
		s := NewInMemoryAlertStore()
		err := s.ResolveAlert(ctx, "nonexistent")
		assert.Error(t, err)
	})
}

// ── Type constant tests ──────────────────────────────────────────────────

func TestThreatEventTypes(t *testing.T) {
	assert.Equal(t, ThreatEventType("auth_failure"), EventAuthFailure)
	assert.Equal(t, ThreatEventType("relay_attack"), EventRelayAttack)
	assert.Equal(t, ThreatEventType("key_tamper"), EventKeyTamper)
	assert.Equal(t, ThreatEventType("replay_attack"), EventReplayAttack)
	assert.Equal(t, ThreatEventType("device_anomaly"), EventDeviceAnomaly)
	assert.Equal(t, ThreatEventType("brute_force"), EventBruteForce)
}

func TestSeverityValues(t *testing.T) {
	assert.Equal(t, Severity("low"), SevLow)
	assert.Equal(t, Severity("medium"), SevMedium)
	assert.Equal(t, Severity("high"), SevHigh)
	assert.Equal(t, Severity("critical"), SevCritical)
}
