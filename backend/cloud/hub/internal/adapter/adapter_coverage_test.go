package adapter

import (
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"go.uber.org/zap"
	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/adapter/s2s"
)

// ── ICCEAdapter: missing coverage (UnbindKey, RevokeNotify, ShareKey, AcceptShare, Notify) ──

func TestICCEAdapterUnbindKey(t *testing.T) {
	logger := zap.NewNop()
	a := NewICCEAdapter("huawei", logger)
	err := a.UnbindKey(context.Background(), "key-icce-001")
	if err != nil {
		t.Fatalf("UnbindKey should not fail: %v", err)
	}
}

func TestICCEAdapterRevokeNotify(t *testing.T) {
	logger := zap.NewNop()
	a := NewICCEAdapter("huawei", logger)
	err := a.RevokeNotify(context.Background(), "key-icce-001", "test-revoke")
	if err != nil {
		t.Fatalf("RevokeNotify should not fail: %v", err)
	}
}

func TestICCEAdapterShareKey(t *testing.T) {
	logger := zap.NewNop()
	a := NewICCEAdapter("huawei", logger)
	req := &pb.CreateShareRequest{
		KeyId:      "key-icce-001",
		FromUserId: "user-1",
	}
	resp, err := a.ShareKey(context.Background(), req)
	if err != nil {
		t.Fatalf("ShareKey should not fail: %v", err)
	}
	if resp.ShareId == "" {
		t.Error("expected non-empty share ID")
	}
	if resp.ShareCode == "" {
		t.Error("expected non-empty share code")
	}
}

func TestICCEAdapterAcceptShare(t *testing.T) {
	logger := zap.NewNop()
	a := NewICCEAdapter("huawei", logger)
	req := &pb.AcceptShareRequest{
		ShareCode: "123456",
		DeviceId:  "dev-002",
		UserId:    "user-002",
		Vendor:    pb.PhoneVendor_HUAWEI,
	}
	resp, err := a.AcceptShare(context.Background(), req)
	if err != nil {
		t.Fatalf("AcceptShare should not fail: %v", err)
	}
	if resp.Key == nil {
		t.Fatal("expected non-nil key")
	}
	if resp.Key.KeyType != pb.KeyType_FRIEND {
		t.Errorf("expected FRIEND key type, got %v", resp.Key.KeyType)
	}
	if resp.Key.Protocol != pb.Protocol_ICCE {
		t.Errorf("expected ICCE protocol, got %v", resp.Key.Protocol)
	}
}

func TestICCEAdapterNotify(t *testing.T) {
	logger := zap.NewNop()
	a := NewICCEAdapter("huawei", logger)
	err := a.Notify(context.Background(), "user-1", &pb.VehicleStatusUpdate{})
	if err != nil {
		t.Fatalf("Notify should not fail: %v", err)
	}
}

func TestICCEAdapterFullLifecycle(t *testing.T) {
	logger := zap.NewNop()
	a := NewICCEAdapter("huawei", logger)

	// BindKey
	bindResp, err := a.BindKey(context.Background(), &pb.BindKeyRequest{
		VehicleId:    "VH-ICCE-001",
		DeviceId:     "DEV-ICCE-001",
		UserId:       "U001",
		DevicePubkey: make([]byte, 64),
		KeyType:      pb.KeyType_OWNER,
	})
	if err != nil {
		t.Fatalf("BindKey failed: %v", err)
	}
	if bindResp.Key.Protocol != pb.Protocol_ICCE {
		t.Errorf("expected ICCE protocol, got %v", bindResp.Key.Protocol)
	}

	// UnbindKey
	if err := a.UnbindKey(context.Background(), bindResp.Key.KeyId); err != nil {
		t.Fatalf("UnbindKey failed: %v", err)
	}

	// RevokeNotify
	if err := a.RevokeNotify(context.Background(), bindResp.Key.KeyId, "owner_request"); err != nil {
		t.Fatalf("RevokeNotify failed: %v", err)
	}

	// HealthCheck
	status, err := a.HealthCheck(context.Background())
	if err != nil {
		t.Fatalf("HealthCheck failed: %v", err)
	}
	if !status.Healthy {
		t.Error("expected healthy")
	}
}

// ── CCCAdapter: missing coverage (UnbindKey, RevokeNotify, Notify) ──

