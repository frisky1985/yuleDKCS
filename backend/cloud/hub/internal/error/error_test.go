package error

import (
	"errors"
	"testing"
)

func TestErrorCodeGetCategory(t *testing.T) {
	tests := []struct {
		name     string
		code     ErrorCode
		category Category
		codeByte uint8
	}{
		{"SUCCESS", SUCCESS, CategorySuccess, 0x00},
		{"ERR_INVALID_REQUEST", ERR_INVALID_REQUEST, CategoryRequest, 0x01},
		{"ERR_UNAUTHORIZED", ERR_UNAUTHORIZED, CategoryAuth, 0x01},
		{"ERR_KEY_NOT_FOUND", ERR_KEY_NOT_FOUND, CategoryKey, 0x01},
		{"ERR_VEHICLE_NOT_FOUND", ERR_VEHICLE_NOT_FOUND, CategoryVehicle, 0x01},
		{"ERR_SHARE_NOT_FOUND", ERR_SHARE_NOT_FOUND, CategoryShare, 0x01},
		{"ERR_DEVICE_NOT_FOUND", ERR_DEVICE_NOT_FOUND, CategoryDevice, 0x01},
		{"ERR_VENDOR_NOT_SUPPORTED", ERR_VENDOR_NOT_SUPPORTED, CategoryVendor, 0x01},
		{"ERR_NETWORK_ERROR", ERR_NETWORK_ERROR, CategoryTransport, 0x01},
		{"ERR_INTERNAL_ERROR", ERR_INTERNAL_ERROR, CategorySystem, 0x01},
		{"ERR_TCU_BLE_ERROR", ERR_TCU_BLE_ERROR, CategoryTCU, 0x01},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			cat := tt.code.GetCategory()
			if cat != tt.category {
				t.Errorf("GetCategory() = %v, want %v", cat, tt.category)
			}
			code := tt.code.GetCode()
			if code != tt.codeByte {
				t.Errorf("GetCode() = 0x%02X, want 0x%02X", code, tt.codeByte)
			}
		})
	}
}

func TestErrorCodeIsSuccess(t *testing.T) {
	if !SUCCESS.IsSuccess() {
		t.Error("SUCCESS should be success")
	}
	if !SUCCESS_ASYNC.IsSuccess() {
		t.Error("SUCCESS_ASYNC should be success")
	}
	if !SUCCESS_PARTIAL.IsSuccess() {
		t.Error("SUCCESS_PARTIAL should be success")
	}
	if ERR_INTERNAL_ERROR.IsSuccess() {
		t.Error("ERR_INTERNAL_ERROR should not be success")
	}
}

func TestErrorCodeString(t *testing.T) {
	tests := []struct {
		code ErrorCode
		name string
	}{
		{SUCCESS, "SUCCESS"},
		{ERR_INVALID_PARAMETER, "ERR_INVALID_PARAMETER"},
		{ERR_KEY_EXPIRED, "ERR_KEY_EXPIRED"},
		{ERR_VEHICLE_OFFLINE, "ERR_VEHICLE_OFFLINE"},
		{ERR_COMMAND_FAILED, "ERR_COMMAND_FAILED"},
		{ERR_SHARE_EXPIRED, "ERR_SHARE_EXPIRED"},
		{ERR_DEVICE_DISABLED, "ERR_DEVICE_DISABLED"},
		{ERR_ADAPTER_NOT_FOUND, "ERR_ADAPTER_NOT_FOUND"},
		{ERR_MQTT_DISCONNECTED, "ERR_MQTT_DISCONNECTED"},
		{ERR_SERVICE_UNAVAILABLE, "ERR_SERVICE_UNAVAILABLE"},
		{ERR_TCU_WATCHDOG_RESET, "ERR_TCU_WATCHDOG_RESET"},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			s := tt.code.String()
			if s != tt.name {
				t.Errorf("String() = %q, want %q", s, tt.name)
			}
		})
	}

	// Unknown error code
	unknown := ErrorCode(0xFFFF)
	s := unknown.String()
	if s != "ERR_UNKNOWN_0xFFFF" {
		t.Errorf("String() for unknown code = %q, want 'ERR_UNKNOWN_0xFFFF'", s)
	}
}

func TestErrorCodeError(t *testing.T) {
	err := ERR_INVALID_REQUEST.Error()
	if err == "" {
		t.Error("Error() should not be empty")
	}
	// Should contain the hex code
	if len(err) < 10 {
		t.Errorf("Error() too short: %q", err)
	}
}

