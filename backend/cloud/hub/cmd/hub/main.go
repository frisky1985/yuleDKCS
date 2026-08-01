package main

import (
	"context"
	"fmt"
	"log"
	"net"
	"os"
	"os/signal"
	"strconv"
	"strings"
	"syscall"
	"time"

	"go.uber.org/zap"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/keepalive"
	"google.golang.org/grpc/reflection"

	pb_relay "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/relay/v1"
	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/adapter"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/adapter/s2s"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/gateway"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/relay"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/service"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/store"
)

func main() {
	logger, _ := zap.NewProduction()
	defer logger.Sync()

	// ── 持久化存储 (PostgreSQL) ──
	dsn := os.Getenv("DATABASE_URL")
	if dsn == "" {
		logger.Fatal("DATABASE_URL must be set — refusing to start without persistence")
	}

	// [P1-2] 失败即关闭: 管理端凭据与 OEM JWKS 至少配置其一, 否则拒绝启动。
	// OEM_JWKS_URLS 先解析再判断 — 裸字符串非空但格式全错时不通过。
	adminConfigured := os.Getenv("ADMIN_USERNAME") != "" && os.Getenv("ADMIN_PASSWORD") != ""
	oemURLs := parseOEMJWKSURLs(os.Getenv("OEM_JWKS_URLS"))
	oemConfigured := len(oemURLs) > 0
	if !adminConfigured && !oemConfigured {
		logger.Fatal("neither ADMIN_USERNAME/ADMIN_PASSWORD nor valid OEM_JWKS_URLS is set — refusing to start without any auth configuration (JWT_SECRET is always required)")
	}
	// [P1-2] 管理员占位密码防御 — 与 JWT_SECRET 弱密钥检查同思路, 拒绝已知占位值
	if adminConfigured {
		adminPass := os.Getenv("ADMIN_PASSWORD")
		for _, dk := range []string{"change-me-admin-password-at-least-16-chars", "admin", "password", "changeme", "admin123"} {
			if adminPass == dk {
				logger.Fatal("ADMIN_PASSWORD must not be a default/placeholder value — refusing to start")
			}
		}
	}

	ctx := context.Background()
	pgStore, err := store.NewPostgresStore(ctx, dsn)
	if err != nil {
		logger.Fatal("failed to init postgres store", zap.Error(err))
	}
	defer pgStore.Close()

	grpcSrv, gw := setupHubGRPCServerWithStores(logger, pgStore, pgStore)

	// [P1-2] JWT_SECRET: 管理端 HS256 令牌签名密钥 (Serve() 中还有空值/弱密钥检查)
	gw.WithJWTSecret(os.Getenv("JWT_SECRET"))

	// [P1-2] OEM_JWKS_URLS: 逗号分隔 key=value, 例如 "oemA=https://a.example.com/jwks,oemB=..."
	if len(oemURLs) > 0 {
		gw.WithOEMJWKS(oemURLs)
		logger.Info("OEM JWKS configured", zap.Int("oem_count", len(oemURLs)))
	}

	// ── TLS (可选) ──
	// K8s 部署中 hub-tls secret 为 optional: 证书未就绪时回退 HTTP 并打 WARN
	// （cert-manager/托管证书签发后自动启用 HTTPS，无需改 manifest）
	if cert := os.Getenv("TLS_CERT_FILE"); cert != "" {
		key := os.Getenv("TLS_KEY_FILE")
		if key == "" {
			logger.Fatal("TLS_CERT_FILE set but TLS_KEY_FILE missing")
		}
		if _, err := os.Stat(cert); err != nil {
			logger.Warn("TLS_CERT_FILE set but cert file not found — serving HTTP until cert is provisioned",
				zap.String("cert", cert), zap.Error(err))
		} else {
			gw.WithTLS(cert, key)
		}
	}

	// ── Start ──
	lis, err := net.Listen("tcp", ":9090")
	if err != nil {
		log.Fatalf("failed to listen: %v", err)
	}

	// [SDK-E2E] REST gateway 转发依赖到自身 gRPC 服务器的客户端连接。
	// 缺失时所有 /api/v1 管理端点 (keys/devices/shares/mailbox) 返回 503 GRPC_UNAVAILABLE。
	// grpc.NewClient 为惰性连接, 可在 Serve 之前创建; 与服务端监听地址保持一致。
	grpcTarget := net.JoinHostPort("localhost", strconv.Itoa(lis.Addr().(*net.TCPAddr).Port))
	grpcConn, err := grpc.NewClient(grpcTarget, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		logger.Fatal("failed to create gRPC client connection", zap.String("target", grpcTarget), zap.Error(err))
	}
	defer grpcConn.Close()
	gw.WithGRPCConn(grpcConn)
	logger.Info("REST gateway gRPC forwarding wired", zap.String("target", grpcTarget))

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
// Uses in-memory stores (no persistence).
func setupHubGRPCServer(logger *zap.Logger) (*grpc.Server, *gateway.RESTGateway) {
	return setupHubGRPCServerWithStores(logger, nil, nil)
}

// setupHubGRPCServerWithStores 与 setupHubGRPCServer 相同，但注入持久化存储。
// keyStore / mailboxStore 为 nil 时使用内存实现（测试默认路径）。
func setupHubGRPCServerWithStores(
	logger *zap.Logger,
	keyStore service.KeyStore,
	mailboxStore relay.MailboxStore,
) (*grpc.Server, *gateway.RESTGateway) {
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
	if mailboxStore != nil {
		mailboxCtrl = mailboxCtrl.WithStore(mailboxStore)
		logger.Info("mailbox store: postgres")
	} else {
		logger.Info("mailbox store: in-memory (dev/test)")
	}
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
	if keyStore != nil {
		keySvc = keySvc.WithKeyStore(keyStore)
		logger.Info("key store: postgres")
	} else {
		logger.Info("key store: in-memory (dev/test)")
	}
	shareSvc := service.NewKeyShareService(adapterRegistry, logger)
	if keyStore != nil {
		shareSvc = shareSvc.WithKeyStore(keyStore)
		if ss, ok := keyStore.(service.ShareStore); ok {
			shareSvc = shareSvc.WithShareStore(ss)
			logger.Info("share store: postgres")
		}
	}
	vehicleSvc := service.NewVehicleControlService(logger)
	if keyStore != nil {
		// 生产: SendCommand 权限校验基于 PG key store
		vehicleSvc = vehicleSvc.WithKeyStore(keyStore)
		logger.Info("vehicle control: key store postgres (permission checks enabled)")
	}
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

// parseOEMJWKSURLs 解析 OEM_JWKS_URLS 环境变量。
// 格式: 逗号分隔的 key=value 对, 例如 "oemA=https://a.example.com/jwks,oemB=https://b.example.com/jwks"。
// 返回 oem_id -> JWKS URL 映射; 格式错误的条目会被跳过。
func parseOEMJWKSURLs(raw string) map[string]string {
	result := make(map[string]string)
	for _, pair := range strings.Split(raw, ",") {
		pair = strings.TrimSpace(pair)
		if pair == "" {
			continue
		}
		kv := strings.SplitN(pair, "=", 2)
		if len(kv) != 2 {
			continue // 跳过格式错误的条目
		}
		key := strings.TrimSpace(kv[0])
		val := strings.TrimSpace(kv[1])
		if key != "" && val != "" {
			result[key] = val
		}
	}
	return result
}