func TestCCCAdapterUnbindKey(t *testing.T) {
	logger := zap.NewNop()
	a := NewCCCAdapter("apple", logger)
	err := a.UnbindKey(context.Background(), "key-ccc-001")
	if err != nil {
		t.Fatalf("UnbindKey should not fail: %v", err)
	}
}

func TestCCCAdapterRevokeNotify(t *testing.T) {
	logger := zap.NewNop()
	a := NewCCCAdapter("apple", logger)
	err := a.RevokeNotify(context.Background(), "key-ccc-001", "stolen")
	if err != nil {
		t.Fatalf("RevokeNotify should not fail: %v", err)
	}
}

func TestCCCAdapterNotify(t *testing.T) {
	logger := zap.NewNop()
	a := NewCCCAdapter("apple", logger)
	err := a.Notify(context.Background(), "user-1", &pb.VehicleStatusUpdate{})
	if err != nil {
		t.Fatalf("Notify should not fail: %v", err)
	}
}

func TestCCCAdapterFullLifecycle(t *testing.T) {
	logger := zap.NewNop()
	a := NewCCCAdapter("samsung", logger)

	req := &pb.BindKeyRequest{
		VehicleId:    "VH-CCC-001",
		DeviceId:     "DEV-CCC-001",
		UserId:       "U001",
		DevicePubkey: make([]byte, 64),
		KeyType:      pb.KeyType_OWNER,
		AccessLevel: &pb.AccessLevel{
			Lock: true, Unlock: true, Engine: true, Trunk: true,
		},
	}

	bindResp, err := a.BindKey(context.Background(), req)
	if err != nil {
		t.Fatalf("BindKey failed: %v", err)
	}
	if bindResp.Key.Protocol != pb.Protocol_CCC_DK3 {
		t.Errorf("expected CCC_DK3 protocol, got %v", bindResp.Key.Protocol)
	}

	// Unbind
	if err := a.UnbindKey(context.Background(), bindResp.Key.KeyId); err != nil {
		t.Fatalf("UnbindKey failed: %v", err)
	}

	// RevokeNotify
	if err := a.RevokeNotify(context.Background(), bindResp.Key.KeyId, "owner_request"); err != nil {
		t.Fatalf("RevokeNotify failed: %v", err)
	}

	// Notify
	if err := a.Notify(context.Background(), bindResp.Key.UserId, &pb.VehicleStatusUpdate{
		VehicleId: "VH-CCC-001", LockStatus: 1,
	}); err != nil {
		t.Fatalf("Notify failed: %v", err)
	}

	// Share + Accept
	shareResp, err := a.ShareKey(context.Background(), &pb.CreateShareRequest{
		KeyId: bindResp.Key.KeyId, FromUserId: "U001",
	})
	if err != nil {
		t.Fatalf("ShareKey failed: %v", err)
	}
	if shareResp.ShareId == "" {
		t.Error("expected non-empty share ID")
	}

	acceptResp, err := a.AcceptShare(context.Background(), &pb.AcceptShareRequest{
		ShareCode: shareResp.ShareCode, Vendor: pb.PhoneVendor_SAMSUNG,
	})
	if err != nil {
		t.Fatalf("AcceptShare failed: %v", err)
	}
	if acceptResp.Key.KeyType != pb.KeyType_FRIEND {
		t.Errorf("expected FRIEND, got %v", acceptResp.Key.KeyType)
	}
}

// ── ICCOAAdapter: mock S2S client tests ─────────────────────
// Tests that the adapter correctly delegates to S2S client methods
// and falls back to stub when client is nil or S2S fails.

