package cache

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
	"go.uber.org/zap"
)

// MiddlewareConfig 缓存中间件配置
type MiddlewareConfig struct {
	CacheManager    *Manager
	Logger          *zap.Logger
	DefaultLevel    CacheLevel
	DefaultTTL      time.Duration
	SkipPaths       []string
	KeyPrefix       string
	CacheHeader     string
	CacheStatusHeader string
}

// DefaultMiddlewareConfig 默认中间件配置
func DefaultMiddlewareConfig(manager *Manager, logger *zap.Logger) *MiddlewareConfig {
	return &MiddlewareConfig{
		CacheManager:      manager,
		Logger:            logger,
		DefaultLevel:      Both,
		DefaultTTL:        5 * time.Minute,
		SkipPaths:         []string{"/health", "/metrics", "/debug"},
		KeyPrefix:         "api:",
		CacheHeader:       "X-Cache",
		CacheStatusHeader: "X-Cache-Status",
	}
}

// Middleware 缓存中间件
func Middleware(config *MiddlewareConfig) gin.HandlerFunc {
	return func(c *gin.Context) {
		// 检查是否需要跳过缓存
		if shouldSkip(c, config.SkipPaths) {
			c.Next()
			return
		}

		// 只缓存 GET 请求
		if c.Request.Method != http.MethodGet {
			c.Next()
			return
		}

		// 生成缓存键
		cacheKey := generateCacheKey(c, config.KeyPrefix)

		// 尝试获取缓存
		var cachedResponse CachedResponse
		found, err := config.CacheManager.Get(c.Request.Context(), cacheKey, &cachedResponse)
		if err != nil {
			config.Logger.Warn("cache get failed", zap.Error(err))
		}

		if found {
			// 命中缓存
			c.Header(config.CacheHeader, "HIT")
			c.Header(config.CacheStatusHeader, cachedResponse.Status)
			c.Data(cachedResponse.StatusCode, cachedResponse.ContentType, cachedResponse.Body)
			c.Abort()
			return
		}
		
		// 未命中缓存，记录响应
		c.Header(config.CacheHeader, "MISS")
		blw := &bufferedResponseWriter{
			ResponseWriter: c.Writer,
			body:           &bytes.Buffer{},
		}
		c.Writer = blw

		c.Next()

		// 只缓存成功响应
		if c.Writer.Status() >= 200 && c.Writer.Status() < 300 {
			response := CachedResponse{
				StatusCode:  c.Writer.Status(),
				ContentType: c.Writer.Header().Get("Content-Type"),
				Body:        blw.body.Bytes(),
				Status:      http.StatusText(c.Writer.Status()),
			}

			// 写入缓存
			if err := config.CacheManager.Set(c.Request.Context(), cacheKey, response, config.DefaultLevel, config.DefaultTTL); err != nil {
				config.Logger.Warn("cache set failed", zap.Error(err))
			}
		}
	}
}

// CachedResponse 缓存的响应结构
type CachedResponse struct {
	StatusCode  int    `json:"status_code"`
	ContentType string `json:"content_type"`
	Body        []byte `json:"body"`
	Status      string `json:"status"`
}

// bufferedResponseWriter 缓冲响应写入器
type bufferedResponseWriter struct {
	gin.ResponseWriter
	body *bytes.Buffer
}

func (w *bufferedResponseWriter) Write(b []byte) (int, error) {
	w.body.Write(b)
	return w.ResponseWriter.Write(b)
}

// shouldSkip 检查是否需要跳过缓存
func shouldSkip(c *gin.Context, skipPaths []string) bool {
	path := c.Request.URL.Path
	for _, skipPath := range skipPaths {
		if strings.HasPrefix(path, skipPath) {
			return true
		}
	}

	// 检查是否有 No-Cache 头
	if c.GetHeader("Cache-Control") == "no-cache" {
		return true
	}

	return false
}

// generateCacheKey 生成缓存键
func generateCacheKey(c *gin.Context, prefix string) string {
	// 基础键：路径 + 查询参数
	key := fmt.Sprintf("%s%s?%s", prefix, c.Request.URL.Path, c.Request.URL.RawQuery)
	
	// 如果有用户标识，添加到键中
	if userID, exists := c.Get("user_id"); exists {
		key = fmt.Sprintf("%s:user:%v", key, userID)
	}

	return key
}

