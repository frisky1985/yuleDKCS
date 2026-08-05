package repository

import (
	"context"
	"testing"
	"time"

	"github.com/alicebob/miniredis/v2"
	"github.com/redis/go-redis/v9"
)

// ─────────────────────────────────────────────────────────────
// Helper: create mock infrastructure
// ─────────────────────────────────────────────────────────────

func newMockRedis(t *testing.T) *miniredis.Miniredis {
	t.Helper()
	mr, err := miniredis.Run()
	if err != nil {
		t.Fatalf("miniredis.Run: %v", err)
	}
	t.Cleanup(mr.Close)
	return mr
}

func newRedisClient(mr *miniredis.Miniredis) *redis.Client {
	return redis.NewClient(&redis.Options{
		Addr: mr.Addr(),
	})
}

// ─────────────────────────────────────────────────────────────
// KeyRepository — Redis cache layer tests
// ─────────────────────────────────────────────────────────────

func newTestKeyRepo(t *testing.T) (*KeyRepository, *miniredis.Miniredis) {
	t.Helper()
	mr := newMockRedis(t)
	rc := newRedisClient(mr)
	return NewKeyRepository(nil, rc), mr
}

func TestKeyRepository_CacheKeyID(t *testing.T) {
	repo, _ := newTestKeyRepo(t)
	id := repo.cacheKeyID("key-abc")
	if id != "key:key-abc" {
		t.Errorf("cacheKeyID: want 'key:key-abc', got '%s'", id)
	}
}

func TestKeyRepository_CacheRoundtrip(t *testing.T) {
	repo, mr := newTestKeyRepo(t)
	ctx := context.Background()

	key := &Key{
		ID:        "k-cache-001",
		VehicleID: "v-001",
		UserID:    "u-001",
		KeyType:   "primary",
		Status:    "active",
		CreatedAt: time.Now(),
		ExpiresAt: time.Now().Add(24 * time.Hour),
		Permissions: []string{"lock", "unlock", "engine_start"},
	}

	repo.cacheKey(ctx, key)

	cachedID := repo.cacheKeyID(key.ID)
	val, err := mr.Get(cachedID)
	if err != nil {
		t.Fatalf("Key should be in cache: %v", err)
	}
	if val == "" {
		t.Errorf("Cached value should not be empty")
	}

	cached, err := repo.getCachedKey(ctx, key.ID)
	if err != nil {
		t.Fatalf("getCachedKey: %v", err)
	}
	if cached.ID != key.ID {
		t.Errorf("Cached ID: want %s, got %s", key.ID, cached.ID)
	}
	if cached.Status != "active" {
		t.Errorf("Cached Status: want active, got %s", cached.Status)
	}
	if len(cached.Permissions) != 3 {
		t.Errorf("Cached Permissions: want 3, got %d", len(cached.Permissions))
	}
}

func TestKeyRepository_CacheMiss(t *testing.T) {
	repo, _ := newTestKeyRepo(t)
	ctx := context.Background()

	_, err := repo.getCachedKey(ctx, "nonexistent")
	if err == nil {
		t.Errorf("Expected error for cache miss")
	}
}

func TestKeyRepository_InvalidateCache(t *testing.T) {
	repo, mr := newTestKeyRepo(t)
	ctx := context.Background()

	key := &Key{
		ID:        "k-invalidate-001",
		VehicleID: "v-001",
		UserID:    "u-001",
		KeyType:   "primary",
		Status:    "active",
		CreatedAt: time.Now(),
		ExpiresAt: time.Now().Add(24 * time.Hour),
	}

	repo.cacheKey(ctx, key)

	cachedID := repo.cacheKeyID(key.ID)
	if !mr.Exists(cachedID) {
		t.Errorf("Key should exist in cache before invalidation")
	}

	repo.invalidateCache(ctx, key.ID)

	if mr.Exists(cachedID) {
		t.Errorf("Key should be removed from cache after invalidation")
	}
}

func TestKeyRepository_CacheMultipleAndInvalidate(t *testing.T) {
	repo, _ := newTestKeyRepo(t)
	ctx := context.Background()

	ids := []string{"k-a", "k-b", "k-c"}
	for _, id := range ids {
		repo.cacheKey(ctx, &Key{
			ID:        id,
			VehicleID: "v",
			UserID:    "u",
			Status:    "active",
			CreatedAt: time.Now(),
			ExpiresAt: time.Now().Add(24 * time.Hour),
		})
	}

	for _, id := range ids {
		repo.invalidateCache(ctx, id)
	}

	for _, id := range ids {
		_, err := repo.getCachedKey(ctx, id)
		if err == nil {
			t.Errorf("Key %s should not be in cache after invalidation", id)
		}
	}
}

// ─────────────────────────────────────────────────────────────
// VehicleRepository — Redis cache layer tests
// ─────────────────────────────────────────────────────────────

func newTestVehicleRepo(t *testing.T) (*VehicleRepository, *miniredis.Miniredis) {
	t.Helper()
	mr := newMockRedis(t)
	rc := newRedisClient(mr)
	return NewVehicleRepository(nil, rc), mr
}

func TestVehicleRepository_CacheVehicleID(t *testing.T) {
	repo, _ := newTestVehicleRepo(t)
	id := repo.cacheVehicleID("vehicle-abc")
	if id != "vehicle:vehicle-abc" {
		t.Errorf("cacheVehicleID: want 'vehicle:vehicle-abc', got '%s'", id)
	}
}

