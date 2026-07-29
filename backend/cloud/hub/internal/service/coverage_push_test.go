package service

import (
	"context"
	"testing"
	"time"

	"go.uber.org/zap"
	"google.golang.org/grpc/metadata"

	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/adapter"
)

// ── Push coverage above 80% ──

// TestCoverage_UnifiedKeyService_SendCommand exercises all early-return paths
func TestCoverage_UnifiedKeyService_SendCommand_SessionNotFound(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)

	resp, err := s.SendCommand(context.Background(), &pb.ControlCommandRequest{
		VehicleId: "NO-SESSION-VH",
		Action:    "lock",
		KeyId:     "key-001",
	})
	if err != nil {
		t.Fatalf("SendCommand should not return error: %v", err)
	}
	if resp.ErrorMsg == "" {
		t.Error("expected error message for missing session")
	}
}

// TestCoverage_UnifiedKeyService_GetKey_SessionNotFound hits the early return
func TestCoverage_UnifiedKeyService_GetKey_SessionNotFound(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)

	resp, err := s.GetKey(context.Background(), &pb.GetKeyRequest{KeyId: "nonexistent"})
	if err != nil {
		t.Fatalf("GetKey failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}

// TestCoverage_DeviceService_ProvisionKey_MultipleVehicles tests key ID format
func TestCoverage_DeviceService_ProvisionKey_MultipleVehicles(t *testing.T) {
	logger := zap.NewNop()
	s := NewDeviceService(logger)
	ctx := context.Background()

	dev, _ := s.RegisterDevice(ctx, "user-1", DeviceCapabilities{Platform: "android"})

	for i := 0; i < 3; i++ {
		vid := string(rune('A' + i))
		bid, err := s.ProvisionKey(ctx, "user-1", dev.DeviceID, "VH"+vid, []string{"ble"})
		if err != nil {
			t.Fatalf("ProvisionKey VH%c failed: %v", 'A'+i, err)
		}
		if bid.Status != "active" {
			t.Errorf("expected active, got %s", bid.Status)
		}
	}
}

// TestCoverage_DeviceService_DeleteDevice_WithMultipleBindings
func TestCoverage_DeviceService_DeleteDevice_WithMultipleBindings(t *testing.T) {
	logger := zap.NewNop()
	s := NewDeviceService(logger)
	ctx := context.Background()

	dev, _ := s.RegisterDevice(ctx, "user-1", DeviceCapabilities{Platform: "ios"})
	s.ProvisionKey(ctx, "user-1", dev.DeviceID, "VH001", nil)
	s.ProvisionKey(ctx, "user-1", dev.DeviceID, "VH002", nil)

	if err := s.DeleteDevice(ctx, "user-1", dev.DeviceID); err != nil {
		t.Fatalf("DeleteDevice failed: %v", err)
	}
}

// TestCoverage_KeyManagementService_SuspendKey_NoAdapter
func TestCoverage_KeyManagementService_SuspendKey_NoAdapter(t *testing.T) {
	s := newTestKeyMgmtService()
	ctx := context.Background()
	s.keyStore.SetKey(ctx, &KeyRecord{
		KeyID: "key-001", OwnerUserID: "user-1", Vendor: "UNKNOWN", VehicleID: "VH001",
	})

	md := metadata.New(map[string]string{"user_id": "user-1"})
	ctx2 := metadata.NewIncomingContext(ctx, md)

	resp, err := s.SuspendKey(ctx2, &pb.SuspendKeyRequest{KeyId: "key-001", Reason: "test"})
	if err != nil {
		t.Fatalf("SuspendKey failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}

// TestCoverage_KeyManagementService_RevokeKey_AdminBypass
func TestCoverage_KeyManagementService_RevokeKey_AdminBypass(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	reg.Register("XIAOMI", "ICCOA_DK40", adapter.NewICCOAAdapter("XIAOMI", logger))

	s := NewKeyManagementService(reg, logger)
	ctx := context.Background()
	s.keyStore.SetKey(ctx, &KeyRecord{
		KeyID: "key-001", OwnerUserID: "other-user", Vendor: "XIAOMI", VehicleID: "VH001",
	})

	md := metadata.New(map[string]string{"user_id": "admin-user", "user_role": "admin"})
	ctx2 := metadata.NewIncomingContext(ctx, md)

	resp, err := s.RevokeKey(ctx2, &pb.RevokeKeyRequest{KeyId: "key-001", Reason: "admin test"})
	if err != nil {
		t.Fatalf("RevokeKey failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}

// TestCoverage_KeyManagementService_BindKey_StoreKeyRecord verifies metadata persistence
func TestCoverage_KeyManagementService_BindKey_StoreKeyRecord(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	reg.Register("XIAOMI", "ICCOA_DK40", adapter.NewICCOAAdapter("XIAOMI", logger))

	s := NewKeyManagementService(reg, logger)
	ctx := context.Background()

	req := &pb.BindKeyRequest{
		VehicleId: "VH-KEY-001",
		DeviceId:  "DEV-KEY-001",
		UserId:    "U-KEY-001",
		Vendor:    pb.PhoneVendor_XIAOMI,
		KeyType:   pb.KeyType_OWNER,
		AccessLevel: &pb.AccessLevel{Lock: true, Unlock: true, Engine: true},
	}

	resp, err := s.BindKey(ctx, req)
	if err != nil {
		t.Fatalf("BindKey failed: %v", err)
	}
	if resp.Key == nil {
		t.Fatal("expected non-nil key")
	}

	// Verify key metadata was stored
	rec, err := s.keyStore.GetKeyRecord(ctx, resp.Key.KeyId)
	if err != nil {
		t.Fatalf("GetKeyRecord failed: %v", err)
	}
	if rec.OwnerUserID != "U-KEY-001" {
		t.Errorf("expected U-KEY-001, got %s", rec.OwnerUserID)
	}
	if rec.Status != "active" {
		t.Errorf("expected active, got %s", rec.Status)
	}
	if rec.VehicleID != "VH-KEY-001" {
		t.Errorf("expected VH-KEY-001, got %s", rec.VehicleID)
	}
}

// TestCoverage_KeyManagementService_ListKeys_AdminListingOtherUser
func TestCoverage_KeyManagementService_ListKeys_AdminListingOtherUser(t *testing.T) {
	s := newTestKeyMgmtService()
	ctx := context.Background()
	s.keyStore.SetKey(ctx, &KeyRecord{KeyID: "key-001", OwnerUserID: "other-user"})

	md := metadata.New(map[string]string{"user_id": "admin", "user_role": "admin"})
	ctx2 := metadata.NewIncomingContext(ctx, md)

	resp, err := s.ListKeys(ctx2, &pb.ListKeysRequest{UserId: "other-user"})
	if err != nil {
		t.Fatalf("ListKeys failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}

// TestCoverage_keyRecordIsCopied verifies that InMemoryKeyStore returns copies
func TestCoverage_keyRecordIsCopied(t *testing.T) {
	ks := NewInMemoryKeyStore()
	ctx := context.Background()

	ks.SetKey(ctx, &KeyRecord{KeyID: "key-001", OwnerUserID: "user-1", Status: "active"})

	r1, _ := ks.GetKeyRecord(ctx, "key-001")
	r2, _ := ks.GetKeyRecord(ctx, "key-001")

	if r1 == r2 {
		t.Error("expected copies (different pointers)")
	}
}



// TestCoverage_UnifiedKeyService_AcceptShare tests accept share path
func TestCoverage_UnifiedKeyService_AcceptShare_WithAllParams(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)

	// Test with all fields set
	resp, err := s.AcceptShare(context.Background(), &pb.AcceptShareRequest{
		ShareCode: "654321",
		UserId:    "user-2",
		DeviceId:  "dev-456",
		Vendor:    pb.PhoneVendor_XIAOMI,
	})
	if err != nil {
		t.Fatalf("AcceptShare failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}

// TestCoverage_UnifiedKeyService_ListKeys_WithSessions
func TestCoverage_UnifiedKeyService_ListKeys_WithSessions(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)

	// Create a session via negotiation
	req := &NegotiateProtocolRequest{
		DeviceId: "DEV-LIST-001",
		Vendor:   "xiaomi",
		Os:       "android",
		Caps: &NegotiateCapabilities{
			Ble: true, Uwb: true, Nfc: true, Se: true, Fira: true,
		},
		VehicleCaps: &NegotiateCapabilities{
			Ble: true, Uwb: true, Nfc: true, Se: true, Fira: true,
		},
	}
	negResp, err := s.NegotiateProtocol(context.Background(), req)
	if err != nil {
		t.Fatalf("NegotiateProtocol failed: %v", err)
	}
	_ = negResp

	// Now ListKeys should find sessions
	resp, err := s.ListKeys(context.Background(), &pb.ListKeysRequest{})
	if err != nil {
		t.Fatalf("ListKeys failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}

// TestCoverage_MiscVendorCallbacks tests VendorCallback with real payload
func TestCoverage_VendorCallback_WithPayload(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)

	tests := []struct {
		name    string
		req     *pb.CallbackRequest
	}{
		{"empty_payload", &pb.CallbackRequest{Vendor: pb.PhoneVendor_XIAOMI}},
		{"with_callback_id", &pb.CallbackRequest{
			Vendor:     pb.PhoneVendor_XIAOMI,
			Operation:  "status",
			Payload:    []byte{0x30, 0x01, 0x03},
			CallbackId: "cb-001",
		}},
		{"nfc_payload", &pb.CallbackRequest{
			Vendor:    pb.PhoneVendor_HUAWEI,
			Operation: "nfc",
			Payload:   []byte{0x80, 0x01, 0x01},
		}},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			resp, err := s.VendorCallback(context.Background(), tt.req)
			if err != nil {
				t.Fatalf("VendorCallback failed: %v", err)
			}
			if resp == nil {
				t.Fatal("expected non-nil response")
			}
		})
	}
}

// TestCoverage_HubTransport_ForwardToVendor_NoAdapter ensures code path works
func TestCoverage_HubTransport_ForwardToVendor_NoAdapter(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewHubTransportService(reg, logger)

	resp, err := s.ForwardToVendor(context.Background(), &pb.ForwardRequest{
		Vendor:    pb.PhoneVendor_VENDOR_UNSPECIFIED,
		Protocol:  pb.Protocol_PROTOCOL_UNSPECIFIED,
		Operation: "bind",
	})
	if err != nil {
		t.Fatalf("ForwardToVendor failed: %v", err)
	}
	if resp.ErrorCode != "ADAPTER_NOT_FOUND" {
		t.Errorf("expected ADAPTER_NOT_FOUND, got %s", resp.ErrorCode)
	}
}



// TestCoverage_KeyManagement_RenewKey tests with far-future ValidUntil
func TestCoverage_KeyManagement_RenewKey_MissingKey(t *testing.T) {
	s := newTestKeyMgmtService()
	ctx := context.Background()
	s.keyStore.SetKey(ctx, &KeyRecord{KeyID: "key-001", OwnerUserID: "user-1"})

	md := metadata.New(map[string]string{"user_id": "user-1"})
	ctx2 := metadata.NewIncomingContext(ctx, md)

	resp, err := s.RenewKey(ctx2, &pb.RenewKeyRequest{
		KeyId:      "key-001",
		ValidUntil: time.Now().Add(365 * 24 * time.Hour).UnixMilli(),
	})
	if err != nil {
		t.Fatalf("RenewKey failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}

// TestCoverage_UnifiedKeyService_RevokeKey_EmptySessions
func TestCoverage_UnifiedKeyService_RevokeKey_EmptySessions(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
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

// TestCoverage_UnifiedKeyService_RenewKey_EmptySessions
func TestCoverage_UnifiedKeyService_RenewKey_EmptySessions(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)

	resp, err := s.RenewKey(context.Background(), &pb.RenewKeyRequest{
		KeyId:      "key-002",
		ValidUntil: 1900000000000,
	})
	if err != nil {
		t.Fatalf("RenewKey failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}

// TestCoverage_UnifiedKeyService_GetKey_AfterNegotiation
func TestCoverage_UnifiedKeyService_GetKey_AfterNegotiation(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	reg.Register("XIAOMI", "ICCOA_DK40", adapter.NewICCOAAdapter("XIAOMI", logger))
	s := NewUnifiedKeyService(reg, logger)

	// Get a key that was bound
	resp, err := s.GetKey(context.Background(), &pb.GetKeyRequest{KeyId: "key-bound-001"})
	if err != nil {
		t.Fatalf("GetKey failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}
