package store

import (
	"context"
	"fmt"

	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/service"
)

// ─── PostgresShareStore: 实现 service.ShareStore ─────────────

var _ service.ShareStore = (*PostgresStore)(nil)

// CreateShare 创建一条分享记录 (状态 PENDING)。
func (s *PostgresStore) CreateShare(ctx context.Context, share *service.ShareRecord) error {
	_, err := s.pool.Exec(ctx,
		`INSERT INTO shares (share_id, key_id, from_user_id, to_user_id, to_vendor,
		                    share_code, status, access_bits, valid_from, valid_until,
		                    max_uses, friend_key_id, created_at, accepted_at, cancelled_at)
		 VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15)`,
		share.ShareID, share.KeyID, share.FromUserID, share.ToUserID, share.ToVendor,
		share.ShareCode, share.Status, share.AccessBits, share.ValidFrom, share.ValidUntil,
		share.MaxUses, share.FriendKeyID, share.CreatedAt, share.AcceptedAt, share.CancelledAt,
	)
	if err != nil {
		return fmt.Errorf("create share: %w", err)
	}
	return nil
}

// GetShareByID 按 share_id 查询分享记录。
func (s *PostgresStore) GetShareByID(ctx context.Context, shareID string) (*service.ShareRecord, error) {
	return s.scanShare(ctx, `WHERE share_id = $1`, shareID)
}

// GetShareByCode 按分享码查询分享记录 (用于 AcceptShare 校验)。
func (s *PostgresStore) GetShareByCode(ctx context.Context, shareCode string) (*service.ShareRecord, error) {
	return s.scanShare(ctx, `WHERE share_code = $1`, shareCode)
}

// ListSharesByKey 返回某钥匙关联的全部分享记录。
func (s *PostgresStore) ListSharesByKey(ctx context.Context, keyID string) ([]*service.ShareRecord, error) {
	rows, err := s.pool.Query(ctx,
		`SELECT share_id, key_id, from_user_id, to_user_id, to_vendor, share_code, status,
		        access_bits, valid_from, valid_until, max_uses, friend_key_id,
		        created_at, accepted_at, cancelled_at
		 FROM shares WHERE key_id = $1`, keyID,
	)
	if err != nil {
		return nil, fmt.Errorf("list shares by key: %w", err)
	}
	defer rows.Close()

	var result []*service.ShareRecord
	for rows.Next() {
		rec := &service.ShareRecord{}
		if err := scanShareRow(rows, rec); err != nil {
			return nil, fmt.Errorf("scan share record: %w", err)
		}
		result = append(result, rec)
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("iterate shares: %w", err)
	}
	return result, nil
}

// UpdateShare 更新分享记录的可变字段。
func (s *PostgresStore) UpdateShare(ctx context.Context, share *service.ShareRecord) error {
	tag, err := s.pool.Exec(ctx,
		`UPDATE shares SET
		   to_user_id    = $2,
		   to_vendor     = $3,
		   share_code    = $4,
		   status        = $5,
		   access_bits   = $6,
		   valid_from    = $7,
		   valid_until   = $8,
		   max_uses      = $9,
		   friend_key_id = $10,
		   accepted_at   = $11,
		   cancelled_at  = $12
		 WHERE share_id = $1`,
		share.ShareID, share.ToUserID, share.ToVendor, share.ShareCode, share.Status,
		share.AccessBits, share.ValidFrom, share.ValidUntil, share.MaxUses,
		share.FriendKeyID, share.AcceptedAt, share.CancelledAt,
	)
	if err != nil {
		return fmt.Errorf("update share: %w", err)
	}
	if tag.RowsAffected() == 0 {
		return fmt.Errorf("share %s not found", share.ShareID)
	}
	return nil
}

// scanShare 按 WHERE 条件查询单条分享记录。
func (s *PostgresStore) scanShare(ctx context.Context, where string, arg any) (*service.ShareRecord, error) {
	rec := &service.ShareRecord{}
	row := s.pool.QueryRow(ctx,
		`SELECT share_id, key_id, from_user_id, to_user_id, to_vendor, share_code, status,
		        access_bits, valid_from, valid_until, max_uses, friend_key_id,
		        created_at, accepted_at, cancelled_at
		 FROM shares `+where, arg,
	)
	if err := scanShareRow(row, rec); err != nil {
		if isNotFound(err) {
			return nil, fmt.Errorf("share not found")
		}
		return nil, fmt.Errorf("get share: %w", err)
	}
	return rec, nil
}

// rowScanner 统一 pgx Row / Rows 的 Scan 接口。
type rowScanner interface {
	Scan(dest ...any) error
}

// scanShareRow 将一行扫描到 ShareRecord。
func scanShareRow(row rowScanner, rec *service.ShareRecord) error {
	return row.Scan(
		&rec.ShareID, &rec.KeyID, &rec.FromUserID, &rec.ToUserID, &rec.ToVendor,
		&rec.ShareCode, &rec.Status, &rec.AccessBits, &rec.ValidFrom, &rec.ValidUntil,
		&rec.MaxUses, &rec.FriendKeyID, &rec.CreatedAt, &rec.AcceptedAt, &rec.CancelledAt,
	)
}
