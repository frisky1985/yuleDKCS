package s2s

import (
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"go.uber.org/zap"
)

// ── 通用 helper ──────────────────────────────────────────────

func newICCEMockServer(t *testing.T, method, path string, statusCode int, respBody interface{}) *httptest.Server {
	t.Helper()
	return httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.Method != method {
			t.Errorf("expected method %s, got %s", method, r.Method)
		}
		if r.URL.Path != path {
			t.Errorf("expected path %s, got %s", path, r.URL.Path)
		}
		// Verify ICCE required headers are present
		if r.Header.Get("Content-Type") != "application/json" {
			t.Error("missing Content-Type: application/json header")
		}
		if r.Header.Get("Accept") != "application/json" {
			t.Error("missing Accept: application/json header")
		}
		if r.Header.Get("X-Vendor-ID") == "" {
			t.Error("missing X-Vendor-ID header")
		}
		if r.Header.Get("X-Request-Timestamp") == "" {
			t.Error("missing X-Request-Timestamp header")
		}
		if r.Header.Get("User-Agent") != "YuleDKCS-Hub/1.0 ICCE-S2S/1.0" {
			t.Errorf("unexpected User-Agent: %s", r.Header.Get("User-Agent"))
		}

		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(statusCode)
		if respBody != nil {
			json.NewEncoder(w).Encode(respBody)
		}
	}))
}

func newICCEClient(baseURL string) *ICCEClient {
	logger := zap.NewNop()
	endpoint := ICCEEndpoint{
		BaseURL:    baseURL,
		Timeout:    30 * time.Second,
		RetryCount: 2,
		RetryWait:  1 * time.Second,
	}
	return NewICCEClient("huawei", endpoint, logger)
}

// ── BindKey ──────────────────────────────────────────────────

func TestICCEClient_BindKey(t *testing.T) {
	mockResp := ICCEBindResponse{
		KeyID:         "key-bound-001",
		VehiclePubKey: "vehicle-pubkey-base64",
		SharedSecret:  "shared-secret-base64",
		Status:        "ACTIVE",
		CreatedAt:     time.Now().UnixMilli(),
	}
	mockSrv := newICCEMockServer(t, http.MethodPost, "/bind", http.StatusOK, mockResp)
	defer mockSrv.Close()

	client := newICCEClient(mockSrv.URL)
	resp, err := client.BindKey(context.Background(), &ICCEBindRequest{
		VehicleID:    "VH-001",
		DeviceID:     "DEV-001",
		UserID:       "user-001",
		DevicePubKey: "device-pubkey-base64",
		KeyType:      "owner",
		AccessLevel:  "full",
	})
	if err != nil {
		t.Fatalf("BindKey failed: %v", err)
	}
	if resp.KeyID != "key-bound-001" {
		t.Errorf("expected key-bound-001, got %s", resp.KeyID)
	}
	if resp.VehiclePubKey != "vehicle-pubkey-base64" {
		t.Errorf("expected vehicle-pubkey-base64, got %s", resp.VehiclePubKey)
	}
	if resp.SharedSecret != "shared-secret-base64" {
		t.Errorf("expected shared-secret-base64, got %s", resp.SharedSecret)
	}
	if resp.Status != "ACTIVE" {
		t.Errorf("expected ACTIVE, got %s", resp.Status)
	}
}

// ── UnbindKey ────────────────────────────────────────────────

func TestICCEClient_UnbindKey(t *testing.T) {
	mockSrv := newICCEMockServer(t, http.MethodPost, "/unbind", http.StatusOK, nil)
	defer mockSrv.Close()

	client := newICCEClient(mockSrv.URL)
	err := client.UnbindKey(context.Background(), "key-001")
	if err != nil {
		t.Fatalf("UnbindKey failed: %v", err)
	}
}

func TestICCEClient_UnbindKey_SendsKeyID(t *testing.T) {
	var reqBody ICCEUnbindRequest
	mockSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		json.NewDecoder(r.Body).Decode(&reqBody)
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusOK)
	}))
	defer mockSrv.Close()

	client := newICCEClient(mockSrv.URL)
	err := client.UnbindKey(context.Background(), "key-to-unbind")
	if err != nil {
		t.Fatalf("UnbindKey failed: %v", err)
	}
	if reqBody.KeyID != "key-to-unbind" {
		t.Errorf("expected key-to-unbind, got %s", reqBody.KeyID)
	}
}

// ── RevokeKey ────────────────────────────────────────────────

func TestICCEClient_RevokeKey(t *testing.T) {
	mockSrv := newICCEMockServer(t, http.MethodPost, "/revoke", http.StatusOK, nil)
	defer mockSrv.Close()

	client := newICCEClient(mockSrv.URL)
	err := client.RevokeKey(context.Background(), "key-001", "stolen")
	if err != nil {
		t.Fatalf("RevokeKey failed: %v", err)
	}
}

