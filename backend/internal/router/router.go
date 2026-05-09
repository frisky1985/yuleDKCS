package router

import (
	"github.com/gin-gonic/gin"

	"github.com/frisky1985/yuleDKCS/backend/internal/config"
)

// Setup 初始化路由
func Setup(r *gin.Engine, cfg *config.Config) {
	// 使用 Gin 默认中间件（Logger + Recovery）
	r.Use(gin.Logger())
	r.Use(gin.Recovery())

	// 健康检查
	r.GET("/health", healthCheck)

	// API 路由组
	api := r.Group("/api/v1")
	{
		// 公开路由
		public := api.Group("/")
		{
			public.GET("/ping", ping)
		}

		// 认证路由组
		auth := api.Group("/auth")
		{
			auth.POST("/login", loginHandler)
			auth.POST("/register", registerHandler)
			auth.POST("/refresh", refreshTokenHandler)
		}

		// 需要认证的路由
		authorized := api.Group("/")
		// TODO: 添加 JWT 中间件
		// authorized.Use(middleware.JWTAuth(cfg.JWT.Secret))
		{
			authorized.GET("/user/profile", getUserProfile)
		}
	}

	// WebSocket 路由
	r.GET("/ws", websocketHandler)

	// 404 处理
	r.NoRoute(func(c *gin.Context) {
		c.JSON(404, gin.H{"error": "Not Found"})
	})
}

// healthCheck 健康检查
func healthCheck(c *gin.Context) {
	c.JSON(200, gin.H{
		"status": "ok",
		"time":   gin.H{},
	})
}

// ping 测试接口
func ping(c *gin.Context) {
	c.JSON(200, gin.H{
		"message": "pong",
	})
}

// 处理器函数占位符
func loginHandler(c *gin.Context) {
	c.JSON(501, gin.H{"error": "Not implemented"})
}

func registerHandler(c *gin.Context) {
	c.JSON(501, gin.H{"error": "Not implemented"})
}

func refreshTokenHandler(c *gin.Context) {
	c.JSON(501, gin.H{"error": "Not implemented"})
}

func getUserProfile(c *gin.Context) {
	c.JSON(501, gin.H{"error": "Not implemented"})
}

func websocketHandler(c *gin.Context) {
	c.JSON(501, gin.H{"error": "Not implemented"})
}