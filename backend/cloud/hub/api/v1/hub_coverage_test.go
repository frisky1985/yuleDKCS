package v1

import (
	"testing"

	"google.golang.org/protobuf/proto"
)

// ─────────────────────────────────────────────────────────────
// Protocol message field access tests
// ─────────────────────────────────────────────────────────────

func TestBindKeyRequestRoundtrip(t *testing.T) {
	req := &BindKeyRequest{
		VehicleId:  "vehicle-001",
		UserId:     "user-001",
		DeviceId:   "dev-a1b2c3",
		Protocol:   Protocol_ICCE,
		Vendor:     PhoneVendor_APPLE,
		KeyType:    KeyType_OWNER,
		ValidFrom:  1000000,
		ValidUntil: 2000000,
		AccessLevel: &AccessLevel{
			Lock:   true,
			Unlock: true,
			Engine: true,
		},
	}
	if req.GetVehicleId() != "vehicle-001" {
		t.Errorf("VehicleId: want vehicle-001, got %s", req.GetVehicleId())
	}
	if req.GetUserId() != "user-001" {
		t.Errorf("UserId: want user-001, got %s", req.GetUserId())
	}
	if req.GetProtocol() != Protocol_ICCE {
		t.Errorf("Protocol: want ICCE, got %s", req.GetProtocol())
	}
	if req.GetDeviceId() != "dev-a1b2c3" {
		t.Errorf("DeviceId: want dev-a1b2c3, got %s", req.GetDeviceId())
	}
	if req.GetVendor() != PhoneVendor_APPLE {
		t.Errorf("Vendor: want APPLE, got %s", req.GetVendor())
	}
	if !req.GetAccessLevel().GetLock() {
		t.Errorf("AccessLevel.Lock should be true")
	}
}

func TestBindKeyResponseRoundtrip(t *testing.T) {
	resp := &BindKeyResponse{
		Key: &DigitalKey{
			KeyId:     "key-abc-123",
			VehicleId: "vehicle-001",
			UserId:    "user-001",
			KeyType:   KeyType_OWNER,
			Protocol:  Protocol_ICCE,
			Status:    KeyStatus_ACTIVE,
		},
	}
	if resp.GetKey().GetKeyId() != "key-abc-123" {
		t.Errorf("KeyId mismatch")
	}
	if resp.GetKey().GetStatus() != KeyStatus_ACTIVE {
		t.Errorf("Status mismatch")
	}
	if resp.GetKey().GetProtocol() != Protocol_ICCE {
		t.Errorf("Protocol mismatch")
	}
}

func TestControlCommandRequestRoundtrip(t *testing.T) {
	req := &ControlCommandRequest{
		VehicleId: "vehicle-002",
		UserId:    "user-003",
		KeyId:     "key-def-456",
		Action:    "unlock",
		Source:    3, // UWB
	}
	if req.GetVehicleId() != "vehicle-002" {
		t.Errorf("VehicleId mismatch")
	}
	if req.GetKeyId() != "key-def-456" {
		t.Errorf("KeyId mismatch")
	}
	if req.GetUserId() != "user-003" {
		t.Errorf("UserId mismatch")
	}
	if req.GetAction() != "unlock" {
		t.Errorf("Action: want unlock, got %s", req.GetAction())
	}
	if req.GetSource() != 3 {
		t.Errorf("Source: want 3, got %d", req.GetSource())
	}
}

func TestControlCommandResponseRoundtrip(t *testing.T) {
	resp := &ControlCommandResponse{
		CmdId:      "cmd-001",
		ResultCode: 0,
		ErrorMsg:   "",
	}
	if resp.GetCmdId() != "cmd-001" {
		t.Errorf("CmdId mismatch")
	}
	if resp.GetResultCode() != 0 {
		t.Errorf("ResultCode mismatch")
	}
}

func TestVehicleStatusRequestRoundtrip(t *testing.T) {
	req := &VehicleStatusRequest{
		VehicleId: "vehicle-v-01",
	}
	if req.GetVehicleId() != "vehicle-v-01" {
		t.Errorf("VehicleId mismatch")
	}
}

