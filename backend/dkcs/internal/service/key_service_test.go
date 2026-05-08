package service

import (
	"context"
	"testing"
	"time"

	"github.com/digitalkey/dkcs/internal/repository"
)

// ─────────────────────────────────────────────────────────────
// Mock Implementations
// ─────────────────────────────────────────────────────────────

type mockKeyRepo struct {
	keys       map[string]*repository.Key
	createErr  error
	getErr     error
	updateErr  error
	listResult []*repository.Key
	listErr    error
}

func newMockKeyRepo() *mockKeyRepo {
	return &mockKeyRepo{keys: make(map[string]*repository.Key)}
}

func (m *mockKeyRepo) Create(ctx context.Context, key *repository.Key) error {
	if m.createErr != nil {
		return m.createErr
	}
	m.keys[key.ID] = key
	return nil
}

func (m *mockKeyRepo) GetByID(ctx context.Context, id string) (*repository.Key, error) {
	if m.getErr != nil {
		return nil, m.getErr
	}
	k, ok := m.keys[id]
	if !ok {
		return nil, repository.ErrKeyNotFound
	}
	return k, nil
}

func (m *mockKeyRepo) Update(ctx context.Context, key *repository.Key) error {
	if m.updateErr != nil {
		return m.updateErr
	}
	m.keys[key.ID] = key
	return nil
}

func (m *mockKeyRepo) Delete(ctx context.Context, id string) error {
	delete(m.keys, id)
	return nil
}

func (m *mockKeyRepo) ListByUser(ctx context.Context, userID string, limit, offset int) ([]*repository.Key, error) {
	if m.listErr != nil {
		return nil, m.listErr
	}
	return m.listResult, nil
}

func (m *mockKeyRepo) ListByVehicle(ctx context.Context, vehicleID string, limit, offset int) ([]*repository.Key, error) {
	if m.listErr != nil {
		return nil, m.listErr
	}
	return m.listResult, nil
}

type mockVehicleRepo struct {
	vehicles map[string]*repository.Vehicle
	getErr   error
}

func newMockVehicleRepo() *mockVehicleRepo {
	return &mockVehicleRepo{vehicles: make(map[string]*repository.Vehicle)}
}

func (m *mockVehicleRepo) GetByID(ctx context.Context, id string) (*repository.Vehicle, error) {
	if m.getErr != nil {
		return nil, m.getErr
	}
	v, ok := m.vehicles[id]
	if !ok {
		return nil, repository.ErrVehicleNotFound
	}
	return v, nil
}

func (m *mockVehicleRepo) Create(ctx context.Context, v *repository.Vehicle) error {
	m.vehicles[v.ID] = v
	return nil
}

func (m *mockVehicleRepo) Update(ctx context.Context, v *repository.Vehicle) error {
	m.vehicles[v.ID] = v
	return nil
}

type mockLogger struct{}

func (m *mockLogger) Info(msg string, fields ...interface{})  {}
func (m *mockLogger) Error(msg string, fields ...interface{}) {}
func (m *mockLogger) Debug(msg string, fields ...interface{}) {}

type mockTelemetry struct {
	counters map[string]int
}

func newMockTelemetry() *mockTelemetry {
	return &mockTelemetry{counters: make(map[string]int)}
}

func (m *mockTelemetry) IncCounter(name string, labels map[string]string) {
	m.counters[name]++
}

func (m *mockTelemetry) RecordDuration(name string, d time.Duration) {}

// ─────────────────────────────────────────────────────────────
// Repository error definitions (add these so tests compile)
// ─────────────────────────────────────────────────────────────

var (
	ErrKeyNotFound    = repository.ErrKeyNotFound
	ErrVehicleNotFound = repository.ErrVehicleNotFound
)

func init() {
	// Ensure error is registered
	_ = repository.ErrKeyNotFound
}

// ─────────────────────────────────────────────────────────────
// Helper: build service with mocks
// ─────────────────────────────────────────────────────────────

