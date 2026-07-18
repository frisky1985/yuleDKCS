package service

import (
	"context"
	"encoding/json"
	"sync"
	"testing"
	"time"

	"github.com/DATA-DOG/go-sqlmock"
	pb "github.com/frisky1985/yuleDKCS/backend/dkcs/proto/dkcs"
	"github.com/frisky1985/yuleDKCS/backend/dkcs/internal/repository"
	"github.com/jmoiron/sqlx"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
)

// ─────────────────────────────────────────────────────────────
// NewEventService Tests
// ─────────────────────────────────────────────────────────────

func TestNewEventService(t *testing.T) {
	svc := NewEventService(
		nil, // eventRepo — will be nil, only used in DB operations
		&mockLogger{},
		newMockTelemetry(),
	)
	if svc == nil {
		t.Fatal("NewEventService should return non-nil")
	}
	if svc.logger == nil {
		t.Error("logger should be set")
	}
	if svc.telemetry == nil {
		t.Error("telemetry should be set")
	}
}

// ─────────────────────────────────────────────────────────────
// convertDataToMap Tests
// ─────────────────────────────────────────────────────────────

func TestConvertDataToMap_WithData(t *testing.T) {
	data := map[string]interface{}{
		"command":    "unlock",
		"command_id": "cmd-001",
		"count":      3,
	}

	result := convertDataToMap(data)
	if result == nil {
		t.Fatal("result should not be nil")
	}
	if result["command"] != "unlock" {
		t.Errorf("command: want 'unlock', got '%s'", result["command"])
	}
	if result["command_id"] != "cmd-001" {
		t.Errorf("command_id: want 'cmd-001', got '%s'", result["command_id"])
	}
	if result["count"] != "3" {
		t.Errorf("count: want '3', got '%s'", result["count"])
	}
}

func TestConvertDataToMap_Nil(t *testing.T) {
	result := convertDataToMap(nil)
	if result != nil {
		t.Error("nil input should return nil result")
	}
}

func TestConvertDataToMap_WrongType(t *testing.T) {
	result := convertDataToMap("not a map")
	if result != nil {
		t.Error("non-map input should return nil")
	}
}

func TestConvertDataToMap_IntValue(t *testing.T) {
	data := map[string]interface{}{
		"number": 42,
	}
	result := convertDataToMap(data)
	if result["number"] != "42" {
		t.Errorf("number: want '42', got '%s'", result["number"])
	}
}

func TestConvertDataToMap_BoolValue(t *testing.T) {
	data := map[string]interface{}{
		"enabled": true,
	}
	result := convertDataToMap(data)
	if result["enabled"] != "true" {
		t.Errorf("enabled: want 'true', got '%s'", result["enabled"])
	}
}

// ─────────────────────────────────────────────────────────────
// convertStatsToProto Tests
// ─────────────────────────────────────────────────────────────

func TestConvertStatsToProto_WithData(t *testing.T) {
	stats := map[string]int64{
		"key_created":   5,
		"key_activated": 3,
		"key_revoked":   1,
	}

	result := convertStatsToProto(stats)
	if len(result) != 3 {
		t.Fatalf("want 3 items, got %d", len(result))
	}

	// Build map for easier assertion
	resultMap := make(map[string]int64)
	for _, item := range result {
		resultMap[item.Key] = item.Value
	}
	if resultMap["key_created"] != 5 {
		t.Errorf("key_created: want 5, got %d", resultMap["key_created"])
	}
	if resultMap["key_activated"] != 3 {
		t.Errorf("key_activated: want 3, got %d", resultMap["key_activated"])
	}
	if resultMap["key_revoked"] != 1 {
		t.Errorf("key_revoked: want 1, got %d", resultMap["key_revoked"])
	}
}

func TestConvertStatsToProto_Empty(t *testing.T) {
	stats := map[string]int64{}

	result := convertStatsToProto(stats)
	// Nil or empty both acceptable for empty input
	if result != nil && len(result) != 0 {
		t.Errorf("want 0 items, got %d", len(result))
	}
}

