package middleware

import (
	"fmt"
	"net/http"
	"strconv"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/redis/go-redis/v9"
	"go.uber.org/zap"
)

// RateLimiter 限流器接口
type RateLimiter interface {
	Allow(key string) (bool, time.Duration)
}

// RateLimitConfig 限流配置
type RateLimitConfig struct {
	Redis        *redis.Client
	Logger       *zap.Logger
	
	// IP 限流配置
	IPRate       RateConfig
	
	// 用户限流配置
	UserRate     RateConfig
	
	// 特殊路径限流配置
	PathRates    map[string]RateConfig
	
	// 白名单 IP
	WhitelistIPs []string
	
	// 忽略路径
	SkipPaths    []string
	
	// 限流响应状态码
	StatusCode   int
	
	// 是否在响应头中返回限流信息
	EnableHeaders bool
}

// RateConfig 速率配置
type RateConfig struct {
	Requests int           // 请求数量
	Window   time.Duration // 时间窗口
	Burst    int           // 突发流量
}

// DefaultRateLimitConfig 默认限流配置
func DefaultRateLimitConfig(redisClient *redis.Client, logger *zap.Logger) *RateLimitConfig {
	return &RateLimitConfig{
		Redis:  redisClient,
		Logger: logger,
		IPRate: RateConfig{
			Requests: 100,
			Window:   time.Minute,
			Burst:    20,
		},
		UserRate: RateConfig{
			Requests: 200,
			Window:   time.Minute,
			Burst:    50,
		},
		PathRates:    make(map[string]RateConfig),
		WhitelistIPs: []string{},
		SkipPaths:    []string{"/health", "/metrics", "/debug"},
		StatusCode:   http.StatusTooManyRequests,
		EnableHeaders: true,
	}
}

// SlidingWindowLimiter 滑动窗口限流器
type SlidingWindowLimiter struct {
	redis    *redis.Client
	config   RateConfig
	keyPrefix string
}

// NewSlidingWindowLimiter 创建滑动窗口限流器
func NewSlidingWindowLimiter(redis *redis.Client, config RateConfig, keyPrefix string) *SlidingWindowLimiter {
	return &SlidingWindowLimiter{
		redis:     redis,
		config:    config,
		keyPrefix: keyPrefix,
	}
}

// Allow 检查是否允许请求
func (l *SlidingWindowLimiter) Allow(key string) (bool, time.Duration) {
	now := time.Now().UnixNano()
	windowStart := now - l.config.Window.Nanoseconds()
	
	fullKey := fmt.Sprintf("%s:%s", l.keyPrefix, key)
	
	// 使用 Redis 有序集合实现滑动窗口
	pipe := l.redis.Pipeline()
	
	// 移除窗口外的记录
	pipe.ZRemRangeByScore(l.redis.Context(), fullKey, "0", strconv.FormatInt(windowStart, 10))
	
	// 获取当前窗口内的记录数
	countCmd := pipe.ZCard(l.redis.Context(), fullKey)
	
	// 添加当前请求
	pipe.ZAdd(l.redis.Context(), fullKey, redis.Z{
		Score:  float64(now),
		Member: now,
	})
	
	// 设置键的过期时间
	pipe.Expire(l.redis.Context(), fullKey, l.config.Window)
	
	_, err := pipe.Exec(l.redis.Context())
	if err != nil {
		// Redis 失败时允许请求，避免影响服务
		return true, 0
	}
	
	count := countCmd.Val()
	
	// 检查是否超过限制
	if int(count) >= l.config.Requests {
		// 计算剩余等待时间
		oldest, _ := l.redis.ZRangeWithScores(l.redis.Context(), fullKey, 0, 0).Result()
		if len(oldest) > 0 {
			oldestTimestamp := int64(oldest[0].Score)
			resetTime := time.Unix(0, oldestTimestamp).Add(l.config.Window)
			waitTime := time.Until(resetTime)
			if waitTime > 0 {
				return false, waitTime
			}
		}
		return false, l.config.Window
	}
	
	return true, 0
}

