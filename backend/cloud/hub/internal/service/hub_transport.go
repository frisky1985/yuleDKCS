package service

import (
	"context"
	"fmt"

	"go.uber.org/zap"
	"google.golang.org/protobuf/proto"

	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/adapter"
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
// 根据 operation 分发到对应 adapter 方法
// payload 使用 protobuf 序列化，由各 forwardXxx 方法解析
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

	switch req.Operation {
	case "bind":
		return s.forwardBind(ctx, a, req)
	case "unbind":
		return s.forwardUnbind(ctx, a, req)
	case "share":
		return s.forwardShare(ctx, a, req)
	case "revoke":
		return s.forwardRevoke(ctx, a, req)
	case "notify":
		return s.forwardNotify(ctx, a, req)
	default:
		return &pb.ForwardResponse{
			ErrorCode: "UNSUPPORTED_OPERATION",
			ErrorMsg:  fmt.Sprintf("unsupported operation: %s", req.Operation),
		}, nil
	}
}

// forwardBind 解析 payload → 调用适配器 BindKey
func (s *HubTransportService) forwardBind(ctx context.Context, a adapter.Adapter, req *pb.ForwardRequest) (*pb.ForwardResponse, error) {
	bindReq := &pb.BindKeyRequest{}
	if err := proto.Unmarshal(req.Payload, bindReq); err != nil {
		s.logger.Warn("forwardBind: payload unmarshal failed", zap.Error(err))
		return &pb.ForwardResponse{ErrorCode: "PAYLOAD_PARSE_ERROR", ErrorMsg: err.Error()}, nil
	}

	resp, err := a.BindKey(ctx, bindReq)
	if err != nil {
		return &pb.ForwardResponse{ErrorCode: "ADAPTER_ERROR", ErrorMsg: err.Error()}, nil
	}

	respPayload, _ := proto.Marshal(resp)
	return &pb.ForwardResponse{Payload: respPayload}, nil
}

// forwardUnbind 解析 payload → 调用适配器 UnbindKey
func (s *HubTransportService) forwardUnbind(ctx context.Context, a adapter.Adapter, req *pb.ForwardRequest) (*pb.ForwardResponse, error) {
	unbindReq := &pb.UnbindKeyRequest{}
	if err := proto.Unmarshal(req.Payload, unbindReq); err != nil {
		s.logger.Warn("forwardUnbind: payload unmarshal failed", zap.Error(err))
		return &pb.ForwardResponse{ErrorCode: "PAYLOAD_PARSE_ERROR", ErrorMsg: err.Error()}, nil
	}

	if err := a.UnbindKey(ctx, unbindReq.KeyId); err != nil {
		return &pb.ForwardResponse{ErrorCode: "ADAPTER_ERROR", ErrorMsg: err.Error()}, nil
	}
	return &pb.ForwardResponse{}, nil
}

// forwardShare 解析 payload → 调用适配器 ShareKey
func (s *HubTransportService) forwardShare(ctx context.Context, a adapter.Adapter, req *pb.ForwardRequest) (*pb.ForwardResponse, error) {
	shareReq := &pb.CreateShareRequest{}
	if err := proto.Unmarshal(req.Payload, shareReq); err != nil {
		s.logger.Warn("forwardShare: payload unmarshal failed", zap.Error(err))
		return &pb.ForwardResponse{ErrorCode: "PAYLOAD_PARSE_ERROR", ErrorMsg: err.Error()}, nil
	}

	resp, err := a.ShareKey(ctx, shareReq)
	if err != nil {
		return &pb.ForwardResponse{ErrorCode: "ADAPTER_ERROR", ErrorMsg: err.Error()}, nil
	}

	respPayload, _ := proto.Marshal(resp)
	return &pb.ForwardResponse{Payload: respPayload}, nil
}

// forwardRevoke 调用适配器 RevokeNotify
// payload 作为 keyID
func (s *HubTransportService) forwardRevoke(ctx context.Context, a adapter.Adapter, req *pb.ForwardRequest) (*pb.ForwardResponse, error) {
	if err := a.RevokeNotify(ctx, string(req.Payload), ""); err != nil {
		return &pb.ForwardResponse{ErrorCode: "ADAPTER_ERROR", ErrorMsg: err.Error()}, nil
	}
	return &pb.ForwardResponse{}, nil
}

// forwardNotify 调用适配器 Notify
func (s *HubTransportService) forwardNotify(ctx context.Context, a adapter.Adapter, req *pb.ForwardRequest) (*pb.ForwardResponse, error) {
	s.logger.Warn("forwardNotify: vendor notification not implemented",
		zap.String("vendor", req.Vendor.String()),
	)
	return &pb.ForwardResponse{ErrorCode: "NOT_IMPLEMENTED"}, nil
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
