package service

import (
	"context"
	"errors"
	"fmt"
	"sync"
	"testing"
	"time"

	"go.uber.org/zap"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"

	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/adapter"
)

// ── InMemoryKeyStore Tests ──

func TestNewInMemoryKeyStore(t *testing.T) {
	ks := NewInMemoryKeyStore()
	if ks == nil {
		t.Fatal("NewInMemoryKeyStore returned nil")
	}
	if ks.data == nil {
		t.Fatal("expected initialized data map")
	}
}

func TestInMemoryKeyStore_SetAndGetKey(t *testing.T) {
	ks := NewInMemoryKeyStore()
	ctx := context.Background()

	rec := &KeyRecord{
		KeyID:       "key-001",
		OwnerUserID: "user-1",
		VehicleID:   "VH001",
		Vendor:      "XIAOMI",
		Status:      "active",
		CreatedAt:   time.Now().UnixMilli(),
	}

	if err := ks.SetKey(ctx, rec); err != nil {
		t.Fatalf("SetKey failed: %v", err)
	}

	got, err := ks.GetKeyRecord(ctx, "key-001")
	if err != nil {
		t.Fatalf("GetKeyRecord failed: %v", err)
	}
	if got.OwnerUserID != "user-1" {
		t.Errorf("expected owner user-1, got %s", got.OwnerUserID)
	}
	if got.KeyID != "key-001" {
		t.Errorf("expected key-001, got %s", got.KeyID)
	}
}

func TestInMemoryKeyStore_GetKeyRecord_NotFound(t *testing.T) {
	ks := NewInMemoryKeyStore()
	_, err := ks.GetKeyRecord(context.Background(), "nonexistent")
	if err == nil {
		t.Fatal("expected error for nonexistent key")
	}
}

func TestInMemoryKeyStore_GetKeyOwner(t *testing.T) {
	ks := NewInMemoryKeyStore()
	ctx := context.Background()

	ks.SetKey(ctx, &KeyRecord{KeyID: "key-001", OwnerUserID: "user-1", Status: "active"})

	owner, err := ks.GetKeyOwner(ctx, "key-001")
	if err != nil {
		t.Fatalf("GetKeyOwner failed: %v", err)
	}
	if owner != "user-1" {
		t.Errorf("expected user-1, got %s", owner)
	}
}

func TestInMemoryKeyStore_GetKeyOwner_NotFound(t *testing.T) {
	ks := NewInMemoryKeyStore()
	_, err := ks.GetKeyOwner(context.Background(), "nonexistent")
	if err == nil {
		t.Fatal("expected error for nonexistent key")
	}
}

func TestInMemoryKeyStore_GetKeyStatus(t *testing.T) {
	ks := NewInMemoryKeyStore()
	ctx := context.Background()

	ks.SetKey(ctx, &KeyRecord{KeyID: "key-001", Status: "suspended", OwnerUserID: "u1"})

	status, err := ks.GetKeyStatus(ctx, "key-001")
	if err != nil {
		t.Fatalf("GetKeyStatus failed: %v", err)
	}
	if status != "suspended" {
		t.Errorf("expected suspended, got %s", status)
	}
}

func TestInMemoryKeyStore_GetKeyStatus_NotFound(t *testing.T) {
	ks := NewInMemoryKeyStore()
	_, err := ks.GetKeyStatus(context.Background(), "nonexistent")
	if err == nil {
		t.Fatal("expected error for nonexistent key")
	}
}

func TestInMemoryKeyStore_SetKeyStatus(t *testing.T) {
	ks := NewInMemoryKeyStore()
	ctx := context.Background()

	ks.SetKey(ctx, &KeyRecord{KeyID: "key-001", OwnerUserID: "u1", Status: "active"})
	if err := ks.SetKeyStatus(ctx, "key-001", "revoked"); err != nil {
		t.Fatalf("SetKeyStatus failed: %v", err)
	}

	got, _ := ks.GetKeyRecord(ctx, "key-001")
	if got.Status != "revoked" {
		t.Errorf("expected revoked, got %s", got.Status)
	}
}

func TestInMemoryKeyStore_SetKeyStatus_NotFound(t *testing.T) {
	ks := NewInMemoryKeyStore()
	err := ks.SetKeyStatus(context.Background(), "nonexistent", "revoked")
	if err == nil {
		t.Fatal("expected error for nonexistent key")
	}
}

func TestInMemoryKeyStore_ListKeysByUser(t *testing.T) {
	ks := NewInMemoryKeyStore()
	ctx := context.Background()

	ks.SetKey(ctx, &KeyRecord{KeyID: "key-001", OwnerUserID: "user-1", VehicleID: "VH001"})
	ks.SetKey(ctx, &KeyRecord{KeyID: "key-002", OwnerUserID: "user-1", VehicleID: "VH002"})
	ks.SetKey(ctx, &KeyRecord{KeyID: "key-003", OwnerUserID: "user-2", VehicleID: "VH003"})

	keys, err := ks.ListKeysByUser(ctx, "user-1")
	if err != nil {
		t.Fatalf("ListKeysByUser failed: %v", err)
	}
	if len(keys) != 2 {
		t.Fatalf("expected 2 keys for user-1, got %d", len(keys))
	}
}

