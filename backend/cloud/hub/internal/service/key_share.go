package service

import (
	"context"

	"go.uber.org/zap"
	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/adapter"
	pb_relay "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/relay/v1"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/relay"
)

type KeyShareService struct {
	pb.UnimplementedKeyShareServiceServer
	registry          *adapter.Registry
	mailboxController *relay.MailboxController
	logger            *zap.Logger
}

func NewKeyShareService(registry *adapter.Registry, mailboxController *relay.MailboxController, logger *zap.Logger) *KeyShareService {
	return &KeyShareService{
		registry:          registry,
		mailboxController: mailboxController,
		logger:            logger.With(zap.String("service", "KeyShare")),
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

	// 1. 先走厂商适配器生成分享信息
	resp, err := a.ShareKey(ctx, req)
	if err != nil {
		return nil, err
	}
	if resp.ErrorCode != "" {
		return resp, nil
	}

	// 2. 创建 Mailbox 作为底层传输通道（CCC §11.3.4）
	//    Mailbox 承载加密的密钥配置数据，发送方凭 sharing_url 传给接收方
	mbReq := &pb_relay.CreateMailboxRequest{
		Payload:          []byte(req.KeyId),              // 预留：后续传入加密的 provisioning payload
		SenderDeviceId:   req.FromUserId,
		SenderVendor:     req.ToVendor.String(),
		NotificationToken: "",                            // Push token — Phase C 接入
		Config: &pb_relay.MailboxConfig{
			AccessRights:      pb_relay.AccessRights_READ_WRITE_DELETE,
			ExpirationSeconds: 86400,                     // 24h TTL
			MaxUpdates:        10,
		},
		TraceId: resp.ShareId,
	}

	mb, err := s.mailboxController.Create(ctx, mbReq)
	if err != nil {
		s.logger.Error("CreateMailbox failed", zap.Error(err))
		// 不阻塞分享流程 — 分享本身已创建成功
		return resp, nil
	}

	s.logger.Info("Mailbox created for share",
		zap.String("share_id", resp.ShareId),
		zap.String("mailbox_id", mb.MailboxId),
		zap.String("sharing_url", mb.SharingUrl),
	)

	return resp, nil
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
