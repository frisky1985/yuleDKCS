package service

import (
	"context"

	"go.uber.org/zap"
	pb "github.com/digitalkey/hub/api/v1"
	"github.com/digitalkey/hub/internal/adapter"
)

type HubTransportService struct {
	pb.UnimplementedHubTransportServiceServer
	registry *adapter.Registry
	logger   *zap.Logger
}

func NewHubTransportService(registry *adapter.Registry, logger *zap.Logger) *HubTransportService {
	return &HubTransportService{
		registry: registry,
		logger:   logger.With(zap.String("service", "HubTransport")),
	}
}

// ForwardToVendor DKCS调用HUB → 转发到手机厂商云端
func (s *HubTransportService) ForwardToVendor(ctx context.Context, req *pb.ForwardRequest) (*pb.ForwardResponse, error) {
	s.logger.Info("ForwardToVendor",
		zap.String("vendor", req.Vendor.String()),
		zap.String("protocol", req.Protocol.String()),
		zap.String("operation", req.Operation),
	)

	a, ok := s.registry.Get(req.Vendor.String(), req.Protocol.String())
	if !ok {
		return &pb.ForwardResponse{
			ErrorCode: "ADAPTER_NOT_FOUND",
			ErrorMsg:  "no adapter for vendor/protocol combination",
		}, nil
	}

	// 根据operation分发
	switch req.Operation {
	case "bind":
		// 解析payload → 调用适配器BindKey
	case "unbind":
		// 解析payload → 调用适配器UnbindKey
	case "share":
		// 解析payload → 调用适配器ShareKey
	case "notify":
		// 解析payload → 调用适配器Notify
	default:
		return &pb.ForwardResponse{
			ErrorCode: "UNSUPPORTED_OPERATION",
		}, nil
	}

	_ = a
	return &pb.ForwardResponse{}, nil
}

// VendorCallback 手机厂商回调HUB
func (s *HubTransportService) VendorCallback(ctx context.Context, req *pb.CallbackRequest) (*pb.CallbackResponse, error) {
	s.logger.Info("VendorCallback",
		zap.String("vendor", req.Vendor.String()),
		zap.String("operation", req.Operation),
	)

	// 验证回调签名
	// 转换为内部模型
	// 转发给DKCS (通过gRPC)

	return &pb.CallbackResponse{}, nil
}

// HealthCheck HUB健康检查
func (s *HubTransportService) HealthCheck(ctx context.Context, req *pb.HealthCheckRequest) (*pb.HealthCheckResponse, error) {
	statuses := s.registry.ListStatus(ctx)
	allHealthy := true
	for _, st := range statuses {
		if !st.Healthy {
			allHealthy = false
			break
		}
	}
	return &pb.HealthCheckResponse{
		Healthy:  allHealthy,
		Adapters: statuses,
	}, nil
}
