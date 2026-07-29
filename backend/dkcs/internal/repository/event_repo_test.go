package repository

import (
	"context"
	"encoding/json"
	"testing"
	"time"

	"github.com/DATA-DOG/go-sqlmock"
	"github.com/jmoiron/sqlx"
)

// ─────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────

func newEventRepo(t *testing.T) (*EventRepository, sqlmock.Sqlmock) {
	t.Helper()
	mockDB, mock, err := sqlmock.New()
	if err != nil {
		t.Fatalf("failed to create sqlmock: %v", err)
	}
	db := sqlx.NewDb(mockDB, "postgres")
	repo := NewEventRepository(db)
	return repo, mock
}

func testEvent() *Event {
	now := time.Now().Truncate(time.Millisecond)
	return &Event{
		ID:        "evt-001",
		Type:      EventTypeKeyCreated,
		VehicleID: "vehicle-001",
		UserID:    "user-001",
		KeyID:     strPtr("key-001"),
		Data:      map[string]interface{}{"source": "test"},
		CreatedAt: now,
	}
}

func strPtr(s string) *string { return &s }

func expectEventInsert(_ sqlmock.Sqlmock, _ *Event) *sqlmock.Rows {
	return sqlmock.NewRows([]string{"id"}).AddRow("mock-id")
}

// ─────────────────────────────────────────────────────────────
// Create Tests
// ─────────────────────────────────────────────────────────────

func TestEventRepo_Create_Success(t *testing.T) {
	repo, mock := newEventRepo(t)
	ctx := context.Background()
	event := testEvent()

	mock.ExpectQuery(
		`INSERT INTO events \(id,type,vehicle_id,user_id,key_id,data,created_at\) VALUES \(\?,\?,\?,\?,\?,\?,\?\) RETURNING id`,
	).WithArgs(
		event.ID, event.Type, event.VehicleID, event.UserID, event.KeyID, sqlmock.AnyArg(), event.CreatedAt,
	).WillReturnRows(sqlmock.NewRows([]string{"id"}).AddRow(event.ID))

	err := repo.Create(ctx, event)
	if err != nil {
		t.Fatalf("Create failed: %v", err)
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("expectations not met: %v", err)
	}
}

func TestEventRepo_Create_DBError(t *testing.T) {
	repo, mock := newEventRepo(t)
	ctx := context.Background()
	event := testEvent()

	mock.ExpectQuery(
		`INSERT INTO events \(id,type,vehicle_id,user_id,key_id,data,created_at\) VALUES \(\?,\?,\?,\?,\?,\?,\?\) RETURNING id`,
	).WithArgs(
		sqlmock.AnyArg(), sqlmock.AnyArg(), sqlmock.AnyArg(), sqlmock.AnyArg(), sqlmock.AnyArg(), sqlmock.AnyArg(), sqlmock.AnyArg(),
	).WillReturnError(sqlmock.ErrCancelled)

	err := repo.Create(ctx, event)
	if err == nil {
		t.Fatal("expected error, got nil")
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("expectations not met: %v", err)
	}
}

func TestEventRepo_Create_NilKeyID(t *testing.T) {
	repo, mock := newEventRepo(t)
	ctx := context.Background()
	event := testEvent()
	event.KeyID = nil

	mock.ExpectQuery(
		`INSERT INTO events \(id,type,vehicle_id,user_id,key_id,data,created_at\) VALUES \(\?,\?,\?,\?,\?,\?,\?\) RETURNING id`,
	).WithArgs(
		event.ID, event.Type, event.VehicleID, event.UserID, nil, sqlmock.AnyArg(), event.CreatedAt,
	).WillReturnRows(sqlmock.NewRows([]string{"id"}).AddRow(event.ID))

	err := repo.Create(ctx, event)
	if err != nil {
		t.Fatalf("Create failed: %v", err)
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("expectations not met: %v", err)
	}
}

// ─────────────────────────────────────────────────────────────
// GetByID Tests
// ─────────────────────────────────────────────────────────────