// ═════════════════════════════════════════════════════════════
// EventService — RecordEvent / ListEvents / StreamEvents / GetEventStats
// ═════════════════════════════════════════════════════════════

// newEventServiceWithSQL creates an EventService backed by sqlmock
func newEventServiceWithSQL(t *testing.T) (*EventService, sqlmock.Sqlmock) {
	t.Helper()
	mockDB, mock, err := sqlmock.New()
	if err != nil {
		t.Fatalf("failed to create sqlmock: %v", err)
	}
	db := sqlx.NewDb(mockDB, "postgres")
	eventRepo := repository.NewEventRepository(db)

	svc := NewEventService(eventRepo, &mockLogger{}, newMockTelemetry())
	return svc, mock
}

// ─────────────────────────────────────────────────────────────
// RecordEvent
// ─────────────────────────────────────────────────────────────

func TestRecordEvent_Success(t *testing.T) {
	svc, mock := newEventServiceWithSQL(t)
	ctx := context.Background()

	event := &repository.Event{
		ID:        "evt-001",
		Type:      "key_created",
		VehicleID: "v-001",
		UserID:    "u-001",
		KeyID:     strPtrES("k-001"),
		Data:      map[string]interface{}{"source": "test"},
		CreatedAt: time.Now(),
	}

	mock.ExpectQuery(
		`INSERT INTO events \(id,type,vehicle_id,user_id,key_id,data,created_at\) VALUES \(\?,\?,\?,\?,\?,\?,\?\) RETURNING id`,
	).WithArgs(
		event.ID, event.Type, event.VehicleID, event.UserID, event.KeyID, sqlmock.AnyArg(), event.CreatedAt,
	).WillReturnRows(sqlmock.NewRows([]string{"id"}).AddRow(event.ID))

	err := svc.RecordEvent(ctx, event)
	if err != nil {
		t.Fatalf("RecordEvent failed: %v", err)
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("expectations not met: %v", err)
	}
}

func TestRecordEvent_DBError(t *testing.T) {
	svc, mock := newEventServiceWithSQL(t)
	ctx := context.Background()

	event := &repository.Event{
		ID:        "evt-err",
		Type:      "key_created",
		VehicleID: "v-001",
		UserID:    "u-001",
		CreatedAt: time.Now(),
	}

	mock.ExpectQuery(
		`INSERT INTO events \(id,type,vehicle_id,user_id,key_id,data,created_at\) VALUES \(\?,\?,\?,\?,\?,\?,\?\) RETURNING id`,
	).WithArgs(
		event.ID, sqlmock.AnyArg(), sqlmock.AnyArg(), sqlmock.AnyArg(), sqlmock.AnyArg(), sqlmock.AnyArg(), sqlmock.AnyArg(),
	).WillReturnError(sqlmock.ErrCancelled)

	err := svc.RecordEvent(ctx, event)
	if err == nil {
		t.Fatal("expected error, got nil")
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("expectations not met: %v", err)
	}
}

// ─────────────────────────────────────────────────────────────
// ListEvents
// ─────────────────────────────────────────────────────────────

func TestListEvents_Success(t *testing.T) {
	svc, mock := newEventServiceWithSQL(t)
	ctx := context.Background()

	now := time.Now()
	dataJSON, _ := json.Marshal(map[string]interface{}{"command": "unlock"})
	keyID := "k-001"

	columns := []string{"id", "type", "vehicle_id", "user_id", "key_id", "data", "created_at"}
	mock.ExpectQuery(
		`SELECT \* FROM events WHERE vehicle_id = \? ORDER BY created_at DESC LIMIT 10 OFFSET 0`,
	).WithArgs("v-001").
		WillReturnRows(sqlmock.NewRows(columns).AddRow(
			"evt-1", "key_created", "v-001", "u-001", &keyID, dataJSON, now,
		).AddRow(
			"evt-2", "command_sent", "v-001", "u-001", nil, nil, now,
		))

	req := &pb.ListEventsRequest{VehicleId: "v-001", Limit: 10, Offset: 0}
	resp, err := svc.ListEvents(ctx, req)
	if err != nil {
		t.Fatalf("ListEvents failed: %v", err)
	}
	if len(resp.Events) != 2 {
		t.Fatalf("want 2 events, got %d", len(resp.Events))
	}
	if resp.Events[0].EventId != "evt-1" {
		t.Errorf("first event: want evt-1, got %s", resp.Events[0].EventId)
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("expectations not met: %v", err)
	}
}

