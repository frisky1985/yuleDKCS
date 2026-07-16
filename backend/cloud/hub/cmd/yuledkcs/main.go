package main

import (
	"flag"
	"fmt"
	"log"
	"net"
	"os"
	"os/signal"
	"syscall"

	"go.uber.org/zap"
	"google.golang.org/grpc"

	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/gateway"
	hub_logger "github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/logger"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/service"
)

func main() {
	// ── 启动参数 ──
	mode := flag.String("mode", "all-in-one", "启动模式: all-in-one | hub-only | server-only")
	httpAddr := flag.String("http-addr", ":8080", "REST API 监听地址")
	grpcAddr := flag.String("grpc-addr", ":9090", "gRPC 监听地址")
	jwtSecret := flag.String("jwt-secret", "", "JWT 签名密钥 (建议从环境变量读取)")
	dbDSN := flag.String("db-dsn", "", "数据库连接串 (可选)")
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
			ServiceName: "yuledkcs",
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
		hub_logger.Info("INIT", "logger initialized",
			hub_logger.WithField("level", *logLevel),
			hub_logger.WithField("log_file", *logFile))
	}

	// ── 日志 ──
	logger, err := zap.NewProduction()
	if err != nil {
		log.Fatalf("init logger failed: %v", err)
	}
	defer logger.Sync()

	logger.Info("yuleDKCS starting",
		zap.String("mode", *mode),
		zap.String("http_addr", *httpAddr),
		zap.String("grpc_addr", *grpcAddr),
	)

	// ── 密钥 ──
	secret := *jwtSecret
	if secret == "" {
		secret = os.Getenv("JWT_SECRET")
	}

	// ── 组件初始化 ──
	deviceSvc := service.NewDeviceService(logger)

	switch *mode {
	case "hub-only":
		startHubOnly(logger, *httpAddr, *grpcAddr, secret, deviceSvc)

	case "server-only":
		startServerOnly(logger, *grpcAddr, *dbDSN, deviceSvc)

	case "all-in-one":
		fallthrough
	default:
		startAllInOne(logger, *httpAddr, *grpcAddr, secret, *dbDSN, deviceSvc)
	}
}

// startHubOnly 只启动编排层（Hub），密钥材料层通过 gRPC 外连车厂 DK Server
func startHubOnly(logger *zap.Logger, httpAddr, grpcAddr, secret string, deviceSvc *service.DeviceService) {
	logger.Info("mode: hub-only — 编排层独立运行")

	// TODO: 通过 gRPC 连接车厂的 DK Server
	// conn, err := grpc.Dial(dkServerAddr, grpc.WithInsecure())
	// dkClient := pb.NewDKServerClient(conn)

	hub := gateway.NewRESTGateway(nil, logger)
	hub.WithJWTSecret(secret)

	logger.Info("Hub ready", zap.String("http", httpAddr))
	if err := hub.Serve(httpAddr); err != nil {
		logger.Fatal("hub serve failed", zap.Error(err))
	}
}

// startServerOnly 只启动密钥材料层（DK Server），接受 Hub 的 gRPC 请求
func startServerOnly(logger *zap.Logger, grpcAddr, dbDSN string, deviceSvc *service.DeviceService) {
	logger.Info("mode: server-only — 密钥材料层独立运行")

	_ = dbDSN
	_ = deviceSvc

	lis, err := net.Listen("tcp", grpcAddr)
	if err != nil {
		logger.Fatal("grpc listen failed", zap.String("addr", grpcAddr), zap.Error(err))
	}

	srv := grpc.NewServer()
	dkSrv := service.NewGRPCDKServer()
	dkSrv.RegisterGRPCServer(srv)

	logger.Info("DK Server gRPC listening", zap.String("addr", grpcAddr))
	if err := srv.Serve(lis); err != nil {
		logger.Fatal("grpc serve failed", zap.Error(err))
	}
}

// startAllInOne Hub + DK Server 同进程部署（当前默认模式）
func startAllInOne(logger *zap.Logger, httpAddr, grpcAddr, secret, dbDSN string, deviceSvc *service.DeviceService) {
	logger.Info("mode: all-in-one — Hub + DK Server 同进程")

	_ = dbDSN

	// 初始化 Hub 网关（编排层）
	hub := gateway.NewRESTGateway(nil, logger)
	hub.WithJWTSecret(secret)

	// Hub + Server 同进程：内部通过 Go 函数调用，零网络开销
	// 当前 backend/cloud/hub/ 和 backend/dkcs/ 都直接可用

	// ── 等待退出信号 ──
	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)

	go func() {
		logger.Info(fmt.Sprintf("yuleDKCS listening on %s", httpAddr))
		if err := hub.Serve(httpAddr); err != nil {
			logger.Fatal("hub serve failed", zap.Error(err))
		}
	}()

	sig := <-quit
	logger.Info("shutting down", zap.String("signal", sig.String()))
}
