package repository

import (
	"context"
	"database/sql"
	"encoding/json"
	"fmt"
	"testing"
	"time"

	"github.com/DATA-DOG/go-sqlmock"
	"github.com/alicebob/miniredis/v2"
	"github.com/jmoiron/sqlx"
	"github.com/redis/go-redis/v9"
)

func newSQLMockVehicleRepo(t *testing.T) (*VehicleRepository, sqlmock.Sqlmock, *miniredis.Miniredis) {
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
	repo := NewVehicleRepository(sqlxDB, rc)
	return repo, mock, mr
}

// ── GetByID ──

func TestVehicleRepoSQL_GetByID_CacheHit(t *testing.T) {
	repo, mock, _ := newSQLMockVehicleRepo(t)
	ctx := context.Background()
	v := testVehicle(nil)
	repo.cacheVehicle(ctx, v)

	got, err := repo.GetByID(ctx, v.ID)
	if err != nil {
		t.Fatalf("GetByID failed: %v", err)
	}
	if got.ID != v.ID {
		t.Errorf("ID: want %s, got %s", v.ID, got.ID)
	}
	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("unexpected SQL calls: %v", err)
	}
}

func TestVehicleRepoSQL_GetByID_CacheMiss(t *testing.T) {
	repo, mock, mr := newSQLMockVehicleRepo(t)
	ctx := context.Background()

	vehicleID := "v-db-001"
	now := time.Now()
	featuresJSON, _ := json.Marshal([]string{"ble", "nfc"})

	mock.ExpectQuery(`SELECT \* FROM vehicles WHERE id = \? LIMIT 1`).
		WithArgs(vehicleID).
		WillReturnRows(sqlmock.NewRows([]string{
			"id", "owner_id", "vin", "brand", "model",
			"year", "color", "plate_number", "tcu_id", "protocol",
			"is_online", "last_online", "battery_level", "odometer",
			"latitude", "longitude", "features", "created_at", "updated_at",
		}).AddRow(
			vehicleID, "owner-001", "WDBGA57E8LA000001", "BYD", "Han EV",
			2025, "White", "京A12345", "tcu-001", "CCC",
			true, now, 85, 5000,
			39.9042, 116.4074, featuresJSON, now, now,
		))

	got, err := repo.GetByID(ctx, vehicleID)
	if err != nil {
		t.Fatalf("GetByID failed: %v", err)
	}
	if got.ID != vehicleID {
		t.Errorf("ID: want %s, got %s", vehicleID, got.ID)
	}
	if got.Brand != "BYD" {
		t.Errorf("Brand: want BYD, got %s", got.Brand)
	}
	if len(got.Features) != 2 {
		t.Errorf("Features: want 2, got %d", len(got.Features))
	}

	cachedID := repo.cacheVehicleID(vehicleID)
	if !mr.Exists(cachedID) {
		t.Errorf("expected vehicle to be cached after DB lookup")
	}
	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("sqlmock expectations not met: %v", err)
	}
}

func TestVehicleRepoSQL_GetByID_NotFound(t *testing.T) {
	repo, mock, _ := newSQLMockVehicleRepo(t)
	ctx := context.Background()
	mock.ExpectQuery(`SELECT \* FROM vehicles WHERE id = \? LIMIT 1`).
		WithArgs("nonexistent").
		WillReturnError(sql.ErrNoRows)
	_, err := repo.GetByID(ctx, "nonexistent")
	if err == nil {
		t.Fatal("expected error for nonexistent vehicle")
	}
}