func TestInMemoryKeyStore_ListKeysByUser_Empty(t *testing.T) {
	ks := NewInMemoryKeyStore()
	keys, err := ks.ListKeysByUser(context.Background(), "nobody")
	if err != nil {
		t.Fatalf("ListKeysByUser failed: %v", err)
	}
	if len(keys) != 0 {
		t.Errorf("expected 0 keys, got %d", len(keys))
	}
}

func TestInMemoryKeyStore_SetKey_Immutability(t *testing.T) {
	ks := NewInMemoryKeyStore()
	ctx := context.Background()

	original := &KeyRecord{KeyID: "key-001", OwnerUserID: "user-1", Status: "active"}
	ks.SetKey(ctx, original)

	// Modify the original pointer after set
	original.Status = "revoked"

	got, _ := ks.GetKeyRecord(ctx, "key-001")
	if got.Status != "active" {
		t.Error("expected SetKey to store a copy, not a reference")
	}
}

func TestInMemoryKeyStore_Concurrency(t *testing.T) {
	ks := NewInMemoryKeyStore()
	ctx := context.Background()

	var wg sync.WaitGroup
	for i := 0; i < 100; i++ {
		wg.Add(1)
		go func(n int) {
			defer wg.Done()
			keyID := fmt.Sprintf("key-%03d", n)
			_ = ks.SetKey(ctx, &KeyRecord{KeyID: keyID, OwnerUserID: "u1", Status: "active"})
			_, _ = ks.GetKeyRecord(ctx, keyID)
			_, _ = ks.GetKeyOwner(ctx, keyID)
			_, _ = ks.GetKeyStatus(ctx, keyID)
			_ = ks.SetKeyStatus(ctx, keyID, "revoked")
		}(i)
	}
	wg.Wait()

	// Verify store still consistent
	keys, _ := ks.ListKeysByUser(ctx, "u1")
	if len(keys) != 100 {
		t.Errorf("expected 100 keys after concurrent ops, got %d", len(keys))
	}
}

// ── PushService Mock ──

type mockPushService struct {
	sendFunc func(ctx context.Context, userID string, payload *PushPayload) error
}

func (m *mockPushService) SendPush(ctx context.Context, userID string, payload *PushPayload) error {
	if m.sendFunc != nil {
		return m.sendFunc(ctx, userID, payload)
	}
	return nil
}

// ── KeyManagementService Tests ──

func newTestKeyMgmtService() *KeyManagementService {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	return NewKeyManagementService(reg, logger)
}

func TestNewKeyManagementService(t *testing.T) {
	s := newTestKeyMgmtService()
	if s == nil {
		t.Fatal("NewKeyManagementService returned nil")
	}
	if s.keyStore == nil {
		t.Error("expected non-nil keyStore")
	}
}

func TestKeyManagementService_WithKeyStore(t *testing.T) {
	s := newTestKeyMgmtService()
	custom := NewInMemoryKeyStore()
	result := s.WithKeyStore(custom)
	if result != s {
		t.Error("expected WithKeyStore to return the same service")
	}
	if s.keyStore != custom {
		t.Error("expected custom keyStore to be set")
	}
}

func TestKeyManagementService_WithPushService(t *testing.T) {
	s := newTestKeyMgmtService()
	ps := &mockPushService{}
	result := s.WithPushService(ps)
	if result != s {
		t.Error("expected WithPushService to return the same service")
	}
	if s.pushService != ps {
		t.Error("expected pushService to be set")
	}
}

// ── extractUserFromContext ──

func TestExtractUserFromContext_WithMetadata(t *testing.T) {
	md := metadata.New(map[string]string{
		"user_id":   "user-1",
		"user_role": "admin",
	})
	ctx := metadata.NewIncomingContext(context.Background(), md)

	userID, role := extractUserFromContext(ctx)
	if userID != "user-1" {
		t.Errorf("expected user-1, got %s", userID)
	}
	if role != "admin" {
		t.Errorf("expected admin, got %s", role)
	}
}

func TestExtractUserFromContext_NoMetadata(t *testing.T) {
	userID, role := extractUserFromContext(context.Background())
	if userID != "" || role != "" {
		t.Errorf("expected empty, got user=%s role=%s", userID, role)
	}
}

func TestExtractUserFromContext_PartialMetadata(t *testing.T) {
	md := metadata.New(map[string]string{
		"user_id": "user-1",
	})
	ctx := metadata.NewIncomingContext(context.Background(), md)

	userID, role := extractUserFromContext(ctx)
	if userID != "user-1" {
		t.Errorf("expected user-1, got %s", userID)
	}
	if role != "" {
		t.Errorf("expected empty role, got %s", role)
	}
}

func TestIsAdminUser(t *testing.T) {
	if !isAdminUser("admin") {
		t.Error("expected admin to be admin")
	}
	if isAdminUser("user") {
		t.Error("expected user to not be admin")
	}
	if isAdminUser("") {
		t.Error("expected empty to not be admin")
	}
}

// ── verifyKeyOwnership ──

func TestVerifyKeyOwnership_OwnsKey(t *testing.T) {
	s := newTestKeyMgmtService()
	ctx := context.Background()

	// Add a key
	ks := s.keyStore
	ks.SetKey(ctx, &KeyRecord{KeyID: "key-001", OwnerUserID: "user-1", Status: "active"})

	// Non-admin context
	md := metadata.New(map[string]string{"user_id": "user-1"})
	ctx2 := metadata.NewIncomingContext(ctx, md)

	if !s.verifyKeyOwnership(ctx2, "user-1", "key-001") {
		t.Error("expected user-1 to own key-001")
	}
}

