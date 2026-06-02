package config

import (
	"os"
	"testing"
	"time"
)

func TestLoadDefaults(t *testing.T) {
	// Clear env to test defaults
	clearEnv()

	cfg := Load()
	if cfg == nil {
		t.Fatal("Load() returned nil")
	}

	// Server defaults
	if cfg.Server.GRPCPort != 50051 {
		t.Errorf("expected GRPCPort=50051, got %d", cfg.Server.GRPCPort)
	}
	if cfg.Server.HTTPPort != 8080 {
		t.Errorf("expected HTTPPort=8080, got %d", cfg.Server.HTTPPort)
	}

	// Database defaults
	if cfg.Database.Host != "localhost" {
		t.Errorf("expected DB host=localhost, got %s", cfg.Database.Host)
	}
	if cfg.Database.Port != 5432 {
		t.Errorf("expected DB port=5432, got %d", cfg.Database.Port)
	}
	if cfg.Database.User != "digitalkey" {
		t.Errorf("expected DB user=digitalkey, got %s", cfg.Database.User)
	}
	if cfg.Database.Password != "" {
		t.Errorf("expected empty DB password, got %s", cfg.Database.Password)
	}
	if cfg.Database.Database != "digitalkey_db" {
		t.Errorf("expected DB name=digitalkey_db, got %s", cfg.Database.Database)
	}
	if cfg.Database.SSLMode != "disable" {
		t.Errorf("expected SSLMode=disable, got %s", cfg.Database.SSLMode)
	}
	if cfg.Database.MaxOpenConns != 100 {
		t.Errorf("expected MaxOpenConns=100, got %d", cfg.Database.MaxOpenConns)
	}
	if cfg.Database.MaxIdleConns != 10 {
		t.Errorf("expected MaxIdleConns=10, got %d", cfg.Database.MaxIdleConns)
	}
	if cfg.Database.ConnMaxLifetime != 30*time.Minute {
		t.Errorf("expected ConnMaxLifetime=30m, got %v", cfg.Database.ConnMaxLifetime)
	}

	// Redis defaults
	if cfg.Redis.Addr != "localhost:6379" {
		t.Errorf("expected Redis addr=localhost:6379, got %s", cfg.Redis.Addr)
	}
	if cfg.Redis.Password != "" {
		t.Errorf("expected empty Redis password, got %s", cfg.Redis.Password)
	}
	if cfg.Redis.DB != 0 {
		t.Errorf("expected Redis DB=0, got %d", cfg.Redis.DB)
	}

	// Kafka defaults
	if len(cfg.Kafka.Brokers) != 1 || cfg.Kafka.Brokers[0] != "localhost:9092" {
		t.Errorf("expected Kafka brokers=[localhost:9092], got %v", cfg.Kafka.Brokers)
	}
	if cfg.Kafka.Topics.KeyEvents != "dkcs.key.events" {
		t.Errorf("expected KeyEvents topic, got %s", cfg.Kafka.Topics.KeyEvents)
	}

	// JWT defaults
	if cfg.JWT.Secret != "change-me-in-production" {
		t.Errorf("expected JWT secret default, got %s", cfg.JWT.Secret)
	}
	if cfg.JWT.ExpireTime != 24*time.Hour {
		t.Errorf("expected JWT expire=24h, got %v", cfg.JWT.ExpireTime)
	}
	if cfg.JWT.Issuer != "dkcs" {
		t.Errorf("expected JWT issuer=dkcs, got %s", cfg.JWT.Issuer)
	}

	// Log defaults
	if cfg.Log.Level != "info" {
		t.Errorf("expected log level=info, got %s", cfg.Log.Level)
	}
	if cfg.Log.Format != "json" {
		t.Errorf("expected log format=json, got %s", cfg.Log.Format)
	}
	if cfg.Log.Output != "stdout" {
		t.Errorf("expected log output=stdout, got %s", cfg.Log.Output)
	}

	// Metrics defaults
	if !cfg.Metrics.Enabled {
		t.Error("expected metrics enabled by default")
	}
	if cfg.Metrics.Port != 9090 {
		t.Errorf("expected metrics port=9090, got %d", cfg.Metrics.Port)
	}
	if cfg.Metrics.Path != "/metrics" {
		t.Errorf("expected metrics path=/metrics, got %s", cfg.Metrics.Path)
	}
}

