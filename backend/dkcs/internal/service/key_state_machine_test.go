package service

import (
	"context"
	"testing"
	"time"

	pb "github.com/frisky1985/yuleDKCS/backend/dkcs/proto/dkcs"
	"github.com/frisky1985/yuleDKCS/backend/dkcs/internal/repository"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
)

// ──────────────────────────────────────────────────────────────────────────
// State Machine Unit Tests
// Each transition is a standalone test named:
//   TestKeyLifecycle_StateFrom_to_StateTo
// ──────────────────────────────────────────────────────────────────────────

// -------------------------------------------------------------------------
// VALID TRANSITIONS per CCC/ICCE protocol
// -------------------------------------------------------------------------

// TestKeyLifecycle_pending_to_active: Issued → Active
func TestKeyLifecycle_pending_to_active(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	vehicleRepo.vehicles["v-001"] = &repository.Vehicle{ID: "v-001"}
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	// Create key (status = pending)
	resp, err := svc.CreateKey(ctx, &pb.CreateKeyRequest{
		VehicleId: "v-001",
		UserId:    "user-001",
		KeyType:   "primary",
	})
	if err != nil {
		t.Fatalf("CreateKey failed: %v", err)
	}

	if resp.Status != "pending" {
		t.Fatalf("newly created key should be 'pending', got %q", resp.Status)
	}

	// Activate (pending → active)
	actResp, err := svc.ActivateKey(ctx, &pb.ActivateKeyRequest{KeyId: resp.KeyId})
	if err != nil {
		t.Fatalf("ActivateKey failed: %v", err)
	}
	if actResp.Status != "active" {
		t.Errorf("after activation, status should be 'active', got %q", actResp.Status)
	}

	// Verify persisted state
	key, err := keyRepo.GetByID(ctx, resp.KeyId)
	if err != nil {
		t.Fatalf("GetByID failed: %v", err)
	}
	if key.Status != "active" {
		t.Errorf("persisted key status: want 'active', got %q", key.Status)
	}
	if key.ActivatedAt == nil || key.ActivatedAt.IsZero() {
		t.Error("ActivatedAt should be set after activation")
	}
}

// TestKeyLifecycle_active_to_suspended: Active → Suspended
func TestKeyLifecycle_active_to_suspended(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	// Pre-create an active key
	keyID := "key-active-for-suspend"
	keyRepo.keys[keyID] = &repository.Key{
		ID:        keyID,
		VehicleID: "v-001",
		UserID:    "user-001",
		Status:    "active",
	}

	// Suspend (active → suspended)
	err := svc.SuspendKey(ctx, keyID)
	if err != nil {
		t.Fatalf("SuspendKey failed: %v", err)
	}

	// Verify persisted state
	key, err := keyRepo.GetByID(ctx, keyID)
	if err != nil {
		t.Fatalf("GetByID failed: %v", err)
	}
	if key.Status != "suspended" {
		t.Errorf("suspended key status: want 'suspended', got %q", key.Status)
	}
}

// TestKeyLifecycle_active_to_revoked: Active → Revoked
func TestKeyLifecycle_active_to_revoked(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	keyID := "key-active-for-revoke"
	keyRepo.keys[keyID] = &repository.Key{
		ID:     keyID,
		Status: "active",
	}

	resp, err := svc.RevokeKey(ctx, &pb.RevokeKeyRequest{
		KeyId:  keyID,
		Reason: "lost phone",
	})
	if err != nil {
		t.Fatalf("RevokeKey failed: %v", err)
	}
	if resp.Status != "revoked" {
		t.Errorf("after revoke, status should be 'revoked', got %q", resp.Status)
	}

	key, err := keyRepo.GetByID(ctx, keyID)
	if err != nil {
		t.Fatalf("GetByID failed: %v", err)
	}
	if key.Status != "revoked" {
		t.Errorf("persisted status: want 'revoked', got %q", key.Status)
	}
	if key.RevokeReason != "lost phone" {
		t.Errorf("RevokeReason: want 'lost phone', got %q", key.RevokeReason)
	}
	if key.RevokedAt == nil || key.RevokedAt.IsZero() {
		t.Error("RevokedAt should be set")
	}
}