// newICCOAMockS2SServer creates an httptest server that records calls and returns predefined responses.
func newICCOAMockS2SServer(t *testing.T) *httptest.Server {
	t.Helper()
	return httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		switch r.URL.Path {
		case "/trackKey":
			w.WriteHeader(http.StatusOK)
			json.NewEncoder(w).Encode(s2s.ICCOATrackKeyResponse{
				KeyID:         "s2s-key-tracked-001",
				VehiclePubKey: "s2s-vehicle-pubkey",
				Status:        "ACTIVE",
			})
		case "/manageKey":
			w.WriteHeader(http.StatusOK)
			json.NewEncoder(w).Encode(s2s.ICCOAManageKeyResponse{Status: "REVOKED"})
		case "/notifyKeyEvent":
			w.WriteHeader(http.StatusOK)
			json.NewEncoder(w).Encode(map[string]string{"status": "ok"})
		case "/share/genSession":
			w.WriteHeader(http.StatusOK)
			json.NewEncoder(w).Encode(s2s.ICCOAGenSessionResponse{
				SessionID: "s2s-session-001",
				ShareCode: "888888",
			})
		case "/share/sign":
			w.WriteHeader(http.StatusOK)
			json.NewEncoder(w).Encode(s2s.ICCOASignResponse{
				KeyID:  "s2s-key-signed-001",
				DkCert: "s2s-dk-cert",
			})
		case "/healthCheck":
			w.WriteHeader(http.StatusOK)
			json.NewEncoder(w).Encode(s2s.ICCOAHealthResponse{Status: "OK"})
		default:
			t.Errorf("unexpected path: %s", r.URL.Path)
			w.WriteHeader(http.StatusNotFound)
		}
	}))
}

func newICCOAAdapterWithMockS2S(mockSrvURL string) *ICCOAAdapter {
	logger := zap.NewNop()
	config := s2s.NewDefaultICCOAConfig("xiaomi", mockSrvURL, "VENDOR-OEM-001", "DEVICE-OEM-001")
	client := s2s.NewICCOAClient("xiaomi", config, logger)
	return NewICCOAAdapterWithClient("xiaomi", logger, client)
}

func TestICCOAAdapter_BindKey_WithS2S(t *testing.T) {
	mockSrv := newICCOAMockS2SServer(t)
	defer mockSrv.Close()

	a := newICCOAAdapterWithMockS2S(mockSrv.URL)
	resp, err := a.BindKey(context.Background(), &pb.BindKeyRequest{
		VehicleId:    "VH-ICCOA-001",
		DeviceId:     "DEV-ICCOA-001",
		UserId:       "U001",
		DevicePubkey: make([]byte, 64),
		KeyType:      pb.KeyType_OWNER,
	})
	if err != nil {
		t.Fatalf("BindKey failed: %v", err)
	}
	if resp.Key.KeyId != "s2s-key-tracked-001" {
		t.Errorf("expected s2s-key-tracked-001, got %s", resp.Key.KeyId)
	}
	if resp.Key.Protocol != pb.Protocol_ICCOA_DK40 {
		t.Errorf("expected ICCOA_DK40, got %v", resp.Key.Protocol)
	}
}

func TestICCOAAdapter_BindKey_StubMode(t *testing.T) {
	logger := zap.NewNop()
	a := NewICCOAAdapter("xiaomi", logger) // client = nil
	resp, err := a.BindKey(context.Background(), &pb.BindKeyRequest{
		VehicleId:    "VH-ICCOA-001",
		DeviceId:     "DEV-ICCOA-001",
		UserId:       "U001",
		DevicePubkey: make([]byte, 64),
		KeyType:      pb.KeyType_OWNER,
	})
	if err != nil {
		t.Fatalf("BindKey stub should not fail: %v", err)
	}
	if resp.Key.KeyId == "" {
		t.Error("expected non-empty key ID in stub mode")
	}
	if resp.Key.Protocol != pb.Protocol_ICCOA_DK40 {
		t.Errorf("expected ICCOA_DK40, got %v", resp.Key.Protocol)
	}
	if resp.Key.Status != pb.KeyStatus_ACTIVE {
		t.Errorf("expected ACTIVE status, got %v", resp.Key.Status)
	}
}

func TestICCOAAdapter_ShareKey_WithS2S(t *testing.T) {
	mockSrv := newICCOAMockS2SServer(t)
	defer mockSrv.Close()

	a := newICCOAAdapterWithMockS2S(mockSrv.URL)
	resp, err := a.ShareKey(context.Background(), &pb.CreateShareRequest{
		KeyId:      "key-001",
		FromUserId: "user-001",
		ToUserId:   "user-002",
	})
	if err != nil {
		t.Fatalf("ShareKey failed: %v", err)
	}
	if resp.ShareId != "s2s-session-001" {
		t.Errorf("expected s2s-session-001, got %s", resp.ShareId)
	}
	if resp.ShareCode != "888888" {
		t.Errorf("expected 888888, got %s", resp.ShareCode)
	}
}

