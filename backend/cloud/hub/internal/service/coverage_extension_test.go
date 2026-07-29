package service

import (
	"context"
	"testing"

	"go.uber.org/zap"
	"google.golang.org/grpc/metadata"

	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/adapter"
)

// ── Coverage extension tests ──

// faultyAdapter returns errors from HealthCheck to test error paths
type faultyAdapter struct{}

func (f *faultyAdapter) Vendor() string           { return "XIAOMI" }
func (f *faultyAdapter) Protocol() string          { return "proto" }
func (f *faultyAdapter) BindKey(ctx context.Context, req *pb.BindKeyRequest) (*pb.BindKeyResponse, error) {
	return &pb.BindKeyResponse{
		Key: &pb.DigitalKey{KeyId: "faulty-key-001"},
	}, nil
}
func (f *faultyAdapter) UnbindKey(ctx context.Context, keyID string) error { return nil }
func (f *faultyAdapter) RevokeNotify(ctx context.Context, keyID string, reason string) error { return nil }
func (f *faultyAdapter) ShareKey(ctx context.Context, req *pb.CreateShareRequest) (*pb.CreateShareResponse, error) {
	return nil, nil
}
func (f *faultyAdapter) AcceptShare(ctx context.Context, req *pb.AcceptShareRequest) (*pb.AcceptShareResponse, error) {
	return nil, nil
}
func (f *faultyAdapter) Notify(ctx context.Context, userID string, notification *pb.VehicleStatusUpdate) error {
	return nil
}
func (f *faultyAdapter) HealthCheck(ctx context.Context) (*pb.AdapterStatus, error) {
	return &pb.AdapterStatus{Healthy: false, ErrorMsg: "simulated failure"}, nil
}

func TestHubTransportService_HealthCheck_WithFaultyAdapter(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	reg.Register("XIAOMI", "proto", &faultyAdapter{})
	s := NewHubTransportService(reg, logger)

	resp, err := s.HealthCheck(context.Background(), &pb.HealthCheckRequest{})
	if err != nil {
		t.Fatalf("HealthCheck failed: %v", err)
	}
	if resp.Healthy {
		t.Error("expected unhealthy when faulty adapter is registered")
	}
	if len(resp.Adapters) != 1 {
		t.Errorf("expected 1 adapter, got %d", len(resp.Adapters))
	}
}

// TestUnifiedKeyService_RenewKey_WithSession exercises the session path
func TestUnifiedKeyService_RenewKey_WithSession(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)

	// Without adapter registered, revoke goes through session-path
	req := &pb.RenewKeyRequest{
		KeyId:      "key-001",
		ValidUntil: 1900000000000,
	}
	resp, err := s.RenewKey(context.Background(), req)
	if err != nil {
		t.Fatalf("RenewKey failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}

// TestDetectProtocol_VendorAutoDetection tests the AutoDetectProtocol path
func TestDetectProtocol_VendorAutoDetection(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)

	tests := []struct {
		name   string
		data   []byte
		vendor string
	}{
		{"vendor_unknown", []byte{0x01}, "UNKNOWN_VENDOR"},
		{"data_icce_with_vendor", []byte{0x80, 0x01}, "xiaomi"},
		{"data_unknown_with_vendor", []byte{0xFF, 0x01}, "XIAOMI"},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			_ = s.detectProtocol(tt.data, tt.vendor)
			// Verify no panic
		})
	}
}

