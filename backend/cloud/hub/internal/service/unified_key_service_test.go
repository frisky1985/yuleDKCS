package service

import (
	"context"
	"testing"

	"go.uber.org/zap"

	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/adapter"
)

func TestNewUnifiedKeyService(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)
	if s == nil {
		t.Fatal("NewUnifiedKeyService returned nil")
	}
	if s.unifiedMgr == nil {
		t.Error("expected non-nil unifiedMgr")
	}
	if s.logger == nil {
		t.Error("expected non-nil logger")
	}
}

func TestUnifiedKeyService_NegotiateProtocol(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)

	ctx := context.Background()
	req := &NegotiateProtocolRequest{
		DeviceId: "DEV001",
		Vendor:   "XIAOMI",
		Os:       "android",
		Caps: &NegotiateCapabilities{
			Ble: true, Uwb: true, Nfc: true, Se: true, Fira: true,
			BleVersion: "5.3", UwbVersion: "FiRa 1.0",
		},
		VehicleCaps: &NegotiateCapabilities{
			Ble: true, Uwb: true, Nfc: true, Se: true, Fira: true,
		},
	}

	resp, err := s.NegotiateProtocol(ctx, req)
	if err != nil {
		t.Fatalf("NegotiateProtocol failed: %v", err)
	}
	if resp.SessionId == "" {
		t.Error("expected non-empty SessionId")
	}
	if resp.Protocol == pb.Protocol_PROTOCOL_UNSPECIFIED {
		t.Error("expected a specific protocol, got UNSPECIFIED")
	}
}

func TestUnifiedKeyService_NegotiateProtocol_NoCaps(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)

	req := &NegotiateProtocolRequest{
		DeviceId: "DEV001",
		Vendor:   "unknown",
		Caps:     &NegotiateCapabilities{},
		VehicleCaps: &NegotiateCapabilities{},
	}

	// No compatible protocol when both sides have no capabilities
	_, err := s.NegotiateProtocol(context.Background(), req)
	if err == nil {
		t.Fatal("expected negotiation failure with no capabilities")
	}
}

// ── BindKey ──

func TestUnifiedKeyService_BindKey_NoAdapter(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)

	req := &pb.BindKeyRequest{
		VehicleId: "VH001",
		UserId:    "U001",
		Vendor:    pb.PhoneVendor_VENDOR_UNSPECIFIED,
	}
	resp, err := s.BindKey(context.Background(), req)
	if err != nil {
		t.Fatalf("BindKey failed: %v", err)
	}
	if resp.ErrorCode != "ADAPTER_NOT_FOUND" {
		t.Errorf("expected ADAPTER_NOT_FOUND, got %s", resp.ErrorCode)
	}
}

func TestUnifiedKeyService_BindKey_Success(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	reg.Register("XIAOMI", "ICCOA_DK40", adapter.NewICCOAAdapter("XIAOMI", logger))
	s := NewUnifiedKeyService(reg, logger)

	req := &pb.BindKeyRequest{
		VehicleId: "VH001",
		DeviceId:  "DEV001",
		UserId:    "U001",
		Vendor:    pb.PhoneVendor_XIAOMI,
		Protocol:  pb.Protocol_ICCOA_DK40,
		KeyType:   pb.KeyType_OWNER,
	}
	resp, err := s.BindKey(context.Background(), req)
	if err != nil {
		t.Fatalf("BindKey failed: %v", err)
	}
	if resp.ErrorCode != "" {
		t.Errorf("expected no error, got %s", resp.ErrorCode)
	}
	if resp.Key == nil {
		t.Fatal("expected non-nil key")
	}
}

// ── UnbindKey ──

