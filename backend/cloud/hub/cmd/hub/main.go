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

	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
	pb_relay "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/relay/v1"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/adapter"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/gateway"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/relay"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/service"
)

func main() {
	logger, _ := zap.NewProduction()
	defer logger.Sync()

	grpcSrv, gw := setupHubGRPCServer(logger)

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

// setupHubGRPCServer creates and configures the gRPC server with all adapters and services.
// Extracted for testability — returns the gRPC server and REST gateway.
func setupHubGRPCServer(logger *zap.Logger) (*grpc.Server, *gateway.RESTGateway) {
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
	mailboxCtrl := relay.NewMailboxController(logger)
	shareSvc := service.NewKeyShareService(adapterRegistry, mailboxCtrl, logger)
	vehicleSvc := service.NewVehicleControlService(logger)
	transportSvc := service.NewHubTransportService(adapterRegistry, logger)

	pb.RegisterKeyManagementServiceServer(grpcSrv, keySvc)
	pb.RegisterKeyShareServiceServer(grpcSrv, shareSvc)
	pb.RegisterVehicleControlServiceServer(grpcSrv, vehicleSvc)
	pb.RegisterHubTransportServiceServer(grpcSrv, transportSvc)

	// ── Relay Server (CCC Mailbox API) ──
	// 构建 Push 通知链（环境变量配置，无配置则走 NoopPusher）
	var pushNotifiers []relay.PushNotifier
	if projectID := os.Getenv("FCM_PROJECT_ID"); projectID != "" {
		fcmPusher := relay.NewFCMPusher(relay.FCMConfig{ProjectID: projectID})
		pushNotifiers = append(pushNotifiers, fcmPusher)
		logger.Info("FCM push enabled", zap.String("project", projectID))
	}
	if apnsKeyID := os.Getenv("APNS_KEY_ID"); apnsKeyID != "" {
		apnsPusher, err := relay.NewAPNsPusher(relay.APNsConfig{
			KeyID:      apnsKeyID,
			TeamID:     os.Getenv("APNS_TEAM_ID"),
			BundleID:   os.Getenv("APNS_BUNDLE_ID"),
			AuthKey:    os.Getenv("APNS_AUTH_KEY"),
			Production: os.Getenv("APNS_ENV") == "production",
		})
		if err != nil {
			logger.Warn("APNs pusher init failed, skipping", zap.Error(err))
		} else {
			pushNotifiers = append(pushNotifiers, apnsPusher)
			logger.Info("APNs push enabled", zap.String("key_id", apnsKeyID))
		}
	}

	var pushNotifier relay.PushNotifier
	switch len(pushNotifiers) {
	case 0:
		pushNotifier = &relay.NoopPusher{}
	case 1:
		pushNotifier = pushNotifiers[0]
	default:
		pushNotifier = relay.NewCompositePusher(pushNotifiers...)
	}

	relaySvc := relay.NewRelayService(logger, pushNotifier)
	pb_relay.RegisterRelayServiceServer(grpcSrv, relaySvc)

	// ── Gateway (REST -> gRPC) ──
	gw := gateway.NewRESTGateway(grpcSrv, logger)

	reflection.Register(grpcSrv)

	return grpcSrv, gw
}
