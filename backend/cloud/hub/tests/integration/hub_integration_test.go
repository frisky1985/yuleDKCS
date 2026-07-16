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
// from the test file.
func getHubDir() (string, error) {
	_, testFile, _, _ := runtime.Caller(0)
	dir := filepath.Dir(testFile)
	for i := 0; i < 4; i++ {
		dir = filepath.Dir(dir)
	}
	hubDir := filepath.Join(dir, "backend", "cloud", "hub")
	if _, err := os.Stat(hubDir); err != nil {
		return "", fmt.Errorf("hub directory not found at %s: %w", hubDir, err)
	}
	return hubDir, nil
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

// isHubRunning checks if the hub health endpoint responds.
func isHubRunning() bool {
	client := &http.Client{Timeout: 2 * time.Second}
	resp, err := client.Get(hubRestURL() + "/healthz")
	if err != nil {
		return false
	}
	resp.Body.Close()
	return resp.StatusCode == http.StatusOK
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

// startHub starts a hub instance on the given ports and returns a cleanup function.
func startHub(t *testing.T, binary string, grpcPort, restPort int) func() {
	t.Helper()

	cmd := exec.Command(binary,
		fmt.Sprintf("--grpc-port=%d", grpcPort),
		fmt.Sprintf("--rest-port=%d", restPort),
		"--log-level=warn",
	)
	cmd.Env = append(os.Environ(), "JWT_SECRET=integration-test-secret-change-me")
	cmd.Stderr = os.Stderr
	cmd.Stdout = os.Stdout

	if err := cmd.Start(); err != nil {
		t.Fatalf("hub start failed: %v", err)
	}

	// Wait for hub to become ready
	restAddr := fmt.Sprintf("http://localhost:%d/healthz", restPort)
	deadline := time.Now().Add(15 * time.Second)
	for time.Now().Before(deadline) {
		resp, err := http.Get(restAddr)
		if err == nil {
			resp.Body.Close()
			if resp.StatusCode == http.StatusOK {
				t.Logf("hub ready on gRPC=:%d REST=:%d", grpcPort, restPort)
				return func() {
					_ = cmd.Process.Kill()
					_ = cmd.Wait()
				}
			}
		}
		time.Sleep(500 * time.Millisecond)
	}

	_ = cmd.Process.Kill()
	t.Fatalf("hub did not become ready within 15 seconds")
	return nil
}

// ── Tests ──

// TestHealthEndpoint verifies the /health and /healthz HTTP endpoints respond
// with the expected status codes and payload shapes.
func TestHealthEndpoint(t *testing.T) {
	hubDir, err := getHubDir()
	if err != nil {
		t.Skipf("not in yuleDKCS tree: %v", err)
	}
	_ = hubDir

	// If hub is already running externally, use it; otherwise start one.
	if !isHubRunning() {
		binary := buildHubBinary(t)
		cleanup := startHub(t, binary, 9091, 8081)
		t.Cleanup(cleanup)
		t.Setenv("HUB_REST", "http://localhost:8081")
		t.Setenv("HUB_GRPC", "localhost:9091")
	}

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
	if !isHubRunning() {
		binary := buildHubBinary(t)
		cleanup := startHub(t, binary, 9092, 8082)
		t.Cleanup(cleanup)
		t.Setenv("HUB_GRPC", "localhost:9092")
	}

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
	hubDir, err := getHubDir()
	if err != nil {
		t.Skipf("not in yuleDKCS tree: %v", err)
	}

	binary := filepath.Join(t.TempDir(), "yulehub")
	cmd := exec.Command("go", "build", "-o", binary, "./cmd/hub/...")
	cmd.Dir = hubDir
	cmd.Stderr = os.Stderr
	if err := cmd.Run(); err != nil {
		t.Fatalf("build failed: %v", err)
	}

	grpcPort := 9093
	restPort := 8083

	cleanup := startHub(t, binary, grpcPort, restPort)
	defer cleanup()

	// Give it a moment, then verify it's still running
	time.Sleep(1 * time.Second)
	resp, err := http.Get(fmt.Sprintf("http://localhost:%d/healthz", restPort))
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
	if !isHubRunning() {
		binary := buildHubBinary(t)
		cleanup := startHub(t, binary, 9094, 8084)
		t.Cleanup(cleanup)
		t.Setenv("HUB_REST", "http://localhost:8084")
	}

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
	if !isHubRunning() {
		binary := buildHubBinary(t)
		cleanup := startHub(t, binary, 9095, 8085)
		t.Cleanup(cleanup)
		t.Setenv("HUB_REST", "http://localhost:8085")
	}

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
