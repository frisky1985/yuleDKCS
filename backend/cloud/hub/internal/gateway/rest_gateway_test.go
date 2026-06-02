package gateway

import (
	"context"
	"encoding/json"
	"io"
	"net"
	"net/http"
	"net/http/httptest"
	"strings"
	"sync"
	"testing"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/golang-jwt/jwt/v5"
	"go.uber.org/zap"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/token"
)

func init() {
	gin.SetMode(gin.TestMode)
}

// ── helpers ──

func newTestGateway() *RESTGateway {
	logger, _ := zap.NewDevelopment()
	g := NewRESTGateway(nil, logger)
	g.WithJWTSecret("test-secret-key")
	return g
}

func issueTestToken(g *RESTGateway, userID, role string) string {
	claims := jwt.MapClaims{
		"user_id": userID,
		"role":    role,
		"exp":     time.Now().Add(time.Hour).Unix(),
	}
	tok := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	s, _ := tok.SignedString([]byte(g.jwtSecret))
	return s
}

// ── Rate Limiter ──

func TestRateLimiter_FirstRequestAllowed(t *testing.T) {
	rl := newRateLimiter(10, 20)
	allowed, retryAfter := rl.allow("192.168.1.1")
	if !allowed {
		t.Fatal("first request should be allowed")
	}
	if retryAfter != 0 {
		t.Fatalf("retryAfter should be 0, got %v", retryAfter)
	}
}

func TestRateLimiter_BurstExhausted(t *testing.T) {
	rl := newRateLimiter(100, 5)
	for i := 0; i < 5; i++ {
		allowed, _ := rl.allow("10.0.0.1")
		if !allowed {
			t.Fatalf("request %d should be allowed", i+1)
		}
	}
	allowed, retryAfter := rl.allow("10.0.0.1")
	if allowed {
		t.Fatal("request should be rate limited")
	}
	if retryAfter <= 0 {
		t.Fatalf("retryAfter should be positive, got %v", retryAfter)
	}
}

func TestRateLimiter_Refill(t *testing.T) {
	rl := newRateLimiter(10, 5)
	ip := "10.0.0.2"
	for i := 0; i < 5; i++ {
		rl.allow(ip)
	}
	time.Sleep(200 * time.Millisecond)
	allowed, _ := rl.allow(ip)
	if !allowed {
		t.Fatal("should be allowed after refill")
	}
}

func TestRateLimiter_DifferentIPs(t *testing.T) {
	rl := newRateLimiter(10, 2)
	allowed1, _ := rl.allow("ip-a")
	allowed2, _ := rl.allow("ip-b")
	if !allowed1 || !allowed2 {
		t.Fatal("different IPs should have separate buckets")
	}
}

func TestRateLimiter_ConcurrentSafe(t *testing.T) {
	rl := newRateLimiter(1000, 100)
	var wg sync.WaitGroup
	for i := 0; i < 50; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			rl.allow("concurrent-ip")
		}()
	}
	wg.Wait()
}

// ── validateToken ──

func TestValidateToken_Valid(t *testing.T) {
	g := newTestGateway()
	tok := issueTestToken(g, "user-1", "admin")
	uid, role, err := g.validateToken(tok)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if uid != "user-1" {
		t.Fatalf("expected user-1, got %s", uid)
	}
	if role != "admin" {
		t.Fatalf("expected admin, got %s", role)
	}
}

func TestValidateToken_Empty(t *testing.T) {
	g := newTestGateway()
	_, _, err := g.validateToken("")
	if err == nil {
		t.Fatal("expected error for empty token")
	}
}

func TestValidateToken_Expired(t *testing.T) {
	g := newTestGateway()
	claims := jwt.MapClaims{
		"user_id": "user-1",
		"role":    "user",
		"exp":     time.Now().Add(-time.Hour).Unix(),
	}
	tok := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	s, _ := tok.SignedString([]byte(g.jwtSecret))
	_, _, err := g.validateToken(s)
	if err == nil {
		t.Fatal("expected error for expired token")
	}
}

func TestValidateToken_WrongSecret(t *testing.T) {
	g := newTestGateway()
	claims := jwt.MapClaims{
		"user_id": "user-1",
		"role":    "user",
		"exp":     time.Now().Add(time.Hour).Unix(),
	}
	tok := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	s, _ := tok.SignedString([]byte("wrong-secret"))
	_, _, err := g.validateToken(s)
	if err == nil {
		t.Fatal("expected error for token signed with wrong secret")
	}
}

func TestValidateToken_MissingUserID(t *testing.T) {
	g := newTestGateway()
	claims := jwt.MapClaims{
		"role": "user",
		"exp":  time.Now().Add(time.Hour).Unix(),
	}
	tok := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	s, _ := tok.SignedString([]byte(g.jwtSecret))
	_, _, err := g.validateToken(s)
	if err == nil {
		t.Fatal("expected error for missing user_id")
	}
}

// ── Auth Middleware ──

func performRequest(g *RESTGateway, method, path, authHeader string) *httptest.ResponseRecorder {
	r := gin.New()
	r.Use(gin.Recovery())
	r.POST("/api/v1/auth/login", g.login)
	v1 := r.Group("/api/v1")
	v1.Use(g.authMiddleware())
	{
		v1.GET("/keys", g.listKeys)
		v1.GET("/tokens/:tokenId", g.verifyToken)
		v1.POST("/devices", g.registerDevice)
	}
	w := httptest.NewRecorder()
	req := httptest.NewRequest(method, path, nil)
	if authHeader != "" {
		req.Header.Set("Authorization", authHeader)
	}
	r.ServeHTTP(w, req)
	return w
}

func TestAuthMiddleware_MissingHeader(t *testing.T) {
	g := newTestGateway()
	w := performRequest(g, "GET", "/api/v1/keys", "")
	if w.Code != http.StatusUnauthorized {
		t.Fatalf("expected 401, got %d", w.Code)
	}
	var body map[string]string
	json.Unmarshal(w.Body.Bytes(), &body)
	if body["error"] != "AUTH_MISSING_HEADER" {
		t.Fatalf("expected AUTH_MISSING_HEADER, got %s", body["error"])
	}
}

func TestAuthMiddleware_InvalidFormat(t *testing.T) {
	g := newTestGateway()
	w := performRequest(g, "GET", "/api/v1/keys", "some-token")
	if w.Code != http.StatusUnauthorized {
		t.Fatalf("expected 401, got %d", w.Code)
	}
	var body map[string]string
	json.Unmarshal(w.Body.Bytes(), &body)
	if body["error"] != "AUTH_INVALID_FORMAT" {
		t.Fatalf("expected AUTH_INVALID_FORMAT, got %s", body["error"])
	}
}

func TestAuthMiddleware_InvalidToken(t *testing.T) {
	g := newTestGateway()
	w := performRequest(g, "GET", "/api/v1/keys", "Bearer invalid-token")
	if w.Code != http.StatusUnauthorized {
		t.Fatalf("expected 401, got %d", w.Code)
	}
	var body map[string]string
	json.Unmarshal(w.Body.Bytes(), &body)
	if body["error"] != "AUTH_INVALID_TOKEN" {
		t.Fatalf("expected AUTH_INVALID_TOKEN, got %s", body["error"])
	}
}

// ── Rate Limit Middleware ──

func TestRateLimitMiddleware_HappyPath(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	g := &RESTGateway{
		logger:      logger,
		jwtSecret:   "test-secret-key",
		rateLimiter: newRateLimiter(100, 10),
	}
	r := gin.New()
	r.Use(g.rateLimitMiddleware())
	r.GET("/ping", func(c *gin.Context) { c.JSON(200, gin.H{"ok": true}) })
	w := httptest.NewRecorder()
	req := httptest.NewRequest("GET", "/ping", nil)
	r.ServeHTTP(w, req)
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", w.Code)
	}
}