func TestLoadWithEnv(t *testing.T) {
	clearEnv()
	defer clearEnv()

	// Set environment variables
	os.Setenv("GRPC_PORT", "50052")
	os.Setenv("HTTP_PORT", "8081")
	os.Setenv("DB_HOST", "db.example.com")
	os.Setenv("DB_PORT", "5433")
	os.Setenv("DB_USER", "admin")
	os.Setenv("DB_PASSWORD", "secret123")
	os.Setenv("DB_NAME", "prod_db")
	os.Setenv("DB_SSL_MODE", "require")
	os.Setenv("DB_MAX_OPEN_CONNS", "50")
	os.Setenv("DB_MAX_IDLE_CONNS", "5")
	os.Setenv("DB_CONN_MAX_LIFETIME_MIN", "60")
	os.Setenv("REDIS_ADDR", "redis.example.com:6380")
	os.Setenv("REDIS_PASSWORD", "redis-secret")
	os.Setenv("REDIS_DB", "1")
	os.Setenv("LOG_LEVEL", "debug")
	os.Setenv("LOG_FORMAT", "text")
	os.Setenv("LOG_OUTPUT", "stderr")
	os.Setenv("JWT_SECRET", "my-production-secret")
	os.Setenv("JWT_EXPIRE_HOURS", "48")
	os.Setenv("JWT_ISSUER", "prod-dkcs")

	cfg := Load()

	if cfg.Server.GRPCPort != 50052 {
		t.Errorf("expected GRPCPort=50052, got %d", cfg.Server.GRPCPort)
	}
	if cfg.Server.HTTPPort != 8081 {
		t.Errorf("expected HTTPPort=8081, got %d", cfg.Server.HTTPPort)
	}
	if cfg.Database.Host != "db.example.com" {
		t.Errorf("expected DB host=db.example.com, got %s", cfg.Database.Host)
	}
	if cfg.Database.Port != 5433 {
		t.Errorf("expected DB port=5433, got %d", cfg.Database.Port)
	}
	if cfg.Database.User != "admin" {
		t.Errorf("expected DB user=admin, got %s", cfg.Database.User)
	}
	if cfg.Database.Password != "secret123" {
		t.Errorf("expected DB password=secret123, got %s", cfg.Database.Password)
	}
	if cfg.Database.Database != "prod_db" {
		t.Errorf("expected DB name=prod_db, got %s", cfg.Database.Database)
	}
	if cfg.Database.SSLMode != "require" {
		t.Errorf("expected SSLMode=require, got %s", cfg.Database.SSLMode)
	}
	if cfg.Database.MaxOpenConns != 50 {
		t.Errorf("expected MaxOpenConns=50, got %d", cfg.Database.MaxOpenConns)
	}
	if cfg.Database.MaxIdleConns != 5 {
		t.Errorf("expected MaxIdleConns=5, got %d", cfg.Database.MaxIdleConns)
	}
	if cfg.Database.ConnMaxLifetime != 60*time.Minute {
		t.Errorf("expected ConnMaxLifetime=60m, got %v", cfg.Database.ConnMaxLifetime)
	}
	if cfg.Redis.Addr != "redis.example.com:6380" {
		t.Errorf("expected Redis addr=redis.example.com:6380, got %s", cfg.Redis.Addr)
	}
	if cfg.Redis.Password != "redis-secret" {
		t.Errorf("expected Redis password=redis-secret, got %s", cfg.Redis.Password)
	}
	if cfg.Redis.DB != 1 {
		t.Errorf("expected Redis DB=1, got %d", cfg.Redis.DB)
	}
	if cfg.Log.Level != "debug" {
		t.Errorf("expected log level=debug, got %s", cfg.Log.Level)
	}
	if cfg.JWT.Secret != "my-production-secret" {
		t.Errorf("expected JWT secret, got %s", cfg.JWT.Secret)
	}
	if cfg.JWT.ExpireTime != 48*time.Hour {
		t.Errorf("expected JWT expire=48h, got %v", cfg.JWT.ExpireTime)
	}
}

