package adapter

import (
	"context"
	"testing"

	"go.uber.org/zap"
	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
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