func TestVerifyKeyOwnership_NotOwner(t *testing.T) {
	s := newTestKeyMgmtService()
	ctx := context.Background()

	ks := s.keyStore
	ks.SetKey(ctx, &KeyRecord{KeyID: "key-001", OwnerUserID: "user-1", Status: "active"})

	md := metadata.New(map[string]string{"user_id": "user-2"})
	ctx2 := metadata.NewIncomingContext(ctx, md)

	if s.verifyKeyOwnership(ctx2, "user-2", "key-001") {
		t.Error("expected user-2 to NOT own key-001")
	}
}

func TestVerifyKeyOwnership_AdminBypass(t *testing.T) {
	s := newTestKeyMgmtService()
	ctx := context.Background()

	md := metadata.New(map[string]string{"user_id": "admin-user", "user_role": "admin"})
	ctx2 := metadata.NewIncomingContext(ctx, md)

	if !s.verifyKeyOwnership(ctx2, "admin-user", "nonexistent") {
		t.Error("expected admin to bypass ownership check")
	}
}

func TestVerifyKeyOwnership_KeyNotFound(t *testing.T) {
	s := newTestKeyMgmtService()

	md := metadata.New(map[string]string{"user_id": "user-1"})
	ctx := metadata.NewIncomingContext(context.Background(), md)

	if s.verifyKeyOwnership(ctx, "user-1", "nonexistent") {
		t.Error("expected false for nonexistent key")
	}
}

// ── BindKey ──

func TestKeyManagementService_BindKey_AdapterNotFound(t *testing.T) {
	s := newTestKeyMgmtService()
	ctx := context.Background()

	req := &pb.BindKeyRequest{
		VehicleId: "VH001",
		UserId:    "user-1",
		Vendor:    pb.PhoneVendor_VENDOR_UNSPECIFIED,
	}
	resp, err := s.BindKey(ctx, req)
	if err != nil {
		t.Fatalf("BindKey failed: %v", err)
	}
	if resp.ErrorCode != "ADAPTER_NOT_FOUND" {
		t.Errorf("expected ADAPTER_NOT_FOUND, got %s", resp.ErrorCode)
	}
}

func TestKeyManagementService_BindKey_Success(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	// Register with uppercase vendor name matching proto enum String()
	reg.Register("XIAOMI", "ICCOA_DK40", adapter.NewICCOAAdapter("XIAOMI", logger))

	s := NewKeyManagementService(reg, logger)
	ctx := context.Background()

	req := &pb.BindKeyRequest{
		VehicleId:   "VH001",
		DeviceId:    "DEV001",
		UserId:      "U001",
		Vendor:      pb.PhoneVendor_XIAOMI,
		KeyType:     pb.KeyType_OWNER,
		AccessLevel: &pb.AccessLevel{Lock: true, Unlock: true, Engine: true},
	}

	resp, err := s.BindKey(ctx, req)
	if err != nil {
		t.Fatalf("BindKey failed: %v", err)
	}
	if resp.ErrorCode != "" {
		t.Errorf("expected empty error code, got %s", resp.ErrorCode)
	}
	if resp.Key == nil {
		t.Fatal("expected non-nil key")
	}
}

// ── UnbindKey ──

func TestKeyManagementService_UnbindKey_NoAuth(t *testing.T) {
	s := newTestKeyMgmtService()
	_, err := s.UnbindKey(context.Background(), &pb.UnbindKeyRequest{KeyId: "key-001"})
	if err == nil {
		t.Fatal("expected error for no auth")
	}
	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.Unauthenticated {
		t.Errorf("expected Unauthenticated code, got %v", st.Code())
	}
}

func TestKeyManagementService_UnbindKey_NotOwner(t *testing.T) {
	s := newTestKeyMgmtService()
	ctx := context.Background()

	// Add a key owned by user-1
	s.keyStore.SetKey(ctx, &KeyRecord{KeyID: "key-001", OwnerUserID: "user-1"})

	// Request as user-2
	md := metadata.New(map[string]string{"user_id": "user-2"})
	ctx2 := metadata.NewIncomingContext(ctx, md)

	_, err := s.UnbindKey(ctx2, &pb.UnbindKeyRequest{KeyId: "key-001"})
	if err == nil {
		t.Fatal("expected PermissionDenied for non-owner")
	}
	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.PermissionDenied {
		t.Errorf("expected PermissionDenied, got %v", st.Code())
	}
}

func TestKeyManagementService_UnbindKey_NoAdapter(t *testing.T) {
	s := newTestKeyMgmtService()
	ctx := context.Background()

	s.keyStore.SetKey(ctx, &KeyRecord{KeyID: "key-001", OwnerUserID: "user-1", Vendor: "unknown"})

	md := metadata.New(map[string]string{"user_id": "user-1"})
	ctx2 := metadata.NewIncomingContext(ctx, md)

	resp, err := s.UnbindKey(ctx2, &pb.UnbindKeyRequest{KeyId: "key-001"})
	if err != nil {
		t.Fatalf("UnbindKey failed: %v", err)
	}
	if resp.ErrorCode != "SUCCESS_NO_ADAPTER" {
		t.Errorf("expected SUCCESS_NO_ADAPTER, got %s", resp.ErrorCode)
	}
}

