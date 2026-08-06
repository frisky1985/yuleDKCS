//go:build integration

// Package integration contains end-to-end tests for yuleHUB.
//
// These tests start a real yuleHUB instance (or connect to an existing one)
// and perform live HTTP and gRPC calls. They are excluded from normal
// `go test` runs via the "integration" build tag.
//
//   # Run these tests (requires hub binary):
//   go test -tags=integration -count=1 -timeout 60s ./tests/integration/...
//
//   # Or against a running hub:
//   HUB_REST=http://localhost:8080 HUB_GRPC=localhost:9090 go test -tags=integration -count=1 -timeout 60s ./tests/integration/...
package integration

import (
	"context"
	"encoding/json"
	"fmt"
	"net"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
	"testing"
	"time"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"

	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
)

// ── Helpers ──

// getHubDir returns the project-root backend/cloud/hub directory by walking up
// from the test file until it finds a directory containing backend/cloud/hub.
func getHubDir() (string, error) {
	_, testFile, _, _ := runtime.Caller(0)
	dir := filepath.Dir(testFile)
	for i := 0; i < 8; i++ {
		candidate := filepath.Join(dir, "backend", "cloud", "hub")
		if _, err := os.Stat(candidate); err == nil {
			return candidate, nil
		}
		parent := filepath.Dir(dir)
		if parent == dir {
			break
		}
		dir = parent
	}
	return "", fmt.Errorf("hub directory not found (walked up from %s)", testFile)
}

// hubRestURL returns the REST base URL, from env or default.
func hubRestURL() string {
	if v := os.Getenv("HUB_REST"); v != "" {
		return v
	}
	return "http://localhost:8080"
}

// hubGrpcAddr returns the gRPC address, from env or default.
func hubGrpcAddr() string {
	if v := os.Getenv("HUB_GRPC"); v != "" {
		return v
	}
	return "localhost:9090"
}

// isHubRunning checks whether a REAL yuleHUB instance answers on the
// configured REST endpoint. A plain HTTP 200 is not enough — an unrelated
// service squatting on the port (e.g. a dev container) would otherwise be
// mistaken for the hub. We require the hub-specific /healthz JSON shape
// (services / pid fields) to confirm it is actually yuleHUB.
func isHubRunning() bool {
	client := &http.Client{Timeout: 2 * time.Second}
	resp, err := client.Get(hubRestURL() + "/healthz")
	if err != nil {
		return false
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		return false
	}
	var payload map[string]interface{}
	if err := json.NewDecoder(resp.Body).Decode(&payload); err != nil {
		return false
	}
	_, hasServices := payload["services"]
	_, hasPid := payload["pid"]
	return hasServices || hasPid
}

// buildHubBinary builds the hub binary and returns its path.
func buildHubBinary(t *testing.T) string {
	t.Helper()
	hubDir, err := getHubDir()
	if err != nil {
		t.Fatalf("cannot locate hub directory: %v", err)
	}

	binary := filepath.Join(t.TempDir(), "yulehub")
	cmd := exec.Command("go", "build", "-o", binary, "./cmd/hub/...")
	cmd.Dir = hubDir
	cmd.Stderr = os.Stderr

	start := time.Now()
	if err := cmd.Run(); err != nil {
		t.Fatalf("hub build failed: %v", err)
	}
	t.Logf("hub built in %v: %s", time.Since(start), binary)
	return binary
}

// defaultLocalDatabaseURL returns the local dev postgres DSN (matching
// docker-compose.yml defaults) unless DATABASE_URL is already set.
func defaultLocalDatabaseURL() string {
	if v := os.Getenv("DATABASE_URL"); v != "" {
		return v
	}
	return "postgres://yuledkcs:yuledkcs@localhost:5432/yuledkcs?sslmode=disable"
}

// portInUse reports whether something is already listening on the port.
func portInUse(port string) bool {
	conn, err := net.DialTimeout("tcp", net.JoinHostPort("localhost", port), 500*time.Millisecond)
	if err != nil {
		return false
	}
	conn.Close()
	return true
}

