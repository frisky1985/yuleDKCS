package repository

import (
	"context"
	"database/sql"
	"fmt"
	"time"

	sq "github.com/Masterminds/squirrel"
	"github.com/jmoiron/sqlx"
)

// Event represents an event in the system
type Event struct {
	ID        string                 `db:"id" json:"id"`
	Type      string                 `db:"type" json:"type"`
	VehicleID string                 `db:"vehicle_id" json:"vehicle_id"`
	UserID    string                 `db:"user_id" json:"user_id"`
	KeyID     *string                `db:"key_id" json:"key_id"`
	Data      map[string]interface{} `db:"data" json:"data"`
	CreatedAt time.Time              `db:"created_at" json:"created_at"`
}

// EventRepository handles event data persistence
type EventRepository struct {
	db *sqlx.DB
}

// NewEventRepository creates a new EventRepository
func NewEventRepository(db *sqlx.DB) *EventRepository {
	return &EventRepository{db: db}
}

// Create creates a new event
func (r *EventRepository) Create(ctx context.Context, event *Event) error {
	dataJSON, _ := json.Marshal(event.Data)

	query := sq.Insert("events").
		Columns("id", "type", "vehicle_id", "user_id", "key_id", "data", "created_at").
		Values(event.ID, event.Type, event.VehicleID, event.UserID, event.KeyID, dataJSON, event.CreatedAt).
		Suffix("RETURNING id")

	sqlQuery, args, err := query.ToSql()
	if err != nil {
		return fmt.Errorf("failed to build query: %w", err)
	}

	var returnedID string
	if err := r.db.QueryRowContext(ctx, sqlQuery, args...).Scan(&returnedID); err != nil {
		return fmt.Errorf("failed to create event: %w", err)
	}

	return nil
}

// GetByID retrieves an event by ID
func (r *EventRepository) GetByID(ctx context.Context, id string) (*Event, error) {
	query := sq.Select("*").
		From("events").
		Where(sq.Eq{"id": id}).
		Limit(1)

	sqlQuery, args, err := query.ToSql()
	if err != nil {
		return nil, fmt.Errorf("failed to build query: %w", err)
	}

	var event Event
	var dataJSON []byte
	err = r.db.QueryRowContext(ctx, sqlQuery, args...).Scan(
		&event.ID, &event.Type, &event.VehicleID, &event.UserID, &event.KeyID, &dataJSON, &event.CreatedAt,
	)
	if err != nil {
		if err == sql.ErrNoRows {
			return nil, fmt.Errorf("event not found")
		}
		return nil, fmt.Errorf("failed to get event: %w", err)
	}

	json.Unmarshal(dataJSON, &event.Data)

	return &event, nil
}

// ListByVehicle lists events for a vehicle
func (r *EventRepository) ListByVehicle(ctx context.Context, vehicleID string, limit, offset int) ([]*Event, error) {
	query := sq.Select("*").
		From("events").
		Where(sq.Eq{"vehicle_id": vehicleID}).
		OrderBy("created_at DESC").
		Limit(uint64(limit)).
		Offset(uint64(offset))

	return r.listEvents(ctx, query)
}

// ListByUser lists events for a user
func (r *EventRepository) ListByUser(ctx context.Context, userID string, limit, offset int) ([]*Event, error) {
	query := sq.Select("*").
		From("events").
		Where(sq.Eq{"user_id": userID}).
		OrderBy("created_at DESC").
		Limit(uint64(limit)).
		Offset(uint64(offset))

	return r.listEvents(ctx, query)
}

// ListByKey lists events for a key
func (r *EventRepository) ListByKey(ctx context.Context, keyID string, limit, offset int) ([]*Event, error) {
	query := sq.Select("*").
		From("events").
		Where(sq.Eq{"key_id": keyID}).
		OrderBy("created_at DESC").
		Limit(uint64(limit)).
		Offset(uint64(offset))

	return r.listEvents(ctx, query)
}

// GetStats gets event statistics for a vehicle within a time range
func (r *EventRepository) GetStats(ctx context.Context, vehicleID string, startTime, endTime int64) (map[string]int64, error) {
	query := sq.Select("type", "COUNT(*) as count").
		From("events").
		Where(sq.Eq{"vehicle_id": vehicleID}).
		Where(sq.GtOrEq{"created_at": time.Unix(startTime, 0)}).
		Where(sq.LtOrEq{"created_at": time.Unix(endTime, 0)}).
		GroupBy("type")

	sqlQuery, args, err := query.ToSql()
	if err != nil {
		return nil, fmt.Errorf("failed to build query: %w", err)
	}

	rows, err := r.db.QueryContext(ctx, sqlQuery, args...)
	if err != nil {
		return nil, fmt.Errorf("failed to query event stats: %w", err)
	}
	defer rows.Close()

	stats := make(map[string]int64)
	for rows.Next() {
		var eventType string
		var count int64
		if err := rows.Scan(&eventType, &count); err != nil {
			return nil, fmt.Errorf("failed to scan event stat: %w", err)
		}
		stats[eventType] = count
	}

	return stats, nil
}

func (r *EventRepository) listEvents(ctx context.Context, query sq.SelectBuilder) ([]*Event, error) {
	sqlQuery, args, err := query.ToSql()
	if err != nil {
		return nil, fmt.Errorf("failed to build query: %w", err)
	}

	rows, err := r.db.QueryContext(ctx, sqlQuery, args...)
	if err != nil {
		return nil, fmt.Errorf("failed to query events: %w", err)
	}
	defer rows.Close()

	var events []*Event
	for rows.Next() {
		var event Event
		var dataJSON []byte
		err := rows.Scan(
			&event.ID, &event.Type, &event.VehicleID, &event.UserID, &event.KeyID, &dataJSON, &event.CreatedAt,
		)
		if err != nil {
			return nil, fmt.Errorf("failed to scan event: %w", err)
		}

		json.Unmarshal(dataJSON, &event.Data)
		events = append(events, &event)
	}

	return events, nil
}

// Event types
const (
	EventTypeKeyCreated      = "key_created"
	EventTypeKeyActivated    = "key_activated"
	EventTypeKeyRevoked      = "key_revoked"
	EventTypeKeyShared       = "key_shared"
	EventTypeKeyExpired      = "key_expired"
	EventTypeCommandSent     = "command_sent"
	EventTypeCommandReceived = "command_received"
	EventTypeCommandFailed   = "command_failed"
	EventTypeVehicleOnline   = "vehicle_online"
	EventTypeVehicleOffline  = "vehicle_offline"
	EventTypeVehicleLocation = "vehicle_location"
)
