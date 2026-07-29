package relay

import (
	"context"

	"go.uber.org/zap"
	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/relay/v1"
)

// RelayService 实现 CCC Digital Key v4.0 Relay Server 的 Mailbox API
// 作为 Hub 的内部服务，支持跨厂商密钥分享
type RelayService struct {
	pb.UnimplementedRelayServiceServer
	controller *MailboxController
	logger     *zap.Logger
}

func NewRelayService(logger *zap.Logger) *RelayService {
	return &RelayService{
		controller: NewMailboxController(logger),
		logger:     logger.With(zap.String("service", "relay")),
	}
}

// ─── Mailbox API 实现 ────────────────────────────────────────

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
			ErrorCode: "CREATE_FAILED",
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
			ErrorCode: "UPDATE_FAILED",
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
			ErrorCode: "DELETE_FAILED",
		}, nil
	}

	return &pb.DeleteMailboxResponse{
		Success: true,
	}, nil
}

// ReadDisplayInformation §11.3.4.4: 读取展示信息
func (s *RelayService) ReadDisplayInformation(ctx context.Context, req *pb.ReadDisplayInformationRequest) (*pb.ReadDisplayInformationResponse, error) {
	info, version, err := s.controller.ReadDisplayInfo(ctx, req.MailboxId)
	if err != nil {
		return &pb.ReadDisplayInformationResponse{
			ErrorCode: "READ_FAILED",
		}, nil
	}

	return &pb.ReadDisplayInformationResponse{
		DisplayInfo: info,
		Version:     version,
	}, nil
}

// ReadSecureContent §11.3.4.5: 读取加密内容
func (s *RelayService) ReadSecureContent(ctx context.Context, req *pb.ReadSecureContentRequest) (*pb.ReadSecureContentResponse, error) {
	payload, version, err := s.controller.ReadSecureContent(ctx, req.MailboxId)
	if err != nil {
		return &pb.ReadSecureContentResponse{
			ErrorCode: "READ_FAILED",
			ErrorMsg:  err.Error(),
		}, nil
	}

	return &pb.ReadSecureContentResponse{
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
			ErrorCode: "RELINQUISH_FAILED",
			ErrorMsg:  err.Error(),
		}, nil
	}

	return &pb.RelinquishMailboxResponse{
		Success: true,
	}, nil
}
