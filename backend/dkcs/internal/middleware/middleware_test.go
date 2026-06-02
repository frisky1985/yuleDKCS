package middleware

import (
	"context"
	"fmt"
	"sync"
	"testing"
	"time"

	"github.com/frisky1985/yuleDKCS/backend/dkcs/pkg/logger"
	"github.com/golang-jwt/jwt/v5"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
)

// mockLoggerRecorder captures logger calls for testing
type mockLoggerRecorder struct {
	mu     sync.Mutex
	infos  []logCall
	errors []logCall
}

type logCall struct {
	msg    string
	fields []interface{}
}

func (m *mockLoggerRecorder) Info(msg string, fields ...interface{}) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.infos = append(m.infos, logCall{msg: msg, fields: fields})
}

func (m *mockLoggerRecorder) Error(msg string, fields ...interface{}) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.errors = append(m.errors, logCall{msg: msg, fields: fields})
}

// mockLogger wraps mockLoggerRecorder to match *logger.Logger interface
type mockLogger struct {
	recorder *mockLoggerRecorder
}

func newMockLogger() *mockLogger {
	return &mockLogger{recorder: &mockLoggerRecorder{}}
}

func (ml *mockLogger) Info(msg string, fields ...interface{})  { ml.recorder.Info(msg, fields...) }
func (ml *mockLogger) Error(msg string, fields ...interface{}) { ml.recorder.Error(msg, fields...) }
func (ml *mockLogger) Debug(msg string, fields ...interface{}) {}
func (ml *mockLogger) Warn(msg string, fields ...interface{})  {}
func (ml *mockLogger) Fatal(msg string, fields ...interface{}) {}
func (ml *mockLogger) Sync() error                             { return nil }

// --- AuthInterceptor tests ---

func TestAuthInterceptor_SkipHealthCheck(t *testing.T) {
	interceptor := AuthInterceptor("test-secret")
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return "ok", nil
	}

	info := &grpc.UnaryServerInfo{FullMethod: "/grpc.health.v1.Health/Check"}
	resp, err := interceptor(context.Background(), "req", info, handler)
	if err != nil {
		t.Fatalf("expected no error for health check, got: %v", err)
	}
	if resp != "ok" {
		t.Fatalf("expected 'ok', got %v", resp)
	}
}

func TestAuthInterceptor_MissingMetadata(t *testing.T) {
	interceptor := AuthInterceptor("test-secret")
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return "ok", nil
	}

	// Context without metadata
	info := &grpc.UnaryServerInfo{FullMethod: "/dkcs.KeyService/Create"}
	_, err := interceptor(context.Background(), "req", info, handler)
	if err == nil {
		t.Fatal("expected error for missing metadata")
	}
	if status.Code(err) != codes.Unauthenticated {
		t.Errorf("expected Unauthenticated, got %v", status.Code(err))
	}
}

func TestAuthInterceptor_MissingAuthHeader(t *testing.T) {
	interceptor := AuthInterceptor("test-secret")
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return "ok", nil
	}

	ctx := metadata.NewIncomingContext(context.Background(), metadata.Pairs("something", "else"))
	info := &grpc.UnaryServerInfo{FullMethod: "/dkcs.KeyService/Create"}
	_, err := interceptor(ctx, "req", info, handler)
	if err == nil {
		t.Fatal("expected error for missing auth header")
	}
	if status.Code(err) != codes.Unauthenticated {
		t.Errorf("expected Unauthenticated, got %v", status.Code(err))
	}
}

func TestAuthInterceptor_InvalidTokenFormat(t *testing.T) {
	interceptor := AuthInterceptor("test-secret")
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return "ok", nil
	}

	ctx := metadata.NewIncomingContext(context.Background(), metadata.Pairs("authorization", "NotBearer token"))
	info := &grpc.UnaryServerInfo{FullMethod: "/dkcs.KeyService/Create"}
	_, err := interceptor(ctx, "req", info, handler)
	if err == nil {
		t.Fatal("expected error for invalid token format")
	}
	if status.Code(err) != codes.Unauthenticated {
		t.Errorf("expected Unauthenticated, got %v", status.Code(err))
	}
}

