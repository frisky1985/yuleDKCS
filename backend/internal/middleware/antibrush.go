package middleware

import (
	"crypto/sha256"
	"fmt"
	"net/http"
	"strings"
	"sync"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/redis/go-redis/v9"
)

// AntiBrushConfig 防刷配置
// 符合 ISO/SAE 21434 标准 - 入侵检测和防护系统要求
// WP-13: 入侵检测系统、WP-15: 安全监控

// RateLimitConfig 限流配置
type RateLimitConfig struct {
	// IP级别限流
	IPRateLimit    int           // IP每分钟最大请求数
	IPBurstLimit   int           // IP突发请求限制
	IPBlockDuration time.Duration // IP封禁时长
	
	// 用户级别限流
	UserRateLimit    int           // 用户每分钟最大请求数
	UserBurstLimit   int           // 用户突发请求限制
	UserBlockDuration time.Duration // 用户封禁时长
	
	// API级别限流
	APIRateLimits map[string]int // 特定API的限流配置
	
	// 异常检测阈值
	ErrorThreshold      int           // 错误请求阈值（触发封禁）
	ErrorTimeWindow     time.Duration // 错误检测时间窗口
	AbnormalRequestSize int64         // 异常请求大小（KB）
}

// DefaultRateLimitConfig 默认限流配置
func DefaultRateLimitConfig() *RateLimitConfig {
	return &RateLimitConfig{
		IPRateLimit:       100,
		IPBurstLimit:      20,
		IPBlockDuration:   30 * time.Minute,
		UserRateLimit:     60,
		UserBurstLimit:    10,
		UserBlockDuration: 1 * time.Hour,
		APIRateLimits: map[string]int{
			"/api/v1/auth/login":    5,   // 登录接口限流
			"/api/v1/auth/register": 3,   // 注册接口限流
			"/api/v1/keys/generate": 10,  // 钥匙生成限流
			"/api/v1/vehicles/cmd/": 30,  // 车辆命令限流
		},
		ErrorThreshold:      10,
		ErrorTimeWindow:     5 * time.Minute,
		AbnormalRequestSize: 10 * 1024, // 10MB
	}
}

// AntiBrushMiddleware 防刷中间件
type AntiBrushMiddleware struct {
	redisClient *redis.Client
	config      *RateLimitConfig
	localCache  *localRateCache
}

// localRateCache 本地限流缓存（内存级，用于快速判断）
type localRateCache struct {
	mu      sync.RWMutex
	records map[string]*rateRecord
}

type rateRecord struct {
	count      int
	resetAt    time.Time
	errorCount int
	blocked    bool
	blockUntil *time.Time
}

// NewAntiBrushMiddleware 创建防刷中间件
func NewAntiBrushMiddleware(redisClient *redis.Client, config *RateLimitConfig) *AntiBrushMiddleware {
	if config == nil {
		config = DefaultRateLimitConfig()
	}
	
	middleware := &AntiBrushMiddleware{
		redisClient: redisClient,
		config:      config,
		localCache: &localRateCache{
			records: make(map[string]*rateRecord),
		},
	}
	
	// 启动本地缓存清理协程
	go middleware.cleanupLoop()
	
	return middleware
}

