package adapter

import (
	"context"
	"fmt"
	"time"

	"go.uber.org/zap"
	pb "github.com/digitalkey/hub/api/v1"
)

// ICCEAdapter ICCE协议适配器 (华为等)
type ICCEAdapter struct {
	vendor string
	logger *zap.Logger
}

func NewICCEAdapter(vendor string, logger *zap.Logger) *ICCEAdapter {
	return &ICCEAdapter{
		vendor: vendor,
		logger: logger.With(zap.String("vendor", vendor), zap.String("protocol", "icce")),
	}
}

func (a *ICCEAdapter) Vendor() string   { return a.vendor }
func (a *ICCEAdapter) Protocol() string { return "icce" }

func (a *ICCEAdapter) BindKey(ctx context.Context, req *pb.BindKeyRequest) (*pb.BindKeyResponse, error) {
	a.logger.Info("BindKey: ICCE binding",
		zap.String("vehicle_id", req.VehicleId),
		zap.String("user_id", req.UserId),
	)

	// ICCE绑定: 边缘计算 + UWB FiRa配置
	// 华为: POST /api/v2/hwcarkey/bind
	vehiclePubKey := make([]byte, 64)
	sharedSecret := make([]byte, 32)

	return &pb.BindKeyResponse{
		Key: &pb.DigitalKey{
			KeyId:       fmt.Sprintf("key-icce-%d", time.Now().UnixMilli()),
			VehicleId:   req.VehicleId,
			DeviceId:    req.DeviceId,
			UserId:      req.UserId,
			KeyType:     req.KeyType,
			Protocol:    pb.Protocol_ICCE,
			AccessLevel: req.AccessLevel,
			Status:      pb.KeyStatus_ACTIVE,
			ValidFrom:   req.ValidFrom,
			ValidUntil:  req.ValidUntil,
			CreatedAt:   time.Now().Unix(),
		},
		VehiclePubkey: vehiclePubKey,
		SharedSecret:  sharedSecret,
	}, nil
}

func (a *ICCEAdapter) UnbindKey(ctx context.Context, keyID string) error {
	return nil
}

func (a *ICCOAAdapter) RevokeNotify(ctx context.Context, keyID string, reason string) error {
	return nil
}

func (a *ICCEAdapter) RevokeNotify(ctx context.Context, keyID string, reason string) error {
	return nil
}

func (a *ICCEAdapter) ShareKey(ctx context.Context, req *pb.CreateShareRequest) (*pb.CreateShareResponse, error) {
	return &pb.CreateShareResponse{
		ShareId:   fmt.Sprintf("share-icce-%d", time.Now().UnixMilli()),
		ShareCode: fmt.Sprintf("%06d", time.Now().UnixNano()%1000000),
	}, nil
}

func (a *ICCEAdapter) AcceptShare(ctx context.Context, req *pb.AcceptShareRequest) (*pb.AcceptShareResponse, error) {
	return &pb.AcceptShareResponse{
		Key: &pb.DigitalKey{
			KeyId:    fmt.Sprintf("key-icce-share-%d", time.Now().UnixMilli()),
			KeyType:  pb.KeyType_FRIEND,
			Protocol: pb.Protocol_ICCE,
			Status:   pb.KeyStatus_ACTIVE,
		},
	}, nil
}

func (a *ICCEAdapter) Notify(ctx context.Context, userID string, notification *pb.VehicleStatusUpdate) error {
	return nil
}

func (a *ICCEAdapter) HealthCheck(ctx context.Context) (*pb.AdapterStatus, error) {
	return &pb.AdapterStatus{
		Vendor:      a.vendor,
		Protocol:    "icce",
		Healthy:     true,
		LastCheckMs: time.Now().UnixMilli(),
	}, nil
}
