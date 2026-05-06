package middleware

import (
	"context"
	"strings"
	"time"

	"github.com/digitalkey/dkcs/pkg/logger"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
)

// AuthInterceptor validates authentication tokens
func AuthInterceptor(jwtSecret string) grpc.UnaryServerInterceptor {
	return func(ctx context.Context, req interface{}, info *grpc.UnaryServerInfo, handler grpc.UnaryHandler) (interface{}, error) {
		// Skip health check
		if strings.HasSuffix(info.FullMethod, "Health/Check") {
			return handler(ctx, req)
		}

		md, ok := metadata.FromIncomingContext(ctx)
		if !ok {
			return nil, status.Error(codes.Unauthenticated, "missing metadata")
		}

		authHeader := md.Get("authorization")
		if len(authHeader) == 0 {
			return nil, status.Error(codes.Unauthenticated, "missing authorization header")
		}

		token := strings.TrimPrefix(authHeader[0], "Bearer ")
		if token == "" {
			return nil, status.Error(codes.Unauthenticated, "invalid token format")
		}

		// Validate JWT token
		claims, err := validateJWT(token, jwtSecret)
		if err != nil {
			return nil, status.Error(codes.Unauthenticated, "invalid token")
		}

		// Add claims to context
		ctx = context.WithValue(ctx, "user_id", claims.UserID)
		ctx = context.WithValue(ctx, "user_role", claims.Role)

		return handler(ctx, req)
	}
}

// RateLimitInterceptor limits request rate
func RateLimitInterceptor(rateLimit int) grpc.UnaryServerInterceptor {
	limiter := NewTokenBucket(rateLimit)
	
	return func(ctx context.Context, req interface{}, info *grpc.UnaryServerInfo, handler grpc.UnaryHandler) (interface{}, error) {
		if !limiter.Allow() {
			return nil, status.Error(codes.ResourceExhausted, "rate limit exceeded")
		}
		return handler(ctx, req)
	}
}

// LoggingInterceptor logs requests
func LoggingInterceptor(logger *logger.Logger) grpc.UnaryServerInterceptor {
	return func(ctx context.Context, req interface{}, info *grpc.UnaryServerInfo, handler grpc.UnaryHandler) (interface{}, error) {
		start := time.Now()

		// Log request
		logger.Info("gRPC request started",
			logger.String("method", info.FullMethod),
			logger.String("request", fmt.Sprintf("%+v", req)),
		)

		// Call handler
		resp, err := handler(ctx, req)

		// Log response
		duration := time.Since(start)
		if err != nil {
			logger.Error("gRPC request failed",
				logger.String("method", info.FullMethod),
				logger.Duration("duration", duration),
				logger.Err(err),
			)
		} else {
			logger.Info("gRPC request completed",
				logger.String("method", info.FullMethod),
				logger.Duration("duration", duration),
			)
		}

		return resp, err
	}
}

// RecoveryInterceptor recovers from panics
func RecoveryInterceptor(logger *logger.Logger) grpc.UnaryServerInterceptor {
	return func(ctx context.Context, req interface{}, info *grpc.UnaryServerInfo, handler grpc.UnaryHandler) (resp interface{}, err error) {
		defer func() {
			if r := recover(); r != nil {
				logger.Error("Panic recovered",
					logger.String("method", info.FullMethod),
					logger.Any("panic", r),
				)
				err = status.Error(codes.Internal, "internal error")
			}
		}()
		return handler(ctx, req)
	}
}

// TimeoutInterceptor adds timeout to requests
func TimeoutInterceptor(timeout time.Duration) grpc.UnaryServerInterceptor {
	return func(ctx context.Context, req interface{}, info *grpc.UnaryServerInfo, handler grpc.UnaryHandler) (interface{}, error) {
		ctx, cancel := context.WithTimeout(ctx, timeout)
		defer cancel()

		respCh := make(chan interface{})
		errCh := make(chan error)

		go func() {
			resp, err := handler(ctx, req)
			if err != nil {
				errCh <- err
			} else {
				respCh <- resp
			}
		}()

		select {
		case <-ctx.Done():
			return nil, status.Error(codes.DeadlineExceeded, "request timeout")
		case resp := <-respCh:
			return resp, nil
		case err := <-errCh:
			return nil, err
		}
	}
}

// MetricsInterceptor records metrics
func MetricsInterceptor(metrics MetricsRecorder) grpc.UnaryServerInterceptor {
	return func(ctx context.Context, req interface{}, info *grpc.UnaryServerInfo, handler grpc.UnaryHandler) (interface{}, error) {
		start := time.Now()

		resp, err := handler(ctx, req)

		duration := time.Since(start)
		code := codes.OK
		if err != nil {
			if s, ok := status.FromError(err); ok {
				code = s.Code()
			}
		}

		metrics.RecordGRPCRequest(info.FullMethod, code, duration)

		return resp, err
	}
}

// MetricsRecorder interface
type MetricsRecorder interface {
	RecordGRPCRequest(method string, code codes.Code, duration time.Duration)
}

// TokenBucket for rate limiting
type TokenBucket struct {
	tokens     chan struct{}
	refillRate time.Duration
}

func NewTokenBucket(rate int) *TokenBucket {
	tb := &TokenBucket{
		tokens:     make(chan struct{}, rate),
		refillRate: time.Second / time.Duration(rate),
	}

	// Fill bucket
	for i := 0; i < rate; i++ {
		tb.tokens <- struct{}{}
	}

	// Refill goroutine
	go func() {
		ticker := time.NewTicker(tb.refillRate)
		for range ticker.C {
			select {
			case tb.tokens <- struct{}{}:
			default:
			}
		}
	}()

	return tb
}

func (tb *TokenBucket) Allow() bool {
	select {
	case <-tb.tokens:
		return true
	default:
		return false
	}
}