func TestVehicleRepoSQL_GetByID_NilFeatures(t *testing.T) {
	repo, mock, _ := newSQLMockVehicleRepo(t)
	ctx := context.Background()

	vehicleID := "v-nil-features"
	now := time.Now()

	mock.ExpectQuery(`SELECT \* FROM vehicles WHERE id = \? LIMIT 1`).
		WithArgs(vehicleID).
		WillReturnRows(sqlmock.NewRows([]string{
			"id", "owner_id", "vin", "brand", "model",
			"year", "color", "plate_number", "tcu_id", "protocol",
			"is_online", "last_online", "battery_level", "odometer",
			"latitude", "longitude", "features", "created_at", "updated_at",
		}).AddRow(
			vehicleID, "owner-001", "VIN001", "Test", "Model",
			2024, "Blue", "京B00001", "tcu-002", "ICCOA",
			false, nil, 0, 0,
			0.0, 0.0, nil, now, now,
		))

	got, err := repo.GetByID(ctx, vehicleID)
	if err != nil {
		t.Fatalf("GetByID failed: %v", err)
	}
	if len(got.Features) != 0 {
		t.Errorf("expected empty features, got %v", got.Features)
	}
}

// ── GetByVIN ──

func TestVehicleRepoSQL_GetByVIN_Success(t *testing.T) {
	repo, mock, _ := newSQLMockVehicleRepo(t)
	ctx := context.Background()

	vin := "WDBGA57E8LA000001"
	now := time.Now()
	featuresJSON, _ := json.Marshal([]string{"ble"})

	mock.ExpectQuery(`SELECT \* FROM vehicles WHERE vin = \? LIMIT 1`).
		WithArgs(vin).
		WillReturnRows(sqlmock.NewRows([]string{
			"id", "owner_id", "vin", "brand", "model",
			"year", "color", "plate_number", "tcu_id", "protocol",
			"is_online", "last_online", "battery_level", "odometer",
			"latitude", "longitude", "features", "created_at", "updated_at",
		}).AddRow(
			"v-by-vin", "owner-001", vin, "BYD", "Seal",
			2025, "Red", "京C00001", "tcu-003", "CCC",
			false, nil, 100, 0,
			0, 0, featuresJSON, now, now,
		))

	got, err := repo.GetByVIN(ctx, vin)
	if err != nil {
		t.Fatalf("GetByVIN failed: %v", err)
	}
	if got.VIN != vin {
		t.Errorf("VIN: want %s, got %s", vin, got.VIN)
	}
}

func TestVehicleRepoSQL_GetByVIN_NotFound(t *testing.T) {
	repo, mock, _ := newSQLMockVehicleRepo(t)
	ctx := context.Background()
	mock.ExpectQuery(`SELECT \* FROM vehicles WHERE vin = \? LIMIT 1`).
		WithArgs("UNKNOWNVIN").
		WillReturnError(sql.ErrNoRows)
	_, err := repo.GetByVIN(ctx, "UNKNOWNVIN")
	if err == nil {
		t.Fatal("expected error for unknown VIN")
	}
}

// ── GetByTCUID ──

func TestVehicleRepoSQL_GetByTCUID_Success(t *testing.T) {
	repo, mock, _ := newSQLMockVehicleRepo(t)
	ctx := context.Background()

	tcuID := "tcu-001"
	now := time.Now()

	mock.ExpectQuery(`SELECT \* FROM vehicles WHERE tcu_id = \? LIMIT 1`).
		WithArgs(tcuID).
		WillReturnRows(sqlmock.NewRows([]string{
			"id", "owner_id", "vin", "brand", "model",
			"year", "color", "plate_number", "tcu_id", "protocol",
			"is_online", "last_online", "battery_level", "odometer",
			"latitude", "longitude", "features", "created_at", "updated_at",
		}).AddRow(
			"v-tcu", "owner-001", "VINTCU", "Test", "Model",
			2025, "Silver", "京D00001", tcuID, "ICCOA",
			true, now, 50, 10000,
			31.2304, 121.4737, nil, now, now,
		))

	got, err := repo.GetByTCUID(ctx, tcuID)
	if err != nil {
		t.Fatalf("GetByTCUID failed: %v", err)
	}
	if got.TCUID != tcuID {
		t.Errorf("TCUID: want %s, got %s", tcuID, got.TCUID)
	}
}