// TestKeyLifecycle_suspended_to_active: Suspended → Active (resume)
func TestKeyLifecycle_suspended_to_active(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	keyID := "key-suspended-for-resume"
	keyRepo.keys[keyID] = &repository.Key{
		ID:     keyID,
		Status: "suspended",
	}

	// Resume (suspended → active)
	err := svc.ResumeKey(ctx, keyID)
	if err != nil {
		t.Fatalf("ResumeKey failed: %v", err)
	}

	key, err := keyRepo.GetByID(ctx, keyID)
	if err != nil {
		t.Fatalf("GetByID failed: %v", err)
	}
	if key.Status != "active" {
		t.Errorf("resumed key status: want 'active', got %q", key.Status)
	}
}

// TestKeyLifecycle_suspended_to_revoked: Suspended → Revoked
func TestKeyLifecycle_suspended_to_revoked(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	keyID := "key-suspended-for-revoke"
	keyRepo.keys[keyID] = &repository.Key{
		ID:     keyID,
		Status: "suspended",
	}

	resp, err := svc.RevokeKey(ctx, &pb.RevokeKeyRequest{
		KeyId:  keyID,
		Reason: "vehicle sold",
	})
	if err != nil {
		t.Fatalf("RevokeKey (from suspended) failed: %v", err)
	}
	if resp.Status != "revoked" {
		t.Errorf("after revoke from suspended, status should be 'revoked', got %q", resp.Status)
	}

	key, err := keyRepo.GetByID(ctx, keyID)
	if err != nil {
		t.Fatalf("GetByID failed: %v", err)
	}
	if key.Status != "revoked" {
		t.Errorf("persisted status: want 'revoked', got %q", key.Status)
	}
	if key.RevokeReason != "vehicle sold" {
		t.Errorf("RevokeReason: want 'vehicle sold', got %q", key.RevokeReason)
	}
}

// -------------------------------------------------------------------------
// ILLEGAL TRANSITIONS — each should return an error
// -------------------------------------------------------------------------

// TestKeyLifecycle_pending_to_suspended: should be illegal
func TestKeyLifecycle_pending_to_suspended(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	keyID := "key-pending-suspend"
	keyRepo.keys[keyID] = &repository.Key{
		ID:     keyID,
		Status: "pending",
	}

	err := svc.SuspendKey(ctx, keyID)
	if err == nil {
		t.Fatal("expected error: pending keys cannot be suspended")
	}
	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.FailedPrecondition {
		t.Errorf("want FailedPrecondition, got %v", st.Code())
	}
}

// TestKeyLifecycle_pending_to_revoked: should be illegal
func TestKeyLifecycle_pending_to_revoked(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	keyID := "key-pending-revoke"
	keyRepo.keys[keyID] = &repository.Key{
		ID:     keyID,
		Status: "pending",
	}

	_, err := svc.RevokeKey(ctx, &pb.RevokeKeyRequest{
		KeyId:  keyID,
		Reason: "test illegal transition",
	})
	if err == nil {
		t.Fatal("expected error: pending keys cannot be revoked directly")
	}
	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.FailedPrecondition {
		t.Errorf("want FailedPrecondition, got %v", st.Code())
	}
}

// TestKeyLifecycle_active_to_pending: should be illegal (no going back)
func TestKeyLifecycle_active_to_pending(t *testing.T) {
	// This is tested implicitly: activating an already-active key should fail
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	keyID := "key-already-active"
	keyRepo.keys[keyID] = &repository.Key{
		ID:     keyID,
		Status: "active",
	}

	_, err := svc.ActivateKey(ctx, &pb.ActivateKeyRequest{KeyId: keyID})
	if err == nil {
		t.Fatal("expected error: activate an already-active key should fail")
	}
	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.FailedPrecondition {
		t.Errorf("want FailedPrecondition, got %v", st.Code())
	}
}

