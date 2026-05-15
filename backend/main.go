package main

import (
	"database/sql"
	"log"
	"os"

	"gorm.io/driver/postgres"
	"gorm.io/gorm"

	"github.com/frisky1985/yuleDKCS/backend/cmd/api"
	"github.com/frisky1985/yuleDKCS/backend/internal/config"
)

func main() {
	// 加载配置
	cfg, err := config.Load()
	if err != nil {
		log.Fatalf("Failed to load config: %v", err)
	}

	// 初始化数据库连接
	var gormDB *gorm.DB
	dsn := cfg.Database.DSN()
	
	// 检查环境变量，如果设置了 SKIP_DB 则跳过数据库连接
	if os.Getenv("SKIP_DB") == "true" {
		log.Println("Skipping database connection (SKIP_DB=true)")
	} else {
		gormDB, err = gorm.Open(postgres.Open(dsn), &gorm.Config{})
		if err != nil {
			log.Printf("Warning: Failed to connect to database: %v", err)
			log.Println("Starting without database connection...")
		} else {
			log.Println("Database connected successfully")
		}
	}

	// 获取原生数据库连接
	var sqlDB *sql.DB
	if gormDB != nil {
		sqlDB, err = gormDB.DB()
		if err != nil {
			log.Printf("Warning: Failed to get raw DB connection: %v", err)
			sqlDB = nil
		}
	}

	// 启动 API 服务器
	if err := api.Run(sqlDB, gormDB); err != nil {
		log.Fatalf("Failed to start server: %v", err)
	}
}