func TestVehicleStatusUpdateRoundtrip(t *testing.T) {
	status := &VehicleStatusUpdate{
		VehicleId:    "vehicle-v-01",
		LockStatus:   1,
		EngineStatus: 0,
		BatteryPct:   85,
		InteriorTemp: 22,
		AlarmStatus:  0,
		Latitude:     31.2304,
		Longitude:    121.4737,
	}
	if status.GetVehicleId() != "vehicle-v-01" {
		t.Errorf("VehicleId mismatch")
	}
	if status.GetLockStatus() != 1 {
		t.Errorf("LockStatus: want 1, got %d", status.GetLockStatus())
	}
	if status.GetBatteryPct() != 85 {
		t.Errorf("BatteryPct: want 85, got %d", status.GetBatteryPct())
	}
	if status.GetInteriorTemp() != 22 {
		t.Errorf("InteriorTemp: want 22, got %d", status.GetInteriorTemp())
	}
}

// ─────────────────────────────────────────────────────────────
// Key sharing tests
// ─────────────────────────────────────────────────────────────

func TestCreateShareRequestRoundtrip(t *testing.T) {
	req := &CreateShareRequest{
		KeyId:       "key-share-1",
		FromUserId:  "user-owner",
		ToUserId:    "user-friend",
		ToVendor:    PhoneVendor_XIAOMI,
		ValidFrom:   1000000,
		ValidUntil:  2000000,
		MaxUses:     10,
		AccessLevel: &AccessLevel{Lock: true, Unlock: true},
	}
	if req.GetKeyId() != "key-share-1" {
		t.Errorf("KeyId mismatch")
	}
	if req.GetFromUserId() != "user-owner" {
		t.Errorf("FromUserId mismatch")
	}
	if req.GetToUserId() != "user-friend" {
		t.Errorf("ToUserId mismatch")
	}
	if req.GetMaxUses() != 10 {
		t.Errorf("MaxUses: want 10, got %d", req.GetMaxUses())
	}
}

func TestCreateShareResponseRoundtrip(t *testing.T) {
	resp := &CreateShareResponse{
		ShareId:   "share-abc",
		ShareCode: "123456",
	}
	if resp.GetShareId() != "share-abc" {
		t.Errorf("ShareId mismatch")
	}
	if resp.GetShareCode() != "123456" {
		t.Errorf("ShareCode: want 123456, got %s", resp.GetShareCode())
	}
}

func TestAcceptShareResponseRoundtrip(t *testing.T) {
	resp := &AcceptShareResponse{
		Key: &DigitalKey{
			KeyId:   "shared-key-001",
			KeyType: KeyType_FRIEND,
		},
	}
	if resp.GetKey().GetKeyId() != "shared-key-001" {
		t.Errorf("Shared KeyId mismatch")
	}
	if resp.GetKey().GetKeyType() != KeyType_FRIEND {
		t.Errorf("KeyType: want FRIEND, got %s", resp.GetKey().GetKeyType())
	}
}

// ─────────────────────────────────────────────────────────────
// Revocation tests
// ─────────────────────────────────────────────────────────────

func TestRevokeKeyRequestRoundtrip(t *testing.T) {
	req := &RevokeKeyRequest{
		KeyId:   "key-revoke-1",
		Reason:  "lost phone",
		TraceId: "trace-001",
	}
	if req.GetKeyId() != "key-revoke-1" {
		t.Errorf("KeyId mismatch")
	}
	if req.GetReason() != "lost phone" {
		t.Errorf("Reason: want 'lost phone', got '%s'", req.GetReason())
	}
}

func TestRevokeKeyResponseRoundtrip(t *testing.T) {
	resp := &RevokeKeyResponse{}
	if resp.GetErrorCode() != "" {
		t.Errorf("ErrorCode should be empty on success")
	}
}

// ─────────────────────────────────────────────────────────────
// Enum values coverage
// ─────────────────────────────────────────────────────────────

func TestProtocolEnumAllValues(t *testing.T) {
	tests := []struct {
		p    Protocol
		name string
		num  int32
	}{
		{Protocol_PROTOCOL_UNSPECIFIED, "PROTOCOL_UNSPECIFIED", 0},
		{Protocol_CCC_DK3, "CCC_DK3", 1},
		{Protocol_ICCOA_DK30, "ICCOA_DK30", 2},
		{Protocol_ICCOA_DK40, "ICCOA_DK40", 3},
		{Protocol_ICCE, "ICCE", 4},
	}
	for _, tt := range tests {
		if got := tt.p.String(); got != tt.name {
			t.Errorf("Protocol(%d).String() = %q, want %q", tt.p, got, tt.name)
		}
		if got := int32(tt.p); got != tt.num {
			t.Errorf("Protocol(%d) value = %d, want %d", tt.p, got, tt.num)
		}
	}
}