func TestKeyManagementService_UnbindKey_Success(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	reg.Register("XIAOMI", "ICCOA_DK40", adapter.NewICCOAAdapter("XIAOMI", logger))

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
	if resp.ErrorCode != "" {
		t.Errorf("expected empty error code, got %s", resp.ErrorCode)
	}
}

// ── SuspendKey ──

func TestKeyManagementService_SuspendKey_NoAuth(t *testing.T) {
	s := newTestKeyMgmtService()
	_, err := s.SuspendKey(context.Background(), &pb.SuspendKeyRequest{KeyId: "key-001"})
	if err == nil {
		t.Fatal("expected error for no auth")
	}
}

func TestKeyManagementService_SuspendKey_NotOwner(t *testing.T) {
	s := newTestKeyMgmtService()
	ctx := context.Background()
	s.keyStore.SetKey(ctx, &KeyRecord{KeyID: "key-001", OwnerUserID: "user-1"})

	md := metadata.New(map[string]string{"user_id": "user-2"})
	ctx2 := metadata.NewIncomingContext(ctx, md)

	_, err := s.SuspendKey(ctx2, &pb.SuspendKeyRequest{KeyId: "key-001"})
	if err == nil {
		t.Fatal("expected PermissionDenied")
	}
}

func TestKeyManagementService_SuspendKey_Success(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	reg.Register("XIAOMI", "ICCOA_DK40", adapter.NewICCOAAdapter("XIAOMI", logger))

	s := NewKeyManagementService(reg, logger)
	ctx := context.Background()
	s.keyStore.SetKey(ctx, &KeyRecord{
		KeyID: "key-001", OwnerUserID: "user-1", Vendor: "XIAOMI", VehicleID: "VH001",
	})

	md := metadata.New(map[string]string{"user_id": "user-1"})
	ctx2 := metadata.NewIncomingContext(ctx, md)

	_, err := s.SuspendKey(ctx2, &pb.SuspendKeyRequest{KeyId: "key-001", Reason: "test"})
	if err != nil {
		t.Fatalf("SuspendKey failed: %v", err)
	}

	// Verify status changed
	rec, _ := s.keyStore.GetKeyRecord(ctx, "key-001")
	if rec.Status != "suspended" {
		t.Errorf("expected suspended, got %s", rec.Status)
	}
}

// ── ResumeKey ──

func TestKeyManagementService_ResumeKey_Success(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	reg.Register("XIAOMI", "ICCOA_DK40", adapter.NewICCOAAdapter("XIAOMI", logger))

	s := NewKeyManagementService(reg, logger)
	ctx := context.Background()
	s.keyStore.SetKey(ctx, &KeyRecord{
		KeyID: "key-001", OwnerUserID: "user-1", Vendor: "XIAOMI", VehicleID: "VH001",
	})

	md := metadata.New(map[string]string{"user_id": "user-1"})
	ctx2 := metadata.NewIncomingContext(ctx, md)

	_, err := s.ResumeKey(ctx2, &pb.ResumeKeyRequest{KeyId: "key-001"})
	if err != nil {
		t.Fatalf("ResumeKey failed: %v", err)
	}

	rec, _ := s.keyStore.GetKeyRecord(ctx, "key-001")
	if rec.Status != "active" {
		t.Errorf("expected active, got %s", rec.Status)
	}
}

// ── 状态流转 (ICCOA 四态: 未激活→已激活→已冻结→已删除) ──

// 流转: active → suspended (SuspendKey) → active (ResumeKey)
func TestKeyManagementService_StateFlow_ActiveSuspendedActive(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	reg.Register("XIAOMI", "ICCOA_DK40", adapter.NewICCOAAdapter("XIAOMI", logger))

	s := NewKeyManagementService(reg, logger)
	ctx := context.Background()
	s.keyStore.SetKey(ctx, &KeyRecord{
		KeyID: "key-001", OwnerUserID: "user-1", Vendor: "XIAOMI", VehicleID: "VH001",
		Status: KeyStatusActive,
	})

	md := metadata.New(map[string]string{"user_id": "user-1"})
	ctx2 := metadata.NewIncomingContext(ctx, md)

	// active → suspended
	if _, err := s.SuspendKey(ctx2, &pb.SuspendKeyRequest{KeyId: "key-001", Reason: "trip"}); err != nil {
		t.Fatalf("SuspendKey failed: %v", err)
	}
	rec, _ := s.keyStore.GetKeyRecord(ctx, "key-001")
	if rec.Status != KeyStatusSuspended {
		t.Fatalf("expected suspended after SuspendKey, got %s", rec.Status)
	}

	// suspended → active
	if _, err := s.ResumeKey(ctx2, &pb.ResumeKeyRequest{KeyId: "key-001"}); err != nil {
		t.Fatalf("ResumeKey failed: %v", err)
	}
	rec, _ = s.keyStore.GetKeyRecord(ctx, "key-001")
	if rec.Status != KeyStatusActive {
		t.Fatalf("expected active after ResumeKey, got %s", rec.Status)
	}
}