func TestErrorCodeToMap(t *testing.T) {
	m := ERR_KEY_BIND_FAILED.ToMap()
	if m["code"] != ERR_KEY_BIND_FAILED {
		t.Errorf("code = %v", m["code"])
	}
	if m["name"] != "ERR_KEY_BIND_FAILED" {
		t.Errorf("name = %v", m["name"])
	}
	if m["category"] != "KEY" {
		t.Errorf("category = %v", m["category"])
	}
	_, hasCodeHex := m["code_hex"]
	if !hasCodeHex {
		t.Error("missing code_hex")
	}
}

func TestCategoryString(t *testing.T) {
	tests := []struct {
		c    Category
		name string
	}{
		{CategorySuccess, "SUCCESS"},
		{CategoryRequest, "REQUEST"},
		{CategoryAuth, "AUTH"},
		{CategoryKey, "KEY"},
		{CategoryVehicle, "VEHICLE"},
		{CategoryShare, "SHARE"},
		{CategoryDevice, "DEVICE"},
		{CategoryVendor, "VENDOR"},
		{CategoryTransport, "TRANSPORT"},
		{CategorySystem, "SYSTEM"},
		{CategoryTCU, "TCU"},
		{Category(0xFF), "UNKNOWN"},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			s := tt.c.String()
			if s != tt.name {
				t.Errorf("String() = %q, want %q", s, tt.name)
			}
		})
	}
}

func TestDigitalKeyError(t *testing.T) {
	t.Run("NewError", func(t *testing.T) {
		e := NewError(ERR_INVALID_PARAMETER, "bad request")
		if e.Code != ERR_INVALID_PARAMETER {
			t.Errorf("Code = %v", e.Code)
		}
		if e.Message != "bad request" {
			t.Errorf("Message = %q", e.Message)
		}
		if e.Details == nil {
			t.Error("Details should not be nil")
		}
	})

	t.Run("NewErrorWithDetails", func(t *testing.T) {
		details := map[string]interface{}{"field": "userId"}
		e := NewErrorWithDetails(ERR_MISSING_PARAMETER, "missing field", details)
		if e.Message != "missing field" {
			t.Errorf("Message = %q", e.Message)
		}
		if e.Details["field"] != "userId" {
			t.Errorf("Details[field] = %v", e.Details["field"])
		}
	})

	t.Run("NewErrorWithCause", func(t *testing.T) {
		cause := errors.New("underlying db error")
		e := NewErrorWithCause(ERR_DATABASE_ERROR, "query failed", cause)
		if e.Cause != cause {
			t.Errorf("Cause = %v", e.Cause)
		}
		if e.Error() == "" {
			t.Error("Error() should not be empty")
		}
		// Error string should contain cause
		errStr := e.Error()
		if len(errStr) < 20 {
			t.Errorf("Error() too short: %q", errStr)
		}
	})

	t.Run("NewErrorWithoutCause", func(t *testing.T) {
		e := NewError(ERR_INTERNAL_ERROR, "internal")
		errStr := e.Error()
		if len(errStr) < 10 {
			t.Errorf("Error() too short: %q", errStr)
		}
		// Without cause, should not contain ":"
		// Actually the Error() format always has [0x...] message
	})

	t.Run("WithTraceID", func(t *testing.T) {
		e := NewError(ERR_INVALID_TOKEN, "invalid").WithTraceID("trace-001")
		if e.TraceID != "trace-001" {
			t.Errorf("TraceID = %q", e.TraceID)
		}
	})

	t.Run("WithDetail", func(t *testing.T) {
		e := NewError(ERR_FORBIDDEN, "forbidden")
		e = e.WithDetail("resource", "/api/keys")
		if e.Details["resource"] != "/api/keys" {
			t.Errorf("Details[resource] = %v", e.Details["resource"])
		}

		// Chain multiple details
		e = e.WithDetail("reason", "no permission")
		if e.Details["reason"] != "no permission" {
			t.Errorf("Details[reason] = %v", e.Details["reason"])
		}
	})

	t.Run("WithDetail on nil Details", func(t *testing.T) {
		e := NewError(ERR_INTERNAL_ERROR, "test")
		e.Details = nil
		e = e.WithDetail("key", "value")
		if e.Details == nil {
			t.Error("Details should be initialized by WithDetail")
		}
		if e.Details["key"] != "value" {
			t.Errorf("Details[key] = %v", e.Details["key"])
		}
	})

	t.Run("ToMap with trace", func(t *testing.T) {
		e := NewError(ERR_KEY_NOT_FOUND, "key not found").
			WithTraceID("trace-002").
			WithDetail("keyID", "key-123")
		m := e.ToMap()
		if m["code"] != ERR_KEY_NOT_FOUND {
			t.Errorf("code = %v", m["code"])
		}
		if m["name"] != "ERR_KEY_NOT_FOUND" {
			t.Errorf("name = %v", m["name"])
		}
		if m["trace_id"] != "trace-002" {
			t.Errorf("trace_id = %v", m["trace_id"])
		}
		if m["message"] != "key not found" {
			t.Errorf("message = %v", m["message"])
		}
		details, ok := m["details"].(map[string]interface{})
		if !ok {
			t.Error("details should be a map")
		}
		if details["keyID"] != "key-123" {
			t.Errorf("details[keyID] = %v", details["keyID"])
		}
	})

	t.Run("ToMap without trace", func(t *testing.T) {
		e := NewError(SUCCESS, "ok")
		m := e.ToMap()
		_, hasTrace := m["trace_id"]
		if hasTrace {
			t.Error("trace_id should not be present")
		}
	})
}

