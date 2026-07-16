package repository

import (
	"context"
	"database/sql"
	"encoding/json"
	"fmt"
	"regexp"
	"testing"
	"time"

	"github.com/DATA-DOG/go-sqlmock"
	"github.com/alicebob/miniredis/v2"
	"github.com/jmoiron/sqlx"
	"github.com/redis/go-redis/v9"
)

// ─────────────────────────────────────────────────────────────
// Helper: sqlmock + miniredis test infrastructure
// ─────────────────────────────────────────────────────────────

func newSQLMockKeyRepo(t *testing.T) (*KeyRepository, sqlmock.Sqlmock, *miniredis.Miniredis) {
	t.Helper()

	db, mock, err := sqlmock.New()
	if err != nil {
		t.Fatalf("sqlmock.New: %v", err)
	}
	t.Cleanup(func() { db.Close() })

	sqlxDB := sqlx.NewDb(db, "postgres")

	mr, err := miniredis.Run()
	if err != nil {
		t.Fatalf("miniredis.Run: %v", err)
	}
	t.Cleanup(mr.Close)

	rc := redis.NewClient(&redis.Options{Addr: mr.Addr()})

	repo := NewKeyRepository(sqlxDB, rc)
	return repo, mock, mr
}

// ─────────────────────────────────────────────────────────────
// KeyRepository.Create
// ─────────────────────────────────────────────────────────────

func TestKeyRepoSQL_Create_Success(t *testing.T) {
	repo, mock, mr := newSQLMockKeyRepo(t)
	ctx := context.Background()

	key := testKey(nil)

	permJSON, _ := json.Marshal(key.Permissions)
	metaJSON, _ := json.Marshal(key.Metadata)

	mock.ExpectQuery(regexp.QuoteMeta(`INSERT INTO digital_keys`)).
		WithArgs(key.ID, key.VehicleID, key.UserID, key.KeyType, key.Status,
			permJSON, key.Secret, key.ParentKeyID, key.CreatedAt, key.ExpiresAt, metaJSON).
		WillReturnRows(sqlmock.NewRows([]string{"id"}).AddRow(key.ID))

	if err := repo.Create(ctx, key); err != nil {
		t.Fatalf("Create failed: %v", err)
	}

	// Verify redis cache was set
	cachedID := repo.cacheKeyID(key.ID)
	if !mr.Exists(cachedID) {
		t.Errorf("expected key to be cached after create")
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("sqlmock expectations not met: %v", err)
	}
}

func TestKeyRepoSQL_Create_QueryBuildError(t *testing.T) {
	repo, mock, _ := newSQLMockKeyRepo(t)
	ctx := context.Background()

	key := testKey(nil)
	permJSON, _ := json.Marshal(key.Permissions)
	metaJSON, _ := json.Marshal(key.Metadata)

	mock.ExpectQuery(regexp.QuoteMeta(`INSERT INTO digital_keys`)).
		WithArgs(key.ID, key.VehicleID, key.UserID, key.KeyType, key.Status,
			permJSON, key.Secret, key.ParentKeyID, key.CreatedAt, key.ExpiresAt, metaJSON).
		WillReturnError(fmt.Errorf("duplicate key"))

	err := repo.Create(ctx, key)
	if err == nil {
		t.Fatal("expected error from Create, got nil")
	}
}

// ─────────────────────────────────────────────────────────────
// KeyRepository.GetByID
// ─────────────────────────────────────────────────────────────

func TestKeyRepoSQL_GetByID_CacheHit(t *testing.T) {
	repo, mock, _ := newSQLMockKeyRepo(t)
	ctx := context.Background()

	now := time.Now()
	key := &Key{
		ID:        "k-cached-001",
		VehicleID: "v-001",
		UserID:    "u-001",
		KeyType:   "primary",
		Status:    "active",
		CreatedAt: now,
		ExpiresAt: now.Add(24 * time.Hour),
		Permissions: []string{"lock", "unlock"},
		Metadata: map[string]interface{}{"source": "test"},
	}

	// Warm the cache
	repo.cacheKey(ctx, key)

	// GetByID should hit cache — no SQL expectations needed
	got, err := repo.GetByID(ctx, key.ID)
	if err != nil {
		t.Fatalf("GetByID failed: %v", err)
	}
	if got.ID != key.ID {
		t.Errorf("ID: want %s, got %s", key.ID, got.ID)
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("unexpected SQL expectations: %v", err)
	}
}

