package gateway

import (
	"context"
	"crypto/subtle"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/golang-jwt/jwt/v5"
	"go.uber.org/zap"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/encoding/protojson"
	"google.golang.org/protobuf/proto"

	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/service"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/token"
)

var errAuthFailed = fmt.Errorf("authentication required")

// adminIssuer 管理端令牌的 iss 声明值 (HS256 + JWT_SECRET 轨道)
const adminIssuer = "dkcs-admin"

// ── Configuration Defaults ──

const (
	DefaultRateLimit      = 100 // max requests per second per IP
	DefaultRateLimitBurst = 200 // max burst size
	RateLimitCleanupSec   = 300 // clean up stale entries every 5 min
	RateLimitEntryTTL     = 10  // evict IP entries idle for >10 min
)

// ── Rate Limiter ──

// rateLimiter implements a per-IP token bucket rate limiter.
// Thread-safe: all public methods acquire the mutex.
type rateLimiter struct {
	mu       sync.Mutex
	visitors map[string]*tokenBucket
	rate     float64 // tokens replenished per second
	burst    int     // max accumulated tokens
}

// tokenBucket tracks one IP's token balance and last refill time.
type tokenBucket struct {
	tokens     float64
	lastUpdate time.Time
}

// newRateLimiter creates a token-bucket rate limiter.
//
//	rate:  tokens replenished per second
//	burst: maximum accumulated tokens (prevents abuse via burst)
func newRateLimiter(rate float64, burst int) *rateLimiter {
	rl := &rateLimiter{
		visitors: make(map[string]*tokenBucket),
		rate:     rate,
		burst:    burst,
	}
	// Start periodic cleanup of stale entries
	go rl.cleanupLoop(RateLimitCleanupSec * time.Second)
	return rl
}

// allow checks whether the given IP may consume one token.
// Returns (allowed bool, retryAfter time.Duration).
func (rl *rateLimiter) allow(ip string) (bool, time.Duration) {
	rl.mu.Lock()
	defer rl.mu.Unlock()

	bucket, exists := rl.visitors[ip]
	now := time.Now()

	if !exists {
		// First request — start with burst-1 tokens remaining
		rl.visitors[ip] = &tokenBucket{
			tokens:     float64(rl.burst - 1),
			lastUpdate: now,
		}
		return true, 0
	}

	// Refill based on elapsed time
	elapsedSec := now.Sub(bucket.lastUpdate).Seconds()
	bucket.tokens += elapsedSec * rl.rate
	if bucket.tokens > float64(rl.burst) {
		bucket.tokens = float64(rl.burst)
	}
	bucket.lastUpdate = now

	// Attempt to consume one token
	if bucket.tokens >= 1.0 {
		bucket.tokens--
		return true, 0
	}

	// Rate limited — calculate when the next token will be available
	waitSec := (1.0 - bucket.tokens) / rl.rate
	retryAfter := time.Duration(waitSec * float64(time.Second))
	return false, retryAfter
}

// cleanupLoop periodically removes visitors that haven't been seen recently.
func (rl *rateLimiter) cleanupLoop(interval time.Duration) {
	ticker := time.NewTicker(interval)
	defer ticker.Stop()
	for range ticker.C {
		rl.mu.Lock()
		cutoff := time.Now().Add(-RateLimitEntryTTL * time.Minute)
		for ip, bucket := range rl.visitors {
			if bucket.lastUpdate.Before(cutoff) {
				delete(rl.visitors, ip)
			}
		}
		rl.mu.Unlock()
	}
}

// ── RESTGateway ──

// RESTGateway REST API → gRPC 转换网关
// 手机App通过HTTPS访问，网关转为gRPC调用HUB内部服务
type RESTGateway struct {
	grpcSrv       *grpc.Server
	logger        *zap.Logger
	httpSrv       *http.Server
	jwtSecret     string
	oemJWKS       *oemJWKSVerifier // OEM 令牌 (RS256/ES256 + JWKS) 验证器, nil 时拒绝 OEM 令牌
	grpcConn      *grpc.ClientConn
	rateLimiter   *rateLimiter
	deviceService *service.DeviceService
	tokenSvc      *token.Service
	dkServer      service.DKServer
	tlsCertFile   string
	tlsKeyFile    string
}

func NewRESTGateway(grpcSrv *grpc.Server, logger *zap.Logger) *RESTGateway {
	return &RESTGateway{
		grpcSrv:       grpcSrv,
		logger:        logger,
		deviceService: service.NewDeviceService(logger),
		tokenSvc:      token.NewService(""),
		dkServer:      service.NewLocalDKServer(),
	}
}

// WithJWTSecret sets the JWT secret for auth middleware
func (g *RESTGateway) WithJWTSecret(secret string) *RESTGateway {
	g.jwtSecret = secret
	return g
}

