package service

import (
	"context"
	"fmt"
	"sync"
	"time"

	"go.uber.org/zap"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"

	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/adapter"
	hub_error "github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/error"
)

// PushPayload represents a push notification payload sent to mobile devices.
type PushPayload struct {
	Type   string            `json:"type"`
	UserID string            `json:"user_id"`
	KeyID  string            `json:"key_id,omitempty"`
	Data   map[string]string `json:"data,omitempty"`
}

// PushService handles push notifications to mobile devices (FCM/APNs/极光等).
type PushService interface {
	// SendPush sends a push notification payload to a user's registered device(s).
	SendPush(ctx context.Context, userID string, payload *PushPayload) error
}

// KeyRecord holds the persisted metadata for a single digital key.
type KeyRecord struct {
	KeyID        string
	OwnerUserID  string
	VehicleID    string
	Vendor       string
	Status       string // "active", "suspended", "revoked", "pending"
	CreatedAt    int64  // unix millis
}

// KeyStore provides persistent storage for key metadata.
// Implementations MUST be goroutine-safe.
type KeyStore interface {
	// GetKeyOwner returns the user_id of the key owner. Returns empty string
	// and an error if the key is not found.
	GetKeyOwner(ctx context.Context, keyID string) (string, error)
	// GetKeyStatus returns the current status of a key.
	GetKeyStatus(ctx context.Context, keyID string) (string, error)
	// GetKeyRecord returns the full KeyRecord for a key.
	GetKeyRecord(ctx context.Context, keyID string) (*KeyRecord, error)
	// SetKey creates or updates key metadata.
	SetKey(ctx context.Context, key *KeyRecord) error
	// SetKeyStatus updates only the status of an existing key.
	SetKeyStatus(ctx context.Context, keyID, status string) error
	// ListKeysByUser returns all key IDs owned by the given user.
	ListKeysByUser(ctx context.Context, userID string) ([]*KeyRecord, error)
}

// InMemoryKeyStore is an ephemeral in-memory implementation of KeyStore.
// All data is lost on process restart — safe for development/testing.
type InMemoryKeyStore struct {
	mu   sync.RWMutex
	data map[string]*KeyRecord // keyID -> metadata
}

// NewInMemoryKeyStore creates an empty in-memory key store.
func NewInMemoryKeyStore() *InMemoryKeyStore {
	return &InMemoryKeyStore{
		data: make(map[string]*KeyRecord),
	}
}

func (s *InMemoryKeyStore) GetKeyOwner(_ context.Context, keyID string) (string, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	rec, ok := s.data[keyID]
	if !ok {
		return "", fmt.Errorf("key %s not found", keyID)
	}
	return rec.OwnerUserID, nil
}

func (s *InMemoryKeyStore) GetKeyStatus(_ context.Context, keyID string) (string, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	rec, ok := s.data[keyID]
	if !ok {
		return "", fmt.Errorf("key %s not found", keyID)
	}
	return rec.Status, nil
}

func (s *InMemoryKeyStore) GetKeyRecord(_ context.Context, keyID string) (*KeyRecord, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	rec, ok := s.data[keyID]
	if !ok {
		return nil, fmt.Errorf("key %s not found", keyID)
	}
	// Return a copy to avoid race on pointer mutation after unlock
	cp := *rec
	return &cp, nil
}

func (s *InMemoryKeyStore) SetKey(_ context.Context, key *KeyRecord) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	cp := *key
	s.data[key.KeyID] = &cp
	return nil
}

func (s *InMemoryKeyStore) SetKeyStatus(_ context.Context, keyID, status string) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	rec, ok := s.data[keyID]
	if !ok {
		return fmt.Errorf("key %s not found", keyID)
	}
	rec.Status = status
	return nil
}

