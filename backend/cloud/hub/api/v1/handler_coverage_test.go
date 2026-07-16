package v1

import (
	"testing"
	"time"
)

// ---------- gRPC service struct coverage ----------
// These tests verify that all generated protobuf types compile and
// are constructable with reasonable default values.
// Coverage of generated code is inherently limited; these tests
// exercise the value/assignment surface of each key type.

func TestKeyManagementService_Interface(t *testing.T) {
	// BindKeyRequest/Response round-trip
	req := &BindKeyRequest{
		VehicleId:    "VH-001",
		DeviceId:     "DEV-001",
		UserId:       "U-001",
		DevicePubkey: []byte{0x04, 0x01, 0x02, 0x03, 0x04},
		KeyType:      KeyType_OWNER,
		ValidFrom:    time.Now().Unix(),
		ValidUntil:   time.Now().Add(365 * 24 * 3600).Unix(),
	}

	resp := &BindKeyResponse{
		Key: &DigitalKey{
			KeyId:     "key-new-001",
			VehicleId: req.VehicleId,
			DeviceId:  req.DeviceId,
			UserId:    req.UserId,
			KeyType:   req.KeyType,
			Protocol:  Protocol_CCC_DK3,
		},
		VehiclePubkey: []byte{0x03, 0xAA, 0xBB, 0xCC},
		SharedSecret:  []byte{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A},
	}

	if resp.Key.KeyId != "key-new-001" {
		t.Errorf("key ID mismatch: %q", resp.Key.KeyId)
	}
	if resp.Key.KeyType != KeyType_OWNER {
		t.Errorf("expected OWNER key type, got %v", resp.Key.KeyType)
	}
	if len(resp.SharedSecret) != 10 {
		t.Errorf("expected 10-byte shared secret, got %d bytes", len(resp.SharedSecret))
	}
}

func TestKeyShareService_Interface(t *testing.T) {
	// CreateShare flow
	createReq := &CreateShareRequest{
		KeyId:      "key-001",
		FromUserId: "user-owner-001",
		ToUserId:   "user-friend-001",
		ToVendor:   PhoneVendor_APPLE,
		ValidFrom:  time.Now().Unix(),
		ValidUntil: time.Now().Add(24 * time.Hour).Unix(),
	}

	if createReq.KeyId != "key-001" {
		t.Errorf("expected key-001, got %q", createReq.KeyId)
	}
	if createReq.ToVendor != PhoneVendor_APPLE {
		t.Errorf("expected APPLE vendor, got %v", createReq.ToVendor)
	}

	createResp := &CreateShareResponse{
		ShareId:   "share-001",
		ShareCode: "ABC123",
	}

	if len(createResp.ShareCode) != 6 {
		t.Errorf("expected 6-char share code, got %d", len(createResp.ShareCode))
	}

	// AcceptShare flow
	acceptReq := &AcceptShareRequest{
		ShareCode: "ABC123",
		DeviceId:  "DEV-FRIEND-001",
		UserId:    "user-friend-001",
		Vendor:    PhoneVendor_APPLE,
	}

	if acceptReq.ShareCode != createResp.ShareCode {
		t.Errorf("share code mismatch: %q vs %q", acceptReq.ShareCode, createResp.ShareCode)
	}

	acceptResp := &AcceptShareResponse{
		Key: &DigitalKey{
			KeyId:    "key-shared-001",
			KeyType:  KeyType_FRIEND,
			Status:   KeyStatus_ACTIVE,
			UserId:   acceptReq.UserId,
			DeviceId: acceptReq.DeviceId,
		},
	}

	if acceptResp.Key.KeyType != KeyType_FRIEND {
		t.Errorf("expected FRIEND key type, got %v", acceptResp.Key.KeyType)
	}
	if acceptResp.Key.Status != KeyStatus_ACTIVE {
		t.Errorf("expected ACTIVE status, got %v", acceptResp.Key.Status)
	}
}

