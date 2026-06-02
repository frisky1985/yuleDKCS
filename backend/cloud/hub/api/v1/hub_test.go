package v1

import (
	"testing"
	"time"
)

// ---------- Protocol enum ----------

func TestProtocolString(t *testing.T) {
	tests := []struct {
		p    Protocol
		want string
	}{
		{Protocol_PROTOCOL_UNSPECIFIED, "PROTOCOL_UNSPECIFIED"},
		{Protocol_CCC_DK3, "CCC_DK3"},
		{Protocol_ICCOA_DK30, "ICCOA_DK30"},
		{Protocol_ICCOA_DK40, "ICCOA_DK40"},
		{Protocol_ICCE, "ICCE"},
	}
	for _, tt := range tests {
		got := tt.p.String()
		if got != tt.want {
			t.Errorf("Protocol(%d).String() = %q, want %q", tt.p, got, tt.want)
		}
	}
}

func TestProtocolValue(t *testing.T) {
	if v := Protocol_value["CCC_DK3"]; v != 1 {
		t.Errorf("CCC_DK3 value = %d, want 1", v)
	}
	if v := Protocol_value["ICCOA_DK40"]; v != 3 {
		t.Errorf("ICCOA_DK40 value = %d, want 3", v)
	}
	if v := Protocol_value["ICCE"]; v != 4 {
		t.Errorf("ICCE value = %d, want 4", v)
	}
}

func TestProtocolName(t *testing.T) {
	if n := Protocol_name[1]; n != "CCC_DK3" {
		t.Errorf("Protocol_name[1] = %q, want CCC_DK3", n)
	}
	if n := Protocol_name[3]; n != "ICCOA_DK40" {
		t.Errorf("Protocol_name[3] = %q, want ICCOA_DK40", n)
	}
}

// ---------- PhoneVendor enum ----------

func TestPhoneVendorString(t *testing.T) {
	tests := []struct {
		v    PhoneVendor
		want string
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
		got := tt.v.String()
		if got != tt.want {
			t.Errorf("PhoneVendor(%d).String() = %q, want %q", tt.v, got, tt.want)
		}
	}
}

func TestPhoneVendorValue(t *testing.T) {
	if v := PhoneVendor_value["APPLE"]; v != 1 {
		t.Errorf("APPLE value = %d, want 1", v)
	}
	if v := PhoneVendor_value["XIAOMI"]; v != 3 {
		t.Errorf("XIAOMI value = %d, want 3", v)
	}
	if v := PhoneVendor_value["HUAWEI"]; v != 6 {
		t.Errorf("HUAWEI value = %d, want 6", v)
	}
}

// ---------- KeyType enum ----------

func TestKeyTypeString(t *testing.T) {
	tests := []struct {
		kt   KeyType
		want string
	}{
		{KeyType_KEY_TYPE_UNSPECIFIED, "KEY_TYPE_UNSPECIFIED"},
		{KeyType_OWNER, "OWNER"},
		{KeyType_FRIEND, "FRIEND"},
		{KeyType_SERVICE, "SERVICE"},
		{KeyType_TEMPORARY, "TEMPORARY"},
	}
	for _, tt := range tests {
		got := tt.kt.String()
		if got != tt.want {
			t.Errorf("KeyType(%d).String() = %q, want %q", tt.kt, got, tt.want)
		}
	}
}

// ---------- KeyStatus enum ----------

func TestKeyStatusString(t *testing.T) {
	tests := []struct {
		ks   KeyStatus
		want string
	}{
		{KeyStatus_KEY_STATUS_UNSPECIFIED, "KEY_STATUS_UNSPECIFIED"},
		{KeyStatus_ACTIVE, "ACTIVE"},
		{KeyStatus_SUSPENDED, "SUSPENDED"},
		{KeyStatus_REVOKED, "REVOKED"},
		{KeyStatus_EXPIRED, "EXPIRED"},
	}
	for _, tt := range tests {
		got := tt.ks.String()
		if got != tt.want {
			t.Errorf("KeyStatus(%d).String() = %q, want %q", tt.ks, got, tt.want)
		}
	}
}

// ---------- AccessLevel message ----------

