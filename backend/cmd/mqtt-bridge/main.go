package main

import (
	"context"
	"log"
	"os"
	"os/signal"
	"syscall"

	"yuledkcs/internal/mqttbridge"
)

func main() {
	log.Println("启动 MQTT Bridge 服务...")

	// 创建 bridge
	bridge, err := mqttbridge.NewBridge()
	if err != nil {
		log.Fatalf("创建 MQTT Bridge 失败: %v", err)
	}

	// 启动 bridge
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	go func() {
		if err := bridge.Start(ctx); err != nil {
			log.Fatalf("MQTT Bridge 启动失败: %v", err)
		}
	}()

	log.Println("✅ MQTT Bridge 服务已启动")
	log.Println("等待连接...")

	// 等待中断信号
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)

	<-sigChan
	log.Println("正在关闭 MQTT Bridge...")

	// 停止 bridge
	if err := bridge.Stop(); err != nil {
		log.Printf("关闭 MQTT Bridge 时发生错误: %v", err)
	}

	log.Println("✅ MQTT Bridge 已停止")
}