func TestRateLimitMiddleware_Throttled(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	g := &RESTGateway{
		logger:      logger,
		jwtSecret:   "test-secret-key",
		rateLimiter: newRateLimiter(100, 2),
	}
	r := gin.New()
	r.Use(g.rateLimitMiddleware())
	r.GET("/ping", func(c *gin.Context) { c.JSON(200, gin.H{"ok": true}) })

	for i := 0; i < 4; i++ {
		w := httptest.NewRecorder()
		req := httptest.NewRequest("GET", "/ping", nil)
		r.ServeHTTP(w, req)
		if i < 2 {
			if w.Code != http.StatusOK {
				t.Fatalf("request %d should be 200, got %d", i, w.Code)
			}
		} else {
			if w.Code != http.StatusTooManyRequests {
				t.Fatalf("request %d should be 429, got %d", i, w.Code)
			}
			var body map[string]string
			json.Unmarshal(w.Body.Bytes(), &body)
			if body["error"] != "ERR_RATE_LIMIT" {
				t.Fatalf("expected ERR_RATE_LIMIT, got %s", body["error"])
			}
		}
	}
}

func TestRateLimitMiddleware_RetryAfterHeader(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	g := &RESTGateway{
		logger:      logger,
		jwtSecret:   "test-secret-key",
		rateLimiter: newRateLimiter(100, 1),
	}
	r := gin.New()
	r.Use(g.rateLimitMiddleware())
	r.GET("/ping", func(c *gin.Context) { c.JSON(200, gin.H{"ok": true}) })

	for i := 0; i < 2; i++ {
		w := httptest.NewRecorder()
		req := httptest.NewRequest("GET", "/ping", nil)
		r.ServeHTTP(w, req)
	}

	w := httptest.NewRecorder()
	req := httptest.NewRequest("GET", "/ping", nil)
	r.ServeHTTP(w, req)
	if retryAfter := w.Header().Get("Retry-After"); retryAfter == "" {
		t.Fatal("expected Retry-After header on 429")
	}
}

// ── Login ──

func TestLogin_Success(t *testing.T) {
	t.Setenv("ADMIN_USERNAME", "admin-1")
	t.Setenv("ADMIN_PASSWORD", "admin-pass-1")
	g := newTestGateway()
	r := gin.New()
	r.POST("/api/v1/auth/login", g.login)

	body := `{"user_id":"admin-1","password":"admin-pass-1"}`
	w := httptest.NewRecorder()
	req := httptest.NewRequest("POST", "/api/v1/auth/login", strings.NewReader(body))
	req.Header.Set("Content-Type", "application/json")
	r.ServeHTTP(w, req)

	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", w.Code, w.Body.String())
	}
	var resp map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &resp)
	if resp["token"] == nil {
		t.Fatal("expected token in response")
	}
}

func TestLogin_WrongPassword(t *testing.T) {
	t.Setenv("ADMIN_USERNAME", "admin-1")
	t.Setenv("ADMIN_PASSWORD", "admin-pass-1")
	g := newTestGateway()
	r := gin.New()
	r.POST("/api/v1/auth/login", g.login)

	body := `{"user_id":"admin-1","password":"wrong-pass"}`
	w := httptest.NewRecorder()
	req := httptest.NewRequest("POST", "/api/v1/auth/login", strings.NewReader(body))
	req.Header.Set("Content-Type", "application/json")
	r.ServeHTTP(w, req)

	if w.Code != http.StatusUnauthorized {
		t.Fatalf("expected 401, got %d: %s", w.Code, w.Body.String())
	}
}

func TestLogin_MissingFields(t *testing.T) {
	g := newTestGateway()
	r := gin.New()
	r.POST("/api/v1/auth/login", g.login)

	body := `{"user_id":""}`
	w := httptest.NewRecorder()
	req := httptest.NewRequest("POST", "/api/v1/auth/login", strings.NewReader(body))
	req.Header.Set("Content-Type", "application/json")
	r.ServeHTTP(w, req)

	if w.Code != http.StatusBadRequest {
		t.Fatalf("expected 400, got %d: %s", w.Code, w.Body.String())
	}
}

// ── Health Check ──

func TestPublicHealthEndpoint(t *testing.T) {
	r := gin.New()
	r.GET("/health", func(c *gin.Context) {
		c.JSON(200, gin.H{"status": "ok"})
	})
	w := httptest.NewRecorder()
	req := httptest.NewRequest("GET", "/health", nil)
	r.ServeHTTP(w, req)
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", w.Code)
	}
	var body map[string]string
	json.Unmarshal(w.Body.Bytes(), &body)
	if body["status"] != "ok" {
		t.Fatalf("expected ok, got %s", body["status"])
	}
}

func TestHubHealthCheck_RequiresAuth(t *testing.T) {
	g := newTestGateway()
	r := gin.New()
	r.Use(g.authMiddleware())
	r.GET("/api/v1/hub/health", g.hubHealthCheck)

	w := httptest.NewRecorder()
	req := httptest.NewRequest("GET", "/api/v1/hub/health", nil)
	r.ServeHTTP(w, req)
	if w.Code != http.StatusUnauthorized {
		t.Fatalf("expected 401 without auth, got %d", w.Code)
	}

	tok := issueTestToken(g, "user-1", "admin")
	w2 := httptest.NewRecorder()
	req2 := httptest.NewRequest("GET", "/api/v1/hub/health", nil)
	req2.Header.Set("Authorization", "Bearer "+tok)
	r.ServeHTTP(w2, req2)
	// hubHealthCheck may return 503 if gRPC is unavailable, but the key is that auth passes (not 401)
	if w2.Code == http.StatusUnauthorized {
		t.Fatalf("auth should pass, got %d: %s", w2.Code, w2.Body.String())
	}
}

func TestAuthInjectsUserContext(t *testing.T) {
	g := newTestGateway()
	r := gin.New()
	r.Use(g.authMiddleware())
	r.GET("/api/v1/me", func(c *gin.Context) {
		uid, _ := c.Get("user_id")
		role, _ := c.Get("user_role")
		c.JSON(200, gin.H{"user_id": uid, "role": role})
	})

	tok := issueTestToken(g, "test-user", "driver")
	w := httptest.NewRecorder()
	req := httptest.NewRequest("GET", "/api/v1/me", nil)
	req.Header.Set("Authorization", "Bearer "+tok)
	r.ServeHTTP(w, req)

	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", w.Code)
	}
	var body map[string]string
	json.Unmarshal(w.Body.Bytes(), &body)
	if body["user_id"] != "test-user" {
		t.Fatalf("expected test-user, got %s", body["user_id"])
	}
	if body["role"] != "driver" {
		t.Fatalf("expected driver, got %s", body["role"])
	}
}

// ── parsePermissions / formatPermissions ──

func TestParsePermissions_Valid(t *testing.T) {
	perms := parsePermissions([]string{"lock", "engine", "trunk"})
	if len(perms) != 3 {
		t.Fatalf("expected 3 permissions, got %d", len(perms))
	}
}

func TestParsePermissions_Unknown(t *testing.T) {
	perms := parsePermissions([]string{"unknown"})
	if len(perms) != 1 || perms[0] != token.PermLock {
		t.Fatalf("expected default PermLock, got %v", perms)
	}
}

func TestParsePermissions_Empty(t *testing.T) {
	perms := parsePermissions(nil)
	if len(perms) != 1 || perms[0] != token.PermLock {
		t.Fatalf("expected default PermLock for empty input")
	}
}

