// Package oms — unit tests for OMS (Key Lifecycle Management)
package oms

import (
	"context"
	"fmt"
	"sync"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// ── InMemoryKeyStore ────────────────────────────────────────────────────

// InMemoryKeyStore 内存钥匙存储，用于测试
type InMemoryKeyStore struct {
	mu   sync.RWMutex
	keys map[string]KeyRecord
}

func NewInMemoryKeyStore() *InMemoryKeyStore {
	return &InMemoryKeyStore{keys: make(map[string]KeyRecord)}
}

func (s *InMemoryKeyStore) SaveKey(_ context.Context, key KeyRecord) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.keys[key.KeyID] = key
	return nil
}

func (s *InMemoryKeyStore) GetKey(_ context.Context, keyID string) (*KeyRecord, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	k, ok := s.keys[keyID]
	if !ok {
		return nil, fmt.Errorf("oms: key %s not found", keyID)
	}
	return &k, nil
}

func (s *InMemoryKeyStore) ListKeys(_ context.Context, filter KeyFilter) ([]KeyRecord, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	var result []KeyRecord
	for _, k := range s.keys {
		if filter.OwnerID != nil && k.OwnerID != *filter.OwnerID {
			continue
		}
		if filter.DeviceID != nil && k.DeviceID != *filter.DeviceID {
			continue
		}
		if filter.State != nil && k.State != *filter.State {
			continue
		}
		if filter.OemID != nil && k.Metadata["oem_id"] != *filter.OemID {
			continue
		}
		result = append(result, k)
	}
	// apply offset/limit
	if filter.Offset > 0 && filter.Offset < len(result) {
		result = result[filter.Offset:]
	}
	if filter.Limit > 0 && len(result) > filter.Limit {
		result = result[:filter.Limit]
	}
	return result, nil
}

func (s *InMemoryKeyStore) DeleteKey(ctx context.Context, keyID string) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	k, ok := s.keys[keyID]
	if !ok {
		return fmt.Errorf("oms: key %s not found", keyID)
	}
	k.State = StateDeleted
	now := time.Now()
	k.DeletedAt = &now
	s.keys[keyID] = k
	return nil
}

func (s *InMemoryKeyStore) CountKeys(_ context.Context, filter KeyFilter) (int64, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	var count int64
	for _, k := range s.keys {
		if filter.State != nil && k.State != *filter.State {
			continue
		}
		count++
	}
	return count, nil
}

// ── Test InMemoryKeyStore ─────────────────────────────────────────────────

func TestInMemoryKeyStore_SaveAndGet(t *testing.T) {
	ctx := context.Background()

	s := NewInMemoryKeyStore()
	key := KeyRecord{
		KeyID:     "key-001",
		DeviceID:  "dev-001",
		OwnerID:   "user-001",
		State:     StateCreated,
		CreatedAt: time.Now(),
	}

	err := s.SaveKey(ctx, key)
	require.NoError(t, err)

	retrieved, err := s.GetKey(ctx, "key-001")
	require.NoError(t, err)
	assert.Equal(t, "key-001", retrieved.KeyID)
	assert.Equal(t, StateCreated, retrieved.State)
}

func TestInMemoryKeyStore_GetNotFound(t *testing.T) {
	ctx := context.Background()
	s := NewInMemoryKeyStore()

	_, err := s.GetKey(ctx, "non-existent")
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "not found")
}

func TestInMemoryKeyStore_ListKeys(t *testing.T) {
	ctx := context.Background()
	s := NewInMemoryKeyStore()

	keys := []KeyRecord{
		{KeyID: "k1", OwnerID: "owner-1", DeviceID: "d1", State: StateActive},
		{KeyID: "k2", OwnerID: "owner-1", DeviceID: "d2", State: StateActive},
		{KeyID: "k3", OwnerID: "owner-2", DeviceID: "d3", State: StateRevoked},
		{KeyID: "k4", OwnerID: "owner-1", DeviceID: "d4", State: StateSuspended},
	}
	for _, k := range keys {
		_ = s.SaveKey(ctx, k)
	}

	t.Run("list all", func(t *testing.T) {
		result, err := s.ListKeys(ctx, KeyFilter{})
		require.NoError(t, err)
		assert.Len(t, result, 4)
	})

	t.Run("filter by owner", func(t *testing.T) {
		owner := "owner-1"
		result, err := s.ListKeys(ctx, KeyFilter{OwnerID: &owner})
		require.NoError(t, err)
		assert.Len(t, result, 3)
	})

	t.Run("filter by state", func(t *testing.T) {
		state := StateRevoked
		result, err := s.ListKeys(ctx, KeyFilter{State: &state})
		require.NoError(t, err)
		assert.Len(t, result, 1)
	})

	t.Run("limit results", func(t *testing.T) {
		result, err := s.ListKeys(ctx, KeyFilter{Limit: 2})
		require.NoError(t, err)
		assert.Len(t, result, 2)
	})
}

