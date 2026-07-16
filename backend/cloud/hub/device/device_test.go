// Package device — unit tests for Device Registry & Store
package device

import (
	"context"
	"sync"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// ── InMemoryRegistry ─────────────────────────────────────────────────────

type InMemoryRegistry struct {
	mu      sync.RWMutex
	devices map[string]*Device
	bindings map[string]*DeviceBinding
}

func NewInMemoryRegistry() *InMemoryRegistry {
	return &InMemoryRegistry{
		devices:  make(map[string]*Device),
		bindings: make(map[string]*DeviceBinding),
	}
}

func (r *InMemoryRegistry) Register(_ context.Context, device *Device) error {
	r.mu.Lock()
	defer r.mu.Unlock()
	r.devices[device.DeviceID] = device
	return nil
}

func (r *InMemoryRegistry) Unregister(_ context.Context, deviceID string) error {
	r.mu.Lock()
	defer r.mu.Unlock()
	delete(r.devices, deviceID)
	return nil
}

func (r *InMemoryRegistry) Get(_ context.Context, deviceID string) (*Device, error) {
	r.mu.RLock()
	defer r.mu.RUnlock()
	d, ok := r.devices[deviceID]
	if !ok {
		return nil, nil
	}
	return d, nil
}

func (r *InMemoryRegistry) ListByUser(_ context.Context, userID string) ([]*Device, error) {
	r.mu.RLock()
	defer r.mu.RUnlock()
	var result []*Device
	for _, d := range r.devices {
		if d.UserID == userID {
			result = append(result, d)
		}
	}
	return result, nil
}

func (r *InMemoryRegistry) ListByCapability(_ context.Context, caps []DeviceCapability) ([]*Device, error) {
	r.mu.RLock()
	defer r.mu.RUnlock()
	var result []*Device
	for _, d := range r.devices {
		if d.Capabilities == nil {
			continue
		}
		match := true
		for _, cap := range caps {
			switch cap {
			case CapBLE:
				if !d.Capabilities.BLE {
					match = false
				}
			case CapUWB:
				if !d.Capabilities.UWB {
					match = false
				}
			case CapNFC:
				if !d.Capabilities.NFC {
					match = false
				}
			case CapSE:
				if !d.Capabilities.SE {
					match = false
				}
			}
		}
		if match {
			result = append(result, d)
		}
	}
	return result, nil
}

func (r *InMemoryRegistry) UpdateStatus(_ context.Context, deviceID string, status DeviceStatus) error {
	r.mu.Lock()
	defer r.mu.Unlock()
	d, ok := r.devices[deviceID]
	if !ok {
		return nil
	}
	d.Status = status
	return nil
}

func (r *InMemoryRegistry) UpdateLastSeen(_ context.Context, deviceID string) error {
	r.mu.Lock()
	defer r.mu.Unlock()
	d, ok := r.devices[deviceID]
	if !ok {
		return nil
	}
	d.LastSeen = time.Now()
	return nil
}

func (r *InMemoryRegistry) CountByUser(_ context.Context, userID string) (int, error) {
	r.mu.RLock()
	defer r.mu.RUnlock()
	var count int
	for _, d := range r.devices {
		if d.UserID == userID {
			count++
		}
	}
	return count, nil
}

// ── InMemoryStore ────────────────────────────────────────────────────────

type InMemoryStore struct {
	mu       sync.RWMutex
	devices  map[string]*Device
	bindings map[string]*DeviceBinding
}

func NewInMemoryStore() *InMemoryStore {
	return &InMemoryStore{
		devices:  make(map[string]*Device),
		bindings: make(map[string]*DeviceBinding),
	}
}

func (s *InMemoryStore) SaveDevice(_ context.Context, device *Device) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.devices[device.DeviceID] = device
	return nil
}

func (s *InMemoryStore) GetDevice(_ context.Context, deviceID string) (*Device, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	d, ok := s.devices[deviceID]
	if !ok {
		return nil, nil
	}
	return d, nil
}

func (s *InMemoryStore) DeleteDevice(_ context.Context, deviceID string) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	delete(s.devices, deviceID)
	return nil
}

func (s *InMemoryStore) ListDevicesByUser(_ context.Context, userID string) ([]*Device, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	var result []*Device
	for _, d := range s.devices {
		if d.UserID == userID {
			result = append(result, d)
		}
	}
	return result, nil
}

