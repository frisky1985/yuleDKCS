package main

import (
	"context"
	"fmt"
	"net"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/jmoiron/sqlx"
	_ "github.com/lib/pq"
	"github.com/redis/go-redis/v9"
	"google.golang.org/grpc"
	"google.golang.org/grpc/health"
	healthpb "google.golang.org/grpc/health/grpc_health_v1"
	"google.golang.org/grpc/reflection"

	pb "github.com/digitalkey/proto/dkcs"
	"github.com/digitalkey/dkcs/internal/cache"
	"github.com/digitalkey/dkcs/internal/config"
	"github.com/digitalkey/dkcs/internal/middleware"
	"github.com/digitalkey/dkcs/internal/mq"
	"github.com/digitalkey/dkcs/internal/repository"
	"github.com/digitalkey/dkcs/internal/service"
	"github.com/digitalkey/dkcs/pkg/logger"
	"github.com/digitalkey/dkcs/pkg/telemetry"
)

func main() {
	// Load configuration
	cfg := config.Load()

	// Initialize logger
	log := logger.NewLogger(&logger.Config{
		Level:  cfg.Log.Level,
		Format: cfg.Log.Format,
		Output: cfg.Log.Output,
	})
	defer log.Sync()

	log.Info("Starting DKCS (Digital Key Control Service)...")

	// Create context with cancellation
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	// Initialize database connection
	db, err := initDatabase(cfg.Database)
	if err != nil {
		log.Fatal("Failed to connect to database", logger.Err(err))
	}
	defer db.Close()

	log.Info("Database connection established")

	// Initialize Redis client
	redisClient := initRedis(cfg.Redis)
	if err := redisClient.Ping(ctx).Err(); err != nil {
		log.Fatal("Failed to connect to Redis", logger.Err(err))
	}
	defer redisClient.Close()

	log.Info("Redis connection established")

	// Initialize cache
	redisCache := cache.NewRedisCacheFromClient(redisClient)

	// Initialize Kafka producer
	kafkaProducer, err := mq.NewKafkaProducer(mq.KafkaConfig{
		Brokers: cfg.Kafka.Brokers,
		Topic:   cfg.Kafka.Topics.KeyEvents,
	})
	if err != nil {
		log.Warn("Kafka producer not available, running in standalone mode", logger.Err(err))
	} else {
		defer kafkaProducer.Close()
		log.Info("Kafka producer initialized")
	}

	// Initialize telemetry
	telemetry, err := telemetry.NewTelemetry(&telemetry.Config{
		Enabled: cfg.Metrics.Enabled,
		Port:    cfg.Metrics.Port,
		Path:    cfg.Metrics.Path,
	})
	if err != nil {
		log.Warn("Telemetry initialization failed", logger.Err(err))
	}

	// Initialize repositories
	keyRepo := repository.NewKeyRepository(db, redisClient)
	vehicleRepo := repository.NewVehicleRepository(db, redisClient)
	eventRepo := repository.NewEventRepository(db)

	// Initialize services
	eventService := service.NewEventService(eventRepo, log, telemetry)
	keyService := service.NewKeyService(keyRepo, vehicleRepo, log, telemetry)
	commandService := service.NewCommandService(keyRepo, vehicleRepo, log, telemetry, eventService)

	// Create gRPC server with interceptors
	grpcServer := grpc.NewServer(
		grpc.UnaryInterceptor(
			middleware.ChainInterceptors(
				middleware.RecoveryInterceptor(log),
				middleware.LoggingInterceptor(log),
				middleware.MetricsInterceptor(telemetry),
				middleware.AuthInterceptor(cfg.JWT.Secret),
				middleware.RateLimitInterceptor(1000),
				middleware.TimeoutInterceptor(30*time.Second),
			),
		),
	)

	// Register services
	pb.RegisterKeyServiceServer(grpcServer, keyService)
	pb.RegisterCommandServiceServer(grpcServer, commandService)
	pb.RegisterEventServiceServer(grpcServer, eventService)

	// Register health check
	healthServer := health.NewServer()
	healthpb.RegisterHealthServer(grpcServer, healthServer)

	// Enable reflection for debugging
	reflection.Register(grpcServer)

	// Start gRPC server
	lis, err := net.Listen("tcp", fmt.Sprintf(":%d", cfg.Server.GRPCPort))
	if err != nil {
		log.Fatal("Failed to listen on gRPC port", logger.Err(err), logger.Int("port", cfg.Server.GRPCPort))
	}

	go func() {
		log.Info("gRPC server listening", logger.Int("port", cfg.Server.GRPCPort))
		if err := grpcServer.Serve(lis); err != nil {
			log.Error("gRPC server error", logger.Err(err))
			cancel()
		}
	}()

	// Set health status
	healthServer.SetServingStatus("dkcs", healthpb.HealthCheckResponse_SERVING)

	log.Info("DKCS started successfully",
		logger.Int("grpc_port", cfg.Server.GRPCPort),
		logger.Int("metrics_port", cfg.Metrics.Port),
	)

	// Wait for shutdown signal
	sigCh := make(chan os.Signal, 1)
	signal.Notify(sigCh, syscall.SIGINT, syscall.SIGTERM)

	select {
	case <-sigCh:
		log.Info("Shutdown signal received")
	case <-ctx.Done():
		log.Info("Context cancelled")
	}

	// Graceful shutdown
	log.Info("Initiating graceful shutdown...")

	// Set health status to not serving
	healthServer.SetServingStatus("dkcs", healthpb.HealthCheckResponse_NOT_SERVING)

	// Stop gRPC server
	grpcServer.GracefulStop()

	log.Info("DKCS shutdown complete")
}

// initDatabase initializes database connection
func initDatabase(cfg config.DatabaseConfig) (*sqlx.DB, error) {
	db, err := sqlx.Connect("postgres", cfg.DSN())
	if err != nil {
		return nil, fmt.Errorf("failed to connect to database: %w", err)
	}

	// Configure connection pool
	db.SetMaxOpenConns(cfg.MaxOpenConns)
	db.SetMaxIdleConns(cfg.MaxIdleConns)
	db.SetConnMaxLifetime(cfg.ConnMaxLifetime)

	return db, nil
}

// initRedis initializes Redis client
func initRedis(cfg config.RedisConfig) *redis.Client {
	return redis.NewClient(&redis.Options{
		Addr:     cfg.Addr,
		Password: cfg.Password,
		DB:       cfg.DB,
	})
}
