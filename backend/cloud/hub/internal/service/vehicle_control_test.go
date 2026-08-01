package service

import (
	"context"
	"testing"

	"go.uber.org/zap"

	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
)

// newTestVehicleSvc 构建注入 keyStore + MockCommandDispatcher 的 VehicleControlService。
// key 默认 active + 全权限。
func newTestVehicleSvc(t *testing.T, accessBits uint32) (*VehicleControlService, *MockCommandDispatcher) {
	t.Helper()
	logger := zap.NewNop()
	s := NewVehicleControlService(logger)
	ks := NewInMemoryKeyStore()
	_ = ks.SetKey(context.Background(), &KeyRecord{
		KeyID: "key-001", OwnerUserID: "user-1", VehicleID: "VH001",
		Status: "active", AccessBits: accessBits,
	})
	s.WithKeyStore(ks)
	d := NewMockCommandDispatcher()
	s.WithCommandDispatcher(d)
	return s, d
}

func TestNewVehicleControlService(t *testing.T) {
	logger := zap.NewNop()
	s := NewVehicleControlService(logger)
	if s == nil {
		t.Fatal("NewVehicleControlService returned nil")
	}
}

func TestVehicleControlService_SendCommand(t *testing.T) {
	s, d := newTestVehicleSvc(t, PermBitAll)
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
	if d.Count() != 1 {
		t.Errorf("expected dispatcher called once, got %d", d.Count())
	}
}

func TestVehicleControlService_SendCommand_CmdIDFormat(t *testing.T) {
	s, _ := newTestVehicleSvc(t, PermBitAll)
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
	s, d := newTestVehicleSvc(t, PermBitAll)
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
	if d.Count() != len(actions) {
		t.Errorf("expected %d dispatches, got %d", len(actions), d.Count())
	}
}

func TestVehicleControlService_SendCommand_EmptyFields(t *testing.T) {
	s, _ := newTestVehicleSvc(t, PermBitAll)

	_, err := s.SendCommand(context.Background(), &pb.ControlCommandRequest{})
	if err == nil {
		t.Fatal("expected error for empty request")
	}
}

func TestVehicleControlService_SendCommand_KeyNotFound(t *testing.T) {
	s, _ := newTestVehicleSvc(t, PermBitAll)
	_, err := s.SendCommand(context.Background(), &pb.ControlCommandRequest{
		VehicleId: "VH001", Action: "unlock", KeyId: "key-missing",
	})
	if err == nil {
		t.Fatal("expected error for missing key")
	}
}

func TestVehicleControlService_SendCommand_NoPermission(t *testing.T) {
	// key 只有 lock 权限, 执行 unlock → 权限拒绝
	s, d := newTestVehicleSvc(t, PermBitLock)
	_, err := s.SendCommand(context.Background(), &pb.ControlCommandRequest{
		VehicleId: "VH001", Action: "unlock", KeyId: "key-001",
	})
	if err == nil {
		t.Fatal("expected permission denied for unlock without unlock bit")
	}
	if d.Count() != 0 {
		t.Errorf("expected no dispatch, got %d", d.Count())
	}
}

func TestVehicleControlService_SendCommand_DispatcherFailure(t *testing.T) {
	s, d := newTestVehicleSvc(t, PermBitAll)
	d.Err = context.DeadlineExceeded
	_, err := s.SendCommand(context.Background(), &pb.ControlCommandRequest{
		VehicleId: "VH001", Action: "unlock", KeyId: "key-001",
	})
	if err == nil {
		t.Fatal("expected error when dispatcher fails")
	}
}