func TestICCEClient_RevokeKey_SendsReason(t *testing.T) {
	var reqBody ICCERevokeRequest
	mockSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		json.NewDecoder(r.Body).Decode(&reqBody)
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusOK)
	}))
	defer mockSrv.Close()

	client := newICCEClient(mockSrv.URL)
	err := client.RevokeKey(context.Background(), "key-001", "device_lost")
	if err != nil {
		t.Fatalf("RevokeKey failed: %v", err)
	}
	if reqBody.KeyID != "key-001" {
		t.Errorf("expected key-001, got %s", reqBody.KeyID)
	}
	if reqBody.Reason != "device_lost" {
		t.Errorf("expected device_lost, got %s", reqBody.Reason)
	}
}

// ── ShareKey ─────────────────────────────────────────────────

func TestICCEClient_ShareKey(t *testing.T) {
	mockResp := ICCEShareResponse{
		ShareID:   "share-001",
		ShareCode: "654321",
		ExpireAt:  time.Now().Add(1 * time.Hour).UnixMilli(),
	}
	mockSrv := newICCEMockServer(t, http.MethodPost, "/share", http.StatusOK, mockResp)
	defer mockSrv.Close()

	client := newICCEClient(mockSrv.URL)
	resp, err := client.ShareKey(context.Background(), &ICCEShareRequest{
		KeyID:      "key-001",
		FromUserID: "user-001",
	})
	if err != nil {
		t.Fatalf("ShareKey failed: %v", err)
	}
	if resp.ShareID != "share-001" {
		t.Errorf("expected share-001, got %s", resp.ShareID)
	}
	if resp.ShareCode != "654321" {
		t.Errorf("expected 654321, got %s", resp.ShareCode)
	}
}

// ── AcceptShare ──────────────────────────────────────────────

func TestICCEClient_AcceptShare(t *testing.T) {
	mockResp := ICCEBindResponse{
		KeyID:         "key-accepted-001",
		VehiclePubKey: "vehicle-pubkey",
		SharedSecret:  "shared-secret",
		Status:        "ACTIVE",
		CreatedAt:     time.Now().UnixMilli(),
	}
	mockSrv := newICCEMockServer(t, http.MethodPost, "/share/accept", http.StatusOK, mockResp)
	defer mockSrv.Close()

	client := newICCEClient(mockSrv.URL)
	resp, err := client.AcceptShare(context.Background(), &ICCEAcceptShareRequest{
		ShareCode: "654321",
		DeviceID:  "DEV-002",
		UserID:    "user-002",
	})
	if err != nil {
		t.Fatalf("AcceptShare failed: %v", err)
	}
	if resp.KeyID != "key-accepted-001" {
		t.Errorf("expected key-accepted-001, got %s", resp.KeyID)
	}
}

// ── HealthCheck ──────────────────────────────────────────────

func TestICCEClient_HealthCheck(t *testing.T) {
	now := time.Now().UnixMilli()
	mockResp := ICCEHealthResponse{
		Status:    "OK",
		Timestamp: now,
	}
	mockSrv := newICCEMockServer(t, http.MethodGet, "/health", http.StatusOK, mockResp)
	defer mockSrv.Close()

	client := newICCEClient(mockSrv.URL)
	resp, err := client.HealthCheck(context.Background())
	if err != nil {
		t.Fatalf("HealthCheck failed: %v", err)
	}
	if resp.Status != "OK" {
		t.Errorf("expected OK, got %s", resp.Status)
	}
	if resp.Timestamp != now {
		t.Errorf("expected timestamp %d, got %d", now, resp.Timestamp)
	}
}

// ── Headers ──────────────────────────────────────────────────

func TestICCEClient_Headers(t *testing.T) {
	var capturedHeaders http.Header
	mockSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		capturedHeaders = r.Header.Clone()
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusOK)
		json.NewEncoder(w).Encode(ICCEBindResponse{KeyID: "test"})
	}))
	defer mockSrv.Close()

	client := newICCEClient(mockSrv.URL)
	_, err := client.BindKey(context.Background(), &ICCEBindRequest{
		VehicleID: "VH-001", DeviceID: "DEV-001", UserID: "user-001",
		DevicePubKey: "pk", KeyType: "owner",
	})
	if err != nil {
		t.Fatalf("BindKey failed: %v", err)
	}

	tests := []struct {
		name   string
		header string
	}{
		{"Content-Type", "Content-Type"},
		{"Accept", "Accept"},
		{"X-Vendor-ID", "X-Vendor-ID"},
		{"X-Request-Timestamp", "X-Request-Timestamp"},
		{"User-Agent", "User-Agent"},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if v := capturedHeaders.Get(tt.header); v == "" {
				t.Errorf("missing header: %s", tt.header)
			}
		})
	}

	if got := capturedHeaders.Get("Content-Type"); got != "application/json" {
		t.Errorf("Content-Type: expected application/json, got %s", got)
	}
	if got := capturedHeaders.Get("Accept"); got != "application/json" {
		t.Errorf("Accept: expected application/json, got %s", got)
	}
	if got := capturedHeaders.Get("X-Vendor-ID"); got != "huawei" {
		t.Errorf("X-Vendor-ID: expected huawei, got %s", got)
	}
	if got := capturedHeaders.Get("User-Agent"); got != "YuleDKCS-Hub/1.0 ICCE-S2S/1.0" {
		t.Errorf("User-Agent: expected YuleDKCS-Hub/1.0 ICCE-S2S/1.0, got %s", got)
	}
}

