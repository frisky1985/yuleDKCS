package adapter

import (
	"context"
	"testing"
	"time"

	"go.uber.org/zap"
	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
)

func TestNewRegistry(t *testing.T) {
	logger := zap.NewNop()
	r := NewRegistry(logger)
	if r == nil {
		t.Fatal("NewRegistry returned nil")
	}
}

func TestRegistryRegisterAndGet(t *testing.T) {
	logger := zap.NewNop()
	r := NewRegistry(logger)

	// Register an adapter
	a := NewICCOAAdapter("xiaomi", logger)
	r.Register("xiaomi", "iccoa_dk40", a)

	// Get by vendor+protocol
	got, ok := r.Get("xiaomi", "iccoa_dk40")
	if !ok {
		t.Fatal("expected to find adapter")
	}
	if got.Vendor() != "xiaomi" {
		t.Errorf("expected vendor xiaomi, got %s", got.Vendor())
	}
	if got.Protocol() != "iccoa_dk40" {
		t.Errorf("expected protocol iccoa_dk40, got %s", got.Protocol())
	}
}

func TestRegistryGetNotFound(t *testing.T) {
	logger := zap.NewNop()
	r := NewRegistry(logger)

	_, ok := r.Get("nonexistent", "proto")
	if ok {
		t.Fatal("expected not found")
	}
}

func TestRegistryGetByVendor(t *testing.T) {
	logger := zap.NewNop()
	r := NewRegistry(logger)

	r.Register("xiaomi", "iccoa_dk40", NewICCOAAdapter("xiaomi", logger))
	r.Register("apple", "ccc_dk3", NewCCCAdapter("apple", logger))

	// Find by vendor
	a, ok := r.GetByVendor("xiaomi")
	if !ok {
		t.Fatal("expected to find xiaomi adapter")
	}
	if a.Vendor() != "xiaomi" {
		t.Errorf("expected xiaomi, got %s", a.Vendor())
	}

	// Find another vendor
	a, ok = r.GetByVendor("apple")
	if !ok {
		t.Fatal("expected to find apple adapter")
	}
	if a.Vendor() != "apple" {
		t.Errorf("expected apple, got %s", a.Vendor())
	}

	// Non-existent vendor
	_, ok = r.GetByVendor("samsung")
	if ok {
		t.Fatal("expected not found for unknown vendor")
	}
}

func TestRegistryListStatus(t *testing.T) {
	logger := zap.NewNop()
	r := NewRegistry(logger)

	r.Register("xiaomi", "iccoa_dk40", NewICCOAAdapter("xiaomi", logger))
	r.Register("huawei", "icce", NewICCEAdapter("huawei", logger))
	r.Register("apple", "ccc_dk3", NewCCCAdapter("apple", logger))

	ctx := context.Background()
	statuses := r.ListStatus(ctx)
	if len(statuses) != 3 {
		t.Fatalf("expected 3 statuses, got %d", len(statuses))
	}

	// Verify each status
	vendorSet := make(map[string]bool)
	for _, s := range statuses {
		vendorSet[s.Vendor] = true
		if !s.Healthy {
			t.Errorf("expected healthy for %s", s.Vendor)
		}
	}

	if !vendorSet["xiaomi"] || !vendorSet["huawei"] || !vendorSet["apple"] {
		t.Errorf("missing vendors in status, got %v", vendorSet)
	}
}

func TestRegistryConcurrency(t *testing.T) {
	logger := zap.NewNop()
	r := NewRegistry(logger)

	// Concurrent register and reads
	done := make(chan bool)
	const goroutines = 10

	for i := 0; i < goroutines; i++ {
		go func() {
			switch i % 3 {
			case 0:
				r.Register("xiaomi", "iccoa_dk40", NewICCOAAdapter("xiaomi", logger))
			case 1:
				r.Register("huawei", "icce", NewICCEAdapter("huawei", logger))
			case 2:
				r.Register("apple", "ccc_dk3", NewCCCAdapter("apple", logger))
			}
			done <- true
		}()
	}

	for i := 0; i < goroutines; i++ {
		<-done
	}

	// Verify no panics and we can still read
	_, ok := r.Get("xiaomi", "iccoa_dk40")
	if !ok {
		t.Error("expected to find xiaomi after concurrent operations")
	}
}