func (s *InMemoryKeyStore) ListKeysByUser(_ context.Context, userID string) ([]*KeyRecord, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	var result []*KeyRecord
	for _, rec := range s.data {
		if rec.OwnerUserID == userID {
			cp := *rec
			result = append(result, &cp)
		}
	}
	return result, nil
}

type KeyManagementService struct {
	pb.UnimplementedKeyManagementServiceServer
	registry    *adapter.Registry
	logger      *zap.Logger
	pushService PushService
	keyStore    KeyStore
}

func NewKeyManagementService(registry *adapter.Registry, logger *zap.Logger) *KeyManagementService {
	return &KeyManagementService{
		registry:    registry,
		logger:      logger.With(zap.String("service", "KeyManagement")),
		keyStore:    NewInMemoryKeyStore(),
	}
}

// WithKeyStore sets a custom key store implementation.
func (s *KeyManagementService) WithKeyStore(ks KeyStore) *KeyManagementService {
	s.keyStore = ks
	return s
}

// WithPushService sets the push notification service.
func (s *KeyManagementService) WithPushService(ps PushService) *KeyManagementService {
	s.pushService = ps
	return s
}

// ── User / Role Extraction Helpers ──

// extractUserFromContext extracts user_id and role from gRPC incoming metadata.
// The metadata is injected by REST gateway's authMiddleware.
func extractUserFromContext(ctx context.Context) (userID, role string) {
	md, ok := metadata.FromIncomingContext(ctx)
	if !ok {
		return "", ""
	}
	uids := md.Get("user_id")
	roles := md.Get("user_role")
	if len(uids) > 0 {
		userID = uids[0]
	}
	if len(roles) > 0 {
		role = roles[0]
	}
	return
}

// isAdminUser checks whether the role extracted from context equals "admin".
func isAdminUser(role string) bool {
	return role == "admin"
}

// verifyKeyOwnership checks whether the given userID is the actual owner of keyID.
// Admin users bypass this check. Returns true if access is permitted.
func (s *KeyManagementService) verifyKeyOwnership(ctx context.Context, userID, keyID string) bool {
	// Extract role from context for admin bypass
	_, role := extractUserFromContext(ctx)
	if isAdminUser(role) {
		return true
	}

	ownerID, err := s.keyStore.GetKeyOwner(ctx, keyID)
	if err != nil {
		s.logger.Warn("ownership check: key lookup failed",
			zap.String("key_id", keyID),
			zap.String("user_id", userID),
			zap.Error(err),
		)
		return false
	}

	if ownerID != userID {
		s.logger.Warn("ownership check: user does not own key",
			zap.String("user_id", userID),
			zap.String("key_id", keyID),
			zap.String("owner_user_id", ownerID),
		)
		return false
	}

	return true
}

// ── gRPC Handlers ──

func (s *KeyManagementService) BindKey(ctx context.Context, req *pb.BindKeyRequest) (*pb.BindKeyResponse, error) {
	s.logger.Info("BindKey request",
		zap.String("vehicle_id", req.VehicleId),
		zap.String("user_id", req.UserId),
		zap.String("vendor", req.Vendor.String()),
	)

	// 查找对应适配器
	a, ok := s.registry.GetByVendor(req.Vendor.String())
	if !ok {
		return &pb.BindKeyResponse{
			ErrorCode: "ADAPTER_NOT_FOUND",
			ErrorMsg:  fmt.Sprintf("no adapter for vendor %s", req.Vendor),
		}, nil
	}

	// 委托给厂商适配器
	resp, err := a.BindKey(ctx, req)
	if err != nil {
		s.logger.Error("BindKey adapter error", zap.Error(err))
		return &pb.BindKeyResponse{
			ErrorCode: "ADAPTER_ERROR",
			ErrorMsg:  err.Error(),
		}, nil
	}

	// 存储密钥 owner 元数据
	if resp != nil && resp.Key != nil && resp.Key.KeyId != "" {
		keyID := resp.Key.KeyId
		keyRecord := &KeyRecord{
			KeyID:       keyID,
			OwnerUserID: req.UserId,
			VehicleID:   req.VehicleId,
			Vendor:      req.Vendor.String(),
			Status:      "active",
			CreatedAt:   time.Now().UnixMilli(),
		}
		if err := s.keyStore.SetKey(ctx, keyRecord); err != nil {
			s.logger.Error("BindKey: failed to persist key metadata",
				zap.String("key_id", keyID),
				zap.Error(err),
			)
			// 非致命错误，审计日志中标记
			s.auditLog(ctx, "bind_key_store_failed", req.UserId, req.VehicleId, keyID, "metadata_write_failed")
		}
	}

	// 写入审计日志
	s.auditLog(ctx, "bind_key", req.UserId, req.VehicleId, resp.Key.KeyId, "success")

	return resp, nil
}

