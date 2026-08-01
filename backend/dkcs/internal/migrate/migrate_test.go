package migrate

import (
	"context"
	"os"
	"path/filepath"
	"testing"

	"github.com/DATA-DOG/go-sqlmock"
	"github.com/jmoiron/sqlx"
)

// newMockDB 创建 sqlx.DB + sqlmock
func newMockDB(t *testing.T) (*sqlx.DB, sqlmock.Sqlmock) {
	t.Helper()
	db, mock, err := sqlmock.New()
	if err != nil {
		t.Fatalf("failed to create sqlmock: %v", err)
	}
	t.Cleanup(func() { db.Close() })
	return sqlx.NewDb(db, "postgres"), mock
}

// writeMigration 在临时目录写入迁移文件
func writeMigration(t *testing.T, dir, name, content string) {
	t.Helper()
	if err := os.WriteFile(filepath.Join(dir, name), []byte(content), 0o644); err != nil {
		t.Fatalf("failed to write %s: %v", name, err)
	}
}

// TestRun_AppliesPendingMigrations 未应用迁移按序执行并记录版本
func TestRun_AppliesPendingMigrations(t *testing.T) {
	db, mock := newMockDB(t)
	dir := t.TempDir()
	writeMigration(t, dir, "0001_init.up.sql", "CREATE TABLE t1 (id INT);")
	writeMigration(t, dir, "0002_seed.up.sql", "INSERT INTO t1 VALUES (1);")

	mock.ExpectExec("CREATE TABLE IF NOT EXISTS schema_migrations").WillReturnResult(sqlmock.NewResult(0, 0))
	// 0001: 未应用 → 事务执行 + 记录
	mock.ExpectQuery("SELECT EXISTS").WithArgs("0001_init").WillReturnRows(sqlmock.NewRows([]string{"exists"}).AddRow(false))
	mock.ExpectBegin()
	mock.ExpectExec("CREATE TABLE t1").WillReturnResult(sqlmock.NewResult(0, 0))
	mock.ExpectExec("INSERT INTO schema_migrations").WithArgs("0001_init").WillReturnResult(sqlmock.NewResult(0, 1))
	mock.ExpectCommit()
	// 0002: 未应用 → 事务执行 + 记录
	mock.ExpectQuery("SELECT EXISTS").WithArgs("0002_seed").WillReturnRows(sqlmock.NewRows([]string{"exists"}).AddRow(false))
	mock.ExpectBegin()
	mock.ExpectExec("INSERT INTO t1").WillReturnResult(sqlmock.NewResult(0, 1))
	mock.ExpectExec("INSERT INTO schema_migrations").WithArgs("0002_seed").WillReturnResult(sqlmock.NewResult(0, 1))
	mock.ExpectCommit()

	if err := Run(context.Background(), db, dir); err != nil {
		t.Fatalf("Run failed: %v", err)
	}
	if err := mock.ExpectationsWereMet(); err != nil {
		t.Fatalf("unmet expectations: %v", err)
	}
}

// TestRun_SkipsAppliedMigrations 已应用版本跳过
func TestRun_SkipsAppliedMigrations(t *testing.T) {
	db, mock := newMockDB(t)
	dir := t.TempDir()
	writeMigration(t, dir, "0001_init.up.sql", "CREATE TABLE t1 (id INT);")

	mock.ExpectExec("CREATE TABLE IF NOT EXISTS schema_migrations").WillReturnResult(sqlmock.NewResult(0, 0))
	mock.ExpectQuery("SELECT EXISTS").WithArgs("0001_init").WillReturnRows(sqlmock.NewRows([]string{"exists"}).AddRow(true))
	// 已应用 → 不执行任何事务

	if err := Run(context.Background(), db, dir); err != nil {
		t.Fatalf("Run failed: %v", err)
	}
	if err := mock.ExpectationsWereMet(); err != nil {
		t.Fatalf("unmet expectations: %v", err)
	}
}

// TestRun_MigrationFailure_RollsBack 迁移失败回滚且不记录版本
func TestRun_MigrationFailure_RollsBack(t *testing.T) {
	db, mock := newMockDB(t)
	dir := t.TempDir()
	writeMigration(t, dir, "0001_bad.up.sql", "SELECT * FROM nonexistent_table;")

	mock.ExpectExec("CREATE TABLE IF NOT EXISTS schema_migrations").WillReturnResult(sqlmock.NewResult(0, 0))
	mock.ExpectQuery("SELECT EXISTS").WithArgs("0001_bad").WillReturnRows(sqlmock.NewRows([]string{"exists"}).AddRow(false))
	mock.ExpectBegin()
	mock.ExpectExec("SELECT \\* FROM nonexistent_table").WillReturnError(sqlmock.ErrCancelled)
	mock.ExpectRollback()

	if err := Run(context.Background(), db, dir); err == nil {
		t.Fatal("expected error on failed migration, got nil")
	}
	if err := mock.ExpectationsWereMet(); err != nil {
		t.Fatalf("unmet expectations: %v", err)
	}
}

// TestRun_MissingDir_ReturnsError 迁移目录不存在返回错误 (调用方决定是否致命)
func TestRun_MissingDir_ReturnsError(t *testing.T) {
	db, mock := newMockDB(t)
	mock.ExpectExec("CREATE TABLE IF NOT EXISTS schema_migrations").WillReturnResult(sqlmock.NewResult(0, 0))

	if err := Run(context.Background(), db, filepath.Join(t.TempDir(), "no-such-dir")); err == nil {
		t.Fatal("expected error for missing migrations dir, got nil")
	}
	if err := mock.ExpectationsWereMet(); err != nil {
		t.Fatalf("unmet expectations: %v", err)
	}
}

// TestRun_EmptyDir_Noop 空迁移目录 (无 .up.sql) 为 no-op
func TestRun_EmptyDir_Noop(t *testing.T) {
	db, mock := newMockDB(t)
	dir := t.TempDir()
	writeMigration(t, dir, "0001_init.down.sql", "DROP TABLE t1;") // 只有 down 文件, 不执行

	mock.ExpectExec("CREATE TABLE IF NOT EXISTS schema_migrations").WillReturnResult(sqlmock.NewResult(0, 0))

	if err := Run(context.Background(), db, dir); err != nil {
		t.Fatalf("Run failed: %v", err)
	}
	if err := mock.ExpectationsWereMet(); err != nil {
		t.Fatalf("unmet expectations: %v", err)
	}
}
