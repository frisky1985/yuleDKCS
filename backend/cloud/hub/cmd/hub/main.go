package main

import (
	"context"
	"flag"
	"fmt"
	"log"
	"net"
	"os"
	"os/signal"
	"syscall"
	"time"

	"go.uber.org/zap"
	"google.golang.org/grpc"
	"google.golang.org/grpc/keepalive"
	"google.golang.org/grpc/reflection"

	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/adapter"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/gateway"
	hub_logger "github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/logger"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/service"
)

func main() {
	// ── 命令行参数 ──
	logLevel := flag.String("log-level", "info", "日志级别: trace/debug/info/warn/error/fatal")
	logFile := flag.String("log-file", "", "日志输出文件 (默认 stderr)")
	flag.Parse()

	// ── 初始化内部日志系统 ──
	{
		lvl, err := hub_logger.ParseLevel(*logLevel)
		if err != nil {
			log.Fatalf("invalid --log-level: %v", err)
		}
		cfg := hub_logger.LoggerConfig{
			ServiceName: "hub",
			Level:       lvl,
			EnableJSON:  true,
		}
		if *logFile != "" {
			f, err := os.OpenFile(*logFile, os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0644)
			if err != nil {
				log.Fatalf("open log file %q failed: %v", *logFile, err)
			}
			cfg.Output = f
		}
		hub_logger.Init(cfg)
		hub_logger.Info("HUB", "logger initialized",
			hub_logger.WithField("level", *logLevel),
			hub_logger.WithField("log_file", *logFile))
	}

	logger, _ := zap.NewProduction()
	defer logger.Sync()

	// ── gRPC Server ──
	kaParams := keepalive.ServerParameters{
		MaxConnectionIdle:     5 * time.Minute,
		MaxConnectionAge:      30 * time.Minute,
		MaxConnectionAgeGrace: 10 * time.Second,
		Time:                  30 * time.Second,
		Timeout:               10 * time.Second,
	}

	grpcSrv := grpc.NewServer(
		grpc.KeepaliveParams(kaParams),
		grpc.MaxRecvMsgSize(4*1024*1024),
		grpc.MaxSendMsgSize(4*1024*1024),
	)

	// ── Adapters ──
	adapterRegistry := adapter.NewRegistry(logger)
	adapterRegistry.Register("apple", "ccc_dk3", adapter.NewCCCAdapter("apple", logger))
	adapterRegistry.Register("samsung", "ccc_dk3", adapter.NewCCCAdapter("samsung", logger))
	adapterRegistry.Register("xiaomi", "iccoa_dk40", adapter.NewICCOAAdapter("xiaomi", logger))
	adapterRegistry.Register("oppo", "iccoa_dk40", adapter.NewICCOAAdapter("oppo", logger))
	adapterRegistry.Register("vivo", "iccoa_dk40", adapter.NewICCOAAdapter("vivo", logger))
	adapterRegistry.Register("huawei", "icce", adapter.NewICCEAdapter("huawei", logger))

	// ── Services ──
	keySvc := service.NewKeyManagementService(adapterRegistry, logger)
	shareSvc := service.NewKeyShareService(adapterRegistry, logger)
	vehicleSvc := service.NewVehicleControlService(logger)
	transportSvc := service.NewHubTransportService(adapterRegistry, logger)

	pb.RegisterKeyManagementServiceServer(grpcSrv, keySvc)
	pb.RegisterKeyShareServiceServer(grpcSrv, shareSvc)
	pb.RegisterVehicleControlServiceServer(grpcSrv, vehicleSvc)
	pb.RegisterHubTransportServiceServer(grpcSrv, transportSvc)

	// ── Gateway (REST -> gRPC) ──
	gw := gateway.NewRESTGateway(grpcSrv, logger)

	reflection.Register(grpcSrv)

	// ── Start ──
	lis, err := net.Listen("tcp", ":9090")
	if err != nil {
		log.Fatalf("failed to listen: %v", err)
	}

	go func() {
		logger.Info("HUB gRPC server starting", zap.String("addr", ":9090"))
		if err := grpcSrv.Serve(lis); err != nil {
			logger.Fatal("gRPC serve failed", zap.Error(err))
		}
	}()

	go func() {
		logger.Info("HUB REST gateway starting", zap.String("addr", ":8080"))
		if err := gw.Serve(":8080"); err != nil {
			logger.Fatal("REST gateway failed", zap.Error(err))
		}
	}()

	// ── Graceful shutdown ──
	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	logger.Info("shutting down...")
	grpcSrv.GracefulStop()
	gw.Shutdown(context.Background())
	fmt.Println("HUB stopped")
}
