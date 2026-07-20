package service

import (
	"context"

	"go.uber.org/zap"
	pb "github.com/digitalkey/hub/api/v1"
	"github.com/digitalkey/hub/internal/adapter"
)

type KeyShareService struct {
	pb.UnimplementedKeyShareServiceServer
	registry *adapter.Registry
	logger   *zap.Logger
}

func NewKeyShareService(registry *adapter.Registry, logger *zap.Logger) *KeyShareService {
	return &KeyShareService{
		registry: registry,
		logger:   logger.With(zap.String("service", "KeyShare")),
	}
}

func (s *KeyShareService) CreateShare(ctx context.Context, req *pb.CreateShareRequest) (*pb.CreateShareResponse, error) {
	s.logger.Info("CreateShare",
		zap.String("key_id", req.KeyId),
		zap.String("from_user", req.FromUserId),
		zap.String("to_vendor", req.ToVendor.String()),
	)

	// 查找目标厂商适配器
	a, ok := s.registry.GetByVendor(req.ToVendor.String())
	if !ok {
		return &pb.CreateShareResponse{
			ErrorCode: "ADAPTER_NOT_FOUND",
		}, nil
	}

	return a.ShareKey(ctx, req)
}

func (s *KeyShareService) AcceptShare(ctx context.Context, req *pb.AcceptShareRequest) (*pb.AcceptShareResponse, error) {
	s.logger.Info("AcceptShare", zap.String("vendor", req.Vendor.String()))

	a, ok := s.registry.GetByVendor(req.Vendor.String())
	if !ok {
		return &pb.AcceptShareResponse{ErrorCode: "ADAPTER_NOT_FOUND"}, nil
	}

	return a.AcceptShare(ctx, req)
}

func (s *KeyShareService) CancelShare(ctx context.Context, req *pb.CancelShareRequest) (*pb.CancelShareResponse, error) {
	return &pb.CancelShareResponse{}, nil
}

func (s *KeyShareService) GetShare(ctx context.Context, req *pb.GetShareRequest) (*pb.GetShareResponse, error) {
	return &pb.GetShareResponse{}, nil
}
