package service

import (
	"context"
	"fmt"
	"sync"
	"testing"
	"time"

	"go.uber.org/zap"
)

func TestNewDeviceService(t *testing.T) {
	logger := zap.NewNop()
	s := NewDeviceService(logger)
	if s == nil {
		t.Fatal("NewDeviceService returned nil")
	}
}

func TestDeviceService_RegisterDevice(t *testing.T) {
	logger := zap.NewNop()
	s := NewDeviceService(logger)

	caps := DeviceCapabilities{
		BLE: true, UWB: true, NFC: true, SecureElement: true,
		Platform: "ios", Model: "iPhone 15 Pro", OSVersion: "17.0", AppVersion: "1.0.0",
	}

	dev, err := s.RegisterDevice(context.Background(), "user-1", caps)
	if err != nil {
		t.Fatalf("RegisterDevice failed: %v", err)
	}
	if dev.DeviceID == "" {
		t.Error("expected non-empty DeviceID")
	}
	if dev.UserID != "user-1" {
		t.Errorf("expected user-1, got %s", dev.UserID)
	}
	if dev.RegisteredAt == 0 {
		t.Error("expected non-zero RegisteredAt")
	}
	if dev.LastSeen != dev.RegisteredAt {
		t.Error("expected LastSeen to equal RegisteredAt on registration")
	}
}

func TestDeviceService_RegisterDevice_LimitExceeded(t *testing.T) {
	logger := zap.NewNop()
	s := NewDeviceService(logger)

	ctx := context.Background()
	caps := DeviceCapabilities{Platform: "android", Model: "Pixel 8"}

	for i := 0; i < 5; i++ {
		_, err := s.RegisterDevice(ctx, "user-1", caps)
		if err != nil {
			t.Fatalf("RegisterDevice %d failed: %v", i+1, err)
		}
	}

	// Should fail on 6th device
	_, err := s.RegisterDevice(ctx, "user-1", caps)
	if err == nil {
		t.Fatal("expected error for exceeding device limit")
	}
}

func TestDeviceService_GetDevice(t *testing.T) {
	logger := zap.NewNop()
	s := NewDeviceService(logger)

	dev, _ := s.RegisterDevice(context.Background(), "user-1",
		DeviceCapabilities{Platform: "ios", Model: "iPhone 15"})

	got, err := s.GetDevice(context.Background(), dev.DeviceID)
	if err != nil {
		t.Fatalf("GetDevice failed: %v", err)
	}
	if got.DeviceID != dev.DeviceID {
		t.Errorf("expected %s, got %s", dev.DeviceID, got.DeviceID)
	}
}

func TestDeviceService_GetDevice_NotFound(t *testing.T) {
	logger := zap.NewNop()
	s := NewDeviceService(logger)

	_, err := s.GetDevice(context.Background(), "nonexistent")
	if err == nil {
		t.Fatal("expected error for nonexistent device")
	}
}

func TestDeviceService_ListUserDevices(t *testing.T) {
	logger := zap.NewNop()
	s := NewDeviceService(logger)

	ctx := context.Background()
	caps := DeviceCapabilities{Platform: "android"}

	s.RegisterDevice(ctx, "user-1", caps)
	s.RegisterDevice(ctx, "user-1", caps)
	s.RegisterDevice(ctx, "user-2", caps)

	devices, err := s.ListUserDevices(ctx, "user-1")
	if err != nil {
		t.Fatalf("ListUserDevices failed: %v", err)
	}
	if len(devices) != 2 {
		t.Errorf("expected 2 devices for user-1, got %d", len(devices))
	}
}

func TestDeviceService_ListUserDevices_Empty(t *testing.T) {
	logger := zap.NewNop()
	s := NewDeviceService(logger)

	devices, err := s.ListUserDevices(context.Background(), "nobody")
	if err != nil {
		t.Fatalf("ListUserDevices failed: %v", err)
	}
	if len(devices) != 0 {
		t.Errorf("expected 0 devices, got %d", len(devices))
	}
}

