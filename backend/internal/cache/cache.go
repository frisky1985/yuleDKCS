package cache

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"sync"
	"time"

	"github.com/patrickmn/go-cache"
	"github.com/redis/go-redis/v9"
	"go.uber.org/zap"
)

// CacheLevel 缓存级別
type CacheLevel int

const (
	L1 CacheLevel = iota // 内存缓存
	L2                   // Redis缓存
	Both                 // 两级缓存
)

// Manager 缓存管理器
type Manager struct {
	l1     *cache.Cache      // 内存缓存
	l2     *redis.Client     // Redis缓存
	logger *zap.Logger
	config *Config
	mu     sync.RWMutex
}

// Config 缓存配置
type Config struct {
	L1Expiration      time.Duration // L1默认过期时间
	L1CleanupInterval time.Duration // L1清理间隔
	L2Expiration      time.Duration // L2默认过期时间
	LockTimeout       time.Duration // 分布式锁超时时间
	LockRetryDelay    time.Duration // 锁重试间隔
	MaxLockRetries    int           // 最大锁重试次数
}

// DefaultConfig 默认缓存配置
func DefaultConfig() *Config {
	return &Config{
		L1Expiration:      5 * time.Minute,
		L1CleanupInterval: 10 * time.Minute,
		L2Expiration:      30 * time.Minute,
		LockTimeout:       10 * time.Second,
		LockRetryDelay:    100 * time.Millisecond,
		MaxLockRetries:    50,
	}
}

// NewManager 创建缓存管理器
func NewManager(redisClient *redis.Client, logger *zap.Logger, cfg *Config) *Manager {
	if cfg == nil {
		cfg = DefaultConfig()
	}

	return &Manager{
		l1:     cache.New(cfg.L1Expiration, cfg.L1CleanupInterval),
		l2:     redisClient,
		logger: logger,
		config: cfg,
	}
}

// keyWithPrefix 为键添加前缀
func (m *Manager) keyWithPrefix(key string) string {
	return fmt.Sprintf("yuledkcs:%s", key)
}

// Get 获取缓存
func (m *Manager) Get(ctx context.Context, key string, dest interface{}) (bool, error) {
	// 首先尝试 L1
	if val, found := m.l1.Get(key); found {
		if data, ok := val.([]byte); ok {
			if err := json.Unmarshal(data, dest); err == nil {
				m.logger.Debug("cache hit L1", zap.String("key", key))
				return true, nil
			}
		}
	}

	// 然后尝试 L2
	if m.l2 != nil {
		data, err := m.l2.Get(ctx, m.keyWithPrefix(key)).Bytes()
		if err == nil {
			if err := json.Unmarshal(data, dest); err == nil {
				// 回填 L1
				m.l1.Set(key, data, m.config.L1Expiration)
				m.logger.Debug("cache hit L2, backfill L1", zap.String("key", key))
				return true, nil
			}
		} else if err != redis.Nil {
			m.logger.Warn("redis get failed", zap.String("key", key), zap.Error(err))
		}
	}

	return false, nil
}

// Set 设置缓存
func (m *Manager) Set(ctx context.Context, key string, value interface{}, level CacheLevel, expiration time.Duration) error {
	data, err := json.Marshal(value)
	if err != nil {
		return fmt.Errorf("marshal value failed: %w", err)
	}

	// 设置 L1
	if level == L1 || level == Both {
		m.l1.Set(key, data, expiration)
	}

	// 设置 L2
	if (level == L2 || level == Both) && m.l2 != nil {
		if expiration == 0 {
			expiration = m.config.L2Expiration
		}
		if err := m.l2.Set(ctx, m.keyWithPrefix(key), data, expiration).Err(); err != nil {
			m.logger.Warn("redis set failed", zap.String("key", key), zap.Error(err))
			return err
		}
	}

	m.logger.Debug("cache set", zap.String("key", key), zap.String("level", level.String()))
	return nil
}

// Delete 删除缓存
func (m *Manager) Delete(ctx context.Context, key string) error {
	m.l1.Delete(key)
	
	if m.l2 != nil {
		if err := m.l2.Del(ctx, m.keyWithPrefix(key)).Err(); err != nil {
			return err
		}
	}

	return nil
}

// DeleteByPattern 根据模式删除缓存
func (m *Manager) DeleteByPattern(ctx context.Context, pattern string) error {
	// 清除 L1中符合条件的键
	m.mu.Lock()
	for key := range m.l1.Items() {
		if matchPattern(key, pattern) {
			m.l1.Delete(key)
		}
	}
	m.mu.Unlock()

	// 清除 L2中符合条件的键
	if m.l2 != nil {
		keys, err := m.l2.Keys(ctx, m.keyWithPrefix(pattern)).Result()
		if err != nil {
			return err
		}
		if len(keys) > 0 {
			if err := m.l2.Del(ctx, keys...).Err(); err != nil {
				return err
			}
		}
	}

	return nil
}

