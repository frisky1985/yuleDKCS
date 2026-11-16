package store

import (
	"context"
	"fmt"

	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/revocation"
)

// AddRevocationToken persists one CRL token and bumps the monotonic epoch in
// the same transaction so vehicles can reject replayed lists.
func (s *PostgresStore) AddRevocationToken(ctx context.Context, typ revocation.EntryType, token [revocation.TokenLen]byte) (uint32, error) {
	tx, err := s.pool.Begin(ctx)
	if err != nil {
		return 0, fmt.Errorf("begin revocation tx: %w", err)
	}
	defer func() { _ = tx.Rollback(ctx) }()

	var epoch int64
	if err := tx.QueryRow(ctx,
		`UPDATE revocation_state SET epoch = epoch + 1 WHERE id = TRUE RETURNING epoch`,
	).Scan(&epoch); err != nil {
		return 0, fmt.Errorf("bump CRL epoch: %w", err)
	}

	if _, err := tx.Exec(ctx,
		`INSERT INTO revocation_entries (token, entry_type) VALUES ($1, $2)
		 ON CONFLICT (token) DO NOTHING`,
		token[:], int16(typ),
	); err != nil {
		return 0, fmt.Errorf("insert revocation token: %w", err)
	}

	if err := tx.Commit(ctx); err != nil {
		return 0, fmt.Errorf("commit revocation tx: %w", err)
	}
	return uint32(epoch), nil
}

// RevocationTokens returns all persisted tokens plus the current epoch.
func (s *PostgresStore) RevocationTokens(ctx context.Context) ([]revocation.StoredToken, uint32, error) {
	var epoch int64
	if err := s.pool.QueryRow(ctx,
		`SELECT epoch FROM revocation_state WHERE id = TRUE`,
	).Scan(&epoch); err != nil {
		return nil, 0, fmt.Errorf("read CRL epoch: %w", err)
	}

	rows, err := s.pool.Query(ctx,
		`SELECT token, entry_type FROM revocation_entries ORDER BY created_at, token`,
	)
	if err != nil {
		return nil, 0, fmt.Errorf("query revocation tokens: %w", err)
	}
	defer rows.Close()

	var out []revocation.StoredToken
	for rows.Next() {
		var (
			token []byte
			typ   int16
		)
		if err := rows.Scan(&token, &typ); err != nil {
			return nil, 0, fmt.Errorf("scan revocation token: %w", err)
		}
		if len(token) != revocation.TokenLen {
			return nil, 0, fmt.Errorf("corrupt revocation token length %d", len(token))
		}
		var item revocation.StoredToken
		item.Type = revocation.EntryType(typ)
		copy(item.Token[:], token)
		out = append(out, item)
	}
	if err := rows.Err(); err != nil {
		return nil, 0, fmt.Errorf("iterate revocation tokens: %w", err)
	}

	return out, uint32(epoch), nil
}
