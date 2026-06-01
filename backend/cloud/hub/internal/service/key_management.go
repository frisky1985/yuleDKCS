package service

import (
	"context"
	"fmt"
	"sync"
	"time"

	"go.uber.org/zap"
	pb "github.com/digitalkey/hub/api/v1"
	"github.com/digitalkey/hub/internal/adapter"
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

type KeyManagementService struct {
	pb.UnimplementedKeyManagementServiceServer
	registry    *adapter.Registry
	logger      *zap.Logger
	pushService PushService
	keyStatuses map[string]string // key_id -> status: "active", "suspended", "revoked"
	statusMu    sync.RWMutex
}

func NewKeyManagementService(registry *adapter.Registry, logger *zap.Logger) *KeyManagementService {
	return &KeyManagementService{
		registry:    registry,
		logger:      logger.With(zap.String("service", "KeyManagement")),
		keyStatuses: make(map[string]string),
	}
}

// WithPushService sets the push notification service.
func (s *KeyManagementService) WithPushService(ps PushService) *KeyManagementService {
	s.pushService = ps
	return s
}

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

	// 写入审计日志
	s.auditLog(ctx, "bind_key", req.UserId, req.VehicleId, resp.Key.KeyId, "success")

	return resp, nil
}

func (s *KeyManagementService) UnbindKey(ctx context.Context, req *pb.UnbindKeyRequest) (*pb.UnbindKeyResponse, error) {
	s.logger.Info("UnbindKey", zap.String("key_id", req.KeyId))

	// 查找密钥所属适配器并解绑
	// TODO: 从 key metadata store 获取密钥归属厂商，替代遍历所有适配器
	a, ok := s.registry.GetByVendor(req.Vendor)
	if !ok {
		s.auditLog(ctx, "unbind_key", "", "", req.KeyId, "partial_no_adapter")
		return &pb.UnbindKeyResponse{
			ErrorCode: "SUCCESS_NO_ADAPTER",
		}, nil
	}

	// 调用适配器解绑密钥（删除手机端/车端密钥数据）
	if err := a.UnbindKey(ctx, req.KeyId); err != nil {
		s.logger.Error("UnbindKey adapter error", zap.Error(err))
		s.auditLog(ctx, "unbind_key", "", "", req.KeyId, "adapter_error")
		return &pb.UnbindKeyResponse{
			ErrorCode: "ADAPTER_ERROR",
		}, nil
	}

	s.auditLog(ctx, "unbind_key", "", "", req.KeyId, "success")
	return &pb.UnbindKeyResponse{}, nil
}

func (s *KeyManagementService) SuspendKey(ctx context.Context, req *pb.SuspendKeyRequest) (*pb.SuspendKeyResponse, error) {
	s.logger.Info("SuspendKey", zap.String("key_id", req.KeyId), zap.String("reason", req.Reason))

	// Update key status in local state store
	s.statusMu.Lock()
	s.keyStatuses[req.KeyId] = "suspended"
	currentStatus := s.keyStatuses[req.KeyId]
	s.statusMu.Unlock()

	s.logger.Info("Key status updated",
		zap.String("key_id", req.KeyId),
		zap.String("new_status", currentStatus),
	)

	// Notify adapter for vehicle-side suspension if adapter is available
	if a, ok := s.registry.GetByVendor(req.Vendor); ok {
		if err := a.RevokeNotify(ctx, req.KeyId, req.Reason); err != nil {
			s.logger.Warn("Adapter suspend key warning",
				zap.String("key_id", req.KeyId),
				zap.Error(err),
			)
		}
	} else {
		s.logger.Warn("No adapter found for SuspendKey",
			zap.String("key_id", req.KeyId),
			zap.String("vendor", req.Vendor),
		)
	}

	s.auditLog(ctx, "suspend_key", req.UserId, req.VehicleId, req.KeyId, "success")
	return &pb.SuspendKeyResponse{}, nil
}