func TestNewICCOAAdapter(t *testing.T) {
	logger := zap.NewNop()
	a := NewICCOAAdapter("oppo", logger)
	if a == nil {
		t.Fatal("NewICCOAAdapter returned nil")
	}
	if a.Vendor() != "oppo" {
		t.Errorf("expected vendor oppo, got %s", a.Vendor())
	}
	if a.Protocol() != "iccoa_dk40" {
		t.Errorf("expected protocol iccoa_dk40, got %s", a.Protocol())
	}
}

func TestICCOAAdapterBindKey(t *testing.T) {
	logger := zap.NewNop()
	a := NewICCOAAdapter("xiaomi", logger)

	req := &pb.BindKeyRequest{
		VehicleId: "VH001",
		DeviceId:  "DEV001",
		UserId:    "U001",
		KeyType:   pb.KeyType_OWNER,
		AccessLevel: &pb.AccessLevel{
			Lock: true, Unlock: true, Engine: true,
		},
		ValidFrom:  time.Now().Unix(),
		ValidUntil: time.Now().Add(365 * 24 * time.Hour).Unix(),
	}

	ctx := context.Background()
	resp, err := a.BindKey(ctx, req)
	if err != nil {
		t.Fatalf("BindKey failed: %v", err)
	}
	if resp.Key == nil {
		t.Fatal("expected non-nil key in response")
	}
	if resp.Key.VehicleId != "VH001" {
		t.Errorf("expected vehicle VH001, got %s", resp.Key.VehicleId)
	}
	if resp.Key.Protocol != pb.Protocol_ICCOA_DK40 {
		t.Errorf("expected ICCOA_DK40 protocol, got %v", resp.Key.Protocol)
	}
	if resp.Key.KeyType != pb.KeyType_OWNER {
		t.Errorf("expected OWNER key type, got %v", resp.Key.KeyType)
	}
	if len(resp.VehiclePubkey) != 64 {
		t.Errorf("expected 64-byte vehicle pubkey, got %d", len(resp.VehiclePubkey))
	}
	if len(resp.SharedSecret) != 32 {
		t.Errorf("expected 32-byte shared secret, got %d", len(resp.SharedSecret))
	}
}

func TestICCOAAdapterUnbindKey(t *testing.T) {
	logger := zap.NewNop()
	a := NewICCOAAdapter("xiaomi", logger)
	err := a.UnbindKey(context.Background(), "key-1")
	if err != nil {
		t.Fatalf("UnbindKey failed: %v", err)
	}
}

func TestICCOAAdapterShareKey(t *testing.T) {
	logger := zap.NewNop()
	a := NewICCOAAdapter("xiaomi", logger)
	req := &pb.CreateShareRequest{
		KeyId:      "key-1",
		FromUserId: "user-1",
		ToVendor:   pb.PhoneVendor_VIVO,
	}
	resp, err := a.ShareKey(context.Background(), req)
	if err != nil {
		t.Fatalf("ShareKey failed: %v", err)
	}
	if resp.ShareId == "" {
		t.Error("expected non-empty share ID")
	}
	if resp.ShareCode == "" {
		t.Error("expected non-empty share code")
	}
}

func TestICCOAAdapterAcceptShare(t *testing.T) {
	logger := zap.NewNop()
	a := NewICCOAAdapter("xiaomi", logger)
	req := &pb.AcceptShareRequest{
		ShareCode: "123456",
		DeviceId:  "dev-2",
		UserId:    "user-2",
		Vendor:    pb.PhoneVendor_VIVO,
	}
	resp, err := a.AcceptShare(context.Background(), req)
	if err != nil {
		t.Fatalf("AcceptShare failed: %v", err)
	}
	if resp.Key == nil {
		t.Fatal("expected non-nil key")
	}
	if resp.Key.KeyType != pb.KeyType_FRIEND {
		t.Errorf("expected FRIEND key type, got %v", resp.Key.KeyType)
	}
}

func TestICCOAAdapterHealthCheck(t *testing.T) {
	logger := zap.NewNop()
	a := NewICCOAAdapter("xiaomi", logger)
	status, err := a.HealthCheck(context.Background())
	if err != nil {
		t.Fatalf("HealthCheck failed: %v", err)
	}
	if !status.Healthy {
		t.Error("expected healthy")
	}
	if status.Vendor != "xiaomi" {
		t.Errorf("expected vendor xiaomi, got %s", status.Vendor)
	}
}

