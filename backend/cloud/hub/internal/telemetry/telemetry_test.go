package telemetry

import (
	"sync"
	"testing"
	"time"
)

func TestEventTypeConstants(t *testing.T) {
	tests := []struct {
		event EventType
		name  string
	}{
		{EventAppLaunch, "app_launch"},
		{EventAppBackground, "app_background"},
		{EventSdkInit, "sdk_init"},
		{EventSdkError, "sdk_error"},
		{EventUserLogin, "user_login"},
		{EventUserLogout, "user_logout"},
		{EventDeviceBindStart, "device_bind_start"},
		{EventDeviceBindSuccess, "device_bind_success"},
		{EventDeviceBindFailed, "device_bind_failed"},
		{EventDeviceUnbind, "device_unbind"},
		{EventKeyCreate, "key_create"},
		{EventKeyDelete, "key_delete"},
		{EventKeyRefresh, "key_refresh"},
		{EventKeyUse, "key_use"},
		{EventKeyExpired, "key_expired"},
		{EventVehicleUnlock, "vehicle_unlock"},
		{EventVehicleLock, "vehicle_lock"},
		{EventVehicleStart, "vehicle_start"},
		{EventVehicleStop, "vehicle_stop"},
		{EventShareCreate, "share_create"},
		{EventShareAccept, "share_accept"},
		{EventShareRevoke, "share_revoke"},
		{EventBleScan, "ble_scan"},
		{EventBleConnect, "ble_connect"},
		{EventBleDisconnect, "ble_disconnect"},
		{EventNfcTap, "nfc_tap"},
		{EventUwbRanging, "uwb_ranging"},
		{EventChannelSwitch, "channel_switch"},
		{EventProtocolMsg, "protocol_msg"},
		{EventSecurityAlert, "security_alert"},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if string(tt.event) != tt.name {
				t.Errorf("EventType = %q, want %q", string(tt.event), tt.name)
			}
		})
	}
}

func TestSourceConstants(t *testing.T) {
	tests := []struct {
		source Source
		name   string
	}{
		{SourceSDK, "SDK"},
		{SourceTCU, "TCU"},
		{SourceCloud, "CLOUD"},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if string(tt.source) != tt.name {
				t.Errorf("Source = %q, want %q", string(tt.source), tt.name)
			}
		})
	}
}

func TestNewTelemetry(t *testing.T) {
	t.Run("default config", func(t *testing.T) {
		tel := NewTelemetry(TelemetryConfig{
			Source:    SourceCloud,
			SessionID: "sess-001",
		})
		if tel == nil {
			t.Fatal("NewTelemetry() returned nil")
		}
		if tel.source != SourceCloud {
			t.Errorf("source = %v, want CLOUD", tel.source)
		}
		if tel.sessionID != "sess-001" {
			t.Errorf("sessionID = %q", tel.sessionID)
		}
	})

	t.Run("with custom batch size", func(t *testing.T) {
		tel := NewTelemetry(TelemetryConfig{
			Source:     SourceSDK,
			BatchSize:  50,
			SessionID:  "sess-002",
			FlushInterval: time.Second,
		})
		if tel == nil {
			t.Fatal("NewTelemetry() returned nil")
		}
		if tel.source != SourceSDK {
			t.Errorf("source = %v", tel.source)
		}
	})

	t.Run("TCU source", func(t *testing.T) {
		tel := NewTelemetry(TelemetryConfig{
			Source: SourceTCU,
		})
		if tel.source != SourceTCU {
			t.Errorf("source = %v", tel.source)
		}
	})
}

