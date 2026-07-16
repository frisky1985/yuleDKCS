package repository

import (
	"context"
	"sync"
	"testing"
	"time"

	"github.com/google/uuid"
)

// ---------------------------------------------------------------------------
// InMemoryVehicleStore — in-memory store implementing VehicleRepository contract
// ---------------------------------------------------------------------------

type InMemoryVehicleStore struct {
	mu       sync.RWMutex
	vehicles map[string]*Vehicle
}

func NewInMemoryVehicleStore() *InMemoryVehicleStore {
	return &InMemoryVehicleStore{vehicles: make(map[string]*Vehicle)}
}

func (s *InMemoryVehicleStore) Create(_ context.Context, v *Vehicle) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	if _, exists := s.vehicles[v.ID]; exists {
		return ErrVehicleConflict
	}
	cp := *v
	s.vehicles[v.ID] = &cp
	return nil
}

func (s *InMemoryVehicleStore) GetByID(_ context.Context, id string) (*Vehicle, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	v, ok := s.vehicles[id]
	if !ok {
		return nil, ErrVehicleNotFound
	}
	cp := *v
	return &cp, nil
}

func (s *InMemoryVehicleStore) UpdateStatus(_ context.Context, id string, isOnline bool) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	v, ok := s.vehicles[id]
	if !ok {
		return ErrVehicleNotFound
	}
	v.IsOnline = isOnline
	now := time.Now()
	if isOnline {
		v.LastOnline = &now
	}
	v.UpdatedAt = now
	return nil
}

func (s *InMemoryVehicleStore) UpdateLocation(_ context.Context, id string, lat, lng float64) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	v, ok := s.vehicles[id]
	if !ok {
		return ErrVehicleNotFound
	}
	v.Latitude = lat
	v.Longitude = lng
	v.UpdatedAt = time.Now()
	return nil
}

func (s *InMemoryVehicleStore) UpdateTelemetry(_ context.Context, id string, batteryLevel, odometer int) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	v, ok := s.vehicles[id]
	if !ok {
		return ErrVehicleNotFound
	}
	v.BatteryLevel = batteryLevel
	v.Odometer = odometer
	v.UpdatedAt = time.Now()
	return nil
}

func (s *InMemoryVehicleStore) ListByOwner(_ context.Context, ownerID string) ([]*Vehicle, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	var result []*Vehicle
	for _, v := range s.vehicles {
		if v.OwnerID == ownerID {
			cp := *v
			result = append(result, &cp)
		}
	}
	if result == nil {
		return []*Vehicle{}, nil
	}
	return result, nil
}

var ErrVehicleConflict = &VehicleConflictError{Message: "vehicle already exists"}

type VehicleConflictError struct{ Message string }

func (e *VehicleConflictError) Error() string { return e.Message }

// ---------------------------------------------------------------------------
// Helper: build a test vehicle with sensible defaults
// ---------------------------------------------------------------------------

