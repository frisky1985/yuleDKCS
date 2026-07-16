package diagnostics

import (
	"context"
	"database/sql"
	"fmt"
	"net/http"
	"sync"
	"time"

	"go.uber.org/zap"
)

// ── 健康检查 ───────────────────────────────────────────────────────────────
// 对标银基 Dting 服务健康模块：提供 /health 端点支持

// CheckFunc 健康检查函数类型
type CheckFunc func(ctx context.Context) HealthCheck

// HealthChecker 健康检查器
type HealthChecker struct {
	mu       sync.RWMutex
	checks   map[string]CheckFunc
	status   *HealthStatus
	interval time.Duration
	logger   *zap.Logger
	stopCh   chan struct{}
	started  bool
}

// NewHealthChecker 创建健康检查器
func NewHealthChecker(service string, version string, logger *zap.Logger) *HealthChecker {
	if logger == nil {
		logger = zap.NewNop()
	}
	return &HealthChecker{
		checks:   make(map[string]CheckFunc),
		interval: 30 * time.Second,
		logger:   logger,
		stopCh:   make(chan struct{}),
		status: &HealthStatus{
			Service:   service,
			Status:    "healthy",
			Version:   version,
			Checks:    []HealthCheck{},
			LastCheck: time.Now(),
		},
	}
}

// RegisterCheck 注册健康检查项
func (h *HealthChecker) RegisterCheck(name string, fn CheckFunc) {
	h.mu.Lock()
	defer h.mu.Unlock()
	h.checks[name] = fn
	h.logger.Info("health check registered", zap.String("name", name))
}

// RegisterDBCheck 注册数据库健康检查
func (h *HealthChecker) RegisterDBCheck(name string, db *sql.DB, timeout time.Duration) {
	h.RegisterCheck(name, func(ctx context.Context) HealthCheck {
		start := time.Now()
		pingCtx, cancel := context.WithTimeout(ctx, timeout)
		defer cancel()

		err := db.PingContext(pingCtx)
		latency := time.Since(start).Milliseconds()

		if err != nil {
			return HealthCheck{
				Name:    name,
				Status:  "fail",
				Message: fmt.Sprintf("database ping failed: %v", err),
				Latency: latency,
			}
		}
		return HealthCheck{
			Name:    name,
			Status:  "pass",
			Message: "database reachable",
			Latency: latency,
		}
	})
}

// RegisterHTTPCheck 注册外部服务 HTTP 健康检查
func (h *HealthChecker) RegisterHTTPCheck(name string, url string, timeout time.Duration) {
	h.RegisterCheck(name, func(ctx context.Context) HealthCheck {
		start := time.Now()
		reqCtx, cancel := context.WithTimeout(ctx, timeout)
		defer cancel()

		req, err := http.NewRequestWithContext(reqCtx, http.MethodGet, url, nil)
		if err != nil {
			return HealthCheck{
				Name:    name,
				Status:  "fail",
				Message: fmt.Sprintf("request creation failed: %v", err),
				Latency: time.Since(start).Milliseconds(),
			}
		}

		resp, err := http.DefaultClient.Do(req)
		latency := time.Since(start).Milliseconds()

		if err != nil {
			return HealthCheck{
				Name:    name,
				Status:  "fail",
				Message: fmt.Sprintf("http check failed: %v", err),
				Latency: latency,
			}
		}
		resp.Body.Close()

		if resp.StatusCode >= 500 {
			return HealthCheck{
				Name:    name,
				Status:  "fail",
				Message: fmt.Sprintf("service returned %d", resp.StatusCode),
				Latency: latency,
			}
		}

		return HealthCheck{
			Name:    name,
			Status:  "pass",
			Message: fmt.Sprintf("HTTP %d", resp.StatusCode),
			Latency: latency,
		}
	})
}

// RunChecks 执行所有已注册的健康检查
func (h *HealthChecker) RunChecks(ctx context.Context) *HealthStatus {
	h.mu.RLock()
	checkFuncs := make(map[string]CheckFunc)
	for k, v := range h.checks {
		checkFuncs[k] = v
	}
	h.mu.RUnlock()

	status := &HealthStatus{
		Service:   h.status.Service,
		Status:    "healthy",
		Version:   h.status.Version,
		Checks:    make([]HealthCheck, 0, len(checkFuncs)),
		LastCheck: time.Now(),
	}

	var allPass = true
	for name, fn := range checkFuncs {
		check := fn(ctx)
		status.Checks = append(status.Checks, check)
		if check.Status != "pass" {
			allPass = false
			h.logger.Warn("health check failed",
				zap.String("name", name),
				zap.String("message", check.Message),
			)
		}
	}

	if !allPass {
		// 如果全部失败才标记 unhealthy
		var failCount int
		for _, c := range status.Checks {
			if c.Status != "pass" {
				failCount++
			}
		}
		if failCount == len(status.Checks) {
			status.Status = "unhealthy"
		} else {
			status.Status = "degraded"
		}
	}

	// 更新缓存
	h.mu.Lock()
	h.status = status
	h.mu.Unlock()

	return status
}

// GetStatus 获取缓存的健康状态
func (h *HealthChecker) GetStatus() *HealthStatus {
	h.mu.RLock()
	defer h.mu.RUnlock()
	status := *h.status
	status.Checks = make([]HealthCheck, len(h.status.Checks))
	copy(status.Checks, h.status.Checks)
	return &status
}

// Start 启动周期性健康检查
func (h *HealthChecker) Start(ctx context.Context) {
	h.mu.Lock()
	if h.started {
		h.mu.Unlock()
		return
	}
	h.started = true
	h.mu.Unlock()

	go func() {
		ticker := time.NewTicker(h.interval)
		defer ticker.Stop()

		// 首次立即执行
		h.RunChecks(ctx)

		for {
			select {
			case <-ticker.C:
				h.RunChecks(ctx)
			case <-h.stopCh:
				h.logger.Info("health checker stopped")
				return
			case <-ctx.Done():
				return
			}
		}
	}()

	h.logger.Info("health checker started", zap.Duration("interval", h.interval))
}

// Stop 停止周期性健康检查
func (h *HealthChecker) Stop() {
	h.mu.Lock()
	defer h.mu.Unlock()
	if h.started {
		close(h.stopCh)
		h.started = false
	}
}

// HTTPHealthHandler 返回适配 HTTP handler 的健康检查回调
func (h *HealthChecker) HTTPHealthHandler() http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		status := h.RunChecks(r.Context())

		w.Header().Set("Content-Type", "application/json")
		if status.Status == "healthy" {
			w.WriteHeader(http.StatusOK)
		} else {
			w.WriteHeader(http.StatusServiceUnavailable)
		}

		// 简易 JSON 输出
		w.Write([]byte(`{"status":"` + status.Status + `","service":"` + status.Service + `"}`))
	}
}
