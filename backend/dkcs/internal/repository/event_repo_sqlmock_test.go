package repository

import (
	"context"
	"database/sql"
	"encoding/json"
	"fmt"
	"testing"
	"time"

	"github.com/DATA-DOG/go-sqlmock"
	"github.com/jmoiron/sqlx"
)

func newSQLMockEventRepo(t *testing.T) (*EventRepository, sqlmock.Sqlmock) {
	t.Helper()
	db, mock, err := sqlmock.New()
	if err != nil {
		t.Fatalf("sqlmock.New: %v", err)
	}
	t.Cleanup(func() { db.Close() })
	sqlxDB := sqlx.NewDb(db, "postgres")
	repo := NewEventRepository(sqlxDB)
	return repo, mock
}

// ── Create ──

func TestEventRepoSQL_Create_Success(t *testing.T) {
	repo, mock := newSQLMockEventRepo(t)
	ctx := context.Background()

	now := time.Now()
	e := &Event{
		ID: "evt-001", Type: EventTypeKeyCreated,
		VehicleID: "v-001", UserID: "u-001",
		Data: map[string]interface{}{"source": "test"}, CreatedAt: now,
	}
	dataJSON, _ := json.Marshal(e.Data)

	mock.ExpectQuery(`INSERT INTO events`).
		WithArgs(e.ID, e.Type, e.VehicleID, e.UserID, e.KeyID, dataJSON, e.CreatedAt).
		WillReturnRows(sqlmock.NewRows([]string{"id"}).AddRow(e.ID))

	if err := repo.Create(ctx, e); err != nil {
		t.Fatalf("Create failed: %v", err)
	}
	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("sqlmock expectations not met: %v", err)
	}
}

func TestEventRepoSQL_Create_WithKeyID(t *testing.T) {
	repo, mock := newSQLMockEventRepo(t)
	ctx := context.Background()

	keyID := "key-001"
	now := time.Now()
	e := &Event{
		ID: "evt-key", Type: EventTypeKeyRevoked,
		VehicleID: "v-001", UserID: "u-001",
		KeyID: &keyID,
		Data:  map[string]interface{}{"reason": "stolen"}, CreatedAt: now,
	}
	dataJSON, _ := json.Marshal(e.Data)

	mock.ExpectQuery(`INSERT INTO events`).
		WithArgs(e.ID, e.Type, e.VehicleID, e.UserID, e.KeyID, dataJSON, e.CreatedAt).
		WillReturnRows(sqlmock.NewRows([]string{"id"}).AddRow(e.ID))

	if err := repo.Create(ctx, e); err != nil {
		t.Fatalf("Create failed: %v", err)
	}
}

func TestEventRepoSQL_Create_DBError(t *testing.T) {
	repo, mock := newSQLMockEventRepo(t)
	ctx := context.Background()

	now := time.Now()
	e := &Event{
		ID: "evt-error", Type: EventTypeKeyCreated,
		VehicleID: "v-001", UserID: "u-001",
		Data: nil, CreatedAt: now,
	}
	dataJSON, _ := json.Marshal(e.Data)

	mock.ExpectQuery(`INSERT INTO events`).
		WithArgs(e.ID, e.Type, e.VehicleID, e.UserID, e.KeyID, dataJSON, e.CreatedAt).
		WillReturnError(fmt.Errorf("constraint violation"))

	err := repo.Create(ctx, e)
	if err == nil {
		t.Fatal("expected error from Create, got nil")
	}
}

// ── GetByID ──

func TestEventRepoSQL_GetByID_Success(t *testing.T) {
	repo, mock := newSQLMockEventRepo(t)
	ctx := context.Background()

	now := time.Now()
	eventID := "evt-get-001"
	dataJSON, _ := json.Marshal(map[string]interface{}{"source": "test"})

	mock.ExpectQuery(`SELECT \* FROM events WHERE id = \? LIMIT 1`).
		WithArgs(eventID).
		WillReturnRows(sqlmock.NewRows([]string{
			"id", "type", "vehicle_id", "user_id", "key_id", "data", "created_at",
		}).AddRow(eventID, EventTypeKeyActivated, "v-001", "u-001", nil, dataJSON, now))

	got, err := repo.GetByID(ctx, eventID)
	if err != nil {
		t.Fatalf("GetByID failed: %v", err)
	}
	if got.ID != eventID {
		t.Errorf("ID: want %s, got %s", eventID, got.ID)
	}
	if got.Type != EventTypeKeyActivated {
		t.Errorf("Type: want %s, got %s", EventTypeKeyActivated, got.Type)
	}
}