func TestTrack(t *testing.T) {
	tel := NewTelemetry(TelemetryConfig{
		Source:    SourceCloud,
		SessionID: "sess-track",
	})

	t.Run("basic track", func(t *testing.T) {
		tel.Track(EventKeyCreate, map[string]interface{}{
			"key_id": "key-001",
		})

		tel.mu.Lock()
		if len(tel.events) != 1 {
			t.Errorf("events count = %d, want 1", len(tel.events))
		}
		if tel.events[0].EventType != EventKeyCreate {
			t.Errorf("EventType = %v", tel.events[0].EventType)
		}
		if tel.events[0].Properties["key_id"] != "key-001" {
			t.Errorf("Properties[key_id] = %v", tel.events[0].Properties["key_id"])
		}
		tel.mu.Unlock()
	})

	t.Run("track with nil properties", func(t *testing.T) {
		tel.Track(EventAppLaunch, nil)
		tel.mu.Lock()
		if len(tel.events) < 2 {
			t.Error("events should have grown")
		}
		tel.mu.Unlock()
	})

	t.Run("track sets user/device/vehicle IDs", func(t *testing.T) {
		tel.SetUser("user-123")
		tel.SetDevice("device-456")
		tel.SetVehicle("vehicle-789")

		tel.Track(EventVehicleLock, map[string]interface{}{})
		tel.mu.Lock()
		last := tel.events[len(tel.events)-1]
		if last.UserID != "user-123" {
			t.Errorf("UserID = %q", last.UserID)
		}
		if last.DeviceID != "device-456" {
			t.Errorf("DeviceID = %q", last.DeviceID)
		}
		if last.VehicleID != "vehicle-789" {
			t.Errorf("VehicleID = %q", last.VehicleID)
		}
		if last.Source != SourceCloud {
			t.Errorf("Source = %v", last.Source)
		}
		if last.SessionID != "sess-track" {
			t.Errorf("SessionID = %q", last.SessionID)
		}
		tel.mu.Unlock()
	})
}

func TestTrackError(t *testing.T) {
	tel := NewTelemetry(TelemetryConfig{
		Source: SourceCloud,
	})

	t.Run("basic error tracking", func(t *testing.T) {
		tel.TrackError(0x0901, "internal error", map[string]interface{}{
			"component": "codec",
		})

		tel.mu.Lock()
		if len(tel.events) < 1 {
			t.Fatal("no events tracked")
		}
		last := tel.events[len(tel.events)-1]
		if last.EventType != EventSdkError {
			t.Errorf("EventType = %v, want sdk_error", last.EventType)
		}
		if last.Properties["error_code"] != uint16(0x0901) {
			t.Errorf("error_code = %v", last.Properties["error_code"])
		}
		if last.Properties["error_msg"] != "internal error" {
			t.Errorf("error_msg = %v", last.Properties["error_msg"])
		}
		if last.Properties["component"] != "codec" {
			t.Errorf("component = %v", last.Properties["component"])
		}
		tel.mu.Unlock()
	})

	t.Run("error with nil context", func(t *testing.T) {
		tel.TrackError(0x0000, "success", nil)

		tel.mu.Lock()
		last := tel.events[len(tel.events)-1]
		_, ok := last.Properties["error_code"]
		if !ok {
			t.Error("error_code should be present")
		}
		tel.mu.Unlock()
	})
}

func TestSetMethods(t *testing.T) {
	tel := NewTelemetry(TelemetryConfig{
		Source: SourceSDK,
	})

	t.Run("SetUser", func(t *testing.T) {
		tel.SetUser("u-001")
		if tel.userID != "u-001" {
			t.Errorf("userID = %q", tel.userID)
		}
	})

	t.Run("SetDevice", func(t *testing.T) {
		tel.SetDevice("d-001")
		if tel.deviceID != "d-001" {
			t.Errorf("deviceID = %q", tel.deviceID)
		}
	})

	t.Run("SetVehicle", func(t *testing.T) {
		tel.SetVehicle("v-001")
		if tel.vehicleID != "v-001" {
			t.Errorf("vehicleID = %q", tel.vehicleID)
		}
	})
}

func TestFlush(t *testing.T) {
	tel := NewTelemetry(TelemetryConfig{
		Source: SourceCloud,
	})

	tel.Track(EventAppLaunch, nil)
	tel.Track(EventUserLogin, nil)

	tel.mu.Lock()
	if len(tel.events) == 0 {
		t.Error("events should not be empty before flush")
	}
	tel.mu.Unlock()

	tel.Flush()

	tel.mu.Lock()
	if len(tel.events) != 0 {
		t.Errorf("events should be empty after flush, got %d", len(tel.events))
	}
	tel.mu.Unlock()
}

func TestConcurrentAccess(t *testing.T) {
	tel := NewTelemetry(TelemetryConfig{
		Source:    SourceCloud,
		SessionID: "sess-concurrent",
	})

	var wg sync.WaitGroup

	// Concurrent Track calls
	for i := 0; i < 50; i++ {
		wg.Add(1)
		go func(n int) {
			defer wg.Done()
			tel.Track(EventKeyUse, map[string]interface{}{
				"key_id":   "key-" + string(rune('0'+n%10)),
				"duration": n * 100,
			})
		}(i)
	}

	// Concurrent Set calls
	for i := 0; i < 10; i++ {
		wg.Add(1)
		go func(n int) {
			defer wg.Done()
			tel.SetUser("user-" + string(rune('0'+n%10)))
			tel.SetDevice("device-" + string(rune('0'+n%10)))
		}(i)
	}

	wg.Wait()

	// Should not panic
	tel.mu.Lock()
	if len(tel.events) == 0 {
		t.Error("should have at least some events")
	}
	tel.mu.Unlock()
}

