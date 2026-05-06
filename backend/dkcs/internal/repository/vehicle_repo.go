package repository

import (
	"context"
	"database/sql"
	"fmt"
	"time"

	sq "github.com/Masterminds/squirrel"
	"github.com/jmoiron/sqlx"
	"github.com/redis/go-redis/v9"
)

// Vehicle represents a vehicle entity
type Vehicle struct {
	ID           string    `db:"id" json:"id"`
	OwnerID      string    `db:"owner_id" json:"owner_id"`
	VIN          string    `db:"vin" json:"vin"`
	Brand        string    `db:"brand" json:"brand"`
	Model        string    `db:"model" json:"model"`
	Year         int       `db:"year" json:"year"`
	Color        string    `db:"color" json:"color"`
	PlateNumber  string    `db:"plate_number" json:"plate_number"`
	TCUID        string    `db:"tcu_id" json:"tcu_id"` // TCU device ID
	Protocol     string    `db:"protocol" json:"protocol"` // CCC, ICCOA, ICCE
	IsOnline     bool      `db:"is_online" json:"is_online"`
	LastOnline   *time.Time `db:"last_online" json:"last_online"`
	BatteryLevel int       `db:"battery_level" json:"battery_level"`
	Odometer     int       `db:"odometer" json:"odometer"` // km
	Latitude     float64   `db:"latitude" json:"latitude"`
	Longitude    float64   `db:"longitude" json:"longitude"`
	Features     []string  `db:"features" json:"features"` // Supported features
	CreatedAt    time.Time `db:"created_at" json:"created_at"`
	UpdatedAt    time.Time `db:"updated_at" json:"updated_at"`
}

// VehicleRepository handles vehicle data persistence
type VehicleRepository struct {
	db    *sqlx.DB
	redis *redis.Client
}

// NewVehicleRepository creates a new VehicleRepository
func NewVehicleRepository(db *sqlx.DB, redis *redis.Client) *VehicleRepository {
	return &VehicleRepository{db: db, redis: redis}
}

// Create creates a new vehicle
func (r *VehicleRepository) Create(ctx context.Context, vehicle *Vehicle) error {
	query := sq.Insert("vehicles").
		Columns("id", "owner_id", "vin", "brand", "model", "year", "color", "plate_number", "tcu_id", "protocol", "features").
		Values(vehicle.ID, vehicle.OwnerID, vehicle.VIN, vehicle.Brand, vehicle.Model, vehicle.Year, vehicle.Color, vehicle.PlateNumber, vehicle.TCUID, vehicle.Protocol, vehicle.Features).
		Suffix("RETURNING id")

	sqlQuery, args, err := query.ToSql()
	if err != nil {
		return fmt.Errorf("failed to build query: %w", err)
	}

	var returnedID string
	if err := r.db.QueryRowContext(ctx, sqlQuery, args...).Scan(&returnedID); err != nil {
		return fmt.Errorf("failed to create vehicle: %w", err)
	}

	// Cache vehicle
	r.cacheVehicle(ctx, vehicle)

	return nil
}

// GetByID retrieves a vehicle by ID
func (r *VehicleRepository) GetByID(ctx context.Context, id string) (*Vehicle, error) {
	// Try cache first
	cached, err := r.getCachedVehicle(ctx, id)
	if err == nil && cached != nil {
		return cached, nil
	}

	query := sq.Select("*").
		From("vehicles").
		Where(sq.Eq{"id": id}).
		Limit(1)

	sqlQuery, args, err := query.ToSql()
	if err != nil {
		return nil, fmt.Errorf("failed to build query: %w", err)
	}

	var vehicle Vehicle
	var featuresJSON []byte
	err = r.db.QueryRowContext(ctx, sqlQuery, args...).Scan(
		&vehicle.ID, &vehicle.OwnerID, &vehicle.VIN, &vehicle.Brand, &vehicle.Model,
		&vehicle.Year, &vehicle.Color, &vehicle.PlateNumber, &vehicle.TCUID, &vehicle.Protocol,
		&vehicle.IsOnline, &vehicle.LastOnline, &vehicle.BatteryLevel, &vehicle.Odometer,
		&vehicle.Latitude, &vehicle.Longitude, &featuresJSON, &vehicle.CreatedAt, &vehicle.UpdatedAt,
	)
	if err != nil {
		if err == sql.ErrNoRows {
			return nil, fmt.Errorf("vehicle not found")
		}
		return nil, fmt.Errorf("failed to get vehicle: %w", err)
	}

	// Parse features
	if featuresJSON != nil {
		vehicle.Features = parseStringArray(featuresJSON)
	}

	// Cache for future requests
	r.cacheVehicle(ctx, &vehicle)

	return &vehicle, nil
}

