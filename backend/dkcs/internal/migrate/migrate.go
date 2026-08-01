// Package migrate 提供 dkcs 服务的轻量级 SQL 迁移执行器 (零第三方依赖)。
//
// 迁移文件位于 db/migrations/ 目录, 命名格式 NNNN_name.up.sql /
// NNNN_name.down.sql, 按文件名升序执行; 已应用版本记录在
// schema_migrations 表, 重复执行自动跳过。每个迁移在独立事务中执行,
// 失败自动回滚且不记录版本。
package migrate

import (
	"context"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strings"

	"github.com/jmoiron/sqlx"
)

// DefaultDir 默认迁移目录 (相对进程工作目录)
const DefaultDir = "db/migrations"

// Run 执行 dir 目录下所有未应用的 *.up.sql 迁移。
// dir 为空时使用 DefaultDir; 目录不存在返回错误 (由调用方决定是否致命)。
func Run(ctx context.Context, db *sqlx.DB, dir string) error {
	if dir == "" {
		dir = DefaultDir
	}

	// 1. 确保版本表存在
	if _, err := db.ExecContext(ctx, `CREATE TABLE IF NOT EXISTS schema_migrations (
		version    TEXT PRIMARY KEY,
		applied_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
	)`); err != nil {
		return fmt.Errorf("migrate: failed to create schema_migrations: %w", err)
	}

	// 2. 读取迁移目录, 收集 *.up.sql 并按文件名排序
	entries, err := os.ReadDir(dir)
	if err != nil {
		return fmt.Errorf("migrate: failed to read migrations dir %q: %w", dir, err)
	}
	var upFiles []string
	for _, e := range entries {
		if e.IsDir() {
			continue
		}
		if strings.HasSuffix(e.Name(), ".up.sql") {
			upFiles = append(upFiles, e.Name())
		}
	}
	sort.Strings(upFiles)

	// 3. 逐个执行未应用的迁移
	for _, name := range upFiles {
		version := strings.TrimSuffix(name, ".up.sql")
		applied, err := isApplied(ctx, db, version)
		if err != nil {
			return err
		}
		if applied {
			continue
		}
		if err := applyOne(ctx, db, filepath.Join(dir, name), version); err != nil {
			return err
		}
	}
	return nil
}

// isApplied 查询版本是否已应用
func isApplied(ctx context.Context, db *sqlx.DB, version string) (bool, error) {
	var exists bool
	err := db.QueryRowContext(ctx,
		`SELECT EXISTS (SELECT 1 FROM schema_migrations WHERE version = $1)`, version).
		Scan(&exists)
	if err != nil {
		return false, fmt.Errorf("migrate: failed to check version %q: %w", version, err)
	}
	return exists, nil
}

// applyOne 在事务中执行单个迁移文件并记录版本
func applyOne(ctx context.Context, db *sqlx.DB, path, version string) error {
	sqlBytes, err := os.ReadFile(path)
	if err != nil {
		return fmt.Errorf("migrate: failed to read migration %q: %w", path, err)
	}

	tx, err := db.BeginTxx(ctx, nil)
	if err != nil {
		return fmt.Errorf("migrate: failed to begin tx for %q: %w", version, err)
	}
	defer tx.Rollback() //nolint:errcheck // 提交成功后 Rollback 为 no-op

	// lib/pq 无参数 Exec 走 simple query protocol, 支持多语句迁移文件
	if _, err := tx.ExecContext(ctx, string(sqlBytes)); err != nil {
		return fmt.Errorf("migrate: failed to apply %q: %w", version, err)
	}
	if _, err := tx.ExecContext(ctx,
		`INSERT INTO schema_migrations (version) VALUES ($1)`, version); err != nil {
		return fmt.Errorf("migrate: failed to record version %q: %w", version, err)
	}
	if err := tx.Commit(); err != nil {
		return fmt.Errorf("migrate: failed to commit %q: %w", version, err)
	}
	return nil
}