func TestEventRepo_GetByID_Success(t *testing.T) {
	repo, mock := newEventRepo(t)
	ctx := context.Background()
	event := testEvent()

	dataJSON, _ := json.Marshal(event.Data)
	columns := []string{"id", "type", "vehicle_id", "user_id", "key_id", "data", "created_at"}

	mock.ExpectQuery(`SELECT \* FROM events WHERE id = \? LIMIT 1`).
		WithArgs(event.ID).
		WillReturnRows(sqlmock.NewRows(columns).AddRow(
			event.ID, event.Type, event.VehicleID, event.UserID, event.KeyID, dataJSON, event.CreatedAt,
		))

	got, err := repo.GetByID(ctx, event.ID)
	if err != nil {
		t.Fatalf("GetByID failed: %v", err)
	}
	if got.ID != event.ID {
		t.Errorf("ID: want %q, got %q", event.ID, got.ID)
	}
	if got.Type != event.Type {
		t.Errorf("Type: want %q, got %q", event.Type, got.Type)
	}
	if got.KeyID == nil || *got.KeyID != *event.KeyID {
		t.Errorf("KeyID mismatch")
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("expectations not met: %v", err)
	}
}

func TestEventRepo_GetByID_NotFound(t *testing.T) {
	repo, mock := newEventRepo(t)
	ctx := context.Background()

	mock.ExpectQuery(`SELECT \* FROM events WHERE id = \? LIMIT 1`).
		WithArgs("nonexistent").
		WillReturnError(sqlmock.ErrCancelled)

	_, err := repo.GetByID(ctx, "nonexistent")
	if err == nil {
		t.Fatal("expected error, got nil")
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("expectations not met: %v", err)
	}
}

func TestEventRepo_GetByID_NoRows(t *testing.T) {
	repo, mock := newEventRepo(t)
	ctx := context.Background()

	mock.ExpectQuery(`SELECT \* FROM events WHERE id = \? LIMIT 1`).
		WithArgs("nonexistent").
		WillReturnRows(sqlmock.NewRows([]string{"id", "type", "vehicle_id", "user_id", "key_id", "data", "created_at"}))

	_, err := repo.GetByID(ctx, "nonexistent")
	if err == nil {
		t.Fatal("expected 'event not found' error")
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("expectations not met: %v", err)
	}
}

// ─────────────────────────────────────────────────────────────
// ListByVehicle Tests
// ─────────────────────────────────────────────────────────────

func TestEventRepo_ListByVehicle_Success(t *testing.T) {
	repo, mock := newEventRepo(t)
	ctx := context.Background()

	now := time.Now().Truncate(time.Millisecond)
	events := []*Event{
		{ID: "e1", Type: "key_created", VehicleID: "v1", UserID: "u1", KeyID: strPtr("k1"), Data: map[string]interface{}{"a": "1"}, CreatedAt: now.Add(-1 * time.Hour)},
		{ID: "e2", Type: "command_sent", VehicleID: "v1", UserID: "u1", KeyID: nil, Data: nil, CreatedAt: now},
	}

	columns := []string{"id", "type", "vehicle_id", "user_id", "key_id", "data", "created_at"}
	rows := sqlmock.NewRows(columns)
	for _, e := range events {
		dataJSON, _ := json.Marshal(e.Data)
		rows.AddRow(e.ID, e.Type, e.VehicleID, e.UserID, e.KeyID, dataJSON, e.CreatedAt)
	}

	mock.ExpectQuery(`SELECT \* FROM events WHERE vehicle_id = \? ORDER BY created_at DESC LIMIT 10 OFFSET 0`).
		WithArgs("v1").
		WillReturnRows(rows)

	result, err := repo.ListByVehicle(ctx, "v1", 10, 0)
	if err != nil {
		t.Fatalf("ListByVehicle failed: %v", err)
	}
	if len(result) != 2 {
		t.Fatalf("want 2 events, got %d", len(result))
	}
	if result[0].ID != "e1" {
		t.Errorf("first event: want e1, got %s", result[0].ID)
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("expectations not met: %v", err)
	}
}