// GetByVIN retrieves a vehicle by VIN
func (r *VehicleRepository) GetByVIN(ctx context.Context, vin string) (*Vehicle, error) {
	query := sq.Select("*").
		From("vehicles").
		Where(sq.Eq{"vin": vin}).
		Limit(1)

	sqlQuery, args, err := query.ToSql()
	if err != nil {
		return nil, fmt.Errorf("failed to build query: %w", err)
	}

	var vehicle Vehicle
	var featuresJSON []byte
	err = r.db.QueryRowContext(ctx, sqlQuery, args...).Scan(
		&vehicle.ID, &vehicle.OwnerID, &vehicle.VIN, &vehicle.Brand, &vehicle.Model,
		&vehicle.Year, &vehicle.Color, &vehicle.PlateNumber, &vehicle.TCUID, &vehicle.Protocol,
		&vehicle.IsOnline, &vehicle.LastOnline, &vehicle.BatteryLevel, &vehicle.Odometer,
		&vehicle.Latitude, &vehicle.Longitude, &featuresJSON, &vehicle.CreatedAt, &vehicle.UpdatedAt,
	)
	if err != nil {
		if err == sql.ErrNoRows {
			return nil, fmt.Errorf("vehicle not found")
		}
		return nil, fmt.Errorf("failed to get vehicle: %w", err)
	}

	if featuresJSON != nil {
		vehicle.Features = parseStringArray(featuresJSON)
	}

	return &vehicle, nil
}

// GetByTCUID retrieves a vehicle by TCU ID
func (r *VehicleRepository) GetByTCUID(ctx context.Context, tcuID string) (*Vehicle, error) {
	query := sq.Select("*").
		From("vehicles").
		Where(sq.Eq{"tcu_id": tcuID}).
		Limit(1)

	sqlQuery, args, err := query.ToSql()
	if err != nil {
		return nil, fmt.Errorf("failed to build query: %w", err)
	}

	var vehicle Vehicle
	var featuresJSON []byte
	err = r.db.QueryRowContext(ctx, sqlQuery, args...).Scan(
		&vehicle.ID, &vehicle.OwnerID, &vehicle.VIN, &vehicle.Brand, &vehicle.Model,
		&vehicle.Year, &vehicle.Color, &vehicle.PlateNumber, &vehicle.TCUID, &vehicle.Protocol,
		&vehicle.IsOnline, &vehicle.LastOnline, &vehicle.BatteryLevel, &vehicle.Odometer,
		&vehicle.Latitude, &vehicle.Longitude, &featuresJSON, &vehicle.CreatedAt, &vehicle.UpdatedAt,
	)
	if err != nil {
		if err == sql.ErrNoRows {
			return nil, fmt.Errorf("vehicle not found")
		}
		return nil, fmt.Errorf("failed to get vehicle: %w", err)
	}

	if featuresJSON != nil {
		vehicle.Features = parseStringArray(featuresJSON)
	}

	return &vehicle, nil
}

// UpdateStatus updates vehicle online status
func (r *VehicleRepository) UpdateStatus(ctx context.Context, id string, isOnline bool) error {
	now := time.Now()
	var lastOnline *time.Time
	if isOnline {
		lastOnline = &now
	}

	query := sq.Update("vehicles").
		Set("is_online", isOnline).
		Set("last_online", lastOnline).
		Set("updated_at", now).
		Where(sq.Eq{"id": id})

	sqlQuery, args, err := query.ToSql()
	if err != nil {
		return fmt.Errorf("failed to build query: %w", err)
	}

	result, err := r.db.ExecContext(ctx, sqlQuery, args...)
	if err != nil {
		return fmt.Errorf("failed to update vehicle status: %w", err)
	}

	rowsAffected, _ := result.RowsAffected()
	if rowsAffected == 0 {
		return fmt.Errorf("vehicle not found")
	}

	// Invalidate cache
	r.invalidateCache(ctx, id)

	return nil
}

