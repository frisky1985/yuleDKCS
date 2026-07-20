package main

import (
	"context"
	"log"
	"net"
	"os"
	"os/signal"
	"syscall"
	"time"

	"go.uber.org/zap"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials"

	pb "github.com/digitalkey/dkcs/api/v1"
	"github.com/digitalkey/dkcs/internal/keymgmt"
	"github.com/digitalkey/dkcs/internal/tsp"
	"github.com/digitalkey/dkcs/internal/device"
)

func main() {
	logger, _ := zap.NewProduction()
	defer logger.Sync()

	// ── HUB连接 (mTLS) ──
	creds, err := credentials.NewClientTLSFromFile("certs/hub-ca.pem", "hub.digitalkey.local")
	if err != nil {
		log.Fatalf("failed to load HUB TLS creds: %v", err)
	}

	hubConn, err := grpc.Dial("hub.digitalkey.local:9090",
		grpc.WithTransportCredentials(creds),
		grpc.WithBlock(),
		grpc.WithTimeout(10*time.Second),
	)
	if err != nil {
		log.Fatalf("failed to connect to HUB: %v", err)
	}
	defer hubConn.Close()

	hubClient := pb.NewHubTransportServiceClient(hubConn)

	// ── Services ──
	keySvc := keymgmt.NewService(hubClient, logger)
	tspSvc := tsp.NewService(logger)
	deviceSvc := device.NewService(logger)

	// ── gRPC Server (车厂内网) ──
	serverCreds, err := credentials.NewServerTLSFromFile("certs/server.pem", "certs/server-key.pem")
	if err != nil {
		log.Fatalf("failed to load server TLS creds: %v", err)
	}

	grpcSrv := grpc.NewServer(grpc.Creds(serverCreds))

	pb.RegisterKeyManagementServiceServer(grpcSrv, keySvc)
	pb.RegisterVehicleControlServiceServer(grpcSrv, tspSvc)
	pb.RegisterDeviceManagementServiceServer(grpcSrv, deviceSvc)

	lis, err := net.Listen("tcp", ":9091")
	if err != nil {
		log.Fatalf("failed to listen: %v", err)
	}

	go func() {
		logger.Info("DKCS server starting", zap.String("addr", ":9091"))
		if err := grpcSrv.Serve(lis); err != nil {
			logger.Fatal("DKCS serve failed", zap.Error(err))
		}
	}()

	// ── MQTT Client (连接TCU) ──
	// mqttClient := mqtt.NewClient("tcp://mqtt-broker:1883", "dkcs", logger)
	// mqttClient.Subscribe("vehicle/+/status")
	// mqttClient.Subscribe("vehicle/+/command/response")

	// ── Graceful shutdown ──
	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	logger.Info("DKCS shutting down...")
	grpcSrv.GracefulStop()
}
