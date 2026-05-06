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

// EventService implements pb.EventServiceServer
type EventService struct {
	pb.UnimplementedEventServiceServer
	eventRepo *repository.EventRepository
	logger    *logger.Logger
	telemetry *telemetry.Telemetry
}

// NewEventService creates a new EventService
func NewEventService(
	eventRepo *repository.EventRepository,
	logger *logger.Logger,
	telemetry *telemetry.Telemetry,
) *EventService {
	return &EventService{
		eventRepo: eventRepo,
		logger:    logger,
		telemetry: telemetry,
	}
}

// RecordEvent records an event
func (s *EventService) RecordEvent(ctx context.Context, event *repository.Event) error {
	if err := s.eventRepo.Create(ctx, event); err != nil {
		s.logger.Error("Failed to record event", logger.Err(err))
		return err
	}

	s.telemetry.IncCounter("dkcs.event.recorded", map[string]string{"type": event.Type})
	return nil
}

// ListEvents lists events for a vehicle
func (s *EventService) ListEvents(ctx context.Context, req *pb.ListEventsRequest) (*pb.ListEventsResponse, error) {
	events, err := s.eventRepo.ListByVehicle(ctx, req.VehicleId, int(req.Limit), int(req.Offset))
	if err != nil {
		s.logger.Error("Failed to list events", logger.Err(err))
		return nil, status.Error(codes.Internal, "failed to list events")
	}

	resp := &pb.ListEventsResponse{}
	for _, event := range events {
		resp.Events = append(resp.Events, &pb.Event{
			EventId:   event.ID,
			Type:      event.Type,
			VehicleId: event.VehicleID,
			UserId:    event.UserID,
			KeyId:     event.KeyID,
			Data:      convertDataToMap(event.Data),
			Timestamp: event.CreatedAt.Unix(),
		})
	}

	return resp, nil
}

// StreamEvents streams events in real-time
func (s *EventService) StreamEvents(req *pb.StreamEventsRequest, stream pb.EventService_StreamEventsServer) error {
	s.logger.Info("StreamEvents started", logger.String("vehicle_id", req.VehicleId))

	// In real implementation, subscribe to event stream from message queue
	// For now, simulate with periodic queries
	ticker := time.NewTicker(1 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case <-stream.Context().Done():
			s.logger.Info("StreamEvents cancelled")
			return nil
		case <-ticker.C:
			// Query recent events
			events, err := s.eventRepo.ListByVehicle(stream.Context(), req.VehicleId, 10, 0)
			if err != nil {
				s.logger.Error("Failed to query events", logger.Err(err))
				continue
			}

			for _, event := range events {
				if err := stream.Send(&pb.Event{
					EventId:   event.ID,
					Type:      event.Type,
					VehicleId: event.VehicleID,
					UserId:    event.UserID,
					KeyId:     event.KeyID,
					Data:      convertDataToMap(event.Data),
					Timestamp: event.CreatedAt.Unix(),
				}); err != nil {
					s.logger.Error("Failed to send event", logger.Err(err))
					return err
				}
			}
		}
	}
}

// GetEventStats gets event statistics
func (s *EventService) GetEventStats(ctx context.Context, req *pb.GetEventStatsRequest) (*pb.GetEventStatsResponse, error) {
	stats, err := s.eventRepo.GetStats(ctx, req.VehicleId, req.StartTime, req.EndTime)
	if err != nil {
		s.logger.Error("Failed to get event stats", logger.Err(err))
		return nil, status.Error(codes.Internal, "failed to get stats")
	}

	return &pb.GetEventStatsResponse{
		Stats: convertStatsToProto(stats),
	}, nil
}

func convertDataToMap(data interface{}) map[string]string {
	if m, ok := data.(map[string]interface{}); ok {
		result := make(map[string]string)
		for k, v := range m {
			result[k] = fmt.Sprintf("%v", v)
		}
		return result
	}
	return nil
}

func convertStatsToProto(stats map[string]int64) []*pb.StatItem {
	var items []*pb.StatItem
	for k, v := range stats {
		items = append(items, &pb.StatItem{
			Key:   k,
			Value: v,
		})
	}
	return items
}