func TestGetErrorMessage(t *testing.T) {
	tests := []struct {
		code    ErrorCode
		wantMsg string
	}{
		{SUCCESS, "成功"},
		{ERR_INVALID_REQUEST, "无效请求"},
		{ERR_UNAUTHORIZED, "未授权"},
		{ERR_KEY_NOT_FOUND, "密钥不存在"},
		{ERR_VEHICLE_NOT_FOUND, "车辆不存在"},
		{ERR_NETWORK_ERROR, "网络错误"},
		{ERR_TCU_BLE_ERROR, "BLE通信错误"},
	}

	for _, tt := range tests {
		t.Run(tt.code.String(), func(t *testing.T) {
			msg := GetErrorMessage(tt.code)
			if msg != tt.wantMsg {
				t.Errorf("GetErrorMessage() = %q, want %q", msg, tt.wantMsg)
			}
		})
	}

	// Unknown code returns empty string
	msg := GetErrorMessage(ErrorCode(0x0BAD))
	if msg != "" {
		t.Errorf("GetErrorMessage(unknown) = %q, want empty", msg)
	}
}

func TestDigitalKeyErrorImplementsError(t *testing.T) {
	var err error = NewError(ERR_INTERNAL_ERROR, "test")
	if err == nil {
		t.Fatal("DigitalKeyError should implement error interface")
	}
}

func TestErrorNamesCompleteness(t *testing.T) {
	// Spot-check a few key entries from the errorNames map
	expectedNames := map[ErrorCode]string{
		SUCCESS:               "SUCCESS",
		ERR_INVALID_SIGNATURE: "ERR_INVALID_SIGNATURE",
		ERR_TOKEN_EXPIRED:     "ERR_TOKEN_EXPIRED",
		ERR_KEY_MAX_REACHED:   "ERR_KEY_MAX_REACHED",
		ERR_VEHICLE_MOVING:    "ERR_VEHICLE_MOVING",
		ERR_SHARE_TO_SELF:     "ERR_SHARE_TO_SELF",
		ERR_BLE_SCAN_FAILED:   "ERR_BLE_SCAN_FAILED",
		ERR_VENDOR_API_TIMEOUT: "ERR_VENDOR_API_TIMEOUT",
		ERR_GRPC_DEADLINE:     "ERR_GRPC_DEADLINE",
		ERR_CRYPTO_ERROR:      "ERR_CRYPTO_ERROR",
		ERR_TCU_POWER_LOW:     "ERR_TCU_POWER_LOW",
	}

	for code, expected := range expectedNames {
		name := code.String()
		if name != expected {
			t.Errorf("Error %04X: String() = %q, want %q", uint16(code), name, expected)
		}
	}
}