// WithOEMJWKS 配置 OEM JWKS 验证器 (oem_id -> JWKS URL 映射)。
// 未配置时 (nil/空 map) OEM 令牌将被拒绝 (失败即关闭)。
func (g *RESTGateway) WithOEMJWKS(urls map[string]string) *RESTGateway {
	if len(urls) > 0 {
		g.oemJWKS = newOEMJWKSVerifier(g.logger, urls)
	} else {
		g.oemJWKS = nil
	}
	return g
}

// WithGRPCConn sets a gRPC client connection for forwarding to HUB services
func (g *RESTGateway) WithGRPCConn(conn *grpc.ClientConn) *RESTGateway {
	g.grpcConn = conn
	return g
}

// WithTLS configures TLS certificate/key for HTTPS serving.
// If not set, the gateway serves plain HTTP (local development only).
func (g *RESTGateway) WithTLS(certFile, keyFile string) *RESTGateway {
	g.tlsCertFile = certFile
	g.tlsKeyFile = keyFile
	return g
}

// WithRateLimit configures the per-IP rate limiter.
// Set limit <= 0 to disable rate limiting entirely.
func (g *RESTGateway) WithRateLimit(requestsPerSec, burst int) *RESTGateway {
	if requestsPerSec > 0 {
		burstVal := burst
		if burstVal <= 0 {
			burstVal = requestsPerSec * 2
		}
		g.rateLimiter = newRateLimiter(float64(requestsPerSec), burstVal)
		g.logger.Info("Rate limiter enabled",
			zap.Int("requests_per_sec", requestsPerSec),
			zap.Int("burst", burstVal),
		)
	} else {
		g.logger.Info("Rate limiter disabled explicitly")
	}
	return g
}

func (g *RESTGateway) Serve(addr string) error {
	// [S-01] JWT空密钥防御 — 启动时检查
	if g.jwtSecret == "" {
		g.logger.Fatal("JWT_SECRET must be set — refusing to start")
	}
	// 检查是否为默认/弱密钥
	defaultKeys := []string{"default", "secret", "changeme", "your-secret-key"}
	for _, dk := range defaultKeys {
		if g.jwtSecret == dk {
			g.logger.Fatal("JWT_SECRET must not be a default/weak value — refusing to start")
		}
	}

	r := gin.New()
	r.Use(gin.Recovery())
	r.Use(g.loggerMiddleware())

	// [CRIT-2] 注册速率限制中间件（全局应用）
	if g.rateLimiter != nil {
		r.Use(g.rateLimitMiddleware())
	}

	// ── 健康检查 (公开) ──
	r.GET("/health", func(c *gin.Context) {
		c.JSON(200, gin.H{"status": "ok"})
	})

	r.POST("/api/v1/auth/login", g.login) // 登录获取token

	// ── Mailbox API (公开 — 无需认证，安全由 mailbox_id 随机性 + E2E 加密保障) ──
	mailbox := r.Group("/api/v1/mailbox")
	{
		mailbox.POST("", g.createMailbox)                    // 创建邮箱
		mailbox.GET("/:id/display", g.readMailboxDisplay)    // 读取展示信息
		mailbox.GET("/:id/content", g.readMailboxContent)    // 读取加密内容
		mailbox.PUT("/:id", g.updateMailbox)                 // 更新邮箱
		mailbox.DELETE("/:id", g.deleteMailbox)              // 删除邮箱
		mailbox.POST("/:id/relinquish", g.relinquishMailbox) // 转移邮箱
	}

	// ── API v1 (需要认证) ──
	v1 := r.Group("/api/v1")
	v1.Use(g.authMiddleware())
	{
		// 密钥管理
		keys := v1.Group("/keys")
		{
			keys.POST("", g.bindKey)            // 绑定密钥
			keys.DELETE("/:keyId", g.unbindKey) // 解绑密钥
			keys.PUT("/:keyId/suspend", g.suspendKey)
			keys.PUT("/:keyId/resume", g.resumeKey)
			keys.PUT("/:keyId/revoke", g.revokeKey)
			keys.PUT("/:keyId/renew", g.renewKey)
			keys.GET("/:keyId", g.getKey)
			keys.GET("", g.listKeys)
		}

		// 密钥分享
		shares := v1.Group("/shares")
		{
			shares.POST("", g.createShare)
			shares.POST("/accept", g.acceptShare)
			shares.DELETE("/:shareId", g.cancelShare)
			shares.GET("/:shareId", g.getShare)
		}

		// 车辆控制
		vehicles := v1.Group("/vehicles")
		{
			vehicles.POST("/:vehicleId/command", g.sendCommand)
			vehicles.GET("/:vehicleId/status", g.streamStatus)
		}

		// HUB管理
		hub := v1.Group("/hub")
		{
			hub.GET("/adapters", g.listAdapters)
			hub.GET("/health", g.hubHealthCheck)
		}

		// Token 管理（统一授权）
		tokens := v1.Group("/tokens")
		{
			tokens.POST("", g.issueToken)
			tokens.GET("/:tokenId", g.verifyToken)
			tokens.DELETE("/:tokenId", g.revokeToken)
		}

		// 多设备管理
		devices := v1.Group("/devices")
		{
			devices.POST("", g.registerDevice)                      // 注册设备+上报能力
			devices.GET("", g.listDevices)                          // 列出我的设备
			devices.GET("/:deviceId", g.getDevice)                  // 查看设备详情
			devices.POST("/:deviceId/provision", g.provisionDevice) // 给设备配钥
			devices.POST("/:deviceId/revoke", g.revokeDevice)       // 吊销设备钥匙
			devices.DELETE("/:deviceId", g.deleteDevice)            // 删除设备
		}
	}

	g.httpSrv = &http.Server{
		Addr:         addr,
		Handler:      r,
		ReadTimeout:  10 * time.Second,
		WriteTimeout: 30 * time.Second,
	}

	// TLS 支持: 配置了证书则 HTTPS，否则 HTTP（本地开发）
	if g.tlsCertFile != "" && g.tlsKeyFile != "" {
		g.logger.Info("REST gateway serving HTTPS",
			zap.String("cert", g.tlsCertFile),
			zap.String("addr", addr),
		)
		return g.httpSrv.ListenAndServeTLS(g.tlsCertFile, g.tlsKeyFile)
	}
	g.logger.Info("REST gateway serving HTTP (no TLS configured)", zap.String("addr", addr))
	return g.httpSrv.ListenAndServe()
}

