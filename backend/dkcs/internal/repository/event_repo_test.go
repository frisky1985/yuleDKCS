package repository

import (
	"context"
	"sync"
	"testing"
	"time"

	"github.com/google/uuid"
)

// ---------------------------------------------------------------------------
// InMemoryEventStore — in-memory store implementing EventRepository contract
// ---------------------------------------------------------------------------

type InMemoryEventStore struct {
	mu     sync.RWMutex
	events []*Event // ordered by creation
}

func NewInMemoryEventStore() *InMemoryEventStore {
	return &InMemoryEventStore{events: make([]*Event, 0)}
}

func (s *InMemoryEventStore) Create(_ context.Context, event *Event) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	// Check for duplicate
	for _, e := range s.events {
		if e.ID == event.ID {
			return ErrEventConflict
		}
	}
	cp := *event
	s.events = append(s.events, &cp)
	return nil
}

func (s *InMemoryEventStore) GetByID(_ context.Context, id string) (*Event, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	for _, e := range s.events {
		if e.ID == id {
			cp := *e
			return &cp, nil
		}
	}
	return nil, ErrEventNotFound
}

func (s *InMemoryEventStore) ListByVehicle(_ context.Context, vehicleID string, limit, offset int) ([]*Event, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	var result []*Event
	for _, e := range s.events {
		if e.VehicleID == vehicleID {
			cp := *e
			result = append(result, &cp)
		}
	}
	return applyEventPagination(result, limit, offset), nil
}

func (s *InMemoryEventStore) ListByUser(_ context.Context, userID string, limit, offset int) ([]*Event, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	var result []*Event
	for _, e := range s.events {
		if e.UserID == userID {
			cp := *e
			result = append(result, &cp)
		}
	}
	return applyEventPagination(result, limit, offset), nil
}

func (s *InMemoryEventStore) ListByKey(_ context.Context, keyID string, limit, offset int) ([]*Event, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	var result []*Event
	for _, e := range s.events {
		if e.KeyID != nil && *e.KeyID == keyID {
			cp := *e
			result = append(result, &cp)
		}
	}
	return applyEventPagination(result, limit, offset), nil
}

func applyEventPagination(events []*Event, limit, offset int) []*Event {
	if offset >= len(events) {
		return []*Event{}
	}
	end := offset + limit
	if end > len(events) {
		end = len(events)
	}
	return events[offset:end]
}

var ErrEventConflict = &EventConflictError{Message: "event already exists"}

type EventConflictError struct{ Message string }

func (e *EventConflictError) Error() string { return e.Message }

// ---------------------------------------------------------------------------
// Helper: build a test event
// ---------------------------------------------------------------------------