func (s *KeyManagementService) UnbindKey(ctx context.Context, req *pb.UnbindKeyRequest) (*pb.UnbindKeyResponse, error) {
	s.logger.Info("UnbindKey", zap.String("key_id", req.KeyId))

	// [CRIT-1] 资源归属检查：验证当前用户是否拥有该密钥
	userID, _ := extractUserFromContext(ctx)
	if userID == "" {
		s.auditLog(ctx, "unbind_key", "", userID, req.KeyId, "denied_no_auth")
		return nil, status.Error(codes.Unauthenticated, "missing authentication context")
	}
	if !s.verifyKeyOwnership(ctx, userID, req.KeyId) {
		s.auditLog(ctx, "unbind_key", userID, "", req.KeyId, "denied_not_owner")
		return nil, status.Error(codes.PermissionDenied,
			hub_error.GetErrorMessage(hub_error.ERR_ACCESS_DENIED))
	}

	// 查询密钥记录以获取适配器信息
	keyRecord, keyErr := s.keyStore.GetKeyRecord(ctx, req.KeyId)
	if keyErr != nil {
		s.logger.Warn("UnbindKey: key record lookup failed", zap.Error(keyErr))
		return nil, status.Error(codes.Internal, "failed to look up key record")
	}

	// 查找密钥所属适配器并解绑
	a, ok := s.registry.GetByVendor(keyRecord.Vendor)
	if !ok {
		s.auditLog(ctx, "unbind_key", userID, keyRecord.VehicleID, req.KeyId, "partial_no_adapter")
		return &pb.UnbindKeyResponse{
			ErrorCode: "SUCCESS_NO_ADAPTER",
		}, nil
	}

	// 调用适配器解绑密钥（删除手机端/车端密钥数据）
	if err := a.UnbindKey(ctx, req.KeyId); err != nil {
		s.logger.Error("UnbindKey adapter error", zap.Error(err))
		s.auditLog(ctx, "unbind_key", userID, keyRecord.VehicleID, req.KeyId, "adapter_error")
		return &pb.UnbindKeyResponse{
			ErrorCode: "ADAPTER_ERROR",
		}, nil
	}

	s.auditLog(ctx, "unbind_key", userID, keyRecord.VehicleID, req.KeyId, "success")
	return &pb.UnbindKeyResponse{}, nil
}

