package repository

import (
	"context"
	"sync"
	"testing"
	"time"

	"github.com/google/uuid"
)

// ---------------------------------------------------------------------------
// InMemoryKeyStore — an in-memory store implementing the KeyRepository
// contract without needing a real SQL/Redis backend.
// ---------------------------------------------------------------------------

type InMemoryKeyStore struct {
	mu   sync.RWMutex
	keys map[string]*Key
}

func NewInMemoryKeyStore() *InMemoryKeyStore {
	return &InMemoryKeyStore{keys: make(map[string]*Key)}
}

func (s *InMemoryKeyStore) Create(_ context.Context, key *Key) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	if _, exists := s.keys[key.ID]; exists {
		return ErrKeyConflict
	}
	cp := *key // shallow copy to avoid aliasing
	s.keys[key.ID] = &cp
	return nil
}

func (s *InMemoryKeyStore) GetByID(_ context.Context, id string) (*Key, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	k, ok := s.keys[id]
	if !ok {
		return nil, ErrKeyNotFound
	}
	cp := *k
	return &cp, nil
}

func (s *InMemoryKeyStore) Update(_ context.Context, key *Key) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	if _, ok := s.keys[key.ID]; !ok {
		return ErrKeyNotFound
	}
	cp := *key
	s.keys[key.ID] = &cp
	return nil
}

func (s *InMemoryKeyStore) Delete(_ context.Context, id string) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	delete(s.keys, id)
	return nil
}

func (s *InMemoryKeyStore) ListByUser(_ context.Context, userID string, limit, offset int) ([]*Key, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	var result []*Key
	for _, k := range s.keys {
		if k.UserID == userID {
			cp := *k
			result = append(result, &cp)
		}
	}
	// sort by CreatedAt desc
	for i := 0; i < len(result); i++ {
		for j := i + 1; j < len(result); j++ {
			if result[i].CreatedAt.Before(result[j].CreatedAt) {
				result[i], result[j] = result[j], result[i]
			}
		}
	}
	return applyPagination(result, limit, offset), nil
}

func (s *InMemoryKeyStore) ListByVehicle(_ context.Context, vehicleID string, limit, offset int) ([]*Key, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	var result []*Key
	for _, k := range s.keys {
		if k.VehicleID == vehicleID {
			cp := *k
			result = append(result, &cp)
		}
	}
	for i := 0; i < len(result); i++ {
		for j := i + 1; j < len(result); j++ {
			if result[i].CreatedAt.Before(result[j].CreatedAt) {
				result[i], result[j] = result[j], result[i]
			}
		}
	}
	return applyPagination(result, limit, offset), nil
}

// ListActiveByVehicle lists non-expired active keys for a vehicle.
func (s *InMemoryKeyStore) ListActiveByVehicle(_ context.Context, vehicleID string) ([]*Key, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	var result []*Key
	now := time.Now()
	for _, k := range s.keys {
		if k.VehicleID == vehicleID && k.Status == "active" && k.ExpiresAt.After(now) {
			cp := *k
			result = append(result, &cp)
		}
	}
	return result, nil
}

func applyPagination(keys []*Key, limit, offset int) []*Key {
	if offset >= len(keys) {
		return []*Key{}
	}
	end := offset + limit
	if end > len(keys) {
		end = len(keys)
	}
	return keys[offset:end]
}

// ErrKeyConflict is returned when a key with the same ID already exists.
var ErrKeyConflict = &ConflictError{Message: "key already exists"}

type ConflictError struct{ Message string }

func (e *ConflictError) Error() string { return e.Message }

// ---------------------------------------------------------------------------
// Helper: build a test key with sensible defaults
// ---------------------------------------------------------------------------

