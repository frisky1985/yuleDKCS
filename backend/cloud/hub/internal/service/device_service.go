package service

import (
	"context"
	"fmt"
	"sync"
	"time"

	"go.uber.org/zap"
)

// ─── Device Model ────────────────────────────────────────────────────────────

// DeviceCapabilities describes what a device can do
type DeviceCapabilities struct {
	BLE            bool   `json:"ble"`
	UWB            bool   `json:"uwb"`
	NFC            bool   `json:"nfc"`
	SecureElement  bool   `json:"se"`
	Platform       string `json:"platform"`        // "ios" / "android"
	Model          string `json:"model"`            // e.g. "iPhone 15 Pro"
	OSVersion      string `json:"os_version"`       // e.g. "15.0"
	AppVersion     string `json:"app_version"`
	UWBVersion     string `json:"uwb_version,omitempty"`
	UWBAccuracyMM  int32  `json:"uwb_accuracy_mm,omitempty"`
}

// Device represents a registered device
type Device struct {
	DeviceID     string              `json:"device_id"`
	UserID       string              `json:"user_id"`
	Capabilities DeviceCapabilities `json:"capabilities"`
	LastSeen     int64               `json:"last_seen"`
	RegisteredAt int64               `json:"registered_at"`
}

// DeviceKeyBinding links a key to a device
type DeviceKeyBinding struct {
	ID        string `json:"id"`
	KeyID     string `json:"key_id"`
	DeviceID  string `json:"device_id"`
	UserID    string `json:"user_id"`
	VehicleID string `json:"vehicle_id"`
	Status    string `json:"status"` // "active" | "revoked"
	BoundAt   int64  `json:"bound_at"`
}

// ─── DeviceService ───────────────────────────────────────────────────────────

// DeviceService manages multi-device key provisioning
type DeviceService struct {
	logger *zap.Logger

	mu      sync.RWMutex
	devices map[string]*Device              // device_id → Device
	bindings map[string][]*DeviceKeyBinding // device_id → bindings
	userDevices map[string][]string         // user_id → device_ids
}

// NewDeviceService creates a new device service
func NewDeviceService(logger *zap.Logger) *DeviceService {
	return &DeviceService{
		logger:      logger.With(zap.String("service", "DeviceService")),
		devices:     make(map[string]*Device),
		bindings:    make(map[string][]*DeviceKeyBinding),
		userDevices: make(map[string][]string),
	}
}

// RegisterDevice registers a new device for a user
func (s *DeviceService) RegisterDevice(ctx context.Context, userID string, caps DeviceCapabilities) (*Device, error) {
	s.mu.Lock()
	defer s.mu.Unlock()

	now := time.Now().UnixMilli()
	deviceID := fmt.Sprintf("dev-%s-%d", userID, now)

	// Check device limit (max 5 devices per user)
	existingDevices := s.userDevices[userID]
	if len(existingDevices) >= 5 {
		return nil, fmt.Errorf("device limit reached (max 5 per user)")
	}

	device := &Device{
		DeviceID:     deviceID,
		UserID:       userID,
		Capabilities: caps,
		LastSeen:     now,
		RegisteredAt: now,
	}

	s.devices[deviceID] = device
	s.userDevices[userID] = append(s.userDevices[userID], deviceID)

	s.logger.Info("Device registered",
		zap.String("device_id", deviceID),
		zap.String("user_id", userID),
		zap.String("platform", caps.Platform),
		zap.String("model", caps.Model),
	)

	return device, nil
}

// GetDevice returns a device by ID
func (s *DeviceService) GetDevice(ctx context.Context, deviceID string) (*Device, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()

	device, ok := s.devices[deviceID]
	if !ok {
		return nil, fmt.Errorf("device not found: %s", deviceID)
	}
	return device, nil
}

// ListUserDevices lists all devices for a user
func (s *DeviceService) ListUserDevices(ctx context.Context, userID string) ([]*Device, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()

	deviceIDs := s.userDevices[userID]
	result := make([]*Device, 0, len(deviceIDs))
	for _, id := range deviceIDs {
		if d, ok := s.devices[id]; ok {
			result = append(result, d)
		}
	}
	return result, nil
}