func TestLoadWithKafkaBrokers(t *testing.T) {
	clearEnv()
	defer clearEnv()

	os.Setenv("KAFKA_BROKERS", "broker1:9092,broker2:9093,broker3:9094")

	cfg := Load()
	if len(cfg.Kafka.Brokers) != 3 {
		t.Fatalf("expected 3 Kafka brokers, got %d: %v", len(cfg.Kafka.Brokers), cfg.Kafka.Brokers)
	}
	if cfg.Kafka.Brokers[0] != "broker1:9092" {
		t.Errorf("expected broker1:9092, got %s", cfg.Kafka.Brokers[0])
	}
	if cfg.Kafka.Brokers[2] != "broker3:9094" {
		t.Errorf("expected broker3:9094, got %s", cfg.Kafka.Brokers[2])
	}
}

func TestLoadWithMetricsDisabled(t *testing.T) {
	clearEnv()
	defer clearEnv()

	os.Setenv("METRICS_ENABLED", "false")

	cfg := Load()
	if cfg.Metrics.Enabled {
		t.Error("expected metrics disabled")
	}
}

func TestDSN(t *testing.T) {
	cfg := DatabaseConfig{
		Host:     "db.example.com",
		Port:     5432,
		User:     "admin",
		Password: "pass",
		Database: "mydb",
		SSLMode:  "require",
	}
	dsn := cfg.DSN()
	expected := "host=db.example.com port=5432 user=admin password=pass dbname=mydb sslmode=require"
	if dsn != expected {
		t.Errorf("DSN = %q, want %q", dsn, expected)
	}
}

func TestDSNWithEmptyPassword(t *testing.T) {
	cfg := DatabaseConfig{
		Host:     "localhost",
		Port:     5432,
		User:     "digitalkey",
		Password: "",
		Database: "digitalkey_db",
		SSLMode:  "disable",
	}
	dsn := cfg.DSN()
	expected := "host=localhost port=5432 user=digitalkey password= dbname=digitalkey_db sslmode=disable"
	if dsn != expected {
		t.Errorf("DSN = %q, want %q", dsn, expected)
	}
}

// ---------- Helper functions tests ----------

func TestGetEnvWithDefault(t *testing.T) {
	os.Unsetenv("TEST_ENV_VAR")
	result := getEnv("TEST_ENV_VAR", "default-value")
	if result != "default-value" {
		t.Errorf("expected default-value, got %s", result)
	}
}

func TestGetEnvWithValue(t *testing.T) {
	os.Setenv("TEST_ENV_VAR", "actual-value")
	defer os.Unsetenv("TEST_ENV_VAR")

	result := getEnv("TEST_ENV_VAR", "default-value")
	if result != "actual-value" {
		t.Errorf("expected actual-value, got %s", result)
	}
}

func TestGetEnvIntWithDefault(t *testing.T) {
	os.Unsetenv("TEST_INT_VAR")
	result := getEnvInt("TEST_INT_VAR", 42)
	if result != 42 {
		t.Errorf("expected 42, got %d", result)
	}
}

func TestGetEnvIntWithValue(t *testing.T) {
	os.Setenv("TEST_INT_VAR", "99")
	defer os.Unsetenv("TEST_INT_VAR")

	result := getEnvInt("TEST_INT_VAR", 42)
	if result != 99 {
		t.Errorf("expected 99, got %d", result)
	}
}

func TestGetEnvIntInvalidValue(t *testing.T) {
	os.Setenv("TEST_INT_VAR", "not-a-number")
	defer os.Unsetenv("TEST_INT_VAR")

	result := getEnvInt("TEST_INT_VAR", 42)
	if result != 42 {
		t.Errorf("expected default 42, got %d", result)
	}
}

func TestGetEnvBoolWithDefault(t *testing.T) {
	os.Unsetenv("TEST_BOOL_VAR")
	result := getEnvBool("TEST_BOOL_VAR", true)
	if !result {
		t.Error("expected true default")
	}
}

