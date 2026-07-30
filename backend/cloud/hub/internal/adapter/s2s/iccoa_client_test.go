package s2s

import (
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"go.uber.org/zap"
)

// ── 通用 helper ──────────────────────────────────────────────

// newICCOAMockServer returns an httptest.Server that records the last request for header verification.
// The handlerFunc receives the request and writes a JSON response of the given respBody.
func newICCOAMockServer(t *testing.T, method, path string, statusCode int, respBody interface{}) *httptest.Server {
	t.Helper()
	return httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.Method != method {
			t.Errorf("expected method %s, got %s", method, r.Method)
		}
		if r.URL.Path != path {
			t.Errorf("expected path %s, got %s", path, r.URL.Path)
		}
		// Verify ICCOA required headers are present
		if r.Header.Get("Content-Type") != "application/json" {
			t.Error("missing Content-Type: application/json header")
		}
		if r.Header.Get("Accept") != "application/json" {
			t.Error("missing Accept: application/json header")
		}
		if r.Header.Get("X-ICCOA-Request-ID") == "" {
			t.Error("missing X-ICCOA-Request-ID header")
		}
		if r.Header.Get("X-ICCOA-Timestamp") == "" {
			t.Error("missing X-ICCOA-Timestamp header")
		}
		if r.Header.Get("X-ICCOA-Device-OEM-ID") != "DEVICE-OEM-001" {
			t.Errorf("expected X-ICCOA-Device-OEM-ID 'DEVICE-OEM-001', got '%s'",
				r.Header.Get("X-ICCOA-Device-OEM-ID"))
		}
		if r.Header.Get("X-ICCOA-Vehicle-OEM-ID") != "VENDOR-OEM-001" {
			t.Errorf("expected X-ICCOA-Vehicle-OEM-ID 'VENDOR-OEM-001', got '%s'",
				r.Header.Get("X-ICCOA-Vehicle-OEM-ID"))
		}
		if r.Header.Get("User-Agent") != "YuleDKCS-Hub/1.0 ICCOA-S2S/1.0" {
			t.Errorf("unexpected User-Agent: %s", r.Header.Get("User-Agent"))
		}

		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(statusCode)
		if respBody != nil {
			json.NewEncoder(w).Encode(respBody)
		}
	}))
}

func newICCOAClient(baseURL string) *ICCOAClient {
	logger := zap.NewNop()
	config := NewDefaultICCOAConfig("xiaomi", baseURL, "VENDOR-OEM-001", "DEVICE-OEM-001")
	return NewICCOAClient("xiaomi", config, logger)
}

// ── GenSession ───────────────────────────────────────────────

func TestICCOAClient_GenSession(t *testing.T) {
	mockResp := ICCOAGenSessionResponse{
		SessionID: "session-test-123",
		ShareCode: "123456",
		ExpireAt:  time.Now().Add(24 * time.Hour).UnixMilli(),
	}
	mockSrv := newICCOAMockServer(t, http.MethodPost, "/share/genSession", http.StatusOK, mockResp)
	defer mockSrv.Close()

	client := newICCOAClient(mockSrv.URL)
	resp, err := client.GenSession(context.Background(), &ICCOAGenSessionRequest{
		KeyID:      "key-001",
		FromUserID: "user-001",
		ToUserID:   "user-002",
	})
	if err != nil {
		t.Fatalf("GenSession failed: %v", err)
	}
	if resp.SessionID != "session-test-123" {
		t.Errorf("expected session-test-123, got %s", resp.SessionID)
	}
	if resp.ShareCode != "123456" {
		t.Errorf("expected 123456, got %s", resp.ShareCode)
	}
}

// ── GetMidCsr ────────────────────────────────────────────────

