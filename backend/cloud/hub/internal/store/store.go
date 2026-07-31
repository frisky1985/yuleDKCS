// Package store provides PostgreSQL persistence for yuleDKCS Hub.
//
// 实现:
//   - service.KeyStore    (数字钥匙元数据)
//   - relay.MailboxStore  (CCC Mailbox)
//
// 连接管理: pgxpool 连接池
// 迁移:     golang-migrate (embed migrations/)
package store

import (
	"context"
	"embed"
	"errors"
	"fmt"
	"time"

	"github.com/golang-migrate/migrate/v4"
	_ "github.com/golang-migrate/migrate/v4/database/postgres"
	"github.com/golang-migrate/migrate/v4/source/iofs"
	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"
)

//go:embed migrations/*.sql
var migrationsFS embed.FS

// PostgresStore 是 Hub 的 PostgreSQL 持久化层
// 同时实现 KeyStore 和 MailboxStore 接口。
type PostgresStore struct {
	pool *pgxpool.Pool
}

// NewPostgresStore 创建 PostgreSQL store 并执行迁移。
//
// dsn 示例: postgres://yuledkcs:yuledkcs@localhost:5432/yuledkcs?sslmode=disable
func NewPostgresStore(ctx context.Context, dsn string) (*PostgresStore, error) {
	pool, err := pgxpool.New(ctx, dsn)
	if err != nil {
		return nil, fmt.Errorf("create pgx pool: %w", err)
	}

	// 等待数据库可用（最多 15s）
	pingCtx, cancel := context.WithTimeout(ctx, 15*time.Second)
	defer cancel()
	if err := pool.Ping(pingCtx); err != nil {
		pool.Close()
		return nil, fmt.Errorf("ping postgres: %w", err)
	}

	s := &PostgresStore{pool: pool}
	if err := s.runMigrations(ctx, dsn); err != nil {
		pool.Close()
		return nil, fmt.Errorf("run migrations: %w", err)
	}

	return s, nil
}

// runMigrations 执行 embedded 迁移。
func (s *PostgresStore) runMigrations(ctx context.Context, dsn string) error {
	src, err := iofs.New(migrationsFS, "migrations")
	if err != nil {
		return fmt.Errorf("load migrations fs: %w", err)
	}

	m, err := migrate.NewWithSourceInstance("iofs", src, dsn)
	if err != nil {
		return fmt.Errorf("init migrate: %w", err)
	}
	defer m.Close()

	if err := m.Up(); err != nil && !errors.Is(err, migrate.ErrNoChange) {
		return fmt.Errorf("migrate up: %w", err)
	}
	return nil
}

// Close 关闭连接池。
func (s *PostgresStore) Close() {
	s.pool.Close()
}

// ─── 查询辅助 ─────────────────────────────────────────────

// isNotFound 判断错误是否为 "记录不存在"。
func isNotFound(err error) bool {
	return errors.Is(err, pgx.ErrNoRows)
}