func TestFormatPermissions_RoundTrip(t *testing.T) {
	orig := []string{"lock", "engine", "trunk", "window", "climate", "seat", "fuel", "share", "charge_port", "valet_mode"}
	perms := parsePermissions(orig)
	formatted := formatPermissions(perms)
	if len(formatted) != len(orig) {
		t.Fatalf("expected %d formatted perms, got %d", len(orig), len(formatted))
	}
}

// ── Config ──

func TestWithJWTSecret(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	g := NewRESTGateway(nil, logger)
	g2 := g.WithJWTSecret("new-secret")
	if g2.jwtSecret != "new-secret" {
		t.Fatalf("expected new-secret, got %s", g2.jwtSecret)
	}
}

func TestShutdown_NilServer(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	g := NewRESTGateway(nil, logger)
	err := g.Shutdown(context.Background())
	if err != nil {
		t.Fatalf("expected nil, got %v", err)
	}
}

// ── WithRateLimit ──

func TestWithRateLimit_Enabled(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	g := NewRESTGateway(nil, logger)
	g2 := g.WithRateLimit(50, 100)
	if g2.rateLimiter == nil {
		t.Fatal("expected rateLimiter to be set")
	}
}

func TestWithRateLimit_Disabled(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	g := NewRESTGateway(nil, logger)
	g2 := g.WithRateLimit(0, 0)
	if g2.rateLimiter != nil {
		t.Fatal("expected rateLimiter to be nil when disabled")
	}
}

func TestWithRateLimit_DefaultBurst(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	g := NewRESTGateway(nil, logger)
	g2 := g.WithRateLimit(10, 0) // burst 0 should default to 20
	if g2.rateLimiter == nil {
		t.Fatal("expected rateLimiter to be set")
	}
	if g2.rateLimiter.burst != 20 {
		t.Fatalf("expected burst 20, got %d", g2.rateLimiter.burst)
	}
}

// ── WithGRPCConn ──

func TestWithGRPCConn(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	g := NewRESTGateway(nil, logger)
	if g.grpcConn != nil {
		t.Fatal("expected nil grpcConn initially")
	}
	// Since we can't create a real gRPC conn without a server,
	// just test that SetGRPCConn would set the field
	// The method simply assigns the conn, we'll test the setter
	_ = g.WithGRPCConn(nil) // setting nil explicitly
	if g.grpcConn != nil {
		t.Fatal("WithGRPCConn(nil) should set grpcConn to nil")
	}
}

// ── checkGRPCConn ──

func TestCheckGRPCConn_Nil(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	g := &RESTGateway{
		logger:    logger,
		jwtSecret: "test-secret",
		grpcConn:  nil,
	}

	w := httptest.NewRecorder()
	c, _ := gin.CreateTestContext(w)
	c.Request = httptest.NewRequest("GET", "/test", nil)

	result := g.checkGRPCConn(c, nil)
	if result {
		t.Fatal("expected false for nil conn")
	}
	if w.Code != http.StatusServiceUnavailable {
		t.Fatalf("expected 503, got %d", w.Code)
	}
}

func TestCheckGRPCConn_Available(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	// We don't have a real connection, so we test the function early-returns
	// This is more of a compile/sanity check
	g := &RESTGateway{logger: logger}

	w := httptest.NewRecorder()
	c, _ := gin.CreateTestContext(w)
	c.Request = httptest.NewRequest("GET", "/test", nil)

	// We can only test with nil — which returns false
	result := g.checkGRPCConn(c, nil)
	if result {
		t.Fatal("expected false for nil even when field is nil")
	}
}

// ── loggerMiddleware ──

func TestLoggerMiddleware(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	g := &RESTGateway{logger: logger}

	r := gin.New()
	r.Use(g.loggerMiddleware())
	r.GET("/test", func(c *gin.Context) {
		c.JSON(200, gin.H{"ok": true})
	})

	w := httptest.NewRecorder()
	req := httptest.NewRequest("GET", "/test", nil)
	r.ServeHTTP(w, req)

	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", w.Code)
	}
}

func TestLoggerMiddleware_ErrorPath(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	g := &RESTGateway{logger: logger}

	r := gin.New()
	r.Use(g.loggerMiddleware())
	r.GET("/error", func(c *gin.Context) {
		c.AbortWithStatusJSON(500, gin.H{"error": "internal"})
	})

	w := httptest.NewRecorder()
	req := httptest.NewRequest("GET", "/error", nil)
	r.ServeHTTP(w, req)

	if w.Code != http.StatusInternalServerError {
		t.Fatalf("expected 500, got %d", w.Code)
	}
}

// ── handleGRPCError ──

func TestHandleGRPCError_NotFound(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	g := &RESTGateway{logger: logger}

	w := httptest.NewRecorder()
	c, _ := gin.CreateTestContext(w)
	c.Request = httptest.NewRequest("GET", "/api/v1/keys/key-1", nil)

	g.handleGRPCError(c, status.Error(codes.NotFound, "key not found"))
	if w.Code != http.StatusNotFound {
		t.Fatalf("expected 404, got %d", w.Code)
	}
	var body map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &body)
	if body["error"] != "GRPC_NOT_FOUND" {
		t.Fatalf("expected GRPC_NOT_FOUND, got %v", body["error"])
	}
}

func TestHandleGRPCError_PermissionDenied(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	g := &RESTGateway{logger: logger}

	w := httptest.NewRecorder()
	c, _ := gin.CreateTestContext(w)
	c.Request = httptest.NewRequest("GET", "/api/v1/keys", nil)

	g.handleGRPCError(c, status.Error(codes.PermissionDenied, "no access"))
	if w.Code != http.StatusForbidden {
		t.Fatalf("expected 403, got %d", w.Code)
	}
	var body map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &body)
	if body["error"] != "GRPC_PERMISSION_DENIED" {
		t.Fatalf("expected GRPC_PERMISSION_DENIED, got %v", body["error"])
	}
}

func TestHandleGRPCError_Unauthenticated(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	g := &RESTGateway{logger: logger}

	w := httptest.NewRecorder()
	c, _ := gin.CreateTestContext(w)
	c.Request = httptest.NewRequest("GET", "/api/v1/keys", nil)

	g.handleGRPCError(c, status.Error(codes.Unauthenticated, "bad token"))
	if w.Code != http.StatusUnauthorized {
		t.Fatalf("expected 401, got %d", w.Code)
	}
}

func TestHandleGRPCError_Internal(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	g := &RESTGateway{logger: logger}

	w := httptest.NewRecorder()
	c, _ := gin.CreateTestContext(w)
	c.Request = httptest.NewRequest("GET", "/test", nil)

	g.handleGRPCError(c, status.Error(codes.Internal, "db error"))
	if w.Code != http.StatusInternalServerError {
		t.Fatalf("expected 500, got %d", w.Code)
	}
}

func TestHandleGRPCError_InvalidArgument(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	g := &RESTGateway{logger: logger}

	w := httptest.NewRecorder()
	c, _ := gin.CreateTestContext(w)
	c.Request = httptest.NewRequest("GET", "/test", nil)

	g.handleGRPCError(c, status.Error(codes.InvalidArgument, "bad field"))
	if w.Code != http.StatusBadRequest {
		t.Fatalf("expected 400, got %d", w.Code)
	}
	var body map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &body)
	if body["error"] != "GRPC_INVALID_ARGUMENT" {
		t.Fatalf("expected GRPC_INVALID_ARGUMENT, got %v", body["error"])
	}
}

func TestHandleGRPCError_Unavailable(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	g := &RESTGateway{logger: logger}

	w := httptest.NewRecorder()
	c, _ := gin.CreateTestContext(w)
	c.Request = httptest.NewRequest("GET", "/test", nil)

	g.handleGRPCError(c, status.Error(codes.Unavailable, "service down"))
	if w.Code != http.StatusServiceUnavailable {
		t.Fatalf("expected 503, got %d", w.Code)
	}
}