func TestGetEnvBoolWithValue(t *testing.T) {
	os.Setenv("TEST_BOOL_VAR", "false")
	defer os.Unsetenv("TEST_BOOL_VAR")

	result := getEnvBool("TEST_BOOL_VAR", true)
	if result {
		t.Error("expected false from env")
	}
}

func TestGetEnvBoolInvalidValue(t *testing.T) {
	os.Setenv("TEST_BOOL_VAR", "maybe")
	defer os.Unsetenv("TEST_BOOL_VAR")

	result := getEnvBool("TEST_BOOL_VAR", true)
	if !result {
		t.Error("expected default true for invalid bool")
	}
}

func TestSplitByComma(t *testing.T) {
	tests := []struct {
		input string
		want  []string
	}{
		{"a,b,c", []string{"a", "b", "c"}},
		{"single", []string{"single"}},
		{"", []string{}},
		{"a,,b", []string{"a", "", "b"}},
	}
	for _, tt := range tests {
		got := splitByComma(tt.input)
		if len(got) != len(tt.want) {
			t.Errorf("splitByComma(%q) = %v, want %v", tt.input, got, tt.want)
			continue
		}
		for i := range got {
			if got[i] != tt.want[i] {
				t.Errorf("splitByComma(%q)[%d] = %q, want %q", tt.input, i, got[i], tt.want[i])
			}
		}
	}
}

func TestGetEnvSliceWithDefault(t *testing.T) {
	os.Unsetenv("TEST_SLICE_VAR")
	result := getEnvSlice("TEST_SLICE_VAR", []string{"default:9092"})
	if len(result) != 1 || result[0] != "default:9092" {
		t.Errorf("expected [default:9092], got %v", result)
	}
}

func TestGetEnvSliceWithValue(t *testing.T) {
	os.Setenv("TEST_SLICE_VAR", "host1:9092,host2:9093")
	defer os.Unsetenv("TEST_SLICE_VAR")

	result := getEnvSlice("TEST_SLICE_VAR", []string{"default:9092"})
	if len(result) != 2 {
		t.Fatalf("expected 2 items, got %d: %v", len(result), result)
	}
	if result[0] != "host1:9092" {
		t.Errorf("expected host1:9092, got %s", result[0])
	}
}

// ---------- Struct field access tests ----------

func TestKafkaTopicsDefaults(t *testing.T) {
	clearEnv()
	cfg := Load()

	if cfg.Kafka.Topics.DLQ != "dkcs.dlq" {
		t.Errorf("expected DLQ topic=dkcs.dlq, got %s", cfg.Kafka.Topics.DLQ)
	}
}

func TestMetricsDefaults(t *testing.T) {
	clearEnv()
	cfg := Load()

	if cfg.Metrics.Path != "/metrics" {
		t.Errorf("expected /metrics, got %s", cfg.Metrics.Path)
	}
}

// clearEnv removes all config-relevant env vars
func clearEnv() {
	envVars := []string{
		"GRPC_PORT", "HTTP_PORT",
		"DB_HOST", "DB_PORT", "DB_USER", "DB_PASSWORD", "DB_NAME", "DB_SSL_MODE",
		"DB_MAX_OPEN_CONNS", "DB_MAX_IDLE_CONNS", "DB_CONN_MAX_LIFETIME_MIN",
		"REDIS_ADDR", "REDIS_PASSWORD", "REDIS_DB",
		"KAFKA_BROKERS",
		"KAFKA_TOPIC_KEY_EVENTS", "KAFKA_TOPIC_COMMANDS", "KAFKA_TOPIC_EVENTS", "KAFKA_TOPIC_DLQ",
		"JWT_SECRET", "JWT_EXPIRE_HOURS", "JWT_ISSUER",
		"LOG_LEVEL", "LOG_FORMAT", "LOG_OUTPUT", "LOG_FILE",
		"METRICS_ENABLED", "METRICS_PORT", "METRICS_PATH",
	}
	for _, v := range envVars {
		os.Unsetenv(v)
	}
}