// TestKeyLifecycle_suspended_to_pending: should be illegal
func TestKeyLifecycle_suspended_to_pending(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	keyID := "key-suspended-activate"
	keyRepo.keys[keyID] = &repository.Key{
		ID:     keyID,
		Status: "suspended",
	}

	// Activate only accepts pending keys, so suspended → active via ActivateKey should fail
	_, err := svc.ActivateKey(ctx, &pb.ActivateKeyRequest{KeyId: keyID})
	if err == nil {
		t.Fatal("expected error: suspended keys cannot be activated via ActivateKey")
	}
	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.FailedPrecondition {
		t.Errorf("want FailedPrecondition, got %v", st.Code())
	}
}

// TestKeyLifecycle_revoked_to_active: should be illegal (terminal state)
func TestKeyLifecycle_revoked_to_active(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	keyID := "key-revoked-resume"
	keyRepo.keys[keyID] = &repository.Key{
		ID:     keyID,
		Status: "revoked",
	}

	err := svc.ResumeKey(ctx, keyID)
	if err == nil {
		t.Fatal("expected error: revoked keys cannot be resumed")
	}
	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.FailedPrecondition {
		t.Errorf("want FailedPrecondition, got %v", st.Code())
	}
}

// TestKeyLifecycle_revoked_to_suspended: should be illegal (terminal state)
func TestKeyLifecycle_revoked_to_suspended(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	keyID := "key-revoked-suspend"
	keyRepo.keys[keyID] = &repository.Key{
		ID:     keyID,
		Status: "revoked",
	}

	err := svc.SuspendKey(ctx, keyID)
	if err == nil {
		t.Fatal("expected error: revoked keys cannot be suspended")
	}
	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.FailedPrecondition {
		t.Errorf("want FailedPrecondition, got %v", st.Code())
	}
}

// TestKeyLifecycle_expired_to_active: should be illegal (terminal state)
func TestKeyLifecycle_expired_to_active(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	keyID := "key-expired-resume"
	keyRepo.keys[keyID] = &repository.Key{
		ID:     keyID,
		Status: "expired",
	}

	err := svc.ResumeKey(ctx, keyID)
	if err == nil {
		t.Fatal("expected error: expired keys cannot be resumed")
	}
	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.FailedPrecondition {
		t.Errorf("want FailedPrecondition, got %v", st.Code())
	}
}

// TestKeyLifecycle_revoked_to_revoked: should be illegal (already terminal)
func TestKeyLifecycle_revoked_to_revoked(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	keyID := "key-already-revoked"
	keyRepo.keys[keyID] = &repository.Key{
		ID:     keyID,
		Status: "revoked",
	}

	_, err := svc.RevokeKey(ctx, &pb.RevokeKeyRequest{
		KeyId:  keyID,
		Reason: "double revoke",
	})
	if err == nil {
		t.Fatal("expected error: already revoked key cannot be revoked again")
	}
	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.FailedPrecondition {
		t.Errorf("want FailedPrecondition, got %v", st.Code())
	}
}

// TestKeyLifecycle_expired_to_revoked: should be illegal (terminal state)
func TestKeyLifecycle_expired_to_revoked(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	keyID := "key-expired-revoke"
	keyRepo.keys[keyID] = &repository.Key{
		ID:     keyID,
		Status: "expired",
	}

	_, err := svc.RevokeKey(ctx, &pb.RevokeKeyRequest{
		KeyId:  keyID,
		Reason: "test",
	})
	if err == nil {
		t.Fatal("expected error: expired keys cannot be revoked")
	}
	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.FailedPrecondition {
		t.Errorf("want FailedPrecondition, got %v", st.Code())
	}
}

// -------------------------------------------------------------------------
// FULL LIFECYCLE SCENARIO TESTS
// -------------------------------------------------------------------------

