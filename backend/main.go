package main

import (
	"log"

	"github.com/frisky1985/yuleDKCS/backend/cmd/api"
)

func main() {
	// 启动 API 服务器（暂时传递 nil 数据库连接）
	if err := api.Run(nil); err != nil {
		log.Fatalf("Failed to start server: %v", err)
	}
}