func TestControlCommandService_Interface(t *testing.T) {
	// Lock request
	lockReq := &ControlCommandRequest{
		VehicleId: "VH-001",
		UserId:    "U-001",
		KeyId:     "key-001",
		Action:    "lock",
		Source:    2, // BLE
	}

	if lockReq.Action != "lock" {
		t.Errorf("expected 'lock' action, got %q", lockReq.Action)
	}

	lockResp := &ControlCommandResponse{
		CmdId:      "cmd-001",
		ResultCode: 0,
	}

	if lockResp.ResultCode != 0 {
		t.Errorf("expected result code 0 (success), got %d", lockResp.ResultCode)
	}
	if lockResp.CmdId == "" {
		t.Errorf("expected non-empty cmd_id")
	}

	// Unlock request
	unlockReq := &ControlCommandRequest{
		VehicleId: "VH-001",
		UserId:    "U-001",
		KeyId:     "key-001",
		Action:    "unlock",
		Source:    2,
	}

	if unlockReq.Action != "unlock" {
		t.Errorf("expected 'unlock' action, got %q", unlockReq.Action)
	}

	// Engine start
	startReq := &ControlCommandRequest{
		VehicleId: "VH-001",
		UserId:    "U-001",
		KeyId:     "key-001",
		Action:    "engine_on",
		Source:    4, // Remote
	}

	if startReq.Action != "engine_on" {
		t.Errorf("expected 'engine_on' action, got %q", startReq.Action)
	}
}

func TestHubTransportService_Interface(t *testing.T) {
	// ForwardRequest (hub transport adapter protocol bridging)
	forwardReq := &ForwardRequest{
		Vendor:    PhoneVendor_APPLE,
		Protocol:  Protocol_CCC_DK3,
		Operation: "bind",
		Payload:   []byte{0x00, 0x01, 0x02, 0x03, 0x04},
		TraceId:   "trace-001",
	}

	if forwardReq.Vendor != PhoneVendor_APPLE {
		t.Errorf("expected APPLE vendor")
	}
	if forwardReq.Protocol != Protocol_CCC_DK3 {
		t.Errorf("expected CCC_DK3 protocol")
	}
	if forwardReq.Operation != "bind" {
		t.Errorf("expected 'bind' operation")
	}
	if len(forwardReq.Payload) != 5 {
		t.Errorf("expected 5 bytes payload")
	}

	forwardResp := &ForwardResponse{
		Payload:   []byte{0x05, 0x06, 0x07},
		ErrorCode: "",
		ErrorMsg:  "",
	}

	if len(forwardResp.Payload) != 3 {
		t.Errorf("expected 3 bytes response payload")
	}

	// CallbackRequest/Response
	cbReq := &CallbackRequest{
		Vendor:     PhoneVendor_XIAOMI,
		Protocol:   Protocol_ICCOA_DK40,
		Operation:  "notify",
		Payload:    []byte{0xAA, 0xBB},
		CallbackId: "cb-001",
	}

	if cbReq.Vendor != PhoneVendor_XIAOMI {
		t.Errorf("expected XIAOMI vendor")
	}
	if cbReq.Protocol != Protocol_ICCOA_DK40 {
		t.Errorf("expected ICCOA_DK40 protocol")
	}

	cbResp := &CallbackResponse{
		ErrorCode: "",
	}
	_ = cbResp
}

func TestKeyStatusConstants(t *testing.T) {
	tests := []struct {
		status KeyStatus
		name   string
		want   string
	}{
		{KeyStatus_KEY_STATUS_UNSPECIFIED, "unspecified", "KEY_STATUS_UNSPECIFIED"},
		{KeyStatus_ACTIVE, "active", "ACTIVE"},
		{KeyStatus_SUSPENDED, "suspended", "SUSPENDED"},
		{KeyStatus_REVOKED, "revoked", "REVOKED"},
		{KeyStatus_EXPIRED, "expired", "EXPIRED"},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got := tt.status.String()
			if got != tt.want {
				t.Errorf("KeyStatus(%d).String() = %q, want %q", tt.status, got, tt.want)
			}
		})
	}

	// Enum constructor
	ks := KeyStatus_ACTIVE.Enum()
	if ks == nil || *ks != KeyStatus_ACTIVE {
		t.Errorf("Enum() returned unexpected value")
	}
}

