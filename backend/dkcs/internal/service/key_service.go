package service

import (
	"context"
	"crypto/rand"
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"sync"
	"time"

	pb "github.com/frisky1985/yuleDKCS/backend/dkcs/proto/dkcs"
	"github.com/frisky1985/yuleDKCS/backend/dkcs/internal/repository"
	"github.com/google/uuid"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
)

// KeyService implements pb.KeyServiceServer
type KeyService struct {
	pb.UnimplementedKeyServiceServer
	keyRepo    KeyRepository
	vehicleRepo VehicleRepository
	logger      Logger
	telemetry   Telemetry
	eventBus    EventBus // optional Kafka event producer

	// [M-07] 幂等性保证
	idempotencyMu     sync.RWMutex
	idempotencyKeys   map[string]bool  // processed idempotency keys
	idempotencyMaxAge time.Duration    // 键保留时长
}

// IdempotencyKeyOption 配置幂等性参数
func (s *KeyService) WithIdempotency(maxAge time.Duration) *KeyService {
	s.idempotencyMu.Lock()
	defer s.idempotencyMu.Unlock()
	s.idempotencyKeys = make(map[string]bool)
	s.idempotencyMaxAge = maxAge
	return s
}

// [M-07] 检查并标记幂等性键
// 返回 true 表示已处理过（重复请求），false 表示首次处理
func (s *KeyService) checkAndMarkIdempotency(key string) bool {
	s.idempotencyMu.Lock()
	defer s.idempotencyMu.Unlock()

	if s.idempotencyKeys == nil {
		s.idempotencyKeys = make(map[string]bool)
	}

	if s.idempotencyKeys[key] {
		s.logger.Warn("Idempotency key already processed", "idempotency_key", key)
		return true
	}

	s.idempotencyKeys[key] = true
	return false
}

// NewKeyService creates a new KeyService
func NewKeyService(
	keyRepo KeyRepository,
	vehicleRepo VehicleRepository,
	logger Logger,
	telemetry Telemetry,
) *KeyService {
	return &KeyService{
		keyRepo:    keyRepo,
		vehicleRepo: vehicleRepo,
		logger:      logger,
		telemetry:   telemetry,
	}
}

// WithEventBus attaches an optional EventBus for publishing key lifecycle events.
// Events are published asynchronously; failures are logged but never block the
// business flow. Use the optional EventBus builder pattern to avoid coupling
// the service layer to a specific message queue implementation.
func (s *KeyService) WithEventBus(bus EventBus) *KeyService {
	s.eventBus = bus
	return s
}

// CreateKey creates a new digital key
func (s *KeyService) CreateKey(ctx context.Context, req *pb.CreateKeyRequest) (*pb.CreateKeyResponse, error) {
	start := time.Now()
	defer func() {
		s.telemetry.RecordDuration("dkcs.key.create.duration", time.Since(start))
	}()

	s.logger.Info("CreateKey request", "vehicle_id", req.VehicleId, "user_id", req.UserId)

	// Validate request
	if req.VehicleId == "" || req.UserId == "" {
		s.telemetry.IncCounter("dkcs.key.create.error", map[string]string{"reason": "invalid_request"})
		return nil, status.Error(codes.InvalidArgument, "vehicle_id and user_id are required")
	}

	// Check if vehicle exists
	_ , err := s.vehicleRepo.GetByID(ctx, req.VehicleId)   // vehicle validated elsewhere
	if err != nil {
		s.logger.Error("Failed to get vehicle", "error", err)
		s.telemetry.IncCounter("dkcs.key.create.error", map[string]string{"reason": "vehicle_not_found"})
		return nil, status.Error(codes.NotFound, "vehicle not found")
	}

	// Generate key ID
	keyID := uuid.New().String()

	// Generate key secret — stored hashed server-side, never returned to client
	secretBytes := make([]byte, 32)
	if _, err := rand.Read(secretBytes); err != nil {
		s.logger.Error("Failed to generate secret", "error", err)
		return nil, status.Error(codes.Internal, "failed to generate key secret")
	}
	secret := hex.EncodeToString(secretBytes)
	secretHash := hashSecret(secret) // One-way hash for server storage

	// Create key
	key := &repository.Key{
		ID:          keyID,
		VehicleID:   req.VehicleId,
		UserID:      req.UserId,
		KeyType:     req.KeyType,
		Status:      "pending",
		Permissions: req.Permissions,
		Secret:      secretHash, // Store hash only
		CreatedAt:   time.Now(),
		ExpiresAt:   time.Now().Add(365 * 24 * time.Hour), // Default 1 year
	}

	if err := s.keyRepo.Create(ctx, key); err != nil {
		s.logger.Error("Failed to create key", "error", err)
		s.telemetry.IncCounter("dkcs.key.create.error", map[string]string{"reason": "db_error"})
		return nil, status.Error(codes.Internal, "failed to create key")
	}

	s.telemetry.IncCounter("dkcs.key.create.success", nil)
	s.logger.Info("Key created successfully", "key_id", keyID)

	// V-03 fix: Secret is NOT returned to client.
	// The key establishment uses ECDH key agreement between phone SE and vehicle SE050, 
	// not pre-shared secrets. The "secret" here is a server-side reference only.
	return &pb.CreateKeyResponse{
		KeyId:     keyID,
		Secret:    "", // Empty: secret never leaves the server
		Status:    "pending",
		CreatedAt: key.CreatedAt.Unix(),
	}, nil
}