func TestDeviceService_ProvisionKey(t *testing.T) {
	logger := zap.NewNop()
	s := NewDeviceService(logger)

	ctx := context.Background()
	dev, _ := s.RegisterDevice(ctx, "user-1", DeviceCapabilities{Platform: "ios"})

	binding, err := s.ProvisionKey(ctx, "user-1", dev.DeviceID, "VH001", []string{"iccoa_dk40"})
	if err != nil {
		t.Fatalf("ProvisionKey failed: %v", err)
	}
	if binding.KeyID == "" {
		t.Error("expected non-empty KeyID")
	}
	if binding.Status != "active" {
		t.Errorf("expected active, got %s", binding.Status)
	}
	if binding.VehicleID != "VH001" {
		t.Errorf("expected VH001, got %s", binding.VehicleID)
	}
}

func TestDeviceService_ProvisionKey_DeviceNotFound(t *testing.T) {
	logger := zap.NewNop()
	s := NewDeviceService(logger)

	_, err := s.ProvisionKey(context.Background(), "user-1", "nonexistent", "VH001", nil)
	if err == nil {
		t.Fatal("expected error for nonexistent device")
	}
}

func TestDeviceService_ProvisionKey_WrongUser(t *testing.T) {
	logger := zap.NewNop()
	s := NewDeviceService(logger)

	dev, _ := s.RegisterDevice(context.Background(), "user-1", DeviceCapabilities{Platform: "ios"})

	_, err := s.ProvisionKey(context.Background(), "user-2", dev.DeviceID, "VH001", nil)
	if err == nil {
		t.Fatal("expected error for wrong user")
	}
}

func TestDeviceService_ProvisionKey_ExistingBinding(t *testing.T) {
	logger := zap.NewNop()
	s := NewDeviceService(logger)

	ctx := context.Background()
	dev, _ := s.RegisterDevice(ctx, "user-1", DeviceCapabilities{Platform: "ios"})

	// First provisioning
	b1, err := s.ProvisionKey(ctx, "user-1", dev.DeviceID, "VH001", nil)
	if err != nil {
		t.Fatalf("First ProvisionKey failed: %v", err)
	}

	// Second provisioning for same vehicle should return existing
	b2, err := s.ProvisionKey(ctx, "user-1", dev.DeviceID, "VH001", nil)
	if err != nil {
		t.Fatalf("Second ProvisionKey failed: %v", err)
	}
	if b2.KeyID != b1.KeyID {
		t.Error("expected same KeyID for existing binding")
	}
}

func TestDeviceService_ProvisionKey_PlatformFallback(t *testing.T) {
	logger := zap.NewNop()
	s := NewDeviceService(logger)

	ctx := context.Background()
	dev, _ := s.RegisterDevice(ctx, "user-1", DeviceCapabilities{Platform: ""})

	binding, _ := s.ProvisionKey(ctx, "user-1", dev.DeviceID, "VH001", nil)
	if binding.KeyID == "" {
		t.Error("expected non-empty KeyID")
	}
}

func TestDeviceService_RevokeDeviceKeys(t *testing.T) {
	logger := zap.NewNop()
	s := NewDeviceService(logger)

	ctx := context.Background()
	dev, _ := s.RegisterDevice(ctx, "user-1", DeviceCapabilities{Platform: "android"})
	s.ProvisionKey(ctx, "user-1", dev.DeviceID, "VH001", nil)
	s.ProvisionKey(ctx, "user-1", dev.DeviceID, "VH002", nil)

	revoked, err := s.RevokeDeviceKeys(ctx, "user-1", dev.DeviceID)
	if err != nil {
		t.Fatalf("RevokeDeviceKeys failed: %v", err)
	}
	if len(revoked) != 2 {
		t.Errorf("expected 2 revoked keys, got %d", len(revoked))
	}
	for _, b := range revoked {
		if b.Status != "revoked" {
			t.Errorf("expected revoked, got %s", b.Status)
		}
	}
}

func TestDeviceService_RevokeDeviceKeys_DeviceNotFound(t *testing.T) {
	logger := zap.NewNop()
	s := NewDeviceService(logger)

	_, err := s.RevokeDeviceKeys(context.Background(), "user-1", "nonexistent")
	if err == nil {
		t.Fatal("expected error for nonexistent device")
	}
}

func TestDeviceService_RevokeDeviceKeys_WrongUser(t *testing.T) {
	logger := zap.NewNop()
	s := NewDeviceService(logger)

	dev, _ := s.RegisterDevice(context.Background(), "user-1", DeviceCapabilities{})

	_, err := s.RevokeDeviceKeys(context.Background(), "user-2", dev.DeviceID)
	if err == nil {
		t.Fatal("expected error for wrong user")
	}
}