func TestICCOAAdapter_ShareKey_StubMode(t *testing.T) {
	logger := zap.NewNop()
	a := NewICCOAAdapter("xiaomi", logger)
	resp, err := a.ShareKey(context.Background(), &pb.CreateShareRequest{
		KeyId: "key-001", FromUserId: "user-001",
	})
	if err != nil {
		t.Fatalf("ShareKey stub should not fail: %v", err)
	}
	if resp.ShareId == "" {
		t.Error("expected non-empty share ID in stub mode")
	}
	if resp.ShareCode == "" {
		t.Error("expected non-empty share code in stub mode")
	}
}

func TestICCOAAdapter_AcceptShare_WithS2S(t *testing.T) {
	mockSrv := newICCOAMockS2SServer(t)
	defer mockSrv.Close()

	a := newICCOAAdapterWithMockS2S(mockSrv.URL)
	resp, err := a.AcceptShare(context.Background(), &pb.AcceptShareRequest{
		ShareCode: "888888",
		DeviceId:  "DEV-002",
		UserId:    "user-002",
		DevicePubkey: make([]byte, 32),
	})
	if err != nil {
		t.Fatalf("AcceptShare failed: %v", err)
	}
	if resp.Key.KeyId != "s2s-key-signed-001" {
		t.Errorf("expected s2s-key-signed-001, got %s", resp.Key.KeyId)
	}
	if resp.Key.KeyType != pb.KeyType_FRIEND {
		t.Errorf("expected FRIEND, got %v", resp.Key.KeyType)
	}
}

func TestICCOAAdapter_AcceptShare_StubMode(t *testing.T) {
	logger := zap.NewNop()
	a := NewICCOAAdapter("xiaomi", logger)
	resp, err := a.AcceptShare(context.Background(), &pb.AcceptShareRequest{
		ShareCode: "123456",
	})
	if err != nil {
		t.Fatalf("AcceptShare stub should not fail: %v", err)
	}
	if resp.Key.KeyId == "" {
		t.Error("expected non-empty key ID in stub mode")
	}
	if resp.Key.KeyType != pb.KeyType_FRIEND {
		t.Errorf("expected FRIEND, got %v", resp.Key.KeyType)
	}
}

func TestICCOAAdapter_UnbindKey_WithS2S(t *testing.T) {
	mockSrv := newICCOAMockS2SServer(t)
	defer mockSrv.Close()

	a := newICCOAAdapterWithMockS2S(mockSrv.URL)
	err := a.UnbindKey(context.Background(), "key-001")
	if err != nil {
		t.Fatalf("UnbindKey failed: %v", err)
	}
}

func TestICCOAAdapter_UnbindKey_StubMode(t *testing.T) {
	logger := zap.NewNop()
	a := NewICCOAAdapter("xiaomi", logger)
	err := a.UnbindKey(context.Background(), "key-001")
	if err != nil {
		t.Fatalf("UnbindKey stub should not fail: %v", err)
	}
}

func TestICCOAAdapter_RevokeNotify_WithS2S(t *testing.T) {
	mockSrv := newICCOAMockS2SServer(t)
	defer mockSrv.Close()

	a := newICCOAAdapterWithMockS2S(mockSrv.URL)
	err := a.RevokeNotify(context.Background(), "key-001", "stolen")
	if err != nil {
		t.Fatalf("RevokeNotify failed: %v", err)
	}
}

func TestICCOAAdapter_RevokeNotify_StubMode(t *testing.T) {
	logger := zap.NewNop()
	a := NewICCOAAdapter("xiaomi", logger)
	err := a.RevokeNotify(context.Background(), "key-001", "stolen")
	if err != nil {
		t.Fatalf("RevokeNotify stub should not fail: %v", err)
	}
}

func TestICCOAAdapter_HealthCheck_WithS2S(t *testing.T) {
	mockSrv := newICCOAMockS2SServer(t)
	defer mockSrv.Close()

	a := newICCOAAdapterWithMockS2S(mockSrv.URL)
	status, err := a.HealthCheck(context.Background())
	if err != nil {
		t.Fatalf("HealthCheck failed: %v", err)
	}
	if !status.Healthy {
		t.Error("expected healthy")
	}
}