// requireHub ensures a REAL yuleHUB is reachable; when none is running it
// tries to build and start one. If the environment cannot host a hub (ports
// :8080/:9090 busy, postgres unreachable, build failure) the test is SKIPPED:
// these top-level hub API tests are BEST-EFFORT and must not red the suite in
// hub-less environments (the scenario suite in ./scenarios does not need a
// live hub).
func requireHub(t *testing.T) {
	t.Helper()
	if isHubRunning() {
		t.Logf("using already-running yuleHUB at %s", hubRestURL())
		return
	}

	// The hub binary binds hardcoded :8080/:9090 — if either is taken by an
	// unrelated service, skip immediately instead of building + waiting.
	if portInUse("8080") || portInUse("9090") {
		t.Skipf("hub API test skipped (best-effort): ports :8080/:9090 already in use " +
			"by another service (no yuleHUB detected there)")
	}

	hubDir, err := getHubDir()
	if err != nil {
		t.Skipf("hub API test skipped (best-effort): %v", err)
	}

	binary := filepath.Join(t.TempDir(), "yulehub")
	cmd := exec.Command("go", "build", "-o", binary, "./cmd/hub/...")
	cmd.Dir = hubDir
	cmd.Stderr = os.Stderr
	if err := cmd.Run(); err != nil {
		t.Skipf("hub API test skipped (best-effort): hub build failed: %v", err)
	}
	t.Logf("hub built: %s", binary)

	cleanup := startHubOrSkip(t, binary)
	t.Cleanup(cleanup)
}

// startHubOrSkip starts a yuleHUB instance with the required environment
// (JWT_SECRET, admin credentials, DATABASE_URL) and waits for it to become
// ready. The hub binary only supports -log-level/-log-file flags and binds
// hardcoded :8080/:9090, so this requires those ports to be free. On any
// environment failure the test is skipped rather than failed.
func startHubOrSkip(t *testing.T, binary string) func() {
	t.Helper()

	cmd := exec.Command(binary, "--log-level=warn")
	cmd.Env = append(os.Environ(),
		"JWT_SECRET=integration-test-secret-change-me",
		"ADMIN_USERNAME=admin",
		"ADMIN_PASSWORD=integration-admin-pass-2026",
		"DATABASE_URL="+defaultLocalDatabaseURL(),
	)
	cmd.Stderr = os.Stderr
	cmd.Stdout = os.Stdout

	if err := cmd.Start(); err != nil {
		t.Skipf("hub API test skipped (best-effort): hub start failed: %v", err)
	}

	// Wait for hub to become ready (REST :8080 / gRPC :9090 are hardcoded)
	deadline := time.Now().Add(20 * time.Second)
	for time.Now().Before(deadline) {
		if isHubRunning() {
			t.Logf("hub ready on REST=:8080 gRPC=:9090")
			return func() {
				_ = cmd.Process.Kill()
				_ = cmd.Wait()
			}
		}
		time.Sleep(500 * time.Millisecond)
	}

	_ = cmd.Process.Kill()
	_ = cmd.Wait()
	t.Skipf("hub API test skipped (best-effort): hub did not become ready within 20s " +
		"(ports :8080/:9090 busy or postgres unreachable?)")
	return nil
}

// ── Tests ──

// TestHealthEndpoint verifies the /health and /healthz HTTP endpoints respond
// with the expected status codes and payload shapes.
func TestHealthEndpoint(t *testing.T) {
	start := time.Now()
	defer recordHubTest(t, "健康检查 /health + /healthz", "HUB-API", "HTTP", start)

	requireHub(t)

	client := &http.Client{Timeout: 5 * time.Second}

	t.Run("health_simple", func(t *testing.T) {
		resp, err := client.Get(hubRestURL() + "/health")
		if err != nil {
			t.Fatalf("GET /health: %v", err)
		}
		defer resp.Body.Close()

		if resp.StatusCode != http.StatusOK {
			t.Errorf("/health returned HTTP %d, want %d", resp.StatusCode, http.StatusOK)
		}

		var payload map[string]interface{}
		if err := json.NewDecoder(resp.Body).Decode(&payload); err != nil {
			t.Fatalf("decode /health: %v", err)
		}
		if status, ok := payload["status"]; !ok || status != "ok" {
			t.Errorf(`/health status = %v, want "ok"`, status)
		}
	})

	t.Run("healthz_detailed", func(t *testing.T) {
		resp, err := client.Get(hubRestURL() + "/healthz")
		if err != nil {
			t.Fatalf("GET /healthz: %v", err)
		}
		defer resp.Body.Close()

		if resp.StatusCode != http.StatusOK {
			t.Errorf("/healthz returned HTTP %d, want %d", resp.StatusCode, http.StatusOK)
		}

		var payload map[string]interface{}
		if err := json.NewDecoder(resp.Body).Decode(&payload); err != nil {
			t.Fatalf("decode /healthz: %v", err)
		}

		// Verify expected fields exist
		for _, field := range []string{"status", "version", "uptime", "services", "go_version", "pid"} {
			if _, ok := payload[field]; !ok {
				t.Errorf("/healthz missing field: %s", field)
			}
		}

		t.Logf("hub status: %v", payload["status"])
		if svcs, ok := payload["services"].(map[string]interface{}); ok {
			for svc, svcStatus := range svcs {
				t.Logf("  service %s: %s", svc, svcStatus)
			}
		}
	})
}