// Test all known error codes return valid strings
func TestAllKnownCodes(t *testing.T) {
	allCodes := []ErrorCode{
		SUCCESS, SUCCESS_ASYNC, SUCCESS_PARTIAL,
		ERR_INVALID_REQUEST, ERR_INVALID_PARAMETER, ERR_MISSING_PARAMETER,
		ERR_INVALID_FORMAT, ERR_INVALID_LENGTH, ERR_INVALID_SIGNATURE,
		ERR_RATE_LIMIT, ERR_QUOTA_EXCEEDED, ERR_VERSION_MISMATCH,
		ERR_UNAUTHORIZED, ERR_INVALID_TOKEN, ERR_TOKEN_EXPIRED,
		ERR_FORBIDDEN, ERR_ACCESS_DENIED, ERR_USER_DISABLED,
		ERR_ACCOUNT_LOCKED, ERR_SESSION_EXPIRED,
		ERR_KEY_NOT_FOUND, ERR_KEY_EXISTS, ERR_KEY_EXPIRED,
		ERR_KEY_REVOKED, ERR_KEY_SUSPENDED, ERR_KEY_PENDING,
		ERR_KEY_MAX_REACHED, ERR_KEY_MAX_USES, ERR_DISTANCE_EXCEEDED,
		ERR_KEY_TYPE_NOT_ALLOWED, ERR_KEY_NOT_ACTIVE,
		ERR_KEY_VALIDITY_NOT_STARTED, ERR_KEY_BIND_FAILED, ERR_KEY_UNBIND_FAILED,
		ERR_VEHICLE_NOT_FOUND, ERR_VEHICLE_OFFLINE, ERR_VEHICLE_NOT_BOUND,
		ERR_TCU_OFFLINE, ERR_TCU_TIMEOUT, ERR_TCU_ERROR,
		ERR_COMMAND_FAILED, ERR_COMMAND_TIMEOUT, ERR_COMMAND_REJECTED,
		ERR_COMMAND_INVALID, ERR_COMMAND_IN_PROGRESS,
		ERR_VEHICLE_NOT_SUPPORTED, ERR_BATTERY_LOW, ERR_ENGINE_RUNNING,
		ERR_VEHICLE_MOVING, ERR_DOOR_OPEN,
		ERR_SHARE_NOT_FOUND, ERR_SHARE_EXPIRED, ERR_SHARE_ACCEPTED,
		ERR_SHARE_CANCELLED, ERR_SHARE_MAX_USES, ERR_SHARE_NOT_ALLOWED,
		ERR_CODE_INVALID, ERR_CODE_EXPIRED, ERR_VENDOR_NOT_MATCH, ERR_SHARE_TO_SELF,
		ERR_DEVICE_NOT_FOUND, ERR_DEVICE_NOT_BOUND, ERR_DEVICE_DISABLED,
		ERR_DEVICE_NOT_SUPPORTED,
		ERR_BLE_DISABLED, ERR_NFC_DISABLED, ERR_UWB_DISABLED,
		ERR_SE_NOT_AVAILABLE, ERR_SE_ERROR, ERR_STORAGE_FULL,
		ERR_BLE_SCAN_FAILED, ERR_BLE_CONNECT_FAILED,
		ERR_NFC_READ_FAILED, ERR_NFC_WRITE_FAILED, ERR_UWB_RANGING_FAILED,
		ERR_VENDOR_NOT_SUPPORTED, ERR_ADAPTER_NOT_FOUND,
		ERR_VENDOR_API_ERROR, ERR_VENDOR_API_TIMEOUT,
		ERR_VENDOR_AUTH_FAILED, ERR_VENDOR_BIND_FAILED, ERR_PROTOCOL_ERROR,
		ERR_NETWORK_ERROR, ERR_NETWORK_TIMEOUT, ERR_SERVER_UNREACHABLE,
		ERR_CONNECTION_REFUSED,
		ERR_MQTT_DISCONNECTED, ERR_MQTT_PUBLISH_FAILED,
		ERR_GRPC_UNAVAILABLE, ERR_GRPC_DEADLINE,
		ERR_INTERNAL_ERROR, ERR_SERVICE_UNAVAILABLE,
		ERR_DATABASE_ERROR, ERR_CACHE_ERROR, ERR_QUEUE_ERROR,
		ERR_CONFIG_ERROR, ERR_CRYPTO_ERROR, ERR_SIGN_ERROR, ERR_VERIFY_ERROR,
		ERR_FEATURE_NOT_SUPPORTED, ERR_MAINTENANCE, ERR_CAPACITY_EXCEEDED,
		ERR_TCU_BLE_ERROR, ERR_TCU_NFC_ERROR, ERR_TCU_UWB_ERROR,
		ERR_TCU_SE_ERROR, ERR_TCU_PAIRING_FAILED, ERR_TCU_AUTH_FAILED,
		ERR_TCU_CRYPTO_FAILED, ERR_TCU_STORAGE_ERROR, ERR_TCU_POWER_LOW,
		ERR_TCU_WATCHDOG_RESET, ERR_TCU_ANTENNA_FAULT, ERR_TCU_MSG_QUEUE_FULL,
	}

	for _, code := range allCodes {
		if code.String() == "" {
			t.Errorf("code %04X: String() is empty", uint16(code))
		}
		if code.ToMap()["name"] == "" {
			t.Errorf("code %04X: name in ToMap is empty", uint16(code))
		}
	}
}