func TestEventRepoSQL_GetByID_NotFound(t *testing.T) {
	repo, mock := newSQLMockEventRepo(t)
	ctx := context.Background()

	mock.ExpectQuery(`SELECT \* FROM events WHERE id = \? LIMIT 1`).
		WithArgs("nonexistent").
		WillReturnError(sql.ErrNoRows)

	_, err := repo.GetByID(ctx, "nonexistent")
	if err == nil {
		t.Fatal("expected error for nonexistent event")
	}
}

// ── ListByVehicle ──

func TestEventRepoSQL_ListByVehicle_Success(t *testing.T) {
	repo, mock := newSQLMockEventRepo(t)
	ctx := context.Background()

	now := time.Now()
	dataJSON, _ := json.Marshal(map[string]interface{}{})

	mock.ExpectQuery(`SELECT \* FROM events WHERE vehicle_id = \? ORDER BY created_at DESC LIMIT 10 OFFSET 0`).
		WithArgs("v-001").
		WillReturnRows(sqlmock.NewRows([]string{
			"id", "type", "vehicle_id", "user_id", "key_id", "data", "created_at",
		}).AddRow("evt-1", EventTypeCommandSent, "v-001", "u-001", nil, dataJSON, now).
		AddRow("evt-2", EventTypeVehicleOnline, "v-001", "u-001", nil, dataJSON, now.Add(-1*time.Hour)))

	events, err := repo.ListByVehicle(ctx, "v-001", 10, 0)
	if err != nil {
		t.Fatalf("ListByVehicle failed: %v", err)
	}
	if len(events) != 2 {
		t.Fatalf("want 2 events, got %d", len(events))
	}
}

func TestEventRepoSQL_ListByVehicle_Empty(t *testing.T) {
	repo, mock := newSQLMockEventRepo(t)
	ctx := context.Background()

	mock.ExpectQuery(`SELECT \* FROM events WHERE vehicle_id = \? ORDER BY created_at DESC LIMIT 10 OFFSET 0`).
		WithArgs("v-empty").
		WillReturnRows(sqlmock.NewRows([]string{
			"id", "type", "vehicle_id", "user_id", "key_id", "data", "created_at",
		}))

	events, err := repo.ListByVehicle(ctx, "v-empty", 10, 0)
	if err != nil {
		t.Fatalf("ListByVehicle failed: %v", err)
	}
	if len(events) != 0 {
		t.Errorf("want 0 events, got %d", len(events))
	}
}

// ── ListByUser ──

func TestEventRepoSQL_ListByUser_Success(t *testing.T) {
	repo, mock := newSQLMockEventRepo(t)
	ctx := context.Background()

	now := time.Now()
	dataJSON, _ := json.Marshal(map[string]interface{}{})

	mock.ExpectQuery(`SELECT \* FROM events WHERE user_id = \? ORDER BY created_at DESC LIMIT 5 OFFSET 0`).
		WithArgs("u-001").
		WillReturnRows(sqlmock.NewRows([]string{
			"id", "type", "vehicle_id", "user_id", "key_id", "data", "created_at",
		}).AddRow("evt-u1", EventTypeKeyCreated, "v-001", "u-001", nil, dataJSON, now))

	events, err := repo.ListByUser(ctx, "u-001", 5, 0)
	if err != nil {
		t.Fatalf("ListByUser failed: %v", err)
	}
	if len(events) != 1 {
		t.Fatalf("want 1 event, got %d", len(events))
	}
}

// ── ListByKey ──