func TestDeviceService_DeleteDevice(t *testing.T) {
	logger := zap.NewNop()
	s := NewDeviceService(logger)

	ctx := context.Background()
	dev, _ := s.RegisterDevice(ctx, "user-1", DeviceCapabilities{Platform: "ios"})
	s.ProvisionKey(ctx, "user-1", dev.DeviceID, "VH001", nil)

	err := s.DeleteDevice(ctx, "user-1", dev.DeviceID)
	if err != nil {
		t.Fatalf("DeleteDevice failed: %v", err)
	}

	// Device should be gone
	_, err = s.GetDevice(ctx, dev.DeviceID)
	if err == nil {
		t.Error("expected device to be deleted")
	}

	// User device list should be empty
	devices, _ := s.ListUserDevices(ctx, "user-1")
	if len(devices) != 0 {
		t.Errorf("expected 0 devices, got %d", len(devices))
	}
}

func TestDeviceService_DeleteDevice_NotFound(t *testing.T) {
	logger := zap.NewNop()
	s := NewDeviceService(logger)

	err := s.DeleteDevice(context.Background(), "user-1", "nonexistent")
	if err == nil {
		t.Fatal("expected error for nonexistent device")
	}
}

func TestDeviceService_DeleteDevice_WrongUser(t *testing.T) {
	logger := zap.NewNop()
	s := NewDeviceService(logger)

	dev, _ := s.RegisterDevice(context.Background(), "user-1", DeviceCapabilities{})

	err := s.DeleteDevice(context.Background(), "user-2", dev.DeviceID)
	if err == nil {
		t.Fatal("expected error for wrong user")
	}
}

func TestDeviceService_ProvisionKey_KeyIDFormat(t *testing.T) {
	logger := zap.NewNop()
	s := NewDeviceService(logger)
	ctx := context.Background()

	dev, _ := s.RegisterDevice(ctx, "user-1", DeviceCapabilities{Platform: "ios"})
	binding, _ := s.ProvisionKey(ctx, "user-1", dev.DeviceID, "VH001", nil)

	if len(binding.KeyID) < 10 {
		t.Errorf("KeyID looks too short: %s", binding.KeyID)
	}
}

func TestDeviceService_ProvisionKey_UpdatesLastSeen(t *testing.T) {
	logger := zap.NewNop()
	s := NewDeviceService(logger)
	ctx := context.Background()

	dev, _ := s.RegisterDevice(ctx, "user-1", DeviceCapabilities{Platform: "android"})
	originalLastSeen := dev.LastSeen

	time.Sleep(time.Millisecond) // Ensure time tick
	s.ProvisionKey(ctx, "user-1", dev.DeviceID, "VH001", nil)

	if dev.LastSeen <= originalLastSeen {
		t.Error("expected LastSeen to be updated after provisioning")
	}
}

func TestDeviceService_Concurrency(t *testing.T) {
	logger := zap.NewNop()
	s := NewDeviceService(logger)
	ctx := context.Background()

	var wg sync.WaitGroup
	for i := 0; i < 10; i++ {
		wg.Add(1)
		go func(n int) {
			defer wg.Done()
			userID := fmt.Sprintf("user-%d", n%3)
			caps := DeviceCapabilities{Platform: "android", Model: fmt.Sprintf("Device-%d", n)}
			dev, err := s.RegisterDevice(ctx, userID, caps)
			if err != nil {
				return
			}
			_, _ = s.ProvisionKey(ctx, userID, dev.DeviceID, "VH001", nil)
			_, _ = s.GetDevice(ctx, dev.DeviceID)
			_, _ = s.ListUserDevices(ctx, userID)
		}(i)
	}
	wg.Wait()

	// Should not panic
	devices, _ := s.ListUserDevices(ctx, "user-0")
	_ = devices
}

func TestDeviceService_ImplementsDeviceCapabilities(t *testing.T) {
	// Just verify the types exist and can be used
	caps := DeviceCapabilities{
		BLE: true, UWB: true, NFC: true, SecureElement: true,
		Platform: "ios", Model: "iPhone 15", OSVersion: "17.0",
		AppVersion: "1.0", UWBVersion: "1.0",
	}
	if !caps.BLE {
		t.Error("BLE should be true")
	}
}