func (g *RESTGateway) Shutdown(ctx context.Context) error {
	if g.httpSrv != nil {
		return g.httpSrv.Shutdown(ctx)
	}
	return nil
}

// ── Middleware ──

func (g *RESTGateway) loggerMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		start := time.Now()
		c.Next()
		g.logger.Info("HTTP",
			zap.String("method", c.Request.Method),
			zap.String("path", c.Request.URL.Path),
			zap.Int("status", c.Writer.Status()),
			zap.Duration("latency", time.Since(start)),
			zap.String("client_ip", c.ClientIP()),
		)
	}
}

// rateLimitMiddleware rejects requests that exceed the per-IP rate limit.
// Returns 429 Too Many Requests with a Retry-After header when throttled.
func (g *RESTGateway) rateLimitMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		clientIP := c.ClientIP()
		allowed, retryAfter := g.rateLimiter.allow(clientIP)
		if !allowed {
			retrySec := int(retryAfter.Seconds()) + 1 // round up
			g.logger.Warn("Rate limit exceeded",
				zap.String("client_ip", clientIP),
				zap.String("method", c.Request.Method),
				zap.String("path", c.Request.URL.Path),
				zap.Int("retry_after_sec", retrySec),
			)
			c.Header("Retry-After", strconv.Itoa(retrySec))
			c.AbortWithStatusJSON(http.StatusTooManyRequests, gin.H{
				"error":   "ERR_RATE_LIMIT",
				"message": "too many requests — slow down",
				"detail":  fmt.Sprintf("retry after %d seconds", retrySec),
			})
			return
		}
		c.Next()
	}
}

// authMiddleware validates JWT Bearer token from Authorization header
func (g *RESTGateway) authMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		authHeader := c.GetHeader("Authorization")
		if authHeader == "" {
			// [M-06] 统一错误格式
			c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{"error": "AUTH_MISSING_HEADER", "message": "missing authorization header", "detail": ""})
			return
		}

		token := strings.TrimPrefix(authHeader, "Bearer ")
		if token == "" || token == authHeader {
			// [M-06] 统一错误格式
			c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{"error": "AUTH_INVALID_FORMAT", "message": "invalid token format", "detail": ""})
			return
		}

		userID, role, err := g.validateToken(token)
		if err != nil {
			g.logger.Warn("Auth failed", zap.String("token_prefix", token[:min(8, len(token))]), zap.Error(err))
			// [M-06] 统一错误格式
			c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{"error": "AUTH_INVALID_TOKEN", "message": "invalid or expired token", "detail": err.Error()})
			return
		}

		// Inject user info into gin context
		c.Set("user_id", userID)
		c.Set("user_role", role)

		// Also inject into gRPC metadata for downstream calls
		md := metadata.Pairs(
			"user_id", userID,
			"user_role", role,
		)
		c.Request = c.Request.WithContext(metadata.NewOutgoingContext(c.Request.Context(), md))

		c.Next()
	}
}

// validateToken validates a JWT bearer token and returns (userID, role, error).
// 双轨验证 [P1-2]:
//   - iss == "dkcs-admin" → HS256 + JWT_SECRET (管理端令牌)
//   - iss 为已配置的 OEM id → RS256/ES256 + 该 OEM 的 JWKS 公钥 (设备厂令牌)
//   - 其他/缺失 iss → 拒绝 (Unauthenticated)
func (g *RESTGateway) validateToken(tokenString string) (string, string, error) {
	if tokenString == "" {
		return "", "", status.Error(codes.Unauthenticated, "empty token")
	}

	// 第一遍: 不验证签名, 仅解析 claims 以获取 iss, 决定验证轨道
	parser := jwt.NewParser(jwt.WithoutClaimsValidation())
	parsed, _, err := parser.ParseUnverified(tokenString, jwt.MapClaims{})
	if err != nil {
		return "", "", status.Errorf(codes.Unauthenticated, "invalid token: %v", err)
	}
	claims, ok := parsed.Claims.(jwt.MapClaims)
	if !ok {
		return "", "", status.Error(codes.Unauthenticated, "invalid token claims")
	}
	iss, _ := claims["iss"].(string)

	switch {
	case iss == adminIssuer:
		return g.validateAdminToken(tokenString)
	case iss != "":
		return g.validateOEMToken(tokenString, iss)
	default:
		return "", "", status.Error(codes.Unauthenticated, "token missing iss claim")
	}
}