func TestKeyRepoSQL_GetByID_CacheMiss(t *testing.T) {
	repo, mock, mr := newSQLMockKeyRepo(t)
	ctx := context.Background()

	now := time.Now()
	keyID := "k-db-001"
	permJSON, _ := json.Marshal([]string{"lock", "unlock"})
	metaJSON, _ := json.Marshal(map[string]interface{}{"source": "test"})

	mock.ExpectQuery(`SELECT \* FROM digital_keys WHERE id = \? LIMIT 1`).
		WithArgs(keyID).
		WillReturnRows(sqlmock.NewRows([]string{
			"id", "vehicle_id", "user_id", "key_type", "status",
			"permissions", "secret", "parent_key_id", "created_at",
			"activated_at", "expires_at", "revoked_at", "revoke_reason", "metadata",
		}).AddRow(
			keyID, "v-001", "u-001", "primary", "active",
			permJSON, "secret-hash", nil, now,
			nil, now.Add(24*time.Hour), nil, "",
			metaJSON,
		))

	got, err := repo.GetByID(ctx, keyID)
	if err != nil {
		t.Fatalf("GetByID failed: %v", err)
	}
	if got.ID != keyID {
		t.Errorf("ID: want %s, got %s", keyID, got.ID)
	}
	if got.Status != "active" {
		t.Errorf("Status: want active, got %s", got.Status)
	}

	// Verify it was cached
	cachedID := repo.cacheKeyID(keyID)
	if !mr.Exists(cachedID) {
		t.Errorf("expected key to be cached after DB lookup")
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("sqlmock expectations not met: %v", err)
	}
}

func TestKeyRepoSQL_GetByID_NotFound(t *testing.T) {
	repo, mock, _ := newSQLMockKeyRepo(t)
	ctx := context.Background()

	mock.ExpectQuery(`SELECT \* FROM digital_keys WHERE id = \? LIMIT 1`).
		WithArgs("nonexistent").
		WillReturnError(sql.ErrNoRows)

	_, err := repo.GetByID(ctx, "nonexistent")
	if err == nil {
		t.Fatal("expected error for nonexistent key")
	}
}

// ─────────────────────────────────────────────────────────────
// KeyRepository.Update
// ─────────────────────────────────────────────────────────────

func TestKeyRepoSQL_Update_Success(t *testing.T) {
	repo, mock, mr := newSQLMockKeyRepo(t)
	ctx := context.Background()

	now := time.Now()
	key := testKey(func(k *Key) {
		k.Status = "active"
		k.ActivatedAt = &now
		k.Permissions = []string{"lock", "unlock", "start"}
	})

	permJSON, _ := json.Marshal(key.Permissions)
	metaJSON, _ := json.Marshal(key.Metadata)

	mock.ExpectExec(`UPDATE digital_keys`).
		WithArgs(key.Status, permJSON, key.ActivatedAt, key.RevokedAt, key.RevokeReason, metaJSON, key.ID).
		WillReturnResult(sqlmock.NewResult(1, 1))

	if err := repo.Update(ctx, key); err != nil {
		t.Fatalf("Update failed: %v", err)
	}

	// Verify cache invalidated
	cachedID := repo.cacheKeyID(key.ID)
	if mr.Exists(cachedID) {
		t.Errorf("expected cache to be invalidated after update")
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("sqlmock expectations not met: %v", err)
	}
}

func TestKeyRepoSQL_Update_NotFound(t *testing.T) {
	repo, mock, _ := newSQLMockKeyRepo(t)
	ctx := context.Background()

	now := time.Now()
	key := testKey(func(k *Key) {
		k.Status = "active"
		k.ActivatedAt = &now
	})
	permJSON, _ := json.Marshal(key.Permissions)
	metaJSON, _ := json.Marshal(key.Metadata)

	mock.ExpectExec(`UPDATE digital_keys`).
		WithArgs(key.Status, permJSON, key.ActivatedAt, key.RevokedAt, key.RevokeReason, metaJSON, key.ID).
		WillReturnResult(sqlmock.NewResult(0, 0))

	err := repo.Update(ctx, key)
	if err == nil {
		t.Fatal("expected error for no rows affected")
	}
}