func TestInMemoryKeyStore_Delete(t *testing.T) {
	ctx := context.Background()
	s := NewInMemoryKeyStore()

	_ = s.SaveKey(ctx, KeyRecord{KeyID: "k1", State: StateActive})
	err := s.DeleteKey(ctx, "k1")
	assert.NoError(t, err)

	k, _ := s.GetKey(ctx, "k1")
	assert.Equal(t, StateDeleted, k.State)
	assert.NotNil(t, k.DeletedAt)
}

func TestInMemoryKeyStore_DeleteNotFound(t *testing.T) {
	ctx := context.Background()
	s := NewInMemoryKeyStore()
	err := s.DeleteKey(ctx, "non-existent")
	assert.Error(t, err)
}

func TestInMemoryKeyStore_CountKeys(t *testing.T) {
	ctx := context.Background()
	s := NewInMemoryKeyStore()

	_ = s.SaveKey(ctx, KeyRecord{KeyID: "k1", State: StateActive})
	_ = s.SaveKey(ctx, KeyRecord{KeyID: "k2", State: StateActive})
	_ = s.SaveKey(ctx, KeyRecord{KeyID: "k3", State: StateRevoked})

	t.Run("all keys", func(t *testing.T) {
		count, err := s.CountKeys(ctx, KeyFilter{})
		assert.NoError(t, err)
		assert.Equal(t, int64(3), count)
	})

	t.Run("by state", func(t *testing.T) {
		state := StateActive
		count, err := s.CountKeys(ctx, KeyFilter{State: &state})
		assert.NoError(t, err)
		assert.Equal(t, int64(2), count)
	})
}

// ── State Machine Tests ──────────────────────────────────────────────────

func TestStateMachineTransitions(t *testing.T) {
	tests := []struct {
		from   KeyLifecycleState
		to     KeyLifecycleState
		valid  bool
		desc   string
	}{
		{StateCreated, StatePrePaired, true, "created → pre_paired"},
		{StatePrePaired, StatePaired, true, "pre_paired → paired"},
		{StatePrePaired, StateCreated, true, "pre_paired → created (rollback)"},
		{StatePaired, StateActive, true, "paired → active"},
		{StatePaired, StateRevoked, true, "paired → revoked"},
		{StateActive, StateSuspended, true, "active → suspended"},
		{StateActive, StateRevoked, true, "active → revoked"},
		{StateSuspended, StateActive, true, "suspended → active (restore)"},
		{StateSuspended, StateRevoked, true, "suspended → revoked"},
		{StateCreated, StateActive, false, "created → active (invalid)"},
		{StateRevoked, StateActive, false, "revoked → active (invalid - terminal)"},
		{StateRevoked, StateSuspended, false, "revoked → suspended (invalid)"},
		{StateDeleted, StateActive, false, "deleted → active (invalid)"},
		{StateActive, StateDeleted, false, "active → deleted (invalid)"},
	}

	for _, tt := range tests {
		t.Run(tt.desc, func(t *testing.T) {
			result := IsValidNextState(tt.from, tt.to)
			assert.Equal(t, tt.valid, result, "%s -> %s: expected %v, got %v", tt.from, tt.to, tt.valid, result)
		})
	}
}

func TestIsTerminal(t *testing.T) {
	assert.True(t, IsTerminal(StateRevoked))
	assert.True(t, IsTerminal(StateDeleted))
	assert.False(t, IsTerminal(StateActive))
	assert.False(t, IsTerminal(StateCreated))
	assert.False(t, IsTerminal(StatePaired))
	assert.False(t, IsTerminal(StateSuspended))
}

func TestValidTransitionsMap(t *testing.T) {
	// All defined states should be in ValidTransitions
	states := []KeyLifecycleState{
		StateCreated, StatePrePaired, StatePaired,
		StateActive, StateSuspended, StateRevoked, StateDeleted,
	}
	for _, s := range states {
		_, ok := ValidTransitions[s]
		assert.True(t, ok, "%s should be in ValidTransitions", s)
	}

	// Terminal states should have empty transition sets
	assert.Empty(t, ValidTransitions[StateRevoked])
	assert.Empty(t, ValidTransitions[StateDeleted])
}

// ── Provisioning tests ──────────────────────────────────────────────────

func TestProvisioningStatusValues(t *testing.T) {
	assert.Equal(t, ProvisioningStatus("pending"), ProvPending)
	assert.Equal(t, ProvisioningStatus("in_progress"), ProvInProgress)
	assert.Equal(t, ProvisioningStatus("completed"), ProvCompleted)
	assert.Equal(t, ProvisioningStatus("failed"), ProvFailed)
	assert.Equal(t, ProvisioningStatus("expired"), ProvExpired)
}

func TestDeploymentRecordDefaults(t *testing.T) {
	d := DeploymentRecord{
		DeployID: "deploy-001",
		OemID:    "nio",
		ModelID:  "et7",
		Version:  "1.0.0",
	}
	assert.Equal(t, 0, d.RolloutPct)
	assert.Empty(t, d.Status)
}

func TestUsageStatsEmpty(t *testing.T) {
	stats := UsageStats{
		ByAction: make(map[string]int64),
	}
	assert.Equal(t, int64(0), stats.TotalActions)
	assert.Empty(t, stats.ByAction)
}
