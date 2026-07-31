package store

import (
	"context"
	"fmt"

	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/service"
)

// ─── PostgresKeyStore: 实现 service.KeyStore ─────────────────

var _ service.KeyStore = (*PostgresStore)(nil)

// GetKeyOwner 返回钥匙所有者 user_id。
func (s *PostgresStore) GetKeyOwner(ctx context.Context, keyID string) (string, error) {
	var owner string
	err := s.pool.QueryRow(ctx,
		`SELECT owner_user_id FROM keys WHERE key_id = $1`, keyID,
	).Scan(&owner)
	if isNotFound(err) {
		return "", fmt.Errorf("key %s not found", keyID)
	}
	if err != nil {
		return "", fmt.Errorf("get key owner: %w", err)
	}
	return owner, nil
}

// GetKeyStatus 返回钥匙当前状态。
func (s *PostgresStore) GetKeyStatus(ctx context.Context, keyID string) (string, error) {
	var status string
	err := s.pool.QueryRow(ctx,
		`SELECT status FROM keys WHERE key_id = $1`, keyID,
	).Scan(&status)
	if isNotFound(err) {
		return "", fmt.Errorf("key %s not found", keyID)
	}
	if err != nil {
		return "", fmt.Errorf("get key status: %w", err)
	}
	return status, nil
}

// GetKeyRecord 返回完整钥匙记录。
func (s *PostgresStore) GetKeyRecord(ctx context.Context, keyID string) (*service.KeyRecord, error) {
	rec := &service.KeyRecord{}
	err := s.pool.QueryRow(ctx,
		`SELECT key_id, owner_user_id, vehicle_id, vendor, status, created_at
		 FROM keys WHERE key_id = $1`, keyID,
	).Scan(&rec.KeyID, &rec.OwnerUserID, &rec.VehicleID, &rec.Vendor, &rec.Status, &rec.CreatedAt)
	if isNotFound(err) {
		return nil, fmt.Errorf("key %s not found", keyID)
	}
	if err != nil {
		return nil, fmt.Errorf("get key record: %w", err)
	}
	return rec, nil
}

// SetKey 创建或更新钥匙元数据 (UPSERT)。
func (s *PostgresStore) SetKey(ctx context.Context, key *service.KeyRecord) error {
	_, err := s.pool.Exec(ctx,
		`INSERT INTO keys (key_id, owner_user_id, vehicle_id, vendor, status, created_at)
		 VALUES ($1, $2, $3, $4, $5, $6)
		 ON CONFLICT (key_id) DO UPDATE SET
		   owner_user_id = EXCLUDED.owner_user_id,
		   vehicle_id    = EXCLUDED.vehicle_id,
		   vendor        = EXCLUDED.vendor,
		   status        = EXCLUDED.status,
		   created_at    = EXCLUDED.created_at`,
		key.KeyID, key.OwnerUserID, key.VehicleID, key.Vendor, key.Status, key.CreatedAt,
	)
	if err != nil {
		return fmt.Errorf("set key: %w", err)
	}
	return nil
}

// SetKeyStatus 仅更新钥匙状态。
func (s *PostgresStore) SetKeyStatus(ctx context.Context, keyID, status string) error {
	tag, err := s.pool.Exec(ctx,
		`UPDATE keys SET status = $2 WHERE key_id = $1`, keyID, status,
	)
	if err != nil {
		return fmt.Errorf("set key status: %w", err)
	}
	if tag.RowsAffected() == 0 {
		return fmt.Errorf("key %s not found", keyID)
	}
	return nil
}

// ListKeysByUser 返回用户拥有的全部钥匙。
func (s *PostgresStore) ListKeysByUser(ctx context.Context, userID string) ([]*service.KeyRecord, error) {
	rows, err := s.pool.Query(ctx,
		`SELECT key_id, owner_user_id, vehicle_id, vendor, status, created_at
		 FROM keys WHERE owner_user_id = $1`, userID,
	)
	if err != nil {
		return nil, fmt.Errorf("list keys by user: %w", err)
	}
	defer rows.Close()

	var result []*service.KeyRecord
	for rows.Next() {
		rec := &service.KeyRecord{}
		if err := rows.Scan(&rec.KeyID, &rec.OwnerUserID, &rec.VehicleID, &rec.Vendor, &rec.Status, &rec.CreatedAt); err != nil {
			return nil, fmt.Errorf("scan key record: %w", err)
		}
		result = append(result, rec)
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("iterate keys: %w", err)
	}
	return result, nil
}