// ActivateKey activates a digital key
func (s *KeyService) ActivateKey(ctx context.Context, req *pb.ActivateKeyRequest) (*pb.ActivateKeyResponse, error) {
	start := time.Now()
	defer func() {
		s.telemetry.RecordDuration("dkcs.key.activate.duration", time.Since(start))
	}()

	s.logger.Info("ActivateKey request", "key_id", req.KeyId)

	key, err := s.keyRepo.GetByID(ctx, req.KeyId)
	if err != nil {
		s.logger.Error("Failed to get key", "error", err)
		return nil, status.Error(codes.NotFound, "key not found")
	}

	if key.Status != "pending" {
		return nil, status.Error(codes.FailedPrecondition, "key is not in pending status")
	}

	// Update key status
	key.Status = "active"
	now := time.Now(); key.ActivatedAt = &now

	if err := s.keyRepo.Update(ctx, key); err != nil {
		s.logger.Error("Failed to update key", "error", err)
		return nil, status.Error(codes.Internal, "failed to activate key")
	}

	s.telemetry.IncCounter("dkcs.key.activate.success", nil)
	s.logger.Info("Key activated successfully", "key_id", req.KeyId)

	// [MQ] Emit key_activated event (async, fire-and-forget)
	s.emitKeyEvent(ctx, "key_activated", req.KeyId, key.UserID, "")

	return &pb.ActivateKeyResponse{
		KeyId:      req.KeyId,
		Status:     "active",
		ActivatedAt: key.ActivatedAt.Unix(),
	}, nil
}

// validateStateTransition checks whether a key status transition is valid under
// the CCC/ICCE digital key lifecycle state machine.
//
// Valid transitions (per CCC/ICCE protocol):
//   Issued (pending) → Active
//   Active           → Suspended
//   Active           → Revoked
//   Suspended        → Active
//   Suspended        → Revoked
//
// Terminal states: Expired, Revoked — no outgoing transitions allowed.
func validateStateTransition(currentStatus, desiredStatus string) error {
	if currentStatus == desiredStatus {
		return fmt.Errorf("key is already in %s status", currentStatus)
	}

	// Terminal states check
	if currentStatus == "expired" {
		return fmt.Errorf("cannot transition from expired status")
	}
	if currentStatus == "revoked" {
		return fmt.Errorf("cannot transition from revoked status")
	}

	switch currentStatus {
	case "pending":
		if desiredStatus != "active" {
			return fmt.Errorf("cannot transition from %s to %s: pending keys can only be activated", currentStatus, desiredStatus)
		}
	case "active":
		if desiredStatus != "suspended" && desiredStatus != "revoked" {
			return fmt.Errorf("cannot transition from %s to %s: active keys can only be suspended or revoked", currentStatus, desiredStatus)
		}
	case "suspended":
		if desiredStatus != "active" && desiredStatus != "revoked" {
			return fmt.Errorf("cannot transition from %s to %s: suspended keys can only be resumed or revoked", currentStatus, desiredStatus)
		}
	default:
		return fmt.Errorf("unknown key status: %s", currentStatus)
	}

	return nil
}

