package tsp

import (
	"context"
	"time"

	"go.uber.org/zap"
	pb "github.com/digitalkey/hub/api/v1"
)

// Service TSP对接服务 - DKCS与车厂TSP系统的桥梁
type Service struct {
	pb.UnimplementedVehicleControlServiceServer
	logger *zap.Logger
}

func NewService(logger *zap.Logger) *Service {
	return &Service{
		logger: logger.With(zap.String("service", "TSP-Bridge")),
	}
}

func (s *Service) SendCommand(ctx context.Context, req *pb.ControlCommandRequest) (*pb.ControlCommandResponse, error) {
	s.logger.Info("TSP: forwarding command",
		zap.String("vehicle_id", req.VehicleId),
		zap.String("action", req.Action),
	)

	// 转换为TSP私有API调用
	// 每家车厂TSP接口不同，这里做适配
	// 例: POST https://tsp.carbrand.com/api/v2/vehicle/{vin}/command
	//     {"action": "unlock", "source": "digital_key", "key_id": "..."}

	// MQTT下发到TCU
	// topic: vehicle/{vehicle_id}/command
	// payload: {"cmd_id":"...","action":"unlock","timestamp":...}

	return &pb.ControlCommandResponse{
		CmdId:     "tsp-cmd-" + req.VehicleId,
		ResultCode: 0,
	}, nil
}

func (s *Service) StreamStatus(req *pb.VehicleStatusRequest, stream pb.VehicleControlService_StreamStatusServer) error {
	// 订阅TSP车辆状态推送
	// 转换为统一模型后推送
	ticker := time.NewTicker(5 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case <-stream.Context().Done():
			return stream.Context().Err()
		case <-ticker.C:
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