func TestAccessLevelCreate(t *testing.T) {
	al := &AccessLevel{
		Lock:    true,
		Unlock:  true,
		Engine:  true,
		Trunk:   true,
		Window:  true,
		Climate: true,
		Find:    true,
		Seat:    true,
	}
	if !al.Lock {
		t.Error("expected Lock=true")
	}
	if !al.Unlock {
		t.Error("expected Unlock=true")
	}
	if !al.Engine {
		t.Error("expected Engine=true")
	}
}

func TestAccessLevelPartial(t *testing.T) {
	al := &AccessLevel{
		Lock:   true,
		Unlock: false,
	}
	if !al.Lock {
		t.Error("expected Lock=true")
	}
	if al.Unlock {
		t.Error("expected Unlock=false")
	}
}

// ---------- DigitalKey message ----------

func TestDigitalKeyCreate(t *testing.T) {
	key := &DigitalKey{
		KeyId:     "key-test-1",
		VehicleId: "VH001",
		DeviceId:  "DEV001",
		UserId:    "U001",
		KeyType:   KeyType_OWNER,
		Protocol:  Protocol_CCC_DK3,
		AccessLevel: &AccessLevel{
			Lock: true, Unlock: true, Engine: true,
		},
		Status:     KeyStatus_ACTIVE,
		ValidFrom:  time.Now().Unix(),
		ValidUntil: time.Now().Add(365 * 24 * 60 * 60).Unix(),
		CreatedAt:  time.Now().Unix(),
	}
	if key.KeyType != KeyType_OWNER {
		t.Errorf("expected OWNER, got %v", key.KeyType)
	}
	if key.Status != KeyStatus_ACTIVE {
		t.Errorf("expected ACTIVE, got %v", key.Status)
	}
	if key.Protocol != Protocol_CCC_DK3 {
		t.Errorf("expected CCC_DK3, got %v", key.Protocol)
	}
}

// ---------- BindKeyRequest message ----------

func TestBindKeyRequestCreate(t *testing.T) {
	req := &BindKeyRequest{
		VehicleId:    "VH001",
		DeviceId:     "DEV001",
		UserId:       "U001",
		DevicePubkey: []byte{1, 2, 3, 4},
		KeyType:      KeyType_OWNER,
		ValidFrom:    1000,
		ValidUntil:   2000,
	}
	if req.UserId != "U001" {
		t.Errorf("expected U001, got %s", req.UserId)
	}
	if len(req.DevicePubkey) != 4 {
		t.Errorf("expected 4-byte pubkey, got %d", len(req.DevicePubkey))
	}
}

// ---------- BindKeyResponse message ----------

func TestBindKeyResponseCreate(t *testing.T) {
	resp := &BindKeyResponse{
		Key: &DigitalKey{
			KeyId: "key-001",
		},
		VehiclePubkey: []byte{10, 20, 30},
		SharedSecret:  []byte{40, 50, 60},
	}
	if resp.Key.KeyId != "key-001" {
		t.Errorf("expected key-001, got %s", resp.Key.KeyId)
	}
	if len(resp.VehiclePubkey) != 3 {
		t.Errorf("expected 3 bytes, got %d", len(resp.VehiclePubkey))
	}
}

// ---------- CreateShareRequest / CreateShareResponse ----------

func TestCreateShareRequestResponse(t *testing.T) {
	req := &CreateShareRequest{
		KeyId:      "key-001",
		FromUserId: "user-001",
		ToVendor:   PhoneVendor_APPLE,
	}
	if req.KeyId != "key-001" {
		t.Errorf("expected key-001, got %s", req.KeyId)
	}
	if req.FromUserId != "user-001" {
		t.Errorf("expected user-001, got %s", req.FromUserId)
	}

	resp := &CreateShareResponse{
		ShareId:   "share-001",
		ShareCode: "123456",
	}
	if resp.ShareCode != "123456" {
		t.Errorf("expected 123456, got %s", resp.ShareCode)
	}
}

// ---------- AcceptShareRequest / AcceptShareResponse ----------

func TestAcceptShareRequestResponse(t *testing.T) {
	req := &AcceptShareRequest{
		ShareCode: "123456",
		DeviceId:  "dev-002",
		UserId:    "user-002",
		Vendor:    PhoneVendor_APPLE,
	}
	if req.Vendor != PhoneVendor_APPLE {
		t.Errorf("expected APPLE, got %v", req.Vendor)
	}
	if req.ShareCode != "123456" {
		t.Errorf("expected 123456, got %s", req.ShareCode)
	}

	resp := &AcceptShareResponse{
		Key: &DigitalKey{
			KeyId:   "key-shared-001",
			KeyType: KeyType_FRIEND,
			Status:  KeyStatus_ACTIVE,
		},
	}
	if resp.Key.KeyType != KeyType_FRIEND {
		t.Errorf("expected FRIEND, got %v", resp.Key.KeyType)
	}
}