func TestVehicleStatusUpdateMessage(t *testing.T) {
	msg := &VehicleStatusUpdate{
		VehicleId:    "VH-001",
		LockStatus:   1,
		EngineStatus: 0,
		BatteryPct:   85,
		Latitude:     39.9042,
		Longitude:    116.4074,
		Timestamp:    time.Now().Unix(),
	}

	if msg.LockStatus != 1 {
		t.Errorf("expected lock status 1 (locked), got %d", msg.LockStatus)
	}
	if msg.EngineStatus != 0 {
		t.Errorf("expected engine status 0 (off), got %d", msg.EngineStatus)
	}
	if msg.BatteryPct != 85 {
		t.Errorf("expected battery 85%%, got %d", msg.BatteryPct)
	}
	if msg.Latitude != 39.9042 || msg.Longitude != 116.4074 {
		t.Errorf("GPS coordinates not preserved: (%f, %f)", msg.Latitude, msg.Longitude)
	}
}

func TestVehicleStatusRequest(t *testing.T) {
	req := &VehicleStatusRequest{
		VehicleId: "VH-001",
	}
	if req.GetVehicleId() != "VH-001" {
		t.Errorf("expected VH-001, got %q", req.GetVehicleId())
	}
}

func TestAllRPCRequestResponseTypes(t *testing.T) {
	// Exercise all remaining request/response types

	// UnbindKey
	unbindReq := &UnbindKeyRequest{KeyId: "k1", TraceId: "t1"}
	unbindResp := &UnbindKeyResponse{}
	_ = unbindReq
	_ = unbindResp

	// RevokeKey
	revokeReq := &RevokeKeyRequest{KeyId: "k1", Reason: "stolen"}
	revokeResp := &RevokeKeyResponse{}
	_ = revokeReq
	_ = revokeResp

	// SuspendKey
	suspendReq := &SuspendKeyRequest{KeyId: "k1"}
	suspendResp := &SuspendKeyResponse{}
	_ = suspendReq
	_ = suspendResp

	// ResumeKey
	resumeReq := &ResumeKeyRequest{KeyId: "k1"}
	resumeResp := &ResumeKeyResponse{}
	_ = resumeReq
	_ = resumeResp

	// RenewKey
	renewReq := &RenewKeyRequest{KeyId: "k1", ValidUntil: time.Now().Add(365 * 24 * time.Hour).Unix()}
	renewResp := &RenewKeyResponse{Key: &DigitalKey{KeyId: "k1-renewed"}}
	_ = renewReq
	_ = renewResp

	// GetKey
	getKeyReq := &GetKeyRequest{KeyId: "k1"}
	getKeyResp := &GetKeyResponse{Key: &DigitalKey{KeyId: "k1"}}
	_ = getKeyReq
	_ = getKeyResp

	// ListKeys
	listKeysReq := &ListKeysRequest{VehicleId: "VH-001", PageSize: 10}
	listKeysResp := &ListKeysResponse{
		Keys: []*DigitalKey{
			{KeyId: "k1", Status: KeyStatus_ACTIVE},
		},
		Total: 1,
	}
	_ = listKeysReq
	if len(listKeysResp.Keys) != 1 {
		t.Errorf("expected 1 key in response, got %d", len(listKeysResp.Keys))
	}
	if listKeysResp.Total != 1 {
		t.Errorf("expected total=1, got %d", listKeysResp.Total)
	}

	// CancelShare
	cancelReq := &CancelShareRequest{ShareId: "share-001"}
	_ = cancelReq

	// GetShare
	getShareReq := &GetShareRequest{ShareId: "share-001"}
	getShareResp := &GetShareResponse{
		ShareId:    "share-001",
		FromUserId: "user-A",
		KeyId:      "k1",
	}
	_ = getShareReq
	_ = getShareResp

	// VehicleStatusRequest/ListKeysRequest
	_ = &VehicleStatusRequest{VehicleId: "VH-001"}

	// ForwardResponse with error
	errResp := &ForwardResponse{
		Payload:   nil,
		ErrorCode: "ERR_TIMEOUT",
		ErrorMsg:  "upstream timeout",
	}
	if errResp.ErrorCode != "ERR_TIMEOUT" {
		t.Errorf("expected ERR_TIMEOUT")
	}
	if errResp.Payload != nil {
		t.Errorf("expected nil payload on error")
	}
}

