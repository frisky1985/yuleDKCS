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
	MQTT     MQTTConfig
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
	// MigrationsDir SQL 迁移目录 (启动时自动执行; 目录不存在仅告警)
	MigrationsDir string
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
	KeyEvents string
	Commands  string
	Events    string
	DLQ       string
}

// MQTTConfig MQTT配置 (TCU车端连接通道)
type MQTTConfig struct {
	Broker         string        // MQTT broker address
	ClientID       string        // Client identifier
	Username       string        // Username for authentication
	Password       string        // Password for authentication
	QoS            byte          // Quality of Service level
	KeepAlive      time.Duration // Keep-alive interval
	ConnectTimeout time.Duration // Connection timeout
	AutoReconnect  bool          // Automatically reconnect
	MaxReconnect   int           // Maximum reconnection attempts
	ReconnectDelay time.Duration // Delay between reconnection attempts
	TLSEnabled     bool          // Enable TLS
	TLSCACert      string        // Path to CA certificate
	TLSClientCert  string        // Path to client certificate
	TLSClientKey   string        // Path to client private key
	Producer       ProducerConfig
	Consumer       ConsumerConfig
	Pool           PoolConfig
}

// ProducerConfig MQTT Producer配置
type ProducerConfig struct {
	DefaultQoS          byte          // Default QoS level for commands
	DefaultRetained     bool          // Whether to retain messages
	CommandTimeout      time.Duration // Time to wait for command acknowledgment
	MaxRetries          int           // Maximum number of retry attempts
	RetryInitialDelay   time.Duration // Initial delay before retry
	RetryMaxDelay       time.Duration // Maximum delay between retries
	RetryBackoffFactor  float64       // Exponential backoff multiplier
	EnablePersistence   bool          // Enable message persistence
	EnableDeduplication bool          // Enable message deduplication
	PoolMinSize         int           // Minimum pool size
	PoolMaxSize         int           // Maximum pool size
}

// ConsumerConfig MQTT Consumer配置
type ConsumerConfig struct {
	DefaultQoS             byte          // Default QoS level for subscriptions
	BufferSize             int           // Channel buffer size
	EnableEventPersistence bool          // Enable event persistence
	EnableMetrics          bool          // Enable metrics collection
	WorkerCount            int           // Number of worker goroutines
	MessageTimeout         time.Duration // Timeout for processing each message
}

// PoolConfig MQTT连接池配置
type PoolConfig struct {
	MinSize     int           // Minimum number of connections
	MaxSize     int           // Maximum number of connections
	IdleTimeout time.Duration // Idle timeout for connections
	MaxLifetime time.Duration // Maximum lifetime for connections
	HealthCheck time.Duration // Health check interval
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
			MigrationsDir:   getEnv("DB_MIGRATIONS_DIR", "db/migrations"),
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
		MQTT: MQTTConfig{
			Broker:         getEnv("MQTT_BROKER", "tcp://localhost:1883"),
			ClientID:       getEnv("MQTT_CLIENT_ID", "dkcs-cloud"),
			Username:       getEnv("MQTT_USERNAME", ""),
			Password:       getEnv("MQTT_PASSWORD", ""),
			QoS:            byte(getEnvInt("MQTT_QOS", 1)),
			KeepAlive:      time.Duration(getEnvInt("MQTT_KEEP_ALIVE_SEC", 60)) * time.Second,
			ConnectTimeout: time.Duration(getEnvInt("MQTT_CONNECT_TIMEOUT_SEC", 30)) * time.Second,
			AutoReconnect:  getEnvBool("MQTT_AUTO_RECONNECT", true),
			MaxReconnect:   getEnvInt("MQTT_MAX_RECONNECT", 5),
			ReconnectDelay: time.Duration(getEnvInt("MQTT_RECONNECT_DELAY_SEC", 5)) * time.Second,
			TLSEnabled:     getEnvBool("MQTT_TLS_ENABLED", false),
			TLSCACert:      getEnv("MQTT_TLS_CA_CERT", ""),
			TLSClientCert:  getEnv("MQTT_TLS_CLIENT_CERT", ""),
			TLSClientKey:   getEnv("MQTT_TLS_CLIENT_KEY", ""),
			Producer: ProducerConfig{
				DefaultQoS:          byte(getEnvInt("MQTT_PRODUCER_QOS", 1)),
				DefaultRetained:     getEnvBool("MQTT_PRODUCER_RETAINED", false),
				CommandTimeout:      time.Duration(getEnvInt("MQTT_PRODUCER_CMD_TIMEOUT_SEC", 30)) * time.Second,
				MaxRetries:          getEnvInt("MQTT_PRODUCER_MAX_RETRIES", 3),
				RetryInitialDelay:   time.Duration(getEnvInt("MQTT_PRODUCER_RETRY_DELAY_SEC", 1)) * time.Second,
				RetryMaxDelay:       time.Duration(getEnvInt("MQTT_PRODUCER_RETRY_MAX_DELAY_SEC", 30)) * time.Second,
				RetryBackoffFactor:  getEnvFloat("MQTT_PRODUCER_RETRY_BACKOFF", 2.0),
				EnablePersistence:   getEnvBool("MQTT_PRODUCER_PERSISTENCE", true),
				EnableDeduplication: getEnvBool("MQTT_PRODUCER_DEDUP", true),
				PoolMinSize:         getEnvInt("MQTT_PRODUCER_POOL_MIN", 2),
				PoolMaxSize:         getEnvInt("MQTT_PRODUCER_POOL_MAX", 10),
			},
			Consumer: ConsumerConfig{
				DefaultQoS:             byte(getEnvInt("MQTT_CONSUMER_QOS", 1)),
				BufferSize:             getEnvInt("MQTT_CONSUMER_BUFFER_SIZE", 1000),
				EnableEventPersistence: getEnvBool("MQTT_CONSUMER_PERSISTENCE", true),
				EnableMetrics:          getEnvBool("MQTT_CONSUMER_METRICS", true),
				WorkerCount:            getEnvInt("MQTT_CONSUMER_WORKERS", 5),
				MessageTimeout:         time.Duration(getEnvInt("MQTT_CONSUMER_MSG_TIMEOUT_SEC", 30)) * time.Second,
			},
			Pool: PoolConfig{
				MinSize:     getEnvInt("MQTT_POOL_MIN_SIZE", 2),
				MaxSize:     getEnvInt("MQTT_POOL_MAX_SIZE", 10),
				IdleTimeout: time.Duration(getEnvInt("MQTT_POOL_IDLE_TIMEOUT_SEC", 300)) * time.Second,
				MaxLifetime: time.Duration(getEnvInt("MQTT_POOL_MAX_LIFETIME_MIN", 30)) * time.Minute,
				HealthCheck: time.Duration(getEnvInt("MQTT_POOL_HEALTH_CHECK_SEC", 30)) * time.Second,
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

func getEnvFloat(key string, defaultValue float64) float64 {
	if value := os.Getenv(key); value != "" {
		if float, err := strconv.ParseFloat(value, 64); err == nil {
			return float
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
