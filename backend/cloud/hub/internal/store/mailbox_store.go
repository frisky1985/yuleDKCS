package store

import (
	"context"
	"fmt"
	"time"

	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/relay"
)

// ─── PostgresMailboxStore: 实现 relay.MailboxStore ───────────

var _ relay.MailboxStore = (*PostgresStore)(nil)

// Create 创建邮箱。
func (s *PostgresStore) Create(ctx context.Context, mb *relay.Mailbox) error {
	_, err := s.pool.Exec(ctx,
		`INSERT INTO mailboxes (
		   mailbox_id, status, sender_device_id, sender_vendor,
		   notification_token, sender_token, receiver_token,
		   display_info, payload, sharing_data_type, sharing_url,
		   receiver_device_id, receiver_vendor,
		   created_at, expires_at, updated_at,
		   version, update_count, max_updates, device_attestation
		 ) VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12,$13,$14,$15,$16,$17,$18,$19,$20)`,
		mb.ID, int32(mb.Status), mb.SenderDeviceID, mb.SenderVendor,
		mb.NotificationToken, mb.SenderToken, mb.ReceiverToken,
		mb.DisplayInfo, mb.Payload, mb.SharingDataType, mb.SharingURL,
		mb.ReceiverDeviceID, mb.ReceiverVendor,
		mb.CreatedAt, mb.ExpiresAt, mb.UpdatedAt,
		mb.Version, mb.UpdateCount, mb.MaxUpdates, mb.DeviceAttestation,
	)
	if err != nil {
		return fmt.Errorf("create mailbox: %w", err)
	}
	return nil
}

// Get 按 ID 读取邮箱。
func (s *PostgresStore) Get(ctx context.Context, mailboxID string) (*relay.Mailbox, error) {
	mb := &relay.Mailbox{}
	err := s.pool.QueryRow(ctx,
		`SELECT mailbox_id, status, sender_device_id, sender_vendor,
		   notification_token, sender_token, receiver_token,
		   display_info, payload, sharing_data_type, sharing_url,
		   receiver_device_id, receiver_vendor,
		   created_at, expires_at, updated_at,
		   version, update_count, max_updates, device_attestation
		 FROM mailboxes WHERE mailbox_id = $1`, mailboxID,
	).Scan(
		&mb.ID, &mb.Status, &mb.SenderDeviceID, &mb.SenderVendor,
		&mb.NotificationToken, &mb.SenderToken, &mb.ReceiverToken,
		&mb.DisplayInfo, &mb.Payload, &mb.SharingDataType, &mb.SharingURL,
		&mb.ReceiverDeviceID, &mb.ReceiverVendor,
		&mb.CreatedAt, &mb.ExpiresAt, &mb.UpdatedAt,
		&mb.Version, &mb.UpdateCount, &mb.MaxUpdates, &mb.DeviceAttestation,
	)
	if isNotFound(err) {
		return nil, relay.ErrMailboxNotFound
	}
	if err != nil {
		return nil, fmt.Errorf("get mailbox: %w", err)
	}
	return mb, nil
}

// Update 覆盖写入整行（含版本号）。
func (s *PostgresStore) Update(ctx context.Context, mb *relay.Mailbox) error {
	tag, err := s.pool.Exec(ctx,
		`UPDATE mailboxes SET
		   status = $2, sender_device_id = $3, sender_vendor = $4,
		   notification_token = $5, sender_token = $6, receiver_token = $7,
		   display_info = $8, payload = $9, sharing_data_type = $10, sharing_url = $11,
		   receiver_device_id = $12, receiver_vendor = $13,
		   created_at = $14, expires_at = $15, updated_at = $16,
		   version = $17, update_count = $18, max_updates = $19, device_attestation = $20
		 WHERE mailbox_id = $1`,
		mb.ID, int32(mb.Status), mb.SenderDeviceID, mb.SenderVendor,
		mb.NotificationToken, mb.SenderToken, mb.ReceiverToken,
		mb.DisplayInfo, mb.Payload, mb.SharingDataType, mb.SharingURL,
		mb.ReceiverDeviceID, mb.ReceiverVendor,
		mb.CreatedAt, mb.ExpiresAt, mb.UpdatedAt,
		mb.Version, mb.UpdateCount, mb.MaxUpdates, mb.DeviceAttestation,
	)
	if err != nil {
		return fmt.Errorf("update mailbox: %w", err)
	}
	if tag.RowsAffected() == 0 {
		return relay.ErrMailboxNotFound
	}
	return nil
}

// Delete 删除邮箱。
func (s *PostgresStore) Delete(ctx context.Context, mailboxID string) error {
	tag, err := s.pool.Exec(ctx,
		`DELETE FROM mailboxes WHERE mailbox_id = $1`, mailboxID,
	)
	if err != nil {
		return fmt.Errorf("delete mailbox: %w", err)
	}
	if tag.RowsAffected() == 0 {
		return relay.ErrMailboxNotFound
	}
	return nil
}

// ListExpired 返回所有已过期但未标记 Expired 的邮箱。
func (s *PostgresStore) ListExpired(ctx context.Context, now time.Time) ([]*relay.Mailbox, error) {
	rows, err := s.pool.Query(ctx,
		`SELECT mailbox_id, status, sender_device_id, sender_vendor,
		   notification_token, sender_token, receiver_token,
		   display_info, payload, sharing_data_type, sharing_url,
		   receiver_device_id, receiver_vendor,
		   created_at, expires_at, updated_at,
		   version, update_count, max_updates, device_attestation
		 FROM mailboxes
		 WHERE expires_at < $1 AND status <> 6  -- 6 = EXPIRED
		 ORDER BY expires_at ASC`,
		now,
	)
	if err != nil {
		return nil, fmt.Errorf("list expired mailboxes: %w", err)
	}
	defer rows.Close()

	var result []*relay.Mailbox
	for rows.Next() {
		mb := &relay.Mailbox{}
		if err := rows.Scan(
			&mb.ID, &mb.Status, &mb.SenderDeviceID, &mb.SenderVendor,
			&mb.NotificationToken, &mb.SenderToken, &mb.ReceiverToken,
			&mb.DisplayInfo, &mb.Payload, &mb.SharingDataType, &mb.SharingURL,
			&mb.ReceiverDeviceID, &mb.ReceiverVendor,
			&mb.CreatedAt, &mb.ExpiresAt, &mb.UpdatedAt,
			&mb.Version, &mb.UpdateCount, &mb.MaxUpdates, &mb.DeviceAttestation,
		); err != nil {
			return nil, fmt.Errorf("scan mailbox: %w", err)
		}
		result = append(result, mb)
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("iterate expired mailboxes: %w", err)
	}
	return result, nil
}
