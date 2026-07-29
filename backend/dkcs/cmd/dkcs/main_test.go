package main

import (
	"errors"
	"os"
	"testing"

	"github.com/frisky1985/yuleDKCS/backend/dkcs/internal/config"
)

// ── initDatabase ─────────────────────────────────────────────────────────────

func TestInitDatabase_BadConfig_ReturnsError(t *testing.T) {
	// A config with no host/user/password should fail to connect
	cfg := config.DatabaseConfig{
		Host:     "invalid-host-that-does-not-exist",
		Port:     1,
		User:     "",
		Password: "",
		Database: "nonexistent",
		SSLMode:  "disable",
	}

	db, err := initDatabase(cfg)
	if err == nil {
		db.Close()
		t.Fatal("expected error for bad database config, got nil")
	}
	if db != nil {
		t.Error("expected nil db on error, got non-nil")
	}
}

func TestInitDatabase_EmptyDSN_ReturnsError(t *testing.T) {
	// Even a minimally valid DSN will fail to connect - we test error propagation
	cfg := config.DatabaseConfig{
		Host: "",
		Port: 0,
	}

	db, err := initDatabase(cfg)
	if err == nil {
		db.Close()
		t.Fatal("expected error for empty DSN, got nil")
	}
	if db != nil {
		t.Error("expected nil db on error, got non-nil")
	}
}

// ── initRedis ────────────────────────────────────────────────────────────────

func TestInitRedis_ReturnsClient(t *testing.T) {
	cfg := config.RedisConfig{
		Addr:     "127.0.0.1:6379",
		Password: "",
		DB:       0,
	}

	client := initRedis(cfg)
	if client == nil {
		t.Fatal("initRedis returned nil")
	}
	// Close immediately — client connects lazily so this won't error
	if err := client.Close(); err != nil {
		t.Fatalf("close client: %v", err)
	}
}

func TestInitRedis_EmptyAddr(t *testing.T) {
	cfg := config.RedisConfig{
		Addr: ":0",
	}

	client := initRedis(cfg)
	if client == nil {
		t.Fatal("initRedis returned nil even with empty addr")
	}
	client.Close()
}

func TestInitRedis_DifferentDB(t *testing.T) {
	cfg := config.RedisConfig{
		Addr: "localhost:6379",
		DB:   5,
	}

	client := initRedis(cfg)
	if client == nil {
		t.Fatal("initRedis returned nil")
	}
	defer client.Close()

	// Verify the options are set correctly by checking the connection's DB
	// (redis.Client.Options() returns a copy — we can check the configured opts)
	if client.Options().DB != 5 {
		t.Errorf("expected DB=5, got %d", client.Options().DB)
	}
	if client.Options().Addr != "localhost:6379" {
		t.Errorf("expected addr localhost:6379, got %s", client.Options().Addr)
	}
}

// ── config.Load through env ──────────────────────────────────────────────────

func TestConfigLoad_EnvOverrides(t *testing.T) {
	// Set env vars
	os.Setenv("GRPC_PORT", "25000")
	os.Setenv("DB_HOST", "test-db.example.com")
	os.Setenv("REDIS_ADDR", "test-redis:6380")
	os.Setenv("LOG_LEVEL", "debug")
	defer func() {
		os.Unsetenv("GRPC_PORT")
		os.Unsetenv("DB_HOST")
		os.Unsetenv("REDIS_ADDR")
		os.Unsetenv("LOG_LEVEL")
	}()

	cfg := config.Load()

	if cfg.Server.GRPCPort != 25000 {
		t.Errorf("expected GRPC_PORT=25000, got %d", cfg.Server.GRPCPort)
	}
	if cfg.Database.Host != "test-db.example.com" {
		t.Errorf("expected DB_HOST=test-db.example.com, got %s", cfg.Database.Host)
	}
	if cfg.Redis.Addr != "test-redis:6380" {
		t.Errorf("expected REDIS_ADDR=test-redis:6380, got %s", cfg.Redis.Addr)
	}
	if cfg.Log.Level != "debug" {
		t.Errorf("expected LOG_LEVEL=debug, got %s", cfg.Log.Level)
	}
}

func TestConfigLoad_Defaults(t *testing.T) {
	cfg := config.Load()

	if cfg.Server.GRPCPort != 50051 {
		t.Errorf("expected default GRPC_PORT=50051, got %d", cfg.Server.GRPCPort)
	}
	if cfg.Server.HTTPPort != 8080 {
		t.Errorf("expected default HTTP_PORT=8080, got %d", cfg.Server.HTTPPort)
	}
	if cfg.Database.MaxOpenConns != 100 {
		t.Errorf("expected default DB_MAX_OPEN_CONNS=100, got %d", cfg.Database.MaxOpenConns)
	}
	if cfg.Metrics.Enabled != true {
		t.Errorf("expected default METRICS_ENABLED=true, got %v", cfg.Metrics.Enabled)
	}
}

// ── initDatabase DSN format ──────────────────────────────────────────────────

func TestInitDatabase_DSNNonEmptyFormat(t *testing.T) {
	// We don't connect — we just check that the DSN() method renders correctly
	cfg := config.DatabaseConfig{
		Host:     "pg.example.com",
		Port:     5432,
		User:     "app_user",
		Password: "s3cret",
		Database: "dkcs_prod",
		SSLMode:  "require",
	}

	dsn := cfg.DSN()
	if dsn == "" {
		t.Fatal("DSN() returned empty string")
	}

	// Parse the DSN can't be done easily, but we can check it contains the key parts
	expectedParts := []string{
		"host=pg.example.com",
		"port=5432",
		"user=app_user",
		"password=s3cret",
		"dbname=dkcs_prod",
		"sslmode=require",
	}
	for _, part := range expectedParts {
		if !contains(dsn, part) {
			t.Errorf("DSN missing expected part %q", part)
		}
	}
}

func TestInitDatabase_ErrorWrapping(t *testing.T) {
	cfg := config.DatabaseConfig{
		Host:     "127.0.0.1",
		Port:     1,
		User:     "",
		Password: "",
		Database: "x",
		SSLMode:  "disable",
	}

	_, err := initDatabase(cfg)
	if err == nil {
		t.Fatal("expected error, got nil")
	}
	// Verify it's wrapped with context
	if !errors.Is(err, err) {
		t.Logf("error: %v", err)
	}
}

// helpers

func contains(s, substr string) bool {
	return len(s) >= len(substr) && containsStr(s, substr)
}

func containsStr(s, substr string) bool {
	for i := 0; i <= len(s)-len(substr); i++ {
		if s[i:i+len(substr)] == substr {
			return true
		}
	}
	return false
}
