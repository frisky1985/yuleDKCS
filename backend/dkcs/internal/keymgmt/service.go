package keymgmt

import (
	"context"
	"encoding/json"
	"fmt"

	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
	"github.com/google/uuid"
	"go.uber.org/zap"
)

// Service DKCS密钥管理服务
// 接收车厂内部系统的密钥操作请求，通过HUB转发到手机厂商云端。
//
// SECURITY: 本服务不生成密钥材料 — 密钥只能由 HSM/厂商后端产生。
// HUB 未返回非空 payload 时一律失败 (fail-closed)，绝不下发占位密钥。
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

// forward 将操作转发到 HUB；返回 HUB 的原始响应。
func (s *Service) forward(ctx context.Context, vendor pb.PhoneVendor, protocol pb.Protocol,
	operation string, payload []byte, traceID string) (*pb.ForwardResponse, error) {
	if s.hubClient == nil {
		return nil, fmt.Errorf("hub transport client not configured")
	}
	return s.hubClient.ForwardToVendor(ctx, &pb.ForwardRequest{
		Vendor:    vendor,
		Protocol:  protocol,
		Operation: operation,
		Payload:   payload,
		TraceId:   traceID,
	})
}

func (s *Service) BindKey(ctx context.Context, req *pb.BindKeyRequest) (*pb.BindKeyResponse, error) {
	s.logger.Info("BindKey: forwarding to HUB",
		zap.String("vehicle_id", req.VehicleId),
		zap.String("vendor", req.Vendor.String()),
	)

	// 车辆归属/配额校验由上游 DKCS 业务层完成；此处只做协议必填项门禁。
	if req.VehicleId == "" || req.DeviceId == "" {
		return &pb.BindKeyResponse{
			ErrorCode: "INVALID_ARGUMENT",
			ErrorMsg:  "vehicle_id and device_id are required",
		}, nil
	}

	payload, err := json.Marshal(map[string]interface{}{
		"vehicle_id":    req.VehicleId,
		"device_id":     req.DeviceId,
		"user_id":       req.UserId,
		"key_type":      req.KeyType.String(),
		"device_pubkey": req.DevicePubkey,
		"access_level":  req.AccessLevel.String(),
		"valid_from":    req.ValidFrom,
		"valid_until":   req.ValidUntil,
	})
	if err != nil {
		return &pb.BindKeyResponse{ErrorCode: "INTERNAL", ErrorMsg: err.Error()}, nil
	}

	resp, err := s.forward(ctx, req.Vendor, req.Protocol, "bind", payload, req.TraceId)
	if err != nil {
		s.logger.Error("HUB forward failed", zap.Error(err))
		return &pb.BindKeyResponse{ErrorCode: "HUB_ERROR", ErrorMsg: err.Error()}, nil
	}
	if resp.ErrorCode != "" {
		return &pb.BindKeyResponse{ErrorCode: resp.ErrorCode, ErrorMsg: resp.ErrorMsg}, nil
	}

	// 关键门禁: 没有厂商返回的密钥材料就绝不下发钥匙。
	if len(resp.Payload) == 0 {
		return &pb.BindKeyResponse{
			ErrorCode: "VENDOR_EMPTY_RESPONSE",
			ErrorMsg:  "vendor returned no key material",
		}, nil
	}

	return &pb.BindKeyResponse{
		Key: &pb.DigitalKey{
			KeyId:      "dkcs-" + uuid.NewString(),
			VehicleId:  req.VehicleId,
			DeviceId:   req.DeviceId,
			UserId:     req.UserId,
			KeyType:    req.KeyType,
			Protocol:   req.Protocol,
			Status:     pb.KeyStatus_ACTIVE,
			ValidFrom:  req.ValidFrom,
			ValidUntil: req.ValidUntil,
		},
	}, nil
}

func (s *Service) UnbindKey(ctx context.Context, req *pb.UnbindKeyRequest) (*pb.UnbindKeyResponse, error) {
	resp, err := s.forwardKeyOp(ctx, "unbind", req.KeyId, req.TraceId, nil)
	if err != nil {
		return &pb.UnbindKeyResponse{ErrorCode: "HUB_ERROR"}, nil
	}
	return &pb.UnbindKeyResponse{ErrorCode: resp.ErrorCode}, nil
}