func TestListEvents_DBError(t *testing.T) {
	svc, mock := newEventServiceWithSQL(t)
	ctx := context.Background()

	mock.ExpectQuery(
		`SELECT \* FROM events WHERE vehicle_id = \? ORDER BY created_at DESC LIMIT 10 OFFSET 0`,
	).WithArgs("v-err").
		WillReturnError(sqlmock.ErrCancelled)

	req := &pb.ListEventsRequest{VehicleId: "v-err", Limit: 10, Offset: 0}
	_, err := svc.ListEvents(ctx, req)
	if err == nil {
		t.Fatal("expected error, got nil")
	}
	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.Internal {
		t.Errorf("want Internal, got %v", st.Code())
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("expectations not met: %v", err)
	}
}

// ─────────────────────────────────────────────────────────────
// StreamEvents
// ─────────────────────────────────────────────────────────────

type mockStreamServer struct {
	ctx    context.Context
	cancel context.CancelFunc
	mu     sync.Mutex
	events []*pb.Event
	sendFn func(*pb.Event) error
}

func newMockStreamServer() *mockStreamServer {
	ctx, cancel := context.WithCancel(context.Background())
	return &mockStreamServer{ctx: ctx, cancel: cancel}
}

func (m *mockStreamServer) Send(e *pb.Event) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	if m.sendFn != nil {
		return m.sendFn(e)
	}
	m.events = append(m.events, e)
	return nil
}

func (m *mockStreamServer) Context() context.Context {
	return m.ctx
}

func TestStreamEvents_Success(t *testing.T) {
	svc, mock := newEventServiceWithSQL(t)

	now := time.Now()
	dataJSON, _ := json.Marshal(map[string]interface{}{"command": "unlock"})

	columns := []string{"id", "type", "vehicle_id", "user_id", "key_id", "data", "created_at"}
	mock.ExpectQuery(
		`SELECT \* FROM events WHERE vehicle_id = \? ORDER BY created_at DESC LIMIT 10 OFFSET 0`,
	).WithArgs("v-001").
		WillReturnRows(sqlmock.NewRows(columns).AddRow(
			"evt-s1", "key_created", "v-001", "u-001", nil, dataJSON, now,
		))

	stream := newMockStreamServer()

	req := &pb.StreamEventsRequest{VehicleId: "v-001"}

	// Start StreamEvents in background and cancel after ticker fires (1s)
	errCh := make(chan error, 1)
	go func() {
		errCh <- svc.StreamEvents(req, stream)
	}()

	// Wait for ticker to fire twice then cancel
	time.Sleep(2100 * time.Millisecond)
	stream.cancel()

	err := <-errCh
	if err != nil {
		t.Fatalf("StreamEvents failed: %v", err)
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("expectations not met: %v", err)
	}
}

func TestStreamEvents_SendError(t *testing.T) {
	svc, mock := newEventServiceWithSQL(t)

	now := time.Now()
	dataJSON, _ := json.Marshal(map[string]interface{}{"command": "unlock"})

	columns := []string{"id", "type", "vehicle_id", "user_id", "key_id", "data", "created_at"}
	mock.ExpectQuery(
		`SELECT \* FROM events WHERE vehicle_id = \? ORDER BY created_at DESC LIMIT 10 OFFSET 0`,
	).WithArgs("v-001").
		WillReturnRows(sqlmock.NewRows(columns).AddRow(
			"evt-s2", "key_created", "v-001", "u-001", nil, dataJSON, now,
		))

	stream := newMockStreamServer()
	stream.sendFn = func(e *pb.Event) error {
		return status.Error(codes.Unavailable, "stream closed")
	}

	req := &pb.StreamEventsRequest{VehicleId: "v-001"}

	err := svc.StreamEvents(req, stream)
	if err == nil {
		t.Fatal("expected error from stream Send, got nil")
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("expectations not met: %v", err)
	}
}

