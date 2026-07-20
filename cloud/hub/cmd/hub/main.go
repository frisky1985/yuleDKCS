package main

import (
	"context"
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

	pb "github.com/digitalkey/hub/api/v1"
	"github.com/digitalkey/hub/internal/adapter"
	"github.com/digitalkey/hub/internal/gateway"
	"github.com/digitalkey/hub/internal/service"
)

func main() {
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