// ProvisionKey provisions a key to a specific device
// Creates a key binding between the device and vehicle, returning existing if already bound
func (s *DeviceService) ProvisionKey(ctx context.Context, userID, deviceID, vehicleID string, protocols []string) (*DeviceKeyBinding, error) {
	s.mu.Lock()
	defer s.mu.Unlock()

	// 1. Validate device exists and belongs to user
	device, ok := s.devices[deviceID]
	if !ok {
		return nil, fmt.Errorf("device not found: %s", deviceID)
	}
	if device.UserID != userID {
		return nil, fmt.Errorf("device %s does not belong to user %s", deviceID, userID)
	}

	// 2. Check if device already has a key for this vehicle
	existingBindings := s.bindings[deviceID]
	for _, b := range existingBindings {
		if b.VehicleID == vehicleID && b.Status == "active" {
			s.logger.Info("Device already has active key for vehicle, returning existing",
				zap.String("device_id", deviceID),
				zap.String("vehicle_id", vehicleID),
				zap.String("key_id", b.KeyID),
			)
			return b, nil
		}
	}

	// 3. Create key binding
	now := time.Now().UnixMilli()
	platform := ""
	if device.Capabilities.Platform != "" {
		platform = device.Capabilities.Platform
	} else {
		platform = "generic"
	}
	keyID := fmt.Sprintf("key-%s-%s-%s-%d", userID, vehicleID, platform, now)

	// 4. Record binding
	binding := &DeviceKeyBinding{
		ID:        fmt.Sprintf("bind-%s-%s", keyID, device.DeviceID),
		KeyID:     keyID,
		DeviceID:  device.DeviceID,
		UserID:    userID,
		VehicleID: vehicleID,
		Status:    "active",
		BoundAt:   now,
	}

	s.bindings[device.DeviceID] = append(s.bindings[device.DeviceID], binding)
	device.LastSeen = now

	s.logger.Info("Key provisioned to device",
		zap.String("key_id", keyID),
		zap.String("device_id", device.DeviceID),
		zap.String("user_id", userID),
		zap.String("vehicle_id", vehicleID),
	)

	return binding, nil
}

// RevokeDeviceKeys revokes all keys for a device
func (s *DeviceService) RevokeDeviceKeys(ctx context.Context, userID, deviceID string) ([]*DeviceKeyBinding, error) {
	s.mu.Lock()
	defer s.mu.Unlock()

	device, ok := s.devices[deviceID]
	if !ok {
		return nil, fmt.Errorf("device not found: %s", deviceID)
	}
	if device.UserID != userID {
		return nil, fmt.Errorf("device %s does not belong to user %s", deviceID, userID)
	}

	bindings := s.bindings[deviceID]
	revoked := make([]*DeviceKeyBinding, 0)
	for _, b := range bindings {
		if b.Status == "active" {
			b.Status = "revoked"
			revoked = append(revoked, b)
		}
	}

	s.logger.Info("Device keys revoked",
		zap.String("device_id", deviceID),
		zap.String("user_id", userID),
		zap.Int("keys_revoked", len(revoked)),
	)

	return revoked, nil
}

// DeleteDevice removes a device and revokes all its keys
func (s *DeviceService) DeleteDevice(ctx context.Context, userID, deviceID string) error {
	s.mu.Lock()
	defer s.mu.Unlock()

	device, ok := s.devices[deviceID]
	if !ok {
		return fmt.Errorf("device not found: %s", deviceID)
	}
	if device.UserID != userID {
		return fmt.Errorf("device %s does not belong to user %s", deviceID, userID)
	}

	// Revoke all keys
	for _, b := range s.bindings[deviceID] {
		b.Status = "revoked"
	}

	// Remove from user device list
	devices := s.userDevices[userID]
	filtered := make([]string, 0, len(devices))
	for _, id := range devices {
		if id != deviceID {
			filtered = append(filtered, id)
		}
	}
	s.userDevices[userID] = filtered

	// Remove device record
	delete(s.devices, deviceID)

	s.logger.Info("Device deleted",
		zap.String("device_id", deviceID),
		zap.String("user_id", userID),
	)

	return nil
}