// ── Error handling ───────────────────────────────────────────

func TestICCEClient_ClientErrorNoRetry(t *testing.T) {
	attempts := 0
	mockSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		attempts++
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusBadRequest)
		json.NewEncoder(w).Encode(ICCEAPIError{
			Code:    40001,
			Message: "invalid bind request",
		})
	}))
	defer mockSrv.Close()

	client := newICCEClient(mockSrv.URL)
	_, err := client.BindKey(context.Background(), &ICCEBindRequest{
		VehicleID: "VH-001", DeviceID: "DEV-001", UserID: "user-001",
		DevicePubKey: "pk", KeyType: "owner",
	})
	if err == nil {
		t.Fatal("expected error, got nil")
	}
	if !strings.Contains(err.Error(), "invalid bind request") {
		t.Errorf("expected 'invalid bind request' in error, got: %v", err)
	}
	if attempts != 1 {
		t.Errorf("expected exactly 1 attempt (no retry for 4xx), got %d", attempts)
	}
}

func TestICCEClient_ServerErrorRetry(t *testing.T) {
	attempts := 0
	mockSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		attempts++
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusInternalServerError)
		json.NewEncoder(w).Encode(ICCEAPIError{
			Code:    50001,
			Message: "server error",
		})
	}))
	defer mockSrv.Close()

	endpoint := ICCEEndpoint{
		BaseURL:    mockSrv.URL,
		Timeout:    30 * time.Second,
		RetryCount: 2,
		RetryWait:  10 * time.Millisecond,
	}
	client := NewICCEClient("huawei", endpoint, zap.NewNop())

	_, err := client.BindKey(context.Background(), &ICCEBindRequest{
		VehicleID: "VH-001", DeviceID: "DEV-001", UserID: "user-001",
		DevicePubKey: "pk", KeyType: "owner",
	})
	if err == nil {
		t.Fatal("expected error, got nil")
	}
	expectedAttempts := endpoint.RetryCount + 1 // 3
	if attempts != expectedAttempts {
		t.Errorf("expected %d attempts (RetryCount=%d), got %d",
			expectedAttempts, endpoint.RetryCount, attempts)
	}
	if !strings.Contains(err.Error(), "after 2 retries") {
		t.Errorf("expected 'after 2 retries' in error, got: %v", err)
	}
}

func TestICCEClient_ServerErrorRetrySuccess(t *testing.T) {
	attempts := 0
	mockSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		attempts++
		w.Header().Set("Content-Type", "application/json")
		if attempts < 3 {
			w.WriteHeader(http.StatusInternalServerError)
			json.NewEncoder(w).Encode(ICCEAPIError{Code: 50001, Message: "server error"})
			return
		}
		w.WriteHeader(http.StatusOK)
		json.NewEncoder(w).Encode(ICCEBindResponse{
			KeyID: "key-after-retry", VehiclePubKey: "pk", SharedSecret: "secret", Status: "ACTIVE",
		})
	}))
	defer mockSrv.Close()

	endpoint := ICCEEndpoint{
		BaseURL: mockSrv.URL, Timeout: 30 * time.Second,
		RetryCount: 3, RetryWait: 10 * time.Millisecond,
	}
	client := NewICCEClient("huawei", endpoint, zap.NewNop())

	resp, err := client.BindKey(context.Background(), &ICCEBindRequest{
		VehicleID: "VH-001", DeviceID: "DEV-001", UserID: "user-001",
		DevicePubKey: "pk", KeyType: "owner",
	})
	if err != nil {
		t.Fatalf("BindKey should succeed after retry, got: %v", err)
	}
	if resp.KeyID != "key-after-retry" {
		t.Errorf("expected key-after-retry, got %s", resp.KeyID)
	}
	if attempts != 3 {
		t.Errorf("expected 3 attempts (2 failures + 1 success), got %d", attempts)
	}
}

