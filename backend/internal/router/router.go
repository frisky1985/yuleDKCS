package router

import (
	"database/sql"

	"github.com/gin-gonic/gin"
	"gorm.io/gorm"

	"github.com/frisky1985/yuleDKCS/backend/internal/config"
	"github.com/frisky1985/yuleDKCS/backend/internal/handlers"
	"github.com/frisky1985/yuleDKCS/backend/internal/middleware"
	"github.com/frisky1985/yuleDKCS/backend/internal/repository"
	"github.com/frisky1985/yuleDKCS/backend/internal/services"
	"github.com/frisky1985/yuleDKCS/backend/internal/websocket"
)

// Setup 初始化路由
func Setup(r *gin.Engine, cfg *config.Config, db *sql.DB, gormDB *gorm.DB) {
	// 使用 Gin 默认中间件（Logger + Recovery）
	r.Use(gin.Logger())
	r.Use(gin.Recovery())

	// Prometheus指标中间件
	r.Use(middleware.PrometheusMiddleware())

	// CORS配置
	r.Use(middleware.CORS())

	// 创建健康检查器
	healthChecker := handlers.NewHealthHandler(cfg, gormDB)

	// 健康检查端点
	r.GET("/health", healthChecker.Liveness)
	r.GET("/health/live", healthChecker.Liveness)
	r.GET("/health/ready", healthChecker.Readiness)

	// Prometheus指标端点
	r.GET("/metrics", healthChecker.Metrics())

	// 初始化依赖
	userRepo := repository.NewUserRepository(gormDB)
	vehicleRepo := repository.NewVehicleRepository(gormDB)
	keyRepo := repository.NewKeyRepository(gormDB)

	userService := services.NewUserService(userRepo)
	vehicleService := services.NewVehicleService(vehicleRepo, keyRepo)
	keyService := services.NewKeyService(keyRepo, vehicleRepo, userRepo)

	userHandler := handlers.NewUserHandler(userService)
	vehicleHandler := handlers.NewVehicleHandler(vehicleService)
	keyHandler := handlers.NewKeyHandler(keyService)

	// OTA 服务与处理器
	otaService := services.NewOTAService(gormDB)
	otaHandler := handlers.NewOTAHandler(otaService)

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
			auth.POST("/login", userHandler.Login)
			auth.POST("/register", userHandler.Register)
			auth.POST("/refresh", middleware.RefreshToken)
		}

		// 需要认证的路由
		authorized := api.Group("/")
		authorized.Use(middleware.JWTAuth())
		{
			authorized.GET("/user/profile", userHandler.GetProfile)
			
			// 车辆相关路由
			authorized.POST("/vehicles", vehicleHandler.RegisterVehicle)
			authorized.GET("/vehicles", vehicleHandler.GetUserVehicles)
			authorized.GET("/vehicles/:id", vehicleHandler.GetVehicle)
			authorized.GET("/vehicles/:id/status", vehicleHandler.GetVehicleStatus)
			authorized.POST("/vehicles/:id/commands", vehicleHandler.SendCommand)
			authorized.GET("/vehicles/:id/commands/:command_id", vehicleHandler.GetCommandStatus)
			authorized.PUT("/vehicles/:id/location", vehicleHandler.UpdateLocation)
			authorized.POST("/vehicles/:id/heartbeat", vehicleHandler.Heartbeat)
			
			// 钥匙相关路由
			keyHandler.RegisterRoutes(authorized)

			// OTA 相关路由
			otaHandler.RegisterRoutes(authorized)
		}
	}

	// WebSocket 路由
	// 初始化 WebSocket Hub
	wsHub := websocket.NewHub()
	go wsHub.Run()
	wsHandler := handlers.NewWebSocketHandler(wsHub)
	r.GET("/ws", middleware.JWTAuth(), wsHandler.HandleWebSocket)

	// 404 处理
	r.NoRoute(func(c *gin.Context) {
		c.JSON(404, gin.H{"code": 404, "message": "接口不存在"})
	})
}

// 处理器函数占位符
func ping(c *gin.Context) {
	c.JSON(200, gin.H{
		"code":    200,
		"message": "pong",
		"service": "yuleDKCS",
		"version": "1.0.0",
	})
}