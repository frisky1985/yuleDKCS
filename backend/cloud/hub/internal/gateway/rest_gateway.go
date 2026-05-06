package gateway

import (
	"context"
	"net/http"
	"time"

	"github.com/gin-gonic/gin"
	"go.uber.org/zap"
	"google.golang.org/grpc"
)

// RESTGateway REST API → gRPC 转换网关
// 手机App通过HTTPS访问，网关转为gRPC调用HUB内部服务
type RESTGateway struct {
	grpcSrv *grpc.Server
	logger  *zap.Logger
	httpSrv *http.Server
}

func NewRESTGateway(grpcSrv *grpc.Server, logger *zap.Logger) *RESTGateway {
	return &RESTGateway{
		grpcSrv: grpcSrv,
		logger:  logger,
	}
}

func (g *RESTGateway) Serve(addr string) error {
	r := gin.New()
	r.Use(gin.Recovery())
	r.Use(g.loggerMiddleware())

	// ── 健康检查 ──
	r.GET("/health", func(c *gin.Context) {
		c.JSON(200, gin.H{"status": "ok"})
	})

	// ── API v1 ──
	v1 := r.Group("/api/v1")
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

// ── Handler stubs (REST → gRPC) ──

func (g *RESTGateway) bindKey(c *gin.Context) {
	// TODO: Parse JSON → gRPC BindKeyRequest → call KeyManagementService.BindKey
	c.JSON(200, gin.H{"message": "bindKey"})
}

func (g *RESTGateway) unbindKey(c *gin.Context) {
	c.JSON(200, gin.H{"message": "unbindKey"})
}

func (g *RESTGateway) suspendKey(c *gin.Context) {
	c.JSON(200, gin.H{"message": "suspendKey"})
}

func (g *RESTGateway) resumeKey(c *gin.Context) {
	c.JSON(200, gin.H{"message": "resumeKey"})
}

func (g *RESTGateway) revokeKey(c *gin.Context) {
	c.JSON(200, gin.H{"message": "revokeKey"})
}

func (g *RESTGateway) renewKey(c *gin.Context) {
	c.JSON(200, gin.H{"message": "renewKey"})
}

func (g *RESTGateway) getKey(c *gin.Context) {
	c.JSON(200, gin.H{"message": "getKey"})
}

func (g *RESTGateway) listKeys(c *gin.Context) {
	c.JSON(200, gin.H{"message": "listKeys"})
}

func (g *RESTGateway) createShare(c *gin.Context) {
	c.JSON(200, gin.H{"message": "createShare"})
}

func (g *RESTGateway) acceptShare(c *gin.Context) {
	c.JSON(200, gin.H{"message": "acceptShare"})
}

func (g *RESTGateway) cancelShare(c *gin.Context) {
	c.JSON(200, gin.H{"message": "cancelShare"})
}

func (g *RESTGateway) getShare(c *gin.Context) {
	c.JSON(200, gin.H{"message": "getShare"})
}

func (g *RESTGateway) sendCommand(c *gin.Context) {
	c.JSON(200, gin.H{"message": "sendCommand"})
}

func (g *RESTGateway) streamStatus(c *gin.Context) {
	c.JSON(200, gin.H{"message": "streamStatus"})
}

func (g *RESTGateway) listAdapters(c *gin.Context) {
	c.JSON(200, gin.H{"message": "listAdapters"})
}

func (g *RESTGateway) hubHealthCheck(c *gin.Context) {
	c.JSON(200, gin.H{"status": "healthy"})
}