func TestICCOAAdapter_HealthCheck_StubMode(t *testing.T) {
	logger := zap.NewNop()
	a := NewICCOAAdapter("xiaomi", logger)
	status, err := a.HealthCheck(context.Background())
	if err != nil {
		t.Fatalf("HealthCheck stub should not fail: %v", err)
	}
	if !status.Healthy {
		t.Error("expected healthy in stub mode")
	}
}

func TestICCOAAdapter_Notify_WithS2S(t *testing.T) {
	mockSrv := newICCOAMockS2SServer(t)
	defer mockSrv.Close()

	a := newICCOAAdapterWithMockS2S(mockSrv.URL)
	err := a.Notify(context.Background(), "user-001", &pb.VehicleStatusUpdate{
		VehicleId: "VH-001",
	})
	if err != nil {
		t.Fatalf("Notify failed: %v", err)
	}
}

func TestICCOAAdapter_Notify_StubMode(t *testing.T) {
	logger := zap.NewNop()
	a := NewICCOAAdapter("xiaomi", logger)
	err := a.Notify(context.Background(), "user-001", &pb.VehicleStatusUpdate{})
	if err != nil {
		t.Fatalf("Notify stub should not fail: %v", err)
	}
}

func TestICCOAAdapter_BindKey_S2SFails_GracefulDegradation(t *testing.T) {
	// S2S returns 500 — adapter should fall back to stub result
	s2sAttempts := 0
	mockSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		s2sAttempts++
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusInternalServerError)
		json.NewEncoder(w).Encode(s2s.ICCOAAPIError{Code: 50001, Message: "server error"})
	}))
	defer mockSrv.Close()

	config := s2s.NewDefaultICCOAConfig("xiaomi", mockSrv.URL, "VENDOR-OEM-001", "DEVICE-OEM-001")
	config.RetryCount = 0 // no retry for speed
	config.RetryWait = 1 * time.Millisecond
	client := s2s.NewICCOAClient("xiaomi", config, zap.NewNop())
	a := NewICCOAAdapterWithClient("xiaomi", zap.NewNop(), client)

	resp, err := a.BindKey(context.Background(), &pb.BindKeyRequest{
		VehicleId:    "VH-ICCOA-001",
		DeviceId:     "DEV-ICCOA-001",
		UserId:       "U001",
		DevicePubkey: make([]byte, 64),
		KeyType:      pb.KeyType_OWNER,
	})
	// Should NOT fail — graceful degradation returns stub
	if err != nil {
		t.Fatalf("BindKey should gracefully degrade, not fail: %v", err)
	}
	if resp.Key.KeyId == "" {
		t.Error("expected stub key ID on S2S failure")
	}
	if s2sAttempts < 1 {
		t.Errorf("expected at least 1 S2S attempt, got %d", s2sAttempts)
	}
}

func TestICCOAAdapter_FullLifecycle_WithS2S(t *testing.T) {
	mockSrv := newICCOAMockS2SServer(t)
	defer mockSrv.Close()

	a := newICCOAAdapterWithMockS2S(mockSrv.URL)

	// BindKey
	bindResp, err := a.BindKey(context.Background(), &pb.BindKeyRequest{
		VehicleId: "VH-ICCOA-001", DeviceId: "DEV-ICCOA-001",
		UserId: "U001", DevicePubkey: make([]byte, 64),
		KeyType: pb.KeyType_OWNER,
	})
	if err != nil {
		t.Fatalf("BindKey failed: %v", err)
	}
	if bindResp.Key.KeyId != "s2s-key-tracked-001" {
		t.Errorf("expected s2s-key-tracked-001, got %s", bindResp.Key.KeyId)
	}

	// ShareKey
	shareResp, err := a.ShareKey(context.Background(), &pb.CreateShareRequest{
		KeyId: bindResp.Key.KeyId, FromUserId: "U001", ToUserId: "U002",
	})
	if err != nil {
		t.Fatalf("ShareKey failed: %v", err)
	}
	if shareResp.ShareId != "s2s-session-001" {
		t.Errorf("expected s2s-session-001, got %s", shareResp.ShareId)
	}

	// AcceptShare
	acceptResp, err := a.AcceptShare(context.Background(), &pb.AcceptShareRequest{
		ShareCode: shareResp.ShareCode, DeviceId: "DEV-002",
		UserId: "U002", DevicePubkey: make([]byte, 32),
	})
	if err != nil {
		t.Fatalf("AcceptShare failed: %v", err)
	}
	if acceptResp.Key.KeyId != "s2s-key-signed-001" {
		t.Errorf("expected s2s-key-signed-001, got %s", acceptResp.Key.KeyId)
	}
	if acceptResp.Key.KeyType != pb.KeyType_FRIEND {
		t.Errorf("expected FRIEND, got %v", acceptResp.Key.KeyType)
	}

	// UnbindKey
	if err := a.UnbindKey(context.Background(), bindResp.Key.KeyId); err != nil {
		t.Fatalf("UnbindKey failed: %v", err)
	}

	// HealthCheck
	status, err := a.HealthCheck(context.Background())
	if err != nil {
		t.Fatalf("HealthCheck failed: %v", err)
	}
	if !status.Healthy {
		t.Error("expected healthy")
	}
}