func buildTestKeyService(keyRepo *mockKeyRepo, vehicleRepo *mockVehicleRepo) *KeyService {
	return NewKeyService(
		keyRepo,
		vehicleRepo,
		&mockLogger{},
		newMockTelemetry(),
	)
}

// ─────────────────────────────────────────────────────────────
// CreateKey Tests
// ─────────────────────────────────────────────────────────────

func TestCreateKey_MissingVehicleID(t *testing.T) {
	svc := buildTestKeyService(newMockKeyRepo(), newMockVehicleRepo())
	ctx := context.Background()

	_, err := svc.CreateKey(ctx, &pb.CreateKeyRequest{
		VehicleId: "", // missing
		UserId:    "user-001",
	})

	if err == nil {
		t.Fatal("expected error for missing vehicle_id")
	}
	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.InvalidArgument {
		t.Errorf("want InvalidArgument, got %v", st.Code())
	}
}

func TestCreateKey_MissingUserID(t *testing.T) {
	svc := buildTestKeyService(newMockKeyRepo(), newMockVehicleRepo())
	ctx := context.Background()

	_, err := svc.CreateKey(ctx, &pb.CreateKeyRequest{
		VehicleId: "vehicle-001",
		UserId:    "", // missing
	})

	if err == nil {
		t.Fatal("expected error for missing user_id")
	}
	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.InvalidArgument {
		t.Errorf("want InvalidArgument, got %v", st.Code())
	}
}

func TestCreateKey_VehicleNotFound(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	_, err := svc.CreateKey(ctx, &pb.CreateKeyRequest{
		VehicleId: "nonexistent-vehicle",
		UserId:    "user-001",
	})

	if err == nil {
		t.Fatal("expected NotFound error for nonexistent vehicle")
	}
	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.NotFound {
		t.Errorf("want NotFound, got %v", st.Code())
	}
}

func TestCreateKey_DBError(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	vehicleRepo.vehicles["vehicle-001"] = &repository.Vehicle{ID: "vehicle-001"}
	keyRepo.createErr = errors.New("db error")
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	_, err := svc.CreateKey(ctx, &pb.CreateKeyRequest{
		VehicleId: "vehicle-001",
		UserId:    "user-001",
	})

	if err == nil {
		t.Fatal("expected error for DB failure")
	}
	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.Internal {
		t.Errorf("want Internal, got %v", st.Code())
	}
}

func TestCreateKey_Success(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	vehicleRepo.vehicles["vehicle-001"] = &repository.Vehicle{ID: "vehicle-001"}
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	resp, err := svc.CreateKey(ctx, &pb.CreateKeyRequest{
		VehicleId: "vehicle-001",
		UserId:    "user-001",
		KeyType:   "primary",
	})

	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if resp.KeyId == "" {
		t.Error("KeyId should not be empty")
	}
	if resp.Secret == "" {
		t.Error("Secret should not be empty")
	}
	if resp.Status != "pending" {
		t.Errorf("want status 'pending', got '%s'", resp.Status)
	}
	if len(resp.Secret) != 64 {
		t.Errorf("Secret should be 64 hex chars (32 bytes), got %d", len(resp.Secret))
	}
}

// ─────────────────────────────────────────────────────────────
// ActivateKey Tests
// ─────────────────────────────────────────────────────────────

func TestActivateKey_NotFound(t *testing.T) {
	svc := buildTestKeyService(newMockKeyRepo(), newMockVehicleRepo())
	ctx := context.Background()

	_, err := svc.ActivateKey(ctx, &pb.ActivateKeyRequest{KeyId: "nonexistent"})

	if err == nil {
		t.Fatal("expected NotFound error")
	}
	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.NotFound {
		t.Errorf("want NotFound, got %v", st.Code())
	}
}