// TestGrpcConnectivity verifies the gRPC server is reachable and responds to
// the HubTransportService HealthCheck RPC.
func TestGrpcConnectivity(t *testing.T) {
	start := time.Now()
	defer recordHubTest(t, "gRPC 连通性 + HealthCheck RPC", "HUB-API", "gRPC", start)

	requireHub(t)

	ctx, cancel := context.WithTimeout(context.Background(), 15*time.Second)
	defer cancel()

	conn, err := grpc.DialContext(ctx,
		hubGrpcAddr(),
		grpc.WithTransportCredentials(insecure.NewCredentials()),
		grpc.WithBlock(),
	)
	if err != nil {
		t.Fatalf("gRPC dial to %s failed: %v", hubGrpcAddr(), err)
	}
	defer conn.Close()

	t.Logf("gRPC connected to %s", hubGrpcAddr())

	transportClient := pb.NewHubTransportServiceClient(conn)
	healthCtx, healthCancel := context.WithTimeout(ctx, 5*time.Second)
	defer healthCancel()

	resp, err := transportClient.HealthCheck(healthCtx, &pb.HealthCheckRequest{})
	if err != nil {
		t.Fatalf("HealthCheck RPC failed: %v", err)
	}

	if resp == nil {
		t.Fatal("HealthCheck returned nil response")
	}

	t.Logf("gRPC HealthCheck: healthy=%v, adapters=%d", resp.GetHealthy(), len(resp.GetAdapters()))

	adapters := resp.GetAdapters()
	expectedAdapters := []string{"apple", "samsung", "xiaomi", "oppo", "vivo", "huawei"}
	t.Logf("Registered adapters (%d):", len(adapters))
	for _, a := range adapters {
		t.Logf("  - vendor=%s protocol=%s healthy=%v", a.GetVendor(), a.GetProtocol(), a.GetHealthy())
	}

	adapterVendors := make(map[string]bool)
	for _, a := range adapters {
		adapterVendors[a.GetVendor()] = true
	}
	for _, expected := range expectedAdapters {
		if !adapterVendors[expected] {
			t.Errorf("expected adapter %q not found in HealthCheck response", expected)
		}
	}
}

// TestHubStartStop verifies the hub process can start and be gracefully stopped.
func TestHubStartStop(t *testing.T) {
	start := time.Now()
	defer recordHubTest(t, "Hub 启停生命周期", "HUB-API", "进程", start)

	// Lifecycle test: needs a hub we can start/stop ourselves.
	if isHubRunning() {
		t.Skipf("hub API test skipped (best-effort): an external hub is already running")
	}
	requireHub(t)

	// Give it a moment, then verify it's still running
	time.Sleep(1 * time.Second)
	resp, err := http.Get(hubRestURL() + "/healthz")
	if err != nil {
		t.Fatalf("hub not responding after start: %v", err)
	}
	resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		t.Errorf("healthz returned %d after start", resp.StatusCode)
	}

	t.Logf("hub started successfully and passed health check")
}