func testKey(overrides func(*Key)) *Key {
	now := time.Now().Truncate(time.Millisecond)
	k := &Key{
		ID:          uuid.New().String(),
		VehicleID:   "vehicle-001",
		UserID:      "user-001",
		KeyType:     "primary",
		Status:      "pending",
		Permissions: []string{"unlock", "start"},
		Secret:      "hashed-secret-value",
		CreatedAt:   now,
		ExpiresAt:   now.Add(365 * 24 * time.Hour),
		Metadata:    map[string]interface{}{"source": "test"},
	}
	if overrides != nil {
		overrides(k)
	}
	return k
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

func TestKeyRepo_CreateAndGet(t *testing.T) {
	store := NewInMemoryKeyStore()
	ctx := context.Background()

	key := testKey(nil)

	// Create
	if err := store.Create(ctx, key); err != nil {
		t.Fatalf("Create failed: %v", err)
	}

	// Get by ID
	got, err := store.GetByID(ctx, key.ID)
	if err != nil {
		t.Fatalf("GetByID failed: %v", err)
	}

	if got.ID != key.ID {
		t.Errorf("ID: want %q, got %q", key.ID, got.ID)
	}
	if got.VehicleID != key.VehicleID {
		t.Errorf("VehicleID: want %q, got %q", key.VehicleID, got.VehicleID)
	}
	if got.UserID != key.UserID {
		t.Errorf("UserID: want %q, got %q", key.UserID, got.UserID)
	}
	if got.Status != key.Status {
		t.Errorf("Status: want %q, got %q", key.Status, got.Status)
	}
	if got.Secret != key.Secret {
		t.Errorf("Secret: want %q, got %q", key.Secret, got.Secret)
	}
	if !got.CreatedAt.Equal(key.CreatedAt) {
		t.Errorf("CreatedAt: want %v, got %v", key.CreatedAt, got.CreatedAt)
	}
	if !got.ExpiresAt.Equal(key.ExpiresAt) {
		t.Errorf("ExpiresAt: want %v, got %v", key.ExpiresAt, got.ExpiresAt)
	}
	if len(got.Permissions) != 2 || got.Permissions[0] != "unlock" {
		t.Errorf("Permissions not preserved: %v", got.Permissions)
	}
	if got.Metadata["source"] != "test" {
		t.Errorf("Metadata not preserved")
	}
}

func TestKeyRepo_CreateDuplicate(t *testing.T) {
	store := NewInMemoryKeyStore()
	ctx := context.Background()

	key := testKey(nil)

	if err := store.Create(ctx, key); err != nil {
		t.Fatalf("first Create failed: %v", err)
	}

	// Second create with same ID should fail
	err := store.Create(ctx, key)
	if err == nil {
		t.Fatal("expected error for duplicate key, got nil")
	}
	if _, ok := err.(*ConflictError); !ok {
		t.Errorf("expected ConflictError, got %T: %v", err, err)
	}
}

func TestKeyRepo_GetNotFound(t *testing.T) {
	store := NewInMemoryKeyStore()
	ctx := context.Background()

	_, err := store.GetByID(ctx, "nonexistent-id")
	if err == nil {
		t.Fatal("expected ErrKeyNotFound, got nil")
	}
	if err != ErrKeyNotFound {
		t.Errorf("expected ErrKeyNotFound, got %v", err)
	}
}

func TestKeyRepo_Update(t *testing.T) {
	store := NewInMemoryKeyStore()
	ctx := context.Background()

	key := testKey(nil)
	if err := store.Create(ctx, key); err != nil {
		t.Fatalf("Create failed: %v", err)
	}

	// Update status and permissions
	key.Status = "active"
	now := time.Now().Truncate(time.Millisecond)
	key.ActivatedAt = &now
	key.Permissions = []string{"unlock", "start", "share"}

	if err := store.Update(ctx, key); err != nil {
		t.Fatalf("Update failed: %v", err)
	}

	got, err := store.GetByID(ctx, key.ID)
	if err != nil {
		t.Fatalf("GetByID after update failed: %v", err)
	}

	if got.Status != "active" {
		t.Errorf("Status: want 'active', got %q", got.Status)
	}
	if got.ActivatedAt == nil || !got.ActivatedAt.Equal(now) {
		t.Errorf("ActivatedAt not updated")
	}
	if len(got.Permissions) != 3 {
		t.Errorf("Permissions: want 3, got %d", len(got.Permissions))
	}
}

func TestKeyRepo_UpdateNotFound(t *testing.T) {
	store := NewInMemoryKeyStore()
	ctx := context.Background()

	key := testKey(nil)
	err := store.Update(ctx, key)
	if err == nil {
		t.Fatal("expected error for updating nonexistent key")
	}
	if err != ErrKeyNotFound {
		t.Errorf("expected ErrKeyNotFound, got %v", err)
	}
}

func TestKeyRepo_Delete(t *testing.T) {
	store := NewInMemoryKeyStore()
	ctx := context.Background()

	key := testKey(nil)
	if err := store.Create(ctx, key); err != nil {
		t.Fatalf("Create failed: %v", err)
	}

	if err := store.Delete(ctx, key.ID); err != nil {
		t.Fatalf("Delete failed: %v", err)
	}

	// Verify deletion
	_, err := store.GetByID(ctx, key.ID)
	if err != ErrKeyNotFound {
		t.Errorf("expected ErrKeyNotFound after delete, got %v", err)
	}
}

func TestKeyRepo_DeleteNotFound(t *testing.T) {
	store := NewInMemoryKeyStore()
	ctx := context.Background()

	// Deleting a nonexistent key should not error (idempotent)
	if err := store.Delete(ctx, "nonexistent"); err != nil {
		t.Errorf("expected no error for deleting nonexistent key, got %v", err)
	}
}

func TestKeyRepo_ListByUser(t *testing.T) {
	store := NewInMemoryKeyStore()
	ctx := context.Background()

	now := time.Now()
	keys := []*Key{
		testKey(func(k *Key) { k.ID = "k1"; k.UserID = "u1"; k.CreatedAt = now.Add(-2 * time.Hour) }),
		testKey(func(k *Key) { k.ID = "k2"; k.UserID = "u2"; k.CreatedAt = now.Add(-1 * time.Hour) }),
		testKey(func(k *Key) { k.ID = "k3"; k.UserID = "u1"; k.CreatedAt = now }),
		testKey(func(k *Key) { k.ID = "k4"; k.UserID = "u1"; k.CreatedAt = now.Add(-3 * time.Hour) }),
		testKey(func(k *Key) { k.ID = "k5"; k.UserID = "u3"; k.CreatedAt = now.Add(-4 * time.Hour) }),
	}

	for _, k := range keys {
		if err := store.Create(ctx, k); err != nil {
			t.Fatalf("Create %s failed: %v", k.ID, err)
		}
	}

	// List keys for u1
	result, err := store.ListByUser(ctx, "u1", 10, 0)
	if err != nil {
		t.Fatalf("ListByUser failed: %v", err)
	}
	if len(result) != 3 {
		t.Fatalf("want 3 keys for u1, got %d", len(result))
	}
	// Should be ordered by CreatedAt DESC: k3, k1, k4
	expectedOrder := []string{"k3", "k1", "k4"}
	for i, id := range expectedOrder {
		if result[i].ID != id {
			t.Errorf("position %d: want %s, got %s", i, id, result[i].ID)
		}
	}
}

func TestKeyRepo_ListByUser_Empty(t *testing.T) {
	store := NewInMemoryKeyStore()
	ctx := context.Background()

	result, err := store.ListByUser(ctx, "nonexistent-user", 10, 0)
	if err != nil {
		t.Fatalf("ListByUser failed: %v", err)
	}
	if len(result) != 0 {
		t.Errorf("want 0 keys, got %d", len(result))
	}
}

func TestKeyRepo_ListByUser_Pagination(t *testing.T) {
	store := NewInMemoryKeyStore()
	ctx := context.Background()

	now := time.Now()
	// Create 10 keys for u1
	for i := 0; i < 10; i++ {
		k := testKey(func(k *Key) {
			k.ID = uuid.New().String()
			k.UserID = "u1"
			k.CreatedAt = now.Add(-time.Duration(i) * time.Hour)
		})
		if err := store.Create(ctx, k); err != nil {
			t.Fatalf("Create failed: %v", err)
		}
	}

	// Page 1: limit=3, offset=0
	page1, err := store.ListByUser(ctx, "u1", 3, 0)
	if err != nil {
		t.Fatalf("ListByUser page 1 failed: %v", err)
	}
	if len(page1) != 3 {
		t.Errorf("page 1: want 3 keys, got %d", len(page1))
	}

	// Page 2: limit=3, offset=3
	page2, err := store.ListByUser(ctx, "u1", 3, 3)
	if err != nil {
		t.Fatalf("ListByUser page 2 failed: %v", err)
	}
	if len(page2) != 3 {
		t.Errorf("page 2: want 3 keys, got %d", len(page2))
	}

	// Verify no overlap
	page1IDs := make(map[string]bool)
	for _, k := range page1 {
		page1IDs[k.ID] = true
	}
	for _, k := range page2 {
		if page1IDs[k.ID] {
			t.Errorf("key %s appears on both pages", k.ID)
		}
	}

	// Page 3: limit=3, offset=6 (should return 1 key since total is 10)
	page3, err := store.ListByUser(ctx, "u1", 3, 6)
	if err != nil {
		t.Fatalf("ListByUser page 3 failed: %v", err)
	}
	if len(page3) != 3 {
		t.Errorf("page 3: want 3 keys, got %d", len(page3))
	}

	// Offset beyond data
	page4, err := store.ListByUser(ctx, "u1", 3, 10)
	if err != nil {
		t.Fatalf("ListByUser page 4 (beyond) failed: %v", err)
	}
	if len(page4) != 0 {
		t.Errorf("page beyond data: want 0 keys, got %d", len(page4))
	}
}

func TestKeyRepo_ListByVehicle(t *testing.T) {
	store := NewInMemoryKeyStore()
	ctx := context.Background()

	k1 := testKey(func(k *Key) { k.ID = "k-v1-a"; k.VehicleID = "v1"; k.UserID = "u1" })
	k2 := testKey(func(k *Key) { k.ID = "k-v1-b"; k.VehicleID = "v1"; k.UserID = "u2" })
	k3 := testKey(func(k *Key) { k.ID = "k-v2-a"; k.VehicleID = "v2"; k.UserID = "u1" })

	for _, k := range []*Key{k1, k2, k3} {
		if err := store.Create(ctx, k); err != nil {
			t.Fatalf("Create failed: %v", err)
		}
	}

	// v1 should have 2 keys
	result, err := store.ListByVehicle(ctx, "v1", 10, 0)
	if err != nil {
		t.Fatalf("ListByVehicle failed: %v", err)
	}
	if len(result) != 2 {
		t.Errorf("want 2 keys for v1, got %d", len(result))
	}

	// v2 should have 1 key
	result, err = store.ListByVehicle(ctx, "v2", 10, 0)
	if err != nil {
		t.Fatalf("ListByVehicle failed: %v", err)
	}
	if len(result) != 1 {
		t.Errorf("want 1 key for v2, got %d", len(result))
	}

	// nonexistent vehicle
	result, err = store.ListByVehicle(ctx, "v-nonexistent", 10, 0)
	if err != nil {
		t.Fatalf("ListByVehicle failed: %v", err)
	}
	if len(result) != 0 {
		t.Errorf("want 0 keys, got %d", len(result))
	}
}

func TestKeyRepo_ListActiveByVehicle(t *testing.T) {
	store := NewInMemoryKeyStore()
	ctx := context.Background()

	now := time.Now()

	// Active key within expiry (should appear)
	k1 := testKey(func(k *Key) { k.ID = "k-active-valid"; k.VehicleID = "v1"; k.Status = "active"; k.ExpiresAt = now.Add(24 * time.Hour) })

	// Active key that has expired (should NOT appear)
	k2 := testKey(func(k *Key) { k.ID = "k-active-expired"; k.VehicleID = "v1"; k.Status = "active"; k.ExpiresAt = now.Add(-1 * time.Hour) })

	// Suspended key (should NOT appear)
	k3 := testKey(func(k *Key) { k.ID = "k-suspended"; k.VehicleID = "v1"; k.Status = "suspended"; k.ExpiresAt = now.Add(24 * time.Hour) })

	// Pending key (should NOT appear)
	k4 := testKey(func(k *Key) { k.ID = "k-pending"; k.VehicleID = "v1"; k.Status = "pending"; k.ExpiresAt = now.Add(24 * time.Hour) })

	for _, k := range []*Key{k1, k2, k3, k4} {
		if err := store.Create(ctx, k); err != nil {
			t.Fatalf("Create failed: %v", err)
		}
	}

	result, err := store.ListActiveByVehicle(ctx, "v1")
	if err != nil {
		t.Fatalf("ListActiveByVehicle failed: %v", err)
	}
	if len(result) != 1 {
		t.Errorf("want exactly 1 active-valid key, got %d", len(result))
	}
	if len(result) == 1 && result[0].ID != "k-active-valid" {
		t.Errorf("expected k-active-valid, got %s", result[0].ID)
	}
}

func TestKeyRepo_Concurrency(t *testing.T) {
	store := NewInMemoryKeyStore()
	ctx := context.Background()

	// Create a key
	key := testKey(nil)
	if err := store.Create(ctx, key); err != nil {
		t.Fatalf("Create failed: %v", err)
	}

	// Concurrent updates
	const goroutines = 10
	done := make(chan struct{}, goroutines)
	for i := 0; i < goroutines; i++ {
		go func(i int) {
			key.Status = "active"
			_ = store.Update(ctx, key)
			done <- struct{}{}
		}(i)
	}
	for i := 0; i < goroutines; i++ {
		<-done
	}

	// Final state should be consistent
	got, err := store.GetByID(ctx, key.ID)
	if err != nil {
		t.Fatalf("GetByID after concurrent updates failed: %v", err)
	}
	if got.Status != "active" {
		t.Errorf("final status should be 'active', got %q", got.Status)
	}
}

func TestKeyRepo_FullCRUDLifecycle(t *testing.T) {
	// Full lifecycle: Create → Update → Get → List → Delete → Get (not found)
	store := NewInMemoryKeyStore()
	ctx := context.Background()

	key := testKey(nil)

	// Create
	if err := store.Create(ctx, key); err != nil {
		t.Fatalf("Create failed: %v", err)
	}

	// Update to active
	key.Status = "active"
	now := time.Now().Truncate(time.Millisecond)
	key.ActivatedAt = &now
	if err := store.Update(ctx, key); err != nil {
		t.Fatalf("Update failed: %v", err)
	}

	// Get and verify
	got, err := store.GetByID(ctx, key.ID)
	if err != nil {
		t.Fatalf("GetByID failed: %v", err)
	}
	if got.Status != "active" {
		t.Errorf("Status: want 'active', got %q", got.Status)
	}

	// List
	list, err := store.ListByUser(ctx, key.UserID, 10, 0)
	if err != nil {
		t.Fatalf("ListByUser failed: %v", err)
	}
	if len(list) != 1 {
		t.Errorf("ListByUser: want 1 key, got %d", len(list))
	}

	// Delete
	if err := store.Delete(ctx, key.ID); err != nil {
		t.Fatalf("Delete failed: %v", err)
	}

	// Verify deletion
	_, err = store.GetByID(ctx, key.ID)
	if err != ErrKeyNotFound {
		t.Errorf("GetByID after delete: expected ErrKeyNotFound, got %v", err)
	}
}
