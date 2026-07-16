package service

import (
	"context"
	"testing"
	"time"

	"go.uber.org/zap"
	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/adapter"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/token"
)

// ─────────────────────────────────────────────────────────────
// VehicleControlService tests
// ─────────────────────────────────────────────────────────────

func TestNewVehicleControlService(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	defer logger.Sync()

	svc := NewVehicleControlService(logger)
	if svc == nil {
		t.Fatal("NewVehicleControlService returned nil")
	}
}

func TestVehicleControlService_SendCommand(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	defer logger.Sync()

	svc := NewVehicleControlService(logger)
	ctx := context.Background()

	resp, err := svc.SendCommand(ctx, &pb.ControlCommandRequest{
		VehicleId: "v-001",
		UserId:    "u-001",
		KeyId:     "k-001",
		Action:    "unlock",
		Source:    4, // Remote
	})
	if err != nil {
		t.Fatalf("SendCommand: %v", err)
	}
	if resp.CmdId == "" {
		t.Errorf("CmdId should not be empty")
	}
	if resp.ResultCode != 0 {
		t.Errorf("ResultCode: want 0, got %d", resp.ResultCode)
	}
}

func TestVehicleControlService_SendCommandWithParams(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	defer logger.Sync()

	svc := NewVehicleControlService(logger)
	ctx := context.Background()

	resp, err := svc.SendCommand(ctx, &pb.ControlCommandRequest{
		VehicleId: "v-002",
		UserId:    "u-002",
		KeyId:     "k-002",
		Action:    "engine_on",
		Params:    []byte(`{"duration":1800}`),
		Source:    5, // Edge
		TraceId:   "trace-001",
	})
	if err != nil {
		t.Fatalf("SendCommand: %v", err)
	}
	if resp.CmdId == "" {
		t.Errorf("CmdId should not be empty")
	}
}

func TestVehicleControlService_AllActions(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	defer logger.Sync()

	svc := NewVehicleControlService(logger)
	ctx := context.Background()

	actions := []string{"unlock", "lock", "engine_on", "engine_off", "trunk", "climate", "find"}
	for _, action := range actions {
		resp, err := svc.SendCommand(ctx, &pb.ControlCommandRequest{
			VehicleId: "v-" + action,
			UserId:    "u-001",
			KeyId:     "k-001",
			Action:    action,
		})
		if err != nil {
			t.Errorf("SendCommand(%s): %v", action, err)
		}
		if resp.CmdId == "" {
			t.Errorf("SendCommand(%s): empty cmd_id", action)
		}
	}
}

// ─────────────────────────────────────────────────────────────
// DKServer tests
// ─────────────────────────────────────────────────────────────

func TestNewLocalDKServer(t *testing.T) {
	s := NewLocalDKServer()
	if s == nil {
		t.Fatal("NewLocalDKServer returned nil")
	}
}

func TestLocalDKServer_IssueKey(t *testing.T) {
	s := NewLocalDKServer()
	ctx := context.Background()

	resp, err := s.IssueKey(ctx, &KeyRequest{
		TokenID:   "token-001",
		SubjectID: "user-001",
		VehicleID: "v-001",
		Permissions: []token.Permission{
			token.PermLock, token.PermEngineStart, token.PermShare,
		},
		ExpiresAt: time.Now().Add(24 * time.Hour).UnixMilli(),
	})
	if err != nil {
		t.Fatalf("IssueKey: %v", err)
	}
	if resp.KeyID == "" {
		t.Errorf("KeyID should not be empty")
	}
	if resp.Status != "issued" {
		t.Errorf("Status: want issued, got %s", resp.Status)
	}
}

func TestLocalDKServer_RevokeKeyByToken(t *testing.T) {
	s := NewLocalDKServer()
	ctx := context.Background()

	err := s.RevokeKeyByToken(ctx, "token-001")
	if err != nil {
		t.Fatalf("RevokeKeyByToken: %v", err)
	}
}

// ─────────────────────────────────────────────────────────────
// GRPCDKServer tests
// ─────────────────────────────────────────────────────────────

func TestNewGRPCDKServer(t *testing.T) {
	s := NewGRPCDKServer()
	if s == nil {
		t.Fatal("NewGRPCDKServer returned nil")
	}
}

func TestGRPCDKServer_RegisterGRPCServer(t *testing.T) {
	s := NewGRPCDKServer()
	// Should not panic with nil argument
	s.RegisterGRPCServer(nil)
}

// ─────────────────────────────────────────────────────────────
// Key management service full lifecycle tests
// ─────────────────────────────────────────────────────────────

func TestKeyManagementService_Getters(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	defer logger.Sync()

	reg := adapter.NewRegistry(logger)
	km := NewKeyManagementService(reg, logger)
	if km == nil {
		t.Fatal("NewKeyManagementService returned nil")
	}
}