func TestICCOAClient_GetMidCsr(t *testing.T) {
	mockResp := ICCOAGetMidCsrResponse{
		MidCsr: "base64-encoded-csr-data",
	}
	mockSrv := newICCOAMockServer(t, http.MethodPost, "/share/getMidCsr", http.StatusOK, mockResp)
	defer mockSrv.Close()

	client := newICCOAClient(mockSrv.URL)
	resp, err := client.GetMidCsr(context.Background(), &ICCOAGetMidCsrRequest{
		SessionID: "session-001",
		Csr:       "device-csr",
	})
	if err != nil {
		t.Fatalf("GetMidCsr failed: %v", err)
	}
	if resp.MidCsr != "base64-encoded-csr-data" {
		t.Errorf("expected base64-encoded-csr-data, got %s", resp.MidCsr)
	}
}

// ── PutMidCert ───────────────────────────────────────────────

func TestICCOAClient_PutMidCert(t *testing.T) {
	mockResp := ICCOAPutMidCertResponse{
		Status: "SUCCESS",
	}
	mockSrv := newICCOAMockServer(t, http.MethodPost, "/share/putMidCert", http.StatusOK, mockResp)
	defer mockSrv.Close()

	client := newICCOAClient(mockSrv.URL)
	resp, err := client.PutMidCert(context.Background(), &ICCOAPutMidCertRequest{
		SessionID: "session-001",
		MidCert:   "signed-mid-cert-data",
	})
	if err != nil {
		t.Fatalf("PutMidCert failed: %v", err)
	}
	if resp.Status != "SUCCESS" {
		t.Errorf("expected SUCCESS, got %s", resp.Status)
	}
}

// ── Sign ─────────────────────────────────────────────────────

func TestICCOAClient_Sign(t *testing.T) {
	mockResp := ICCOASignResponse{
		KeyID:  "key-signed-001",
		DkCert: "base64-dk-cert",
	}
	mockSrv := newICCOAMockServer(t, http.MethodPost, "/share/sign", http.StatusOK, mockResp)
	defer mockSrv.Close()

	client := newICCOAClient(mockSrv.URL)
	resp, err := client.Sign(context.Background(), &ICCOASignRequest{
		SessionID:    "session-001",
		DevicePubKey: "device-pubkey",
	})
	if err != nil {
		t.Fatalf("Sign failed: %v", err)
	}
	if resp.KeyID != "key-signed-001" {
		t.Errorf("expected key-signed-001, got %s", resp.KeyID)
	}
	if resp.DkCert != "base64-dk-cert" {
		t.Errorf("expected base64-dk-cert, got %s", resp.DkCert)
	}
}

// ── CancelShare ──────────────────────────────────────────────

func TestICCOAClient_CancelShare(t *testing.T) {
	// CancelShare returns no response body (nil respBody in doRequest)
	mockSrv := newICCOAMockServer(t, http.MethodPost, "/share/cancel", http.StatusOK, nil)
	defer mockSrv.Close()

	client := newICCOAClient(mockSrv.URL)
	err := client.CancelShare(context.Background(), &ICCOACancelShareRequest{
		KeyID:  "key-001",
		Reason: "user_request",
	})
	if err != nil {
		t.Fatalf("CancelShare failed: %v", err)
	}
}

// ── TrackKey ─────────────────────────────────────────────────

func TestICCOAClient_TrackKey(t *testing.T) {
	mockResp := ICCOATrackKeyResponse{
		KeyID:         "key-tracked-001",
		VehiclePubKey: "vehicle-pubkey-base64",
		Status:        "ACTIVE",
	}
	mockSrv := newICCOAMockServer(t, http.MethodPost, "/trackKey", http.StatusOK, mockResp)
	defer mockSrv.Close()

	client := newICCOAClient(mockSrv.URL)
	resp, err := client.TrackKey(context.Background(), &ICCOATrackKeyRequest{
		KeyID:     "key-001",
		VehicleID: "VH-001",
		DeviceID:  "DEV-001",
		UserID:    "user-001",
		KeyType:   "owner",
	})
	if err != nil {
		t.Fatalf("TrackKey failed: %v", err)
	}
	if resp.KeyID != "key-tracked-001" {
		t.Errorf("expected key-tracked-001, got %s", resp.KeyID)
	}
	if resp.Status != "ACTIVE" {
		t.Errorf("expected ACTIVE, got %s", resp.Status)
	}
}