func TestPhoneVendorEnum_AllValues(t *testing.T) {
	tests := []struct {
		v    PhoneVendor
		name string
	}{
		{PhoneVendor_VENDOR_UNSPECIFIED, "VENDOR_UNSPECIFIED"},
		{PhoneVendor_APPLE, "APPLE"},
		{PhoneVendor_SAMSUNG, "SAMSUNG"},
		{PhoneVendor_XIAOMI, "XIAOMI"},
		{PhoneVendor_OPPO, "OPPO"},
		{PhoneVendor_VIVO, "VIVO"},
		{PhoneVendor_HUAWEI, "HUAWEI"},
	}
	for _, tt := range tests {
		if got := tt.v.String(); got != tt.name {
			t.Errorf("PhoneVendor(%d).String() = %q, want %q", tt.v, got, tt.name)
		}
	}
}

func TestKeyTypeEnum_AllValues(t *testing.T) {
	tests := []struct {
		kt   KeyType
		name string
	}{
		{KeyType_KEY_TYPE_UNSPECIFIED, "KEY_TYPE_UNSPECIFIED"},
		{KeyType_OWNER, "OWNER"},
		{KeyType_FRIEND, "FRIEND"},
		{KeyType_SERVICE, "SERVICE"},
		{KeyType_TEMPORARY, "TEMPORARY"},
	}
	for _, tt := range tests {
		if got := tt.kt.String(); got != tt.name {
			t.Errorf("KeyType(%d).String() = %q, want %q", tt.kt, got, tt.name)
		}
	}
}

func TestKeyStatusEnum_AllValues(t *testing.T) {
	tests := []struct {
		ks   KeyStatus
		name string
	}{
		{KeyStatus_KEY_STATUS_UNSPECIFIED, "KEY_STATUS_UNSPECIFIED"},
		{KeyStatus_ACTIVE, "ACTIVE"},
		{KeyStatus_SUSPENDED, "SUSPENDED"},
		{KeyStatus_REVOKED, "REVOKED"},
		{KeyStatus_EXPIRED, "EXPIRED"},
	}
	for _, tt := range tests {
		if got := tt.ks.String(); got != tt.name {
			t.Errorf("KeyStatus(%d).String() = %q, want %q", tt.ks, got, tt.name)
		}
	}
}

// ─────────────────────────────────────────────────────────────
// DigitalKey helper
// ─────────────────────────────────────────────────────────────

func TestDigitalKeyGetMethods(t *testing.T) {
	dk := &DigitalKey{
		KeyId:          "key-001",
		VehicleId:      "vehicle-001",
		DeviceId:       "device-001",
		UserId:         "user-001",
		KeyType:        KeyType_SERVICE,
		Protocol:       Protocol_CCC_DK3,
		AccessLevel:    &AccessLevel{Lock: true, Unlock: true},
		DistanceLimit:  5000,
		MaxUses:        100,
		UsedCount:      3,
		KeyVersion:     2,
		Status:         KeyStatus_ACTIVE,
		ValidFrom:      1000,
		ValidUntil:     2000,
		CreatedAt:      500,
	}
	if dk.GetKeyId() != "key-001" { t.Errorf("KeyId") }
	if dk.GetVehicleId() != "vehicle-001" { t.Errorf("VehicleId") }
	if dk.GetDeviceId() != "device-001" { t.Errorf("DeviceId") }
	if dk.GetUserId() != "user-001" { t.Errorf("UserId") }
	if dk.GetKeyType() != KeyType_SERVICE { t.Errorf("KeyType") }
	if dk.GetProtocol() != Protocol_CCC_DK3 { t.Errorf("Protocol") }
	if dk.GetDistanceLimit() != 5000 { t.Errorf("DistanceLimit") }
	if dk.GetMaxUses() != 100 { t.Errorf("MaxUses") }
	if dk.GetUsedCount() != 3 { t.Errorf("UsedCount") }
	if dk.GetKeyVersion() != 2 { t.Errorf("KeyVersion") }
	if dk.GetStatus() != KeyStatus_ACTIVE { t.Errorf("Status") }
	if dk.GetValidFrom() != 1000 { t.Errorf("ValidFrom") }
	if dk.GetValidUntil() != 2000 { t.Errorf("ValidUntil") }
	if dk.GetCreatedAt() != 500 { t.Errorf("CreatedAt") }
}