func TestVehicleRepoSQL_GetByTCUID_NotFound(t *testing.T) {
	repo, mock, _ := newSQLMockVehicleRepo(t)
	ctx := context.Background()
	mock.ExpectQuery(`SELECT \* FROM vehicles WHERE tcu_id = \? LIMIT 1`).
		WithArgs("nonexistent-tcu").
		WillReturnError(sql.ErrNoRows)
	_, err := repo.GetByTCUID(ctx, "nonexistent-tcu")
	if err == nil {
		t.Fatal("expected error for nonexistent TCU")
	}
}

// ── UpdateStatus ──

func TestVehicleRepoSQL_UpdateStatus_GoOnline(t *testing.T) {
	repo, mock, mr := newSQLMockVehicleRepo(t)
	ctx := context.Background()

	mock.ExpectExec(`UPDATE vehicles SET is_online = \?, last_online = \?, updated_at = \? WHERE id = \?`).
		WithArgs(true, sqlmock.AnyArg(), sqlmock.AnyArg(), "v-status-001").
		WillReturnResult(sqlmock.NewResult(0, 1))

	if err := repo.UpdateStatus(ctx, "v-status-001", true); err != nil {
		t.Fatalf("UpdateStatus failed: %v", err)
	}

	cachedID := repo.cacheVehicleID("v-status-001")
	if mr.Exists(cachedID) {
		t.Errorf("expected cache to be invalidated")
	}
	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("sqlmock expectations not met: %v", err)
	}
}

func TestVehicleRepoSQL_UpdateStatus_GoOffline(t *testing.T) {
	repo, mock, _ := newSQLMockVehicleRepo(t)
	ctx := context.Background()

	mock.ExpectExec(`UPDATE vehicles SET is_online = \?, last_online = \?, updated_at = \? WHERE id = \?`).
		WithArgs(false, nil, sqlmock.AnyArg(), "v-status-off").
		WillReturnResult(sqlmock.NewResult(0, 1))

	if err := repo.UpdateStatus(ctx, "v-status-off", false); err != nil {
		t.Fatalf("UpdateStatus(offline) failed: %v", err)
	}
}

func TestVehicleRepoSQL_UpdateStatus_NotFound(t *testing.T) {
	repo, mock, _ := newSQLMockVehicleRepo(t)
	ctx := context.Background()

	mock.ExpectExec(`UPDATE vehicles SET is_online = \?, last_online = \?, updated_at = \? WHERE id = \?`).
		WithArgs(true, sqlmock.AnyArg(), sqlmock.AnyArg(), "nonexistent").
		WillReturnResult(sqlmock.NewResult(0, 0))

	err := repo.UpdateStatus(ctx, "nonexistent", true)
	if err == nil {
		t.Fatal("expected error for not found")
	}
}

// ── UpdateLocation ──

func TestVehicleRepoSQL_UpdateLocation_Success(t *testing.T) {
	repo, mock, _ := newSQLMockVehicleRepo(t)
	ctx := context.Background()

	mock.ExpectExec(`UPDATE vehicles SET latitude = \?, longitude = \?, updated_at = \? WHERE id = \?`).
		WithArgs(39.9042, 116.4074, sqlmock.AnyArg(), "v-loc-001").
		WillReturnResult(sqlmock.NewResult(0, 1))

	if err := repo.UpdateLocation(ctx, "v-loc-001", 39.9042, 116.4074); err != nil {
		t.Fatalf("UpdateLocation failed: %v", err)
	}
}

func TestVehicleRepoSQL_UpdateLocation_DBError(t *testing.T) {
	repo, mock, _ := newSQLMockVehicleRepo(t)
	ctx := context.Background()

	mock.ExpectExec(`UPDATE vehicles SET latitude = \?, longitude = \?, updated_at = \? WHERE id = \?`).
		WithArgs(0.0, 0.0, sqlmock.AnyArg(), "v-error").
		WillReturnError(fmt.Errorf("db unavailable"))

	err := repo.UpdateLocation(ctx, "v-error", 0, 0)
	if err == nil {
		t.Fatal("expected error from UpdateLocation")
	}
}

