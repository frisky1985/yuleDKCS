package service

import (
	"context"
	"encoding/json"
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
// Build helper for CommandService
// ─────────────────────────────────────────────────────────────

func buildTestCommandService(keyRepo KeyRepository, vehicleRepo VehicleRepository) *CommandService {
	// Create EventService with nil eventRepo — only used in success path of sendCommand
	// Error paths all exit before RecordEvent is called, so this is safe for error-path tests.
	eventSvc := &EventService{
		eventRepo: nil,
		logger:    &mockLogger{},
		telemetry: newMockTelemetry(),
	}

	return NewCommandService(
		keyRepo,
		vehicleRepo,
		&mockLogger{},
		newMockTelemetry(),
		eventSvc,
	)
}

// ─────────────────────────────────────────────────────────────
// NewCommandService Tests
// ─────────────────────────────────────────────────────────────

func TestNewCommandService(t *testing.T) {
	svc := buildTestCommandService(newMockKeyRepo(), newMockVehicleRepo())
	if svc == nil {
		t.Fatal("NewCommandService should return non-nil")
	}
	if svc.keyRepo == nil {
		t.Error("keyRepo should be set")
	}
	if svc.vehicleRepo == nil {
		t.Error("vehicleRepo should be set")
	}
	if svc.logger == nil {
		t.Error("logger should be set")
	}
	if svc.telemetry == nil {
		t.Error("telemetry should be set")
	}
	if svc.eventSvc == nil {
		t.Error("eventSvc should be set")
	}
}

// ─────────────────────────────────────────────────────────────
// sendCommand error path helpers
// ─────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────
// sendCommand: KeyNotFound
// ─────────────────────────────────────────────────────────────

func TestSendCommand_KeyNotFound(t *testing.T) {
	svc := buildTestCommandService(newMockKeyRepo(), newMockVehicleRepo())
	ctx := context.Background()

	_, err := svc.Unlock(ctx, &pb.UnlockRequest{KeyId: "nonexistent", VehicleId: "v-001"})

	if err == nil {
		t.Fatal("expected NotFound error for nonexistent key")
	}
	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.NotFound {
		t.Errorf("want NotFound, got %v", st.Code())
	}
}

// ─────────────────────────────────────────────────────────────
// sendCommand: KeyNotActive
// ─────────────────────────────────────────────────────────────

func TestSendCommand_KeyNotActive(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	svc := buildTestCommandService(keyRepo, vehicleRepo)
	ctx := context.Background()

	keyRepo.keys["key-pending"] = &repository.Key{
		ID:        "key-pending",
		VehicleID: "v-001",
		Status:    "pending",
		Permissions: []string{"unlock"},
	}

	_, err := svc.Unlock(ctx, &pb.UnlockRequest{KeyId: "key-pending", VehicleId: "v-001"})
	if err == nil {
		t.Fatal("expected FailedPrecondition for non-active key")
	}
	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.FailedPrecondition {
		t.Errorf("want FailedPrecondition, got %v", st.Code())
	}
}

// ─────────────────────────────────────────────────────────────
// sendCommand: NoPermission
// ─────────────────────────────────────────────────────────────

func TestSendCommand_NoPermission(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	svc := buildTestCommandService(keyRepo, vehicleRepo)
	ctx := context.Background()

	keyRepo.keys["key-no-perm"] = &repository.Key{
		ID:          "key-no-perm",
		VehicleID:   "v-001",
		Status:      "active",
		Permissions: []string{}, // no unlock permission
	}

	vehicleRepo.vehicles["v-001"] = &repository.Vehicle{
		ID: "v-001", OwnerID: "user-001", IsOnline: true,
	}

	_, err := svc.Unlock(ctx, &pb.UnlockRequest{KeyId: "key-no-perm", VehicleId: "v-001"})
	if err == nil {
		t.Fatal("expected PermissionDenied for key without permission")
	}
	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.PermissionDenied {
		t.Errorf("want PermissionDenied, got %v", st.Code())
	}
}

// ─────────────────────────────────────────────────────────────
// sendCommand: VehicleMismatch
// ─────────────────────────────────────────────────────────────

func TestSendCommand_VehicleMismatch(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	svc := buildTestCommandService(keyRepo, vehicleRepo)
	ctx := context.Background()

	keyRepo.keys["key-mismatch"] = &repository.Key{
		ID:          "key-mismatch",
		VehicleID:   "vehicle-A",
		Status:      "active",
		Permissions: []string{"unlock"},
	}

	_, err := svc.Unlock(ctx, &pb.UnlockRequest{KeyId: "key-mismatch", VehicleId: "vehicle-B"})
	if err == nil {
		t.Fatal("expected InvalidArgument for vehicle mismatch")
	}
	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.InvalidArgument {
		t.Errorf("want InvalidArgument, got %v", st.Code())
	}
}

// ─────────────────────────────────────────────────────────────
// sendCommand: VehicleNotFound
// ─────────────────────────────────────────────────────────────

func TestSendCommand_VehicleNotFound(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	svc := buildTestCommandService(keyRepo, vehicleRepo)
	ctx := context.Background()

	keyRepo.keys["key-vnotfound"] = &repository.Key{
		ID:          "key-vnotfound",
		VehicleID:   "nonexistent-vehicle",
		Status:      "active",
		Permissions: []string{"unlock"},
	}

	_, err := svc.Unlock(ctx, &pb.UnlockRequest{KeyId: "key-vnotfound", VehicleId: "nonexistent-vehicle"})
	if err == nil {
		t.Fatal("expected NotFound for nonexistent vehicle")
	}
	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.NotFound {
		t.Errorf("want NotFound, got %v", st.Code())
	}
}

// ─────────────────────────────────────────────────────────────
// sendCommand: VehicleOffline
// ─────────────────────────────────────────────────────────────

func TestSendCommand_VehicleOffline(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	svc := buildTestCommandService(keyRepo, vehicleRepo)
	ctx := context.Background()

	keyRepo.keys["key-offline"] = &repository.Key{
		ID:          "key-offline",
		VehicleID:   "vehicle-offline",
		Status:      "active",
		Permissions: []string{"unlock"},
	}

	vehicleRepo.vehicles["vehicle-offline"] = &repository.Vehicle{
		ID: "vehicle-offline", OwnerID: "user-001", IsOnline: false,
	}

	_, err := svc.Unlock(ctx, &pb.UnlockRequest{KeyId: "key-offline", VehicleId: "vehicle-offline"})
	if err == nil {
		t.Fatal("expected Unavailable for offline vehicle")
	}
	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.Unavailable {
		t.Errorf("want Unavailable, got %v", st.Code())
	}
}

// ─────────────────────────────────────────────────────────────
// All Command Methods (each delegates to sendCommand)
// Tests each method with a key-not-found scenario (fastest path)
// ─────────────────────────────────────────────────────────────

func TestCommandMethods_KeyNotFound(t *testing.T) {
	svc := buildTestCommandService(newMockKeyRepo(), newMockVehicleRepo())
	ctx := context.Background()

	tests := []struct {
		name string
		call func() error
	}{
		{
			name: "Lock",
			call: func() error {
				_, err := svc.Lock(ctx, &pb.LockRequest{KeyId: "nonexistent", VehicleId: "v-001"})
				return err
			},
		},
		{
			name: "EngineStart",
			call: func() error {
				_, err := svc.EngineStart(ctx, &pb.EngineStartRequest{KeyId: "nonexistent", VehicleId: "v-001"})
				return err
			},
		},
		{
			name: "EngineStop",
			call: func() error {
				_, err := svc.EngineStop(ctx, &pb.EngineStopRequest{KeyId: "nonexistent", VehicleId: "v-001"})
				return err
			},
		},
		{
			name: "TrunkOpen",
			call: func() error {
				_, err := svc.TrunkOpen(ctx, &pb.TrunkOpenRequest{KeyId: "nonexistent", VehicleId: "v-001"})
				return err
			},
		},
		{
			name: "Panic",
			call: func() error {
				_, err := svc.Panic(ctx, &pb.PanicRequest{KeyId: "nonexistent", VehicleId: "v-001"})
				return err
			},
		},
		{
			name: "FindVehicle",
			call: func() error {
				_, err := svc.FindVehicle(ctx, &pb.FindVehicleRequest{KeyId: "nonexistent", VehicleId: "v-001"})
				return err
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			err := tt.call()
			if err == nil {
				t.Fatal("expected NotFound error")
			}
			st, ok := status.FromError(err)
			if !ok || st.Code() != codes.NotFound {
				t.Errorf("want NotFound, got %v", st.Code())
			}
		})
	}
}

// ─────────────────────────────────────────────────────────────
// GetCommandStatus Tests
// ─────────────────────────────────────────────────────────────

func TestGetCommandStatus_Success(t *testing.T) {
	svc := buildTestCommandService(newMockKeyRepo(), newMockVehicleRepo())
	ctx := context.Background()

	resp, err := svc.GetCommandStatus(ctx, &pb.GetCommandStatusRequest{
		CommandId: "cmd-001",
	})

	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if resp.CommandId != "cmd-001" {
		t.Errorf("CommandId: want 'cmd-001', got '%s'", resp.CommandId)
	}
	if resp.Status != "completed" {
		t.Errorf("Status: want 'completed', got '%s'", resp.Status)
	}
	if resp.Timestamp == 0 {
		t.Error("Timestamp should be non-zero")
	}
}

// ─────────────────────────────────────────────────────────────
// generateCommandID Tests
// ─────────────────────────────────────────────────────────────

func TestGenerateCommandID(t *testing.T) {
	id1 := generateCommandID()
	id2 := generateCommandID()

	if id1 == "" {
		t.Error("command ID should not be empty")
	}
	if id1 == id2 {
		t.Log("Warning: two consecutive command IDs are identical (low probability)")
	}
	// Should be at least 14 chars (20060102150405) + 6 random = 20
	if len(id1) < 18 {
		t.Errorf("command ID length: want >= 18, got %d", len(id1))
	}
}

// ═════════════════════════════════════════════════════════════
// sendCommand Success Path (exercises publishCommand + RecordEvent)
// ═════════════════════════════════════════════════════════════

func buildCommandServiceWithEventRepo(t *testing.T) (*CommandService, sqlmock.Sqlmock) {
	t.Helper()

	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	logger := &mockLogger{}
	telemetry := newMockTelemetry()

	// Create EventService with sqlmock-backed event repo
	mockDB, mock, err := sqlmock.New()
	if err != nil {
		t.Fatalf("failed to create sqlmock: %v", err)
	}
	db := sqlx.NewDb(mockDB, "postgres")
	eventRepo := repository.NewEventRepository(db)

	eventSvc := NewEventService(eventRepo, logger, telemetry)

	svc := NewCommandService(keyRepo, vehicleRepo, logger, telemetry, eventSvc)
	return svc, mock
}

func TestSendCommand_Success(t *testing.T) {
	svc, mock := buildCommandServiceWithEventRepo(t)
	ctx := context.Background()

	// Setup: active key with unlock permission
	keyRepo := svc.keyRepo.(*mockKeyRepo)
	keyRepo.keys["key-success"] = &repository.Key{
		ID:          "key-success",
		VehicleID:   "vehicle-online",
		UserID:      "user-001",
		Status:      "active",
		Permissions: []string{"unlock"},
	}

	// Setup: online vehicle
	vehicleRepo := svc.vehicleRepo.(*mockVehicleRepo)
	vehicleRepo.vehicles["vehicle-online"] = &repository.Vehicle{
		ID:       "vehicle-online",
		OwnerID:  "user-001",
		IsOnline: true,
	}

	// Expect the RecordEvent INSERT call
	mock.ExpectQuery(
		`INSERT INTO events \(id,type,vehicle_id,user_id,key_id,data,created_at\) VALUES \(\?,\?,\?,\?,\?,\?,\?\) RETURNING id`).
		WithArgs(sqlmock.AnyArg(), "command_sent", "vehicle-online", "user-001", sqlmock.AnyArg(), sqlmock.AnyArg(), sqlmock.AnyArg()).
		WillReturnRows(sqlmock.NewRows([]string{"id"}).AddRow("evt-001"))

	// Send command
	resp, err := svc.Unlock(ctx, &pb.UnlockRequest{KeyId: "key-success", VehicleId: "vehicle-online"})
	if err != nil {
		t.Fatalf("Unlock success path failed: %v", err)
	}
	if resp.CommandId == "" {
		t.Error("CommandId should not be empty")
	}
	if resp.Status != "accepted" {
		t.Errorf("Status: want 'accepted', got %q", resp.Status)
	}
	if resp.Timestamp == 0 {
		t.Error("Timestamp should be non-zero")
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("expectations not met: %v", err)
	}
}

// Ensure json and time are used
var _ = json.Marshal
var _ = time.Now
