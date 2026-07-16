// ─────────────────────────────────────────────────────────
// yuleDKCS Migration Runner
// 自动发现并执行 migrations/ 目录下未应用的 .sql 文件
// ─────────────────────────────────────────────────────────
package main

import (
	"database/sql"
	"flag"
	"fmt"
	"log"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"time"

	_ "github.com/lib/pq"
)

func main() {
	dsn := flag.String("dsn", "postgres://postgres:password@localhost:5432/yuledkcs?sslmode=disable", "PostgreSQL DSN")
	dir := flag.String("dir", "", "migrations directory (default: auto-detect)")
	flag.Parse()

	// 定位 migration 目录
	migrationDir := *dir
	if migrationDir == "" {
		// 从当前工作目录或二进制所在目录自动推断
		if _, err := os.Stat(filepath.Join("backend", "cloud", "hub", "migrations")); err == nil {
			migrationDir = filepath.Join("backend", "cloud", "hub", "migrations")
		} else if exe, err := os.Executable(); err == nil {
			migrationDir = filepath.Join(filepath.Dir(exe), "migrations")
		} else {
			migrationDir = "migrations"
		}
	}

	// 校验目录存在
	info, err := os.Stat(migrationDir)
	if err != nil || !info.IsDir() {
		log.Fatalf("migrations directory not found: %s", migrationDir)
	}

	// 连接数据库
	db, err := sql.Open("postgres", *dsn)
	if err != nil {
		log.Fatalf("cannot connect: %v", err)
	}
	defer db.Close()

	// 测试连接
	if err := db.Ping(); err != nil {
		log.Fatalf("cannot ping database: %v", err)
	}
	log.Printf("connected to database successfully")

	// 创建迁移跟踪表
	if _, err := db.Exec(`CREATE TABLE IF NOT EXISTS schema_migrations (
		version    VARCHAR(64) PRIMARY KEY,
		applied_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
	)`); err != nil {
		log.Fatalf("cannot create schema_migrations table: %v", err)
	}

	// 读取 SQL 文件并排序
	entries, err := os.ReadDir(migrationDir)
	if err != nil {
		log.Fatalf("cannot read migration directory: %v", err)
	}

	var files []string
	for _, e := range entries {
		if !e.IsDir() && strings.HasSuffix(e.Name(), ".sql") {
			files = append(files, e.Name())
		}
	}
	sort.Strings(files)

	if len(files) == 0 {
		log.Println("no SQL migration files found")
		return
	}

	// 逐个执行
	applied := 0
	skipped := 0
	for _, f := range files {
		start := time.Now()

		// 检查是否已执行
		var count int
		if err := db.QueryRow("SELECT COUNT(*) FROM schema_migrations WHERE version = $1", f).Scan(&count); err != nil {
			log.Fatalf("cannot check migration status for %s: %v", f, err)
		}
		if count > 0 {
			log.Printf("SKIP %s (already applied)", f)
			skipped++
			continue
		}

		// 读取 SQL 内容
		content, err := os.ReadFile(filepath.Join(migrationDir, f))
		if err != nil {
			log.Fatalf("cannot read %s: %v", f, err)
		}

		sqlStr := string(content)

		// 执行（支持多条语句）
		if _, err := db.Exec(sqlStr); err != nil {
			log.Fatalf("FAIL %s: %v", f, err)
		}

		// 记录已执行
		if _, err := db.Exec("INSERT INTO schema_migrations (version) VALUES ($1)", f); err != nil {
			log.Fatalf("cannot record migration %s: %v", f, err)
		}

		elapsed := time.Since(start)
		log.Printf("OK   %s (%v)", f, elapsed)
		applied++
	}

	log.Printf("migration complete: %d applied, %d skipped", applied, skipped)
	fmt.Println()
	fmt.Println("═══════════════════════════════════════════")
	fmt.Println("  ✅  yuleDKCS migration complete")
	fmt.Println("═══════════════════════════════════════════")
}