func TestConvenienceMethods(t *testing.T) {
	tel := NewTelemetry(TelemetryConfig{
		Source: SourceCloud,
	})

	t.Run("TrackKeyUse", func(t *testing.T) {
		tel.TrackKeyUse("key-001", "vehicle-001", "BLE", 150)

		tel.mu.Lock()
		last := tel.events[len(tel.events)-1]
		if last.EventType != EventKeyUse {
			t.Errorf("EventType = %v", last.EventType)
		}
		if last.Properties["key_id"] != "key-001" {
			t.Errorf("key_id = %v", last.Properties["key_id"])
		}
		if last.Properties["vehicle_id"] != "vehicle-001" {
			t.Errorf("vehicle_id = %v", last.Properties["vehicle_id"])
		}
		if last.Properties["channel_type"] != "BLE" {
			t.Errorf("channel_type = %v", last.Properties["channel_type"])
		}
		if last.Properties["duration_ms"] != int64(150) {
			t.Errorf("duration_ms = %v", last.Properties["duration_ms"])
		}
		tel.mu.Unlock()
	})

	t.Run("TrackVehicleCommand success", func(t *testing.T) {
		tel.TrackVehicleCommand("lock", "vehicle-001", "success", nil)

		tel.mu.Lock()
		last := tel.events[len(tel.events)-1]
		if last.EventType != EventProtocolMsg {
			t.Errorf("EventType = %v", last.EventType)
		}
		if last.Properties["action"] != "lock" {
			t.Errorf("action = %v", last.Properties["action"])
		}
		if last.Properties["result"] != "success" {
			t.Errorf("result = %v", last.Properties["result"])
		}
		_, hasCode := last.Properties["error_code"]
		if hasCode {
			t.Error("error_code should not be present for success")
		}
		tel.mu.Unlock()
	})

	t.Run("TrackVehicleCommand with error", func(t *testing.T) {
		errCode := uint16(0x0407)
		tel.TrackVehicleCommand("unlock", "vehicle-002", "failed", &errCode)

		tel.mu.Lock()
		last := tel.events[len(tel.events)-1]
		if last.Properties["result"] != "failed" {
			t.Errorf("result = %v", last.Properties["result"])
		}
		if last.Properties["error_code"] != uint16(0x0407) {
			t.Errorf("error_code = %v", last.Properties["error_code"])
		}
		tel.mu.Unlock()
	})

	t.Run("TrackSecurityEvent", func(t *testing.T) {
		tel.TrackSecurityEvent("brute_force", 5, map[string]interface{}{
			"source_ip": "10.0.0.1",
		})

		tel.mu.Lock()
		last := tel.events[len(tel.events)-1]
		if last.EventType != EventSecurityAlert {
			t.Errorf("EventType = %v", last.EventType)
		}
		if last.Properties["security_event_type"] != "brute_force" {
			t.Errorf("security_event_type = %v", last.Properties["security_event_type"])
		}
		if last.Properties["threat_level"] != 5 {
			t.Errorf("threat_level = %v", last.Properties["threat_level"])
		}
		if last.Properties["source_ip"] != "10.0.0.1" {
			t.Errorf("source_ip = %v", last.Properties["source_ip"])
		}
		tel.mu.Unlock()
	})

	t.Run("TrackBleConnect", func(t *testing.T) {
		tel.TrackBleConnect("device-ble", "AA:BB:CC:DD:EE:FF", 512, true)

		tel.mu.Lock()
		last := tel.events[len(tel.events)-1]
		if last.EventType != EventBleConnect {
			t.Errorf("EventType = %v", last.EventType)
		}
		if last.Properties["device_id"] != "device-ble" {
			t.Errorf("device_id = %v", last.Properties["device_id"])
		}
		if last.Properties["mac_address"] != "AA:BB:CC:DD:EE:FF" {
			t.Errorf("mac_address = %v", last.Properties["mac_address"])
		}
		if last.Properties["mtu"] != 512 {
			t.Errorf("mtu = %v", last.Properties["mtu"])
		}
		if last.Properties["success"] != true {
			t.Errorf("success = %v", last.Properties["success"])
		}
		tel.mu.Unlock()
	})

	t.Run("TrackNfcTap", func(t *testing.T) {
		tel.TrackNfcTap("nfc-tag-001", "vehicle-nfc", true)

		tel.mu.Lock()
		last := tel.events[len(tel.events)-1]
		if last.EventType != EventNfcTap {
			t.Errorf("EventType = %v", last.EventType)
		}
		if last.Properties["nfc_tag_id"] != "nfc-tag-001" {
			t.Errorf("nfc_tag_id = %v", last.Properties["nfc_tag_id"])
		}
		if last.Properties["vehicle_id"] != "vehicle-nfc" {
			t.Errorf("vehicle_id = %v", last.Properties["vehicle_id"])
		}
		if last.Properties["success"] != true {
			t.Errorf("success = %v", last.Properties["success"])
		}
		tel.mu.Unlock()
	})

	t.Run("TrackUwbRanging", func(t *testing.T) {
		tel.TrackUwbRanging("vehicle-uwb", 5000, 100)

		tel.mu.Lock()
		last := tel.events[len(tel.events)-1]
		if last.EventType != EventUwbRanging {
			t.Errorf("EventType = %v", last.EventType)
		}
		if last.Properties["vehicle_id"] != "vehicle-uwb" {
			t.Errorf("vehicle_id = %v", last.Properties["vehicle_id"])
		}
		if last.Properties["distance_mm"] != 5000 {
			t.Errorf("distance_mm = %v", last.Properties["distance_mm"])
		}
		if last.Properties["accuracy_mm"] != 100 {
			t.Errorf("accuracy_mm = %v", last.Properties["accuracy_mm"])
		}
		tel.mu.Unlock()
	})
}