// RateLimitMiddleware 限流中间件
func RateLimitMiddleware(config *RateLimitConfig) gin.HandlerFunc {
	if config == nil {
		panic("RateLimitConfig is required")
	}

	// 创建限流器
	ipLimiter := NewSlidingWindowLimiter(config.Redis, config.IPRate, "ratelimit:ip")
	userLimiter := NewSlidingWindowLimiter(config.Redis, config.UserRate, "ratelimit:user")
	
	// 创建路径限流器
	pathLimiters := make(map[string]*SlidingWindowLimiter)
	for path, rateConfig := range config.PathRates {
		pathLimiters[path] = NewSlidingWindowLimiter(config.Redis, rateConfig, "ratelimit:path")
	}

	return func(c *gin.Context) {
		// 检查是否在白名单中
		clientIP := c.ClientIP()
		if isWhitelisted(clientIP, config.WhitelistIPs) {
			c.Next()
			return
		}

		// 检查是否需要跳过
		if shouldSkipRateLimit(c, config.SkipPaths) {
			c.Next()
			return
		}

		// 检查路径限流
		path := c.Request.URL.Path
		if limiter, exists := pathLimiters[path]; exists {
			allowed, waitTime := limiter.Allow(path)
			if !allowed {
				rejectRequest(c, config, waitTime, "path")
				return
			}
		}

		// 检查用户限流
		userID, hasUser := c.Get("user_id")
		if hasUser {
			userKey := fmt.Sprintf("%v", userID)
			allowed, waitTime := userLimiter.Allow(userKey)
			if !allowed {
				rejectRequest(c, config, waitTime, "user")
				return
			}
			
			if config.EnableHeaders {
				c.Header("X-RateLimit-User-Limit", strconv.Itoa(config.UserRate.Requests))
			}
		}

		// 检查 IP 限流
		allowed, waitTime := ipLimiter.Allow(clientIP)
		if !allowed {
			rejectRequest(c, config, waitTime, "ip")
			return
		}

		if config.EnableHeaders {
			c.Header("X-RateLimit-IP-Limit", strconv.Itoa(config.IPRate.Requests))
		}

		c.Next()
	}
}

// IPRateLimitMiddleware 仅 IP 限流中间件
func IPRateLimitMiddleware(redis *redis.Client, logger *zap.Logger, requests int, window time.Duration) gin.HandlerFunc {
	config := DefaultRateLimitConfig(redis, logger)
	config.IPRate = RateConfig{
		Requests: requests,
		Window:   window,
	}
	config.UserRate = RateConfig{
		Requests: 0, // 禁用用户限流
	}
	return RateLimitMiddleware(config)
}

// UserRateLimitMiddleware 仅用户限流中间件
func UserRateLimitMiddleware(redis *redis.Client, logger *zap.Logger, requests int, window time.Duration) gin.HandlerFunc {
	config := DefaultRateLimitConfig(redis, logger)
	config.UserRate = RateConfig{
		Requests: requests,
		Window:   window,
	}
	config.IPRate = RateConfig{
		Requests: 0, // 禁用 IP 限流
	}
	return RateLimitMiddleware(config)
}

// StrictRateLimitMiddleware 严格限流中间件 (同时限制 IP 和用户)
func StrictRateLimitMiddleware(redis *redis.Client, logger *zap.Logger) gin.HandlerFunc {
	config := DefaultRateLimitConfig(redis, logger)
	config.IPRate = RateConfig{
		Requests: 30,
		Window:   time.Minute,
		Burst:    10,
	}
	config.UserRate = RateConfig{
		Requests: 60,
		Window:   time.Minute,
		Burst:    20,
	}
	return RateLimitMiddleware(config)
}

// shouldSkipRateLimit 检查是否需要跳过限流
func shouldSkipRateLimit(c *gin.Context, skipPaths []string) bool {
	path := c.Request.URL.Path
	for _, skipPath := range skipPaths {
		if strings.HasPrefix(path, skipPath) {
			return true
		}
	}
	return false
}

// isWhitelisted 检查 IP 是否在白名单中
func isWhitelisted(ip string, whitelist []string) bool {
	for _, allowed := range whitelist {
		if allowed == ip {
			return true
		}
		// 支持 CIDR 格式
		if strings.Contains(allowed, "/") {
			if ipInCIDR(ip, allowed) {
				return true
			}
		}
	}
	return false
}

// ipInCIDR 检查 IP 是否在 CIDR 范围内
func ipInCIDR(ip, cidr string) bool {
	// 简化实现，实际项目中可以使用 net 包
	return false
}

// rejectRequest 拒绝请求
func rejectRequest(c *gin.Context, config *RateLimitConfig, waitTime time.Duration, limitType string) {
	if config.EnableHeaders {
		c.Header("X-RateLimit-Retry-After", strconv.Itoa(int(waitTime.Seconds())))
		c.Header("X-RateLimit-Type", limitType)
	}

	config.Logger.Warn("rate limit exceeded",
		zap.String("client_ip", c.ClientIP()),
		zap.String("path", c.Request.URL.Path),
		zap.String("type", limitType),
		zap.Duration("wait_time", waitTime),
	)

	c.JSON(config.StatusCode, gin.H{
		"error":       "rate limit exceeded",
		"retry_after": int(waitTime.Seconds()),
		"type":        limitType,
	})
	c.Abort()
}

// PathRateLimit 为特定路径设置限流
func PathRateLimit(path string, requests int, window time.Duration) map[string]RateConfig {
	return map[string]RateConfig{
		path: {
			Requests: requests,
			Window:   window,
		},
	}
}