// ─────────────────────────────────────────────────────────────
// KeyRepository.Delete
// ─────────────────────────────────────────────────────────────

func TestKeyRepoSQL_Delete_Success(t *testing.T) {
	repo, mock, mr := newSQLMockKeyRepo(t)
	ctx := context.Background()

	keyID := "k-delete-001"

	mock.ExpectExec(`DELETE FROM digital_keys WHERE id = \?`).
		WithArgs(keyID).
		WillReturnResult(sqlmock.NewResult(0, 1))

	if err := repo.Delete(ctx, keyID); err != nil {
		t.Fatalf("Delete failed: %v", err)
	}

	// Verify cache invalidated
	cachedID := repo.cacheKeyID(keyID)
	if mr.Exists(cachedID) {
		t.Errorf("expected cache to be invalidated after delete")
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("sqlmock expectations not met: %v", err)
	}
}

func TestKeyRepoSQL_Delete_DBError(t *testing.T) {
	repo, mock, _ := newSQLMockKeyRepo(t)
	ctx := context.Background()

	mock.ExpectExec(`DELETE FROM digital_keys WHERE id = \?`).
		WithArgs("k-error").
		WillReturnError(fmt.Errorf("constraint violation"))

	err := repo.Delete(ctx, "k-error")
	if err == nil {
		t.Fatal("expected error from Delete")
	}
}

// ─────────────────────────────────────────────────────────────
// KeyRepository.ListByUser
// ─────────────────────────────────────────────────────────────

func TestKeyRepoSQL_ListByUser_Success(t *testing.T) {
	repo, mock, _ := newSQLMockKeyRepo(t)
	ctx := context.Background()

	userID := "u-001"
	now := time.Now()

	permJSON, _ := json.Marshal([]string{"lock", "unlock"})
	metaJSON, _ := json.Marshal(map[string]interface{}{"source": "test"})

	mock.ExpectQuery(`SELECT \* FROM digital_keys WHERE user_id = \? ORDER BY created_at DESC LIMIT 10 OFFSET 0`).
		WithArgs(userID).
		WillReturnRows(sqlmock.NewRows([]string{
			"id", "vehicle_id", "user_id", "key_type", "status",
			"permissions", "secret", "parent_key_id", "created_at",
			"activated_at", "expires_at", "revoked_at", "revoke_reason", "metadata",
		}).AddRow(
			"k-001", "v-001", userID, "primary", "active",
			permJSON, "secret-1", nil, now,
			nil, now.Add(24*time.Hour), nil, "",
			metaJSON,
		).AddRow(
			"k-002", "v-002", userID, "friend", "pending",
			permJSON, "secret-2", nil, now.Add(-1*time.Hour),
			nil, now.Add(48*time.Hour), nil, "",
			metaJSON,
		))

	keys, err := repo.ListByUser(ctx, userID, 10, 0)
	if err != nil {
		t.Fatalf("ListByUser failed: %v", err)
	}
	if len(keys) != 2 {
		t.Fatalf("want 2 keys, got %d", len(keys))
	}
	if keys[0].ID != "k-001" {
		t.Errorf("keys[0].ID: want k-001, got %s", keys[0].ID)
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("sqlmock expectations not met: %v", err)
	}
}

func TestKeyRepoSQL_ListByUser_Empty(t *testing.T) {
	repo, mock, _ := newSQLMockKeyRepo(t)
	ctx := context.Background()

	mock.ExpectQuery(`SELECT \* FROM digital_keys WHERE user_id = \? ORDER BY created_at DESC LIMIT 10 OFFSET 0`).
		WithArgs("u-empty").
		WillReturnRows(sqlmock.NewRows([]string{
			"id", "vehicle_id", "user_id", "key_type", "status",
			"permissions", "secret", "parent_key_id", "created_at",
			"activated_at", "expires_at", "revoked_at", "revoke_reason", "metadata",
		}))

	keys, err := repo.ListByUser(ctx, "u-empty", 10, 0)
	if err != nil {
		t.Fatalf("ListByUser failed: %v", err)
	}
	if len(keys) != 0 {
		t.Errorf("want 0 keys, got %d", len(keys))
	}
}

// ─────────────────────────────────────────────────────────────
// KeyRepository.ListByVehicle
// ─────────────────────────────────────────────────────────────