func TestAuthInterceptor_ValidToken(t *testing.T) {
	secret := "test-secret"
	interceptor := AuthInterceptor(secret)
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		// Verify claims were added to context
		userID, _ := ctx.Value("user_id").(string)
		role, _ := ctx.Value("user_role").(string)
		return fmt.Sprintf("user=%s role=%s", userID, role), nil
	}

	// Generate a valid JWT
	claims := &Claims{
		UserID: "user-123",
		Role:   "admin",
		RegisteredClaims: jwt.RegisteredClaims{
			Issuer: "test",
		},
	}
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	tokenStr, err := token.SignedString([]byte(secret))
	if err != nil {
		t.Fatalf("failed to sign token: %v", err)
	}

	ctx := metadata.NewIncomingContext(context.Background(), metadata.Pairs("authorization", "Bearer "+tokenStr))
	info := &grpc.UnaryServerInfo{FullMethod: "/dkcs.KeyService/Create"}
	resp, err := interceptor(ctx, "req", info, handler)
	if err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}
	if resp != "user=user-123 role=admin" {
		t.Fatalf("expected 'user=user-123 role=admin', got %v", resp)
	}
}

func TestAuthInterceptor_InvalidTokenSignature(t *testing.T) {
	secret := "test-secret"
	interceptor := AuthInterceptor(secret)
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return "ok", nil
	}

	// Generate a token with a different secret
	claims := &Claims{
		UserID: "user-123",
		Role:   "admin",
	}
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	tokenStr, err := token.SignedString([]byte("wrong-secret"))
	if err != nil {
		t.Fatalf("failed to sign token: %v", err)
	}

	ctx := metadata.NewIncomingContext(context.Background(), metadata.Pairs("authorization", "Bearer "+tokenStr))
	info := &grpc.UnaryServerInfo{FullMethod: "/dkcs.KeyService/Create"}
	_, err = interceptor(ctx, "req", info, handler)
	if err == nil {
		t.Fatal("expected error for invalid token signature")
	}
	if status.Code(err) != codes.Unauthenticated {
		t.Errorf("expected Unauthenticated, got %v", status.Code(err))
	}
}

// --- RateLimitInterceptor tests ---

func TestRateLimitInterceptor_Allows(t *testing.T) {
	interceptor := RateLimitInterceptor(10)
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return "ok", nil
	}

	info := &grpc.UnaryServerInfo{FullMethod: "/test/Method"}
	resp, err := interceptor(context.Background(), "req", info, handler)
	if err != nil {
		t.Fatalf("expected success, got: %v", err)
	}
	if resp != "ok" {
		t.Fatalf("expected 'ok', got %v", resp)
	}
}

func TestRateLimitInterceptor_Exhausted(t *testing.T) {
	interceptor := RateLimitInterceptor(1)
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return "ok", nil
	}

	info := &grpc.UnaryServerInfo{FullMethod: "/test/Method"}

	// First call should succeed
	_, err := interceptor(context.Background(), "req", info, handler)
	if err != nil {
		t.Fatalf("expected first call to succeed, got: %v", err)
	}

	// Consume remaining tokens (only 1 token in bucket)
	_ = interceptor
	// Second call should fail since bucket has no tokens (refill hasn't happened yet)
	_, err = interceptor(context.Background(), "req", info, handler)
	if err == nil {
		t.Fatal("expected error for rate limit exceeded")
	}
	if status.Code(err) != codes.ResourceExhausted {
		t.Errorf("expected ResourceExhausted, got %v", status.Code(err))
	}
}

func TestTokenBucket_Allow(t *testing.T) {
	tb := NewTokenBucket(3)
	defer tb.Stop()

	// First 3 should succeed
	for i := 0; i < 3; i++ {
		if !tb.Allow() {
			t.Errorf("expected allow on attempt %d", i+1)
		}
	}
	// 4th should fail
	if tb.Allow() {
		t.Error("expected deny when bucket is empty")
	}
}

func TestTokenBucket_Stop(t *testing.T) {
	tb := NewTokenBucket(5)
	tb.Stop()
	// Stop again should be safe (M-01)
	tb.Stop()
	tb.Stop()
}

// --- LoggingInterceptor tests ---