// ─────────────────────────────────────────────────────────────
// Forward/Callback/HealthCheck message tests
// ─────────────────────────────────────────────────────────────

func TestForwardRequestRoundtrip(t *testing.T) {
	req := &ForwardRequest{
		Vendor:    PhoneVendor_OPPO,
		Protocol:  Protocol_ICCE,
		Operation: "bind",
		Payload:   []byte{0x01, 0x02, 0x03},
		TraceId:   "trace-001",
	}
	if req.GetVendor() != PhoneVendor_OPPO { t.Errorf("Vendor") }
	if req.GetProtocol() != Protocol_ICCE { t.Errorf("Protocol") }
	if req.GetOperation() != "bind" { t.Errorf("Operation") }
	if len(req.GetPayload()) != 3 { t.Errorf("Payload len") }
}

func TestForwardResponseRoundtrip(t *testing.T) {
	resp := &ForwardResponse{
		Payload: []byte{0x0A, 0x0B},
	}
	if len(resp.GetPayload()) != 2 { t.Errorf("Payload len") }
}

func TestCallbackRequestRoundtrip(t *testing.T) {
	req := &CallbackRequest{
		Vendor:     PhoneVendor_APPLE,
		Protocol:   Protocol_CCC_DK3,
		Operation:  "notify",
		CallbackId: "cb-001",
	}
	if req.GetCallbackId() != "cb-001" { t.Errorf("CallbackId") }
}

func TestHealthCheckRequestResponse(t *testing.T) {
	req := &HealthCheckRequest{}
	_ = req // ensure message exists and compiles

	resp := &HealthCheckResponse{
		Healthy: true,
		Adapters: []*AdapterStatus{
			{Vendor: "apple", Protocol: "CCC_DK3", Healthy: true},
		},
	}
	if !resp.GetHealthy() { t.Errorf("Healthy should be true") }
	if len(resp.GetAdapters()) != 1 { t.Errorf("Adapters count") }
}

// ─────────────────────────────────────────────────────────────
// Proto marshaling/unmarshaling roundtrips
// ─────────────────────────────────────────────────────────────

func TestBindKeyRequestMarshalUnmarshal(t *testing.T) {
	orig := &BindKeyRequest{
		VehicleId: "v-001",
		UserId:    "u-001",
		Protocol:  Protocol_ICCE,
		DeviceId:  "d-001",
	}
	data, err := proto.Marshal(orig)
	if err != nil {
		t.Fatalf("Marshal: %v", err)
	}
	var decoded BindKeyRequest
	if err := proto.Unmarshal(data, &decoded); err != nil {
		t.Fatalf("Unmarshal: %v", err)
	}
	if decoded.GetVehicleId() != "v-001" { t.Errorf("After roundtrip: VehicleId") }
	if decoded.GetProtocol() != Protocol_ICCE { t.Errorf("After roundtrip: Protocol") }
}

func TestControlCommandRequestMarshalUnmarshal(t *testing.T) {
	orig := &ControlCommandRequest{
		VehicleId: "v-002",
		KeyId:     "k-002",
		Action:    "lock",
	}
	data, err := proto.Marshal(orig)
	if err != nil { t.Fatalf("Marshal: %v", err) }
	var decoded ControlCommandRequest
	if err := proto.Unmarshal(data, &decoded); err != nil {
		t.Fatalf("Unmarshal: %v", err)
	}
	if decoded.GetAction() != "lock" { t.Errorf("After roundtrip: Action") }
}

func TestDigitalKeyMarshalRoundtrip(t *testing.T) {
	orig := &DigitalKey{
		KeyId:     "key-rt-001",
		VehicleId: "v-rt-001",
		Status:    KeyStatus_ACTIVE,
	}
	data, err := proto.Marshal(orig)
	if err != nil { t.Fatalf("Marshal: %v", err) }
	var decoded DigitalKey
	if err := proto.Unmarshal(data, &decoded); err != nil {
		t.Fatalf("Unmarshal: %v", err)
	}
	if decoded.GetKeyId() != "key-rt-001" { t.Errorf("After roundtrip: KeyId") }
}