// UpdateLocation updates vehicle location
func (r *VehicleRepository) UpdateLocation(ctx context.Context, id string, lat, lng float64) error {
	query := sq.Update("vehicles").
		Set("latitude", lat).
		Set("longitude", lng).
		Set("updated_at", time.Now()).
		Where(sq.Eq{"id": id})

	sqlQuery, args, err := query.ToSql()
	if err != nil {
		return fmt.Errorf("failed to build query: %w", err)
	}

	_, err = r.db.ExecContext(ctx, sqlQuery, args...)
	if err != nil {
		return fmt.Errorf("failed to update vehicle location: %w", err)
	}

	r.invalidateCache(ctx, id)
	return nil
}

// UpdateTelemetry updates vehicle telemetry data
func (r *VehicleRepository) UpdateTelemetry(ctx context.Context, id string, batteryLevel, odometer int) error {
	query := sq.Update("vehicles").
		Set("battery_level", batteryLevel).
		Set("odometer", odometer).
		Set("updated_at", time.Now()).
		Where(sq.Eq{"id": id})

	sqlQuery, args, err := query.ToSql()
	if err != nil {
		return fmt.Errorf("failed to build query: %w", err)
	}

	_, err = r.db.ExecContext(ctx, sqlQuery, args...)
	if err != nil {
		return fmt.Errorf("failed to update vehicle telemetry: %w", err)
	}

	r.invalidateCache(ctx, id)
	return nil
}

// ListByOwner lists vehicles owned by a user
func (r *VehicleRepository) ListByOwner(ctx context.Context, ownerID string) ([]*Vehicle, error) {
	query := sq.Select("*").
		From("vehicles").
		Where(sq.Eq{"owner_id": ownerID}).
		OrderBy("created_at DESC")

	return r.listVehicles(ctx, query)
}

func (r *VehicleRepository) listVehicles(ctx context.Context, query sq.SelectBuilder) ([]*Vehicle, error) {
	sqlQuery, args, err := query.ToSql()
	if err != nil {
		return nil, fmt.Errorf("failed to build query: %w", err)
	}

	rows, err := r.db.QueryContext(ctx, sqlQuery, args...)
	if err != nil {
		return nil, fmt.Errorf("failed to query vehicles: %w", err)
	}
	defer rows.Close()

	var vehicles []*Vehicle
	for rows.Next() {
		var vehicle Vehicle
		var featuresJSON []byte
		err := rows.Scan(
			&vehicle.ID, &vehicle.OwnerID, &vehicle.VIN, &vehicle.Brand, &vehicle.Model,
			&vehicle.Year, &vehicle.Color, &vehicle.PlateNumber, &vehicle.TCUID, &vehicle.Protocol,
			&vehicle.IsOnline, &vehicle.LastOnline, &vehicle.BatteryLevel, &vehicle.Odometer,
			&vehicle.Latitude, &vehicle.Longitude, &featuresJSON, &vehicle.CreatedAt, &vehicle.UpdatedAt,
		)
		if err != nil {
			return nil, fmt.Errorf("failed to scan vehicle: %w", err)
		}

		if featuresJSON != nil {
			vehicle.Features = parseStringArray(featuresJSON)
		}
		vehicles = append(vehicles, &vehicle)
	}

	return vehicles, nil
}

func (r *VehicleRepository) cacheVehicle(ctx context.Context, vehicle *Vehicle) {
	// Cache vehicle info for 5 minutes
	vehicleJSON, _ := json.Marshal(vehicle)
	r.redis.Set(ctx, r.cacheVehicleID(vehicle.ID), vehicleJSON, 5*time.Minute)
}

func (r *VehicleRepository) getCachedVehicle(ctx context.Context, id string) (*Vehicle, error) {
	data, err := r.redis.Get(ctx, r.cacheVehicleID(id)).Bytes()
	if err != nil {
		return nil, err
	}

	var vehicle Vehicle
	if err := json.Unmarshal(data, &vehicle); err != nil {
		return nil, err
	}

	return &vehicle, nil
}

func (r *VehicleRepository) invalidateCache(ctx context.Context, id string) {
	r.redis.Del(ctx, r.cacheVehicleID(id))
}

func (r *VehicleRepository) cacheVehicleID(id string) string {
	return fmt.Sprintf("vehicle:%s", id)
}

func parseStringArray(data []byte) []string {
	var result []string
	json.Unmarshal(data, &result)
	return result
}