func TestEventRepo_ListByVehicle_Empty(t *testing.T) {
	repo, mock := newEventRepo(t)
	ctx := context.Background()

	mock.ExpectQuery(`SELECT \* FROM events WHERE vehicle_id = \? ORDER BY created_at DESC LIMIT 10 OFFSET 0`).
		WithArgs("v-empty").
		WillReturnRows(sqlmock.NewRows([]string{"id", "type", "vehicle_id", "user_id", "key_id", "data", "created_at"}))

	result, err := repo.ListByVehicle(ctx, "v-empty", 10, 0)
	if err != nil {
		t.Fatalf("ListByVehicle failed: %v", err)
	}
	if len(result) != 0 {
		t.Errorf("want 0 events, got %d", len(result))
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("expectations not met: %v", err)
	}
}

func TestEventRepo_ListByVehicle_DBError(t *testing.T) {
	repo, mock := newEventRepo(t)
	ctx := context.Background()

	mock.ExpectQuery(`SELECT \* FROM events WHERE vehicle_id = \? ORDER BY created_at DESC LIMIT 10 OFFSET 0`).
		WithArgs("v-err").
		WillReturnError(sqlmock.ErrCancelled)

	_, err := repo.ListByVehicle(ctx, "v-err", 10, 0)
	if err == nil {
		t.Fatal("expected error, got nil")
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("expectations not met: %v", err)
	}
}

// ─────────────────────────────────────────────────────────────
// ListByUser Tests
// ─────────────────────────────────────────────────────────────

func TestEventRepo_ListByUser_Success(t *testing.T) {
	repo, mock := newEventRepo(t)
	ctx := context.Background()

	now := time.Now().Truncate(time.Millisecond)
	dataJSON, _ := json.Marshal(nil)

	columns := []string{"id", "type", "vehicle_id", "user_id", "key_id", "data", "created_at"}
	mock.ExpectQuery(`SELECT \* FROM events WHERE user_id = \? ORDER BY created_at DESC LIMIT 5 OFFSET 0`).
		WithArgs("u1").
		WillReturnRows(sqlmock.NewRows(columns).AddRow(
			"e1", "key_created", "v1", "u1", strPtr("k1"), dataJSON, now,
		))

	result, err := repo.ListByUser(ctx, "u1", 5, 0)
	if err != nil {
		t.Fatalf("ListByUser failed: %v", err)
	}
	if len(result) != 1 {
		t.Fatalf("want 1 event, got %d", len(result))
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("expectations not met: %v", err)
	}
}

// ─────────────────────────────────────────────────────────────
// ListByKey Tests
// ─────────────────────────────────────────────────────────────

func TestEventRepo_ListByKey_Success(t *testing.T) {
	repo, mock := newEventRepo(t)
	ctx := context.Background()

	now := time.Now().Truncate(time.Millisecond)
	dataJSON, _ := json.Marshal(nil)

	columns := []string{"id", "type", "vehicle_id", "user_id", "key_id", "data", "created_at"}
	mock.ExpectQuery(`SELECT \* FROM events WHERE key_id = \? ORDER BY created_at DESC LIMIT 10 OFFSET 0`).
		WithArgs("k1").
		WillReturnRows(sqlmock.NewRows(columns).AddRow(
			"e1", "key_created", "v1", "u1", strPtr("k1"), dataJSON, now,
		))

	result, err := repo.ListByKey(ctx, "k1", 10, 0)
	if err != nil {
		t.Fatalf("ListByKey failed: %v", err)
	}
	if len(result) != 1 {
		t.Fatalf("want 1 event, got %d", len(result))
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("expectations not met: %v", err)
	}
}

// ─────────────────────────────────────────────────────────────
// GetStats Tests
// ─────────────────────────────────────────────────────────────

func TestEventRepo_GetStats_Success(t *testing.T) {
	repo, mock := newEventRepo(t)
	ctx := context.Background()

	mock.ExpectQuery(`SELECT type, COUNT\(\*\) as count FROM events WHERE vehicle_id = \? AND created_at >= \? AND created_at <= \? GROUP BY type`).
		WithArgs("v1", sqlmock.AnyArg(), sqlmock.AnyArg()).
		WillReturnRows(sqlmock.NewRows([]string{"type", "count"}).
			AddRow("key_created", int64(5)).
			AddRow("command_sent", int64(3)))

	stats, err := repo.GetStats(ctx, "v1", 1000, 2000)
	if err != nil {
		t.Fatalf("GetStats failed: %v", err)
	}
	if len(stats) != 2 {
		t.Fatalf("want 2 stats, got %d", len(stats))
	}
	if stats["key_created"] != 5 {
		t.Errorf("key_created: want 5, got %d", stats["key_created"])
	}
	if stats["command_sent"] != 3 {
		t.Errorf("command_sent: want 3, got %d", stats["command_sent"])
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("expectations not met: %v", err)
	}
}