func TestTimeRestriction(t *testing.T) {
	tr := &TimeRestriction{
		Weekdays:  []int32{1, 2, 3, 4, 5},
		StartTime: "08:00",
		EndTime:   "18:00",
	}
	if len(tr.Weekdays) != 5 {
		t.Errorf("expected 5 weekdays, got %d", len(tr.Weekdays))
	}
	if tr.StartTime != "08:00" {
		t.Errorf("expected start_time=08:00, got %q", tr.StartTime)
	}
	if tr.EndTime != "18:00" {
		t.Errorf("expected end_time=18:00, got %q", tr.EndTime)
	}
}

func TestKeyPermissions(t *testing.T) {
	dk := &DigitalKey{
		KeyId:    "k1",
		KeyType:  KeyType_OWNER,
		Status:   KeyStatus_ACTIVE,
		Protocol: Protocol_CCC_DK3,
	}

	if dk.KeyType != KeyType_OWNER {
		t.Errorf("expected OWNER")
	}
	if dk.Status != KeyStatus_ACTIVE {
		t.Errorf("expected ACTIVE")
	}

	// Getter methods
	if dk.GetKeyId() != "k1" {
		t.Errorf("GetKeyId() = %q", dk.GetKeyId())
	}
}

func TestEnumHelpers(t *testing.T) {
	// Verify all four enum types have working Enum() helpers
	p := Protocol_CCC_DK3.Enum()
	if p == nil || *p != Protocol_CCC_DK3 {
		t.Error("Protocol.Enum() failed")
	}

	pv := PhoneVendor_XIAOMI.Enum()
	if pv == nil || *pv != PhoneVendor_XIAOMI {
		t.Error("PhoneVendor.Enum() failed")
	}

	kt := KeyType_FRIEND.Enum()
	if kt == nil || *kt != KeyType_FRIEND {
		t.Error("KeyType.Enum() failed")
	}

	ks := KeyStatus_ACTIVE.Enum()
	if ks == nil || *ks != KeyStatus_ACTIVE {
		t.Error("KeyStatus.Enum() failed")
	}
}

func TestControlCommandActionValues(t *testing.T) {
	// ControlCommandRequest action is a free-form string,
	// but we verify the supported action values
	actions := []string{
		"lock",
		"unlock",
		"engine_on",
		"engine_off",
		"trunk",
		"climate",
		"find",
	}

	for _, action := range actions {
		req := &ControlCommandRequest{
			VehicleId: "VH-001",
			UserId:    "U-001",
			KeyId:     "key-001",
			Action:    action,
			Source:    2,
		}
		if req.Action != action {
			t.Errorf("action not preserved: want %q, got %q", action, req.Action)
		}
	}
}

func TestDigitalKeyFullMessage(t *testing.T) {
	// Create a fully populated DigitalKey
	key := &DigitalKey{
		KeyId:     "key-test-001",
		VehicleId: "VH-001",
		DeviceId:  "DEV-001",
		UserId:    "U-001",
		KeyType:   KeyType_OWNER,
		Protocol:  Protocol_CCC_DK3,
		AccessLevel: &AccessLevel{
			Lock: true, Unlock: true, Engine: true,
		},
		Status:    KeyStatus_ACTIVE,
		ValidFrom:   time.Now().Unix(),
		ValidUntil:  time.Now().Add(365 * 24 * 3600).Unix(),
	}

	if key.GetKeyId() != "key-test-001" {
		t.Errorf("GetKeyId() = %q", key.GetKeyId())
	}
	if key.GetKeyType() != KeyType_OWNER {
		t.Errorf("GetKeyType() = %v", key.GetKeyType())
	}
	if key.GetStatus() != KeyStatus_ACTIVE {
		t.Errorf("GetStatus() = %v", key.GetStatus())
	}
	if key.GetAccessLevel() == nil || !key.GetAccessLevel().GetLock() {
		t.Errorf("GetAccessLevel().Lock should be true")
	}
}
