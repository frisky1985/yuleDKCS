package service

import (
	"context"
	"time"

	pb "github.com/digitalkey/proto/dkcs"
	"github.com/digitalkey/dkcs/internal/repository"
	"github.com/digitalkey/dkcs/pkg/logger"
	"github.com/digitalkey/dkcs/pkg/telemetry"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
)

// CommandService implements pb.CommandServiceServer
type CommandService struct {
	pb.UnimplementedCommandServiceServer
	keyRepo     *repository.KeyRepository
	vehicleRepo *repository.VehicleRepository
	logger      *logger.Logger
	telemetry   *telemetry.Telemetry
	eventSvc    *EventService
}

// NewCommandService creates a new CommandService
func NewCommandService(
	keyRepo *repository.KeyRepository,
	vehicleRepo *repository.VehicleRepository,
	logger *logger.Logger,
	telemetry *telemetry.Telemetry,
	eventSvc *EventService,
) *CommandService {
	return &CommandService{
		keyRepo:     keyRepo,
		vehicleRepo: vehicleRepo,
		logger:      logger,
		telemetry:   telemetry,
		eventSvc:    eventSvc,
	}
}

// Unlock sends unlock command
func (s *CommandService) Unlock(ctx context.Context, req *pb.UnlockRequest) (*pb.CommandResponse, error) {
	return s.sendCommand(ctx, req.KeyId, "unlock", req.VehicleId)
}

// Lock sends lock command
func (s *CommandService) Lock(ctx context.Context, req *pb.LockRequest) (*pb.CommandResponse, error) {
	return s.sendCommand(ctx, req.KeyId, "lock", req.VehicleId)
}

// EngineStart sends engine start command
func (s *CommandService) EngineStart(ctx context.Context, req *pb.EngineStartRequest) (*pb.CommandResponse, error) {
	return s.sendCommand(ctx, req.KeyId, "engine_start", req.VehicleId)
}

// EngineStop sends engine stop command
func (s *CommandService) EngineStop(ctx context.Context, req *pb.EngineStopRequest) (*pb.CommandResponse, error) {
	return s.sendCommand(ctx, req.KeyId, "engine_stop", req.VehicleId)
}

// TrunkOpen sends trunk open command
func (s *CommandService) TrunkOpen(ctx context.Context, req *pb.TrunkOpenRequest) (*pb.CommandResponse, error) {
	return s.sendCommand(ctx, req.KeyId, "trunk_open", req.VehicleId)
}

// Panic sends panic alarm command
func (s *CommandService) Panic(ctx context.Context, req *pb.PanicRequest) (*pb.CommandResponse, error) {
	return s.sendCommand(ctx, req.KeyId, "panic", req.VehicleId)
}

// FindVehicle sends find vehicle command (flash lights + honk)
func (s *CommandService) FindVehicle(ctx context.Context, req *pb.FindVehicleRequest) (*pb.CommandResponse, error) {
	return s.sendCommand(ctx, req.KeyId, "find_vehicle", req.VehicleId)
}

// sendCommand is a helper to send commands
func (s *CommandService) sendCommand(ctx context.Context, keyID, cmdType, vehicleID string) (*pb.CommandResponse, error) {
	start := time.Now()
	defer func() {
		s.telemetry.RecordDuration("dkcs.command.duration", time.Since(start), map[string]string{"type": cmdType})
	}()

	s.logger.Info("Send command",
		logger.String("key_id", keyID),
		logger.String("command", cmdType),
		logger.String("vehicle_id", vehicleID),
	)

	// Validate key
	key, err := s.keyRepo.GetByID(ctx, keyID)
	if err != nil {
		s.telemetry.IncCounter("dkcs.command.error", map[string]string{"reason": "key_not_found"})
		return nil, status.Error(codes.NotFound, "key not found")
	}

	// Check key status
	if key.Status != "active" {
		return nil, status.Error(codes.FailedPrecondition, "key is not active")
	}

	// Check permission
	if !key.HasPermission(cmdType) {
		s.telemetry.IncCounter("dkcs.command.error", map[string]string{"reason": "permission_denied"})
		return nil, status.Error(codes.PermissionDenied, "no permission for this command")
	}

	// Validate vehicle
	if key.VehicleID != vehicleID {
		return nil, status.Error(codes.InvalidArgument, "key does not match vehicle")
	}

	vehicle, err := s.vehicleRepo.GetByID(ctx, vehicleID)
	if err != nil {
		return nil, status.Error(codes.NotFound, "vehicle not found")
	}

	// Check if vehicle is online
	if !vehicle.IsOnline {
		return nil, status.Error(codes.Unavailable, "vehicle is offline")
	}

	// Generate command ID
	commandID := generateCommandID()

	// Send command to vehicle via TCU protocol
	// In real implementation, this would send to message queue or direct gRPC to TCU
	err = s.publishCommand(ctx, vehicleID, commandID, cmdType, keyID)
	if err != nil {
		s.logger.Error("Failed to publish command", logger.Err(err))
		s.telemetry.IncCounter("dkcs.command.error", map[string]string{"reason": "publish_failed"})
		return nil, status.Error(codes.Internal, "failed to send command")
	}

	// Record event
	s.eventSvc.RecordEvent(ctx, &repository.Event{
		ID:        uuid.New().String(),
		Type:      "command_sent",
		VehicleID: vehicleID,
		UserID:    key.UserID,
		KeyID:     &keyID,
		Data:      map[string]interface{}{"command": cmdType, "command_id": commandID},
		CreatedAt: time.Now(),
	})

	s.telemetry.IncCounter("dkcs.command.success", map[string]string{"type": cmdType})

	return &pb.CommandResponse{
		CommandId: commandID,
		Status:    "accepted",
		Timestamp: time.Now().Unix(),
	}, nil
}

// GetCommandStatus gets the status of a command
func (s *CommandService) GetCommandStatus(ctx context.Context, req *pb.GetCommandStatusRequest) (*pb.CommandResponse, error) {
	// In real implementation, query command status from cache or database
	// For now, return a mock response
	return &pb.CommandResponse{
		CommandId: req.CommandId,
		Status:    "completed",
		Timestamp: time.Now().Unix(),
	}, nil
}

func generateCommandID() string {
	// Simple implementation - in production use UUID or snowflake
	return time.Now().Format("20060102150405") + randomString(6)
}

func randomString(n int) string {
	const letters = "abcdefghijklmnopqrstuvwxyz0123456789"
	b := make([]byte, n)
	for i := range b {
		b[i] = letters[time.Now().UnixNano()%int64(len(letters))]
		time.Sleep(1) // Simple entropy
	}
	return string(b)
}
