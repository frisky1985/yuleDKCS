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
	"github.com/frisky1985/yuleDKCS/backend/dkcs/pkg/logger"
	"github.com/frisky1985/yuleDKCS/backend/dkcs/pkg/telemetry"
	"github.com/google/uuid"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
)

// KeyService implements pb.KeyServiceServer
type KeyService struct {
	pb.UnimplementedKeyServiceServer
	keyRepo    *repository.KeyRepository
	vehicleRepo *repository.VehicleRepository
	logger      *logger.Logger
	telemetry   *telemetry.Telemetry

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
		s.logger.Warn("Idempotency key already processed", logger.String("idempotency_key", key))
		return true
	}

	s.idempotencyKeys[key] = true
	return false
}

// NewKeyService creates a new KeyService
func NewKeyService(
	keyRepo *repository.KeyRepository,
	vehicleRepo *repository.VehicleRepository,
	logger *logger.Logger,
	telemetry *telemetry.Telemetry,
) *KeyService {
	return &KeyService{
		keyRepo:    keyRepo,
		vehicleRepo: vehicleRepo,
		logger:      logger,
		telemetry:   telemetry,
	}
}

// CreateKey creates a new digital key
func (s *KeyService) CreateKey(ctx context.Context, req *pb.CreateKeyRequest) (*pb.CreateKeyResponse, error) {
	start := time.Now()
	defer func() {
		s.telemetry.RecordDuration("dkcs.key.create.duration", time.Since(start))
	}()

	s.logger.Info("CreateKey request", logger.String("vehicle_id", req.VehicleId), logger.String("user_id", req.UserId))

	// [M-07] 幂等性键检查
	if req.IdempotencyKey != "" {
		if s.checkAndMarkIdempotency("create:" + req.IdempotencyKey) {
			return nil, status.Error(codes.AlreadyExists, "request already processed")
		}
	}

	// Validate request
	if req.VehicleId == "" || req.UserId == "" {
		s.telemetry.IncCounter("dkcs.key.create.error", map[string]string{"reason": "invalid_request"})
		return nil, status.Error(codes.InvalidArgument, "vehicle_id and user_id are required")
	}

	// Check if vehicle exists
	vehicle, err := s.vehicleRepo.GetByID(ctx, req.VehicleId)
	if err != nil {
		s.logger.Error("Failed to get vehicle", logger.Err(err))
		s.telemetry.IncCounter("dkcs.key.create.error", map[string]string{"reason": "vehicle_not_found"})
		return nil, status.Error(codes.NotFound, "vehicle not found")
	}

	// Generate key ID
	keyID := uuid.New().String()

	// Generate key secret — stored hashed server-side, never returned to client
	secretBytes := make([]byte, 32)
	if _, err := rand.Read(secretBytes); err != nil {
		s.logger.Error("Failed to generate secret", logger.Err(err))
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
		s.logger.Error("Failed to create key", logger.Err(err))
		s.telemetry.IncCounter("dkcs.key.create.error", map[string]string{"reason": "db_error"})
		return nil, status.Error(codes.Internal, "failed to create key")
	}

	s.telemetry.IncCounter("dkcs.key.create.success", nil)
	s.logger.Info("Key created successfully", logger.String("key_id", keyID))

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

	s.logger.Info("ActivateKey request", logger.String("key_id", req.KeyId))

	// [M-07] 幂等性键检查
	if req.IdempotencyKey != "" {
		if s.checkAndMarkIdempotency("activate:" + req.IdempotencyKey) {
			return nil, status.Error(codes.AlreadyExists, "request already processed")
		}
	}

	key, err := s.keyRepo.GetByID(ctx, req.KeyId)
	if err != nil {
		s.logger.Error("Failed to get key", logger.Err(err))
		return nil, status.Error(codes.NotFound, "key not found")
	}

	if key.Status != "pending" {
		return nil, status.Error(codes.FailedPrecondition, "key is not in pending status")
	}

	// Update key status
	key.Status = "active"
	key.ActivatedAt = time.Now()

	if err := s.keyRepo.Update(ctx, key); err != nil {
		s.logger.Error("Failed to update key", logger.Err(err))
		return nil, status.Error(codes.Internal, "failed to activate key")
	}

	s.telemetry.IncCounter("dkcs.key.activate.success", nil)
	s.logger.Info("Key activated successfully", logger.String("key_id", req.KeyId))

	return &pb.ActivateKeyResponse{
		KeyId:      req.KeyId,
		Status:     "active",
		ActivatedAt: key.ActivatedAt.Unix(),
	}, nil
}

// RevokeKey revokes a digital key
func (s *KeyService) RevokeKey(ctx context.Context, req *pb.RevokeKeyRequest) (*pb.RevokeKeyResponse, error) {
	start := time.Now()
	defer func() {
		s.telemetry.RecordDuration("dkcs.key.revoke.duration", time.Since(start))
	}()

	s.logger.Info("RevokeKey request", logger.String("key_id", req.KeyId), logger.String("reason", req.Reason))

	// [M-07] 幂等性键检查
	if req.IdempotencyKey != "" {
		if s.checkAndMarkIdempotency("revoke:" + req.IdempotencyKey) {
			return nil, status.Error(codes.AlreadyExists, "request already processed")
		}
	}

	key, err := s.keyRepo.GetByID(ctx, req.KeyId)
	if err != nil {
		s.logger.Error("Failed to get key", logger.Err(err))
		return nil, status.Error(codes.NotFound, "key not found")
	}

	if key.Status == "revoked" {
		return nil, status.Error(codes.FailedPrecondition, "key is already revoked")
	}

	key.Status = "revoked"
	key.RevokedAt = time.Now()
	key.RevokeReason = req.Reason

	if err := s.keyRepo.Update(ctx, key); err != nil {
		s.logger.Error("Failed to update key", logger.Err(err))
		return nil, status.Error(codes.Internal, "failed to revoke key")
	}

	s.telemetry.IncCounter("dkcs.key.revoke.success", nil)
	s.logger.Info("Key revoked successfully", logger.String("key_id", req.KeyId))

	return &pb.RevokeKeyResponse{
		KeyId:     req.KeyId,
		Status:    "revoked",
		RevokedAt: key.RevokedAt.Unix(),
	}, nil
}

// GetKey retrieves key details
func (s *KeyService) GetKey(ctx context.Context, req *pb.GetKeyRequest) (*pb.GetKeyResponse, error) {
	key, err := s.keyRepo.GetByID(ctx, req.KeyId)
	if err != nil {
		s.logger.Error("Failed to get key", logger.Err(err))
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
		s.logger.Error("Failed to list keys", logger.Err(err))
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

	s.logger.Info("ShareKey request", logger.String("key_id", req.KeyId), logger.String("to_user", req.ToUserId))

	// [M-07] 幂等性键检查
	if req.IdempotencyKey != "" {
		if s.checkAndMarkIdempotency("share:" + req.IdempotencyKey) {
			return nil, status.Error(codes.AlreadyExists, "request already processed")
		}
	}

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
		s.logger.Error("Failed to create shared key", logger.Err(err))
		return nil, status.Error(codes.Internal, "failed to share key")
	}

	s.telemetry.IncCounter("dkcs.key.share.success", nil)

	return &pb.ShareKeyResponse{
		SharedKeyId: sharedKeyID,
		ShareCode:   generateShareCode(),
		ExpiresAt:   sharedKey.ExpiresAt.Unix(),
	}, nil
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
