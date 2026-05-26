// Package integration 集成测试
// 使用 httptest.Server + mock 仓库测试 HTTP API
package integration

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"

	"github.com/frisky1985/yuleDKCS/backend/internal/config"
	"github.com/frisky1985/yuleDKCS/backend/internal/router"
)

// setupTestRouter 创建测试用的 HTTP 路由器
func setupTestRouter() *gin.Engine {
	gin.SetMode(gin.TestMode)
	r := gin.New()
	r.Use(gin.Recovery())

	// 使用最小配置
	cfg := &config.Config{
		Server: config.ServerConfig{
			Port: 8080,
		},
		JWT: config.JWTConfig{
			Secret: "test-secret",
		},
	}

	// 创建内存 SQLite 数据库用于测试
	// 注意: 需要在 go.mod 中添加 github.com/glebarez/sqlite 或使用标准 sqlite
	// 当前测试跳过需要 DB 的路由，仅测试无需 DB 的端点
	router.Setup(r, cfg, nil, nil)
	return r
}

// apiV1 拼接 /api/v1 路径
func apiV1(path string) string {
	return "/api/v1" + path
}

// TestHealthEndpoint 健康检查端点
func TestHealthEndpoint(t *testing.T) {
	r := setupTestRouter()

	t.Run("GET /health 返回200", func(t *testing.T) {
		w := httptest.NewRecorder()
		req, _ := http.NewRequest("GET", "/health", nil)
		r.ServeHTTP(w, req)

		assert.Equal(t, 200, w.Code)

		var resp map[string]interface{}
		json.Unmarshal(w.Body.Bytes(), &resp)
		assert.Contains(t, resp, "status")
		assert.NotEmpty(t, resp["status"])
	})

	t.Run("GET /health/live 返回200", func(t *testing.T) {
		w := httptest.NewRecorder()
		req, _ := http.NewRequest("GET", "/health/live", nil)
		r.ServeHTTP(w, req)

		assert.Equal(t, 200, w.Code)
	})
}

// TestPingEndpoint Ping 端点
func TestPingEndpoint(t *testing.T) {
	r := setupTestRouter()

	w := httptest.NewRecorder()
	req, _ := http.NewRequest("GET", apiV1("/ping"), nil)
	r.ServeHTTP(w, req)

	assert.Equal(t, 200, w.Code)

	var resp map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &resp)
	assert.Equal(t, float64(200), resp["code"])
	assert.Equal(t, "pong", resp["message"])
}

// TestNotFound 未定义路由返回404
func TestNotFound(t *testing.T) {
	r := setupTestRouter()

	w := httptest.NewRecorder()
	req, _ := http.NewRequest("GET", "/nonexistent", nil)
	r.ServeHTTP(w, req)

	assert.Equal(t, 404, w.Code)

	var resp map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &resp)
	assert.Contains(t, resp, "message")
}

// TestAuthEndpoint_MissingToken 无token访问受保护端点
func TestAuthEndpoint_MissingToken(t *testing.T) {
	r := setupTestRouter()

	w := httptest.NewRecorder()
	req, _ := http.NewRequest("GET", apiV1("/user/profile"), nil)
	r.ServeHTTP(w, req)

	// 无 token 应返回 401
	assert.Equal(t, 401, w.Code)
}

// TestAuthLogin_InvalidBody 登录参数校验
func TestAuthLogin_InvalidBody(t *testing.T) {
	r := setupTestRouter()

	w := httptest.NewRecorder()
	body := `{"username": "", "password": ""}`
	req, _ := http.NewRequest("POST", apiV1("/auth/login"), strings.NewReader(body))
	req.Header.Set("Content-Type", "application/json")
	r.ServeHTTP(w, req)

	// 空参数应返回 400
	assert.Equal(t, 400, w.Code)
}

// TestMetricsEndpoint Prometheus 指标
func TestMetricsEndpoint(t *testing.T) {
	r := setupTestRouter()

	w := httptest.NewRecorder()
	req, _ := http.NewRequest("GET", "/metrics", nil)
	r.ServeHTTP(w, req)

	assert.Equal(t, 200, w.Code)
}

// TestAuthRegisterEndpoint 注册端点存在性 (跳过，需要DB)
func TestAuthRegisterEndpoint(t *testing.T) {
	t.Skip("需要数据库连接")
}