// ── ICCEAdapter: mock S2S client tests ──────────────────────

func newICCEMockS2SServer(t *testing.T) *httptest.Server {
	t.Helper()
	return httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		switch r.URL.Path {
		case "/bind":
			w.WriteHeader(http.StatusOK)
			json.NewEncoder(w).Encode(s2s.ICCEBindResponse{
				KeyID:         "s2s-key-bound-001",
				VehiclePubKey: "s2s-vehicle-pubkey",
				SharedSecret:  "s2s-shared-secret",
				Status:        "ACTIVE",
			})
		case "/unbind":
			w.WriteHeader(http.StatusOK)
		case "/revoke":
			w.WriteHeader(http.StatusOK)
		case "/share":
			w.WriteHeader(http.StatusOK)
			json.NewEncoder(w).Encode(s2s.ICCEShareResponse{
				ShareID:   "s2s-share-001",
				ShareCode: "999999",
			})
		case "/share/accept":
			w.WriteHeader(http.StatusOK)
			json.NewEncoder(w).Encode(s2s.ICCEBindResponse{
				KeyID:  "s2s-key-accepted-001",
				Status: "ACTIVE",
			})
		case "/health":
			w.WriteHeader(http.StatusOK)
			json.NewEncoder(w).Encode(s2s.ICCEHealthResponse{Status: "OK"})
		default:
			t.Errorf("unexpected path: %s", r.URL.Path)
			w.WriteHeader(http.StatusNotFound)
		}
	}))
}

func newICCEAdapterWithMockS2S(mockSrvURL string) *ICCEAdapter {
	logger := zap.NewNop()
	endpoint := s2s.ICCEEndpoint{
		BaseURL:    mockSrvURL,
		Timeout:    30 * time.Second,
		RetryCount: 0,
		RetryWait:  1 * time.Millisecond,
	}
	client := s2s.NewICCEClient("huawei", endpoint, logger)
	return NewICCEAdapterWithClient("huawei", logger, client)
}

func TestICCEAdapter_BindKey_WithS2S(t *testing.T) {
	mockSrv := newICCEMockS2SServer(t)
	defer mockSrv.Close()

	a := newICCEAdapterWithMockS2S(mockSrv.URL)
	resp, err := a.BindKey(context.Background(), &pb.BindKeyRequest{
		VehicleId: "VH-ICCE-001", DeviceId: "DEV-ICCE-001",
		UserId: "U001", DevicePubkey: make([]byte, 64),
		KeyType: pb.KeyType_OWNER,
	})
	if err != nil {
		t.Fatalf("BindKey failed: %v", err)
	}
	if resp.Key.KeyId != "s2s-key-bound-001" {
		t.Errorf("expected s2s-key-bound-001, got %s", resp.Key.KeyId)
	}
}

func TestICCEAdapter_UnbindKey_WithS2S(t *testing.T) {
	mockSrv := newICCEMockS2SServer(t)
	defer mockSrv.Close()

	a := newICCEAdapterWithMockS2S(mockSrv.URL)
	err := a.UnbindKey(context.Background(), "key-001")
	if err != nil {
		t.Fatalf("UnbindKey failed: %v", err)
	}
}