// GetOrSet 获取或设置缓存 (防缓存击穿)
func (m *Manager) GetOrSet(ctx context.Context, key string, dest interface{}, level CacheLevel, expiration time.Duration, fn func() (interface{}, error)) error {
	// 尝试获取缓存
	found, err := m.Get(ctx, key, dest)
	if err != nil {
		return err
	}
	if found {
		return nil
	}

	// 使用分布式锁防止缓存击穿
	lockKey := fmt.Sprintf("lock:%s", key)
	lockValue := generateLockValue()

	// 尝试获取锁
	locked, err := m.acquireLock(ctx, lockKey, lockValue)
	if err != nil {
		m.logger.Warn("acquire lock failed", zap.String("key", key), zap.Error(err))
	}

	if locked {
		defer m.releaseLock(ctx, lockKey, lockValue)

		// 双检，防止多个请求同时打穿缓存
		found, _ = m.Get(ctx, key, dest)
		if found {
			return nil
		}

		// 执行数据加载函数
		val, err := fn()
		if err != nil {
			return err
		}

		// 设置缓存
		if err := m.Set(ctx, key, val, level, expiration); err != nil {
			m.logger.Warn("set cache failed", zap.String("key", key), zap.Error(err))
		}

		// 复制结果
		data, _ := json.Marshal(val)
		return json.Unmarshal(data, dest)
	}

	// 未获取锁，等待后重试
	for i := 0; i < m.config.MaxLockRetries; i++ {
		time.Sleep(m.config.LockRetryDelay)
		found, _ = m.Get(ctx, key, dest)
		if found {
			return nil
		}
	}

	// 超时，直接执行函数
	val, err := fn()
	if err != nil {
		return err
	}
	data, _ := json.Marshal(val)
	return json.Unmarshal(data, dest)
}

// acquireLock 获取分布式锁
func (m *Manager) acquireLock(ctx context.Context, key, value string) (bool, error) {
	if m.l2 == nil {
		return true, nil
	}
	ok, err := m.l2.SetNX(ctx, m.keyWithPrefix(key), value, m.config.LockTimeout).Result()
	return ok, err
}

// releaseLock 释放分布式锁
func (m *Manager) releaseLock(ctx context.Context, key, value string) {
	if m.l2 == nil {
		return
	}
	// 使用 Lua 脚本确保原子性操作
	script := `
		if redis.call("get", KEYS[1]) == ARGV[1] then
			return redis.call("del", KEYS[1])
		else
			return 0
		end
	`
	m.l2.Eval(ctx, script, []string{m.keyWithPrefix(key)}, value)
}

// SetWithNull 设置空值防缓存穿透
func (m *Manager) SetWithNull(ctx context.Context, key string, level CacheLevel, nullTTL time.Duration) error {
	nullValue := []byte("null")
	if level == L1 || level == Both {
		m.l1.Set(key, nullValue, nullTTL)
	}
	if (level == L2 || level == Both) && m.l2 != nil {
		return m.l2.Set(ctx, m.keyWithPrefix(key), nullValue, nullTTL).Err()
	}
	return nil
}

// IsNull 检查是否为空值缓存
func (m *Manager) IsNull(data []byte) bool {
	return string(data) == "null"
}

// ClearL1 清空内存缓存
func (m *Manager) ClearL1() {
	m.l1.Flush()
	m.logger.Info("L1 cache cleared")
}

// Stats 获取缓存统计
func (m *Manager) Stats() map[string]interface{} {
	stats := map[string]interface{}{
		"l1_items": m.l1.ItemCount(),
	}

	if m.l2 != nil {
		info := m.l2.Info(context.Background())
		if val, err := info.Result(); err == nil {
			stats["redis_info"] = val
		}
	}

	return stats
}

// matchPattern 简单的模式匹配
func matchPattern(key, pattern string) bool {
	// 简化实现，支持 * 通配符
	if pattern == "*" {
		return true
	}
	if len(pattern) > 0 && pattern[len(pattern)-1] == '*' {
		prefix := pattern[:len(pattern)-1]
		return len(key) >= len(prefix) && key[:len(prefix)] == prefix
	}
	return key == pattern
}

// generateLockValue 生成锁值
func generateLockValue() string {
	h := sha256.New()
	h.Write([]byte(fmt.Sprintf("%d", time.Now().UnixNano())))
	return hex.EncodeToString(h.Sum(nil))
}

// String 返回缓存级别字符串
func (c CacheLevel) String() string {
	switch c {
	case L1:
		return "L1"
	case L2:
		return "L2"
	case Both:
		return "Both"
	default:
		return "Unknown"
	}
}