// RevokeKey revokes a digital key.
// Valid transitions: active → revoked, suspended → revoked.
func (s *KeyService) RevokeKey(ctx context.Context, req *pb.RevokeKeyRequest) (*pb.RevokeKeyResponse, error) {
	start := time.Now()
	defer func() {
		s.telemetry.RecordDuration("dkcs.key.revoke.duration", time.Since(start))
	}()

	s.logger.Info("RevokeKey request", "key_id", req.KeyId, "reason", req.Reason)

	key, err := s.keyRepo.GetByID(ctx, req.KeyId)
	if err != nil {
		s.logger.Error("Failed to get key", "error", err)
		return nil, status.Error(codes.NotFound, "key not found")
	}

	if err := validateStateTransition(key.Status, "revoked"); err != nil {
		s.logger.Warn("Invalid state transition for revoke", "key_id", req.KeyId, "current_status", key.Status, "error", err)
		return nil, status.Error(codes.FailedPrecondition, err.Error())
	}

	key.Status = "revoked"
	now := time.Now(); key.RevokedAt = &now
	key.RevokeReason = req.Reason

	if err := s.keyRepo.Update(ctx, key); err != nil {
		s.logger.Error("Failed to update key", "error", err)
		return nil, status.Error(codes.Internal, "failed to revoke key")
	}

	s.telemetry.IncCounter("dkcs.key.revoke.success", nil)
	s.logger.Info("Key revoked successfully", "key_id", req.KeyId)

	// [MQ] Emit key_revoked event (async, fire-and-forget)
	s.emitKeyEvent(ctx, "key_revoked", req.KeyId, key.UserID, "")

	return &pb.RevokeKeyResponse{
		KeyId:     req.KeyId,
		Status:    "revoked",
		RevokedAt: key.RevokedAt.Unix(),
	}, nil
}

// SuspendKey suspends a digital key.
// Valid transition: active → suspended.
func (s *KeyService) SuspendKey(ctx context.Context, keyID string) error {
	s.logger.Info("SuspendKey", "key_id", keyID)

	key, err := s.keyRepo.GetByID(ctx, keyID)
	if err != nil {
		s.logger.Error("Failed to get key for suspend", "error", err)
		return status.Error(codes.NotFound, "key not found")
	}

	if err := validateStateTransition(key.Status, "suspended"); err != nil {
		s.logger.Warn("Invalid state transition for suspend", "key_id", keyID, "current_status", key.Status)
		return status.Error(codes.FailedPrecondition, err.Error())
	}

	key.Status = "suspended"

	if err := s.keyRepo.Update(ctx, key); err != nil {
		s.logger.Error("Failed to suspend key", "error", err)
		return status.Error(codes.Internal, "failed to suspend key")
	}

	s.telemetry.IncCounter("dkcs.key.suspend.success", nil)
	s.logger.Info("Key suspended successfully", "key_id", keyID)

	return nil
}

// ResumeKey resumes a suspended digital key.
// Valid transition: suspended → active.
func (s *KeyService) ResumeKey(ctx context.Context, keyID string) error {
	s.logger.Info("ResumeKey", "key_id", keyID)

	key, err := s.keyRepo.GetByID(ctx, keyID)
	if err != nil {
		s.logger.Error("Failed to get key for resume", "error", err)
		return status.Error(codes.NotFound, "key not found")
	}

	if err := validateStateTransition(key.Status, "active"); err != nil {
		s.logger.Warn("Invalid state transition for resume", "key_id", keyID, "current_status", key.Status)
		return status.Error(codes.FailedPrecondition, err.Error())
	}

	key.Status = "active"

	if err := s.keyRepo.Update(ctx, key); err != nil {
		s.logger.Error("Failed to resume key", "error", err)
		return status.Error(codes.Internal, "failed to resume key")
	}

	s.telemetry.IncCounter("dkcs.key.resume.success", nil)
	s.logger.Info("Key resumed successfully", "key_id", keyID)

	return nil
}

// GetKey retrieves key details
func (s *KeyService) GetKey(ctx context.Context, req *pb.GetKeyRequest) (*pb.GetKeyResponse, error) {
	key, err := s.keyRepo.GetByID(ctx, req.KeyId)
	if err != nil {
		s.logger.Error("Failed to get key", "error", err)
		return nil, status.Error(codes.NotFound, "key not found")
	}

	return &pb.GetKeyResponse{
		KeyId:       key.ID,
		VehicleId:   key.VehicleID,
		UserId:      key.UserID,
		KeyType:     key.KeyType,
		Status:      key.Status,
		Permissions: key.Permissions,
		CreatedAt:   key.CreatedAt.Unix(),
		ExpiresAt:   key.ExpiresAt.Unix(),
	}, nil
}

// ListKeys lists keys for a user or vehicle
func (s *KeyService) ListKeys(ctx context.Context, req *pb.ListKeysRequest) (*pb.ListKeysResponse, error) {
	var keys []*repository.Key
	var err error

	if req.UserId != "" {
		keys, err = s.keyRepo.ListByUser(ctx, req.UserId, int(req.Limit), int(req.Offset))
	} else if req.VehicleId != "" {
		keys, err = s.keyRepo.ListByVehicle(ctx, req.VehicleId, int(req.Limit), int(req.Offset))
	} else {
		return nil, status.Error(codes.InvalidArgument, "user_id or vehicle_id is required")
	}

	if err != nil {
		s.logger.Error("Failed to list keys", "error", err)
		return nil, status.Error(codes.Internal, "failed to list keys")
	}

	resp := &pb.ListKeysResponse{}
	for _, key := range keys {
		resp.Keys = append(resp.Keys, &pb.KeyInfo{
			KeyId:       key.ID,
			VehicleId:   key.VehicleID,
			UserId:      key.UserID,
			KeyType:     key.KeyType,
			Status:      key.Status,
			Permissions: key.Permissions,
			CreatedAt:   key.CreatedAt.Unix(),
			ExpiresAt:   key.ExpiresAt.Unix(),
		})
	}

	return resp, nil
}