func TestICCOAAdapterNotifyAndRevoke(t *testing.T) {
	logger := zap.NewNop()
	a := NewICCOAAdapter("xiaomi", logger)

	err := a.RevokeNotify(context.Background(), "key-1", "stolen")
	if err != nil {
		t.Fatalf("RevokeNotify failed: %v", err)
	}

	err = a.Notify(context.Background(), "user-1", &pb.VehicleStatusUpdate{})
	if err != nil {
		t.Fatalf("Notify failed: %v", err)
	}
}

func TestNewICCEAdapter(t *testing.T) {
	logger := zap.NewNop()
	a := NewICCEAdapter("huawei", logger)
	if a == nil {
		t.Fatal("NewICCEAdapter returned nil")
	}
	if a.Vendor() != "huawei" {
		t.Errorf("expected vendor huawei, got %s", a.Vendor())
	}
	if a.Protocol() != "icce" {
		t.Errorf("expected protocol icce, got %s", a.Protocol())
	}
}

func TestICCEAdapterBindKey(t *testing.T) {
	logger := zap.NewNop()
	a := NewICCEAdapter("huawei", logger)
	req := &pb.BindKeyRequest{
		VehicleId: "VH002",
		UserId:    "U002",
		KeyType:   pb.KeyType_OWNER,
	}
	resp, err := a.BindKey(context.Background(), req)
	if err != nil {
		t.Fatalf("BindKey failed: %v", err)
	}
	if resp.Key.Protocol != pb.Protocol_ICCE {
		t.Errorf("expected ICCE protocol, got %v", resp.Key.Protocol)
	}
}

func TestICCEAdapterHealthCheck(t *testing.T) {
	logger := zap.NewNop()
	a := NewICCEAdapter("huawei", logger)
	status, err := a.HealthCheck(context.Background())
	if err != nil {
		t.Fatalf("HealthCheck failed: %v", err)
	}
	if !status.Healthy {
		t.Error("expected healthy")
	}
	if status.Protocol != "icce" {
		t.Errorf("expected protocol icce, got %s", status.Protocol)
	}
}

func TestNewCCCAdapter(t *testing.T) {
	logger := zap.NewNop()
	a := NewCCCAdapter("apple", logger)
	if a == nil {
		t.Fatal("NewCCCAdapter returned nil")
	}
	if a.Vendor() != "apple" {
		t.Errorf("expected vendor apple, got %s", a.Vendor())
	}
	if a.Protocol() != "ccc_dk3" {
		t.Errorf("expected protocol ccc_dk3, got %s", a.Protocol())
	}
}

func TestCCCAdapterBindKey(t *testing.T) {
	logger := zap.NewNop()
	a := NewCCCAdapter("apple", logger)
	req := &pb.BindKeyRequest{
		VehicleId: "VH003",
		UserId:    "U003",
		KeyType:   pb.KeyType_OWNER,
		AccessLevel: &pb.AccessLevel{
			Lock: true, Unlock: true, Engine: true,
		},
	}
	resp, err := a.BindKey(context.Background(), req)
	if err != nil {
		t.Fatalf("BindKey failed: %v", err)
	}
	if resp.Key.Protocol != pb.Protocol_CCC_DK3 {
		t.Errorf("expected CCC_DK3 protocol, got %v", resp.Key.Protocol)
	}
}

func TestCCCAdapterShareAndAccept(t *testing.T) {
	logger := zap.NewNop()
	a := NewCCCAdapter("samsung", logger)

	// Share
	shareResp, err := a.ShareKey(context.Background(), &pb.CreateShareRequest{KeyId: "key-2"})
	if err != nil {
		t.Fatalf("ShareKey failed: %v", err)
	}
	if shareResp.ShareId == "" {
		t.Error("expected non-empty share ID")
	}

	// Accept
	acceptResp, err := a.AcceptShare(context.Background(), &pb.AcceptShareRequest{
		ShareCode: "654321",
		Vendor:    pb.PhoneVendor_SAMSUNG,
	})
	if err != nil {
		t.Fatalf("AcceptShare failed: %v", err)
	}
	if acceptResp.Key.Protocol != pb.Protocol_CCC_DK3 {
		t.Errorf("expected CCC_DK3 protocol, got %v", acceptResp.Key.Protocol)
	}
}

func TestCCCAdapterHealthCheck(t *testing.T) {
	logger := zap.NewNop()
	a := NewCCCAdapter("apple", logger)
	status, err := a.HealthCheck(context.Background())
	if err != nil {
		t.Fatalf("HealthCheck failed: %v", err)
	}
	if !status.Healthy {
		t.Error("expected healthy")
	}
}
