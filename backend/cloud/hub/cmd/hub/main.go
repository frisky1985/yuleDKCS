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
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/adapter/s2s"
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

	// CCC Adapter + Mailbox middleware
	mailboxCtrl := relay.NewMailboxController(logger)
	mailboxBridge := &mailboxCreatorBridge{ctrl: mailboxCtrl}

	cccApple := adapter.NewCCCAdapter("apple", logger).WithMailboxCreator(mailboxBridge)
	adapterRegistry.Register("apple", "ccc_dk3", cccApple)

	cccSamsung := adapter.NewCCCAdapter("samsung", logger).WithMailboxCreator(mailboxBridge)
	adapterRegistry.Register("samsung", "ccc_dk3", cccSamsung)

	// ICCOA Adapter (小米/OPPO/vivo) — S2S by env, stub otherwise
	registerICCOAAdapter(adapterRegistry, "xiaomi", logger)
	registerICCOAAdapter(adapterRegistry, "oppo", logger)
	registerICCOAAdapter(adapterRegistry, "vivo", logger)

	// ICCE Adapter (华为) — S2S by env, stub otherwise
	registerICCEAdapter(adapterRegistry, "huawei", logger)

	// ── Services ──
	keySvc := service.NewKeyManagementService(adapterRegistry, logger)
	shareSvc := service.NewKeyShareService(adapterRegistry, logger)
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

// mailboxCreatorBridge wraps relay.MailboxController to implement adapter.MailboxCreator.
// Used by CCCAdapter to create Mailboxes during ShareKey.
type mailboxCreatorBridge struct {
	ctrl *relay.MailboxController
}

func (b *mailboxCreatorBridge) CreateMailbox(ctx context.Context, keyID, senderVendor, senderDeviceID, traceID string) (string, string, error) {
	req := &pb_relay.CreateMailboxRequest{
		SenderVendor:   senderVendor,
		SenderDeviceId: senderDeviceID,
		TraceId:        traceID,
	}
	mb, err := b.ctrl.Create(ctx, req)
	if err != nil {
		return "", "", err
	}
	return mb.MailboxId, mb.SharingUrl, nil
}

// ─── S2S Adapter 注册辅助函数 ──────────────────────────────

// registerICCOAAdapter 注册 ICCOA 适配器，环境变量配置 S2S 客户端时启用
// 环境变量: ICCOA_{VENDOR}_BASE_URL, ICCOA_{VENDOR}_VEHICLE_OEM, ICCOA_{VENDOR}_DEVICE_OEM
func registerICCOAAdapter(reg *adapter.Registry, vendor string, logger *zap.Logger) {
	envPrefix := "ICCOA_" + vendorUpper(vendor)

	baseURL := os.Getenv(envPrefix + "_BASE_URL")
	if baseURL == "" {
		logger.Info("ICCOA S2S not configured, using stub",
			zap.String("vendor", vendor),
			zap.String("env", envPrefix+"_BASE_URL"),
		)
		reg.Register(vendor, "iccoa_dk40", adapter.NewICCOAAdapter(vendor, logger))
		return
	}

	config := s2s.NewDefaultICCOAConfig(
		vendor,
		baseURL,
		os.Getenv(envPrefix+"_VEHICLE_OEM"),
		os.Getenv(envPrefix+"_DEVICE_OEM"),
	)
	client := s2s.NewICCOAClient(vendor, config, logger)

	reg.Register(vendor, "iccoa_dk40", adapter.NewICCOAAdapterWithClient(vendor, logger, client))
	logger.Info("ICCOA S2S client enabled",
		zap.String("vendor", vendor),
		zap.String("base_url", baseURL),
	)
}

// registerICCEAdapter 注册 ICCE 适配器，环境变量配置 S2S 客户端时启用
// 环境变量: ICCE_{VENDOR}_BASE_URL
func registerICCEAdapter(reg *adapter.Registry, vendor string, logger *zap.Logger) {
	envPrefix := "ICCE_" + vendorUpper(vendor)

	baseURL := os.Getenv(envPrefix + "_BASE_URL")
	if baseURL == "" {
		logger.Info("ICCE S2S not configured, using stub",
			zap.String("vendor", vendor),
			zap.String("env", envPrefix+"_BASE_URL"),
		)
		reg.Register(vendor, "icce", adapter.NewICCEAdapter(vendor, logger))
		return
	}

	endpoint := s2s.DefaultICCEConfig()
	endpoint.BaseURL = baseURL
	client := s2s.NewICCEClient(vendor, endpoint, logger)

	reg.Register(vendor, "icce", adapter.NewICCEAdapterWithClient(vendor, logger, client))
	logger.Info("ICCE S2S client enabled",
		zap.String("vendor", vendor),
		zap.String("base_url", baseURL),
	)
}

// vendorUpper 将厂商名转为环境变量格式（xiaomi → XIAOMI）
func vendorUpper(v string) string {
	b := []byte(v)
	for i := range b {
		if b[i] >= 'a' && b[i] <= 'z' {
			b[i] -= 32
		}
	}
	return string(b)
}