// 流转: active → terminated (RevokeKey, ICCOA"已删除")
func TestKeyManagementService_StateFlow_ActiveTerminated(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	reg.Register("XIAOMI", "ICCOA_DK40", adapter.NewICCOAAdapter("XIAOMI", logger))

	s := NewKeyManagementService(reg, logger)
	ctx := context.Background()
	s.keyStore.SetKey(ctx, &KeyRecord{
		KeyID: "key-001", OwnerUserID: "user-1", Vendor: "XIAOMI", VehicleID: "VH001",
		Status: KeyStatusActive,
	})

	md := metadata.New(map[string]string{"user_id": "user-1"})
	ctx2 := metadata.NewIncomingContext(ctx, md)

	if _, err := s.RevokeKey(ctx2, &pb.RevokeKeyRequest{KeyId: "key-001", Reason: "stolen"}); err != nil {
		t.Fatalf("RevokeKey failed: %v", err)
	}
	rec, _ := s.keyStore.GetKeyRecord(ctx, "key-001")
	if rec.Status != KeyStatusTerminated {
		t.Fatalf("expected terminated after RevokeKey, got %s", rec.Status)
	}
	// terminated 为终态: 后续 resume 不改变状态语义由上层策略保证, 此处仅验证不再回到 active
	if rec.Status == KeyStatusActive {
		t.Fatal("terminated key must not be active")
	}
}

// ── RevokeKey ──

func TestKeyManagementService_RevokeKey_NoAuth(t *testing.T) {
	s := newTestKeyMgmtService()
	_, err := s.RevokeKey(context.Background(), &pb.RevokeKeyRequest{KeyId: "key-001"})
	if err == nil {
		t.Fatal("expected error for no auth")
	}
}

