package service

import (
	"context"
	"testing"

	"go.uber.org/zap"

	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/adapter"
)

// mkKeyShareService 构建注入 InMemoryShareStore + keyStore 的 KeyShareService。
func mkKeyShareService(t *testing.T, registerAdapter bool) *KeyShareService {
	t.Helper()
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	if registerAdapter {
		reg.Register("XIAOMI", "ICCOA_DK40", adapter.NewICCOAAdapter("XIAOMI", logger))
	}
	s := NewKeyShareService(reg, logger)
	s.WithShareStore(NewInMemoryShareStore())
	s.WithKeyStore(NewInMemoryKeyStore())
	return s
}

func TestNewKeyShareService(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewKeyShareService(reg, logger)
	if s == nil {
		t.Fatal("NewKeyShareService returned nil")
	}
}

func TestKeyShareService_CreateShare_NoAdapter(t *testing.T) {
	s := mkKeyShareService(t, false)
	ctx := context.Background()
	_ = s.keyStore.SetKey(ctx, &KeyRecord{
		KeyID: "key-001", OwnerUserID: "user-1", VehicleID: "VH001",
		Status: "active", AccessBits: PermBitAll,
	})

	resp, err := s.CreateShare(ctx, &pb.CreateShareRequest{
		KeyId:      "key-001",
		FromUserId: "user-1",
		ToVendor:   pb.PhoneVendor_VENDOR_UNSPECIFIED,
	})
	if err != nil {
		t.Fatalf("CreateShare failed: %v", err)
	}
	if resp.ErrorCode != "ADAPTER_NOT_FOUND" {
		t.Errorf("expected ADAPTER_NOT_FOUND, got %s", resp.ErrorCode)
	}
}

func TestKeyShareService_CreateShare_KeyNotFound(t *testing.T) {
	s := mkKeyShareService(t, true)
	_, err := s.CreateShare(context.Background(), &pb.CreateShareRequest{
		KeyId:      "key-missing",
		FromUserId: "user-1",
		ToVendor:   pb.PhoneVendor_XIAOMI,
	})
	if err == nil {
		t.Fatal("expected error for missing key")
	}
}

func TestKeyShareService_CreateShare_Success(t *testing.T) {
	s := mkKeyShareService(t, true)
	ctx := context.Background()
	_ = s.keyStore.SetKey(ctx, &KeyRecord{
		KeyID: "key-001", OwnerUserID: "user-1", VehicleID: "VH001",
		Status: "active", AccessBits: PermBitAll,
	})

	resp, err := s.CreateShare(ctx, &pb.CreateShareRequest{
		KeyId:      "key-001",
		FromUserId: "user-1",
		ToVendor:   pb.PhoneVendor_XIAOMI,
	})
	if err != nil {
		t.Fatalf("CreateShare failed: %v", err)
	}
	if resp.ShareId == "" {
		t.Error("expected non-empty ShareId")
	}
	if resp.ShareCode == "" {
		t.Error("expected non-empty ShareCode")
	}
}

func TestKeyShareService_AcceptShare_InvalidCode(t *testing.T) {
	s := mkKeyShareService(t, true)
	_, err := s.AcceptShare(context.Background(), &pb.AcceptShareRequest{
		ShareCode: "000000",
		Vendor:    pb.PhoneVendor_XIAOMI,
	})
	if err == nil {
		t.Fatal("expected error for invalid share code")
	}
}

// TestKeyShareService_ShareLifecycle 完整状态机: create → accept → get(ACCEPTED)
// → 重复 accept 拒绝 → cancel → get(CANCELLED) → cancel 再拒绝。
func TestKeyShareService_ShareLifecycle(t *testing.T) {
	s := mkKeyShareService(t, true)
	ctx := context.Background()
	_ = s.keyStore.SetKey(ctx, &KeyRecord{
		KeyID: "key-001", OwnerUserID: "user-1", VehicleID: "VH001",
		Status: "active", AccessBits: PermBitAll,
	})

	created, err := s.CreateShare(ctx, &pb.CreateShareRequest{
		KeyId: "key-001", FromUserId: "user-1", ToVendor: pb.PhoneVendor_XIAOMI,
		AccessLevel: &pb.AccessLevel{Lock: true, Unlock: true},
	})
	if err != nil {
		t.Fatalf("CreateShare failed: %v", err)
	}

	got, err := s.GetShare(ctx, &pb.GetShareRequest{ShareId: created.ShareId})
	if err != nil {
		t.Fatalf("GetShare failed: %v", err)
	}
	if got.Status != pb.ShareStatus_PENDING {
		t.Errorf("expected PENDING, got %s", got.Status)
	}

	accepted, err := s.AcceptShare(ctx, &pb.AcceptShareRequest{
		ShareCode:    created.ShareCode,
		UserId:       "user-2",
		Vendor:       pb.PhoneVendor_XIAOMI,
		DevicePubkey: []byte("BASE64PUBKEY"),
	})
	if err != nil {
		t.Fatalf("AcceptShare failed: %v", err)
	}
	if accepted.Key == nil || accepted.Key.KeyId == "" {
		t.Fatal("expected friend key from AcceptShare")
	}

	got2, _ := s.GetShare(ctx, &pb.GetShareRequest{ShareId: created.ShareId})
	if got2.Status != pb.ShareStatus_ACCEPTED {
		t.Errorf("expected ACCEPTED, got %s", got2.Status)
	}

	if _, err := s.AcceptShare(ctx, &pb.AcceptShareRequest{
		ShareCode: created.ShareCode, UserId: "user-3", Vendor: pb.PhoneVendor_XIAOMI,
	}); err == nil {
		t.Fatal("expected error for re-accept")
	}

	if _, err := s.CancelShare(ctx, &pb.CancelShareRequest{ShareId: created.ShareId}); err != nil {
		t.Fatalf("CancelShare failed: %v", err)
	}
	got3, _ := s.GetShare(ctx, &pb.GetShareRequest{ShareId: created.ShareId})
	if got3.Status != pb.ShareStatus_CANCELLED {
		t.Errorf("expected CANCELLED, got %s", got3.Status)
	}

	if _, err := s.CancelShare(ctx, &pb.CancelShareRequest{ShareId: created.ShareId}); err == nil {
		t.Fatal("expected error for re-cancel")
	}
}

func TestKeyShareService_CancelShare_NotFound(t *testing.T) {
	s := mkKeyShareService(t, false)
	_, err := s.CancelShare(context.Background(), &pb.CancelShareRequest{ShareId: "share-missing"})
	if err == nil {
		t.Fatal("expected error for missing share")
	}
}

func TestKeyShareService_GetShare_NotFound(t *testing.T) {
	s := mkKeyShareService(t, false)
	_, err := s.GetShare(context.Background(), &pb.GetShareRequest{ShareId: "share-missing"})
	if err == nil {
		t.Fatal("expected error for missing share")
	}
}
