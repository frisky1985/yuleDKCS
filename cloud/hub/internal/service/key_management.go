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
	// 查找密钥所属适配器 → 调用解绑
	return &pb.UnbindKeyResponse{}, nil
}

func (s *KeyManagementService) SuspendKey(ctx context.Context, req *pb.SuspendKeyRequest) (*pb.SuspendKeyResponse, error) {
	s.logger.Info("SuspendKey", zap.String("key_id", req.KeyId))
	return &pb.SuspendKeyResponse{}, nil
}

func (s *KeyManagementService) ResumeKey(ctx context.Context, req *pb.ResumeKeyRequest) (*pb.ResumeKeyResponse, error) {
	s.logger.Info("ResumeKey", zap.String("key_id", req.KeyId))
	return &pb.ResumeKeyResponse{}, nil
}

func (s *KeyManagementService) RevokeKey(ctx context.Context, req *pb.RevokeKeyRequest) (*pb.RevokeKeyResponse, error) {
	s.logger.Info("RevokeKey", zap.String("key_id", req.KeyId))
	// 通知车端撤销 + 通知手机端删除
	return &pb.RevokeKeyResponse{}, nil
}

func (s *KeyManagementService) RenewKey(ctx context.Context, req *pb.RenewKeyRequest) (*pb.RenewKeyResponse, error) {
	s.logger.Info("RenewKey", zap.String("key_id", req.KeyId))
	return &pb.RenewKeyResponse{}, nil
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