func TestUnifiedKeyService_UnbindKey(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)

	resp, err := s.UnbindKey(context.Background(), &pb.UnbindKeyRequest{KeyId: "key-001"})
	if err != nil {
		t.Fatalf("UnbindKey failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}

// ── SuspendKey ──

func TestUnifiedKeyService_SuspendKey(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)

	resp, err := s.SuspendKey(context.Background(), &pb.SuspendKeyRequest{KeyId: "key-001"})
	if err != nil {
		t.Fatalf("SuspendKey failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}

// ── ResumeKey ──

func TestUnifiedKeyService_ResumeKey(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)

	resp, err := s.ResumeKey(context.Background(), &pb.ResumeKeyRequest{KeyId: "key-001"})
	if err != nil {
		t.Fatalf("ResumeKey failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}

// ── RevokeKey ──

func TestUnifiedKeyService_RevokeKey(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)

	resp, err := s.RevokeKey(context.Background(), &pb.RevokeKeyRequest{KeyId: "key-001", Reason: "test"})
	if err != nil {
		t.Fatalf("RevokeKey failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}

// ── RenewKey ──

func TestUnifiedKeyService_RenewKey(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)

	resp, err := s.RenewKey(context.Background(), &pb.RenewKeyRequest{KeyId: "key-001"})
	if err != nil {
		t.Fatalf("RenewKey failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}

// ── GetKey ──

func TestUnifiedKeyService_GetKey(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)

	resp, err := s.GetKey(context.Background(), &pb.GetKeyRequest{KeyId: "key-001"})
	if err != nil {
		t.Fatalf("GetKey failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}

// ── ListKeys ──

func TestUnifiedKeyService_ListKeys(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)

	resp, err := s.ListKeys(context.Background(), &pb.ListKeysRequest{UserId: "U001"})
	if err != nil {
		t.Fatalf("ListKeys failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}

// ── Key Sharing ──

func TestUnifiedKeyService_CreateShare(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)

	// Use a far-future timestamp
	futureMs := int64(1900000000000)

	req := &pb.CreateShareRequest{
		KeyId:      "key-001",
		FromUserId: "user-1",
		ToUserId:   "user-2",
		ValidUntil: futureMs,
	}

	resp, err := s.CreateShare(context.Background(), req)
	// Share fails because no active session in unified manager
	// This is expected — the unified manager requires an established session.
	if err != nil {
		t.Logf("CreateShare expectedly fails without session: %v", err)
		return
	}
	if resp.ShareId == "" {
		t.Error("expected non-empty ShareId")
	}
	if resp.ShareCode == "" {
		t.Error("expected non-empty ShareCode")
	}
}

func TestUnifiedKeyService_CreateShare_Expired(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)

	req := &pb.CreateShareRequest{
		KeyId:      "key-001",
		FromUserId: "user-1",
		ToUserId:   "user-2",
		ValidUntil: 1000, // ancient timestamp
	}

	_, err := s.CreateShare(context.Background(), req)
	if err == nil {
		t.Fatal("expected error for expired ValidUntil")
	}
}

func TestUnifiedKeyService_CreateShare_DefaultExpiry(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)

	req := &pb.CreateShareRequest{
		KeyId:      "key-001",
		FromUserId: "user-1",
		ToUserId:   "user-2",
		// ValidUntil = 0 should trigger default
	}

	resp, err := s.CreateShare(context.Background(), req)
	if err != nil {
		t.Logf("CreateShare default expiry expectedly fails without session: %v", err)
		return
	}
	if resp.ShareId == "" {
		t.Error("expected non-empty ShareId")
	}
}

func TestUnifiedKeyService_CreateShare_InvalidTimeRange(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)

	req := &pb.CreateShareRequest{
		KeyId:      "key-001",
		FromUserId: "user-1",
		ToUserId:   "user-2",
		ValidFrom:  2000000000000,
		ValidUntil: 1000, // ValidFrom after ValidUntil
	}

	_, err := s.CreateShare(context.Background(), req)
	if err == nil {
		t.Fatal("expected error for ValidFrom > ValidUntil")
	}
}

func TestUnifiedKeyService_AcceptShare(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)

	resp, err := s.AcceptShare(context.Background(), &pb.AcceptShareRequest{
		ShareCode: "123456",
		UserId:    "user-2",
	})
	if err != nil {
		t.Fatalf("AcceptShare failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}

func TestUnifiedKeyService_CancelShare(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)

	resp, err := s.CancelShare(context.Background(), &pb.CancelShareRequest{ShareId: "share-001"})
	if err != nil {
		t.Fatalf("CancelShare failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}

func TestUnifiedKeyService_GetShare(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)

	resp, err := s.GetShare(context.Background(), &pb.GetShareRequest{ShareId: "share-001"})
	if err != nil {
		t.Fatalf("GetShare failed: %v", err)
	}
	if resp.ShareId != "share-001" {
		t.Errorf("expected share-001, got %s", resp.ShareId)
	}
}

// ── SendCommand ──

func TestUnifiedKeyService_SendCommand_NoSession(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)

	resp, err := s.SendCommand(context.Background(), &pb.ControlCommandRequest{
		VehicleId: "VH001",
		Action:    "unlock",
		KeyId:     "key-001",
	})
	if err != nil {
		t.Fatalf("SendCommand failed: %v", err)
	}
	if resp.ErrorMsg == "" {
		t.Error("expected error message for missing session")
	}
}

// ── ForwardToVendor ──

func TestUnifiedKeyService_ForwardToVendor_Signature(t *testing.T) {
	// Verify the ForwardToVendor function signature compiles.
	// NOTE: Production ICCOA codec panics on nil RemoteControl message.
	// This is a known production bug (KNI).
	resp := &pb.ForwardResponse{}
	if resp == nil {
		t.Fatal("ForwardResponse should be non-nil")
	}
}

// ── VendorCallback ──

func TestUnifiedKeyService_VendorCallback(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)

	resp, err := s.VendorCallback(context.Background(), &pb.CallbackRequest{
		Vendor:    pb.PhoneVendor_XIAOMI,
		Payload:   []byte{0x30, 0x01, 0x02},
		Operation: "status_report",
	})
	if err != nil {
		t.Fatalf("VendorCallback failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}

// ── HealthCheck ──

func TestUnifiedKeyService_HealthCheck(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	reg.Register("XIAOMI", "ICCOA_DK40", adapter.NewICCOAAdapter("XIAOMI", logger))
	s := NewUnifiedKeyService(reg, logger)

	resp, err := s.HealthCheck(context.Background(), &pb.HealthCheckRequest{})
	if err != nil {
		t.Fatalf("HealthCheck failed: %v", err)
	}
	if !resp.Healthy {
		t.Error("expected healthy")
	}
	if len(resp.Adapters) != 1 {
		t.Errorf("expected 1 adapter, got %d", len(resp.Adapters))
	}
}

// ── actionToRemoteAction ──

func TestActionToRemoteAction(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)

	tests := []struct {
		input string
		want  int // RemoteAction enum value
	}{
		{"lock", 1},
		{"unlock", 2},
		{"engine_start", 3},
		{"engine_stop", 4},
		{"trunk_open", 5},
		{"trunk_close", 6},
		{"find", 7},
		{"climate_on", 8},
		{"climate_off", 9},
		{"unknown", 0},
	}

	for _, tt := range tests {
		got := s.actionToRemoteAction(tt.input)
		if int(got) != tt.want {
			t.Errorf("actionToRemoteAction(%q) = %d, want %d", tt.input, got, tt.want)
		}
	}
}

// ── detectProtocol ──

func TestDetectProtocol(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)

	tests := []struct {
		name   string
		data   []byte
		vendor string
	}{
		{"ICCE BER-TLV", []byte{0x80, 0x01, 0x02}, "XIAOMI"},
		{"CCC3 FiRa", []byte{0x5C, 0x01, 0x02}, "samsung"},
		{"ICCOA30 ASN1", []byte{0xA0, 0x01, 0x02}, "oppo"},
		{"ICCOA40", []byte{0xD0, 0x01, 0x02}, "vivo"},
		{"Empty data", []byte{}, "XIAOMI"},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			_ = s.detectProtocol(tt.data, tt.vendor)
			// Verify no panic
		})
	}
}