func TestQueueFullBehavior(t *testing.T) {
	// Queue capacity is 1000. We should be able to track events without
	// blocking even if the queue is (effectively) full.
	tel := NewTelemetry(TelemetryConfig{
		Source: SourceCloud,
	})

	// Add many events — queue may drop some, but should not block
	for i := 0; i < 1100; i++ {
		tel.Track(EventKeyUse, map[string]interface{}{
			"idx": i,
		})
	}

	// Should not panic or block
	tel.mu.Lock()
	eventsLen := len(tel.events)
	tel.mu.Unlock()
	if eventsLen == 0 {
		t.Log("events queue dropped due to capacity, this is expected")
	}
}

func TestGlobalTelemetry(t *testing.T) {
	t.Run("Default before Init", func(t *testing.T) {
		// Reset global
		globalTelemetry = nil
		def := Default()
		if def == nil {
			t.Fatal("Default() should not return nil")
		}
		if def.source != SourceCloud {
			t.Errorf("default source = %v, want CLOUD", def.source)
		}
	})

	t.Run("Init and Default", func(t *testing.T) {
		Init(TelemetryConfig{
			Source:    SourceSDK,
			SessionID: "global-sess",
		})
		def := Default()
		if def.source != SourceSDK {
			t.Errorf("source = %v, want SDK", def.source)
		}
		if def.sessionID != "global-sess" {
			t.Errorf("sessionID = %q", def.sessionID)
		}
	})

	t.Run("Track function", func(t *testing.T) {
		Init(TelemetryConfig{
			Source: SourceCloud,
		})
		Track(EventAppLaunch, map[string]interface{}{})

		def := Default()
		def.mu.Lock()
		if len(def.events) == 0 {
			t.Error("Track() should add events")
		}
		if def.events[len(def.events)-1].EventType != EventAppLaunch {
			t.Errorf("EventType = %v", def.events[len(def.events)-1].EventType)
		}
		def.mu.Unlock()
	})
}

func TestGenerateEventID(t *testing.T) {
	id1 := generateEventID()
	id2 := generateEventID()

	if id1 == "" {
		t.Error("id1 should not be empty")
	}
	if id2 == "" {
		t.Error("id2 should not be empty")
	}
	if id1 == id2 {
		t.Log("IDs happened to collide (unlikely but possible)")
	}
}

func TestRandomString(t *testing.T) {
	s1 := randomString(8)
	_ = randomString(8) // s2 unused but verify it compiles

	if len(s1) != 8 {
		t.Errorf("len = %d, want 8", len(s1))
	}
	if s1 == "" {
		t.Error("should not be empty")
	}
	// Should be alphanumeric
	for _, c := range s1 {
		if !((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
			t.Errorf("invalid char %c in random string", c)
		}
	}

	// Edge case
	s3 := randomString(0)
	if s3 != "" {
		t.Errorf("randomString(0) = %q, want empty", s3)
	}
}
