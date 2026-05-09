package featureflag

import (
	"context"
	"fmt"
	"sync"
	"time"

	"github.com/redis/go-redis/v9"
)

// FeatureFlag 功能开关
type FeatureFlag struct {
	Key         string    `json:"key"`
	Enabled     bool      `json:"enabled"`
	Description string    `json:"description"`
	UpdatedAt   time.Time `json:"updated_at"`
	Rules       []Rule    `json:"rules,omitempty"`
}

// Rule 功能开关规则
type Rule struct {
	Type      string            `json:"type"`      // percentage, user, tenant
	Condition string            `json:"condition"` // 规则条件
	Params    map[string]string `json:"params"`    // 规则参数
	Enabled   bool              `json:"enabled"`
}

// Manager 功能开关管理器
type Manager struct {
	redis   *redis.Client
	flags   map[string]*FeatureFlag
	mu      sync.RWMutex
	prefix  string
}

// NewManager 创建功能开关管理器
func NewManager(redisClient *redis.Client) *Manager {
	return &Manager{
		redis:  redisClient,
		flags:  make(map[string]*FeatureFlag),
		prefix: "feature_flag:",
	}
}

// IsEnabled 检查功能是否启用
func (m *Manager) IsEnabled(ctx context.Context, key string) bool {
	flag, err := m.GetFlag(ctx, key)
	if err != nil {
		return false
	}
	return flag.Enabled
}

// IsEnabledForUser 检查功能对用户是否启用
func (m *Manager) IsEnabledForUser(ctx context.Context, key string, userID uint) bool {
	flag, err := m.GetFlag(ctx, key)
	if err != nil {
		return false
	}

	if !flag.Enabled {
		return false
	}

	// 检查用户级别规则
	for _, rule := range flag.Rules {
		if rule.Type == "user" && m.matchUserRule(rule, userID) {
			return rule.Enabled
		}
		if rule.Type == "percentage" {
			return m.matchPercentageRule(rule, userID)
		}
	}

	return flag.Enabled
}

// GetFlag 获取功能开关
func (m *Manager) GetFlag(ctx context.Context, key string) (*FeatureFlag, error) {
	// 先从本地缓存获取
	m.mu.RLock()
	if flag, ok := m.flags[key]; ok {
		m.mu.RUnlock()
		return flag, nil
	}
	m.mu.RUnlock()

	// 从 Redis 获取
	if m.redis != nil {
		val, err := m.redis.Get(ctx, m.prefix+key).Result()
		if err == nil {
			// 解析并缓存到本地
			flag := &FeatureFlag{
				Key:     key,
				Enabled: val == "true",
			}
			m.mu.Lock()
			m.flags[key] = flag
			m.mu.Unlock()
			return flag, nil
		}
	}

	return nil, fmt.Errorf("feature flag not found: %s", key)
}

// SetFlag 设置功能开关
func (m *Manager) SetFlag(ctx context.Context, flag *FeatureFlag) error {
	m.mu.Lock()
	m.flags[flag.Key] = flag
	m.mu.Unlock()

	// 同步到 Redis
	if m.redis != nil {
		val := "false"
		if flag.Enabled {
			val = "true"
		}
		return m.redis.Set(ctx, m.prefix+flag.Key, val, 0).Err()
	}

	return nil
}

// ListFlags 列出所有功能开关
func (m *Manager) ListFlags() []*FeatureFlag {
	m.mu.RLock()
	defer m.mu.RUnlock()

	flags := make([]*FeatureFlag, 0, len(m.flags))
	for _, flag := range m.flags {
		flags = append(flags, flag)
	}
	return flags
}

// DeleteFlag 删除功能开关
func (m *Manager) DeleteFlag(ctx context.Context, key string) error {
	m.mu.Lock()
	delete(m.flags, key)
	m.mu.Unlock()

	if m.redis != nil {
		return m.redis.Del(ctx, m.prefix+key).Err()
	}

	return nil
}

// matchUserRule 匹配用户规则
func (m *Manager) matchUserRule(rule Rule, userID uint) bool {
	// 示例实现: 检查用户 ID 是否在规则中
	return false
}

// matchPercentageRule 匹配百分比规则
func (m *Manager) matchPercentageRule(rule Rule, userID uint) bool {
	percentage := 0
	if p, ok := rule.Params["percentage"]; ok {
		fmt.Sscanf(p, "%d", &percentage)
	}
	// 使用用户 ID 计算哈希值来决定
	return int(userID)%100 < percentage
}

// 预定义功能开关常量
const (
	FeatureNewKeyUI      = "new_key_ui"
	FeatureBetaProtocol  = "beta_protocol"
	FeatureAdvancedStats = "advanced_stats"
	FeatureMultiTenant   = "multi_tenant"
)