func TestActivateKey_WrongStatus(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	vehicleRepo.vehicles["v-001"] = &repository.Vehicle{ID: "v-001"}
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	// Pre-create an already-active key
	keyRepo.keys["key-active"] = &repository.Key{
		ID:        "key-active",
		VehicleID: "v-001",
		UserID:    "user-001",
		Status:    "active",
	}

	_, err := svc.ActivateKey(ctx, &pb.ActivateKeyRequest{KeyId: "key-active"})

	if err == nil {
		t.Fatal("expected FailedPrecondition error for already-active key")
	}
	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.FailedPrecondition {
		t.Errorf("want FailedPrecondition, got %v", st.Code())
	}
}

func TestActivateKey_Success(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	vehicleRepo.vehicles["v-001"] = &repository.Vehicle{ID: "v-001"}
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	keyRepo.keys["key-pending"] = &repository.Key{
		ID:        "key-pending",
		VehicleID: "v-001",
		UserID:    "user-001",
		Status:    "pending",
	}

	resp, err := svc.ActivateKey(ctx, &pb.ActivateKeyRequest{KeyId: "key-pending"})

	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if resp.Status != "active" {
		t.Errorf("want 'active', got '%s'", resp.Status)
	}
	if keyRepo.keys["key-pending"].Status != "active" {
		t.Error("key status should be updated to 'active'")
	}
	if keyRepo.keys["key-pending"].ActivatedAt.IsZero() {
		t.Error("ActivatedAt should be set")
	}
}

func TestActivateKey_UpdateError(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	vehicleRepo.vehicles["v-001"] = &repository.Vehicle{ID: "v-001"}
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	keyRepo.keys["key-update-err"] = &repository.Key{
		ID: "key-update-err", VehicleID: "v-001", UserID: "user-001", Status: "pending",
	}
	keyRepo.updateErr = errors.New("update failed")

	_, err := svc.ActivateKey(ctx, &pb.ActivateKeyRequest{KeyId: "key-update-err"})

	if err == nil {
		t.Fatal("expected error")
	}
	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.Internal {
		t.Errorf("want Internal, got %v", st.Code())
	}
}

// ─────────────────────────────────────────────────────────────
// RevokeKey Tests
// ─────────────────────────────────────────────────────────────

func TestRevokeKey_NotFound(t *testing.T) {
	svc := buildTestKeyService(newMockKeyRepo(), newMockVehicleRepo())
	ctx := context.Background()

	_, err := svc.RevokeKey(ctx, &pb.RevokeKeyRequest{KeyId: "nonexistent"})

	if err == nil {
		t.Fatal("expected NotFound error")
	}
	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.NotFound {
		t.Errorf("want NotFound, got %v", st.Code())
	}
}

func TestRevokeKey_AlreadyRevoked(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	keyRepo.keys["key-revoked"] = &repository.Key{
		ID:     "key-revoked",
		Status: "revoked",
	}

	_, err := svc.RevokeKey(ctx, &pb.RevokeKeyRequest{KeyId: "key-revoked"})

	if err == nil {
		t.Fatal("expected FailedPrecondition for already-revoked key")
	}
	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.FailedPrecondition {
		t.Errorf("want FailedPrecondition, got %v", st.Code())
	}
}

func TestRevokeKey_Success(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	keyRepo.keys["key-active"] = &repository.Key{
		ID:     "key-active",
		Status: "active",
	}

	resp, err := svc.RevokeKey(ctx, &pb.RevokeKeyRequest{
		KeyId:  "key-active",
		Reason: "user requested",
	})

	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if resp.Status != "revoked" {
		t.Errorf("want 'revoked', got '%s'", resp.Status)
	}
	if keyRepo.keys["key-active"].Status != "revoked" {
		t.Error("key status should be updated to 'revoked'")
	}
	if keyRepo.keys["key-active"].RevokeReason != "user requested" {
		t.Error("RevokeReason not set")
	}
	if keyRepo.keys["key-active"].RevokedAt.IsZero() {
		t.Error("RevokedAt should be set")
	}
}