// ---------- AdapterStatus message ----------

func TestAdapterStatusCreate(t *testing.T) {
	status := &AdapterStatus{
		Vendor:      "xiaomi",
		Protocol:    "iccoa_dk40",
		Healthy:     true,
		LastCheckMs: 1000,
	}
	if !status.Healthy {
		t.Error("expected healthy")
	}
	if status.Vendor != "xiaomi" {
		t.Errorf("expected xiaomi, got %s", status.Vendor)
	}
}

// ---------- VehicleStatusUpdate message ----------

func TestVehicleStatusUpdateCreate(t *testing.T) {
	update := &VehicleStatusUpdate{
		VehicleId:    "VH001",
		LockStatus:   1,
		EngineStatus: 0,
		BatteryPct:   85,
		Latitude:     39.9042,
		Longitude:    116.4074,
		Timestamp:    time.Now().Unix(),
	}
	if update.LockStatus != 1 {
		t.Errorf("expected lock status 1 (locked), got %d", update.LockStatus)
	}
	if update.EngineStatus != 0 {
		t.Errorf("expected engine status 0 (off), got %d", update.EngineStatus)
	}
	if update.BatteryPct != 85 {
		t.Errorf("expected battery 85, got %d", update.BatteryPct)
	}
}

func TestRevokeKeyRequest(t *testing.T) {
	req := &RevokeKeyRequest{
		KeyId:  "key-001",
		Reason: "stolen",
	}
	if req.Reason != "stolen" {
		t.Errorf("expected stolen, got %s", req.Reason)
	}
}

func TestSuspendAndResumeKeyRequest(t *testing.T) {
	suspend := &SuspendKeyRequest{
		KeyId: "key-001",
	}
	if suspend.KeyId != "key-001" {
		t.Errorf("expected key-001, got %s", suspend.KeyId)
	}

	resume := &ResumeKeyRequest{
		KeyId: "key-001",
	}
	if resume.KeyId != "key-001" {
		t.Errorf("expected key-001, got %s", resume.KeyId)
	}
}

// ---------- UnbindKey/UnbindKeyResponse ----------

func TestUnbindKeyRequest(t *testing.T) {
	req := &UnbindKeyRequest{
		KeyId: "key-001",
	}
	if req.KeyId != "key-001" {
		t.Errorf("expected key-001, got %s", req.KeyId)
	}
}

func TestUnbindKeyResponse(t *testing.T) {
	resp := &UnbindKeyResponse{}
	_ = resp // struct exists, no fields needed
}

// ---------- Suspend/Resume responses ----------

func TestSuspendKeyResponse(t *testing.T) {
	resp := &SuspendKeyResponse{}
	_ = resp
}

func TestResumeKeyResponse(t *testing.T) {
	resp := &ResumeKeyResponse{}
	_ = resp
}

// ---------- Enum pointer helpers ----------

func TestProtocolEnum(t *testing.T) {
	p := Protocol_CCC_DK3.Enum()
	if p == nil {
		t.Fatal("expected non-nil pointer")
	}
	if *p != Protocol_CCC_DK3 {
		t.Errorf("expected CCC_DK3, got %v", *p)
	}
}

func TestPhoneVendorEnum(t *testing.T) {
	v := PhoneVendor_XIAOMI.Enum()
	if v == nil {
		t.Fatal("expected non-nil pointer")
	}
	if *v != PhoneVendor_XIAOMI {
		t.Errorf("expected XIAOMI, got %v", *v)
	}
}

func TestKeyTypeEnum(t *testing.T) {
	kt := KeyType_FRIEND.Enum()
	if kt == nil {
		t.Fatal("expected non-nil pointer")
	}
	if *kt != KeyType_FRIEND {
		t.Errorf("expected FRIEND, got %v", *kt)
	}
}

func TestKeyStatusEnum(t *testing.T) {
	ks := KeyStatus_ACTIVE.Enum()
	if ks == nil {
		t.Fatal("expected non-nil pointer")
	}
	if *ks != KeyStatus_ACTIVE {
		t.Errorf("expected ACTIVE, got %v", *ks)
	}
}