func TestEventRepoSQL_ListByKey_Success(t *testing.T) {
	repo, mock := newSQLMockEventRepo(t)
	ctx := context.Background()

	keyIDStr := "key-001"
	now := time.Now()
	dataJSON, _ := json.Marshal(map[string]interface{}{})

	mock.ExpectQuery(`SELECT \* FROM events WHERE key_id = \? ORDER BY created_at DESC LIMIT 20 OFFSET 0`).
		WithArgs(keyIDStr).
		WillReturnRows(sqlmock.NewRows([]string{
			"id", "type", "vehicle_id", "user_id", "key_id", "data", "created_at",
		}).AddRow("evt-k1", EventTypeKeyActivated, "v-001", "u-001", &keyIDStr, dataJSON, now).
		AddRow("evt-k2", EventTypeKeyRevoked, "v-001", "u-001", &keyIDStr, dataJSON, now.Add(-30*time.Minute)))

	events, err := repo.ListByKey(ctx, keyIDStr, 20, 0)
	if err != nil {
		t.Fatalf("ListByKey failed: %v", err)
	}
	if len(events) != 2 {
		t.Fatalf("want 2 events, got %d", len(events))
	}
}

func TestEventRepoSQL_ListByKey_Empty(t *testing.T) {
	repo, mock := newSQLMockEventRepo(t)
	ctx := context.Background()

	mock.ExpectQuery(`SELECT \* FROM events WHERE key_id = \? ORDER BY created_at DESC LIMIT 10 OFFSET 0`).
		WithArgs("key-nonexistent").
		WillReturnRows(sqlmock.NewRows([]string{
			"id", "type", "vehicle_id", "user_id", "key_id", "data", "created_at",
		}))

	events, err := repo.ListByKey(ctx, "key-nonexistent", 10, 0)
	if err != nil {
		t.Fatalf("ListByKey failed: %v", err)
	}
	if len(events) != 0 {
		t.Errorf("want 0 events, got %d", len(events))
	}
}

// ── GetStats ──

func TestEventRepoSQL_GetStats_Success(t *testing.T) {
	repo, mock := newSQLMockEventRepo(t)
	ctx := context.Background()

	mock.ExpectQuery(`SELECT type, COUNT\(\*\) as count FROM events WHERE vehicle_id = \? AND created_at >= \? AND created_at <= \? GROUP BY type`).
		WithArgs("v-001", sqlmock.AnyArg(), sqlmock.AnyArg()).
		WillReturnRows(sqlmock.NewRows([]string{"type", "count"}).
			AddRow(EventTypeKeyCreated, int64(5)).
			AddRow(EventTypeCommandSent, int64(20)).
			AddRow(EventTypeVehicleOnline, int64(3)).
			AddRow(EventTypeVehicleOffline, int64(2)))

	stats, err := repo.GetStats(ctx, "v-001", 1700000000, 1700086400)
	if err != nil {
		t.Fatalf("GetStats failed: %v", err)
	}
	if len(stats) != 4 {
		t.Fatalf("want 4 stat entries, got %d", len(stats))
	}
	if stats[EventTypeCommandSent] != 20 {
		t.Errorf("%s: want 20, got %d", EventTypeCommandSent, stats[EventTypeCommandSent])
	}
}

func TestEventRepoSQL_GetStats_Empty(t *testing.T) {
	repo, mock := newSQLMockEventRepo(t)
	ctx := context.Background()

	mock.ExpectQuery(`SELECT type, COUNT\(\*\) as count FROM events WHERE vehicle_id = \? AND created_at >= \? AND created_at <= \? GROUP BY type`).
		WithArgs("v-empty", sqlmock.AnyArg(), sqlmock.AnyArg()).
		WillReturnRows(sqlmock.NewRows([]string{"type", "count"}))

	stats, err := repo.GetStats(ctx, "v-empty", 0, 10000000000)
	if err != nil {
		t.Fatalf("GetStats failed: %v", err)
	}
	if len(stats) != 0 {
		t.Errorf("want 0 stats, got %d", len(stats))
	}
}

func TestEventRepoSQL_GetStats_DBError(t *testing.T) {
	repo, mock := newSQLMockEventRepo(t)
	ctx := context.Background()

	mock.ExpectQuery(`SELECT type, COUNT\(\*\) as count FROM events WHERE vehicle_id = \? AND created_at >= \? AND created_at <= \? GROUP BY type`).
		WithArgs("v-error", sqlmock.AnyArg(), sqlmock.AnyArg()).
		WillReturnError(fmt.Errorf("query failed"))

	_, err := repo.GetStats(ctx, "v-error", 0, 10000000000)
	if err == nil {
		t.Fatal("expected error from GetStats")
	}
}