func (s *Service) SuspendKey(ctx context.Context, req *pb.SuspendKeyRequest) (*pb.SuspendKeyResponse, error) {
	resp, err := s.forwardKeyOp(ctx, "suspend", req.KeyId, req.TraceId,
		map[string]string{"reason": req.Reason})
	if err != nil {
		return &pb.SuspendKeyResponse{ErrorCode: "HUB_ERROR"}, nil
	}
	return &pb.SuspendKeyResponse{ErrorCode: resp.ErrorCode}, nil
}

func (s *Service) ResumeKey(ctx context.Context, req *pb.ResumeKeyRequest) (*pb.ResumeKeyResponse, error) {
	resp, err := s.forwardKeyOp(ctx, "resume", req.KeyId, req.TraceId, nil)
	if err != nil {
		return &pb.ResumeKeyResponse{ErrorCode: "HUB_ERROR"}, nil
	}
	return &pb.ResumeKeyResponse{ErrorCode: resp.ErrorCode}, nil
}

func (s *Service) RevokeKey(ctx context.Context, req *pb.RevokeKeyRequest) (*pb.RevokeKeyResponse, error) {
	resp, err := s.forwardKeyOp(ctx, "revoke", req.KeyId, req.TraceId,
		map[string]string{"reason": req.Reason})
	if err != nil {
		return &pb.RevokeKeyResponse{ErrorCode: "HUB_ERROR"}, nil
	}
	return &pb.RevokeKeyResponse{ErrorCode: resp.ErrorCode}, nil
}

func (s *Service) RenewKey(ctx context.Context, req *pb.RenewKeyRequest) (*pb.RenewKeyResponse, error) {
	resp, err := s.forwardKeyOp(ctx, "renew", req.KeyId, req.TraceId,
		map[string]interface{}{"valid_until": req.ValidUntil})
	if err != nil {
		return &pb.RenewKeyResponse{ErrorCode: "HUB_ERROR"}, nil
	}
	return &pb.RenewKeyResponse{ErrorCode: resp.ErrorCode}, nil
}

// forwardKeyOp 转发非绑定类密钥操作。vendor/protocol 由 HUB 根据 key_id 解析，
// 这里留空 (proto3 零值)，payload 携带操作参数。
func (s *Service) forwardKeyOp(ctx context.Context, operation, keyID, traceID string,
	extra interface{}) (*pb.ForwardResponse, error) {
	if keyID == "" {
		return nil, fmt.Errorf("key_id is required")
	}
	body := map[string]interface{}{"key_id": keyID}
	if extra != nil {
		if m, ok := extra.(map[string]string); ok {
			for k, v := range m {
				body[k] = v
			}
		} else if m, ok := extra.(map[string]interface{}); ok {
			for k, v := range m {
				body[k] = v
			}
		}
	}
	payload, err := json.Marshal(body)
	if err != nil {
		return nil, err
	}
	return s.forward(ctx, pb.PhoneVendor_VENDOR_UNSPECIFIED, pb.Protocol_PROTOCOL_UNSPECIFIED,
		operation, payload, traceID)
}

// GetKey / ListKeys 尚未接入 DKCS 持久化仓储。
// 返回显式 NOT_IMPLEMENTED，避免调用方把空响应误判为"无密钥"。
func (s *Service) GetKey(ctx context.Context, req *pb.GetKeyRequest) (*pb.GetKeyResponse, error) {
	return &pb.GetKeyResponse{
		ErrorCode: "NOT_IMPLEMENTED",
	}, nil
}

func (s *Service) ListKeys(ctx context.Context, req *pb.ListKeysRequest) (*pb.ListKeysResponse, error) {
	// ListKeysResponse 无错误字段；显式记录日志，避免静默返回空列表。
	s.logger.Warn("ListKeys: not implemented (returning empty list)",
		zap.String("user_id", req.UserId), zap.String("vehicle_id", req.VehicleId))
	return &pb.ListKeysResponse{}, nil
}