// ── ManageKey ────────────────────────────────────────────────

func TestICCOAClient_ManageKey(t *testing.T) {
	mockResp := ICCOAManageKeyResponse{
		Status: "REVOKED",
	}
	mockSrv := newICCOAMockServer(t, http.MethodPost, "/manageKey", http.StatusOK, mockResp)
	defer mockSrv.Close()

	client := newICCOAClient(mockSrv.URL)
	resp, err := client.ManageKey(context.Background(), &ICCOAManageKeyRequest{
		KeyID:  "key-001",
		Action: "revoke",
	})
	if err != nil {
		t.Fatalf("ManageKey failed: %v", err)
	}
	if resp.Status != "REVOKED" {
		t.Errorf("expected REVOKED, got %s", resp.Status)
	}
}

// ── NotifyKeyEvent ───────────────────────────────────────────

func TestICCOAClient_NotifyKeyEvent(t *testing.T) {
	// NotifyKeyEvent returns no response body (nil respBody in doRequest)
	mockSrv := newICCOAMockServer(t, http.MethodPost, "/notifyKeyEvent", http.StatusOK, nil)
	defer mockSrv.Close()

	client := newICCOAClient(mockSrv.URL)
	err := client.NotifyKeyEvent(context.Background(), &ICCOANotifyKeyEventRequest{
		KeyID:     "key-001",
		EventType: "bind",
		Timestamp: time.Now().UnixMilli(),
	})
	if err != nil {
		t.Fatalf("NotifyKeyEvent failed: %v", err)
	}
}

// ── GetVehicleProfile ────────────────────────────────────────

func TestICCOAClient_GetVehicleProfile(t *testing.T) {
	mockResp := ICCOAGetVehicleProfileResponse{
		Profile: ICCOAVehicleProfile{
			VehicleModel: "Model-S",
			RkeFunctions: []string{"lock", "unlock"},
			KeyAccessProfiles: []string{"phone-as-key"},
		},
	}
	mockSrv := newICCOAMockServer(t, http.MethodGet, "/getVehicleProfile", http.StatusOK, mockResp)
	defer mockSrv.Close()

	client := newICCOAClient(mockSrv.URL)
	resp, err := client.GetVehicleProfile(context.Background(), &ICCOAGetVehicleProfileRequest{
		VehicleID: "VH-001",
	})
	if err != nil {
		t.Fatalf("GetVehicleProfile failed: %v", err)
	}
	if resp.Profile.VehicleModel != "Model-S" {
		t.Errorf("expected Model-S, got %s", resp.Profile.VehicleModel)
	}
	if len(resp.Profile.RkeFunctions) != 2 {
		t.Errorf("expected 2 RKE functions, got %d", len(resp.Profile.RkeFunctions))
	}
}

func TestICCOAClient_GetVehicleProfile_QueryParam(t *testing.T) {
	// Verify the vehicleId query parameter is passed correctly
	mockSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/getVehicleProfile" {
			t.Errorf("expected /getVehicleProfile, got %s", r.URL.Path)
		}
		if r.URL.RawQuery != "vehicleId=VH-001" {
			t.Errorf("expected vehicleId=VH-001, got %s", r.URL.RawQuery)
		}
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusOK)
		json.NewEncoder(w).Encode(ICCOAGetVehicleProfileResponse{
			Profile: ICCOAVehicleProfile{VehicleModel: "Model-3"},
		})
	}))
	defer mockSrv.Close()

	client := newICCOAClient(mockSrv.URL)
	resp, err := client.GetVehicleProfile(context.Background(), &ICCOAGetVehicleProfileRequest{
		VehicleID: "VH-001",
	})
	if err != nil {
		t.Fatalf("GetVehicleProfile failed: %v", err)
	}
	if resp.Profile.VehicleModel != "Model-3" {
		t.Errorf("expected Model-3, got %s", resp.Profile.VehicleModel)
	}
}

