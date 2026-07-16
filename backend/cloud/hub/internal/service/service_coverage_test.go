package service

import (
	"context"
	"testing"
	"time"

	"go.uber.org/zap"

	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/adapter"
)

// ─────────────────────────────────────────────────────────────
// KeyStore tests (via InMemoryKeyStore)
// ─────────────────────────────────────────────────────────────

func TestInMemoryKeyStore_GetKeyOwner(t *testing.T) {
	s := NewInMemoryKeyStore()
	ctx := context.Background()

	key := &KeyRecord{
		KeyID:       "k-001",
		OwnerUserID: "u-001",
		VehicleID:   "v-001",
		Vendor:      "ICCE",
		Status:      "active",
		CreatedAt:   time.Now().UnixMilli(),
	}
	if err := s.SetKey(ctx, key); err != nil {
		t.Fatalf("SetKey: %v", err)
	}

	owner, err := s.GetKeyOwner(ctx, "k-001")
	if err != nil {
		t.Fatalf("GetKeyOwner: %v", err)
	}
	if owner != "u-001" {
		t.Errorf("Owner: want u-001, got %s", owner)
	}

	_, err = s.GetKeyOwner(ctx, "nonexistent")
	if err == nil {
		t.Errorf("Expected error for nonexistent key")
	}
}

func TestInMemoryKeyStore_GetKeyStatus(t *testing.T) {
	s := NewInMemoryKeyStore()
	ctx := context.Background()

	s.SetKey(ctx, &KeyRecord{
		KeyID:  "k-002",
		Status: "suspended",
	})

	status, err := s.GetKeyStatus(ctx, "k-002")
	if err != nil {
		t.Fatalf("GetKeyStatus: %v", err)
	}
	if status != "suspended" {
		t.Errorf("Status: want suspended, got %s", status)
	}

	_, err = s.GetKeyStatus(ctx, "nonexistent")
	if err == nil {
		t.Errorf("Expected error for nonexistent key")
	}
}

func TestInMemoryKeyStore_GetKeyRecord(t *testing.T) {
	s := NewInMemoryKeyStore()
	ctx := context.Background()

	now := time.Now().UnixMilli()
	key := &KeyRecord{
		KeyID:       "k-003",
		OwnerUserID: "u-003",
		VehicleID:   "v-003",
		Status:      "active",
		CreatedAt:   now,
	}
	s.SetKey(ctx, key)

	rec, err := s.GetKeyRecord(ctx, "k-003")
	if err != nil {
		t.Fatalf("GetKeyRecord: %v", err)
	}
	if rec.KeyID != "k-003" || rec.OwnerUserID != "u-003" {
		t.Errorf("Record mismatch: %+v", rec)
	}
	if rec.CreatedAt != now {
		t.Errorf("CreatedAt: want %d, got %d", now, rec.CreatedAt)
	}

	_, err = s.GetKeyRecord(ctx, "nonexistent")
	if err == nil {
		t.Errorf("Expected error for nonexistent key")
	}
}

func TestInMemoryKeyStore_SetKey(t *testing.T) {
	s := NewInMemoryKeyStore()
	ctx := context.Background()

	err := s.SetKey(ctx, &KeyRecord{
		KeyID:       "k-004",
		OwnerUserID: "u-004",
		Status:      "pending",
	})
	if err != nil {
		t.Fatalf("SetKey create: %v", err)
	}

	// Update same key
	err = s.SetKey(ctx, &KeyRecord{
		KeyID:       "k-004",
		OwnerUserID: "u-004",
		Status:      "active",
	})
	if err != nil {
		t.Fatalf("SetKey update: %v", err)
	}

	rec, _ := s.GetKeyRecord(ctx, "k-004")
	if rec.Status != "active" {
		t.Errorf("After update, status: want active, got %s", rec.Status)
	}
}

func TestInMemoryKeyStore_SetKeyStatus(t *testing.T) {
	s := NewInMemoryKeyStore()
	ctx := context.Background()

	s.SetKey(ctx, &KeyRecord{KeyID: "k-005", Status: "active"})

	if err := s.SetKeyStatus(ctx, "k-005", "revoked"); err != nil {
		t.Fatalf("SetKeyStatus: %v", err)
	}

	status, _ := s.GetKeyStatus(ctx, "k-005")
	if status != "revoked" {
		t.Errorf("After SetKeyStatus: want revoked, got %s", status)
	}

	if err := s.SetKeyStatus(ctx, "nonexistent", "active"); err == nil {
		t.Errorf("Expected error for nonexistent key")
	}
}