// validateAdminToken 验证管理端令牌: HS256 + jwtSecret。
// 校验签名、过期时间 (exp 必填) 与 user_id 声明。
func (g *RESTGateway) validateAdminToken(tokenString string) (string, string, error) {
	parser := jwt.NewParser(jwt.WithExpirationRequired())
	token, err := parser.Parse(tokenString, func(token *jwt.Token) (interface{}, error) {
		if _, ok := token.Method.(*jwt.SigningMethodHMAC); !ok {
			return nil, fmt.Errorf("unexpected signing method: %v", token.Header["alg"])
		}
		return []byte(g.jwtSecret), nil
	})
	if err != nil {
		return "", "", status.Errorf(codes.Unauthenticated, "invalid token: %v", err)
	}

	claims, ok := token.Claims.(jwt.MapClaims)
	if !ok || !token.Valid {
		return "", "", status.Error(codes.Unauthenticated, "invalid token claims")
	}

	userID, _ := claims["user_id"].(string)
	role, _ := claims["role"].(string)

	if userID == "" {
		return "", "", status.Error(codes.Unauthenticated, "invalid user_id in token")
	}
	if role == "" {
		role = "user"
	}

	return userID, role, nil
}

// validateOEMToken 验证设备厂 (OEM) 令牌: RS256/ES256 + 该 OEM 的 JWKS 公钥。
// 未配置该 OEM 的 JWKS 时直接拒绝 (失败即关闭)。
// user_id 命名空间: "oem:<oem_id>:<sub>" (sub 缺失时回退 "unknown"), role 固定为 "user"。
func (g *RESTGateway) validateOEMToken(tokenString, iss string) (string, string, error) {
	if g.oemJWKS == nil {
		return "", "", status.Errorf(codes.Unauthenticated, "OEM issuer %q not configured", iss)
	}
	if _, ok := g.oemJWKS.urls[iss]; !ok {
		return "", "", status.Errorf(codes.Unauthenticated, "unknown OEM issuer %q", iss)
	}

	// [P1-2] OEM 令牌同样要求 exp 必填 — 无过期时间的令牌永不过期, 拒绝
	parser := jwt.NewParser(jwt.WithExpirationRequired())
	token, err := parser.Parse(tokenString, g.oemJWKS.keyfunc(iss))
	if err != nil {
		return "", "", status.Errorf(codes.Unauthenticated, "invalid OEM token: %v", err)
	}

	claims, ok := token.Claims.(jwt.MapClaims)
	if !ok || !token.Valid {
		return "", "", status.Error(codes.Unauthenticated, "invalid OEM token claims")
	}

	// [P1-2] sub 必填 — 缺失 sub 会退化为 oem:<iss>:unknown, 造成跨设备身份碰撞
	sub, _ := claims["sub"].(string)
	if sub == "" {
		return "", "", status.Error(codes.Unauthenticated, "OEM token missing sub claim")
	}
	userID := "oem:" + iss + ":" + sub
	// role 固定为 "user" — OEM 令牌无特权提升通道
	return userID, "user", nil
}