func TestLoggingInterceptor_Success(t *testing.T) {
	interceptor := LoggingInterceptor(&logger.Logger{})

	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return "result", nil
	}

	info := &grpc.UnaryServerInfo{FullMethod: "/test/Method"}
	resp, err := interceptor(context.Background(), "req", info, handler)
	if err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}
	if resp != "result" {
		t.Fatalf("expected 'result', got %v", resp)
	}
}

func TestLoggingInterceptor_Error(t *testing.T) {
	interceptor := LoggingInterceptor(&logger.Logger{})
	expectedErr := fmt.Errorf("something went wrong")

	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return nil, expectedErr
	}

	info := &grpc.UnaryServerInfo{FullMethod: "/test/FailMethod"}
	_, err := interceptor(context.Background(), "req", info, handler)
	if err != expectedErr {
		t.Fatalf("expected original error, got: %v", err)
	}
}

// --- RecoveryInterceptor tests ---

func TestRecoveryInterceptor_Panic(t *testing.T) {
	interceptor := RecoveryInterceptor(&logger.Logger{})
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		panic("unexpected panic")
	}

	info := &grpc.UnaryServerInfo{FullMethod: "/test/PanicMethod"}
	_, err := interceptor(context.Background(), "req", info, handler)
	if err == nil {
		t.Fatal("expected error from panic recovery")
	}
	if status.Code(err) != codes.Internal {
		t.Errorf("expected Internal code, got %v", status.Code(err))
	}
}

func TestRecoveryInterceptor_NoPanic(t *testing.T) {
	interceptor := RecoveryInterceptor(&logger.Logger{})
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return "success", nil
	}

	info := &grpc.UnaryServerInfo{FullMethod: "/test/NormalMethod"}
	resp, err := interceptor(context.Background(), "req", info, handler)
	if err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}
	if resp != "success" {
		t.Fatalf("expected 'success', got %v", resp)
	}
}

// --- TimeoutInterceptor tests ---

func TestTimeoutInterceptor_Success(t *testing.T) {
	interceptor := TimeoutInterceptor(time.Second)
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return "fast", nil
	}

	info := &grpc.UnaryServerInfo{FullMethod: "/test/FastMethod"}
	resp, err := interceptor(context.Background(), "req", info, handler)
	if err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}
	if resp != "fast" {
		t.Fatalf("expected 'fast', got %v", resp)
	}
}

func TestTimeoutInterceptor_HandlerError(t *testing.T) {
	interceptor := TimeoutInterceptor(time.Second)
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return nil, fmt.Errorf("handler error")
	}

	info := &grpc.UnaryServerInfo{FullMethod: "/test/ErrMethod"}
	_, err := interceptor(context.Background(), "req", info, handler)
	if err == nil {
		t.Fatal("expected error from handler")
	}
}

func TestTimeoutInterceptor_Timeout(t *testing.T) {
	interceptor := TimeoutInterceptor(10 * time.Millisecond)
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		time.Sleep(time.Second) // Exceeds timeout
		return "slow", nil
	}

	info := &grpc.UnaryServerInfo{FullMethod: "/test/SlowMethod"}
	_, err := interceptor(context.Background(), "req", info, handler)
	if err == nil {
		t.Fatal("expected timeout error")
	}
	if status.Code(err) != codes.DeadlineExceeded {
		t.Errorf("expected DeadlineExceeded, got %v", status.Code(err))
	}
}

// --- MetricsInterceptor tests ---

type mockMetricsRecorder struct {
	mu       sync.Mutex
	requests []metricCall
}

type metricCall struct {
	method   string
	code     codes.Code
	duration time.Duration
}

func (m *mockMetricsRecorder) RecordGRPCRequest(method string, code codes.Code, duration time.Duration) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.requests = append(m.requests, metricCall{method: method, code: code, duration: duration})
}

func TestMetricsInterceptor_Success(t *testing.T) {
	recorder := &mockMetricsRecorder{}
	interceptor := MetricsInterceptor(recorder)

	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		time.Sleep(time.Millisecond)
		return "result", nil
	}

	info := &grpc.UnaryServerInfo{FullMethod: "/test/MetricMethod"}
	resp, err := interceptor(context.Background(), "req", info, handler)
	if err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}
	if resp != "result" {
		t.Fatalf("expected 'result', got %v", resp)
	}

	recorder.mu.Lock()
	if len(recorder.requests) != 1 {
		t.Fatalf("expected 1 metric, got %d", len(recorder.requests))
	}
	if recorder.requests[0].method != "/test/MetricMethod" {
		t.Errorf("expected method '/test/MetricMethod', got %s", recorder.requests[0].method)
	}
	if recorder.requests[0].code != codes.OK {
		t.Errorf("expected code OK, got %v", recorder.requests[0].code)
	}
	if recorder.requests[0].duration < time.Millisecond {
		t.Errorf("expected duration >= 1ms, got %v", recorder.requests[0].duration)
	}
	recorder.mu.Unlock()
}