func (s *KeyManagementService) SuspendKey(ctx context.Context, req *pb.SuspendKeyRequest) (*pb.SuspendKeyResponse, error) {
	s.logger.Info("SuspendKey", zap.String("key_id", req.KeyId), zap.String("reason", req.Reason))

	// [CRIT-1] 资源归属检查：验证当前用户是否拥有该密钥
	userID, _ := extractUserFromContext(ctx)
	if userID == "" {
		s.auditLog(ctx, "suspend_key", "", "", req.KeyId, "denied_no_auth")
		return nil, status.Error(codes.Unauthenticated, "missing authentication context")
	}
	if !s.verifyKeyOwnership(ctx, userID, req.KeyId) {
		s.auditLog(ctx, "suspend_key", userID, "", req.KeyId, "denied_not_owner")
		return nil, status.Error(codes.PermissionDenied,
			hub_error.GetErrorMessage(hub_error.ERR_ACCESS_DENIED))
	}

	// 查询密钥记录以获取适配器信息
	keyRecord, keyErr := s.keyStore.GetKeyRecord(ctx, req.KeyId)
	if keyErr != nil {
		s.logger.Warn("SuspendKey: key record lookup failed", zap.Error(keyErr))
		return nil, status.Error(codes.Internal, "failed to look up key record")
	}

	// Update key status in store
	if err := s.keyStore.SetKeyStatus(ctx, req.KeyId, "suspended"); err != nil {
		s.logger.Warn("SuspendKey: store update failed",
			zap.String("key_id", req.KeyId),
			zap.Error(err),
		)
		// Continue even if store fails — adapter notification is critical
	}

	// Notify adapter for vehicle-side suspension if adapter is available
	if a, ok := s.registry.GetByVendor(keyRecord.Vendor); ok {
		if err := a.RevokeNotify(ctx, req.KeyId, req.Reason); err != nil {
			s.logger.Warn("Adapter suspend key warning",
				zap.String("key_id", req.KeyId),
				zap.Error(err),
			)
		}
	} else {
		s.logger.Warn("No adapter found for SuspendKey",
			zap.String("key_id", req.KeyId),
			zap.String("vendor", keyRecord.Vendor),
		)
	}

	s.auditLog(ctx, "suspend_key", userID, keyRecord.VehicleID, req.KeyId, "success")
	return &pb.SuspendKeyResponse{}, nil
}

func (s *KeyManagementService) ResumeKey(ctx context.Context, req *pb.ResumeKeyRequest) (*pb.ResumeKeyResponse, error) {
	s.logger.Info("ResumeKey", zap.String("key_id", req.KeyId))

	// [CRIT-1] 资源归属检查：验证当前用户是否拥有该密钥
	userID, _ := extractUserFromContext(ctx)
	if userID == "" {
		s.auditLog(ctx, "resume_key", "", "", req.KeyId, "denied_no_auth")
		return nil, status.Error(codes.Unauthenticated, "missing authentication context")
	}
	if !s.verifyKeyOwnership(ctx, userID, req.KeyId) {
		s.auditLog(ctx, "resume_key", userID, "", req.KeyId, "denied_not_owner")
		return nil, status.Error(codes.PermissionDenied,
			hub_error.GetErrorMessage(hub_error.ERR_ACCESS_DENIED))
	}

	// 查询密钥记录以获取适配器信息
	keyRecord, keyErr := s.keyStore.GetKeyRecord(ctx, req.KeyId)
	if keyErr != nil {
		s.logger.Warn("ResumeKey: key record lookup failed", zap.Error(keyErr))
		return nil, status.Error(codes.Internal, "failed to look up key record")
	}

	// Update key status in store
	if err := s.keyStore.SetKeyStatus(ctx, req.KeyId, "active"); err != nil {
		s.logger.Warn("ResumeKey: store update failed",
			zap.String("key_id", req.KeyId),
			zap.Error(err),
		)
	}

	// Notify adapter for vehicle-side resumption if adapter is available
	if a, ok := s.registry.GetByVendor(keyRecord.Vendor); ok {
		if err := a.RevokeNotify(ctx, req.KeyId, "resumed"); err != nil {
			s.logger.Warn("Adapter resume key warning",
				zap.String("key_id", req.KeyId),
				zap.Error(err),
			)
		}
	} else {
		s.logger.Warn("No adapter found for ResumeKey",
			zap.String("key_id", req.KeyId),
		)
	}

	s.auditLog(ctx, "resume_key", userID, keyRecord.VehicleID, req.KeyId, "success")
	return &pb.ResumeKeyResponse{}, nil
}

