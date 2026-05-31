package gateway

import (
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
	"go.uber.org/zap"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
)

// RESTGateway REST API → gRPC 转换网关
// 手机App通过HTTPS访问，网关转为gRPC调用HUB内部服务
type RESTGateway struct {
	grpcSrv   *grpc.Server
	logger    *zap.Logger
	httpSrv   *http.Server
	jwtSecret string
	grpcConn  *grpc.ClientConn
}

func NewRESTGateway(grpcSrv *grpc.Server, logger *zap.Logger) *RESTGateway {
	return &RESTGateway{
		grpcSrv: grpcSrv,
		logger:  logger,
	}
}

// WithJWTSecret sets the JWT secret for auth middleware
func (g *RESTGateway) WithJWTSecret(secret string) *RESTGateway {
	g.jwtSecret = secret
	return g
}

// WithGRPCConn sets a gRPC client connection for forwarding to HUB services
func (g *RESTGateway) WithGRPCConn(conn *grpc.ClientConn) *RESTGateway {
	g.grpcConn = conn
	return g
}

func (g *RESTGateway) Serve(addr string) error {
	r := gin.New()
	r.Use(gin.Recovery())
	r.Use(g.loggerMiddleware())

	// ── 健康检查 (公开) ──
	r.GET("/health", func(c *gin.Context) {
		c.JSON(200, gin.H{"status": "ok"})
	})

	r.POST("/api/v1/auth/login", g.login) // 登录获取token

	// ── API v1 (需要认证) ──
	v1 := r.Group("/api/v1")
	v1.Use(g.authMiddleware())
	{
		// 密钥管理
		keys := v1.Group("/keys")
		{
			keys.POST("", g.bindKey)          // 绑定密钥
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
	}

	g.httpSrv = &http.Server{
		Addr:         addr,
		Handler:      r,
		ReadTimeout:  10 * time.Second,
		WriteTimeout: 30 * time.Second,
	}

	return g.httpSrv.ListenAndServe()
}

func (g *RESTGateway) Shutdown(ctx context.Context) error {
	if g.httpSrv != nil {
		return g.httpSrv.Shutdown(ctx)
	}
	return nil
}

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

// authMiddleware validates JWT Bearer token from Authorization header
func (g *RESTGateway) authMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		authHeader := c.GetHeader("Authorization")
		if authHeader == "" {
			c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{"error": "missing authorization header"})
			return
		}

		token := strings.TrimPrefix(authHeader, "Bearer ")
		if token == "" || token == authHeader {
			c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{"error": "invalid token format"})
			return
		}

		// Simple JWT-like validation (placeholder for full JWT lib)
		// In production, replace with github.com/golang-jwt/jwt/v5 validation
		userID, role, err := g.validateToken(token)
		if err != nil {
			g.logger.Warn("Auth failed", zap.String("token_prefix", token[:min(8, len(token))]), zap.Error(err))
			c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{"error": "invalid or expired token"})
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

// validateToken validates a bearer token and returns (userID, role, error)
// Placeholder: in production, decode and verify JWT with jwtSecret
func (g *RESTGateway) validateToken(token string) (string, string, error) {
	// TODO: Replace with proper JWT validation
	if token == "" {
		return "", "", status.Error(codes.Unauthenticated, "empty token")
	}
	// For now, use token as user_id placeholder
	// TODO: JWT claims extraction
	return "user:" + token[:min(16, len(token))], "user", nil
}

// login provides a simple token endpoint (placeholder)
func (g *RESTGateway) login(c *gin.Context) {
	var req struct {
		UserID   string `json:"user_id"`
		Password string `json:"password"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid request"})
		return
	}

	// TODO: Validate credentials against user service
	// TODO: Generate proper JWT with expiry
	token := "placeholder-token-" + req.UserID + "-" + time.Now().Format(time.RFC3339)
	c.JSON(http.StatusOK, gin.H{
		"token":      token,
		"token_type": "Bearer",
		"expires_in": 3600,
	})
}

// ── REST → gRPC forwarding handlers ──

func (g *RESTGateway) bindKey(c *gin.Context) {
	userID, _ := c.Get("user_id")
	g.logger.Info("bindKey", zap.String("user_id", fmt.Sprint(userID)))
	c.JSON(http.StatusOK, gin.H{"message": "bindKey", "user_id": userID})
}

func (g *RESTGateway) unbindKey(c *gin.Context) {
	keyID := c.Param("keyId")
	c.JSON(http.StatusOK, gin.H{"message": "unbindKey", "key_id": keyID})
}

func (g *RESTGateway) suspendKey(c *gin.Context) {
	keyID := c.Param("keyId")
	c.JSON(http.StatusOK, gin.H{"message": "suspendKey", "key_id": keyID})
}

func (g *RESTGateway) resumeKey(c *gin.Context) {
	keyID := c.Param("keyId")
	c.JSON(http.StatusOK, gin.H{"message": "resumeKey", "key_id": keyID})
}

func (g *RESTGateway) revokeKey(c *gin.Context) {
	keyID := c.Param("keyId")
	c.JSON(http.StatusOK, gin.H{"message": "revokeKey", "key_id": keyID})
}

func (g *RESTGateway) renewKey(c *gin.Context) {
	keyID := c.Param("keyId")
	c.JSON(http.StatusOK, gin.H{"message": "renewKey", "key_id": keyID})
}

func (g *RESTGateway) getKey(c *gin.Context) {
	keyID := c.Param("keyId")
	c.JSON(http.StatusOK, gin.H{"message": "getKey", "key_id": keyID})
}

func (g *RESTGateway) listKeys(c *gin.Context) {
	c.JSON(http.StatusOK, gin.H{"message": "listKeys"})
}

func (g *RESTGateway) createShare(c *gin.Context) {
	c.JSON(http.StatusOK, gin.H{"message": "createShare"})
}

func (g *RESTGateway) acceptShare(c *gin.Context) {
	c.JSON(http.StatusOK, gin.H{"message": "acceptShare"})
}

func (g *RESTGateway) cancelShare(c *gin.Context) {
	c.JSON(http.StatusOK, gin.H{"message": "cancelShare"})
}

func (g *RESTGateway) getShare(c *gin.Context) {
	c.JSON(http.StatusOK, gin.H{"message": "getShare"})
}

func (g *RESTGateway) sendCommand(c *gin.Context) {
	c.JSON(http.StatusOK, gin.H{"message": "sendCommand"})
}

func (g *RESTGateway) streamStatus(c *gin.Context) {
	c.JSON(http.StatusOK, gin.H{"message": "streamStatus"})
}

func (g *RESTGateway) listAdapters(c *gin.Context) {
	c.JSON(http.StatusOK, gin.H{"message": "listAdapters"})
}

func (g *RESTGateway) hubHealthCheck(c *gin.Context) {
	c.JSON(http.StatusOK, gin.H{"status": "healthy"})
}