// login handles user authentication and issues real JWT tokens.
// Credentials are validated against the admin account configured via environment variables.
func (g *RESTGateway) login(c *gin.Context) {
	var req struct {
		UserID   string `json:"user_id"`
		Password string `json:"password"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		// [M-06] 统一错误格式
		c.JSON(http.StatusBadRequest, gin.H{"error": "BAD_REQUEST", "message": "invalid request", "detail": err.Error()})
		return
	}

	if req.UserID == "" || req.Password == "" {
		// [M-06] 统一错误格式
		c.JSON(http.StatusBadRequest, gin.H{"error": "BAD_REQUEST", "message": "user_id and password are required", "detail": ""})
		return
	}

	// Validate credentials against admin account from environment variables.
	// [P1-2] 失败即关闭: 未配置 ADMIN_USERNAME/ADMIN_PASSWORD 时拒绝一切登录。
	adminUser := os.Getenv("ADMIN_USERNAME")
	adminPass := os.Getenv("ADMIN_PASSWORD")
	if adminUser == "" || adminPass == "" {
		g.logger.Warn("Admin auth not configured — login rejected (set ADMIN_USERNAME/ADMIN_PASSWORD)")
		// [M-06] 统一错误格式
		c.JSON(http.StatusServiceUnavailable, gin.H{
			"error":   "SERVICE_UNAVAILABLE",
			"message": "admin auth not configured",
			"detail":  "",
		})
		return
	}

	// [P1-2] 常量时间比较, 防时序侧信道 (user_id + password)
	userMatch := subtle.ConstantTimeCompare([]byte(req.UserID), []byte(adminUser)) == 1
	passMatch := subtle.ConstantTimeCompare([]byte(req.Password), []byte(adminPass)) == 1
	if !userMatch || !passMatch {
		g.logger.Warn("Login failed", zap.String("user_id", req.UserID))
		// [M-06] 统一错误格式
		c.JSON(http.StatusUnauthorized, gin.H{"error": "AUTH_INVALID_CREDENTIALS", "message": "invalid credentials", "detail": ""})
		return
	}

	// Generate JWT with standard claims
	now := time.Now()
	claims := jwt.MapClaims{
		"user_id": req.UserID,
		"role":    "admin",
		"iss":     adminIssuer,
		"iat":     now.Unix(),
		"exp":     now.Add(1 * time.Hour).Unix(),
	}

	token := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	tokenString, err := token.SignedString([]byte(g.jwtSecret))
	if err != nil {
		g.logger.Error("Failed to sign JWT", zap.Error(err))
		// [M-06] 统一错误格式
		c.JSON(http.StatusInternalServerError, gin.H{"error": "INTERNAL_ERROR", "message": "failed to generate token", "detail": "token signing failure"})
		return
	}

	g.logger.Info("Login success", zap.String("user_id", req.UserID))
	c.JSON(http.StatusOK, gin.H{
		"token":      tokenString,
		"token_type": "Bearer",
		"expires_in": 3600,
	})
}

// ── gRPC Client Helpers ──

func (g *RESTGateway) keyManagementClient() pb.KeyManagementServiceClient {
	return pb.NewKeyManagementServiceClient(g.grpcConn)
}

func (g *RESTGateway) keyShareClient() pb.KeyShareServiceClient {
	return pb.NewKeyShareServiceClient(g.grpcConn)
}

func (g *RESTGateway) vehicleControlClient() pb.VehicleControlServiceClient {
	return pb.NewVehicleControlServiceClient(g.grpcConn)
}

func (g *RESTGateway) hubTransportClient() pb.HubTransportServiceClient {
	return pb.NewHubTransportServiceClient(g.grpcConn)
}

// ── Helper Methods ──

// checkGRPCConn checks whether the gRPC client connection is available.
// Returns true if conn is usable, false and sends 503 if nil.
func (g *RESTGateway) checkGRPCConn(c *gin.Context, conn *grpc.ClientConn) bool {
	if conn == nil {
		g.logger.Warn("gRPC connection not available")
		// [M-06] 统一错误格式
		c.AbortWithStatusJSON(http.StatusServiceUnavailable, gin.H{
			"error":   "GRPC_UNAVAILABLE",
			"message": "hub service temporarily unavailable",
			"detail":  "",
		})
		return false
	}
	return true
}

// handleGRPCError maps gRPC status codes to HTTP status codes and responds
// with a JSON error body.
func (g *RESTGateway) handleGRPCError(c *gin.Context, err error) {
	st, _ := status.FromError(err)
	httpStatus := http.StatusBadGateway
	errorCode := "GRPC_ERROR"

	switch st.Code() {
	case codes.NotFound:
		httpStatus = http.StatusNotFound
		errorCode = "GRPC_NOT_FOUND"
	case codes.PermissionDenied:
		httpStatus = http.StatusForbidden
		errorCode = "GRPC_PERMISSION_DENIED"
	case codes.Unauthenticated:
		httpStatus = http.StatusUnauthorized
		errorCode = "GRPC_UNAUTHENTICATED"
	case codes.Internal:
		httpStatus = http.StatusInternalServerError
		errorCode = "GRPC_INTERNAL"
	case codes.InvalidArgument:
		httpStatus = http.StatusBadRequest
		errorCode = "GRPC_INVALID_ARGUMENT"
	case codes.Unavailable:
		httpStatus = http.StatusServiceUnavailable
		errorCode = "GRPC_UNAVAILABLE"
	case codes.DeadlineExceeded:
		httpStatus = http.StatusGatewayTimeout
		errorCode = "GRPC_DEADLINE_EXCEEDED"
	case codes.FailedPrecondition:
		httpStatus = http.StatusConflict
		errorCode = "GRPC_FAILED_PRECONDITION"
	case codes.ResourceExhausted:
		httpStatus = http.StatusTooManyRequests
		errorCode = "GRPC_RESOURCE_EXHAUSTED"
	default:
		httpStatus = http.StatusBadGateway
		errorCode = "GRPC_UNKNOWN"
	}

	g.logger.Warn("gRPC forwarding error",
		zap.String("path", c.Request.URL.Path),
		zap.String("grpc_code", st.Code().String()),
		zap.String("grpc_message", st.Message()),
	)

	// [M-06] 统一错误格式: {error: code, message: "", detail: ""}
	c.AbortWithStatusJSON(httpStatus, gin.H{
		"error":   errorCode,
		"message": st.Message(),
		"detail":  st.Proto().GetDetails(),
	})
}

// replyProto serializes a protobuf message to JSON and sends it as the HTTP response.
func (g *RESTGateway) replyProto(c *gin.Context, resp proto.Message) {
	raw, err := protojson.Marshal(resp)
	if err != nil {
		g.logger.Error("failed to marshal proto response", zap.Error(err))
		// [M-06] 统一错误格式
		c.AbortWithStatusJSON(http.StatusInternalServerError, gin.H{"error": "INTERNAL_ERROR", "message": "internal serialization error", "detail": err.Error()})
		return
	}
	c.Data(http.StatusOK, "application/json", raw)
}

// parseBody reads the JSON request body and unmarshals it into the given protobuf message.
func (g *RESTGateway) parseBody(c *gin.Context, req proto.Message) error {
	body, err := io.ReadAll(c.Request.Body)
	if err != nil {
		return fmt.Errorf("failed to read request body: %w", err)
	}
	if err := protojson.Unmarshal(body, req); err != nil {
		return fmt.Errorf("failed to unmarshal request: %w", err)
	}
	return nil
}

// ── 1. Key Management (8 handlers) ──

// bindKey POST /api/v1/keys — 绑定密钥
func (g *RESTGateway) bindKey(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	userID, _ := c.Get("user_id")
	userIDStr := fmt.Sprint(userID)

	var req pb.BindKeyRequest
	if err := g.parseBody(c, &req); err != nil {
		// [M-06] 统一错误格式
		c.JSON(http.StatusBadRequest, gin.H{"error": "BAD_REQUEST", "message": "invalid request body", "detail": err.Error()})
		return
	}
	// Set user_id from auth context if not provided in body
	if req.UserId == "" {
		req.UserId = userIDStr
	}

	ctx := c.Request.Context()
	resp, err := g.keyManagementClient().BindKey(ctx, &req)
	if err != nil {
		g.handleGRPCError(c, err)
		return
	}

	g.logger.Info("bindKey",
		zap.String("user_id", userIDStr),
		zap.String("vehicle_id", req.VehicleId),
		zap.String("key_id", resp.GetKey().GetKeyId()),
	)
	g.replyProto(c, resp)
}

// unbindKey DELETE /api/v1/keys/:keyId — 解绑密钥
func (g *RESTGateway) unbindKey(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	keyID := c.Param("keyId")

	req := &pb.UnbindKeyRequest{
		KeyId:   keyID,
		TraceId: c.GetHeader("X-Trace-Id"),
	}

	ctx := c.Request.Context()
	resp, err := g.keyManagementClient().UnbindKey(ctx, req)
	if err != nil {
		g.handleGRPCError(c, err)
		return
	}

	g.logger.Info("unbindKey", zap.String("key_id", keyID))
	g.replyProto(c, resp)
}

// suspendKey PUT /api/v1/keys/:keyId/suspend — 挂起密钥
func (g *RESTGateway) suspendKey(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	keyID := c.Param("keyId")
	userID, _ := c.Get("user_id")

	// Allow optional reason from body
	var body struct {
		Reason string `json:"reason"`
	}
	_ = c.ShouldBindJSON(&body)

	req := &pb.SuspendKeyRequest{
		KeyId:   keyID,
		Reason:  body.Reason,
		TraceId: c.GetHeader("X-Trace-Id"),
	}

	ctx := c.Request.Context()
	resp, err := g.keyManagementClient().SuspendKey(ctx, req)
	if err != nil {
		g.handleGRPCError(c, err)
		return
	}

	g.logger.Info("suspendKey",
		zap.String("key_id", keyID),
		zap.String("user_id", fmt.Sprint(userID)),
	)
	g.replyProto(c, resp)
}

// resumeKey PUT /api/v1/keys/:keyId/resume — 恢复密钥
func (g *RESTGateway) resumeKey(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	keyID := c.Param("keyId")

	req := &pb.ResumeKeyRequest{
		KeyId:   keyID,
		TraceId: c.GetHeader("X-Trace-Id"),
	}

	ctx := c.Request.Context()
	resp, err := g.keyManagementClient().ResumeKey(ctx, req)
	if err != nil {
		g.handleGRPCError(c, err)
		return
	}

	g.logger.Info("resumeKey", zap.String("key_id", keyID))
	g.replyProto(c, resp)
}

// revokeKey PUT /api/v1/keys/:keyId/revoke — 撤销密钥
func (g *RESTGateway) revokeKey(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	keyID := c.Param("keyId")
	userID, _ := c.Get("user_id")
	userIDStr := fmt.Sprint(userID)

	var body struct {
		Reason string `json:"reason"`
	}
	_ = c.ShouldBindJSON(&body)

	req := &pb.RevokeKeyRequest{
		KeyId:   keyID,
		Reason:  body.Reason,
		TraceId: c.GetHeader("X-Trace-Id"),
	}

	ctx := c.Request.Context()
	resp, err := g.keyManagementClient().RevokeKey(ctx, req)
	if err != nil {
		g.handleGRPCError(c, err)
		return
	}

	g.logger.Info("revokeKey",
		zap.String("key_id", keyID),
		zap.String("user_id", userIDStr),
	)
	g.replyProto(c, resp)
}

// renewKey PUT /api/v1/keys/:keyId/renew — 续期密钥
func (g *RESTGateway) renewKey(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	keyID := c.Param("keyId")

	var body struct {
		ValidUntil int64 `json:"valid_until"`
	}
	if err := c.ShouldBindJSON(&body); err != nil {
		// [M-06] 统一错误格式
		c.JSON(http.StatusBadRequest, gin.H{"error": "BAD_REQUEST", "message": "invalid request body", "detail": err.Error()})
		return
	}

	req := &pb.RenewKeyRequest{
		KeyId:      keyID,
		ValidUntil: body.ValidUntil,
		TraceId:    c.GetHeader("X-Trace-Id"),
	}

	ctx := c.Request.Context()
	resp, err := g.keyManagementClient().RenewKey(ctx, req)
	if err != nil {
		g.handleGRPCError(c, err)
		return
	}

	g.logger.Info("renewKey",
		zap.String("key_id", keyID),
		zap.Int64("valid_until", body.ValidUntil),
	)
	g.replyProto(c, resp)
}

// getKey GET /api/v1/keys/:keyId — 查询单个密钥
func (g *RESTGateway) getKey(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	keyID := c.Param("keyId")

	req := &pb.GetKeyRequest{
		KeyId: keyID,
	}

	ctx := c.Request.Context()
	resp, err := g.keyManagementClient().GetKey(ctx, req)
	if err != nil {
		g.handleGRPCError(c, err)
		return
	}

	g.logger.Info("getKey", zap.String("key_id", keyID))
	g.replyProto(c, resp)
}

// listKeys GET /api/v1/keys — 查询密钥列表
func (g *RESTGateway) listKeys(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	req := &pb.ListKeysRequest{}

	if uid := c.Query("user_id"); uid != "" {
		req.UserId = uid
	}
	if vid := c.Query("vehicle_id"); vid != "" {
		req.VehicleId = vid
	}
	if limitStr := c.Query("limit"); limitStr != "" {
		if limit, err := strconv.Atoi(limitStr); err == nil && limit > 0 {
			req.PageSize = int32(limit)
		}
	}
	if offsetStr := c.Query("offset"); offsetStr != "" {
		req.PageToken = offsetStr
	}

	ctx := c.Request.Context()
	resp, err := g.keyManagementClient().ListKeys(ctx, req)
	if err != nil {
		g.handleGRPCError(c, err)
		return
	}

	g.logger.Info("listKeys",
		zap.String("user_id", req.UserId),
		zap.String("vehicle_id", req.VehicleId),
	)
	g.replyProto(c, resp)
}

// ── 2. Key Sharing (4 handlers) ──

// createShare POST /api/v1/shares — 创建密钥分享
func (g *RESTGateway) createShare(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	userID, _ := c.Get("user_id")
	userIDStr := fmt.Sprint(userID)

	var req pb.CreateShareRequest
	if err := g.parseBody(c, &req); err != nil {
		// [M-06] 统一错误格式
		c.JSON(http.StatusBadRequest, gin.H{"error": "BAD_REQUEST", "message": "invalid request body", "detail": err.Error()})
		return
	}
	if req.FromUserId == "" {
		req.FromUserId = userIDStr
	}

	ctx := c.Request.Context()
	resp, err := g.keyShareClient().CreateShare(ctx, &req)
	if err != nil {
		g.handleGRPCError(c, err)
		return
	}

	g.logger.Info("createShare",
		zap.String("from_user", userIDStr),
		zap.String("share_id", resp.GetShareId()),
	)
	g.replyProto(c, resp)
}

// acceptShare POST /api/v1/shares/accept — 接受密钥分享
func (g *RESTGateway) acceptShare(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	userID, _ := c.Get("user_id")
	userIDStr := fmt.Sprint(userID)

	var req pb.AcceptShareRequest
	if err := g.parseBody(c, &req); err != nil {
		// [M-06] 统一错误格式
		c.JSON(http.StatusBadRequest, gin.H{"error": "BAD_REQUEST", "message": "invalid request body", "detail": err.Error()})
		return
	}
	if req.UserId == "" {
		req.UserId = userIDStr
	}

	ctx := c.Request.Context()
	resp, err := g.keyShareClient().AcceptShare(ctx, &req)
	if err != nil {
		g.handleGRPCError(c, err)
		return
	}

	g.logger.Info("acceptShare", zap.String("user_id", userIDStr))
	g.replyProto(c, resp)
}

// cancelShare DELETE /api/v1/shares/:shareId — 取消密钥分享
func (g *RESTGateway) cancelShare(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	shareID := c.Param("shareId")

	req := &pb.CancelShareRequest{
		ShareId: shareID,
		TraceId: c.GetHeader("X-Trace-Id"),
	}

	ctx := c.Request.Context()
	resp, err := g.keyShareClient().CancelShare(ctx, req)
	if err != nil {
		g.handleGRPCError(c, err)
		return
	}

	g.logger.Info("cancelShare", zap.String("share_id", shareID))
	g.replyProto(c, resp)
}

// getShare GET /api/v1/shares/:shareId — 查询分享信息
func (g *RESTGateway) getShare(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	shareID := c.Param("shareId")

	req := &pb.GetShareRequest{
		ShareId: shareID,
	}

	ctx := c.Request.Context()
	resp, err := g.keyShareClient().GetShare(ctx, req)
	if err != nil {
		g.handleGRPCError(c, err)
		return
	}

	g.logger.Info("getShare", zap.String("share_id", shareID))
	g.replyProto(c, resp)
}

// ── 3. Vehicle Control (2 handlers) ──

// sendCommand POST /api/v1/vehicles/:vehicleId/command — 发送车辆控制指令
func (g *RESTGateway) sendCommand(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	vehicleID := c.Param("vehicleId")
	userID, _ := c.Get("user_id")
	userIDStr := fmt.Sprint(userID)

	var body struct {
		Action  string          `json:"action"`
		KeyID   string          `json:"key_id"`
		Params  json.RawMessage `json:"params"`
		Source  int32           `json:"source"`
		TraceID string          `json:"trace_id"`
	}
	if err := c.ShouldBindJSON(&body); err != nil {
		// [M-06] 统一错误格式
		c.JSON(http.StatusBadRequest, gin.H{"error": "BAD_REQUEST", "message": "invalid request body", "detail": err.Error()})
		return
	}

	req := &pb.ControlCommandRequest{
		VehicleId: vehicleID,
		UserId:    userIDStr,
		KeyId:     body.KeyID,
		Action:    body.Action,
		Params:    []byte(body.Params),
		Source:    body.Source,
		TraceId:   body.TraceID,
	}

	ctx := c.Request.Context()
	resp, err := g.vehicleControlClient().SendCommand(ctx, req)
	if err != nil {
		g.handleGRPCError(c, err)
		return
	}

	g.logger.Info("sendCommand",
		zap.String("vehicle_id", vehicleID),
		zap.String("action", req.Action),
		zap.String("user_id", userIDStr),
	)
	g.replyProto(c, resp)
}

// streamStatus GET /api/v1/vehicles/:vehicleId/status — SSE 流式车辆状态推送
func (g *RESTGateway) streamStatus(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	vehicleID := c.Param("vehicleId")

	req := &pb.VehicleStatusRequest{
		VehicleId: vehicleID,
	}

	ctx := c.Request.Context()
	stream, err := g.vehicleControlClient().StreamStatus(ctx, req)
	if err != nil {
		g.handleGRPCError(c, err)
		return
	}

	// Only set SSE headers after gRPC stream is established successfully
	c.Header("Content-Type", "text/event-stream")
	c.Header("Cache-Control", "no-cache")
	c.Header("Connection", "keep-alive")

	g.logger.Info("streamStatus started",
		zap.String("vehicle_id", vehicleID),
	)

	c.Stream(func(w io.Writer) bool {
		update, err := stream.Recv()
		if err != nil {
			if err == io.EOF || status.Code(err) == codes.Canceled {
				g.logger.Info("streamStatus closed",
					zap.String("vehicle_id", vehicleID),
				)
			} else {
				g.logger.Warn("streamStatus recv error",
					zap.String("vehicle_id", vehicleID),
					zap.Error(err),
				)
			}
			return false
		}

		data, err := protojson.Marshal(update)
		if err != nil {
			g.logger.Error("streamStatus marshal error", zap.Error(err))
			return false
		}

		_, writeErr := fmt.Fprintf(w, "data: %s\n\n", string(data))
		if writeErr != nil {
			g.logger.Warn("streamStatus write error",
				zap.String("vehicle_id", vehicleID),
				zap.Error(writeErr),
			)
			return false
		}
		return true
	})
}

// ── 4. HUB Management (2 handlers) ──

// listAdapters GET /api/v1/hub/adapters — 列出所有适配器状态
func (g *RESTGateway) listAdapters(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	ctx := c.Request.Context()
	req := &pb.HealthCheckRequest{}
	resp, err := g.hubTransportClient().HealthCheck(ctx, req)
	if err != nil {
		g.handleGRPCError(c, err)
		return
	}

	g.logger.Info("listAdapters",
		zap.Int("adapter_count", len(resp.GetAdapters())),
	)

	g.replyProto(c, resp)
}

// hubHealthCheck GET /api/v1/hub/health — HUB健康检查
func (g *RESTGateway) hubHealthCheck(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	ctx := c.Request.Context()
	req := &pb.HealthCheckRequest{}
	resp, err := g.hubTransportClient().HealthCheck(ctx, req)
	if err != nil {
		g.handleGRPCError(c, err)
		return
	}

	g.logger.Info("hubHealthCheck",
		zap.Bool("healthy", resp.GetHealthy()),
	)

	g.replyProto(c, resp)
}