func TestICCEAdapter_RevokeNotify_WithS2S(t *testing.T) {
	mockSrv := newICCEMockS2SServer(t)
	defer mockSrv.Close()

	a := newICCEAdapterWithMockS2S(mockSrv.URL)
	err := a.RevokeNotify(context.Background(), "key-001", "test-revoke")
	if err != nil {
		t.Fatalf("RevokeNotify failed: %v", err)
	}
}

func TestICCEAdapter_ShareKey_WithS2S(t *testing.T) {
	mockSrv := newICCEMockS2SServer(t)
	defer mockSrv.Close()

	a := newICCEAdapterWithMockS2S(mockSrv.URL)
	resp, err := a.ShareKey(context.Background(), &pb.CreateShareRequest{
		KeyId: "key-001", FromUserId: "user-001",
	})
	if err != nil {
		t.Fatalf("ShareKey failed: %v", err)
	}
	if resp.ShareId != "s2s-share-001" {
		t.Errorf("expected s2s-share-001, got %s", resp.ShareId)
	}
	if resp.ShareCode != "999999" {
		t.Errorf("expected 999999, got %s", resp.ShareCode)
	}
}

func TestICCEAdapter_AcceptShare_WithS2S(t *testing.T) {
	mockSrv := newICCEMockS2SServer(t)
	defer mockSrv.Close()

	a := newICCEAdapterWithMockS2S(mockSrv.URL)
	resp, err := a.AcceptShare(context.Background(), &pb.AcceptShareRequest{
		ShareCode: "999999", DeviceId: "DEV-002", UserId: "user-002",
	})
	if err != nil {
		t.Fatalf("AcceptShare failed: %v", err)
	}
	if resp.Key.KeyId != "s2s-key-accepted-001" {
		t.Errorf("expected s2s-key-accepted-001, got %s", resp.Key.KeyId)
	}
}

func TestICCEAdapter_HealthCheck_WithS2S(t *testing.T) {
	mockSrv := newICCEMockS2SServer(t)
	defer mockSrv.Close()

	a := newICCEAdapterWithMockS2S(mockSrv.URL)
	status, err := a.HealthCheck(context.Background())
	if err != nil {
		t.Fatalf("HealthCheck failed: %v", err)
	}
	if !status.Healthy {
		t.Error("expected healthy")
	}
}

func TestICCEAdapter_FullLifecycle_WithS2S(t *testing.T) {
	mockSrv := newICCEMockS2SServer(t)
	defer mockSrv.Close()

	a := newICCEAdapterWithMockS2S(mockSrv.URL)

	// BindKey
	bindResp, err := a.BindKey(context.Background(), &pb.BindKeyRequest{
		VehicleId: "VH-ICCE-001", DeviceId: "DEV-ICCE-001",
		UserId: "U001", DevicePubkey: make([]byte, 64),
		KeyType: pb.KeyType_OWNER,
	})
	if err != nil {
		t.Fatalf("BindKey failed: %v", err)
	}
	if bindResp.Key.KeyId != "s2s-key-bound-001" {
		t.Errorf("expected s2s-key-bound-001, got %s", bindResp.Key.KeyId)
	}

	// ShareKey
	shareResp, err := a.ShareKey(context.Background(), &pb.CreateShareRequest{
		KeyId: bindResp.Key.KeyId, FromUserId: "U001",
	})
	if err != nil {
		t.Fatalf("ShareKey failed: %v", err)
	}
	if shareResp.ShareCode != "999999" {
		t.Errorf("expected 999999, got %s", shareResp.ShareCode)
	}

	// AcceptShare
	acceptResp, err := a.AcceptShare(context.Background(), &pb.AcceptShareRequest{
		ShareCode: shareResp.ShareCode, DeviceId: "DEV-002", UserId: "U002",
	})
	if err != nil {
		t.Fatalf("AcceptShare failed: %v", err)
	}
	if acceptResp.Key.KeyId != "s2s-key-accepted-001" {
		t.Errorf("expected s2s-key-accepted-001, got %s", acceptResp.Key.KeyId)
	}

	// UnbindKey
	if err := a.UnbindKey(context.Background(), bindResp.Key.KeyId); err != nil {
		t.Fatalf("UnbindKey failed: %v", err)
	}

	// RevokeNotify
	if err := a.RevokeNotify(context.Background(), bindResp.Key.KeyId, "owner_request"); err != nil {
		t.Fatalf("RevokeNotify failed: %v", err)
	}

	// HealthCheck
	status, err := a.HealthCheck(context.Background())
	if err != nil {
		t.Fatalf("HealthCheck failed: %v", err)
	}
	if !status.Healthy {
		t.Error("expected healthy")
	}
}

