package config

import (
	"fmt"
	"os"
	"strconv"
	"time"
)

// Config holds all configuration
type Config struct {
	Server   ServerConfig
	Database DatabaseConfig
	Redis    RedisConfig
	Kafka    KafkaConfig
	JWT      JWTConfig
	Log      LogConfig
	Metrics  MetricsConfig
}

// ServerConfig holds server configuration
type ServerConfig struct {
	GRPCPort         int
	HTTPPort         int
	ReadTimeout      time.Duration
	WriteTimeout     time.Duration
	GracefulTimeout  time.Duration
}

// DatabaseConfig holds database configuration
type DatabaseConfig struct {
	Host            string
	Port            int
	User            string
	Password        string
	Database        string
	MaxOpenConns    int
	MaxIdleConns    int
	ConnMaxLifetime time.Duration
	SSLMode         string
}

// RedisConfig holds Redis configuration
type RedisConfig struct {
	Addr     string
	Password string
	DB       int
}

// KafkaConfig holds Kafka configuration
type KafkaConfig struct {
	Brokers       []string
	ConsumerGroup string
	Topics        TopicsConfig
}

// TopicsConfig holds Kafka topics configuration
type TopicsConfig struct {
	KeyEvents     string
	Commands      string
	VehicleEvents string
	Telemetry     string
}

// JWTConfig holds JWT configuration
type JWTConfig struct {
	Secret     string
	Expiry     time.Duration
	Issuer     string
}

// LogConfig holds logging configuration
type LogConfig struct {
	Level  string
	Format string
	Output string
}

// MetricsConfig holds metrics configuration
type MetricsConfig struct {
	Enabled bool
	Port   int
	Path   string
}

// Load loads configuration from environment
func Load() *Config {
	return &Config{
		Server: ServerConfig{
			GRPCPort:        getEnvInt("GRPC_PORT", 50051),
			HTTPPort:        getEnvInt("HTTP_PORT", 8080),
			ReadTimeout:     getEnvDuration("READ_TIMEOUT", 10*time.Second),
			WriteTimeout:    getEnvDuration("WRITE_TIMEOUT", 10*time.Second),
			GracefulTimeout: getEnvDuration("GRACEFUL_TIMEOUT", 30*time.Second),
		},
		Database: DatabaseConfig{
			Host:            getEnv("DB_HOST", "localhost"),
			Port:            getEnvInt("DB_PORT", 5432),
			User:            getEnv("DB_USER", "digitalkey"),
			Password:        getEnv("DB_PASSWORD", ""),
			Database:        getEnv("DB_NAME", "digitalkey_db"),
			MaxOpenConns:    getEnvInt("DB_MAX_OPEN_CONNS", 100),
			MaxIdleConns:    getEnvInt("DB_MAX_IDLE_CONNS", 20),
			ConnMaxLifetime: getEnvDuration("DB_CONN_MAX_LIFETIME", time.Hour),
			SSLMode:         getEnv("DB_SSL_MODE", "disable"),
		},
		Redis: RedisConfig{
			Addr:     getEnv("REDIS_ADDR", "localhost:6379"),
			Password: getEnv("REDIS_PASSWORD", ""),
			DB:       getEnvInt("REDIS_DB", 0),
		},
		Kafka: KafkaConfig{
			Brokers:       getEnvSlice("KAFKA_BROKERS", []string{"localhost:9092"}),
			ConsumerGroup: getEnv("KAFKA_CONSUMER_GROUP", "dkcs-consumer"),
			Topics: TopicsConfig{
				KeyEvents:     getEnv("KAFKA_TOPIC_KEY_EVENTS", "dkcs.key.events"),
				Commands:      getEnv("KAFKA_TOPIC_COMMANDS", "dkcs.commands"),
				VehicleEvents: getEnv("KAFKA_TOPIC_VEHICLE_EVENTS", "dkcs.vehicle.events"),
				Telemetry:     getEnv("KAFKA_TOPIC_TELEMETRY", "dkcs.telemetry"),
			},
		},
		JWT: JWTConfig{
			Secret: getEnv("JWT_SECRET", "your-secret-key-change-in-production"),
			Expiry: getEnvDuration("JWT_EXPIRY", 24*time.Hour),
			Issuer: getEnv("JWT_ISSUER", "dkcs"),
		},
		Log: LogConfig{
			Level:  getEnv("LOG_LEVEL", "info"),
			Format: getEnv("LOG_FORMAT", "json"),
			Output: getEnv("LOG_OUTPUT", "stdout"),
		},
		Metrics: MetricsConfig{
			Enabled: getEnvBool("METRICS_ENABLED", true),
			Port:    getEnvInt("METRICS_PORT", 9090),
			Path:    getEnv("METRICS_PATH", "/metrics"),
		},
	}
}

// DSN returns database connection string
func (c *DatabaseConfig) DSN() string {
	return fmt.Sprintf(
		"host=%s port=%d user=%s password=%s dbname=%s sslmode=%s",
		c.Host, c.Port, c.User, c.Password, c.Database, c.SSLMode,
	)
}

// Helper functions
func getEnv(key, defaultValue string) string {
	if value := os.Getenv(key); value != "" {
		return value
	}
	return defaultValue
}

func getEnvInt(key string, defaultValue int) int {
	if value := os.Getenv(key); value != "" {
		if i, err := strconv.Atoi(value); err == nil {
			return i
		}
	}
	return defaultValue
}

func getEnvBool(key string, defaultValue bool) bool {
	if value := os.Getenv(key); value != "" {
		if b, err := strconv.ParseBool(value); err == nil {
			return b
		}
	}
	return defaultValue
}

func getEnvDuration(key string, defaultValue time.Duration) time.Duration {
	if value := os.Getenv(key); value != "" {
		if d, err := time.ParseDuration(value); err == nil {
			return d
		}
	}
	return defaultValue
}

func getEnvSlice(key string, defaultValue []string) []string {
	if value := os.Getenv(key); value != "" {
		// Simple split by comma
		var result []string
		for _, v := range splitByComma(value) {
			if v != "" {
				result = append(result, v)
			}
		}
		if len(result) > 0 {
			return result
		}
	}
	return defaultValue
}

func splitByComma(s string) []string {
	var result []string
	start := 0
	for i := 0; i < len(s); i++ {
		if s[i] == ',' {
			result = append(result, s[start:i])
			start = i + 1
		}
	}
	result = append(result, s[start:])
	return result
}