// AntiBrush 返回防刷中间件Handler
func (ab *AntiBrushMiddleware) AntiBrush() gin.HandlerFunc {
	return func(c *gin.Context) {
		// 获取客户端标识
		clientID := ab.getClientID(c)
		userID, _ := c.Get("user_id")
		
		// 检查是否被封禁
		if ab.isBlocked(clientID, userID) {
			ab.logSecurityEvent(c, "BLOCKED_ACCESS", clientID, "Client is blocked")
			c.JSON(http.StatusForbidden, gin.H{
				"error": "Access temporarily blocked due to suspicious activity",
				"code":  "BLOCKED",
			})
			c.Abort()
			return
		}
		
		// 检查异常请求大小
		if c.Request.ContentLength > ab.config.AbnormalRequestSize {
			ab.logSecurityEvent(c, "ABNORMAL_REQUEST_SIZE", clientID, 
				fmt.Sprintf("Request size: %d bytes", c.Request.ContentLength))
			ab.recordError(clientID)
			c.JSON(http.StatusBadRequest, gin.H{
				"error": "Request size exceeds limit",
			})
			c.Abort()
			return
		}
		
		// IP级别限流检查
		if !ab.checkIPRateLimit(clientID) {
			ab.logSecurityEvent(c, "IP_RATE_LIMIT_EXCEEDED", clientID, "IP rate limit exceeded")
			c.JSON(http.StatusTooManyRequests, gin.H{
				"error": "Too many requests from this IP",
				"code":  "RATE_LIMIT_EXCEEDED",
			})
			c.Abort()
			return
		}
		
		// 用户级别限流检查（如果已登录）
		if userID != nil {
			if !ab.checkUserRateLimit(fmt.Sprintf("%v", userID)) {
				ab.logSecurityEvent(c, "USER_RATE_LIMIT_EXCEEDED", clientID, 
					fmt.Sprintf("User: %v", userID))
				c.JSON(http.StatusTooManyRequests, gin.H{
					"error": "Too many requests for this user",
					"code":  "RATE_LIMIT_EXCEEDED",
				})
				c.Abort()
				return
			}
		}
		
		// API级别限流检查
		if !ab.checkAPIRateLimit(c.Request.URL.Path, clientID) {
			ab.logSecurityEvent(c, "API_RATE_LIMIT_EXCEEDED", clientID, 
				fmt.Sprintf("API: %s", c.Request.URL.Path))
			c.JSON(http.StatusTooManyRequests, gin.H{
				"error": "API rate limit exceeded",
				"code":  "API_RATE_LIMIT_EXCEEDED",
			})
			c.Abort()
			return
		}
		
		// 记录请求（用于后续异常检测）
		ab.recordRequest(clientID)
		
		c.Next()
		
		// 检查响应状态，记录错误
		if c.Writer.Status() >= 400 {
			ab.recordError(clientID)
		}
	}
}

// getClientID 获取客户端唯一标识
func (ab *AntiBrushMiddleware) getClientID(c *gin.Context) string {
	// 尝试获取真实IP（考虑代理）
	clientIP := c.ClientIP()
	
	// 如果有X-Forwarded-For头，使用第一个IP
	forwardedFor := c.GetHeader("X-Forwarded-For")
	if forwardedFor != "" {
		ips := strings.Split(forwardedFor, ",")
		if len(ips) > 0 {
			clientIP = strings.TrimSpace(ips[0])
		}
	}
	
	// 生成客户端指纹（IP + UserAgent哈希）
	userAgent := c.GetHeader("User-Agent")
	fingerprint := sha256.Sum256([]byte(clientIP + userAgent))
	
	return fmt.Sprintf("%x", fingerprint[:8])
}

// isBlocked 检查客户端是否被封禁
func (ab *AntiBrushMiddleware) isBlocked(clientID string, userID interface{}) bool {
	ctx := ab.redisClient.Context()
	
	// 检查IP封禁
	ipBlocked, _ := ab.redisClient.Exists(ctx, "block:ip:"+clientID).Result()
	if ipBlocked > 0 {
		return true
	}
	
	// 检查用户封禁
	if userID != nil {
		userBlocked, _ := ab.redisClient.Exists(ctx, "block:user:"+fmt.Sprintf("%v", userID)).Result()
		if userBlocked > 0 {
			return true
		}
	}
	
	// 检查本地缓存
	ab.localCache.mu.RLock()
	defer ab.localCache.mu.RUnlock()
	
	if record, exists := ab.localCache.records[clientID]; exists {
		if record.blocked && record.blockUntil != nil && time.Now().Before(*record.blockUntil) {
			return true
		}
	}
	
	return false
}

// checkIPRateLimit 检查IP限流
func (ab *AntiBrushMiddleware) checkIPRateLimit(clientID string) bool {
	ctx := ab.redisClient.Context()
	key := "ratelimit:ip:" + clientID
	
	// 使用Redis原子操作进行限流
	pipe := ab.redisClient.Pipeline()
	incr := pipe.Incr(ctx, key)
	pipe.Expire(ctx, key, time.Minute)
	_, _ = pipe.Exec(ctx)
	
	count := incr.Val()
	return count <= int64(ab.config.IPRateLimit)
}

// checkUserRateLimit 检查用户限流
func (ab *AntiBrushMiddleware) checkUserRateLimit(userID string) bool {
	ctx := ab.redisClient.Context()
	key := "ratelimit:user:" + userID
	
	pipe := ab.redisClient.Pipeline()
	incr := pipe.Incr(ctx, key)
	pipe.Expire(ctx, key, time.Minute)
	_, _ = pipe.Exec(ctx)
	
	count := incr.Val()
	return count <= int64(ab.config.UserRateLimit)
}

