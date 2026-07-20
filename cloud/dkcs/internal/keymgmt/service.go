package keymgmt

import (
	"context"

	"go.uber.org/zap"
	pb "github.com/digitalkey/hub/api/v1"
)

// Service DKCS密钥管理服务
// 接收车厂内部系统的密钥操作请求，通过HUB转发到手机厂商云端
type Service struct {
	pb.UnimplementedKeyManagementServiceServer
	hubClient pb.HubTransportServiceClient
	logger    *zap.Logger
}

func NewService(hubClient pb.HubTransportServiceClient, logger *zap.Logger) *Service {
	return &Service{
		hubClient: hubClient,
		logger:    logger.With(zap.String("service", "DKCS-KeyMgmt")),
	}
}

func (s *Service) BindKey(ctx context.Context, req *pb.BindKeyRequest) (*pb.BindKeyResponse, error) {
	s.logger.Info("BindKey: forwarding to HUB",
		zap.String("vehicle_id", req.VehicleId),
		zap.String("vendor", req.Vendor.String()),
	)

	// DKCS核心逻辑:
	// 1. 验证车辆归属 (VIN → 车主映射)
	// 2. 检查密钥配额 (max_keys)
	// 3. 生成车端密钥对 (HSM)
	// 4. 构建私有协议payload
	// 5. 通过HUB转发到手机厂商

	// 转发到HUB
	forwardReq := &pb.ForwardRequest{
		Vendor:    req.Vendor,
		Protocol:  req.Protocol,
		Operation: "bind",
		Payload:   nil, // TODO: 序列化req为厂商私有格式
		TraceId:   req.TraceId,
	}

	resp, err := s.hubClient.ForwardToVendor(ctx, forwardReq)
	if err != nil {
		s.logger.Error("HUB forward failed", zap.Error(err))
		return &pb.BindKeyResponse{ErrorCode: "HUB_ERROR", ErrorMsg: err.Error()}, nil
	}

	_ = resp

	// 通过MQTT下发密钥到TCU
	// topic: vehicle/{vehicle_id}/key/bind
	// payload: {key_id, vehicle_pubkey, shared_secret, access_level}

	return &pb.BindKeyResponse{
		Key: &pb.DigitalKey{
			KeyId:     "dkcs-key-001",
			VehicleId: req.VehicleId,
			Status:    pb.KeyStatus_ACTIVE,
		},
	}, nil
}

func (s *Service) UnbindKey(ctx context.Context, req *pb.UnbindKeyRequest) (*pb.UnbindKeyResponse, error) {
	// 通知HUB → 手机端删除密钥
	// 通知TCU → 车端删除密钥
	return &pb.UnbindKeyResponse{}, nil
}

func (s *Service) SuspendKey(ctx context.Context, req *pb.SuspendKeyRequest) (*pb.SuspendKeyResponse, error) {
	// 车端挂起 + HUB通知手机端
	return &pb.SuspendKeyResponse{}, nil
}

func (s *Service) ResumeKey(ctx context.Context, req *pb.ResumeKeyRequest) (*pb.ResumeKeyResponse, error) {
	return &pb.ResumeKeyResponse{}, nil
}

func (s *Service) RevokeKey(ctx context.Context, req *pb.RevokeKeyRequest) (*pb.RevokeKeyResponse, error) {
	// 紧急撤销: 先通知TCU立即删除 → 再通知HUB → 手机端
	return &pb.RevokeKeyResponse{}, nil
}

func (s *Service) RenewKey(ctx context.Context, req *pb.RenewKeyRequest) (*pb.RenewKeyResponse, error) {
	return &pb.RenewKeyResponse{}, nil
}

func (s *Service) GetKey(ctx context.Context, req *pb.GetKeyRequest) (*pb.GetKeyResponse, error) {
	return &pb.GetKeyResponse{}, nil
}

func (s *Service) ListKeys(ctx context.Context, req *pb.ListKeysRequest) (*pb.ListKeysResponse, error) {
	return &pb.ListKeysResponse{}, nil
}