func TestEventRepo_GetStats_Empty(t *testing.T) {
	repo, mock := newEventRepo(t)
	ctx := context.Background()

	mock.ExpectQuery(`SELECT type, COUNT\(\*\) as count FROM events WHERE vehicle_id = \? AND created_at >= \? AND created_at <= \? GROUP BY type`).
		WithArgs("v-empty", sqlmock.AnyArg(), sqlmock.AnyArg()).
		WillReturnRows(sqlmock.NewRows([]string{"type", "count"}))

	stats, err := repo.GetStats(ctx, "v-empty", 0, 100)
	if err != nil {
		t.Fatalf("GetStats failed: %v", err)
	}
	if len(stats) != 0 {
		t.Errorf("want empty stats, got %d", len(stats))
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("expectations not met: %v", err)
	}
}

func TestEventRepo_GetStats_DBError(t *testing.T) {
	repo, mock := newEventRepo(t)
	ctx := context.Background()

	mock.ExpectQuery(`SELECT type, COUNT\(\*\) as count FROM events WHERE vehicle_id = \? AND created_at >= \? AND created_at <= \? GROUP BY type`).
		WithArgs("v-err", sqlmock.AnyArg(), sqlmock.AnyArg()).
		WillReturnError(sqlmock.ErrCancelled)

	_, err := repo.GetStats(ctx, "v-err", 0, 100)
	if err == nil {
		t.Fatal("expected error, got nil")
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("expectations not met: %v", err)
	}
}

// ─────────────────────────────────────────────────────────────
// listEvents scan error test
// ─────────────────────────────────────────────────────────────

func TestEventRepo_listEvents_ScanError(t *testing.T) {
	repo, mock := newEventRepo(t)
	ctx := context.Background()

	// Return wrong column count to cause a scan error
	mock.ExpectQuery(`SELECT \* FROM events WHERE vehicle_id = \? ORDER BY created_at DESC LIMIT 10 OFFSET 0`).
		WithArgs("v1").
		WillReturnRows(sqlmock.NewRows([]string{"id", "type"}).AddRow("e1", "test"))

	_, err := repo.ListByVehicle(ctx, "v1", 10, 0)
	if err == nil {
		t.Fatal("expected scan error, got nil")
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("expectations not met: %v", err)
	}
}

// ─────────────────────────────────────────────────────────────
// Event types constants test
// ─────────────────────────────────────────────────────────────

func TestEventTypeConstants(t *testing.T) {
	tests := []struct {
		got, expected string
	}{
		{EventTypeKeyCreated, "key_created"},
		{EventTypeKeyActivated, "key_activated"},
		{EventTypeKeyRevoked, "key_revoked"},
		{EventTypeKeyShared, "key_shared"},
		{EventTypeKeyExpired, "key_expired"},
		{EventTypeCommandSent, "command_sent"},
		{EventTypeCommandReceived, "command_received"},
		{EventTypeCommandFailed, "command_failed"},
		{EventTypeVehicleOnline, "vehicle_online"},
		{EventTypeVehicleOffline, "vehicle_offline"},
		{EventTypeVehicleLocation, "vehicle_location"},
	}
	for _, tt := range tests {
		if tt.got != tt.expected {
			t.Errorf("want %q, got %q", tt.expected, tt.got)
		}
	}
}

// ─────────────────────────────────────────────────────────────
// NewEventRepository Test
// ─────────────────────────────────────────────────────────────

func TestNewEventRepository(t *testing.T) {
	db, _, err := sqlmock.New()
	if err != nil {
		t.Fatalf("failed to create sqlmock: %v", err)
	}
	sqlxDB := sqlx.NewDb(db, "postgres")
	repo := NewEventRepository(sqlxDB)
	if repo == nil {
		t.Fatal("NewEventRepository should return non-nil")
	}
	_ = repo
}