func TestInMemoryKeyStore_ListKeysByUser(t *testing.T) {
	s := NewInMemoryKeyStore()
	ctx := context.Background()

	for i := 0; i < 3; i++ {
		keyID := string(rune('a' + i))
		s.SetKey(ctx, &KeyRecord{
			KeyID:       "u1-" + keyID,
			OwnerUserID: "user-1",
			Status:      "active",
		})
	}
	s.SetKey(ctx, &KeyRecord{
		KeyID:       "u2-a",
		OwnerUserID: "user-2",
		Status:      "active",
	})

	keys, err := s.ListKeysByUser(ctx, "user-1")
	if err != nil {
		t.Fatalf("ListKeysByUser: %v", err)
	}
	if len(keys) != 3 {
		t.Errorf("user-1 keys: want 3, got %d", len(keys))
	}

	keys, err = s.ListKeysByUser(ctx, "nonexistent")
	if err != nil {
		t.Fatalf("ListKeysByUser: %v", err)
	}
	if len(keys) != 0 {
		t.Errorf("nonexistent user keys: want 0, got %d", len(keys))
	}
}

// ─────────────────────────────────────────────────────────────
// KeyManagementService tests
// ─────────────────────────────────────────────────────────────

func TestNewKeyManagementService(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	defer logger.Sync()

	reg := adapter.NewRegistry(logger)
	s := NewKeyManagementService(reg, logger)
	if s == nil {
		t.Fatal("NewKeyManagementService returned nil")
	}
}

func TestKeyManagementService_WithKeyStore(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	defer logger.Sync()

	reg := adapter.NewRegistry(logger)
	s := NewKeyManagementService(reg, logger)
	ks := NewInMemoryKeyStore()
	s.WithKeyStore(ks)

	ctx := context.Background()
	ks.SetKey(ctx, &KeyRecord{
		KeyID:       "k-svc-001",
		OwnerUserID: "u-svc-001",
		Status:      "active",
	})

	owner, err := s.keyStore.GetKeyOwner(ctx, "k-svc-001")
	if err != nil {
		t.Fatalf("GetKeyOwner via service: %v", err)
	}
	if owner != "u-svc-001" {
		t.Errorf("Owner: want u-svc-001, got %s", owner)
	}
}

func TestKeyManagementService_WithPushService(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	defer logger.Sync()

	reg := adapter.NewRegistry(logger)
	s := NewKeyManagementService(reg, logger)
	s.WithPushService(&mockPushService{})
	if s.pushService == nil {
		t.Errorf("pushService should not be nil after WithPushService")
	}
}

func TestNewInMemoryKeyStore(t *testing.T) {
	ks := NewInMemoryKeyStore()
	if ks == nil {
		t.Fatal("NewInMemoryKeyStore returned nil")
	}
	if ks.data == nil {
		t.Errorf("data map should be initialized")
	}
}

// ─────────────────────────────────────────────────────────────
// DeviceService tests
// ─────────────────────────────────────────────────────────────

func TestDeviceService_RegisterDevice(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	defer logger.Sync()

	ds := NewDeviceService(logger)
	if ds == nil {
		t.Fatal("NewDeviceService returned nil")
	}

	ctx := context.Background()
	dev, err := ds.RegisterDevice(ctx, "user-001", DeviceCapabilities{
		BLE:           true,
		UWB:           true,
		NFC:           true,
		SecureElement: true,
		Platform:      "ios",
		Model:         "iPhone 15 Pro",
		OSVersion:     "18.0",
		AppVersion:    "1.0.0",
	})
	if err != nil {
		t.Fatalf("RegisterDevice: %v", err)
	}
	if dev.DeviceID == "" {
		t.Errorf("DeviceID should not be empty")
	}
	if dev.UserID != "user-001" {
		t.Errorf("UserID: want user-001, got %s", dev.UserID)
	}
	if !dev.Capabilities.BLE {
		t.Errorf("BLE capability should be true")
	}
	if !dev.Capabilities.UWB {
		t.Errorf("UWB capability should be true")
	}
}

func TestDeviceService_RegisterMaxDevices(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	defer logger.Sync()

	ds := NewDeviceService(logger)
	ctx := context.Background()

	for i := 0; i < 5; i++ {
		_, err := ds.RegisterDevice(ctx, "user-max", DeviceCapabilities{
			BLE:      true,
			Platform: "android",
			Model:    "device-" + string(rune('0'+i)),
		})
		if err != nil {
			t.Fatalf("RegisterDevice %d: %v", i, err)
		}
	}

	_, err := ds.RegisterDevice(ctx, "user-max", DeviceCapabilities{
		BLE:      true,
		Platform: "android",
		Model:    "device-6",
	})
	if err == nil {
		t.Errorf("Expected error for exceeding device limit")
	}
}

// ─────────────────────────────────────────────────────────────
// Mock push service for testing
// ─────────────────────────────────────────────────────────────

type mockPushService struct{}

func (m *mockPushService) SendPush(_ context.Context, userID string, payload *PushPayload) error {
	return nil
}
