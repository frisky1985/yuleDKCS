package service

import (
	"context"

	"go.uber.org/zap"
	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/adapter"
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

	// 每种协议的 adapter 内部决定分享方式:
	//   CCC → 内部创建 Mailbox 做 payload 中继
	//   ICCOA/ICCE → 直连车服务器 S2S
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