// InvalidateCacheMiddleware 缓存失效中间件
func InvalidateCacheMiddleware(manager *Manager, patterns []string) gin.HandlerFunc {
	return func(c *gin.Context) {
		// 等待请求处理完成
		c.Next()

		// 只有写操作才会触发缓存失效
		if c.Request.Method != http.MethodPost && 
		   c.Request.Method != http.MethodPut && 
		   c.Request.Method != http.MethodPatch && 
		   c.Request.Method != http.MethodDelete {
			return
		}

		// 检查响应是否成功
		if c.Writer.Status() < 200 || c.Writer.Status() >= 300 {
			return
		}

		// 执行缓存清除
		ctx := context.Background()
		for _, pattern := range patterns {
			if err := manager.DeleteByPattern(ctx, pattern); err != nil {
				// 记录日志但不中断请求
				continue
			}
		}
	}
}

// CacheControlMiddleware 缓存控制头中间件
func CacheControlMiddleware(maxAge time.Duration) gin.HandlerFunc {
	return func(c *gin.Context) {
		c.Next()

		// 只有成功响应才添加缓存控制头
		if c.Writer.Status() >= 200 && c.Writer.Status() < 300 {
			if c.Request.Method == http.MethodGet {
				c.Header("Cache-Control", fmt.Sprintf("public, max-age=%d", int(maxAge.Seconds())))
			} else {
				c.Header("Cache-Control", "no-store, no-cache, must-revalidate")
			}
		}
	}
}

// MemoryCacheOnly 只使用内存缓存的配置
func MemoryCacheOnly(manager *Manager, logger *zap.Logger, ttl time.Duration) *MiddlewareConfig {
	cfg := DefaultMiddlewareConfig(manager, logger)
	cfg.DefaultLevel = L1
	cfg.DefaultTTL = ttl
	return cfg
}

// RedisCacheOnly 只使用 Redis 缓存的配置
func RedisCacheOnly(manager *Manager, logger *zap.Logger, ttl time.Duration) *MiddlewareConfig {
	cfg := DefaultMiddlewareConfig(manager, logger)
	cfg.DefaultLevel = L2
	cfg.DefaultTTL = ttl
	return cfg
}

// DynamicCacheConfig 动态缓存配置
type DynamicCacheConfig struct {
	TTL   time.Duration
	Level CacheLevel
}

// DynamicMiddleware 动态缓存中间件，支持根据条件调整缓存策略
func DynamicMiddleware(config *MiddlewareConfig, configFn func(c *gin.Context) *DynamicCacheConfig) gin.HandlerFunc {
	return func(c *gin.Context) {
		if shouldSkip(c, config.SkipPaths) || c.Request.Method != http.MethodGet {
			c.Next()
			return
		}

		dynCfg := configFn(c)
		if dynCfg == nil {
			dynCfg = &DynamicCacheConfig{
				TTL:   config.DefaultTTL,
				Level: config.DefaultLevel,
			}
		}

		cacheKey := generateCacheKey(c, config.KeyPrefix)

		var cachedResponse CachedResponse
		found, err := config.CacheManager.Get(c.Request.Context(), cacheKey, &cachedResponse)
		if err != nil {
			config.Logger.Warn("cache get failed", zap.Error(err))
		}

		if found {
			c.Header(config.CacheHeader, "HIT")
			c.Header(config.CacheStatusHeader, cachedResponse.Status)
			c.Data(cachedResponse.StatusCode, cachedResponse.ContentType, cachedResponse.Body)
			c.Abort()
			return
		}

		c.Header(config.CacheHeader, "MISS")
		blw := &bufferedResponseWriter{
			ResponseWriter: c.Writer,
			body:           &bytes.Buffer{},
		}
		c.Writer = blw

		c.Next()

		if c.Writer.Status() >= 200 && c.Writer.Status() < 300 {
			response := CachedResponse{
				StatusCode:  c.Writer.Status(),
				ContentType: c.Writer.Header().Get("Content-Type"),
				Body:        blw.body.Bytes(),
				Status:      http.StatusText(c.Writer.Status()),
			}

			if err := config.CacheManager.Set(c.Request.Context(), cacheKey, response, dynCfg.Level, dynCfg.TTL); err != nil {
				config.Logger.Warn("cache set failed", zap.Error(err))
			}
		}
	}
}