// ── UpdateTelemetry ──

func TestVehicleRepoSQL_UpdateTelemetry_Success(t *testing.T) {
	repo, mock, _ := newSQLMockVehicleRepo(t)
	ctx := context.Background()

	mock.ExpectExec(`UPDATE vehicles SET battery_level = \?, odometer = \?, updated_at = \? WHERE id = \?`).
		WithArgs(75, 20000, sqlmock.AnyArg(), "v-tlm-001").
		WillReturnResult(sqlmock.NewResult(0, 1))

	if err := repo.UpdateTelemetry(ctx, "v-tlm-001", 75, 20000); err != nil {
		t.Fatalf("UpdateTelemetry failed: %v", err)
	}
}

func TestVehicleRepoSQL_UpdateTelemetry_Error(t *testing.T) {
	repo, mock, _ := newSQLMockVehicleRepo(t)
	ctx := context.Background()

	mock.ExpectExec(`UPDATE vehicles SET battery_level = \?, odometer = \?, updated_at = \? WHERE id = \?`).
		WithArgs(0, 0, sqlmock.AnyArg(), "v-error").
		WillReturnError(fmt.Errorf("query timeout"))

	err := repo.UpdateTelemetry(ctx, "v-error", 0, 0)
	if err == nil {
		t.Fatal("expected error from UpdateTelemetry")
	}
}

// ── ListByOwner ──

func TestVehicleRepoSQL_ListByOwner_Success(t *testing.T) {
	repo, mock, _ := newSQLMockVehicleRepo(t)
	ctx := context.Background()

	ownerID := "owner-001"
	now := time.Now()
	featuresJSON, _ := json.Marshal([]string{"ble", "nfc"})

	mock.ExpectQuery(`SELECT \* FROM vehicles WHERE owner_id = \? ORDER BY created_at DESC`).
		WithArgs(ownerID).
		WillReturnRows(sqlmock.NewRows([]string{
			"id", "owner_id", "vin", "brand", "model",
			"year", "color", "plate_number", "tcu_id", "protocol",
			"is_online", "last_online", "battery_level", "odometer",
			"latitude", "longitude", "features", "created_at", "updated_at",
		}).AddRow(
			"v-1", ownerID, "VIN001", "BYD", "Han",
			2025, "White", "京A00001", "tcu-1", "CCC",
			true, now, 100, 1000, 39.9, 116.4, featuresJSON, now, now,
		).AddRow(
			"v-2", ownerID, "VIN002", "BYD", "Seal",
			2025, "Blue", "京A00002", "tcu-2", "ICCOA",
			false, nil, 80, 500, 31.2, 121.5, nil, now, now,
		))

	vehicles, err := repo.ListByOwner(ctx, ownerID)
	if err != nil {
		t.Fatalf("ListByOwner failed: %v", err)
	}
	if len(vehicles) != 2 {
		t.Fatalf("want 2 vehicles, got %d", len(vehicles))
	}
}

func TestVehicleRepoSQL_ListByOwner_Empty(t *testing.T) {
	repo, mock, _ := newSQLMockVehicleRepo(t)
	ctx := context.Background()

	mock.ExpectQuery(`SELECT \* FROM vehicles WHERE owner_id = \? ORDER BY created_at DESC`).
		WithArgs("owner-empty").
		WillReturnRows(sqlmock.NewRows([]string{
			"id", "owner_id", "vin", "brand", "model",
			"year", "color", "plate_number", "tcu_id", "protocol",
			"is_online", "last_online", "battery_level", "odometer",
			"latitude", "longitude", "features", "created_at", "updated_at",
		}))

	vehicles, err := repo.ListByOwner(ctx, "owner-empty")
	if err != nil {
		t.Fatalf("ListByOwner failed: %v", err)
	}
	if len(vehicles) != 0 {
		t.Errorf("want 0 vehicles, got %d", len(vehicles))
	}
}
