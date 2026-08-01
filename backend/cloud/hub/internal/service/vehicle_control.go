package service

import (
	"context"
	"fmt"
	"time"

	"go.uber.org/zap"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
	hub_error "github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/error"
)

type VehicleControlService struct {
	pb.UnimplementedVehicleControlServiceServer
	logger     *zap.Logger
	keyStore   KeyStore          // 钥匙元数据 (权限校验)
	dispatcher CommandDispatcher // 指令下发通道 (生产可注入 MQTT 实现)
}

func NewVehicleControlService(logger *zap.Logger) *VehicleControlService {
	return &VehicleControlService{
		logger:     logger.With(zap.String("service", "VehicleControl")),
		keyStore:   NewInMemoryKeyStore(),
		dispatcher: NewNoopCommandDispatcher(logger),
	}
}

// WithKeyStore 注入钥匙存储, 用于 SendCommand 权限校验 (key 存在 + 权限位)。
func (s *VehicleControlService) WithKeyStore(ks KeyStore) *VehicleControlService {
	s.keyStore = ks
	return s
}

// WithCommandDispatcher 注入指令下发通道实现 (MQTT stub 或测试 mock)。
func (s *VehicleControlService) WithCommandDispatcher(d CommandDispatcher) *VehicleControlService {
	s.dispatcher = d
	return s
}

// SendCommand 远程控车指令（source=Remote 场景）
//
// 流程:
//  1. 权限校验: key 存在 + 状态 active + 权限位匹配 (HUB-4)
//  2. 通过 CommandDispatcher 下发指令到车端 TCU (接口化, 可注入)
//  3. 返回 cmd_id 与结果码
//
// BLE/NFC 本地控车请走 UnifiedKeyService (source=1/2/3)
func (s *VehicleControlService) SendCommand(ctx context.Context, req *pb.ControlCommandRequest) (*pb.ControlCommandResponse, error) {
	s.logger.Info("SendCommand",
		zap.String("vehicle_id", req.VehicleId),
		zap.String("action", req.Action),
		zap.String("key_id", req.KeyId),
	)

	// [HUB-4] 1. 权限校验: key 存在
	if req.KeyId == "" {
		return nil, status.Error(codes.InvalidArgument, "key_id is required")
	}
	rec, err := s.keyStore.GetKeyRecord(ctx, req.KeyId)
	if err != nil {
		s.logger.Warn("SendCommand: key not found",
			zap.String("key_id", req.KeyId),
			zap.Error(err),
		)
		return nil, status.Error(codes.NotFound, hub_error.GetErrorMessage(hub_error.ERR_KEY_NOT_FOUND))
	}

	// [HUB-4] 2. 权限校验: key 状态必须 active (suspended/revoked 拒绝)
	if rec.Status != "active" {
		s.logger.Warn("SendCommand: key not active",
			zap.String("key_id", req.KeyId),
			zap.String("status", rec.Status),
		)
		return nil, status.Error(codes.FailedPrecondition,
			fmt.Sprintf("key %s is not active (status=%s)", req.KeyId, rec.Status))
	}

	// [HUB-4] 3. 权限校验: 请求用户必须是钥匙归属方
	if req.UserId != "" && rec.OwnerUserID != "" && req.UserId != rec.OwnerUserID {
		s.logger.Warn("SendCommand: user does not own key",
			zap.String("user_id", req.UserId),
			zap.String("key_id", req.KeyId),
			zap.String("owner_user_id", rec.OwnerUserID),
		)
		return nil, status.Error(codes.PermissionDenied, hub_error.GetErrorMessage(hub_error.ERR_ACCESS_DENIED))
	}

	// [HUB-4] 4. 权限校验: action 对应的权限位必须已授予
	requiredBit, known := actionRequiresBit(req.Action)
	if !known {
		return nil, status.Error(codes.InvalidArgument, hub_error.GetErrorMessage(hub_error.ERR_COMMAND_INVALID))
	}
	if rec.AccessBits&requiredBit == 0 {
		s.logger.Warn("SendCommand: missing permission",
			zap.String("key_id", req.KeyId),
			zap.String("action", req.Action),
			zap.Uint32("access_bits", rec.AccessBits),
		)
		return nil, status.Error(codes.PermissionDenied, hub_error.GetErrorMessage(hub_error.ERR_ACCESS_DENIED))
	}

	// 校验 action 与车辆匹配 (vehicle_id 非空)
	if req.VehicleId == "" {
		return nil, status.Error(codes.InvalidArgument, "vehicle_id is required")
	}

	// 生成命令 id: cmd-{vehicle}-{action}-{随机后缀}
	cmdID := fmt.Sprintf("cmd-%s-%s-%d", req.VehicleId, req.Action, time.Now().UnixNano())

	// 通过下发通道发送指令到车端 TCU
	cmd := &ControlCommand{
		CmdID:     cmdID,
		VehicleID: req.VehicleId,
		KeyID:     req.KeyId,
		Action:    req.Action,
		Params:    req.Params,
		Source:    req.Source,
		TraceID:   req.TraceId,
		Timestamp: time.Now().UnixMilli(),
	}
	if err := s.dispatcher.Send(ctx, cmd); err != nil {
		s.logger.Error("SendCommand: dispatch failed",
			zap.String("cmd_id", cmdID),
			zap.String("vehicle_id", req.VehicleId),
			zap.Error(err),
		)
		return nil, status.Error(codes.Unavailable, hub_error.GetErrorMessage(hub_error.ERR_MQTT_PUBLISH_FAILED))
	}

	s.logger.Info("SendCommand dispatched",
		zap.String("cmd_id", cmdID),
		zap.String("vehicle_id", req.VehicleId),
		zap.String("action", req.Action),
		zap.String("user_id", req.UserId),
	)

	return &pb.ControlCommandResponse{
		CmdId:      cmdID,
		ResultCode: 0,
	}, nil
}

func (s *VehicleControlService) StreamStatus(req *pb.VehicleStatusRequest, stream pb.VehicleControlService_StreamStatusServer) error {
	s.logger.Info("StreamStatus", zap.String("vehicle_id", req.VehicleId))

	// 订阅车辆状态更新 (Redis Pub/Sub 或 Kafka consumer)
	// 实时推送状态变更到客户端
	ticker := time.NewTicker(5 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case <-stream.Context().Done():
			return stream.Context().Err()
		case <-ticker.C:
			// 从缓存获取最新状态
			status := &pb.VehicleStatusUpdate{
				VehicleId: req.VehicleId,
				Timestamp: time.Now().Unix(),
			}
			if err := stream.Send(status); err != nil {
				return err
			}
		}
	}
}