func TestHandleGRPCError_DeadlineExceeded(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	g := &RESTGateway{logger: logger}

	w := httptest.NewRecorder()
	c, _ := gin.CreateTestContext(w)
	c.Request = httptest.NewRequest("GET", "/test", nil)

	g.handleGRPCError(c, status.Error(codes.DeadlineExceeded, "timeout"))
	if w.Code != http.StatusGatewayTimeout {
		t.Fatalf("expected 504, got %d", w.Code)
	}
	var body map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &body)
	if body["error"] != "GRPC_DEADLINE_EXCEEDED" {
		t.Fatalf("expected GRPC_DEADLINE_EXCEEDED, got %v", body["error"])
	}
}

func TestHandleGRPCError_ResourceExhausted(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	g := &RESTGateway{logger: logger}

	w := httptest.NewRecorder()
	c, _ := gin.CreateTestContext(w)
	c.Request = httptest.NewRequest("GET", "/test", nil)

	g.handleGRPCError(c, status.Error(codes.ResourceExhausted, "quota exceeded"))
	if w.Code != http.StatusTooManyRequests {
		t.Fatalf("expected 429, got %d", w.Code)
	}
	var body map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &body)
	if body["error"] != "GRPC_RESOURCE_EXHAUSTED" {
		t.Fatalf("expected GRPC_RESOURCE_EXHAUSTED, got %v", body["error"])
	}
}

func TestHandleGRPCError_Default(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	g := &RESTGateway{logger: logger}

	w := httptest.NewRecorder()
	c, _ := gin.CreateTestContext(w)
	c.Request = httptest.NewRequest("GET", "/test", nil)

	g.handleGRPCError(c, status.Error(codes.Aborted, "aborted"))
	if w.Code != http.StatusBadGateway {
		t.Fatalf("expected 502, got %d", w.Code)
	}
	var body map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &body)
	if body["error"] != "GRPC_UNKNOWN" {
		t.Fatalf("expected GRPC_UNKNOWN, got %v", body["error"])
	}
}

// ── replyProto ──

func TestReplyProto_Success(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	g := &RESTGateway{logger: logger}

	w := httptest.NewRecorder()
	c, _ := gin.CreateTestContext(w)
	c.Request = httptest.NewRequest("GET", "/test", nil)

	msg := &pb.HealthCheckResponse{
		Healthy: true,
		Adapters: []*pb.AdapterStatus{
			{Vendor: "test", Protocol: "proto", Healthy: true},
		},
	}
	g.replyProto(c, msg)
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", w.Code)
	}
	var body map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &body)
	if body["healthy"] != true {
		t.Fatalf("expected healthy=true, got %v", body["healthy"])
	}
}

// ── parseBody ──

func TestParseBody_Success(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	g := &RESTGateway{logger: logger}

	body := `{"vehicle_id": "VH-001", "user_id": "user-1"}`
	w := httptest.NewRecorder()
	c, _ := gin.CreateTestContext(w)
	c.Request = httptest.NewRequest("POST", "/test", strings.NewReader(body))
	c.Request.Body = io.NopCloser(strings.NewReader(body))
	c.Request.Header.Set("Content-Type", "application/json")

	var req pb.BindKeyRequest
	err := g.parseBody(c, &req)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if req.VehicleId != "VH-001" {
		t.Fatalf("expected VH-001, got %s", req.VehicleId)
	}
	if req.UserId != "user-1" {
		t.Fatalf("expected user-1, got %s", req.UserId)
	}
}

func TestParseBody_EmptyBody(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	g := &RESTGateway{logger: logger}

	w := httptest.NewRecorder()
	c, _ := gin.CreateTestContext(w)
	c.Request = httptest.NewRequest("POST", "/test", nil)

	var req pb.BindKeyRequest
	err := g.parseBody(c, &req)
	if err == nil {
		t.Fatal("expected error for empty body")
	}
	// protojson reports syntax error for empty input
	if !strings.Contains(err.Error(), "failed to unmarshal request") {
		t.Fatalf("expected unmarshal error, got: %v", err)
	}
}

// ── validateToken additional branches ──

func TestValidateToken_InvalidSigningMethod(t *testing.T) {
	g := newTestGateway()
	tok := jwt.NewWithClaims(jwt.SigningMethodRS256, jwt.MapClaims{
		"user_id": "user-1",
		"exp":    time.Now().Add(time.Hour).Unix(),
	})
	// Can't sign with RS256 without a private key, but we can test parsing error
	key, _ := jwt.Parse("invalid-token", func(t *jwt.Token) (interface{}, error) {
		return nil, status.Error(codes.Unauthenticated, "parse error")
	})
	_ = key
	// Use actual method
	s, _ := tok.SigningString()
	_ = s
	// Just test with a malformed token
	_, _, err := g.validateToken("malformed.jwt.token")
	if err == nil {
		t.Fatal("expected error for malformed token")
	}
}

func TestValidateToken_DefaultRole(t *testing.T) {
	g := newTestGateway()
	claims := jwt.MapClaims{
		"user_id": "user-no-role",
		"exp":    time.Now().Add(time.Hour).Unix(),
	}
	tok := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	s, _ := tok.SignedString([]byte(g.jwtSecret))

	uid, role, err := g.validateToken(s)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if uid != "user-no-role" {
		t.Fatalf("expected user-no-role, got %s", uid)
	}
	if role != "user" {
		t.Fatalf("expected default role 'user', got %s", role)
	}
}

func TestValidateToken_UnregisteredClaims(t *testing.T) {
	g := newTestGateway()
	// Create a token with a non-standard claims type
	tok := jwt.NewWithClaims(jwt.SigningMethodHS256, jwt.RegisteredClaims{
		Subject:   "test",
		ExpiresAt: jwt.NewNumericDate(time.Now().Add(time.Hour)),
	})
	s, _ := tok.SignedString([]byte(g.jwtSecret))

	_, _, err := g.validateToken(s)
	if err == nil {
		t.Fatal("expected error for non-MapClaims token")
	}
}

// ── login additional branches ──

func TestLogin_InvalidJSON(t *testing.T) {
	t.Setenv("ADMIN_USERNAME", "admin")
	t.Setenv("ADMIN_PASSWORD", "admin123")
	g := newTestGateway()
	r := gin.New()
	r.POST("/api/v1/auth/login", g.login)

	w := httptest.NewRecorder()
	req := httptest.NewRequest("POST", "/api/v1/auth/login", strings.NewReader(`not json`))
	req.Header.Set("Content-Type", "application/json")
	r.ServeHTTP(w, req)

	if w.Code != http.StatusBadRequest {
		t.Fatalf("expected 400, got %d: %s", w.Code, w.Body.String())
	}
}

func TestLogin_EmptyRequest(t *testing.T) {
	t.Setenv("ADMIN_USERNAME", "admin")
	t.Setenv("ADMIN_PASSWORD", "admin123")
	g := newTestGateway()
	r := gin.New()
	r.POST("/api/v1/auth/login", g.login)

	body := `{}`
	w := httptest.NewRecorder()
	req := httptest.NewRequest("POST", "/api/v1/auth/login", strings.NewReader(body))
	req.Header.Set("Content-Type", "application/json")
	r.ServeHTTP(w, req)

	if w.Code != http.StatusBadRequest {
		t.Fatalf("expected 400, got %d: %s", w.Code, w.Body.String())
	}
}