// checkAPIRateLimit 检查API限流
func (ab *AntiBrushMiddleware) checkAPIRateLimit(path, clientID string) bool {
	// 匹配API限流规则
	for pattern, limit := range ab.config.APIRateLimits {
		if strings.HasPrefix(path, pattern) || strings.Contains(path, pattern) {
			ctx := ab.redisClient.Context()
			key := fmt.Sprintf("ratelimit:api:%s:%s", pattern, clientID)
			
			pipe := ab.redisClient.Pipeline()
			incr := pipe.Incr(ctx, key)
			pipe.Expire(ctx, key, time.Minute)
			_, _ = pipe.Exec(ctx)
			
			count := incr.Val()
			return count <= int64(limit)
		}
	}
	return true
}

// recordRequest 记录请求
func (ab *AntiBrushMiddleware) recordRequest(clientID string) {
	ab.localCache.mu.Lock()
	defer ab.localCache.mu.Unlock()
	
	now := time.Now()
	record, exists := ab.localCache.records[clientID]
	if !exists || now.After(record.resetAt) {
		ab.localCache.records[clientID] = &rateRecord{
			count:   1,
			resetAt: now.Add(time.Minute),
		}
	} else {
		record.count++
	}
}

// recordError 记录错误
func (ab *AntiBrushMiddleware) recordError(clientID string) {
	ab.localCache.mu.Lock()
	defer ab.localCache.mu.Unlock()
	
	now := time.Now()
	record, exists := ab.localCache.records[clientID]
	if !exists {
		record = &rateRecord{
			resetAt: now.Add(ab.config.ErrorTimeWindow),
		}
		ab.localCache.records[clientID] = record
	}
	
	record.errorCount++
	
	// 检查是否超过错误阈值，触发封禁
	if record.errorCount >= ab.config.ErrorThreshold {
		blockedUntil := now.Add(ab.config.IPBlockDuration)
		record.blocked = true
		record.blockUntil = &blockedUntil
		
		// 同步到Redis
		ctx := ab.redisClient.Context()
		ab.redisClient.Set(ctx, "block:ip:"+clientID, "1", ab.config.IPBlockDuration)
	}
}

// blockUser 封禁用户
func (ab *AntiBrushMiddleware) blockUser(userID string, duration time.Duration) {
	ctx := ab.redisClient.Context()
	ab.redisClient.Set(ctx, "block:user:"+userID, "1", duration)
}

// logSecurityEvent 记录安全事件
// 符合 ISO/SAE 21434 标准 - 安全事件日志要求
func (ab *AntiBrushMiddleware) logSecurityEvent(c *gin.Context, eventType, clientID, details string) {
	event := map[string]interface{}{
		"timestamp":   time.Now().UTC().Format(time.RFC3339),
		"event_type":  eventType,
		"client_id":   clientID,
		"ip":          c.ClientIP(),
		"path":        c.Request.URL.Path,
		"method":      c.Request.Method,
		"user_agent":  c.GetHeader("User-Agent"),
		"details":     details,
		"severity":    ab.getEventSeverity(eventType),
	}
	
	// 存储到Redis用于后续分析和告警
	ctx := ab.redisClient.Context()
	eventKey := fmt.Sprintf("security:event:%s:%d", eventType, time.Now().Unix())
	eventData := fmt.Sprintf("%v", event)
	ab.redisClient.Set(ctx, eventKey, eventData, 7*24*time.Hour)
	
	// 同时记录到审计日志列表
	ab.redisClient.LPush(ctx, "security:events", eventData)
	ab.redisClient.LTrim(ctx, "security:events", 0, 9999) // 保留最近10000条
}

// getEventSeverity 获取事件严重级别
func (ab *AntiBrushMiddleware) getEventSeverity(eventType string) string {
	switch eventType {
	case "BLOCKED_ACCESS":
		return "HIGH"
	case "IP_RATE_LIMIT_EXCEEDED", "USER_RATE_LIMIT_EXCEEDED":
		return "MEDIUM"
	case "API_RATE_LIMIT_EXCEEDED":
		return "LOW"
	case "ABNORMAL_REQUEST_SIZE":
		return "MEDIUM"
	default:
		return "INFO"
	}
}

// cleanupLoop 清理过期缓存
func (ab *AntiBrushMiddleware) cleanupLoop() {
	ticker := time.NewTicker(5 * time.Minute)
	defer ticker.Stop()
	
	for range ticker.C {
		ab.localCache.mu.Lock()
		now := time.Now()
		for key, record := range ab.localCache.records {
			if now.After(record.resetAt) && (record.blockUntil == nil || now.After(*record.blockUntil)) {
				delete(ab.localCache.records, key)
			}
		}
		ab.localCache.mu.Unlock()
	}
}