func TestMetricsInterceptor_Error(t *testing.T) {
	recorder := &mockMetricsRecorder{}
	interceptor := MetricsInterceptor(recorder)

	expectedErr := status.Error(codes.PermissionDenied, "no access")
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return nil, expectedErr
	}

	info := &grpc.UnaryServerInfo{FullMethod: "/test/MetricErr"}
	_, err := interceptor(context.Background(), "req", info, handler)
	if err != expectedErr {
		t.Fatalf("expected permission denied error, got: %v", err)
	}

	recorder.mu.Lock()
	if len(recorder.requests) != 1 {
		t.Fatalf("expected 1 metric, got %d", len(recorder.requests))
	}
	if recorder.requests[0].code != codes.PermissionDenied {
		t.Errorf("expected PermissionDenied, got %v", recorder.requests[0].code)
	}
	recorder.mu.Unlock()
}

// --- ChainInterceptors tests ---

func TestChainInterceptors_ExecutesAll(t *testing.T) {
	var order []string
	mu := sync.Mutex{}

	makeInterceptor := func(name string) grpc.UnaryServerInterceptor {
		return func(ctx context.Context, req interface{}, info *grpc.UnaryServerInfo, handler grpc.UnaryHandler) (interface{}, error) {
			mu.Lock()
			order = append(order, "before-"+name)
			mu.Unlock()
			resp, err := handler(ctx, req)
			mu.Lock()
			order = append(order, "after-"+name)
			mu.Unlock()
			return resp, err
		}
	}

	interceptor := ChainInterceptors(
		makeInterceptor("A"),
		makeInterceptor("B"),
		makeInterceptor("C"),
	)

	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		mu.Lock()
		order = append(order, "handler")
		mu.Unlock()
		return "done", nil
	}

	info := &grpc.UnaryServerInfo{FullMethod: "/test/Chain"}
	resp, err := interceptor(context.Background(), "req", info, handler)
	if err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}
	if resp != "done" {
		t.Fatalf("expected 'done', got %v", resp)
	}

	// Verify order: A before → B before → C before → handler → C after → B after → A after
	expected := []string{"before-A", "before-B", "before-C", "handler", "after-C", "after-B", "after-A"}
	if len(order) != len(expected) {
		t.Fatalf("expected %d order entries, got %d: %v", len(expected), len(order), order)
	}
	for i, e := range expected {
		if order[i] != e {
			t.Errorf("order[%d] expected %s, got %s", i, e, order[i])
		}
	}
}

func TestChainInterceptors_Empty(t *testing.T) {
	interceptor := ChainInterceptors()
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return "ok", nil
	}

	info := &grpc.UnaryServerInfo{FullMethod: "/test/Empty"}
	resp, err := interceptor(context.Background(), "req", info, handler)
	if err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}
	if resp != "ok" {
		t.Fatalf("expected 'ok', got %v", resp)
	}
}

// --- validateJWT tests ---

func TestValidateJWT_InvalidSigningMethod(t *testing.T) {
	_, err := validateJWT("not-a-jwt", "secret")
	if err == nil {
		t.Fatal("expected error for invalid token")
	}
}

func TestValidateJWT_ValidToken(t *testing.T) {
	secret := "my-secret"
	claims := &Claims{
		UserID: "user-42",
		Role:   "viewer",
	}
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	tokenStr, err := token.SignedString([]byte(secret))
	if err != nil {
		t.Fatalf("failed to sign token: %v", err)
	}

	result, err := validateJWT(tokenStr, secret)
	if err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}
	if result.UserID != "user-42" {
		t.Errorf("expected UserID='user-42', got '%s'", result.UserID)
	}
	if result.Role != "viewer" {
		t.Errorf("expected Role='viewer', got '%s'", result.Role)
	}
}