// TestKeyLifecycle_FullCycle: Create → Activate → Suspend → Resume → Revoke
func TestKeyLifecycle_FullCycle(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	vehicleRepo.vehicles["v-001"] = &repository.Vehicle{ID: "v-001"}
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	// Create (pending)
	createResp, err := svc.CreateKey(ctx, &pb.CreateKeyRequest{
		VehicleId: "v-001",
		UserId:    "user-001",
		KeyType:   "primary",
	})
	if err != nil {
		t.Fatalf("CreateKey failed: %v", err)
	}
	keyID := createResp.KeyId

	// Activate (pending → active)
	_, err = svc.ActivateKey(ctx, &pb.ActivateKeyRequest{KeyId: keyID})
	if err != nil {
		t.Fatalf("ActivateKey failed at step 2: %v", err)
	}
	assertKeyStatus(t, keyRepo, keyID, "active")

	// Suspend (active → suspended)
	if err := svc.SuspendKey(ctx, keyID); err != nil {
		t.Fatalf("SuspendKey failed: %v", err)
	}
	assertKeyStatus(t, keyRepo, keyID, "suspended")

	// Resume (suspended → active)
	if err := svc.ResumeKey(ctx, keyID); err != nil {
		t.Fatalf("ResumeKey failed: %v", err)
	}
	assertKeyStatus(t, keyRepo, keyID, "active")

	// Suspend again
	if err := svc.SuspendKey(ctx, keyID); err != nil {
		t.Fatalf("Second SuspendKey failed: %v", err)
	}
	assertKeyStatus(t, keyRepo, keyID, "suspended")

	// Revoke (suspended → revoked)
	_, err = svc.RevokeKey(ctx, &pb.RevokeKeyRequest{
		KeyId:  keyID,
		Reason: "end of lifecycle test",
	})
	if err != nil {
		t.Fatalf("RevokeKey failed: %v", err)
	}
	assertKeyStatus(t, keyRepo, keyID, "revoked")

	// Verify terminal state: no further transitions allowed
	err = svc.ResumeKey(ctx, keyID)
	if err == nil {
		t.Error("ResumeKey on revoked key should fail (terminal state)")
	}
}

// TestKeyLifecycle_IllegalSequence: try every illegal jump
func TestKeyLifecycle_IllegalSequence(t *testing.T) {
	// Test that pending → any(active, suspended, revoked) validates correctly
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	vehicleRepo.vehicles["v-001"] = &repository.Vehicle{ID: "v-001"}
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	// Create
	createResp, err := svc.CreateKey(ctx, &pb.CreateKeyRequest{
		VehicleId: "v-001",
		UserId:    "user-001",
	})
	if err != nil {
		t.Fatalf("CreateKey failed: %v", err)
	}
	keyID := createResp.KeyId

	// Try to revoke while pending (should fail)
	_, err = svc.RevokeKey(ctx, &pb.RevokeKeyRequest{KeyId: keyID, Reason: "premature"})
	if err == nil {
		t.Error("RevokeKey from pending should fail")
	}

	// Try to suspend while pending (should fail)
	if err := svc.SuspendKey(ctx, keyID); err == nil {
		t.Error("SuspendKey from pending should fail")
	}

	// Now activate properly
	_, err = svc.ActivateKey(ctx, &pb.ActivateKeyRequest{KeyId: keyID})
	if err != nil {
		t.Fatalf("ActivateKey failed: %v", err)
	}

	// Try to go back to pending (should fail via ActivateKey)
	_, err = svc.ActivateKey(ctx, &pb.ActivateKeyRequest{KeyId: keyID})
	if err == nil {
		t.Error("ActivateKey on active key should fail")
	}

	// Revoke should work from active
	_, err = svc.RevokeKey(ctx, &pb.RevokeKeyRequest{KeyId: keyID, Reason: "test complete"})
	if err != nil {
		t.Fatalf("RevokeKey from active failed: %v", err)
	}
}

// TestKeyLifecycle_DoubleSuspend: suspend an already-suspended key should fail
func TestKeyLifecycle_DoubleSuspend(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	keyID := "key-suspended-double"
	keyRepo.keys[keyID] = &repository.Key{
		ID:     keyID,
		Status: "suspended",
	}

	// Suspend an already-suspended key should fail
	err := svc.SuspendKey(ctx, keyID)
	if err == nil {
		t.Fatal("expected error: already suspended key cannot be suspended again")
	}
	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.FailedPrecondition {
		t.Errorf("want FailedPrecondition, got %v", st.Code())
	}
}

// TestKeyLifecycle_ResumeActiveKey: resuming an active key should fail
func TestKeyLifecycle_ResumeActiveKey(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	keyID := "key-active-resume"
	keyRepo.keys[keyID] = &repository.Key{
		ID:     keyID,
		Status: "active",
	}

	err := svc.ResumeKey(ctx, keyID)
	if err == nil {
		t.Fatal("expected error: active keys cannot be resumed")
	}
}