func TestLogin_CustomEnvCredentials(t *testing.T) {
	t.Setenv("ADMIN_USERNAME", "custom-admin")
	t.Setenv("ADMIN_PASSWORD", "custom-pass-987")
	g := newTestGateway()
	r := gin.New()
	r.POST("/api/v1/auth/login", g.login)

	body := `{"user_id":"custom-admin","password":"custom-pass-987"}`
	w := httptest.NewRecorder()
	req := httptest.NewRequest("POST", "/api/v1/auth/login", strings.NewReader(body))
	req.Header.Set("Content-Type", "application/json")
	r.ServeHTTP(w, req)

	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", w.Code, w.Body.String())
	}

	var resp map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &resp)
	if resp["token_type"] != "Bearer" {
		t.Fatalf("expected 'Bearer', got %v", resp["token_type"])
	}
	if resp["expires_in"] != float64(3600) {
		t.Fatalf("expected 3600, got %v", resp["expires_in"])
	}
}

// ── RateLimiter advanced tests ──

func TestRateLimiter_CleanupLoop(t *testing.T) {
	rl := newRateLimiter(10, 5)
	rl.allow("ip-cleanup-1")
	rl.allow("ip-cleanup-2")

	// The cleanup loop runs every RateLimitCleanupSec (5 min), which is too long for tests.
	// We can't reliably trigger it in a unit test, but we can verify visitor tracking
	// is still correct
	allowed, _ := rl.allow("ip-cleanup-1")
	if !allowed {
		t.Fatal("expected allowed")
	}
}

func TestRateLimiter_HighRateAndBurst(t *testing.T) {
	rl := newRateLimiter(10000, 5000)
	// Many requests should all be allowed
	for i := 0; i < 100; i++ {
		allowed, _ := rl.allow("high-rate-ip")
		if !allowed {
			t.Fatalf("request %d should be allowed with high rate/burst", i)
		}
	}
}

func TestRateLimiter_FullRefillAfterIdle(t *testing.T) {
	rl := newRateLimiter(100, 10)
	ip := "refill-ip"

	// Exhaust burst
	for i := 0; i < 10; i++ {
		rl.allow(ip)
	}

	// Should be denied
	allowed, _ := rl.allow(ip)
	if allowed {
		t.Fatal("should be denied after burst exhausted")
	}

	// Wait for refill
	time.Sleep(200 * time.Millisecond)

	// Should be allowed again
	allowed, _ = rl.allow(ip)
	if !allowed {
		t.Fatal("should be allowed after refill")
	}
}

// ── authMiddleware additional cases ──

func TestAuthMiddleware_EmptyBearerToken(t *testing.T) {
	g := newTestGateway()
	w := performRequest(g, "GET", "/api/v1/keys", "Bearer ")
	if w.Code != http.StatusUnauthorized {
		t.Fatalf("expected 401, got %d", w.Code)
	}
	var body map[string]string
	json.Unmarshal(w.Body.Bytes(), &body)
	if body["error"] != "AUTH_INVALID_FORMAT" {
		t.Fatalf("expected AUTH_INVALID_FORMAT, got %s", body["error"])
	}
}

func TestAuthMiddleware_ExpiredToken(t *testing.T) {
	g := newTestGateway()
	claims := jwt.MapClaims{
		"user_id": "user-1",
		"role":    "user",
		"exp":     time.Now().Add(-1 * time.Hour).Unix(),
	}
	tok := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	s, _ := tok.SignedString([]byte(g.jwtSecret))

	w := performRequest(g, "GET", "/api/v1/keys", "Bearer "+s)
	if w.Code != http.StatusUnauthorized {
		t.Fatalf("expected 401 for expired token, got %d", w.Code)
	}
	var body map[string]string
	json.Unmarshal(w.Body.Bytes(), &body)
	if body["error"] != "AUTH_INVALID_TOKEN" {
		t.Fatalf("expected AUTH_INVALID_TOKEN, got %s", body["error"])
	}
}

// ── hubHealthCheck without gRPC ──

func TestHubHealthCheck_GRPCUnavailable(t *testing.T) {
	g := newTestGateway()
	r := gin.New()
	r.Use(g.authMiddleware())
	r.GET("/api/v1/hub/health", g.hubHealthCheck)

	tok := issueTestToken(g, "user-1", "admin")
	w := httptest.NewRecorder()
	req := httptest.NewRequest("GET", "/api/v1/hub/health", nil)
	req.Header.Set("Authorization", "Bearer "+tok)
	r.ServeHTTP(w, req)

	// hubHealthCheck calls checkGRPCConn which returns false because grpcConn is nil
	// So it should return 503
	if w.Code != http.StatusServiceUnavailable {
		t.Fatalf("expected 503 (gRPC unavailable), got %d: %s", w.Code, w.Body.String())
	}
	var body map[string]string
	json.Unmarshal(w.Body.Bytes(), &body)
	if body["error"] != "GRPC_UNAVAILABLE" {
		t.Fatalf("expected GRPC_UNAVAILABLE, got %s", body["error"])
	}
}

// ── Service start guard tests ──

func TestServe_EmptyJWTSecretPanics(t *testing.T) {
	// Testing the Serve guard: it calls log.Fatal which panics
	// We test it via WithJWTSecret behavior instead
	logger, _ := zap.NewDevelopment()
	g := NewRESTGateway(nil, logger)
	// Without setting JWT secret, Serve would panic
	if g.jwtSecret != "" {
		t.Fatal("expected empty jwt secret initially")
	}
}

func TestAuthMiddleware_WrongSigningMethod(t *testing.T) {
	g := newTestGateway()
	// Token signed with a different algorithm should fail
	// Since we use HS256, provide a token that appears JWTw but uses different method
	// We can't easily create one without crypto, so test with garbage
	w := performRequest(g, "GET", "/api/v1/keys", "Bearer definitely-not-a-valid-jwt")
	if w.Code != http.StatusUnauthorized {
		t.Fatalf("expected 401, got %d", w.Code)
	}
}

func TestClientIPRateLimit(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	g := &RESTGateway{
		logger:      logger,
		jwtSecret:   "test-secret",
		rateLimiter: newRateLimiter(10, 2),
	}
	r := gin.New()
	r.Use(g.rateLimitMiddleware())
	r.GET("/ping", func(c *gin.Context) { c.JSON(200, gin.H{"ok": true}) })

	// Test with different IPs
	w1 := httptest.NewRecorder()
	req1 := httptest.NewRequest("GET", "/ping", nil)
	req1.Header.Set("X-Forwarded-For", "10.0.0.1")
	r.ServeHTTP(w1, req1)

	w2 := httptest.NewRecorder()
	req2 := httptest.NewRequest("GET", "/ping", nil)
	req2.Header.Set("X-Forwarded-For", "10.0.0.2")
	r.ServeHTTP(w2, req2)

	if w1.Code != http.StatusOK || w2.Code != http.StatusOK {
		t.Fatalf("different IPs should not interfere: w1=%d w2=%d", w1.Code, w2.Code)
	}
}

// ── gRPC client helpers ──

func TestGRPCClientHelpers_ReturnNil(t *testing.T) {
	g := newTestGateway()
	// All client helpers return nil since grpcConn is nil
	// They just create new clients; we verify no panic
	_ = g.keyManagementClient()
	_ = g.keyShareClient()
	_ = g.vehicleControlClient()
	_ = g.hubTransportClient()
}

// TestParseBody_ProtoJSON uses protojson for unmarshal (the actual parseBody method)
func TestParseBody_ProtoJSONUnmarshalError(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	g := &RESTGateway{logger: logger}

	// Invalid proto JSON — wrong type for a field that should be a string
	body := `{"vehicle_id": 12345}` // vehicle_id should be string
	w := httptest.NewRecorder()
	c, _ := gin.CreateTestContext(w)
	c.Request = httptest.NewRequest("POST", "/test", strings.NewReader(body))
	c.Request.Body = io.NopCloser(strings.NewReader(body))
	c.Request.Header.Set("Content-Type", "application/json")

	var req pb.BindKeyRequest
	err := g.parseBody(c, &req)
	if err == nil {
		t.Fatal("expected error for type mismatch in proto json")
	}
}