func TestVehicleRepository_CacheRoundtrip(t *testing.T) {
	repo, mr := newTestVehicleRepo(t)
	ctx := context.Background()

	now := time.Now()
	vehicle := &Vehicle{
		ID:       "v-cache-001",
		OwnerID:  "owner-001",
		VIN:      "1HGCM82633A123456",
		Brand:    "BYD",
		Model:    "Han EV",
		Year:     2025,
		IsOnline: true,
		CreatedAt: now,
		UpdatedAt: now,
		Features: []string{"ble", "uwb", "nfc"},
	}

	repo.cacheVehicle(ctx, vehicle)

	cachedID := repo.cacheVehicleID(vehicle.ID)
	if !mr.Exists(cachedID) {
		t.Errorf("Vehicle should exist in cache")
	}

	cached, err := repo.getCachedVehicle(ctx, vehicle.ID)
	if err != nil {
		t.Fatalf("getCachedVehicle: %v", err)
	}
	if cached.ID != vehicle.ID {
		t.Errorf("Cached ID mismatch")
	}
	if cached.VIN != "1HGCM82633A123456" {
		t.Errorf("Cached VIN mismatch")
	}
}

func TestVehicleRepository_InvalidateCache(t *testing.T) {
	repo, mr := newTestVehicleRepo(t)
	ctx := context.Background()

	vehicle := &Vehicle{ID: "v-inv-001", OwnerID: "owner-001", VIN: "WDBGA57E8LA000001"}

	repo.cacheVehicle(ctx, vehicle)
	cachedID := repo.cacheVehicleID(vehicle.ID)
	if !mr.Exists(cachedID) {
		t.Errorf("Vehicle should exist in cache before invalidation")
	}

	repo.invalidateCache(ctx, vehicle.ID)
	if mr.Exists(cachedID) {
		t.Errorf("Vehicle should not exist in cache after invalidation")
	}
}

// ─────────────────────────────────────────────────────────────
// EventRepository tests
// ─────────────────────────────────────────────────────────────

func newTestEventRepo(t *testing.T) *EventRepository {
	t.Helper()
	return NewEventRepository(nil)
}

func TestEventRepository_Constructor(t *testing.T) {
	repo := newTestEventRepo(t)
	if repo == nil {
		t.Fatal("NewEventRepository returned nil")
	}
	if repo.db != nil {
		t.Errorf("db should be nil for nil input")
	}
}

func TestEventRepository_NewWithSQLOnly(t *testing.T) {
	// EventRepository only takes sqlx.DB, not redis
	repo := NewEventRepository(nil)
	if repo == nil {
		t.Fatal("NewEventRepository returned nil")
	}
}

// ─────────────────────────────────────────────────────────────
// Error types tests
// ─────────────────────────────────────────────────────────────

func TestSentinelErrors(t *testing.T) {
	if ErrKeyNotFound.Error() != "key not found" {
		t.Errorf("ErrKeyNotFound: got '%s'", ErrKeyNotFound.Error())
	}
	if ErrVehicleNotFound.Error() != "vehicle not found" {
		t.Errorf("ErrVehicleNotFound: got '%s'", ErrVehicleNotFound.Error())
	}
	if ErrEventNotFound.Error() != "event not found" {
		t.Errorf("ErrEventNotFound: got '%s'", ErrEventNotFound.Error())
	}
}

// ─────────────────────────────────────────────────────────────
// Key struct method tests
// ─────────────────────────────────────────────────────────────

func TestKey_HasPermission(t *testing.T) {
	key := &Key{
		ID:          "k-perm-001",
		Permissions: []string{"lock", "unlock", "engine_start"},
	}

	tests := []struct {
		perm string
		want bool
	}{
		{"lock", true},
		{"unlock", true},
		{"engine_start", true},
		{"start", false},
		{"", false},
	}
	for _, tt := range tests {
		got := key.HasPermission(tt.perm)
		if got != tt.want {
			t.Errorf("HasPermission(%q): want %v, got %v", tt.perm, tt.want, got)
		}
	}
}

func TestKey_HasPermission_AllWildcard(t *testing.T) {
	key := &Key{
		ID:          "k-perm-wildcard",
		Permissions: []string{"all"},
	}
	if !key.HasPermission("lock") {
		t.Errorf("Should have permission 'lock' when 'all' is set")
	}
	if !key.HasPermission("nonexistent") {
		t.Errorf("Should have permission 'nonexistent' when 'all' is set")
	}
}

func TestKey_HasPermission_EmptyKey(t *testing.T) {
	key := &Key{ID: "k-perm-empty", Permissions: []string{}}
	if key.HasPermission("anything") {
		t.Errorf("Should not have any permission on empty list")
	}
}

func TestKey_HasPermission_NilSlice(t *testing.T) {
	k := &Key{ID: "k-nil-perm", Permissions: nil}
	if k.HasPermission("anything") {
		t.Errorf("nil Permissions should return false")
	}
}

// ─────────────────────────────────────────────────────────────
// Constructor tests
// ─────────────────────────────────────────────────────────────

func TestNewKeyRepository_WithNil(t *testing.T) {
	repo := NewKeyRepository(nil, nil)
	if repo == nil {
		t.Fatal("NewKeyRepository(nil, nil) returned nil")
	}
	if repo.db != nil {
		t.Errorf("db should be nil")
	}
}

func TestNewVehicleRepository_WithNil(t *testing.T) {
	repo := NewVehicleRepository(nil, nil)
	if repo == nil {
		t.Fatal("NewVehicleRepository(nil, nil) returned nil")
	}
}

func TestNewEventRepository_Nil(t *testing.T) {
	repo := NewEventRepository(nil)
	if repo == nil {
		t.Fatal("NewEventRepository(nil) returned nil")
	}
}