// ─────────────────────────────────────────────────────────────
// GetKey Tests
// ─────────────────────────────────────────────────────────────

func TestGetKey_NotFound(t *testing.T) {
	svc := buildTestKeyService(newMockKeyRepo(), newMockVehicleRepo())
	ctx := context.Background()

	_, err := svc.GetKey(ctx, &pb.GetKeyRequest{KeyId: "nonexistent"})

	if err == nil {
		t.Fatal("expected NotFound error")
	}
}

func TestGetKey_Success(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	now := time.Now()
	keyRepo.keys["key-001"] = &repository.Key{
		ID:          "key-001",
		VehicleID:   "vehicle-001",
		UserID:      "user-001",
		KeyType:     "primary",
		Status:      "active",
		Permissions: []string{"unlock", "start"},
		CreatedAt:   now,
		ExpiresAt:   now.Add(365 * 24 * time.Hour),
	}

	resp, err := svc.GetKey(ctx, &pb.GetKeyRequest{KeyId: "key-001"})

	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if resp.KeyId != "key-001" {
		t.Errorf("KeyId: want 'key-001', got '%s'", resp.KeyId)
	}
	if resp.Status != "active" {
		t.Errorf("Status: want 'active', got '%s'", resp.Status)
	}
}

// ─────────────────────────────────────────────────────────────
// ListKeys Tests
// ─────────────────────────────────────────────────────────────

func TestListKeys_ByUser(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	now := time.Now()
	keyRepo.listResult = []*repository.Key{
		{ID: "key-001", UserID: "user-001", VehicleID: "v-001", Status: "active", CreatedAt: now},
		{ID: "key-002", UserID: "user-001", VehicleID: "v-002", Status: "active", CreatedAt: now},
	}

	resp, err := svc.ListKeys(ctx, &pb.ListKeysRequest{
		UserId: "user-001",
		Limit:  10,
		Offset: 0,
	})

	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if len(resp.Keys) != 2 {
		t.Errorf("want 2 keys, got %d", len(resp.Keys))
	}
}

func TestListKeys_ByVehicle(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	keyRepo.listResult = []*repository.Key{
		{ID: "key-003", VehicleID: "v-001", UserID: "user-001", Status: "active"},
	}

	resp, err := svc.ListKeys(ctx, &pb.ListKeysRequest{
		VehicleId: "v-001",
		Limit:     10,
		Offset:    0,
	})

	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if len(resp.Keys) != 1 {
		t.Errorf("want 1 key, got %d", len(resp.Keys))
	}
}

func TestListKeys_NeitherUserNorVehicle(t *testing.T) {
	svc := buildTestKeyService(newMockKeyRepo(), newMockVehicleRepo())
	ctx := context.Background()

	_, err := svc.ListKeys(ctx, &pb.ListKeysRequest{
		Limit:  10,
		Offset: 0,
	})

	if err == nil {
		t.Fatal("expected InvalidArgument error")
	}
	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.InvalidArgument {
		t.Errorf("want InvalidArgument, got %v", st.Code())
	}
}

func TestListKeys_DBError(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	keyRepo.listErr = errors.New("query error")

	_, err := svc.ListKeys(ctx, &pb.ListKeysRequest{
		UserId: "user-001",
		Limit:  10,
		Offset: 0,
	})

	if err == nil {
		t.Fatal("expected error")
	}
}

// ─────────────────────────────────────────────────────────────
// ShareKey Tests
// ─────────────────────────────────────────────────────────────

func TestShareKey_NotFound(t *testing.T) {
	svc := buildTestKeyService(newMockKeyRepo(), newMockVehicleRepo())
	ctx := context.Background()

	_, err := svc.ShareKey(ctx, &pb.ShareKeyRequest{
		KeyId:    "nonexistent",
		ToUserId: "user-002",
	})

	if err == nil {
		t.Fatal("expected NotFound error")
	}
	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.NotFound {
		t.Errorf("want NotFound, got %v", st.Code())
	}
}