func TestKeyRepoSQL_ListByVehicle_Success(t *testing.T) {
	repo, mock, _ := newSQLMockKeyRepo(t)
	ctx := context.Background()

	vehicleID := "v-001"
	now := time.Now()

	permJSON, _ := json.Marshal([]string{"lock"})
	metaJSON, _ := json.Marshal(nil)

	mock.ExpectQuery(`SELECT \* FROM digital_keys WHERE vehicle_id = \? ORDER BY created_at DESC LIMIT 20 OFFSET 0`).
		WithArgs(vehicleID).
		WillReturnRows(sqlmock.NewRows([]string{
			"id", "vehicle_id", "user_id", "key_type", "status",
			"permissions", "secret", "parent_key_id", "created_at",
			"activated_at", "expires_at", "revoked_at", "revoke_reason", "metadata",
		}).AddRow(
			"k-001", vehicleID, "u-001", "primary", "active",
			permJSON, "secret-1", nil, now,
			nil, now.Add(24*time.Hour), nil, "",
			metaJSON,
		))

	keys, err := repo.ListByVehicle(ctx, vehicleID, 20, 0)
	if err != nil {
		t.Fatalf("ListByVehicle failed: %v", err)
	}
	if len(keys) != 1 {
		t.Fatalf("want 1 key, got %d", len(keys))
	}
	if keys[0].VehicleID != vehicleID {
		t.Errorf("VehicleID: want %s, got %s", vehicleID, keys[0].VehicleID)
	}
}

// ─────────────────────────────────────────────────────────────
// KeyRepository.ListActiveByVehicle
// ─────────────────────────────────────────────────────────────

func TestKeyRepoSQL_ListActiveByVehicle_Success(t *testing.T) {
	repo, mock, _ := newSQLMockKeyRepo(t)
	ctx := context.Background()

	vehicleID := "v-001"
	now := time.Now()
	permJSON, _ := json.Marshal([]string{"lock", "unlock"})
	metaJSON, _ := json.Marshal(nil)

	mock.ExpectQuery(`SELECT \* FROM digital_keys WHERE vehicle_id = \? AND status = \? AND expires_at > \? ORDER BY created_at DESC`).
		WithArgs(vehicleID, "active", sqlmock.AnyArg()).
		WillReturnRows(sqlmock.NewRows([]string{
			"id", "vehicle_id", "user_id", "key_type", "status",
			"permissions", "secret", "parent_key_id", "created_at",
			"activated_at", "expires_at", "revoked_at", "revoke_reason", "metadata",
		}).AddRow(
			"k-active-1", vehicleID, "u-001", "primary", "active",
			permJSON, "secret-1", nil, now.Add(-1*time.Hour),
			&now, now.Add(23*time.Hour), nil, "",
			metaJSON,
		).AddRow(
			"k-active-2", vehicleID, "u-002", "friend", "active",
			permJSON, "secret-2", nil, now.Add(-2*time.Hour),
			&now, now.Add(22*time.Hour), nil, "",
			metaJSON,
		))

	keys, err := repo.ListActiveByVehicle(ctx, vehicleID)
	if err != nil {
		t.Fatalf("ListActiveByVehicle failed: %v", err)
	}
	if len(keys) != 2 {
		t.Fatalf("want 2 keys, got %d", len(keys))
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("sqlmock expectations not met: %v", err)
	}
}

func TestKeyRepoSQL_ListActiveByVehicle_Empty(t *testing.T) {
	repo, mock, _ := newSQLMockKeyRepo(t)
	ctx := context.Background()

	mock.ExpectQuery(`SELECT \* FROM digital_keys WHERE vehicle_id = \? AND status = \? AND expires_at > \? ORDER BY created_at DESC`).
		WithArgs("v-empty", "active", sqlmock.AnyArg()).
		WillReturnRows(sqlmock.NewRows([]string{
			"id", "vehicle_id", "user_id", "key_type", "status",
			"permissions", "secret", "parent_key_id", "created_at",
			"activated_at", "expires_at", "revoked_at", "revoke_reason", "metadata",
		}))

	keys, err := repo.ListActiveByVehicle(ctx, "v-empty")
	if err != nil {
		t.Fatalf("ListActiveByVehicle failed: %v", err)
	}
	if len(keys) != 0 {
		t.Errorf("want 0 keys, got %d", len(keys))
	}
}