func TestKeyManagementService_RevokeKey_Success(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	reg.Register("XIAOMI", "ICCOA_DK40", adapter.NewICCOAAdapter("XIAOMI", logger))

	s := NewKeyManagementService(reg, logger)
	ctx := context.Background()
	s.keyStore.SetKey(ctx, &KeyRecord{
		KeyID: "key-001", OwnerUserID: "user-1", Vendor: "XIAOMI", VehicleID: "VH001",
	})

	md := metadata.New(map[string]string{"user_id": "user-1"})
	ctx2 := metadata.NewIncomingContext(ctx, md)

	resp, err := s.RevokeKey(ctx2, &pb.RevokeKeyRequest{KeyId: "key-001", Reason: "stolen"})
	if err != nil {
		t.Fatalf("RevokeKey failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}

	rec, _ := s.keyStore.GetKeyRecord(ctx, "key-001")
	if rec.Status != KeyStatusTerminated {
		t.Errorf("expected terminated, got %s", rec.Status)
	}
}

// ── RevokeKey with push service ──

func TestKeyManagementService_RevokeKey_WithPushService(t *testing.T) {
	pushCalled := false
	ps := &mockPushService{
		sendFunc: func(ctx context.Context, userID string, payload *PushPayload) error {
			pushCalled = true
			if payload.Type != "key_revoked" {
				t.Errorf("expected key_revoked type, got %s", payload.Type)
			}
			return nil
		},
	}

	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	reg.Register("XIAOMI", "ICCOA_DK40", adapter.NewICCOAAdapter("XIAOMI", logger))

	s := NewKeyManagementService(reg, logger)
	s.WithPushService(ps)

	ctx := context.Background()
	s.keyStore.SetKey(ctx, &KeyRecord{
		KeyID: "key-001", OwnerUserID: "user-1", Vendor: "XIAOMI", VehicleID: "VH001",
	})

	md := metadata.New(map[string]string{"user_id": "user-1"})
	ctx2 := metadata.NewIncomingContext(ctx, md)

	_, err := s.RevokeKey(ctx2, &pb.RevokeKeyRequest{KeyId: "key-001", Reason: "stolen"})
	if err != nil {
		t.Fatalf("RevokeKey failed: %v", err)
	}
	if !pushCalled {
		t.Error("expected push service to be called")
	}
}

func TestKeyManagementService_RevokeKey_PushServiceError_NonBlocking(t *testing.T) {
	ps := &mockPushService{
		sendFunc: func(ctx context.Context, userID string, payload *PushPayload) error {
			return errors.New("push failed")
		},
	}

	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	reg.Register("XIAOMI", "ICCOA_DK40", adapter.NewICCOAAdapter("XIAOMI", logger))

	s := NewKeyManagementService(reg, logger)
	s.WithPushService(ps)

	ctx := context.Background()
	s.keyStore.SetKey(ctx, &KeyRecord{
		KeyID: "key-001", OwnerUserID: "user-1", Vendor: "XIAOMI", VehicleID: "VH001",
	})

	md := metadata.New(map[string]string{"user_id": "user-1"})
	ctx2 := metadata.NewIncomingContext(ctx, md)

	_, err := s.RevokeKey(ctx2, &pb.RevokeKeyRequest{KeyId: "key-001", Reason: "stolen"})
	if err != nil {
		t.Fatalf("RevokeKey should not fail on push error: %v", err)
	}
}

func TestKeyManagementService_RevokeKey_NoPushService(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	reg.Register("XIAOMI", "ICCOA_DK40", adapter.NewICCOAAdapter("XIAOMI", logger))

	s := NewKeyManagementService(reg, logger)
	// No push service set

	ctx := context.Background()
	s.keyStore.SetKey(ctx, &KeyRecord{
		KeyID: "key-001", OwnerUserID: "user-1", Vendor: "XIAOMI", VehicleID: "VH001",
	})

	md := metadata.New(map[string]string{"user_id": "user-1"})
	ctx2 := metadata.NewIncomingContext(ctx, md)

	_, err := s.RevokeKey(ctx2, &pb.RevokeKeyRequest{KeyId: "key-001", Reason: "stolen"})
	if err != nil {
		t.Fatalf("RevokeKey should not fail: %v", err)
	}
}

// ── notifyPhoneRevocation ──

func TestNotifyPhoneRevocation_NoPushService(t *testing.T) {
	s := newTestKeyMgmtService()
	err := s.notifyPhoneRevocation(context.Background(), "user-1", "key-001")
	if err != nil {
		t.Fatalf("notifyPhoneRevocation should not fail when push is nil: %v", err)
	}
}

func TestNotifyPhoneRevocation_Success(t *testing.T) {
	ps := &mockPushService{
		sendFunc: func(ctx context.Context, userID string, payload *PushPayload) error {
			return nil
		},
	}

	s := newTestKeyMgmtService()
	s.WithPushService(ps)

	err := s.notifyPhoneRevocation(context.Background(), "user-1", "key-001")
	if err != nil {
		t.Fatalf("notifyPhoneRevocation failed: %v", err)
	}
}

func TestNotifyPhoneRevocation_PushError(t *testing.T) {
	ps := &mockPushService{
		sendFunc: func(ctx context.Context, userID string, payload *PushPayload) error {
			return errors.New("network error")
		},
	}

	s := newTestKeyMgmtService()
	s.WithPushService(ps)

	err := s.notifyPhoneRevocation(context.Background(), "user-1", "key-001")
	if err == nil {
		t.Fatal("expected error from push service")
	}
}

// ── RenewKey ──

func TestKeyManagementService_RenewKey_NoAuth(t *testing.T) {
	s := newTestKeyMgmtService()
	_, err := s.RenewKey(context.Background(), &pb.RenewKeyRequest{KeyId: "key-001"})
	if err == nil {
		t.Fatal("expected error for no auth")
	}
}

func TestKeyManagementService_RenewKey_Success(t *testing.T) {
	s := newTestKeyMgmtService()
	ctx := context.Background()
	s.keyStore.SetKey(ctx, &KeyRecord{KeyID: "key-001", OwnerUserID: "user-1"})

	md := metadata.New(map[string]string{"user_id": "user-1"})
	ctx2 := metadata.NewIncomingContext(ctx, md)

	resp, err := s.RenewKey(ctx2, &pb.RenewKeyRequest{KeyId: "key-001", ValidUntil: time.Now().Add(365 * 24 * time.Hour).UnixMilli()})
	if err != nil {
		t.Fatalf("RenewKey failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}

// ── GetKey ──

func TestKeyManagementService_GetKey_NoAuth(t *testing.T) {
	s := newTestKeyMgmtService()
	_, err := s.GetKey(context.Background(), &pb.GetKeyRequest{KeyId: "key-001"})
	if err == nil {
		t.Fatal("expected error for no auth")
	}
}

func TestKeyManagementService_GetKey_Success(t *testing.T) {
	s := newTestKeyMgmtService()
	ctx := context.Background()
	s.keyStore.SetKey(ctx, &KeyRecord{KeyID: "key-001", OwnerUserID: "user-1"})

	md := metadata.New(map[string]string{"user_id": "user-1"})
	ctx2 := metadata.NewIncomingContext(ctx, md)

	resp, err := s.GetKey(ctx2, &pb.GetKeyRequest{KeyId: "key-001"})
	if err != nil {
		t.Fatalf("GetKey failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}

// ── ListKeys ──

func TestKeyManagementService_ListKeys_NoAuth(t *testing.T) {
	s := newTestKeyMgmtService()
	_, err := s.ListKeys(context.Background(), &pb.ListKeysRequest{UserId: "user-1"})
	if err == nil {
		t.Fatal("expected error for no auth")
	}
}

func TestKeyManagementService_ListKeys_AdminCanListAny(t *testing.T) {
	s := newTestKeyMgmtService()
	ctx := context.Background()
	s.keyStore.SetKey(ctx, &KeyRecord{KeyID: "key-001", OwnerUserID: "other-user"})

	md := metadata.New(map[string]string{"user_id": "admin-user", "user_role": "admin"})
	ctx2 := metadata.NewIncomingContext(ctx, md)

	resp, err := s.ListKeys(ctx2, &pb.ListKeysRequest{UserId: "other-user"})
	if err != nil {
		t.Fatalf("ListKeys failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}

func TestKeyManagementService_ListKeys_NonAdminCannotListOther(t *testing.T) {
	s := newTestKeyMgmtService()

	md := metadata.New(map[string]string{"user_id": "user-1"})
	ctx := metadata.NewIncomingContext(context.Background(), md)

	_, err := s.ListKeys(ctx, &pb.ListKeysRequest{UserId: "other-user"})
	if err == nil {
		t.Fatal("expected PermissionDenied for non-admin listing other user")
	}
}

func TestKeyManagementService_ListKeys_OwnKeys(t *testing.T) {
	s := newTestKeyMgmtService()
	ctx := context.Background()
	s.keyStore.SetKey(ctx, &KeyRecord{KeyID: "key-001", OwnerUserID: "user-1", VehicleID: "VH001"})

	md := metadata.New(map[string]string{"user_id": "user-1"})
	ctx2 := metadata.NewIncomingContext(ctx, md)

	resp, err := s.ListKeys(ctx2, &pb.ListKeysRequest{UserId: "user-1"})
	if err != nil {
		t.Fatalf("ListKeys failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}

// ── InMemoryKeyStore implements KeyStore ──

func TestInMemoryKeyStore_ImplementsKeyStore(t *testing.T) {
	var _ KeyStore = (*InMemoryKeyStore)(nil)
}

// ── GetKey / ListKeys real-data population ──

// failingRecordStore wraps InMemoryKeyStore and forces GetKeyRecord to fail,
// simulating a store where ownership lookup succeeds but the record load
// fails (exercises the GetKey NotFound path).
type failingRecordStore struct {
	*InMemoryKeyStore
}

func (f *failingRecordStore) GetKeyRecord(_ context.Context, _ string) (*KeyRecord, error) {
	return nil, fmt.Errorf("simulated record load failure")
}

func TestKeyManagementService_GetKey_ReturnsStoredRecord(t *testing.T) {
	s := newTestKeyMgmtService()
	ctx := context.Background()
	createdAt := time.Now().UnixMilli()
	s.keyStore.SetKey(ctx, &KeyRecord{
		KeyID:       "key-001",
		OwnerUserID: "user-1",
		VehicleID:   "VH001",
		Vendor:      "APPLE",
		Status:      "active",
		CreatedAt:   createdAt,
	})

	md := metadata.New(map[string]string{"user_id": "user-1"})
	ctx2 := metadata.NewIncomingContext(ctx, md)

	resp, err := s.GetKey(ctx2, &pb.GetKeyRequest{KeyId: "key-001"})
	if err != nil {
		t.Fatalf("GetKey failed: %v", err)
	}
	if resp.GetKey() == nil {
		t.Fatal("expected non-nil key in response")
	}
	got := resp.GetKey()
	if got.KeyId != "key-001" {
		t.Errorf("expected KeyId key-001, got %s", got.KeyId)
	}
	if got.UserId != "user-1" {
		t.Errorf("expected UserId user-1, got %s", got.UserId)
	}
	if got.VehicleId != "VH001" {
		t.Errorf("expected VehicleId VH001, got %s", got.VehicleId)
	}
	if got.Status != pb.KeyStatus_ACTIVE {
		t.Errorf("expected Status ACTIVE, got %s", got.Status)
	}
	if got.CreatedAt != createdAt {
		t.Errorf("expected CreatedAt %d, got %d", createdAt, got.CreatedAt)
	}
}

func TestKeyManagementService_GetKey_RecordLoadFailure(t *testing.T) {
	s := newTestKeyMgmtService()
	ctx := context.Background()
	// Ownership check succeeds (key exists, user owns it)…
	s.keyStore.SetKey(ctx, &KeyRecord{KeyID: "key-001", OwnerUserID: "user-1"})
	// …but the record load fails.
	s.keyStore = &failingRecordStore{InMemoryKeyStore: NewInMemoryKeyStore()}
	s.keyStore.SetKey(ctx, &KeyRecord{KeyID: "key-001", OwnerUserID: "user-1"})

	md := metadata.New(map[string]string{"user_id": "user-1"})
	ctx2 := metadata.NewIncomingContext(ctx, md)

	_, err := s.GetKey(ctx2, &pb.GetKeyRequest{KeyId: "key-001"})
	if err == nil {
		t.Fatal("expected NotFound error")
	}
	if status.Code(err) != codes.NotFound {
		t.Errorf("expected codes.NotFound, got %v", status.Code(err))
	}
}

func TestKeyManagementService_GetKey_UnknownVendorNoPanic(t *testing.T) {
	s := newTestKeyMgmtService()
	ctx := context.Background()
	s.keyStore.SetKey(ctx, &KeyRecord{
		KeyID: "key-001", OwnerUserID: "user-1",
		Vendor: "MEGACORP", // not in PhoneVendor enum
		Status: "pending",  // no KeyStatus enum value
	})

	md := metadata.New(map[string]string{"user_id": "user-1"})
	ctx2 := metadata.NewIncomingContext(ctx, md)

	resp, err := s.GetKey(ctx2, &pb.GetKeyRequest{KeyId: "key-001"})
	if err != nil {
		t.Fatalf("GetKey failed: %v", err)
	}
	if resp.GetKey() == nil || resp.GetKey().KeyId != "key-001" {
		t.Fatal("expected populated key despite unknown vendor/status")
	}
	if resp.GetKey().Status != pb.KeyStatus_KEY_STATUS_UNSPECIFIED {
		t.Errorf("expected UNSPECIFIED status for 'pending', got %s", resp.GetKey().Status)
	}
}

func TestKeyManagementService_ListKeys_ReturnsRecords(t *testing.T) {
	s := newTestKeyMgmtService()
	ctx := context.Background()
	s.keyStore.SetKey(ctx, &KeyRecord{
		KeyID: "key-001", OwnerUserID: "user-1", VehicleID: "VH001",
		Vendor: "XIAOMI", Status: "active",
	})
	s.keyStore.SetKey(ctx, &KeyRecord{
		KeyID: "key-002", OwnerUserID: "user-1", VehicleID: "VH002",
		Vendor: "APPLE", Status: "suspended",
	})
	s.keyStore.SetKey(ctx, &KeyRecord{
		KeyID: "key-003", OwnerUserID: "user-2", VehicleID: "VH003",
		Vendor: "APPLE", Status: "revoked",
	})

	md := metadata.New(map[string]string{"user_id": "user-1"})
	ctx2 := metadata.NewIncomingContext(ctx, md)

	resp, err := s.ListKeys(ctx2, &pb.ListKeysRequest{UserId: "user-1"})
	if err != nil {
		t.Fatalf("ListKeys failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
	if len(resp.Keys) != 2 {
		t.Fatalf("expected 2 keys, got %d", len(resp.Keys))
	}
	if resp.Total != 2 {
		t.Errorf("expected Total 2, got %d", resp.Total)
	}

	// Verify mapped fields on each returned key
	byID := map[string]*pb.DigitalKey{}
	for _, k := range resp.Keys {
		byID[k.KeyId] = k
	}
	k1, ok := byID["key-001"]
	if !ok {
		t.Fatal("expected key-001 in response")
	}
	if k1.UserId != "user-1" || k1.VehicleId != "VH001" || k1.Status != pb.KeyStatus_ACTIVE {
		t.Errorf("key-001 mapping wrong: %+v", k1)
	}
	k2, ok := byID["key-002"]
	if !ok {
		t.Fatal("expected key-002 in response")
	}
	if k2.VehicleId != "VH002" || k2.Status != pb.KeyStatus_SUSPENDED {
		t.Errorf("key-002 mapping wrong: %+v", k2)
	}
	// Other user's key must not leak into the list
	if _, leak := byID["key-003"]; leak {
		t.Error("key-003 owned by user-2 leaked into user-1's list")
	}
}

func TestKeyManagementService_ListKeys_AdminRespectsUserFilter(t *testing.T) {
	s := newTestKeyMgmtService()
	ctx := context.Background()
	s.keyStore.SetKey(ctx, &KeyRecord{KeyID: "key-001", OwnerUserID: "user-1"})
	s.keyStore.SetKey(ctx, &KeyRecord{KeyID: "key-002", OwnerUserID: "user-2"})
	s.keyStore.SetKey(ctx, &KeyRecord{KeyID: "key-003", OwnerUserID: "user-2"})

	md := metadata.New(map[string]string{"user_id": "admin-user", "user_role": "admin"})
	ctx2 := metadata.NewIncomingContext(ctx, md)

	resp, err := s.ListKeys(ctx2, &pb.ListKeysRequest{UserId: "user-2"})
	if err != nil {
		t.Fatalf("ListKeys failed: %v", err)
	}
	if len(resp.Keys) != 2 {
		t.Fatalf("expected 2 keys for user-2, got %d", len(resp.Keys))
	}
	if resp.Total != 2 {
		t.Errorf("expected Total 2, got %d", resp.Total)
	}
	for _, k := range resp.Keys {
		if k.UserId != "user-2" {
			t.Errorf("expected only user-2 keys, got %s", k.UserId)
		}
	}
}

func TestKeyManagementService_ListKeys_Empty(t *testing.T) {
	s := newTestKeyMgmtService()
	ctx := context.Background()

	md := metadata.New(map[string]string{"user_id": "user-1"})
	ctx2 := metadata.NewIncomingContext(ctx, md)

	resp, err := s.ListKeys(ctx2, &pb.ListKeysRequest{})
	if err != nil {
		t.Fatalf("ListKeys failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
	if len(resp.Keys) != 0 {
		t.Errorf("expected 0 keys, got %d", len(resp.Keys))
	}
	if resp.Total != 0 {
		t.Errorf("expected Total 0, got %d", resp.Total)
	}
}

// ── mapping helpers ──

func TestKeyStatusFromString(t *testing.T) {
	cases := []struct {
		in   string
		want pb.KeyStatus
	}{
		{"active", pb.KeyStatus_ACTIVE},
		{"suspended", pb.KeyStatus_SUSPENDED},
		{"revoked", pb.KeyStatus_REVOKED},
		{"expired", pb.KeyStatus_EXPIRED},
		{"terminated", pb.KeyStatus_TERMINATED},
		// "pending" has no KeyStatus enum value in hub.proto → UNSPECIFIED
		{"pending", pb.KeyStatus_KEY_STATUS_UNSPECIFIED},
		{"", pb.KeyStatus_KEY_STATUS_UNSPECIFIED},
		{"bogus", pb.KeyStatus_KEY_STATUS_UNSPECIFIED},
	}
	for _, c := range cases {
		if got := keyStatusFromString(c.in); got != c.want {
			t.Errorf("keyStatusFromString(%q) = %v, want %v", c.in, got, c.want)
		}
	}
}

func TestPhoneVendorFromString(t *testing.T) {
	cases := []struct {
		in   string
		want pb.PhoneVendor
	}{
		{"APPLE", pb.PhoneVendor_APPLE},
		{"SAMSUNG", pb.PhoneVendor_SAMSUNG},
		{"XIAOMI", pb.PhoneVendor_XIAOMI},
		{"OPPO", pb.PhoneVendor_OPPO},
		{"VIVO", pb.PhoneVendor_VIVO},
		{"HUAWEI", pb.PhoneVendor_HUAWEI},
		// Unknown names must not panic; map to UNSPECIFIED
		{"MEGACORP", pb.PhoneVendor_VENDOR_UNSPECIFIED},
		{"", pb.PhoneVendor_VENDOR_UNSPECIFIED},
	}
	for _, c := range cases {
		if got := phoneVendorFromString(c.in); got != c.want {
			t.Errorf("phoneVendorFromString(%q) = %v, want %v", c.in, got, c.want)
		}
	}
}

func TestKeyRecordToDigitalKey_Nil(t *testing.T) {
	if got := keyRecordToDigitalKey(nil); got != nil {
		t.Errorf("expected nil for nil record, got %+v", got)
	}
}