// ── HealthCheck ──────────────────────────────────────────────

func TestICCOAClient_HealthCheck(t *testing.T) {
	now := time.Now().UnixMilli()
	mockResp := ICCOAHealthResponse{
		Status:    "OK",
		Timestamp: now,
	}
	mockSrv := newICCOAMockServer(t, http.MethodGet, "/healthCheck", http.StatusOK, mockResp)
	defer mockSrv.Close()

	client := newICCOAClient(mockSrv.URL)
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

// ── GetSar ───────────────────────────────────────────────────

func TestICCOAClient_GetSar(t *testing.T) {
	mockResp := ICCOAGetSarResponse{
		Sar: "base64-sar-data",
	}
	mockSrv := newICCOAMockServer(t, http.MethodPost, "/share/getSar", http.StatusOK, mockResp)
	defer mockSrv.Close()

	client := newICCOAClient(mockSrv.URL)
	resp, err := client.GetSar(context.Background(), &ICCOAGetSarRequest{
		SessionID: "session-001",
	})
	if err != nil {
		t.Fatalf("GetSar failed: %v", err)
	}
	if resp.Sar != "base64-sar-data" {
		t.Errorf("expected base64-sar-data, got %s", resp.Sar)
	}
}

// ── PutSharingAttestation ────────────────────────────────────

func TestICCOAClient_PutSharingAttestation(t *testing.T) {
	mockResp := ICCOAPutSharingAttestationResponse{
		Status: "SUCCESS",
	}
	mockSrv := newICCOAMockServer(t, http.MethodPost, "/share/putSharingAttestation", http.StatusOK, mockResp)
	defer mockSrv.Close()

	client := newICCOAClient(mockSrv.URL)
	resp, err := client.PutSharingAttestation(context.Background(), &ICCOAPutSharingAttestationRequest{
		SessionID:          "session-001",
		SharingAttestation: "attestation-data",
	})
	if err != nil {
		t.Fatalf("PutSharingAttestation failed: %v", err)
	}
	if resp.Status != "SUCCESS" {
		t.Errorf("expected SUCCESS, got %s", resp.Status)
	}
}

// ── Error handling tests ─────────────────────────────────────

func TestICCOAClient_ClientErrorNoRetry(t *testing.T) {
	// 400 error should NOT be retried — return immediately
	attempts := 0
	mockSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		attempts++
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusBadRequest)
		json.NewEncoder(w).Encode(ICCOAAPIError{
			Code:    40001,
			Message: "invalid request",
		})
	}))
	defer mockSrv.Close()

	client := newICCOAClient(mockSrv.URL)
	_, err := client.GenSession(context.Background(), &ICCOAGenSessionRequest{
		KeyID:      "key-001",
		FromUserID: "user-001",
	})

	if err == nil {
		t.Fatal("expected error, got nil")
	}
	if !strings.Contains(err.Error(), "invalid request") {
		t.Errorf("expected 'invalid request' in error, got: %v", err)
	}
	if attempts != 1 {
		t.Errorf("expected exactly 1 attempt (no retry for 4xx), got %d", attempts)
	}
}

func TestICCOAClient_ServerErrorRetry(t *testing.T) {
	// 500 error should be retried up to RetryCount times
	attempts := 0
	mockSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		attempts++
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusInternalServerError)
		json.NewEncoder(w).Encode(ICCOAAPIError{
			Code:    50001,
			Message: "server error",
		})
	}))
	defer mockSrv.Close()

	config := NewDefaultICCOAConfig("xiaomi", mockSrv.URL, "VENDOR-OEM-001", "DEVICE-OEM-001")
	config.RetryCount = 2
	config.RetryWait = 10 * time.Millisecond // fast retry for test
	client := NewICCOAClient("xiaomi", config, zap.NewNop())

	_, err := client.GenSession(context.Background(), &ICCOAGenSessionRequest{
		KeyID:      "key-001",
		FromUserID: "user-001",
	})

	if err == nil {
		t.Fatal("expected error, got nil")
	}
	// RetryCount=2 means attempts: 1 initial + 2 retries = 3
	expectedAttempts := config.RetryCount + 1
	if attempts != expectedAttempts {
		t.Errorf("expected %d attempts (RetryCount=%d), got %d", expectedAttempts, config.RetryCount, attempts)
	}
	// Verify error mentions retries
	if !strings.Contains(err.Error(), "after 2 retries") {
		t.Errorf("expected 'after 2 retries' in error, got: %v", err)
	}
}