func (s *InMemoryStore) SaveBinding(_ context.Context, binding *DeviceBinding) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.bindings[binding.BindingID] = binding
	return nil
}

func (s *InMemoryStore) GetBinding(_ context.Context, bindingID string) (*DeviceBinding, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	b, ok := s.bindings[bindingID]
	if !ok {
		return nil, nil
	}
	return b, nil
}

func (s *InMemoryStore) ListBindingsByDevice(_ context.Context, deviceID string) ([]*DeviceBinding, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	var result []*DeviceBinding
	for _, b := range s.bindings {
		if b.DeviceID == deviceID {
			result = append(result, b)
		}
	}
	return result, nil
}

func (s *InMemoryStore) ListBindingsByVehicle(_ context.Context, vehicleID string) ([]*DeviceBinding, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	var result []*DeviceBinding
	for _, b := range s.bindings {
		if b.VehicleID == vehicleID {
			result = append(result, b)
		}
	}
	return result, nil
}

func (s *InMemoryStore) DeleteBinding(_ context.Context, bindingID string) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	delete(s.bindings, bindingID)
	return nil
}

// ── Tests ─────────────────────────────────────────────────────────────────

func TestInMemoryRegistry_RegisterAndGet(t *testing.T) {
	ctx := context.Background()
	r := NewInMemoryRegistry()

	dev := &Device{
		DeviceID: "dev-001",
		UserID:   "user-001",
		Vendor:   "Apple",
		Platform: "iOS",
		Model:    "iPhone 15 Pro",
		Capabilities: &CapabilitySet{
			BLE: true, UWB: true, NFC: true, SE: true,
		},
	}

	err := r.Register(ctx, dev)
	require.NoError(t, err)

	retrieved, err := r.Get(ctx, "dev-001")
	require.NoError(t, err)
	assert.Equal(t, "dev-001", retrieved.DeviceID)
	assert.Equal(t, "Apple", retrieved.Vendor)
	assert.True(t, retrieved.Capabilities.BLE)
}

func TestInMemoryRegistry_Unregister(t *testing.T) {
	ctx := context.Background()
	r := NewInMemoryRegistry()
	_ = r.Register(ctx, &Device{DeviceID: "dev-001", UserID: "user-001"})

	err := r.Unregister(ctx, "dev-001")
	assert.NoError(t, err)

	retrieved, _ := r.Get(ctx, "dev-001")
	assert.Nil(t, retrieved)
}

func TestInMemoryRegistry_ListByUser(t *testing.T) {
	ctx := context.Background()
	r := NewInMemoryRegistry()
	_ = r.Register(ctx, &Device{DeviceID: "d1", UserID: "u1"})
	_ = r.Register(ctx, &Device{DeviceID: "d2", UserID: "u1"})
	_ = r.Register(ctx, &Device{DeviceID: "d3", UserID: "u2"})

	devices, err := r.ListByUser(ctx, "u1")
	assert.NoError(t, err)
	assert.Len(t, devices, 2)
}

func TestInMemoryRegistry_ListByCapability(t *testing.T) {
	ctx := context.Background()
	r := NewInMemoryRegistry()
	_ = r.Register(ctx, &Device{DeviceID: "d1", Capabilities: &CapabilitySet{BLE: true, UWB: true}})
	_ = r.Register(ctx, &Device{DeviceID: "d2", Capabilities: &CapabilitySet{BLE: true}})
	_ = r.Register(ctx, &Device{DeviceID: "d3", Capabilities: nil})

	t.Run("find BLE devices", func(t *testing.T) {
		devices, err := r.ListByCapability(ctx, []DeviceCapability{CapBLE})
		assert.NoError(t, err)
		assert.Len(t, devices, 2)
	})

	t.Run("find BLE+UWB devices", func(t *testing.T) {
		devices, err := r.ListByCapability(ctx, []DeviceCapability{CapBLE, CapUWB})
		assert.NoError(t, err)
		assert.Len(t, devices, 1)
	})
}

func TestInMemoryRegistry_UpdateStatus(t *testing.T) {
	ctx := context.Background()
	r := NewInMemoryRegistry()
	_ = r.Register(ctx, &Device{DeviceID: "d1", Status: DeviceStatusActive})

	err := r.UpdateStatus(ctx, "d1", DeviceStatusSuspended)
	assert.NoError(t, err)

	d, _ := r.Get(ctx, "d1")
	assert.Equal(t, DeviceStatusSuspended, d.Status)
}

