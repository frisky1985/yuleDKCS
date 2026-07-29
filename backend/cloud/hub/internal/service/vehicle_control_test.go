package service

import (
	"context"
	"testing"

	"go.uber.org/zap"

	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
)

func TestNewVehicleControlService(t *testing.T) {
	logger := zap.NewNop()
	s := NewVehicleControlService(logger)
	if s == nil {
		t.Fatal("NewVehicleControlService returned nil")
	}
}

func TestVehicleControlService_SendCommand(t *testing.T) {
	logger := zap.NewNop()
	s := NewVehicleControlService(logger)
	ctx := context.Background()

	req := &pb.ControlCommandRequest{
		VehicleId: "VH001",
		Action:    "unlock",
		KeyId:     "key-001",
	}

	resp, err := s.SendCommand(ctx, req)
	if err != nil {
		t.Fatalf("SendCommand failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
	if resp.CmdId == "" {
		t.Error("expected non-empty CmdId")
	}
	if resp.ResultCode != 0 {
		t.Errorf("expected result code 0, got %d", resp.ResultCode)
	}
}

func TestVehicleControlService_SendCommand_CmdIDFormat(t *testing.T) {
	logger := zap.NewNop()
	s := NewVehicleControlService(logger)
	ctx := context.Background()

	req := &pb.ControlCommandRequest{
		VehicleId: "VH001",
		Action:    "lock",
		KeyId:     "key-001",
	}

	resp, err := s.SendCommand(ctx, req)
	if err != nil {
		t.Fatalf("SendCommand failed: %v", err)
	}

	expectedPrefix := "cmd-VH001-lock"
	if len(resp.CmdId) < len(expectedPrefix) || resp.CmdId[:len(expectedPrefix)] != expectedPrefix {
		t.Errorf("expected CmdId to start with %s, got %s", expectedPrefix, resp.CmdId)
	}
}

func TestVehicleControlService_SendCommand_DifferentActions(t *testing.T) {
	logger := zap.NewNop()
	s := NewVehicleControlService(logger)
	ctx := context.Background()

	actions := []string{"lock", "unlock", "engine_start", "engine_stop", "trunk_open"}
	for _, action := range actions {
		req := &pb.ControlCommandRequest{
			VehicleId: "VH001",
			Action:    action,
			KeyId:     "key-001",
		}
		resp, err := s.SendCommand(ctx, req)
		if err != nil {
			t.Fatalf("SendCommand with action %s failed: %v", action, err)
		}
		if resp.CmdId == "" {
			t.Errorf("expected non-empty CmdId for action %s", action)
		}
	}
}

func TestVehicleControlService_SendCommand_EmptyFields(t *testing.T) {
	logger := zap.NewNop()
	s := NewVehicleControlService(logger)

	resp, err := s.SendCommand(context.Background(), &pb.ControlCommandRequest{})
	if err != nil {
		t.Fatalf("SendCommand failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}

// ── StreamStatus ──