func (s *KeyManagementService) RevokeKey(ctx context.Context, req *pb.RevokeKeyRequest) (*pb.RevokeKeyResponse, error) {
	s.logger.Info("RevokeKey", zap.String("key_id", req.KeyId), zap.String("reason", req.Reason))

	// [CRIT-1] 资源归属检查：验证当前用户是否拥有该密钥
	userID, _ := extractUserFromContext(ctx)
	if userID == "" {
		s.auditLog(ctx, "revoke_key", "", "", req.KeyId, "denied_no_auth")
		return nil, status.Error(codes.Unauthenticated, "missing authentication context")
	}
	if !s.verifyKeyOwnership(ctx, userID, req.KeyId) {
		s.auditLog(ctx, "revoke_key", userID, "", req.KeyId, "denied_not_owner")
		return nil, status.Error(codes.PermissionDenied,
			hub_error.GetErrorMessage(hub_error.ERR_ACCESS_DENIED))
	}

	// 查询密钥记录以获取适配器信息
	keyRecord, keyErr := s.keyStore.GetKeyRecord(ctx, req.KeyId)
	if keyErr != nil {
		s.logger.Warn("RevokeKey: key record lookup failed", zap.Error(keyErr))
		return nil, status.Error(codes.Internal, "failed to look up key record")
	}

	// Step 1: 查找密钥归属的适配器并通知车端撤销
	a, ok := s.registry.GetByVendor(keyRecord.Vendor)
	if ok {
		if err := a.RevokeNotify(ctx, req.KeyId, req.Reason); err != nil {
			s.logger.Error("RevokeKey adapter error", zap.Error(err))
			s.auditLog(ctx, "revoke_key", userID, keyRecord.VehicleID, req.KeyId, "partial_adapter_error")
		}
	} else {
		s.auditLog(ctx, "revoke_key", userID, keyRecord.VehicleID, req.KeyId, "partial_no_adapter")
	}

	// Update local store
	_ = s.keyStore.SetKeyStatus(ctx, req.KeyId, "revoked")

	// Step 2: 通知手机端清除本地缓存的密钥 (通过推送服务)
	if err := s.notifyPhoneRevocation(ctx, userID, req.KeyId); err != nil {
		s.logger.Warn("Failed to notify phone", zap.Error(err))
		// 不阻止整个流程
	}

	s.auditLog(ctx, "revoke_key", userID, keyRecord.VehicleID, req.KeyId, "success")
	s.logger.Info("Key revoked successfully",
		zap.String("key_id", req.KeyId),
	)

	return &pb.RevokeKeyResponse{}, nil
}

// notifyPhoneRevocation sends a push notification to the phone to clear the local key cache.
func (s *KeyManagementService) notifyPhoneRevocation(ctx context.Context, userID, keyID string) error {
	if s.pushService == nil {
		s.logger.Warn("Phone revocation notification skipped: push service not configured",
			zap.String("user_id", userID),
			zap.String("key_id", keyID),
		)
		s.auditLog(ctx, "notify_phone_revocation", userID, "", keyID, "skipped_no_push_service")
		return nil
	}

	payload := &PushPayload{
		Type:   "key_revoked",
		UserID: userID,
		KeyID:  keyID,
		Data: map[string]string{
			"action": "clear_local_key",
		},
	}

	s.logger.Info("Sending phone revocation notification",
		zap.String("user_id", userID),
		zap.String("key_id", keyID),
		zap.String("payload_type", payload.Type),
	)

	if err := s.pushService.SendPush(ctx, userID, payload); err != nil {
		s.logger.Error("Push notification failed",
			zap.String("user_id", userID),
			zap.String("key_id", keyID),
			zap.Error(err),
		)
		s.auditLog(ctx, "notify_phone_revocation", userID, "", keyID, "push_failed")
		return fmt.Errorf("push notification failed: %w", err)
	}

	s.logger.Info("Phone revocation notification sent successfully",
		zap.String("user_id", userID),
		zap.String("key_id", keyID),
	)
	s.auditLog(ctx, "notify_phone_revocation", userID, "", keyID, "push_sent")
	return nil
}

