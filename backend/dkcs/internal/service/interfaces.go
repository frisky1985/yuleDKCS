package service

import (
	"context"
	"time"

	"github.com/frisky1985/yuleDKCS/backend/dkcs/internal/repository"
)

// KeyRepository defines the interface for key data operations.
// The concrete repository.KeyRepository satisfies this interface.
type KeyRepository interface {
	Create(ctx context.Context, key *repository.Key) error
	GetByID(ctx context.Context, id string) (*repository.Key, error)
	Update(ctx context.Context, key *repository.Key) error
	Delete(ctx context.Context, id string) error
	ListByUser(ctx context.Context, userID string, limit, offset int) ([]*repository.Key, error)
	ListByVehicle(ctx context.Context, vehicleID string, limit, offset int) ([]*repository.Key, error)
}

// VehicleRepository defines the interface for vehicle data operations.
// The concrete repository.VehicleRepository satisfies this interface.
type VehicleRepository interface {
	GetByID(ctx context.Context, id string) (*repository.Vehicle, error)
}

// Logger defines the interface for logging.
// The concrete logger.Logger satisfies this interface.
type Logger interface {
	Info(msg string, fields ...interface{})
	Error(msg string, fields ...interface{})
	Warn(msg string, fields ...interface{})
}

// Telemetry defines the interface for telemetry/metrics collection.
// The concrete telemetry.Telemetry satisfies this interface.
type Telemetry interface {
	IncCounter(name string, labels map[string]string)
	RecordDuration(name string, d time.Duration)
}