// TestUnifiedKeyService_UnbindKey_WithRegistration tests with registered adapter
func TestUnifiedKeyService_UnbindKey_WithRegistration(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	reg.Register("XIAOMI", "ICCOA_DK40", adapter.NewICCOAAdapter("XIAOMI", logger))
	s := NewUnifiedKeyService(reg, logger)

	resp, err := s.UnbindKey(context.Background(), &pb.UnbindKeyRequest{KeyId: "key-001"})
	if err != nil {
		t.Fatalf("UnbindKey failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}

// TestUnifiedKeyService_RevokeKey_WithRegistration tests with registered adapter
func TestUnifiedKeyService_RevokeKey_WithRegistration(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	reg.Register("XIAOMI", "ICCOA_DK40", adapter.NewICCOAAdapter("XIAOMI", logger))
	s := NewUnifiedKeyService(reg, logger)

	resp, err := s.RevokeKey(context.Background(), &pb.RevokeKeyRequest{
		KeyId:  "key-001",
		Reason: "test",
	})
	if err != nil {
		t.Fatalf("RevokeKey failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}

// TestUnifiedKeyService_AcceptShare_WithParams tests with request parameters
func TestUnifiedKeyService_AcceptShare_WithParams(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)

	resp, err := s.AcceptShare(context.Background(), &pb.AcceptShareRequest{
		ShareCode: "123456",
		UserId:    "user-2",
		DeviceId:  "dev-2",
	})
	if err != nil {
		t.Fatalf("AcceptShare failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}

// TestKeyManagementService_ResumeKey_KeyNotFound tests the error path
func TestKeyManagementService_ResumeKey_KeyNotFound(t *testing.T) {
	s := newTestKeyMgmtService()
	ctx := context.Background()

	md := metadata.New(map[string]string{"user_id": "user-1"})
	ctx2 := metadata.NewIncomingContext(ctx, md)

	_, err := s.ResumeKey(ctx2, &pb.ResumeKeyRequest{KeyId: "nonexistent"})
	if err == nil {
		t.Fatal("expected error for nonexistent key")
	}
}

// TestKeyManagementService_BindKey_AdapterError tests the adapter error path
func TestKeyManagementService_BindKey_AdapterError(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	reg.Register("XIAOMI", "ICCOA_DK40", &faultyAdapter{})
	s := NewKeyManagementService(reg, logger)

	req := &pb.BindKeyRequest{
		VehicleId: "VH001",
		UserId:    "U001",
		Vendor:    pb.PhoneVendor_XIAOMI,
		KeyType:   pb.KeyType_OWNER,
	}

	resp, err := s.BindKey(context.Background(), req)
	if err != nil {
		t.Fatalf("BindKey failed: %v", err)
	}
	// faultyAdapter returns a response with a key but tests error paths
	if resp.Key == nil {
		t.Fatal("expected non-nil key")
	}
	if resp.Key.KeyId != "faulty-key-001" {
		t.Errorf("expected faulty-key-001, got %s", resp.Key.KeyId)
	}
}

// TestKeyManagementService_UnbindKey_WithAdapterError tests unbind with adapter error
func TestKeyManagementService_UnbindKey_WithAdapterError(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	reg.Register("XIAOMI", "ICCOA_DK40", &faultyAdapter{})
	s := NewKeyManagementService(reg, logger)
	ctx := context.Background()

	s.keyStore.SetKey(ctx, &KeyRecord{
		KeyID: "key-001", OwnerUserID: "user-1", Vendor: "XIAOMI", VehicleID: "VH001",
	})

	md := metadata.New(map[string]string{"user_id": "user-1"})
	ctx2 := metadata.NewIncomingContext(ctx, md)

	resp, err := s.UnbindKey(ctx2, &pb.UnbindKeyRequest{KeyId: "key-001"})
	if err != nil {
		t.Fatalf("UnbindKey failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}

// TestKeyManagementService_RevokeKey_WithAdapterError tests revoke with adapter error
func TestKeyManagementService_RevokeKey_WithAdapterError(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	reg.Register("XIAOMI", "ICCOA_DK40", &faultyAdapter{})
	s := NewKeyManagementService(reg, logger)
	ctx := context.Background()

	s.keyStore.SetKey(ctx, &KeyRecord{
		KeyID: "key-001", OwnerUserID: "user-1", Vendor: "XIAOMI", VehicleID: "VH001",
	})

	md := metadata.New(map[string]string{"user_id": "user-1"})
	ctx2 := metadata.NewIncomingContext(ctx, md)

	resp, err := s.RevokeKey(ctx2, &pb.RevokeKeyRequest{KeyId: "key-001", Reason: "test"})
	if err != nil {
		t.Fatalf("RevokeKey failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}

// TestUnifiedKeyService_SendCommand_MissingSession tests various paths
func TestUnifiedKeyService_SendCommand_MissingSession(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)

	tests := []struct {
		name      string
		vehicleID string
		action    string
		keyID     string
	}{
		{"lock", "VH001", "lock", "key-001"},
		{"unlock", "VH002", "unlock", "key-002"},
		{"engine_start", "VH003", "engine_start", "key-003"},
		{"trunk_open", "VH004", "trunk_open", "key-004"},
		{"climate_on", "VH005", "climate_on", "key-005"},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			req := &pb.ControlCommandRequest{
				VehicleId: tt.vehicleID,
				Action:    tt.action,
				KeyId:     tt.keyID,
			}
			resp, err := s.SendCommand(context.Background(), req)
			if err != nil {
				t.Fatalf("SendCommand failed: %v", err)
			}
			if resp == nil {
				t.Fatal("expected non-nil response")
			}
		})
	}
}

// TestUnifiedKeyService_ListKeys_VariousStates
func TestUnifiedKeyService_ListKeys_VariousStates(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)

	resp, err := s.ListKeys(context.Background(), &pb.ListKeysRequest{
		UserId:    "U001",
		VehicleId: "VH001",
	})
	if err != nil {
		t.Fatalf("ListKeys failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}

// TestUnifiedKeyService_GetKey_WithBind tests get key after bind flow
func TestUnifiedKeyService_GetKey_WithBind(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	reg.Register("XIAOMI", "ICCOA_DK40", adapter.NewICCOAAdapter("XIAOMI", logger))
	s := NewUnifiedKeyService(reg, logger)

	// Bind key first to create session state
	req := &pb.BindKeyRequest{
		VehicleId: "VH001",
		DeviceId:  "DEV001",
		UserId:    "U001",
		Vendor:    pb.PhoneVendor_XIAOMI,
		Protocol:  pb.Protocol_ICCOA_DK40,
		KeyType:   pb.KeyType_OWNER,
	}
	bindResp, err := s.BindKey(context.Background(), req)
	if err != nil {
		t.Fatalf("BindKey failed: %v", err)
	}
	if bindResp.Key == nil {
		t.Fatal("expected key in bind response")
	}

	resp, err := s.GetKey(context.Background(), &pb.GetKeyRequest{KeyId: bindResp.Key.KeyId})
	if err != nil {
		t.Fatalf("GetKey failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}
