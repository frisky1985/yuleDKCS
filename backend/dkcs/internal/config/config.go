package config

import (
	"os"
	"strconv"
	"time"
)

// Config 应用配置
type Config struct {
	Server   ServerConfig
	Database DatabaseConfig
	Redis    RedisConfig
	Kafka    KafkaConfig
	JWT      JWTConfig
	Log      LogConfig
	Metrics  MetricsConfig
}

// ServerConfig 服务器配置
type ServerConfig struct {
	GRPCPort int
	HTTPPort int
}

// DatabaseConfig 数据库配置
type DatabaseConfig struct {
	Host            string
	Port            int
	User            string
	Password        string
	Database        string
	SSLMode         string
	MaxOpenConns    int
	MaxIdleConns    int
	ConnMaxLifetime time.Duration
}

// DSN 返回数据库连接字符串
func (c DatabaseConfig) DSN() string {
	return "host=" + c.Host +
		" port=" + strconv.Itoa(c.Port) +
		" user=" + c.User +
		" password=" + c.Password +
		" dbname=" + c.Database +
		" sslmode=" + c.SSLMode
}

// RedisConfig Redis配置
type RedisConfig struct {
	Addr     string
	Password string
	DB       int
}

// KafkaConfig Kafka配置
type KafkaConfig struct {
	Brokers []string
	Topics  KafkaTopics
}

// KafkaTopics Kafka主题配置
type KafkaTopics struct {
	KeyEvents    string
	Commands     string
	Events       string
	DLQ          string
}

// JWTConfig JWT配置
type JWTConfig struct {
	Secret     string
	ExpireTime time.Duration
	Issuer     string
}

// LogConfig 日志配置
type LogConfig struct {
	Level  string // debug, info, warn, error
	Format string // json, text
	Output string // stdout, stderr, file
	File   string // 日志文件路径
}

// MetricsConfig 指标配置
type MetricsConfig struct {
	Enabled bool
	Port    int
	Path    string
}

// Load 从环境变量加载配置
func Load() *Config {
	return &Config{
		Server: ServerConfig{
			GRPCPort: getEnvInt("GRPC_PORT", 50051),
			HTTPPort: getEnvInt("HTTP_PORT", 8080),
		},
		Database: DatabaseConfig{
			Host:            getEnv("DB_HOST", "localhost"),
			Port:            getEnvInt("DB_PORT", 5432),
			User:            getEnv("DB_USER", "digitalkey"),
			Password:        getEnv("DB_PASSWORD", ""),
			Database:        getEnv("DB_NAME", "digitalkey_db"),
			SSLMode:         getEnv("DB_SSL_MODE", "disable"),
			MaxOpenConns:    getEnvInt("DB_MAX_OPEN_CONNS", 100),
			MaxIdleConns:    getEnvInt("DB_MAX_IDLE_CONNS", 10),
			ConnMaxLifetime: time.Duration(getEnvInt("DB_CONN_MAX_LIFETIME_MIN", 30)) * time.Minute,
		},
		Redis: RedisConfig{
			Addr:     getEnv("REDIS_ADDR", "localhost:6379"),
			Password: getEnv("REDIS_PASSWORD", ""),
			DB:       getEnvInt("REDIS_DB", 0),
		},
		Kafka: KafkaConfig{
			Brokers: getEnvSlice("KAFKA_BROKERS", []string{"localhost:9092"}),
			Topics: KafkaTopics{
				KeyEvents: getEnv("KAFKA_TOPIC_KEY_EVENTS", "dkcs.key.events"),
				Commands:  getEnv("KAFKA_TOPIC_COMMANDS", "dkcs.commands"),
				Events:    getEnv("KAFKA_TOPIC_EVENTS", "dkcs.events"),
				DLQ:       getEnv("KAFKA_TOPIC_DLQ", "dkcs.dlq"),
			},
		},
		JWT: JWTConfig{
			Secret:     getEnv("JWT_SECRET", "change-me-in-production"),
			ExpireTime: time.Duration(getEnvInt("JWT_EXPIRE_HOURS", 24)) * time.Hour,
			Issuer:     getEnv("JWT_ISSUER", "dkcs"),
		},
		Log: LogConfig{
			Level:  getEnv("LOG_LEVEL", "info"),
			Format: getEnv("LOG_FORMAT", "json"),
			Output: getEnv("LOG_OUTPUT", "stdout"),
			File:   getEnv("LOG_FILE", ""),
		},
		Metrics: MetricsConfig{
			Enabled: getEnvBool("METRICS_ENABLED", true),
			Port:    getEnvInt("METRICS_PORT", 9090),
			Path:    getEnv("METRICS_PATH", "/metrics"),
		},
	}
}

// 辅助函数
func getEnv(key, defaultValue string) string {
	if value := os.Getenv(key); value != "" {
		return value
	}
	return defaultValue
}

func getEnvInt(key string, defaultValue int) int {
	if value := os.Getenv(key); value != "" {
		if int, err := strconv.Atoi(value); err == nil {
			return int
		}
	}
	return defaultValue
}

func getEnvBool(key string, defaultValue bool) bool {
	if value := os.Getenv(key); value != "" {
		if bool, err := strconv.ParseBool(value); err == nil {
			return bool
		}
	}
	return defaultValue
}

func getEnvSlice(key string, defaultValue []string) []string {
	if value := os.Getenv(key); value != "" {
		// 简单的逗号分隔
		result := []string{}
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
	result := []string{}
	current := ""
	for _, c := range s {
		if c == ',' {
			result = append(result, current)
			current = ""
		} else {
			current += string(c)
		}
	}
	if current != "" {
		result = append(result, current)
	}
	return result
}