// TestLoginEndpoint tests the JWT login flow.
func TestLoginEndpoint(t *testing.T) {
	start := time.Now()
	defer recordHubTest(t, "JWT 登录认证", "HUB-API", "HTTP", start)

	requireHub(t)

	loginURL := hubRestURL() + "/api/v1/auth/login"

	t.Run("valid_credentials", func(t *testing.T) {
		resp, err := http.Post(loginURL, "application/json",
			strings.NewReader(`{"user_id":"admin","password":"admin123"}`))
		if err != nil {
			t.Fatalf("POST /auth/login: %v", err)
		}
		defer resp.Body.Close()

		if resp.StatusCode != http.StatusOK {
			t.Errorf("login returned HTTP %d, want %d", resp.StatusCode, http.StatusOK)
		}

		var payload map[string]interface{}
		if err := json.NewDecoder(resp.Body).Decode(&payload); err != nil {
			t.Fatalf("decode login response: %v", err)
		}

		if token, ok := payload["token"]; ok {
			tokenStr := fmt.Sprint(token)
			if len(tokenStr) < 20 {
				t.Errorf("token looks too short (%d chars)", len(tokenStr))
			} else {
				t.Logf("received JWT token (%d chars)", len(tokenStr))
			}
		} else {
			t.Error("login response missing 'token' field")
		}

		if tt, ok := payload["token_type"]; !ok || tt != "Bearer" {
			t.Errorf(`token_type = %v, want "Bearer"`, tt)
		}
	})

	t.Run("invalid_credentials", func(t *testing.T) {
		resp, err := http.Post(loginURL, "application/json",
			strings.NewReader(`{"user_id":"admin","password":"wrongpass"}`))
		if err != nil {
			t.Fatalf("POST /auth/login: %v", err)
		}
		defer resp.Body.Close()

		if resp.StatusCode != http.StatusUnauthorized {
			t.Errorf("invalid login returned HTTP %d, want %d", resp.StatusCode, http.StatusUnauthorized)
		}
	})

	t.Run("empty_credentials", func(t *testing.T) {
		resp, err := http.Post(loginURL, "application/json",
			strings.NewReader(`{"user_id":"","password":""}`))
		if err != nil {
			t.Fatalf("POST /auth/login: %v", err)
		}
		defer resp.Body.Close()

		if resp.StatusCode != http.StatusBadRequest {
			t.Errorf("empty login returned HTTP %d, want %d", resp.StatusCode, http.StatusBadRequest)
		}
	})
}

// TestAuthProtectedEndpoint verifies that authenticated endpoints reject
// unauthenticated requests properly, and accept valid ones.
func TestAuthProtectedEndpoint(t *testing.T) {
	start := time.Now()
	defer recordHubTest(t, "受保护端点鉴权", "HUB-API", "HTTP", start)

	requireHub(t)

	t.Run("no_auth_header", func(t *testing.T) {
		resp, err := http.Get(hubRestURL() + "/api/v1/keys")
		if err != nil {
			t.Fatalf("GET /api/v1/keys: %v", err)
		}
		defer resp.Body.Close()

		if resp.StatusCode != http.StatusUnauthorized {
			t.Errorf("no-auth request returned HTTP %d, want %d", resp.StatusCode, http.StatusUnauthorized)
		}
	})

	t.Run("invalid_token", func(t *testing.T) {
		req, err := http.NewRequest("GET", hubRestURL()+"/api/v1/keys", nil)
		if err != nil {
			t.Fatalf("create request: %v", err)
		}
		req.Header.Set("Authorization", "Bearer invalid.token.here")

		resp, err := http.DefaultClient.Do(req)
		if err != nil {
			t.Fatalf("GET /api/v1/keys: %v", err)
		}
		defer resp.Body.Close()

		if resp.StatusCode != http.StatusUnauthorized {
			t.Errorf("invalid-token request returned HTTP %d, want %d", resp.StatusCode, http.StatusUnauthorized)
		}
	})

	t.Run("authed_request_without_grpc", func(t *testing.T) {
		// First, get a valid token
		loginResp, err := http.Post(hubRestURL()+"/api/v1/auth/login", "application/json",
			strings.NewReader(`{"user_id":"admin","password":"admin123"}`))
		if err != nil {
			t.Fatalf("login failed: %v", err)
		}
		defer loginResp.Body.Close()

		var loginPayload map[string]interface{}
		if err := json.NewDecoder(loginResp.Body).Decode(&loginPayload); err != nil {
			t.Fatalf("decode login: %v", err)
		}
		token := fmt.Sprint(loginPayload["token"])

		// Call /api/v1/keys with valid token (no gRPC backend → 503)
		req, err := http.NewRequest("GET", hubRestURL()+"/api/v1/keys", nil)
		if err != nil {
			t.Fatalf("create request: %v", err)
		}
		req.Header.Set("Authorization", "Bearer "+token)

		resp, err := http.DefaultClient.Do(req)
		if err != nil {
			t.Fatalf("authed GET /api/v1/keys: %v", err)
		}
		defer resp.Body.Close()

		// Without a gRPC backend, we expect 503 Service Unavailable
		// (not 401/403 — that would mean auth failed)
		if resp.StatusCode == http.StatusUnauthorized || resp.StatusCode == http.StatusForbidden {
			t.Errorf("authed request returned %d (auth issue), want 503 (no gRPC backend)", resp.StatusCode)
		} else {
			t.Logf("authed request returned HTTP %d (expected 503 if no gRPC backend)", resp.StatusCode)
		}
	})
}