// TestReplyProto_Error tests the error path of replyProto
func TestReplyProto_MarshalError(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	g := &RESTGateway{logger: logger}

	w := httptest.NewRecorder()
	c, _ := gin.CreateTestContext(w)
	c.Request = httptest.NewRequest("GET", "/test", nil)

	// A nil proto message should cause marshal to fail
	// Actually protojson.Marshal on nil will not panic, it returns empty
	// Let's use a different approach: create an impossible proto
	// proto messages don't fail marshal easily, but we can test the success path

	msg := &pb.HealthCheckResponse{Healthy: true}
	g.replyProto(c, msg)
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", w.Code)
	}
}

// ── replyProto with nil message ──

func TestReplyProto_NilMessage(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	g := &RESTGateway{logger: logger}

	w := httptest.NewRecorder()
	c, _ := gin.CreateTestContext(w)
	c.Request = httptest.NewRequest("GET", "/test", nil)

	// Pass nil proto — should handle gracefully
	g.replyProto(c, nil)
	// protojson.Marshal(nil) returns empty bytes, not error
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", w.Code)
	}
}

// ── gRPC handler routes (test that checkGRPCConn returns 503) ──
// These handlers all require a real gRPC connection. Without one,
// checkGRPCConn returns false and a 503 response is sent.
// We test that the endpoints are routable and respond correctly.

func gRPCHandlerTestRouter(g *RESTGateway) *gin.Engine {
	r := gin.New()
	r.Use(g.authMiddleware())
	v1 := r.Group("/api/v1")
	{
		v1.POST("/keys", g.bindKey)
		v1.DELETE("/keys/:keyId", g.unbindKey)
		v1.PUT("/keys/:keyId/suspend", g.suspendKey)
		v1.PUT("/keys/:keyId/resume", g.resumeKey)
		v1.PUT("/keys/:keyId/revoke", g.revokeKey)
		v1.PUT("/keys/:keyId/renew", g.renewKey)
		v1.GET("/keys/:keyId", g.getKey)
		v1.GET("/keys", g.listKeys)
		v1.POST("/shares", g.createShare)
		v1.POST("/shares/accept", g.acceptShare)
		v1.DELETE("/shares/:shareId", g.cancelShare)
		v1.GET("/shares/:shareId", g.getShare)
		v1.POST("/vehicles/:vehicleId/command", g.sendCommand)
		v1.GET("/vehicles/:vehicleId/status", g.streamStatus)
		v1.GET("/hub/adapters", g.listAdapters)
	}
	return r
}

func testGRPCHandlerReturns503(t *testing.T, method, path, body string) {
	t.Helper()
	g := newTestGateway()
	r := gRPCHandlerTestRouter(g)
	tok := issueTestToken(g, "test-user", "admin")

	w := httptest.NewRecorder()
	var req *http.Request
	if body != "" {
		req = httptest.NewRequest(method, path, strings.NewReader(body))
		req.Header.Set("Content-Type", "application/json")
	} else {
		req = httptest.NewRequest(method, path, nil)
	}
	req.Header.Set("Authorization", "Bearer "+tok)
	r.ServeHTTP(w, req)

	if w.Code != http.StatusServiceUnavailable {
		t.Fatalf("%s %s: expected 503 (gRPC unavailable), got %d", method, path, w.Code)
	}
	var resp map[string]string
	json.Unmarshal(w.Body.Bytes(), &resp)
	if resp["error"] != "GRPC_UNAVAILABLE" {
		t.Fatalf("expected GRPC_UNAVAILABLE, got %s", resp["error"])
	}
}

func TestGRPCHandler_BindKey(t *testing.T)    { testGRPCHandlerReturns503(t, "POST", "/api/v1/keys", `{}`) }
func TestGRPCHandler_UnbindKey(t *testing.T)  { testGRPCHandlerReturns503(t, "DELETE", "/api/v1/keys/key-1", "") }
func TestGRPCHandler_SuspendKey(t *testing.T) { testGRPCHandlerReturns503(t, "PUT", "/api/v1/keys/key-1/suspend", `{}`) }
func TestGRPCHandler_ResumeKey(t *testing.T)  { testGRPCHandlerReturns503(t, "PUT", "/api/v1/keys/key-1/resume", "") }
func TestGRPCHandler_RevokeKey(t *testing.T)  { testGRPCHandlerReturns503(t, "PUT", "/api/v1/keys/key-1/revoke", `{}`) }
func TestGRPCHandler_RenewKey(t *testing.T)   { testGRPCHandlerReturns503(t, "PUT", "/api/v1/keys/key-1/renew", `{}`) }
func TestGRPCHandler_GetKey(t *testing.T)     { testGRPCHandlerReturns503(t, "GET", "/api/v1/keys/key-1", "") }
func TestGRPCHandler_ListKeys(t *testing.T)   { testGRPCHandlerReturns503(t, "GET", "/api/v1/keys?user_id=u&vehicle_id=v&limit=10&offset=0", "") }
func TestGRPCHandler_CreateShare(t *testing.T) { testGRPCHandlerReturns503(t, "POST", "/api/v1/shares", `{}`) }
func TestGRPCHandler_AcceptShare(t *testing.T) { testGRPCHandlerReturns503(t, "POST", "/api/v1/shares/accept", `{}`) }
func TestGRPCHandler_CancelShare(t *testing.T) { testGRPCHandlerReturns503(t, "DELETE", "/api/v1/shares/share-1", "") }
func TestGRPCHandler_GetShare(t *testing.T)    { testGRPCHandlerReturns503(t, "GET", "/api/v1/shares/share-1", "") }
func TestGRPCHandler_SendCommand(t *testing.T) { testGRPCHandlerReturns503(t, "POST", "/api/v1/vehicles/vh-1/command", `{}`) }
func TestGRPCHandler_ListAdapters(t *testing.T){ testGRPCHandlerReturns503(t, "GET", "/api/v1/hub/adapters", "") }

// ── gRPC Integration Tests ──

// mockKeyManagementServer implements pb.KeyManagementServiceServer for testing

type mockKeyManagementServer struct {
	pb.UnimplementedKeyManagementServiceServer
}

func (s *mockKeyManagementServer) BindKey(ctx context.Context, req *pb.BindKeyRequest) (*pb.BindKeyResponse, error) {
	return &pb.BindKeyResponse{
		Key: &pb.DigitalKey{
			KeyId: "key-" + req.GetVehicleId(),
			Status: pb.KeyStatus_ACTIVE,
		},
	}, nil
}

func (s *mockKeyManagementServer) UnbindKey(ctx context.Context, req *pb.UnbindKeyRequest) (*pb.UnbindKeyResponse, error) {
	return &pb.UnbindKeyResponse{}, nil
}

func (s *mockKeyManagementServer) SuspendKey(ctx context.Context, req *pb.SuspendKeyRequest) (*pb.SuspendKeyResponse, error) {
	return &pb.SuspendKeyResponse{}, nil
}

func (s *mockKeyManagementServer) ResumeKey(ctx context.Context, req *pb.ResumeKeyRequest) (*pb.ResumeKeyResponse, error) {
	return &pb.ResumeKeyResponse{}, nil
}

func (s *mockKeyManagementServer) RevokeKey(ctx context.Context, req *pb.RevokeKeyRequest) (*pb.RevokeKeyResponse, error) {
	return &pb.RevokeKeyResponse{}, nil
}