func TestICCEClient_ContextCancel(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	cancel()

	mockSrv := newICCEMockServer(t, http.MethodPost, "/bind", http.StatusOK,
		ICCEBindResponse{KeyID: "should-not-reach"})
	defer mockSrv.Close()

	client := newICCEClient(mockSrv.URL)
	_, err := client.BindKey(ctx, &ICCEBindRequest{
		VehicleID: "VH-001", DeviceID: "DEV-001", UserID: "user-001",
		DevicePubKey: "pk", KeyType: "owner",
	})
	if err == nil {
		t.Fatal("expected context canceled error, got nil")
	}
	if !strings.Contains(err.Error(), "context canceled") {
		t.Errorf("expected 'context canceled' in error, got: %v", err)
	}
}

func TestICCEClient_ContextCancelDuringRetry(t *testing.T) {
	attempts := 0
	mockSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		attempts++
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusInternalServerError)
		json.NewEncoder(w).Encode(ICCEAPIError{Code: 50001, Message: "server error"})
	}))
	defer mockSrv.Close()

	endpoint := ICCEEndpoint{
		BaseURL: mockSrv.URL, Timeout: 30 * time.Second,
		RetryCount: 5, RetryWait: 5 * time.Second,
	}
	client := NewICCEClient("huawei", endpoint, zap.NewNop())

	ctx, cancel := context.WithCancel(context.Background())
	go func() {
		time.Sleep(50 * time.Millisecond)
		cancel()
	}()

	_, err := client.BindKey(ctx, &ICCEBindRequest{
		VehicleID: "VH-001", DeviceID: "DEV-001", UserID: "user-001",
		DevicePubKey: "pk", KeyType: "owner",
	})
	if err == nil {
		t.Fatal("expected context canceled error, got nil")
	}
	if !strings.Contains(err.Error(), "context canceled") {
		t.Errorf("expected 'context canceled' in error, got: %v", err)
	}
	if attempts < 1 {
		t.Errorf("expected at least 1 attempt, got %d", attempts)
	}
}

func TestICCEClient_NonJSONErrorBody(t *testing.T) {
	attempts := 0
	mockSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		attempts++
		w.Header().Set("Content-Type", "text/plain")
		w.WriteHeader(http.StatusBadRequest)
		w.Write([]byte("Bad Request"))
	}))
	defer mockSrv.Close()

	client := newICCEClient(mockSrv.URL)
	_, err := client.BindKey(context.Background(), &ICCEBindRequest{
		VehicleID: "VH-001", DeviceID: "DEV-001", UserID: "user-001",
		DevicePubKey: "pk", KeyType: "owner",
	})
	if err == nil {
		t.Fatal("expected error, got nil")
	}
	if !strings.Contains(err.Error(), "HTTP 400") {
		t.Errorf("expected 'HTTP 400' in error, got: %v", err)
	}
	if attempts != 1 {
		t.Errorf("expected 1 attempt (no retry for 4xx), got %d", attempts)
	}
}

func TestICCEClient_NonJSONServerError(t *testing.T) {
	attempts := 0
	mockSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		attempts++
		w.Header().Set("Content-Type", "text/plain")
		w.WriteHeader(http.StatusServiceUnavailable)
		w.Write([]byte("Service Unavailable"))
	}))
	defer mockSrv.Close()

	endpoint := ICCEEndpoint{
		BaseURL: mockSrv.URL, Timeout: 30 * time.Second,
		RetryCount: 1, RetryWait: 10 * time.Millisecond,
	}
	client := NewICCEClient("huawei", endpoint, zap.NewNop())

	_, err := client.BindKey(context.Background(), &ICCEBindRequest{
		VehicleID: "VH-001", DeviceID: "DEV-001", UserID: "user-001",
		DevicePubKey: "pk", KeyType: "owner",
	})
	if err == nil {
		t.Fatal("expected error, got nil")
	}
	expectedAttempts := endpoint.RetryCount + 1 // 2
	if attempts != expectedAttempts {
		t.Errorf("expected %d attempts, got %d", expectedAttempts, attempts)
	}
}

func TestICCEClient_TimestampFormat(t *testing.T) {
	var timestamp string
	mockSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		timestamp = r.Header.Get("X-Request-Timestamp")
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusOK)
		json.NewEncoder(w).Encode(ICCEBindResponse{KeyID: "test"})
	}))
	defer mockSrv.Close()

	client := newICCEClient(mockSrv.URL)
	_, err := client.BindKey(context.Background(), &ICCEBindRequest{
		VehicleID: "VH-001", DeviceID: "DEV-001", UserID: "user-001",
		DevicePubKey: "pk", KeyType: "owner",
	})
	if err != nil {
		t.Fatalf("BindKey failed: %v", err)
	}
	if timestamp == "" {
		t.Fatal("X-Request-Timestamp is empty")
	}
}