func TestInMemoryRegistry_CountByUser(t *testing.T) {
	ctx := context.Background()
	r := NewInMemoryRegistry()
	_ = r.Register(ctx, &Device{DeviceID: "d1", UserID: "u1"})
	_ = r.Register(ctx, &Device{DeviceID: "d2", UserID: "u1"})

	count, err := r.CountByUser(ctx, "u1")
	assert.NoError(t, err)
	assert.Equal(t, 2, count)
}

func TestInMemoryStore_CRUD(t *testing.T) {
	ctx := context.Background()
	s := NewInMemoryStore()

	dev := &Device{DeviceID: "d1", UserID: "u1", Vendor: "Xiaomi"}

	t.Run("save and get", func(t *testing.T) {
		err := s.SaveDevice(ctx, dev)
		assert.NoError(t, err)

		d, err := s.GetDevice(ctx, "d1")
		assert.NoError(t, err)
		assert.Equal(t, "Xiaomi", d.Vendor)
	})

	t.Run("get non-existent returns nil", func(t *testing.T) {
		d, err := s.GetDevice(ctx, "no-such-device")
		assert.NoError(t, err)
		assert.Nil(t, d)
	})

	t.Run("delete", func(t *testing.T) {
		err := s.DeleteDevice(ctx, "d1")
		assert.NoError(t, err)

		d, _ := s.GetDevice(ctx, "d1")
		assert.Nil(t, d)
	})

	t.Run("list by user", func(t *testing.T) {
		_ = s.SaveDevice(ctx, &Device{DeviceID: "d2", UserID: "u1"})
		_ = s.SaveDevice(ctx, &Device{DeviceID: "d3", UserID: "u2"})

		devices, err := s.ListDevicesByUser(ctx, "u1")
		assert.NoError(t, err)
		assert.Len(t, devices, 1)
	})
}

func TestInMemoryStore_Bindings(t *testing.T) {
	ctx := context.Background()
	s := NewInMemoryStore()

	binding := &DeviceBinding{
		BindingID: "b1",
		DeviceID:  "d1",
		VehicleID: "v1",
		UserID:    "u1",
		Protocol:  "BLE",
		Status:    "active",
	}

	t.Run("save binding", func(t *testing.T) {
		err := s.SaveBinding(ctx, binding)
		assert.NoError(t, err)
	})

	t.Run("get binding", func(t *testing.T) {
		b, err := s.GetBinding(ctx, "b1")
		assert.NoError(t, err)
		require.NotNil(t, b)
		assert.Equal(t, "d1", b.DeviceID)
	})

	t.Run("list by device", func(t *testing.T) {
		bindings, err := s.ListBindingsByDevice(ctx, "d1")
		assert.NoError(t, err)
		assert.Len(t, bindings, 1)
	})

	t.Run("list by vehicle", func(t *testing.T) {
		bindings, err := s.ListBindingsByVehicle(ctx, "v1")
		assert.NoError(t, err)
		assert.Len(t, bindings, 1)
	})

	t.Run("delete binding", func(t *testing.T) {
		err := s.DeleteBinding(ctx, "b1")
		assert.NoError(t, err)

		b, _ := s.GetBinding(ctx, "b1")
		assert.Nil(t, b)
	})
}

func TestDeviceCapabilityConstants(t *testing.T) {
	assert.Equal(t, DeviceCapability("BLE"), CapBLE)
	assert.Equal(t, DeviceCapability("UWB"), CapUWB)
	assert.Equal(t, DeviceCapability("NFC"), CapNFC)
	assert.Equal(t, DeviceCapability("SE"), CapSE)
	assert.Equal(t, DeviceCapability("FiRa"), CapFiRa)
}

func TestDeviceStatusConstants(t *testing.T) {
	assert.Equal(t, DeviceStatus("active"), DeviceStatusActive)
	assert.Equal(t, DeviceStatus("inactive"), DeviceStatusInactive)
	assert.Equal(t, DeviceStatus("suspended"), DeviceStatusSuspended)
	assert.Equal(t, DeviceStatus("revoked"), DeviceStatusRevoked)
	assert.Equal(t, DeviceStatus("disabled"), DeviceStatusDisabled)
}
