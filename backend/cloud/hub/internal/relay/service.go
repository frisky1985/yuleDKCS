package relay

import (
	"context"

	"go.uber.org/zap"
	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/relay/v1"
)

// RelayService 实现 CCC Digital Key v4.0 Relay Server 的 Mailbox API
// 基于 CCC-TS-101 §11.3.4 — Mailbox API
type RelayService struct {
	pb.UnimplementedRelayServiceServer
	controller *MailboxController
	logger     *zap.Logger
}

func NewRelayService(logger *zap.Logger, notifier ...PushNotifier) *RelayService {
	ctrl := NewMailboxController(logger)
	if len(notifier) > 0 && notifier[0] != nil {
		ctrl.WithNotifier(notifier[0])
	}
	return &RelayService{
		controller: ctrl,
		logger:     logger.With(zap.String("service", "relay")),
	}
}

// ─── Mailbox API ──────────────────────────────────────────────

// CreateMailbox §11.3.4.1: 发送方创建分享邮箱
func (s *RelayService) CreateMailbox(ctx context.Context, req *pb.CreateMailboxRequest) (*pb.CreateMailboxResponse, error) {
	s.logger.Info("CreateMailbox",
		zap.String("sender", req.SenderVendor),
		zap.String("device", req.SenderDeviceId),
	)

	mb, err := s.controller.Create(ctx, req)
	if err != nil {
		s.logger.Error("CreateMailbox failed", zap.Error(err))
		return &pb.CreateMailboxResponse{
			ErrorCode: ErrCodeCreateFailed,
			ErrorMsg:  err.Error(),
		}, nil
	}

	return &pb.CreateMailboxResponse{
		MailboxId:  mb.MailboxId,
		SharingUrl: mb.SharingUrl,
		ExpiresAt:  mb.ExpiresAt,
	}, nil
}

// UpdateMailbox §11.3.4.2: 任一方更新邮箱内容
func (s *RelayService) UpdateMailbox(ctx context.Context, req *pb.UpdateMailboxRequest) (*pb.UpdateMailboxResponse, error) {
	s.logger.Info("UpdateMailbox",
		zap.String("mailbox_id", req.MailboxId),
		zap.Int32("data_type", req.SharingDataType),
	)

	mb, err := s.controller.Update(ctx, req)
	if err != nil {
		s.logger.Error("UpdateMailbox failed", zap.Error(err))
		return &pb.UpdateMailboxResponse{
			ErrorCode: ErrCodeUpdateFailed,
			ErrorMsg:  err.Error(),
		}, nil
	}

	return &pb.UpdateMailboxResponse{
		Status:  mb.Status,
		Version: mb.Version,
	}, nil
}

// DeleteMailbox §11.3.4.3: 删除邮箱
func (s *RelayService) DeleteMailbox(ctx context.Context, req *pb.DeleteMailboxRequest) (*pb.DeleteMailboxResponse, error) {
	s.logger.Info("DeleteMailbox",
		zap.String("mailbox_id", req.MailboxId),
		zap.String("reason", req.Reason),
	)

	_, err := s.controller.Delete(ctx, req)
	if err != nil {
		return &pb.DeleteMailboxResponse{
			Success:   false,
			ErrorCode: ErrCodeDeleteFailed,
		}, nil
	}

	return &pb.DeleteMailboxResponse{
		Success: true,
	}, nil
}

// ReadDisplayInformationFromMailbox §11.3.4.4: 读取展示信息
func (s *RelayService) ReadDisplayInformationFromMailbox(ctx context.Context, req *pb.ReadDisplayInformationFromMailboxRequest) (*pb.ReadDisplayInformationFromMailboxResponse, error) {
	info, version, err := s.controller.ReadDisplayInfo(ctx, req.MailboxId)
	if err != nil {
		return &pb.ReadDisplayInformationFromMailboxResponse{
			ErrorCode: ErrCodeReadFailed,
		}, nil
	}

	return &pb.ReadDisplayInformationFromMailboxResponse{
		DisplayInfo: info,
		Version:     version,
	}, nil
}

// ReadSecureContentFromMailbox §11.3.4.5: 读取加密内容
// payload 已由设备端 Secret 端到端加密，relay server 不解密
func (s *RelayService) ReadSecureContentFromMailbox(ctx context.Context, req *pb.ReadSecureContentFromMailboxRequest) (*pb.ReadSecureContentFromMailboxResponse, error) {
	payload, version, err := s.controller.ReadSecureContent(ctx, req.MailboxId)
	if err != nil {
		return &pb.ReadSecureContentFromMailboxResponse{
			ErrorCode: ErrCodeReadFailed,
		}, nil
	}

	return &pb.ReadSecureContentFromMailboxResponse{
		Payload: payload,
		Version: version,
	}, nil
}

// RelinquishMailbox §11.3.4.6: 转移邮箱到另一设备
func (s *RelayService) RelinquishMailbox(ctx context.Context, req *pb.RelinquishMailboxRequest) (*pb.RelinquishMailboxResponse, error) {
	_, err := s.controller.Relinquish(ctx, req)
	if err != nil {
		return &pb.RelinquishMailboxResponse{
			Success:   false,
			ErrorCode: ErrCodeRelinquishFailed,
			ErrorMsg:  err.Error(),
		}, nil
	}

	return &pb.RelinquishMailboxResponse{
		Success: true,
	}, nil
}