func TestStreamEvents_QueryError(t *testing.T) {
	svc, mock := newEventServiceWithSQL(t)

	mock.ExpectQuery(
		`SELECT \* FROM events WHERE vehicle_id = \? ORDER BY created_at DESC LIMIT 10 OFFSET 0`,
	).WithArgs("v-err").
		WillReturnError(sqlmock.ErrCancelled)

	req := &pb.StreamEventsRequest{VehicleId: "v-err"}

	// Use context timeout — ticker fires at 1s, time out after 1500ms
	ctx, cancel := context.WithTimeout(context.Background(), 1500*time.Millisecond)
	defer cancel()
	stream := &mockStreamServer{
		ctx:    ctx,
		cancel: cancel,
		events: nil,
		sendFn: nil,
	}

	errCh := make(chan error, 1)
	go func() {
		errCh <- svc.StreamEvents(req, stream)
	}()

	err := <-errCh
	// Should exit cleanly when context is cancelled
	if err != nil {
		t.Logf("StreamEvents returned: %v", err)
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("expectations not met: %v", err)
	}
}

// ─────────────────────────────────────────────────────────────
// GetEventStats
// ─────────────────────────────────────────────────────────────

func TestGetEventStats_Success(t *testing.T) {
	svc, mock := newEventServiceWithSQL(t)
	ctx := context.Background()

	mock.ExpectQuery(
		`SELECT type, COUNT\(\*\) as count FROM events WHERE vehicle_id = \? AND created_at >= \? AND created_at <= \? GROUP BY type`,
	).WithArgs("v-001", sqlmock.AnyArg(), sqlmock.AnyArg()).
		WillReturnRows(sqlmock.NewRows([]string{"type", "count"}).
			AddRow("key_created", int64(5)).
			AddRow("command_sent", int64(3)))

	req := &pb.GetEventStatsRequest{
		VehicleId: "v-001",
		StartTime: 1000,
		EndTime:   2000,
	}
	resp, err := svc.GetEventStats(ctx, req)
	if err != nil {
		t.Fatalf("GetEventStats failed: %v", err)
	}
	if len(resp.Stats) != 2 {
		t.Fatalf("want 2 stats, got %d", len(resp.Stats))
	}

	// Build map for verification
	statMap := make(map[string]int64)
	for _, s := range resp.Stats {
		statMap[s.Key] = s.Value
	}
	if statMap["key_created"] != 5 {
		t.Errorf("key_created: want 5, got %d", statMap["key_created"])
	}
	if statMap["command_sent"] != 3 {
		t.Errorf("command_sent: want 3, got %d", statMap["command_sent"])
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("expectations not met: %v", err)
	}
}

func TestGetEventStats_DBError(t *testing.T) {
	svc, mock := newEventServiceWithSQL(t)
	ctx := context.Background()

	mock.ExpectQuery(
		`SELECT type, COUNT\(\*\) as count FROM events WHERE vehicle_id = \? AND created_at >= \? AND created_at <= \? GROUP BY type`,
	).WithArgs("v-err", sqlmock.AnyArg(), sqlmock.AnyArg()).
		WillReturnError(sqlmock.ErrCancelled)

	req := &pb.GetEventStatsRequest{VehicleId: "v-err", StartTime: 0, EndTime: 100}
	_, err := svc.GetEventStats(ctx, req)
	if err == nil {
		t.Fatal("expected error, got nil")
	}
	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.Internal {
		t.Errorf("want Internal, got %v", st.Code())
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("expectations not met: %v", err)
	}
}

// strPtrES helper for *string fields (event_service_test.go)
func strPtrES(s string) *string { return &s }
