package database

import (
	"context"
	"fmt"
	"time"

	"github.com/jackc/pgx/v5/pgxpool"
)

// Config 数据库连接池配置
type Config struct {
	Host            string
	Port            int
	User            string
	Password        string
	Database        string
	SSLMode         string
	MaxConns        int32         // 最大连接数
	MinConns        int32         // 最小连接数
	MaxConnLifetime time.Duration // 连接最长生命周期
	MaxConnIdleTime time.Duration // 连接最长空闲时间
	HealthCheckPeriod time.Duration // 健康检查周期
}

// DefaultConfig 返回默认配置
func DefaultConfig() *Config {
	return &Config{
		MaxConns:          25,           // 最大 25 个连接
		MinConns:          5,            // 最小 5 个连接
		MaxConnLifetime:   time.Hour,    // 连接最长使用 1 小时
		MaxConnIdleTime:   time.Minute * 30, // 空闲超过 30 分钟释放
		HealthCheckPeriod: time.Minute * 5,  // 每 5 分钟健康检查
	}
}

// NewPool 创建数据库连接池
func NewPool(cfg *Config) (*pgxpool.Pool, error) {
	connString := fmt.Sprintf(
		"host=%s port=%d user=%s password=%s dbname=%s sslmode=%s",
		cfg.Host, cfg.Port, cfg.User, cfg.Password, cfg.Database, cfg.SSLMode,
	)

	poolConfig, err := pgxpool.ParseConfig(connString)
	if err != nil {
		return nil, fmt.Errorf("failed to parse database config: %w", err)
	}

	// 连接池配置
	poolConfig.MaxConns = cfg.MaxConns
	poolConfig.MinConns = cfg.MinConns
	poolConfig.MaxConnLifetime = cfg.MaxConnLifetime
	poolConfig.MaxConnIdleTime = cfg.MaxConnIdleTime
	poolConfig.HealthCheckPeriod = cfg.HealthCheckPeriod

	// 连接配置
	poolConfig.ConnConfig.ConnectTimeout = time.Second * 10

	pool, err := pgxpool.NewWithConfig(context.Background(), poolConfig)
	if err != nil {
		return nil, fmt.Errorf("failed to create connection pool: %w", err)
	}

	// 验证连接
	ctx, cancel := context.WithTimeout(context.Background(), time.Second*5)
	defer cancel()
	
	if err := pool.Ping(ctx); err != nil {
		return nil, fmt.Errorf("failed to ping database: %w", err)
	}

	return pool, nil
}

// PoolStats 返回连接池统计信息
func PoolStats(pool *pgxpool.Pool) map[string]interface{} {
	stats := pool.Stat()
	return map[string]interface{}{
		"total_connections":    stats.TotalConns(),
		"idle_connections":     stats.IdleConns(),
		"acquired_connections": stats.AcquiredConns(),
		"constructing_conns":   stats.ConstructingConns(),
		"max_connections":      stats.MaxConns(),
		"acquire_count":        stats.AcquireCount(),
		"acquire_duration":     stats.AcquireDuration().String(),
		"empty_acquire_count":  stats.EmptyAcquireCount(),
	}
}