func testEvent(overrides func(*Event)) *Event {
	now := time.Now().Truncate(time.Millisecond)
	e := &Event{
		ID:        uuid.New().String(),
		Type:      EventTypeKeyCreated,
		VehicleID: "vehicle-001",
		UserID:    "user-001",
		Data:      map[string]interface{}{"source": "test", "version": 1},
		CreatedAt: now,
	}
	if overrides != nil {
		overrides(e)
	}
	return e
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

func TestEventRepo_CreateAndGet(t *testing.T) {
	store := NewInMemoryEventStore()
	ctx := context.Background()

	e := testEvent(nil)

	if err := store.Create(ctx, e); err != nil {
		t.Fatalf("Create failed: %v", err)
	}

	got, err := store.GetByID(ctx, e.ID)
	if err != nil {
		t.Fatalf("GetByID failed: %v", err)
	}

	if got.ID != e.ID {
		t.Errorf("ID: want %q, got %q", e.ID, got.ID)
	}
	if got.Type != e.Type {
		t.Errorf("Type: want %q, got %q", e.Type, got.Type)
	}
	if got.VehicleID != e.VehicleID {
		t.Errorf("VehicleID: want %q, got %q", e.VehicleID, got.VehicleID)
	}
	if got.UserID != e.UserID {
		t.Errorf("UserID: want %q, got %q", e.UserID, got.UserID)
	}
	if got.Data["source"] != "test" {
		t.Errorf("Data.source: want 'test', got %v", got.Data["source"])
	}
	if !got.CreatedAt.Equal(e.CreatedAt) {
		t.Errorf("CreatedAt not preserved")
	}
}

func TestEventRepo_CreateDuplicate(t *testing.T) {
	store := NewInMemoryEventStore()
	ctx := context.Background()

	e := testEvent(nil)
	if err := store.Create(ctx, e); err != nil {
		t.Fatalf("first Create failed: %v", err)
	}

	err := store.Create(ctx, e)
	if err == nil {
		t.Fatal("expected error for duplicate event, got nil")
	}
	if _, ok := err.(*EventConflictError); !ok {
		t.Errorf("expected EventConflictError, got %T: %v", err, err)
	}
}

func TestEventRepo_GetNotFound(t *testing.T) {
	store := NewInMemoryEventStore()
	ctx := context.Background()

	_, err := store.GetByID(ctx, "nonexistent-id")
	if err == nil {
		t.Fatal("expected ErrEventNotFound, got nil")
	}
	if err != ErrEventNotFound {
		t.Errorf("expected ErrEventNotFound, got %v", err)
	}
}

func TestEventRepo_ListByVehicle(t *testing.T) {
	store := NewInMemoryEventStore()
	ctx := context.Background()

	e1 := testEvent(func(e *Event) { e.ID = "e1"; e.VehicleID = "v1"; e.Type = EventTypeKeyCreated })
	e2 := testEvent(func(e *Event) { e.ID = "e2"; e.VehicleID = "v2"; e.Type = EventTypeKeyShared })
	e3 := testEvent(func(e *Event) { e.ID = "e3"; e.VehicleID = "v1"; e.Type = EventTypeKeyRevoked })
	e4 := testEvent(func(e *Event) { e.ID = "e4"; e.VehicleID = "v1"; e.Type = EventTypeCommandSent })

	for _, event := range []*Event{e1, e2, e3, e4} {
		if err := store.Create(ctx, event); err != nil {
			t.Fatalf("Create failed: %v", err)
		}
	}

	// v1 should have 3 events
	result, err := store.ListByVehicle(ctx, "v1", 10, 0)
	if err != nil {
		t.Fatalf("ListByVehicle failed: %v", err)
	}
	if len(result) != 3 {
		t.Errorf("want 3 events for v1, got %d", len(result))
	}

	// v2 should have 1 event
	result, err = store.ListByVehicle(ctx, "v2", 10, 0)
	if err != nil {
		t.Fatalf("ListByVehicle failed: %v", err)
	}
	if len(result) != 1 {
		t.Errorf("want 1 event for v2, got %d", len(result))
	}

	// nonexistent vehicle
	result, err = store.ListByVehicle(ctx, "nonexistent", 10, 0)
	if err != nil {
		t.Fatalf("ListByVehicle failed: %v", err)
	}
	if len(result) != 0 {
		t.Errorf("want 0 events, got %d", len(result))
	}
}

func TestEventRepo_ListByUser(t *testing.T) {
	store := NewInMemoryEventStore()
	ctx := context.Background()

	e1 := testEvent(func(e *Event) { e.ID = "e1"; e.UserID = "u1" })
	e2 := testEvent(func(e *Event) { e.ID = "e2"; e.UserID = "u2" })
	e3 := testEvent(func(e *Event) { e.ID = "e3"; e.UserID = "u1" })

	for _, event := range []*Event{e1, e2, e3} {
		if err := store.Create(ctx, event); err != nil {
			t.Fatalf("Create failed: %v", err)
		}
	}

	// u1 should have 2 events
	result, err := store.ListByUser(ctx, "u1", 10, 0)
	if err != nil {
		t.Fatalf("ListByUser failed: %v", err)
	}
	if len(result) != 2 {
		t.Errorf("want 2 events for u1, got %d", len(result))
	}
}

func TestEventRepo_ListByKey(t *testing.T) {
	store := NewInMemoryEventStore()
	ctx := context.Background()

	keyID := "key-001"
	e1 := testEvent(func(e *Event) { e.ID = "e1"; e.KeyID = &keyID })
	otherKeyID := "key-002"
	e2 := testEvent(func(e *Event) { e.ID = "e2"; e.KeyID = &otherKeyID })
	e3 := testEvent(func(e *Event) { e.ID = "e3"; e.KeyID = &keyID })
	e4 := testEvent(func(e *Event) { e.ID = "e4" }) // no key ID

	for _, event := range []*Event{e1, e2, e3, e4} {
		if err := store.Create(ctx, event); err != nil {
			t.Fatalf("Create failed: %v", err)
		}
	}

	result, err := store.ListByKey(ctx, keyID, 10, 0)
	if err != nil {
		t.Fatalf("ListByKey failed: %v", err)
	}
	if len(result) != 2 {
		t.Errorf("want 2 events for key-001, got %d", len(result))
	}

	// event with no key_id should not appear
	for _, event := range result {
		if event.ID == "e4" {
			t.Errorf("event with nil key_id should not appear in ListByKey results")
		}
	}
}

func TestEventRepo_Pagination(t *testing.T) {
	store := NewInMemoryEventStore()
	ctx := context.Background()

	for i := 0; i < 10; i++ {
		e := testEvent(func(e *Event) {
			e.ID = uuid.New().String()
			e.VehicleID = "v1"
		})
		if err := store.Create(ctx, e); err != nil {
			t.Fatalf("Create failed: %v", err)
		}
	}

	page1, err := store.ListByVehicle(ctx, "v1", 3, 0)
	if err != nil {
		t.Fatalf("ListByVehicle page1 failed: %v", err)
	}
	if len(page1) != 3 {
		t.Errorf("page1: want 3, got %d", len(page1))
	}

	page2, err := store.ListByVehicle(ctx, "v1", 3, 3)
	if err != nil {
		t.Fatalf("ListByVehicle page2 failed: %v", err)
	}
	if len(page2) != 3 {
		t.Errorf("page2: want 3, got %d", len(page2))
	}

	// Verify no overlap
	page1IDs := make(map[string]bool)
	for _, e := range page1 {
		page1IDs[e.ID] = true
	}
	for _, e := range page2 {
		if page1IDs[e.ID] {
			t.Errorf("event %s appears on both pages", e.ID)
		}
	}

	// Page beyond data
	page4, err := store.ListByVehicle(ctx, "v1", 3, 10)
	if err != nil {
		t.Fatalf("ListByVehicle (beyond) failed: %v", err)
	}
	if len(page4) != 0 {
		t.Errorf("beyond data: want 0, got %d", len(page4))
	}
}

func TestEventRepo_EventTypes(t *testing.T) {
	eventTypes := []string{
		EventTypeKeyCreated,
		EventTypeKeyActivated,
		EventTypeKeyRevoked,
		EventTypeKeyShared,
		EventTypeKeyExpired,
		EventTypeCommandSent,
		EventTypeCommandReceived,
		EventTypeCommandFailed,
		EventTypeVehicleOnline,
		EventTypeVehicleOffline,
		EventTypeVehicleLocation,
	}

	store := NewInMemoryEventStore()
	ctx := context.Background()

	for i, et := range eventTypes {
		e := testEvent(func(e *Event) {
			e.ID = uuid.New().String()
			e.Type = et
		})
		if err := store.Create(ctx, e); err != nil {
			t.Fatalf("Create event type %s failed: %v", et, err)
		}

		got, err := store.GetByID(ctx, e.ID)
		if err != nil {
			t.Fatalf("GetByID for event type %s failed: %v", et, err)
		}
		if got.Type != et {
			t.Errorf("event %d: want type %q, got %q", i, et, got.Type)
		}
	}
}