func TestShareKey_NoSharePermission(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	keyRepo.keys["key-no-share"] = &repository.Key{
		ID:          "key-no-share",
		VehicleID:   "v-001",
		UserID:      "user-001",
		Status:      "active",
		Permissions: []string{"unlock"}, // no "share" permission
	}

	_, err := svc.ShareKey(ctx, &pb.ShareKeyRequest{
		KeyId:    "key-no-share",
		ToUserId: "user-002",
	})

	if err == nil {
		t.Fatal("expected PermissionDenied error")
	}
	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.PermissionDenied {
		t.Errorf("want PermissionDenied, got %v", st.Code())
	}
}

func TestShareKey_Success(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	origID := "key-orig"
	keyRepo.keys[origID] = &repository.Key{
		ID:          origID,
		VehicleID:   "v-001",
		UserID:      "user-001",
		Status:      "active",
		KeyType:     "primary",
		Permissions: []string{"unlock", "start", "share"},
	}

	resp, err := svc.ShareKey(ctx, &pb.ShareKeyRequest{
		KeyId:        origID,
		ToUserId:     "user-002",
		Permissions:  []string{"unlock"},
	})

	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if resp.SharedKeyId == "" {
		t.Error("SharedKeyId should not be empty")
	}
	if resp.ShareCode == "" {
		t.Error("ShareCode should not be empty")
	}
	if len(resp.ShareCode) != 6 {
		t.Errorf("ShareCode should be 6 digits, got '%s' (len=%d)", resp.ShareCode, len(resp.ShareCode))
	}
	if resp.ExpiresAt == 0 {
		t.Error("ExpiresAt should be set")
	}
	// Verify shared key was created
	if len(keyRepo.keys) != 2 {
		t.Errorf("want 2 keys (original + shared), got %d", len(keyRepo.keys))
	}
}

func TestShareKey_SharedKeyHasParent(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	keyRepo.keys["key-parent"] = &repository.Key{
		ID:          "key-parent",
		VehicleID:   "v-001",
		UserID:      "user-001",
		Status:      "active",
		Permissions: []string{"share"},
	}

	resp, err := svc.ShareKey(ctx, &pb.ShareKeyRequest{
		KeyId:    "key-parent",
		ToUserId: "user-002",
	})

	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	// Verify shared key references parent
	sharedKey := keyRepo.keys[resp.SharedKeyId]
	if sharedKey.ParentKeyID == nil || *sharedKey.ParentKeyID != "key-parent" {
		t.Error("shared key should reference parent key")
	}
}

// ─────────────────────────────────────────────────────────────
// Permission Matrix Tests
// ─────────────────────────────────────────────────────────────

func TestKey_HasPermission(t *testing.T) {
	key := &repository.Key{
		Permissions: []string{"unlock", "start", "share"},
	}

	if !key.HasPermission("unlock") {
		t.Error("should have 'unlock' permission")
	}
	if !key.HasPermission("start") {
		t.Error("should have 'start' permission")
	}
	if key.HasPermission("admin") {
		t.Error("should NOT have 'admin' permission")
	}
}

func TestKey_HasPermission_All(t *testing.T) {
	key := &repository.Key{
		Permissions: []string{"all"},
	}

	if !key.HasPermission("unlock") {
		t.Error("'all' permission should grant 'unlock'")
	}
	if !key.HasPermission("start") {
		t.Error("'all' permission should grant 'start'")
	}
	if !key.HasPermission("share") {
		t.Error("'all' permission should grant 'share'")
	}
}

func TestKey_HasPermission_Empty(t *testing.T) {
	key := &repository.Key{
		Permissions: []string{},
	}

	if key.HasPermission("unlock") {
		t.Error("empty permissions should grant nothing")
	}
}
