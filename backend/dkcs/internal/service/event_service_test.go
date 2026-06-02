package service

import (
	"testing"
)

// ─────────────────────────────────────────────────────────────
// NewEventService Tests
// ─────────────────────────────────────────────────────────────

func TestNewEventService(t *testing.T) {
	svc := NewEventService(
		nil, // eventRepo — will be nil, only used in DB operations
		&mockLogger{},
		newMockTelemetry(),
	)
	if svc == nil {
		t.Fatal("NewEventService should return non-nil")
	}
	if svc.logger == nil {
		t.Error("logger should be set")
	}
	if svc.telemetry == nil {
		t.Error("telemetry should be set")
	}
}

// ─────────────────────────────────────────────────────────────
// convertDataToMap Tests
// ─────────────────────────────────────────────────────────────

func TestConvertDataToMap_WithData(t *testing.T) {
	data := map[string]interface{}{
		"command":    "unlock",
		"command_id": "cmd-001",
		"count":      3,
	}

	result := convertDataToMap(data)
	if result == nil {
		t.Fatal("result should not be nil")
	}
	if result["command"] != "unlock" {
		t.Errorf("command: want 'unlock', got '%s'", result["command"])
	}
	if result["command_id"] != "cmd-001" {
		t.Errorf("command_id: want 'cmd-001', got '%s'", result["command_id"])
	}
	if result["count"] != "3" {
		t.Errorf("count: want '3', got '%s'", result["count"])
	}
}

func TestConvertDataToMap_Nil(t *testing.T) {
	result := convertDataToMap(nil)
	if result != nil {
		t.Error("nil input should return nil result")
	}
}

func TestConvertDataToMap_WrongType(t *testing.T) {
	result := convertDataToMap("not a map")
	if result != nil {
		t.Error("non-map input should return nil")
	}
}

func TestConvertDataToMap_IntValue(t *testing.T) {
	data := map[string]interface{}{
		"number": 42,
	}
	result := convertDataToMap(data)
	if result["number"] != "42" {
		t.Errorf("number: want '42', got '%s'", result["number"])
	}
}

func TestConvertDataToMap_BoolValue(t *testing.T) {
	data := map[string]interface{}{
		"enabled": true,
	}
	result := convertDataToMap(data)
	if result["enabled"] != "true" {
		t.Errorf("enabled: want 'true', got '%s'", result["enabled"])
	}
}

// ─────────────────────────────────────────────────────────────
// convertStatsToProto Tests
// ─────────────────────────────────────────────────────────────

func TestConvertStatsToProto_WithData(t *testing.T) {
	stats := map[string]int64{
		"key_created":   5,
		"key_activated": 3,
		"key_revoked":   1,
	}

	result := convertStatsToProto(stats)
	if len(result) != 3 {
		t.Fatalf("want 3 items, got %d", len(result))
	}

	// Build map for easier assertion
	resultMap := make(map[string]int64)
	for _, item := range result {
		resultMap[item.Key] = item.Value
	}
	if resultMap["key_created"] != 5 {
		t.Errorf("key_created: want 5, got %d", resultMap["key_created"])
	}
	if resultMap["key_activated"] != 3 {
		t.Errorf("key_activated: want 3, got %d", resultMap["key_activated"])
	}
	if resultMap["key_revoked"] != 1 {
		t.Errorf("key_revoked: want 1, got %d", resultMap["key_revoked"])
	}
}

func TestConvertStatsToProto_Empty(t *testing.T) {
	stats := map[string]int64{}

	result := convertStatsToProto(stats)
	// Nil or empty both acceptable for empty input
	if result != nil && len(result) != 0 {
		t.Errorf("want 0 items, got %d", len(result))
	}
}