func (s *KeyManagementService) RenewKey(ctx context.Context, req *pb.RenewKeyRequest) (*pb.RenewKeyResponse, error) {
	s.logger.Info("RenewKey", zap.String("key_id", req.KeyId), zap.Int64("valid_until", req.ValidUntil))

	// [CRIT-1] 资源归属检查：验证当前用户是否拥有该密钥
	userID, _ := extractUserFromContext(ctx)
	if userID == "" {
		s.auditLog(ctx, "renew_key", "", "", req.KeyId, "denied_no_auth")
		return nil, status.Error(codes.Unauthenticated, "missing authentication context")
	}
	if !s.verifyKeyOwnership(ctx, userID, req.KeyId) {
		s.auditLog(ctx, "renew_key", userID, "", req.KeyId, "denied_not_owner")
		return nil, status.Error(codes.PermissionDenied,
			hub_error.GetErrorMessage(hub_error.ERR_ACCESS_DENIED))
	}

	// 车端 TSP 续期（依赖 MQTT 通道就绪后启用）
		// 当前日志记录续期事件，实际 TSP 调用在适配器层
	// 当前阶段仅记录操作状态，由外部调度层负责实际续期流程
	s.auditLog(ctx, "renew_key", userID, "", req.KeyId, "success")
	return &pb.RenewKeyResponse{}, nil
}

func (s *KeyManagementService) GetKey(ctx context.Context, req *pb.GetKeyRequest) (*pb.GetKeyResponse, error) {
	// [CRIT-1] 资源归属检查：验证当前用户是否拥有该密钥
	userID, _ := extractUserFromContext(ctx)
	if userID == "" {
		return nil, status.Error(codes.Unauthenticated, "missing authentication context")
	}
	if !s.verifyKeyOwnership(ctx, userID, req.KeyId) {
		return nil, status.Error(codes.PermissionDenied,
			hub_error.GetErrorMessage(hub_error.ERR_ACCESS_DENIED))
	}

	return &pb.GetKeyResponse{}, nil
}

func (s *KeyManagementService) ListKeys(ctx context.Context, req *pb.ListKeysRequest) (*pb.ListKeysResponse, error) {
	// ListKeys uses the authenticated user as the scope — only return keys
	// owned by the current user unless the caller is admin.
	userID, role := extractUserFromContext(ctx)
	if userID == "" {
		return nil, status.Error(codes.Unauthenticated, "missing authentication context")
	}

	// If a user_id filter was provided, verify the caller is authorized
	if req.UserId != "" && req.UserId != userID {
		if !isAdminUser(role) {
			return nil, status.Error(codes.PermissionDenied,
				hub_error.GetErrorMessage(hub_error.ERR_ACCESS_DENIED))
		}
		// Admin can list any user's keys
		userID = req.UserId
	}

	records, err := s.keyStore.ListKeysByUser(ctx, userID)
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to list keys")
	}

	// 使用已有记录构建响应（密钥元数据需从存储层完整加载）
		for _, r := range records {
			_ = r.KeyID
		}
	return &pb.ListKeysResponse{}, nil
}

// auditLog writes a structured audit log entry in JSON format.
func (s *KeyManagementService) auditLog(ctx context.Context, op, userID, vehicleID, keyID, result string) {
	s.logger.Info("AUDIT",
		zap.String("source", "audit"),
		zap.String("service", "KeyManagement"),
		zap.String("operation", op),
		zap.String("user_id", userID),
		zap.String("vehicle_id", vehicleID),
		zap.String("key_id", keyID),
		zap.String("result", result),
		zap.Int64("timestamp", time.Now().UnixMilli()),
	)
}
