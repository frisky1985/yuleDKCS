package repository

import (
	"context"
	"database/sql"
	"encoding/json"
	"fmt"
	"time"

	sq "github.com/Masterminds/squirrel"
	"github.com/jmoiron/sqlx"
	"github.com/redis/go-redis/v9"
)

// Key represents a digital key
type Key struct {
	ID           string                 `db:"id" json:"id"`
	VehicleID    string                 `db:"vehicle_id" json:"vehicle_id"`
	UserID       string                 `db:"user_id" json:"user_id"`
	KeyType      string                 `db:"key_type" json:"key_type"` // primary, friend, service, temporary
	Status       string                 `db:"status" json:"status"`    // pending, active, suspended, expired, revoked
	Permissions  []string               `db:"permissions" json:"permissions"`
	Secret       string                 `db:"secret" json:"secret"`
	ParentKeyID  *string                `db:"parent_key_id" json:"parent_key_id"`
	CreatedAt    time.Time              `db:"created_at" json:"created_at"`
	ActivatedAt  *time.Time             `db:"activated_at" json:"activated_at"`
	ExpiresAt    time.Time              `db:"expires_at" json:"expires_at"`
	RevokedAt    *time.Time             `db:"revoked_at" json:"revoked_at"`
	RevokeReason string                 `db:"revoke_reason" json:"revoke_reason"`
	Metadata     map[string]interface{} `db:"metadata" json:"metadata"`
}

// KeyRepository handles key data persistence
type KeyRepository struct {
	db    *sqlx.DB
	redis *redis.Client
}

// NewKeyRepository creates a new KeyRepository
func NewKeyRepository(db *sqlx.DB, redis *redis.Client) *KeyRepository {
	return &KeyRepository{db: db, redis: redis}
}

// Create creates a new key
func (r *KeyRepository) Create(ctx context.Context, key *Key) error {
	permissionsJSON, _ := json.Marshal(key.Permissions)
	metadataJSON, _ := json.Marshal(key.Metadata)

	query := sq.Insert("digital_keys").
		Columns("id", "vehicle_id", "user_id", "key_type", "status", "permissions", "secret", "parent_key_id", "created_at", "expires_at", "metadata").
		Values(key.ID, key.VehicleID, key.UserID, key.KeyType, key.Status, permissionsJSON, key.Secret, key.ParentKeyID, key.CreatedAt, key.ExpiresAt, metadataJSON).
		Suffix("RETURNING id")

	sqlQuery, args, err := query.ToSql()
	if err != nil {
		return fmt.Errorf("failed to build query: %w", err)
	}

	var returnedID string
	if err := r.db.QueryRowContext(ctx, sqlQuery, args...).Scan(&returnedID); err != nil {
		return fmt.Errorf("failed to create key: %w", err)
	}

	// Cache key info
	r.cacheKey(ctx, key)

	return nil
}

// GetByID retrieves a key by ID
func (r *KeyRepository) GetByID(ctx context.Context, id string) (*Key, error) {
	// Try cache first
	cachedKey, err := r.getCachedKey(ctx, id)
	if err == nil && cachedKey != nil {
		return cachedKey, nil
	}

	query := sq.Select("*").
		From("digital_keys").
		Where(sq.Eq{"id": id}).
		Limit(1)

	sqlQuery, args, err := query.ToSql()
	if err != nil {
		return nil, fmt.Errorf("failed to build query: %w", err)
	}

	var key Key
	var permissionsJSON, metadataJSON []byte
	err = r.db.QueryRowContext(ctx, sqlQuery, args...).Scan(
		&key.ID, &key.VehicleID, &key.UserID, &key.KeyType, &key.Status,
		&permissionsJSON, &key.Secret, &key.ParentKeyID, &key.CreatedAt,
		&key.ActivatedAt, &key.ExpiresAt, &key.RevokedAt, &key.RevokeReason,
		&metadataJSON,
	)
	if err != nil {
		if err == sql.ErrNoRows {
			return nil, fmt.Errorf("key not found")
		}
		return nil, fmt.Errorf("failed to get key: %w", err)
	}

	json.Unmarshal(permissionsJSON, &key.Permissions)
	json.Unmarshal(metadataJSON, &key.Metadata)

	// Cache for future requests
	r.cacheKey(ctx, &key)

	return &key, nil
}