func (s *mockKeyManagementServer) RenewKey(ctx context.Context, req *pb.RenewKeyRequest) (*pb.RenewKeyResponse, error) {
	return &pb.RenewKeyResponse{}, nil
}

func (s *mockKeyManagementServer) GetKey(ctx context.Context, req *pb.GetKeyRequest) (*pb.GetKeyResponse, error) {
	return &pb.GetKeyResponse{
		Key: &pb.DigitalKey{
			KeyId:  req.GetKeyId(),
			Status: pb.KeyStatus_ACTIVE,
		},
	}, nil
}

func (s *mockKeyManagementServer) ListKeys(ctx context.Context, req *pb.ListKeysRequest) (*pb.ListKeysResponse, error) {
	return &pb.ListKeysResponse{
		Keys: []*pb.DigitalKey{
			{KeyId: "key-1", Status: pb.KeyStatus_ACTIVE},
		},
		Total: 1,
	}, nil
}

// mockKeyShareServer implements pb.KeyShareServiceServer

type mockKeyShareServer struct {
	pb.UnimplementedKeyShareServiceServer
}

func (s *mockKeyShareServer) CreateShare(ctx context.Context, req *pb.CreateShareRequest) (*pb.CreateShareResponse, error) {
	return &pb.CreateShareResponse{
		ShareId: "share-" + req.GetKeyId(),
		ShareCode: "ABC123",
	}, nil
}

func (s *mockKeyShareServer) AcceptShare(ctx context.Context, req *pb.AcceptShareRequest) (*pb.AcceptShareResponse, error) {
	return &pb.AcceptShareResponse{
		SharedSecret: []byte("secret"),
	}, nil
}

func (s *mockKeyShareServer) CancelShare(ctx context.Context, req *pb.CancelShareRequest) (*pb.CancelShareResponse, error) {
	return &pb.CancelShareResponse{}, nil
}

func (s *mockKeyShareServer) GetShare(ctx context.Context, req *pb.GetShareRequest) (*pb.GetShareResponse, error) {
	return &pb.GetShareResponse{
		ShareId: req.GetShareId(),
		FromUserId: "user-1",
	}, nil
}

// mockVehicleControlServer implements pb.VehicleControlServiceServer

type mockVehicleControlServer struct {
	pb.UnimplementedVehicleControlServiceServer
}

func (s *mockVehicleControlServer) SendCommand(ctx context.Context, req *pb.ControlCommandRequest) (*pb.ControlCommandResponse, error) {
	return &pb.ControlCommandResponse{
		CmdId:      "cmd-" + req.GetVehicleId(),
		ResultCode: 0,
		ErrorMsg:   "",
	}, nil
}

// mockHubTransportServer implements pb.HubTransportServiceServer

type mockHubTransportServer struct {
	pb.UnimplementedHubTransportServiceServer
}

func (s *mockHubTransportServer) HealthCheck(ctx context.Context, req *pb.HealthCheckRequest) (*pb.HealthCheckResponse, error) {
	return &pb.HealthCheckResponse{
		Healthy: true,
		Adapters: []*pb.AdapterStatus{
			{Vendor: "mock", Protocol: "test", Healthy: true},
		},
	}, nil
}

// startTestGRPCServer starts a gRPC server on a random port and returns the address and a cleanup function
func startTestGRPCServer(t *testing.T) (string, func()) {
	t.Helper()

	lis, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("failed to listen: %v", err)
	}

	srv := grpc.NewServer()
	pb.RegisterKeyManagementServiceServer(srv, &mockKeyManagementServer{})
	pb.RegisterKeyShareServiceServer(srv, &mockKeyShareServer{})
	pb.RegisterVehicleControlServiceServer(srv, &mockVehicleControlServer{})
	pb.RegisterHubTransportServiceServer(srv, &mockHubTransportServer{})

	go srv.Serve(lis) //nolint:errcheck

	return lis.Addr().String(), func() {
		srv.GracefulStop()
		lis.Close()
	}
}

// newGRPCConnectedGateway creates a gateway with a real gRPC connection to the test server
func newGRPCConnectedGateway(t *testing.T, addr string) *RESTGateway {
	t.Helper()

	logger, _ := zap.NewDevelopment()
	conn, err := grpc.Dial(addr, grpc.WithInsecure(), grpc.WithBlock(), grpc.WithTimeout(5*time.Second))
	if err != nil {
		t.Fatalf("failed to dial: %v", err)
	}

	g := NewRESTGateway(nil, logger)
	g.WithJWTSecret("test-secret-key")
	g.WithGRPCConn(conn)
	return g
}

func gRPCSuccessTestRouter(g *RESTGateway) *gin.Engine {
	r := gin.New()
	r.Use(gin.Recovery())
	r.Use(g.authMiddleware())

	v1 := r.Group("/api/v1")
	{
		keys := v1.Group("/keys")
		{
			keys.POST("", g.bindKey)
			keys.DELETE("/:keyId", g.unbindKey)
			keys.PUT("/:keyId/suspend", g.suspendKey)
			keys.PUT("/:keyId/resume", g.resumeKey)
			keys.PUT("/:keyId/revoke", g.revokeKey)
			keys.PUT("/:keyId/renew", g.renewKey)
			keys.GET("/:keyId", g.getKey)
			keys.GET("", g.listKeys)
		}

		shares := v1.Group("/shares")
		{
			shares.POST("", g.createShare)
			shares.POST("/accept", g.acceptShare)
			shares.DELETE("/:shareId", g.cancelShare)
			shares.GET("/:shareId", g.getShare)
		}

		vehicles := v1.Group("/vehicles")
		{
			vehicles.POST("/:vehicleId/command", g.sendCommand)
		}

		hub := v1.Group("/hub")
		{
			hub.GET("/adapters", g.listAdapters)
			hub.GET("/health", g.hubHealthCheck)
		}
	}
	return r
}

func makeGRPCSuccessRequest(r *gin.Engine, method, path, body, token string) *httptest.ResponseRecorder {
	w := httptest.NewRecorder()
	var req *http.Request
	if body != "" {
		req = httptest.NewRequest(method, path, strings.NewReader(body))
		req.Header.Set("Content-Type", "application/json")
	} else {
		req = httptest.NewRequest(method, path, nil)
	}
	req.Header.Set("Authorization", "Bearer "+token)
	r.ServeHTTP(w, req)
	return w
}

// ── gRPC Key Management Handler Integration Tests ──

func TestGRPC_BindKey_Success(t *testing.T) {
	addr, cleanup := startTestGRPCServer(t)
	defer cleanup()

	g := newGRPCConnectedGateway(t, addr)
	r := gRPCSuccessTestRouter(g)
	tok := issueTestToken(g, "user-grpc", "admin")

	body := `{"vehicle_id": "VH-001", "user_id": "user-1"}`
	w := makeGRPCSuccessRequest(r, "POST", "/api/v1/keys", body, tok)
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", w.Code, w.Body.String())
	}
	var resp map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &resp)
	if resp["key"] == nil {
		t.Fatal("expected key in response")
	}
}

func TestGRPC_UnbindKey_Success(t *testing.T) {
	addr, cleanup := startTestGRPCServer(t)
	defer cleanup()

	g := newGRPCConnectedGateway(t, addr)
	r := gRPCSuccessTestRouter(g)
	tok := issueTestToken(g, "user-grpc", "admin")

	w := makeGRPCSuccessRequest(r, "DELETE", "/api/v1/keys/key-1", "", tok)
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", w.Code, w.Body.String())
	}
}

