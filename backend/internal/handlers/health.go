package handlers

import (
	"net/http"
	"runtime"
	"time"

	"github.com/frisky1985/yuleDKCS/backend/internal/config"
	"github.com/gin-gonic/gin"
	"github.com/prometheus/client_golang/prometheus/promhttp"
)

// HealthHandler 健康检查处理器
type HealthHandler struct {
	cfg *config.Config
	startTime time.Time
}

// NewHealthHandler 创建健康检查处理器
func NewHealthHandler(cfg *config.Config) *HealthHandler {
	return &HealthHandler{
		cfg: cfg,
		startTime: time.Now(),
	}
}

// LivenessResponse 存活检查响应
type LivenessResponse struct {
	Status    string    `json:"status"`
	Timestamp time.Time `json:"timestamp"`
}

// ReadinessResponse 就绪检查响应
type ReadinessResponse struct {
	Status    string            `json:"status"`
	Checks    map[string]string `json:"checks"`
	Timestamp time.Time         `json:"timestamp"`
}

// SystemInfoResponse 系统信息响应
type SystemInfoResponse struct {
	Status      string        `json:"status"`
	Version     string        `json:"version"`
	GoVersion   string        `json:"go_version"`
	Uptime      string        `json:"uptime"`
	Goroutines  int           `json:"goroutines"`
	Memory      MemoryInfo    `json:"memory"`
	Timestamp   time.Time     `json:"timestamp"`
}

// MemoryInfo 内存信息
type MemoryInfo struct {
	Alloc      uint64 `json:"alloc_bytes"`
	TotalAlloc uint64 `json:"total_alloc_bytes"`
	Sys        uint64 `json:"sys_bytes"`
	NumGC      uint32 `json:"num_gc"`
}

// Liveness 存活检查 (Kubernetes livenessProbe)
// 检查应用程序是否运行，失败时重启容器
func (h *HealthHandler) Liveness(c *gin.Context) {
	c.JSON(http.StatusOK, LivenessResponse{
		Status:    "alive",
		Timestamp: time.Now(),
	})
}

// Readiness 就绪检查 (Kubernetes readinessProbe)
// 检查应用是否准备好接受流量
func (h *HealthHandler) Readiness(c *gin.Context) {
	checks := make(map[string]string)
	
	// 检查数据库连接
	// TODO: 实现真实的数据库健康检查
	checks["database"] = "ok"
	
	// 检查 Redis 连接
	// TODO: 实现真实的 Redis 健康检查
	checks["redis"] = "ok"
	
	// 检查配置加载
	if h.cfg == nil {
		checks["config"] = "error: not loaded"
	} else {
		checks["config"] = "ok"
	}
	
	// 确定整体状态
	status := http.StatusOK
	overallStatus := "ready"
	
	for _, check := range checks {
		if check != "ok" {
			status = http.StatusServiceUnavailable
			overallStatus = "not_ready"
			break
		}
	}
	
	c.JSON(status, ReadinessResponse{
		Status:    overallStatus,
		Checks:    checks,
		Timestamp: time.Now(),
	})
}

// SystemInfo 系统信息
func (h *HealthHandler) SystemInfo(c *gin.Context) {
	var m runtime.MemStats
	runtime.ReadMemStats(&m)
	
	c.JSON(http.StatusOK, SystemInfoResponse{
		Status:     "healthy",
		Version:    h.cfg.AppVersion,
		GoVersion:  runtime.Version(),
		Uptime:     time.Since(h.startTime).String(),
		Goroutines: runtime.NumGoroutine(),
		Memory: MemoryInfo{
			Alloc:      m.Alloc,
			TotalAlloc: m.TotalAlloc,
			Sys:        m.Sys,
			NumGC:      m.NumGC,
		},
		Timestamp: time.Now(),
	})
}

// Metrics Prometheus 指标端点
func (h *HealthHandler) Metrics() gin.HandlerFunc {
	return gin.WrapH(promhttp.Handler())
}

// RegisterRoutes 注册健康检查路由
func (h *HealthHandler) RegisterRoutes(r *gin.Engine) {
	// 健康检查 (无需认证)
	r.GET("/health/live", h.Liveness)
	r.GET("/health/ready", h.Readiness)
	r.GET("/health", h.Readiness) // 别名
	
	// 系统信息 (需认证)
	authorized := r.Group("/")
	// authorized.Use(middleware.AuthRequired()) // 如果需要认证可取消注释
	authorized.GET("/system/info", h.SystemInfo)
	
	// Prometheus 指标
	r.GET("/metrics", h.Metrics())
}
