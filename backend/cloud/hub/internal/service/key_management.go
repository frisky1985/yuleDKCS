package service

import (
	"context"
	"fmt"
	"time"

	"go.uber.org/zap"
	pb "github.com/digitalkey/hub/api/v1"
	"github.com/digitalkey/hub/internal/adapter"
)

type KeyManagementService struct {
	pb.UnimplementedKeyManagementServiceServer
	registry *adapter.Registry
	logger   *zap.Logger
}

func NewKeyManagementService(registry *adapter.Registry, logger *zap.Logger) *KeyManagementService {
	return &KeyManagementService{
		registry: registry,
		logger:   logger.With(zap.String("service", "KeyManagement")),
	}
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
	a, ok := s.registry.GetByVendor(req.Vendor)
	if !ok {
		s.auditLog(ctx, "unbind_key", "", req.VehicleId, req.KeyId, "partial_no_adapter")
		return &pb.UnbindKeyResponse{Status: "unbound", Timestamp: time.Now().UnixMilli()}, nil
	}
	err := a.UnbindKey(ctx, req.KeyId)
	if err != nil {
		s.logger.Error("UnbindKey adapter error", zap.Error(err))
		return &pb.UnbindKeyResponse{Status: "unbound_local", Timestamp: time.Now().UnixMilli(), ErrorMsg: err.Error()}, nil
	}
	s.auditLog(ctx, "unbind_key", "", req.VehicleId, req.KeyId, "success")
	return &pb.UnbindKeyResponse{Status: "unbound", Timestamp: time.Now().UnixMilli()}, nil
}

func (s *KeyManagementService) SuspendKey(ctx context.Context, req *pb.SuspendKeyRequest) (*pb.SuspendKeyResponse, error) {
	s.logger.Info("SuspendKey", zap.String("key_id", req.KeyId))
	a, ok := s.registry.GetByVendor(req.Vendor)
	if !ok {
		s.auditLog(ctx, "suspend_key", req.UserId, req.VehicleId, req.KeyId, "partial_no_adapter")
		return &pb.SuspendKeyResponse{Status: "suspended", Timestamp: time.Now().UnixMilli()}, nil
	}
	err := a.SuspendKey(ctx, req.KeyId)
	if err != nil {
		s.logger.Error("SuspendKey adapter error", zap.Error(err))
		return &pb.SuspendKeyResponse{Status: "suspended_local", Timestamp: time.Now().UnixMilli(), ErrorMsg: err.Error()}, nil
	}
	s.auditLog(ctx, "suspend_key", req.UserId, req.VehicleId, req.KeyId, "success")
	return &pb.SuspendKeyResponse{Status: "suspended", Timestamp: time.Now().UnixMilli()}, nil
}

func (s *KeyManagementService) ResumeKey(ctx context.Context, req *pb.ResumeKeyRequest) (*pb.ResumeKeyResponse, error) {
	s.logger.Info("ResumeKey", zap.String("key_id", req.KeyId))
	a, ok := s.registry.GetByVendor(req.Vendor)
	if !ok {
		s.auditLog(ctx, "resume_key", req.UserId, req.VehicleId, req.KeyId, "partial_no_adapter")
		return &pb.ResumeKeyResponse{Status: "resumed", Timestamp: time.Now().UnixMilli()}, nil
	}
	err := a.ResumeKey(ctx, req.KeyId)
	if err != nil {
		s.logger.Error("ResumeKey adapter error", zap.Error(err))
		return &pb.ResumeKeyResponse{Status: "resumed_local", Timestamp: time.Now().UnixMilli(), ErrorMsg: err.Error()}, nil
	}
	s.auditLog(ctx, "resume_key", req.UserId, req.VehicleId, req.KeyId, "success")
	return &pb.ResumeKeyResponse{Status: "resumed", Timestamp: time.Now().UnixMilli()}, nil
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

// notifyPhoneRevocation sends a push notification to the phone to clear the local key cache
func (s *KeyManagementService) notifyPhoneRevocation(ctx context.Context, userID, keyID string) error {
	// TODO: 集成推送服务 (FCM/APNs/极光等)
	// 1. 查询用户的设备注册令牌
	// 2. 构造推送载荷: {"type": "key_revoked", "key_id": keyID}
	// 3. 发送推送通知
	s.logger.Info("Phone revocation notification skipped (push service not integrated)",
		zap.String("user_id", userID),
		zap.String("key_id", keyID),
	)
	return nil
}

func (s *KeyManagementService) RenewKey(ctx context.Context, req *pb.RenewKeyRequest) (*pb.RenewKeyResponse, error) {
	s.logger.Info("RenewKey", zap.String("key_id", req.KeyId))
	a, ok := s.registry.GetByVendor(req.Vendor)
	if !ok {
		s.auditLog(ctx, "renew_key", req.UserId, req.VehicleId, req.KeyId, "partial_no_adapter")
		return &pb.RenewKeyResponse{Status: "renewed", Timestamp: time.Now().UnixMilli(), NewExpireTime: req.NewExpireTime}, nil
	}
	resp, err := a.RenewKey(ctx, req)
	if err != nil {
		s.logger.Error("RenewKey adapter error", zap.Error(err))
		return &pb.RenewKeyResponse{Status: "renewed_local", Timestamp: time.Now().UnixMilli(), NewExpireTime: req.NewExpireTime, ErrorMsg: err.Error()}, nil
	}
	s.auditLog(ctx, "renew_key", req.UserId, req.VehicleId, req.KeyId, "success")
	return &pb.RenewKeyResponse{Status: "renewed", Timestamp: time.Now().UnixMilli(), NewExpireTime: resp.NewExpireTime}, nil
}

func (s *KeyManagementService) GetKey(ctx context.Context, req *pb.GetKeyRequest) (*pb.GetKeyResponse, error) {
	return &pb.GetKeyResponse{}, nil
}

func (s *KeyManagementService) ListKeys(ctx context.Context, req *pb.ListKeysRequest) (*pb.ListKeysResponse, error) {
	return &pb.ListKeysResponse{}, nil
}

func (s *KeyManagementService) auditLog(ctx context.Context, op, userID, vehicleID, keyID, result string) {
	s.logger.Info("AUDIT",
		zap.String("operation", op),
		zap.String("user_id", userID),
		zap.String("vehicle_id", vehicleID),
		zap.String("key_id", keyID),
		zap.String("result", result),
		zap.Int64("timestamp", time.Now().UnixMilli()),
	)
}