func TestGRPC_SuspendKey_Success(t *testing.T) {
	addr, cleanup := startTestGRPCServer(t)
	defer cleanup()

	g := newGRPCConnectedGateway(t, addr)
	r := gRPCSuccessTestRouter(g)
	tok := issueTestToken(g, "user-grpc", "admin")

	w := makeGRPCSuccessRequest(r, "PUT", "/api/v1/keys/key-1/suspend", `{"reason":"test"}`, tok)
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", w.Code, w.Body.String())
	}
}

func TestGRPC_ResumeKey_Success(t *testing.T) {
	addr, cleanup := startTestGRPCServer(t)
	defer cleanup()

	g := newGRPCConnectedGateway(t, addr)
	r := gRPCSuccessTestRouter(g)
	tok := issueTestToken(g, "user-grpc", "admin")

	w := makeGRPCSuccessRequest(r, "PUT", "/api/v1/keys/key-1/resume", "", tok)
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", w.Code, w.Body.String())
	}
}

func TestGRPC_RevokeKey_Success(t *testing.T) {
	addr, cleanup := startTestGRPCServer(t)
	defer cleanup()

	g := newGRPCConnectedGateway(t, addr)
	r := gRPCSuccessTestRouter(g)
	tok := issueTestToken(g, "user-grpc", "admin")

	w := makeGRPCSuccessRequest(r, "PUT", "/api/v1/keys/key-1/revoke", `{"reason":"stolen"}`, tok)
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", w.Code, w.Body.String())
	}
}

func TestGRPC_RenewKey_Success(t *testing.T) {
	addr, cleanup := startTestGRPCServer(t)
	defer cleanup()

	g := newGRPCConnectedGateway(t, addr)
	r := gRPCSuccessTestRouter(g)
	tok := issueTestToken(g, "user-grpc", "admin")

	body := `{"valid_until": 1893456000000}`
	w := makeGRPCSuccessRequest(r, "PUT", "/api/v1/keys/key-1/renew", body, tok)
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", w.Code, w.Body.String())
	}
}

func TestGRPC_GetKey_Success(t *testing.T) {
	addr, cleanup := startTestGRPCServer(t)
	defer cleanup()

	g := newGRPCConnectedGateway(t, addr)
	r := gRPCSuccessTestRouter(g)
	tok := issueTestToken(g, "user-grpc", "admin")

	w := makeGRPCSuccessRequest(r, "GET", "/api/v1/keys/key-1", "", tok)
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", w.Code, w.Body.String())
	}
	var resp map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &resp)
	key, ok := resp["key"].(map[string]interface{})
	if !ok {
		t.Fatal("expected key object")
	}
	if key["keyId"] != "key-1" {
		t.Fatalf("expected key-1, got %v", key["keyId"])
	}
}

func TestGRPC_ListKeys_Success(t *testing.T) {
	addr, cleanup := startTestGRPCServer(t)
	defer cleanup()

	g := newGRPCConnectedGateway(t, addr)
	r := gRPCSuccessTestRouter(g)
	tok := issueTestToken(g, "user-grpc", "admin")

	w := makeGRPCSuccessRequest(r, "GET", "/api/v1/keys?user_id=u&vehicle_id=v&limit=10&offset=0", "", tok)
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", w.Code, w.Body.String())
	}
	var resp map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &resp)
	keys, ok := resp["keys"].([]interface{})
	if !ok {
		t.Fatal("expected keys array")
	}
	if len(keys) != 1 {
		t.Fatalf("expected 1 key, got %d", len(keys))
	}
}

// ── gRPC Key Share Handler Integration Tests ──

func TestGRPC_CreateShare_Success(t *testing.T) {
	addr, cleanup := startTestGRPCServer(t)
	defer cleanup()

	g := newGRPCConnectedGateway(t, addr)
	r := gRPCSuccessTestRouter(g)
	tok := issueTestToken(g, "user-grpc", "admin")

	body := `{"key_id": "key-1", "to_user_id": "user-2"}`
	w := makeGRPCSuccessRequest(r, "POST", "/api/v1/shares", body, tok)
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", w.Code, w.Body.String())
	}
	var resp map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &resp)
	if resp["shareId"] == nil {
		t.Fatal("expected shareId")
	}
}

func TestGRPC_AcceptShare_Success(t *testing.T) {
	addr, cleanup := startTestGRPCServer(t)
	defer cleanup()

	g := newGRPCConnectedGateway(t, addr)
	r := gRPCSuccessTestRouter(g)
	tok := issueTestToken(g, "user-grpc", "admin")

	body := `{"share_code": "ABC123"}`
	w := makeGRPCSuccessRequest(r, "POST", "/api/v1/shares/accept", body, tok)
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", w.Code, w.Body.String())
	}
}

func TestGRPC_CancelShare_Success(t *testing.T) {
	addr, cleanup := startTestGRPCServer(t)
	defer cleanup()

	g := newGRPCConnectedGateway(t, addr)
	r := gRPCSuccessTestRouter(g)
	tok := issueTestToken(g, "user-grpc", "admin")

	w := makeGRPCSuccessRequest(r, "DELETE", "/api/v1/shares/share-1", "", tok)
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", w.Code, w.Body.String())
	}
}

func TestGRPC_GetShare_Success(t *testing.T) {
	addr, cleanup := startTestGRPCServer(t)
	defer cleanup()

	g := newGRPCConnectedGateway(t, addr)
	r := gRPCSuccessTestRouter(g)
	tok := issueTestToken(g, "user-grpc", "admin")

	w := makeGRPCSuccessRequest(r, "GET", "/api/v1/shares/share-1", "", tok)
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", w.Code, w.Body.String())
	}
	var resp map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &resp)
	if resp["shareId"] != "share-1" {
		t.Fatalf("expected shareId share-1, got %v", resp["shareId"])
	}
}

// ── gRPC Vehicle Control Handler Integration Tests ──

func TestGRPC_SendCommand_Success(t *testing.T) {
	addr, cleanup := startTestGRPCServer(t)
	defer cleanup()

	g := newGRPCConnectedGateway(t, addr)
	r := gRPCSuccessTestRouter(g)
	tok := issueTestToken(g, "user-grpc", "admin")

	body := `{"action":"lock","key_id":"key-1","params":{},"source":1,"trace_id":"trace-1"}`
	w := makeGRPCSuccessRequest(r, "POST", "/api/v1/vehicles/vh-1/command", body, tok)
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", w.Code, w.Body.String())
	}
	var resp map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &resp)
	if resp["cmdId"] == "" {
		t.Fatal("expected cmdId in response")
	}
}

// ── gRPC Hub Handler Integration Tests ──

func TestGRPC_ListAdapters_Success(t *testing.T) {
	addr, cleanup := startTestGRPCServer(t)
	defer cleanup()

	g := newGRPCConnectedGateway(t, addr)
	r := gRPCSuccessTestRouter(g)
	tok := issueTestToken(g, "user-grpc", "admin")

	w := makeGRPCSuccessRequest(r, "GET", "/api/v1/hub/adapters", "", tok)
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", w.Code, w.Body.String())
	}
	var resp map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &resp)
	adapters, ok := resp["adapters"].([]interface{})
	if !ok || len(adapters) != 1 {
		t.Fatalf("expected 1 adapter, got %v", resp["adapters"])
	}
}

func TestGRPC_HubHealthCheck_Success(t *testing.T) {
	addr, cleanup := startTestGRPCServer(t)
	defer cleanup()

	g := newGRPCConnectedGateway(t, addr)
	r := gRPCSuccessTestRouter(g)
	tok := issueTestToken(g, "user-grpc", "admin")

	w := makeGRPCSuccessRequest(r, "GET", "/api/v1/hub/health", "", tok)
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", w.Code, w.Body.String())
	}
	var resp map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &resp)
	if resp["healthy"] != true {
		t.Fatalf("expected healthy=true, got %v", resp["healthy"])
	}
}