// ── Registry: cover ListStatus error path ──

// errAdapter simulates a failing adapter with HealthCheck error
type errAdapter struct {
	vendor   string
	protocol string
}

func (e *errAdapter) Vendor() string                                 { return e.vendor }
func (e *errAdapter) Protocol() string                               { return e.protocol }
func (e *errAdapter) BindKey(ctx context.Context, req *pb.BindKeyRequest) (*pb.BindKeyResponse, error) {
	return nil, nil
}
func (e *errAdapter) UnbindKey(ctx context.Context, keyID string) error { return nil }
func (e *errAdapter) RevokeNotify(ctx context.Context, keyID string, reason string) error { return nil }
func (e *errAdapter) ShareKey(ctx context.Context, req *pb.CreateShareRequest) (*pb.CreateShareResponse, error) {
	return nil, nil
}
func (e *errAdapter) AcceptShare(ctx context.Context, req *pb.AcceptShareRequest) (*pb.AcceptShareResponse, error) {
	return nil, nil
}
func (e *errAdapter) Notify(ctx context.Context, userID string, notification *pb.VehicleStatusUpdate) error {
	return nil
}
func (e *errAdapter) HealthCheck(ctx context.Context) (*pb.AdapterStatus, error) {
	return nil, assertAnError // returns an error
}

// assertAnError is a sentinel error for the errAdapter test
var assertAnError = &healthCheckError{msg: "adapter unavailable"}

type healthCheckError struct{ msg string }

func (e *healthCheckError) Error() string { return e.msg }

func TestRegistryListStatus_WithErrors(t *testing.T) {
	logger := zap.NewNop()
	r := NewRegistry(logger)

	r.Register("errVendor", "errProto", &errAdapter{vendor: "errVendor", protocol: "errProto"})
	r.Register("goodVendor", "goodProto", NewICCOAAdapter("goodVendor", logger))

	ctx := context.Background()
	statuses := r.ListStatus(ctx)

	if len(statuses) != 2 {
		t.Fatalf("expected 2 statuses, got %d", len(statuses))
	}

	foundErr := false
	for _, s := range statuses {
		if s.Vendor == "errVendor" {
			if s.Healthy {
				t.Error("expected errVendor to be unhealthy")
			}
			if s.ErrorMsg == "" {
				t.Error("expected errVendor to have error message")
			}
			foundErr = true
		}
		if s.Vendor == "goodVendor" {
			if !s.Healthy {
				t.Errorf("expected %s to be healthy", s.Vendor)
			}
		}
	}
	if !foundErr {
		t.Error("expected to find errVendor in statuses")
	}
}

func TestRegistryListStatus_Empty(t *testing.T) {
	logger := zap.NewNop()
	r := NewRegistry(logger)

	ctx := context.Background()
	statuses := r.ListStatus(ctx)
	if len(statuses) != 0 {
		t.Errorf("expected 0 statuses from empty registry, got %d", len(statuses))
	}
}

func TestRegistryConcurrentReadWrite(t *testing.T) {
	logger := zap.NewNop()
	r := NewRegistry(logger)

	// Pre-register one adapter
	r.Register("base", "proto", NewICCOAAdapter("base", logger))

	done := make(chan bool, 20)

	// Concurrent writes
	for i := 0; i < 5; i++ {
		go func(idx int) {
			vendor := map[int]string{0: "a", 1: "b", 2: "c", 3: "d", 4: "e"}[idx]
			r.Register(vendor, "proto", NewICCOAAdapter(vendor, logger))
			done <- true
		}(i)
	}

	// Concurrent reads
	for i := 0; i < 5; i++ {
		go func() {
			r.Get("base", "proto")
			r.GetByVendor("nonexistent")
			done <- true
		}()
	}

	for i := 0; i < 10; i++ {
		<-done
	}

	// Verify base adapter still available
	_, ok := r.Get("base", "proto")
	if !ok {
		t.Error("expected base adapter to still be accessible")
	}
}