// -------------------------------------------------------------------------
// validateStateTransition unit tests (function-level)
// -------------------------------------------------------------------------

func TestValidateStateTransition_LegalTransitions(t *testing.T) {
	tests := []struct {
		from string
		to   string
	}{
		{"pending", "active"},
		{"active", "suspended"},
		{"active", "revoked"},
		{"suspended", "active"},
		{"suspended", "revoked"},
	}

	for _, tt := range tests {
		t.Run(tt.from+"_to_"+tt.to, func(t *testing.T) {
			if err := validateStateTransition(tt.from, tt.to); err != nil {
				t.Errorf("legal transition %s → %s should be allowed, got: %v", tt.from, tt.to, err)
			}
		})
	}
}

func TestValidateStateTransition_IllegalTransitions(t *testing.T) {
	tests := []struct {
		from string
		to   string
	}{
		{"pending", "suspended"},
		{"pending", "revoked"},
		{"pending", "expired"},
		{"active", "pending"},
		{"active", "expired"},
		{"suspended", "pending"},
		{"suspended", "expired"},
		{"expired", "active"},
		{"expired", "suspended"},
		{"expired", "revoked"},
		{"revoked", "active"},
		{"revoked", "suspended"},
		{"revoked", "expired"},
	}

	for _, tt := range tests {
		t.Run(tt.from+"_to_"+tt.to, func(t *testing.T) {
			if err := validateStateTransition(tt.from, tt.to); err == nil {
				t.Errorf("illegal transition %s → %s should be rejected", tt.from, tt.to)
			}
		})
	}
}

func TestValidateStateTransition_SameState(t *testing.T) {
	if err := validateStateTransition("active", "active"); err == nil {
		t.Error("same-state transition should be rejected")
	}
	if err := validateStateTransition("revoked", "revoked"); err == nil {
		t.Error("same-state transition for revoked should be rejected")
	}
}

func TestValidateStateTransition_UnknownStatus(t *testing.T) {
	if err := validateStateTransition("unknown", "active"); err == nil {
		t.Error("unknown from-status should be rejected")
	}
}

// -------------------------------------------------------------------------
// EXPIRY HANDLING
// -------------------------------------------------------------------------

func TestKeyLifecycle_ExpiredKeyCannotTransition(t *testing.T) {
	keyRepo := newMockKeyRepo()
	vehicleRepo := newMockVehicleRepo()
	svc := buildTestKeyService(keyRepo, vehicleRepo)
	ctx := context.Background()

	keyID := "key-expired"
	keyRepo.keys[keyID] = &repository.Key{
		ID:        keyID,
		Status:    "expired",
		ExpiresAt: time.Now().Add(-1 * time.Hour), // expired 1 hour ago
	}

	// Try to activate expired key
	_, err := svc.ActivateKey(ctx, &pb.ActivateKeyRequest{KeyId: keyID})
	if err == nil {
		t.Error("activating expired key should fail")
	}

	// Try to suspend expired key
	err = svc.SuspendKey(ctx, keyID)
	if err == nil {
		t.Error("suspending expired key should fail")
	}

	// Try to resume expired key
	err = svc.ResumeKey(ctx, keyID)
	if err == nil {
		t.Error("resuming expired key should fail")
	}

	// Try to revoke expired key (should fail per state machine)
	_, err = svc.RevokeKey(ctx, &pb.RevokeKeyRequest{KeyId: keyID})
	if err == nil {
		t.Error("revoking expired key should fail")
	}
}

// -------------------------------------------------------------------------
// Helper: assert key status
// -------------------------------------------------------------------------

func assertKeyStatus(t *testing.T, repo *mockKeyRepo, keyID, expectedStatus string) {
	t.Helper()
	k, err := repo.GetByID(context.Background(), keyID)
	if err != nil {
		t.Fatalf("GetByID(%q) failed: %v", keyID, err)
	}
	if k.Status != expectedStatus {
		t.Errorf("key %q status: want %q, got %q", keyID, expectedStatus, k.Status)
	}
}