// ShareKey shares a digital key with another user
func (s *KeyService) ShareKey(ctx context.Context, req *pb.ShareKeyRequest) (*pb.ShareKeyResponse, error) {
	start := time.Now()
	defer func() {
		s.telemetry.RecordDuration("dkcs.key.share.duration", time.Since(start))
	}()

	s.logger.Info("ShareKey request", "key_id", req.KeyId, "to_user", req.ToUserId)

	// Get original key
	origKey, err := s.keyRepo.GetByID(ctx, req.KeyId)
	if err != nil {
		return nil, status.Error(codes.NotFound, "key not found")
	}

	// Check if sharing is allowed
	if !origKey.HasPermission("share") {
		return nil, status.Error(codes.PermissionDenied, "sharing not allowed for this key")
	}

	// Create shared key
	sharedKeyID := uuid.New().String()
	secretBytes := make([]byte, 32)
	if _, err := rand.Read(secretBytes); err != nil {
		return nil, status.Error(codes.Internal, "failed to generate secret")
	}
	secret := hex.EncodeToString(secretBytes)
	secretHash := hashSecret(secret) // [CR-3 fix] hash before storing

	sharedKey := &repository.Key{
		ID:          sharedKeyID,
		VehicleID:   origKey.VehicleID,
		UserID:      req.ToUserId,
		KeyType:     origKey.KeyType,
		Status:      "pending",
		Permissions: req.Permissions,
		Secret:      secretHash,
		ParentKeyID: &origKey.ID,
		CreatedAt:   time.Now(),
		ExpiresAt:   time.Now().Add(30 * 24 * time.Hour), // 30 days for shared keys
	}

	if err := s.keyRepo.Create(ctx, sharedKey); err != nil {
		s.logger.Error("Failed to create shared key", "error", err)
		return nil, status.Error(codes.Internal, "failed to share key")
	}

	s.telemetry.IncCounter("dkcs.key.share.success", nil)

	// [MQ] Emit key_shared event (async, fire-and-forget)
	s.emitKeyEvent(ctx, "key_shared", sharedKeyID, origKey.UserID, req.ToUserId)

	return &pb.ShareKeyResponse{
		SharedKeyId: sharedKeyID,
		ShareCode:   generateShareCode(),
		ExpiresAt:   sharedKey.ExpiresAt.Unix(),
	}, nil
}

// emitKeyEvent publishes a key lifecycle event to the optional EventBus.
// The publish is non-blocking: it uses a select with a 500ms timeout and
// a goroutine fallback to prevent the business flow from stalling.
// This is a fire-and-forget pattern — failures are logged but not returned.
func (s *KeyService) emitKeyEvent(ctx context.Context, eventType, keyID, ownerID, targetID string) {
	if s.eventBus == nil {
		return // Kafka not configured, silently skip
	}

	done := make(chan error, 1)
	go func() {
		done <- s.eventBus.PublishKeyEvent(ctx, eventType, keyID, ownerID, targetID)
	}()

	select {
	case err := <-done:
		if err != nil {
			s.logger.Error("Failed to publish key event to EventBus",
				"event_type", eventType,
				"key_id", keyID,
				"error", err,
			)
		}
	case <-time.After(500 * time.Millisecond):
		s.logger.Warn("EventBus publish timed out (fire-and-forget)",
			"event_type", eventType,
			"key_id", keyID,
		)
	}
}

func generateShareCode() string {
	bytes := make([]byte, 4)
	rand.Read(bytes)
	// [CR-4 fix] use all 4 bytes for ~32 bits of entropy
	code := (uint32(bytes[0]) << 24) | (uint32(bytes[1]) << 16) |
		(uint32(bytes[2]) << 8) | uint32(bytes[3])
	return fmt.Sprintf("%08X", code)
}

// hashSecret creates a one-way hash of the key secret for server-side storage.
// Uses SHA-256 with a per-key salt. The plaintext secret is never stored.
func hashSecret(secret string) string {
	salt := make([]byte, 16)
	rand.Read(salt)
	h := sha256.New()
	h.Write(salt)
	h.Write([]byte(secret))
	return hex.EncodeToString(salt) + ":" + hex.EncodeToString(h.Sum(nil))
}