// Update updates a key
func (r *KeyRepository) Update(ctx context.Context, key *Key) error {
	permissionsJSON, _ := json.Marshal(key.Permissions)
	metadataJSON, _ := json.Marshal(key.Metadata)

	query := sq.Update("digital_keys").
		Set("status", key.Status).
		Set("permissions", permissionsJSON).
		Set("activated_at", key.ActivatedAt).
		Set("revoked_at", key.RevokedAt).
		Set("revoke_reason", key.RevokeReason).
		Set("metadata", metadataJSON).
		Where(sq.Eq{"id": key.ID})

	sqlQuery, args, err := query.ToSql()
	if err != nil {
		return fmt.Errorf("failed to build query: %w", err)
	}

	result, err := r.db.ExecContext(ctx, sqlQuery, args...)
	if err != nil {
		return fmt.Errorf("failed to update key: %w", err)
	}

	rowsAffected, _ := result.RowsAffected()
	if rowsAffected == 0 {
		return fmt.Errorf("key not found")
	}

	// Invalidate cache
	r.invalidateCache(ctx, key.ID)

	return nil
}

// Delete deletes a key
func (r *KeyRepository) Delete(ctx context.Context, id string) error {
	query := sq.Delete("digital_keys").Where(sq.Eq{"id": id})

	sqlQuery, args, err := query.ToSql()
	if err != nil {
		return fmt.Errorf("failed to build query: %w", err)
	}

	_, err = r.db.ExecContext(ctx, sqlQuery, args...)
	if err != nil {
		return fmt.Errorf("failed to delete key: %w", err)
	}

	// Invalidate cache
	r.invalidateCache(ctx, id)

	return nil
}

// ListByUser lists keys for a user
func (r *KeyRepository) ListByUser(ctx context.Context, userID string, limit, offset int) ([]*Key, error) {
	query := sq.Select("*").
		From("digital_keys").
		Where(sq.Eq{"user_id": userID}).
		OrderBy("created_at DESC").
		Limit(uint64(limit)).
		Offset(uint64(offset))

	return r.listKeys(ctx, query)
}

// ListByVehicle lists keys for a vehicle
func (r *KeyRepository) ListByVehicle(ctx context.Context, vehicleID string, limit, offset int) ([]*Key, error) {
	query := sq.Select("*").
		From("digital_keys").
		Where(sq.Eq{"vehicle_id": vehicleID}).
		OrderBy("created_at DESC").
		Limit(uint64(limit)).
		Offset(uint64(offset))

	return r.listKeys(ctx, query)
}

// ListActiveByVehicle lists active keys for a vehicle
func (r *KeyRepository) ListActiveByVehicle(ctx context.Context, vehicleID string) ([]*Key, error) {
	query := sq.Select("*").
		From("digital_keys").
		Where(sq.Eq{"vehicle_id": vehicleID}).
		Where(sq.Eq{"status": "active"}).
		Where(sq.Gt{"expires_at": time.Now()}).
		OrderBy("created_at DESC")

	return r.listKeys(ctx, query)
}

func (r *KeyRepository) listKeys(ctx context.Context, query sq.SelectBuilder) ([]*Key, error) {
	sqlQuery, args, err := query.ToSql()
	if err != nil {
		return nil, fmt.Errorf("failed to build query: %w", err)
	}

	rows, err := r.db.QueryContext(ctx, sqlQuery, args...)
	if err != nil {
		return nil, fmt.Errorf("failed to query keys: %w", err)
	}
	defer rows.Close()

	var keys []*Key
	for rows.Next() {
		var key Key
		var permissionsJSON, metadataJSON []byte
		err := rows.Scan(
			&key.ID, &key.VehicleID, &key.UserID, &key.KeyType, &key.Status,
			&permissionsJSON, &key.Secret, &key.ParentKeyID, &key.CreatedAt,
			&key.ActivatedAt, &key.ExpiresAt, &key.RevokedAt, &key.RevokeReason,
			&metadataJSON,
		)
		if err != nil {
			return nil, fmt.Errorf("failed to scan key: %w", err)
		}

		json.Unmarshal(permissionsJSON, &key.Permissions)
		json.Unmarshal(metadataJSON, &key.Metadata)
		keys = append(keys, &key)
	}

	return keys, nil
}

// HasPermission checks if key has specific permission
func (k *Key) HasPermission(permission string) bool {
	for _, p := range k.Permissions {
		if p == permission || p == "all" {
			return true
		}
	}
	return false
}

func (r *KeyRepository) cacheKey(ctx context.Context, key *Key) {
	keyJSON, _ := json.Marshal(key)
	r.redis.Set(ctx, r.cacheKeyID(key.ID), keyJSON, 5*time.Minute)
}

func (r *KeyRepository) getCachedKey(ctx context.Context, id string) (*Key, error) {
	data, err := r.redis.Get(ctx, r.cacheKeyID(id)).Bytes()
	if err != nil {
		return nil, err
	}

	var key Key
	if err := json.Unmarshal(data, &key); err != nil {
		return nil, err
	}

	return &key, nil
}

func (r *KeyRepository) invalidateCache(ctx context.Context, id string) {
	r.redis.Del(ctx, r.cacheKeyID(id))
}

func (r *KeyRepository) cacheKeyID(id string) string {
	return fmt.Sprintf("key:%s", id)
}