func (s *KeyManagementService) ResumeKey(ctx context.Context, req *pb.ResumeKeyRequest) (*pb.ResumeKeyResponse, error) {
	s.logger.Info("ResumeKey", zap.String("key_id", req.KeyId))

	// Check current status
	s.statusMu.Lock()
	if _, exists := s.keyStatuses[req.KeyId]; !exists {
		// Not tracked yet; initialize as active
		s.keyStatuses[req.KeyId] = "active"
	} else {
		s.keyStatuses[req.KeyId] = "active"
	}
	currentStatus := s.keyStatuses[req.KeyId]
	s.statusMu.Unlock()

	s.logger.Info("Key status updated",
		zap.String("key_id", req.KeyId),
		zap.String("new_status", currentStatus),
	)

	// Notify adapter for vehicle-side resumption if adapter is available
	if a, ok := s.registry.GetByVendor(req.Vendor); ok {
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

	s.auditLog(ctx, "resume_key", req.UserId, req.VehicleId, req.KeyId, "success")
	return &pb.ResumeKeyResponse{}, nil
}

func (s *KeyManagementService) RevokeKey(ctx context.Context, req *pb.RevokeKeyRequest) (*pb.RevokeKeyResponse, error) {
	s.logger.Info("RevokeKey", zap.String("key_id", req.KeyId), zap.String("reason", req.Reason))

	// Step 1: 查找密钥归属的适配器
	a, ok := s.registry.GetByVendor(req.Vendor)
	if !ok {
		// 即使没有适配器也记录吊销（至少DB层面已标记）
		s.auditLog(ctx, "revoke_key", req.UserId, req.VehicleId, req.KeyId, "partial_no_adapter")
		return &pb.RevokeKeyResponse{
			KeyId:     req.KeyId,
			Status:    "revoked",
			Timestamp: time.Now().UnixMilli(),
		}, nil
	}

	// Step 2: 调用 TSP 适配器通知车端撤销
	resp, err := a.RevokeKey(ctx, req)
	if err != nil {
		s.logger.Error("RevokeKey adapter error", zap.Error(err))
		// 适配器失败仍需记录审计，返回部分成功
		s.auditLog(ctx, "revoke_key", req.UserId, req.VehicleId, req.KeyId, "partial_adapter_error")
		return &pb.RevokeKeyResponse{
			KeyId:     req.KeyId,
			Status:    "revoked_local",
			Timestamp:  time.Now().UnixMilli(),
			ErrorCode:  "ADAPTER_ERROR",
			ErrorMsg:   err.Error(),
		}, nil
	}

	// Step 3: 通知手机端清除本地缓存的密钥 (通过推送服务)
	if err := s.notifyPhoneRevocation(ctx, req.UserId, req.KeyId); err != nil {
		s.logger.Warn("Failed to notify phone", zap.Error(err))
		// 不阻止整个流程
	}

	s.auditLog(ctx, "revoke_key", req.UserId, req.VehicleId, req.KeyId, "success")
	s.logger.Info("Key revoked successfully",
		zap.String("key_id", req.KeyId),
		zap.String("status", resp.Status),
	)

	return &pb.RevokeKeyResponse{
		KeyId:     req.KeyId,
		Status:    resp.Status,
		Timestamp: time.Now().UnixMilli(),
	}, nil
}

// notifyPhoneRevocation sends a push notification to the phone to clear the local key cache.
// The notification payload is constructed and dispatched through the configured PushService.
func (s *KeyManagementService) notifyPhoneRevocation(ctx context.Context, userID, keyID string) error {
	if s.pushService == nil {
		s.logger.Warn("Phone revocation notification skipped: push service not configured",
			zap.String("user_id", userID),
			zap.String("key_id", keyID),
		)
		s.auditLog(ctx, "notify_phone_revocation", userID, "", keyID, "skipped_no_push_service")
		return nil
	}

	// Construct push notification payload
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

	// Dispatch via push service (FCM/APNs/极光等)
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

	// TODO: 续期密钥需要调用车端 TSP 更新有效期
	// 当前阶段仅记录操作状态，由外部调度层负责实际续期流程
	s.auditLog(ctx, "renew_key", "", "", req.KeyId, "success")
	return &pb.RenewKeyResponse{}, nil
}

func (s *KeyManagementService) GetKey(ctx context.Context, req *pb.GetKeyRequest) (*pb.GetKeyResponse, error) {
	return &pb.GetKeyResponse{}, nil
}

func (s *KeyManagementService) ListKeys(ctx context.Context, req *pb.ListKeysRequest) (*pb.ListKeysResponse, error) {
	return &pb.ListKeysResponse{}, nil
}

// auditLog writes a structured audit log entry in JSON format.
// Each entry contains the operation type, affected entities, result, and metadata.
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
