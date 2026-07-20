package adapter

import (
	"context"
	"fmt"
	"time"

	"go.uber.org/zap"
	pb "github.com/digitalkey/hub/api/v1"
)

// ICCOAAdapter ICCOA协议适配器 (小米/OPPO/vivo)
// 对接小米钱包/OPPO钱包/vivo钱包数字钥匙服务
type ICCOAAdapter struct {
	vendor string
	logger *zap.Logger
}

func NewICCOAAdapter(vendor string, logger *zap.Logger) *ICCOAAdapter {
	return &ICCOAAdapter{
		vendor: vendor,
		logger: logger.With(zap.String("vendor", vendor), zap.String("protocol", "iccoa_dk40")),
	}
}

func (a *ICCOAAdapter) Vendor() string   { return a.vendor }
func (a *ICCOAAdapter) Protocol() string { return "iccoa_dk40" }

func (a *ICCOAAdapter) BindKey(ctx context.Context, req *pb.BindKeyRequest) (*pb.BindKeyResponse, error) {
	a.logger.Info("BindKey: ICCOA DK4.0 binding",
		zap.String("vehicle_id", req.VehicleId),
		zap.String("user_id", req.UserId),
	)

	// ICCOA DK4.0绑定流程:
	// 1. 设备能力协商 (BLE 5.0 + UWB FiRa)
	// 2. 车端生成密钥对
	// 3. DK4.0 BLE配对帧交换
	// 4. UWB配置下发
	// 5. 调用厂商钱包API注册
	//    小米: POST /api/v1/carkey/bind
	//    OPPO: POST /ocar/v2/key/bind
	//    vivo: POST /vivotsp/v1/digitalkey/bind

	vehiclePubKey := make([]byte, 64)
	sharedSecret := make([]byte, 32)

	return &pb.BindKeyResponse{
		Key: &pb.DigitalKey{
			KeyId:       fmt.Sprintf("key-iccoa-%d", time.Now().UnixMilli()),
			VehicleId:   req.VehicleId,
			DeviceId:    req.DeviceId,
			UserId:      req.UserId,
			KeyType:     req.KeyType,
			Protocol:    pb.Protocol_ICCOA_DK40,
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

func (a *ICCOAAdapter) UnbindKey(ctx context.Context, keyID string) error {
	a.logger.Info("UnbindKey", zap.String("key_id", keyID))
	// 小米: DELETE /api/v1/carkey/{key_id}
	// OPPO: DELETE /ocar/v2/key/{key_id}
	// vivo: DELETE /vivotsp/v1/digitalkey/{key_id}
	return nil
}

func (a *ICCOAAdapter) RevokeNotify(ctx context.Context, keyID string, reason string) error {
	a.logger.Info("RevokeNotify", zap.String("key_id", keyID))
	// 厂商推送: 小米Push/OPPO Push/vivo Push
	return nil
}

func (a *ICCOAAdapter) ShareKey(ctx context.Context, req *pb.CreateShareRequest) (*pb.CreateShareResponse, error) {
	a.logger.Info("ShareKey: ICCOA DK4.0 sharing", zap.String("key_id", req.KeyId))
	// ICCOA分享: 支持跨厂商分享 (ICCOA联盟内互认)
	// 小米: POST /api/v1/carkey/{key_id}/share
	// OPPO: POST /ocar/v2/key/{key_id}/share
	return &pb.CreateShareResponse{
		ShareId:   fmt.Sprintf("share-iccoa-%d", time.Now().UnixMilli()),
		ShareCode: fmt.Sprintf("%06d", time.Now().UnixNano()%1000000),
	}, nil
}

func (a *ICCOAAdapter) AcceptShare(ctx context.Context, req *pb.AcceptShareRequest) (*pb.AcceptShareResponse, error) {
	a.logger.Info("AcceptShare", zap.String("vendor", req.Vendor.String()))
	return &pb.AcceptShareResponse{
		Key: &pb.DigitalKey{
			KeyId:    fmt.Sprintf("key-iccoa-share-%d", time.Now().UnixMilli()),
			KeyType:  pb.KeyType_FRIEND,
			Protocol: pb.Protocol_ICCOA_DK40,
			Status:   pb.KeyStatus_ACTIVE,
		},
	}, nil
}

func (a *ICCOAAdapter) Notify(ctx context.Context, userID string, notification *pb.VehicleStatusUpdate) error {
	return nil
}

func (a *ICCOAAdapter) HealthCheck(ctx context.Context) (*pb.AdapterStatus, error) {
	return &pb.AdapterStatus{
		Vendor:      a.vendor,
		Protocol:    "iccoa_dk40",
		Healthy:     true,
		LastCheckMs: time.Now().UnixMilli(),
	}, nil
}