func TestICCOAClient_ServerErrorRetrySuccess(t *testing.T) {
	// 500 → 500 → 200: succeeds on third attempt
	attempts := 0
	mockSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		attempts++
		w.Header().Set("Content-Type", "application/json")
		if attempts < 3 {
			w.WriteHeader(http.StatusInternalServerError)
			json.NewEncoder(w).Encode(ICCOAAPIError{Code: 50001, Message: "server error"})
			return
		}
		w.WriteHeader(http.StatusOK)
		json.NewEncoder(w).Encode(ICCOAGenSessionResponse{
			SessionID: "session-after-retry",
			ShareCode: "789012",
		})
	}))
	defer mockSrv.Close()

	config := NewDefaultICCOAConfig("xiaomi", mockSrv.URL, "VENDOR-OEM-001", "DEVICE-OEM-001")
	config.RetryCount = 3
	config.RetryWait = 10 * time.Millisecond
	client := NewICCOAClient("xiaomi", config, zap.NewNop())

	resp, err := client.GenSession(context.Background(), &ICCOAGenSessionRequest{
		KeyID:      "key-001",
		FromUserID: "user-001",
	})
	if err != nil {
		t.Fatalf("GenSession should succeed after retry, got: %v", err)
	}
	if resp.SessionID != "session-after-retry" {
		t.Errorf("expected session-after-retry, got %s", resp.SessionID)
	}
	if resp.ShareCode != "789012" {
		t.Errorf("expected 789012, got %s", resp.ShareCode)
	}
	if attempts != 3 {
		t.Errorf("expected 3 attempts (2 failures + 1 success), got %d", attempts)
	}
}

func TestICCOAClient_ContextCancel(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	cancel() // cancel immediately

	mockSrv := newICCOAMockServer(t, http.MethodPost, "/share/genSession", http.StatusOK,
		ICCOAGenSessionResponse{SessionID: "should-not-reach"})
	defer mockSrv.Close()

	client := newICCOAClient(mockSrv.URL)
	_, err := client.GenSession(ctx, &ICCOAGenSessionRequest{
		KeyID:      "key-001",
		FromUserID: "user-001",
	})

	if err == nil {
		t.Fatal("expected context canceled error, got nil")
	}
	if !strings.Contains(err.Error(), "context canceled") {
		t.Errorf("expected 'context canceled' in error, got: %v", err)
	}
}

func TestICCOAClient_ContextCancelDuringRetry(t *testing.T) {
	// Cancel context during retry wait
	attempts := 0
	mockSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		attempts++
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusInternalServerError)
		json.NewEncoder(w).Encode(ICCOAAPIError{Code: 50001, Message: "server error"})
	}))
	defer mockSrv.Close()

	config := NewDefaultICCOAConfig("xiaomi", mockSrv.URL, "VENDOR-OEM-001", "DEVICE-OEM-001")
	config.RetryCount = 5
	config.RetryWait = 5 * time.Second // long wait to ensure cancel happens during retry
	client := NewICCOAClient("xiaomi", config, zap.NewNop())

	ctx, cancel := context.WithCancel(context.Background())
	// Cancel context after a short delay so the first request goes through but retry is cancelled
	go func() {
		time.Sleep(50 * time.Millisecond)
		cancel()
	}()

	_, err := client.GenSession(ctx, &ICCOAGenSessionRequest{
		KeyID:      "key-001",
		FromUserID: "user-001",
	})

	if err == nil {
		t.Fatal("expected context canceled error, got nil")
	}
	if !strings.Contains(err.Error(), "context canceled") {
		t.Errorf("expected 'context canceled' in error, got: %v", err)
	}
	// Should have made exactly 1 attempt (first request), then cancelled during retry wait
	if attempts < 1 {
		t.Errorf("expected at least 1 attempt, got %d", attempts)
	}
}

