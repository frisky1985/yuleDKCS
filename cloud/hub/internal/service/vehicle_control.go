package service

import (
	"context"

	"go.uber.org/zap"
	pb "github.com/digitalkey/hub/api/v1"
)

type VehicleControlService struct {
	pb.UnimplementedVehicleControlServiceServer
	logger *zap.Logger
}

func NewVehicleControlService(logger *zap.Logger) *VehicleControlService {
	return &VehicleControlService{
		logger: logger.With(zap.String("service", "VehicleControl")),
	}
}

func (s *VehicleControlService) SendCommand(ctx context.Context, req *pb.ControlCommandRequest) (*pb.ControlCommandResponse, error) {
	s.logger.Info("SendCommand",
		zap.String("vehicle_id", req.VehicleId),
		zap.String("action", req.Action),
		zap.String("key_id", req.KeyId),
	)

	// 1. 验证密钥权限
	// key := s.keyRepo.GetByKeyID(req.KeyId)
	// if !hasPermission(key.AccessLevel, req.Action) {
	//     return error
	// }

	// 2. 通过MQTT发送指令到TCU
	// topic: vehicle/{vehicle_id}/command
	// payload: {"cmd_id":"...","action":"unlock","source":"remote","timestamp":...}

	// 3. 等待TCU响应 (异步, 通过Kafka回调更新状态)

	return &pb.ControlCommandResponse{
		CmdId:     "cmd-" + req.VehicleId + "-" + req.Action,
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