func testVehicle(overrides func(*Vehicle)) *Vehicle {
	now := time.Now().Truncate(time.Millisecond)
	v := &Vehicle{
		ID:       uuid.New().String(),
		OwnerID:  "owner-001",
		VIN:      "LSVAU2A31M1234567",
		Brand:    "TestBrand",
		Model:    "TestModel",
		Year:     2026,
		Color:    "White",
		Protocol: "CCC",
		Features: []string{"ble", "nfc", "uwb"},
		IsOnline: false,
		CreatedAt: now,
		UpdatedAt: now,
	}
	if overrides != nil {
		overrides(v)
	}
	return v
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

func TestVehicleRepo_CreateAndGet(t *testing.T) {
	store := NewInMemoryVehicleStore()
	ctx := context.Background()

	v := testVehicle(nil)

	if err := store.Create(ctx, v); err != nil {
		t.Fatalf("Create failed: %v", err)
	}

	got, err := store.GetByID(ctx, v.ID)
	if err != nil {
		t.Fatalf("GetByID failed: %v", err)
	}

	if got.ID != v.ID {
		t.Errorf("ID: want %q, got %q", v.ID, got.ID)
	}
	if got.VIN != v.VIN {
		t.Errorf("VIN: want %q, got %q", v.VIN, got.VIN)
	}
	if got.Brand != v.Brand {
		t.Errorf("Brand: want %q, got %q", v.Brand, got.Brand)
	}
	if got.Model != v.Model {
		t.Errorf("Model: want %q, got %q", v.Model, got.Model)
	}
	if got.OwnerID != v.OwnerID {
		t.Errorf("OwnerID: want %q, got %q", v.OwnerID, got.OwnerID)
	}
	if got.Protocol != v.Protocol {
		t.Errorf("Protocol: want %q, got %q", v.Protocol, got.Protocol)
	}
	if got.Year != v.Year {
		t.Errorf("Year: want %d, got %d", v.Year, got.Year)
	}
	if len(got.Features) != 3 || got.Features[0] != "ble" {
		t.Errorf("Features not preserved: %v", got.Features)
	}
}

func TestVehicleRepo_CreateDuplicate(t *testing.T) {
	store := NewInMemoryVehicleStore()
	ctx := context.Background()

	v := testVehicle(nil)
	if err := store.Create(ctx, v); err != nil {
		t.Fatalf("first Create failed: %v", err)
	}

	err := store.Create(ctx, v)
	if err == nil {
		t.Fatal("expected error for duplicate vehicle, got nil")
	}
	if _, ok := err.(*VehicleConflictError); !ok {
		t.Errorf("expected VehicleConflictError, got %T: %v", err, err)
	}
}

func TestVehicleRepo_GetNotFound(t *testing.T) {
	store := NewInMemoryVehicleStore()
	ctx := context.Background()

	_, err := store.GetByID(ctx, "nonexistent-id")
	if err == nil {
		t.Fatal("expected ErrVehicleNotFound, got nil")
	}
	if err != ErrVehicleNotFound {
		t.Errorf("expected ErrVehicleNotFound, got %v", err)
	}
}

func TestVehicleRepo_UpdateStatus(t *testing.T) {
	store := NewInMemoryVehicleStore()
	ctx := context.Background()

	v := testVehicle(nil)
	if err := store.Create(ctx, v); err != nil {
		t.Fatalf("Create failed: %v", err)
	}

	if err := store.UpdateStatus(ctx, v.ID, true); err != nil {
		t.Fatalf("UpdateStatus failed: %v", err)
	}

	got, err := store.GetByID(ctx, v.ID)
	if err != nil {
		t.Fatalf("GetByID after update failed: %v", err)
	}
	if !got.IsOnline {
		t.Errorf("IsOnline: want true, got false")
	}
	if got.LastOnline == nil {
		t.Errorf("LastOnline should not be nil after going online")
	}
}

func TestVehicleRepo_UpdateStatusNotFound(t *testing.T) {
	store := NewInMemoryVehicleStore()
	ctx := context.Background()

	err := store.UpdateStatus(ctx, "nonexistent", true)
	if err == nil {
		t.Fatal("expected error for updating nonexistent vehicle")
	}
	if err != ErrVehicleNotFound {
		t.Errorf("expected ErrVehicleNotFound, got %v", err)
	}
}

func TestVehicleRepo_UpdateLocation(t *testing.T) {
	store := NewInMemoryVehicleStore()
	ctx := context.Background()

	v := testVehicle(nil)
	if err := store.Create(ctx, v); err != nil {
		t.Fatalf("Create failed: %v", err)
	}

	lat, lng := 39.9042, 116.4074
	if err := store.UpdateLocation(ctx, v.ID, lat, lng); err != nil {
		t.Fatalf("UpdateLocation failed: %v", err)
	}

	got, err := store.GetByID(ctx, v.ID)
	if err != nil {
		t.Fatalf("GetByID after location update failed: %v", err)
	}
	if got.Latitude != lat {
		t.Errorf("Latitude: want %f, got %f", lat, got.Latitude)
	}
	if got.Longitude != lng {
		t.Errorf("Longitude: want %f, got %f", lng, got.Longitude)
	}
}

func TestVehicleRepo_UpdateTelemetry(t *testing.T) {
	store := NewInMemoryVehicleStore()
	ctx := context.Background()

	v := testVehicle(nil)
	if err := store.Create(ctx, v); err != nil {
		t.Fatalf("Create failed: %v", err)
	}

	if err := store.UpdateTelemetry(ctx, v.ID, 85, 15000); err != nil {
		t.Fatalf("UpdateTelemetry failed: %v", err)
	}

	got, err := store.GetByID(ctx, v.ID)
	if err != nil {
		t.Fatalf("GetByID after telemetry update failed: %v", err)
	}
	if got.BatteryLevel != 85 {
		t.Errorf("BatteryLevel: want 85, got %d", got.BatteryLevel)
	}
	if got.Odometer != 15000 {
		t.Errorf("Odometer: want 15000, got %d", got.Odometer)
	}
}

func TestVehicleRepo_ListByOwner(t *testing.T) {
	store := NewInMemoryVehicleStore()
	ctx := context.Background()

	v1 := testVehicle(func(v *Vehicle) { v.ID = "v1"; v.OwnerID = "owner-001"; v.VIN = "VIN001" })
	v2 := testVehicle(func(v *Vehicle) { v.ID = "v2"; v.OwnerID = "owner-002"; v.VIN = "VIN002" })
	v3 := testVehicle(func(v *Vehicle) { v.ID = "v3"; v.OwnerID = "owner-001"; v.VIN = "VIN003" })

	for _, v := range []*Vehicle{v1, v2, v3} {
		if err := store.Create(ctx, v); err != nil {
			t.Fatalf("Create %s failed: %v", v.ID, err)
		}
	}

	// owner-001 should have 2 vehicles
	result, err := store.ListByOwner(ctx, "owner-001")
	if err != nil {
		t.Fatalf("ListByOwner failed: %v", err)
	}
	if len(result) != 2 {
		t.Errorf("want 2 vehicles for owner-001, got %d", len(result))
	}

	// owner-002 should have 1 vehicle
	result, err = store.ListByOwner(ctx, "owner-002")
	if err != nil {
		t.Fatalf("ListByOwner failed: %v", err)
	}
	if len(result) != 1 {
		t.Errorf("want 1 vehicle for owner-002, got %d", len(result))
	}

	// nonexistent owner should return empty, not nil
	result, err = store.ListByOwner(ctx, "unknown-owner")
	if err != nil {
		t.Fatalf("ListByOwner failed: %v", err)
	}
	if result == nil {
		t.Error("expected empty slice, got nil")
	}
	if len(result) != 0 {
		t.Errorf("want 0 vehicles, got %d", len(result))
	}
}

func TestVehicleRepo_FullLifecycle(t *testing.T) {
	store := NewInMemoryVehicleStore()
	ctx := context.Background()

	v := testVehicle(nil)

	// Create
	if err := store.Create(ctx, v); err != nil {
		t.Fatalf("Create failed: %v", err)
	}

	// Update status to online
	if err := store.UpdateStatus(ctx, v.ID, true); err != nil {
		t.Fatalf("UpdateStatus failed: %v", err)
	}

	// Update location
	if err := store.UpdateLocation(ctx, v.ID, 31.2304, 121.4737); err != nil {
		t.Fatalf("UpdateLocation failed: %v", err)
	}

	// Update telemetry
	if err := store.UpdateTelemetry(ctx, v.ID, 90, 5000); err != nil {
		t.Fatalf("UpdateTelemetry failed: %v", err)
	}

	// Final verification
	got, err := store.GetByID(ctx, v.ID)
	if err != nil {
		t.Fatalf("Final GetByID failed: %v", err)
	}
	if !got.IsOnline {
		t.Errorf("vehicle should be online")
	}
	if got.Latitude != 31.2304 || got.Longitude != 121.4737 {
		t.Errorf("location not preserved: (%f, %f)", got.Latitude, got.Longitude)
	}
	if got.BatteryLevel != 90 || got.Odometer != 5000 {
		t.Errorf("telemetry not preserved: battery=%d, odometer=%d", got.BatteryLevel, got.Odometer)
	}
}