// ── Headers ──────────────────────────────────────────────────

func TestICCOAClient_Headers(t *testing.T) {
	// Dedicated test to verify all ICCOA-specific headers
	var capturedHeaders http.Header
	mockSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		capturedHeaders = r.Header.Clone()
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusOK)
		json.NewEncoder(w).Encode(ICCOAGenSessionResponse{SessionID: "test"})
	}))
	defer mockSrv.Close()

	client := newICCOAClient(mockSrv.URL)
	_, err := client.GenSession(context.Background(), &ICCOAGenSessionRequest{
		KeyID:      "key-001",
		FromUserID: "user-001",
	})
	if err != nil {
		t.Fatalf("GenSession failed: %v", err)
	}

	tests := []struct {
		name   string
		header string
	}{
		{"Content-Type", "Content-Type"},
		{"Accept", "Accept"},
		{"X-ICCOA-Request-ID", "X-ICCOA-Request-ID"},
		{"X-ICCOA-Timestamp", "X-ICCOA-Timestamp"},
		{"X-ICCOA-Device-OEM-ID", "X-ICCOA-Device-OEM-ID"},
		{"X-ICCOA-Vehicle-OEM-ID", "X-ICCOA-Vehicle-OEM-ID"},
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
	if got := capturedHeaders.Get("X-ICCOA-Device-OEM-ID"); got != "DEVICE-OEM-001" {
		t.Errorf("X-ICCOA-Device-OEM-ID: expected DEVICE-OEM-001, got %s", got)
	}
	if got := capturedHeaders.Get("X-ICCOA-Vehicle-OEM-ID"); got != "VENDOR-OEM-001" {
		t.Errorf("X-ICCOA-Vehicle-OEM-ID: expected VENDOR-OEM-001, got %s", got)
	}
	if got := capturedHeaders.Get("User-Agent"); got != "YuleDKCS-Hub/1.0 ICCOA-S2S/1.0" {
		t.Errorf("User-Agent: expected YuleDKCS-Hub/1.0 ICCOA-S2S/1.0, got %s", got)
	}
}

func TestICCOAClient_RequestIDFormat(t *testing.T) {
	// Verify X-ICCOA-Request-ID follows the format: "vendor-timestamp"
	var requestID string
	mockSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		requestID = r.Header.Get("X-ICCOA-Request-ID")
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusOK)
		json.NewEncoder(w).Encode(ICCOAGenSessionResponse{SessionID: "test"})
	}))
	defer mockSrv.Close()

	client := newICCOAClient(mockSrv.URL)
	_, err := client.GenSession(context.Background(), &ICCOAGenSessionRequest{
		KeyID:      "key-001",
		FromUserID: "user-001",
	})
	if err != nil {
		t.Fatalf("GenSession failed: %v", err)
	}

	if !strings.HasPrefix(requestID, "xiaomi-") {
		t.Errorf("expected requestID to start with 'xiaomi-', got %s", requestID)
	}
}

func TestICCOAClient_TimestampFormat(t *testing.T) {
	// Verify X-ICCOA-Timestamp is a valid millisecond timestamp
	var timestamp string
	mockSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		timestamp = r.Header.Get("X-ICCOA-Timestamp")
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusOK)
		json.NewEncoder(w).Encode(ICCOAGenSessionResponse{SessionID: "test"})
	}))
	defer mockSrv.Close()

	client := newICCOAClient(mockSrv.URL)
	_, err := client.GenSession(context.Background(), &ICCOAGenSessionRequest{
		KeyID:      "key-001",
		FromUserID: "user-001",
	})
	if err != nil {
		t.Fatalf("GenSession failed: %v", err)
	}

	// Verify it's a valid number (not a string like "NaN" or empty)
	if timestamp == "" {
		t.Fatal("X-ICCOA-Timestamp is empty")
	}
	// Verify it can be parsed as an int64 (millisecond timestamp)
	var ts int64
	var parseErr error
	_, parseErr = fmt.Sscanf(timestamp, "%d", &ts)
	if parseErr != nil {
		t.Errorf("X-ICCOA-Timestamp '%s' is not a valid number: %v", timestamp, parseErr)
	}
	if ts <= 0 {
		t.Errorf("X-ICCOA-Timestamp should be positive, got %d", ts)
	}
}

// ── GenSession with default OEM IDs ──────────────────────────

func TestICCOAClient_GenSession_DefaultOEMIDs(t *testing.T) {
	// Verify that empty DeviceOEMID/VehicleOEMID in request are filled from config
	var reqBody ICCOAGenSessionRequest
	mockSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		json.NewDecoder(r.Body).Decode(&reqBody)
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusOK)
		json.NewEncoder(w).Encode(ICCOAGenSessionResponse{SessionID: "test"})
	}))
	defer mockSrv.Close()

	client := newICCOAClient(mockSrv.URL)
	_, err := client.GenSession(context.Background(), &ICCOAGenSessionRequest{
		KeyID:      "key-001",
		FromUserID: "user-001",
		// Deliberately omit DeviceOEMID and VehicleOEMID
	})
	if err != nil {
		t.Fatalf("GenSession failed: %v", err)
	}

	if reqBody.DeviceOEMID != "DEVICE-OEM-001" {
		t.Errorf("expected DeviceOEMID default 'DEVICE-OEM-001', got %s", reqBody.DeviceOEMID)
	}
	if reqBody.VehicleOEMID != "VENDOR-OEM-001" {
		t.Errorf("expected VehicleOEMID default 'VENDOR-OEM-001', got %s", reqBody.VehicleOEMID)
	}
}

// ── Non-JSON error body ──────────────────────────────────────

func TestICCOAClient_NonJSONErrorBody(t *testing.T) {
	// When the server returns a non-JSON body with 4xx, doRequest should handle it gracefully
	attempts := 0
	mockSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		attempts++
		w.Header().Set("Content-Type", "text/plain")
		w.WriteHeader(http.StatusBadRequest)
		w.Write([]byte("Bad Request"))
	}))
	defer mockSrv.Close()

	client := newICCOAClient(mockSrv.URL)
	_, err := client.GenSession(context.Background(), &ICCOAGenSessionRequest{
		KeyID:      "key-001",
		FromUserID: "user-001",
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

func TestICCOAClient_NonJSONServerError(t *testing.T) {
	// 5xx with non-JSON body should still count as retryable
	attempts := 0
	mockSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		attempts++
		w.Header().Set("Content-Type", "text/plain")
		w.WriteHeader(http.StatusServiceUnavailable)
		w.Write([]byte("Service Unavailable"))
	}))
	defer mockSrv.Close()

	config := NewDefaultICCOAConfig("xiaomi", mockSrv.URL, "VENDOR-OEM-001", "DEVICE-OEM-001")
	config.RetryCount = 1
	config.RetryWait = 10 * time.Millisecond
	client := NewICCOAClient("xiaomi", config, zap.NewNop())

	_, err := client.GenSession(context.Background(), &ICCOAGenSessionRequest{
		KeyID:      "key-001",
		FromUserID: "user-001",
	})

	if err == nil {
		t.Fatal("expected error, got nil")
	}
	expectedAttempts := config.RetryCount + 1 // 2
	if attempts != expectedAttempts {
		t.Errorf("expected %d attempts, got %d", expectedAttempts, attempts)
	}
}
