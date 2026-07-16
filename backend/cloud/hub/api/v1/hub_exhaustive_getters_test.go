package v1

import (
	"testing"
)

// ─────────────────────────────────────────────────────────────
// Exhaustive Getter method tests for ALL message types
// Ensures every proto getter is exercised at least once.
// ─────────────────────────────────────────────────────────────

func TestExhaustiveGetters_BindKey(t *testing.T) {
	req := &BindKeyRequest{VehicleId: "v", DeviceId: "d", UserId: "u", Vendor: PhoneVendor_APPLE, Protocol: Protocol_ICCE, KeyType: KeyType_OWNER, TraceId: "t"}
	_ = req.GetVehicleId(); _ = req.GetDeviceId(); _ = req.GetUserId(); _ = req.GetVendor()
	_ = req.GetProtocol(); _ = req.GetKeyType(); _ = req.GetAccessLevel(); _ = req.GetDevicePubkey()
	_ = req.GetValidFrom(); _ = req.GetValidUntil(); _ = req.GetTraceId()

	resp := &BindKeyResponse{Key: &DigitalKey{KeyId: "k"}, VehiclePubkey: []byte{0x01}, SharedSecret: []byte{0x02}, ErrorCode: "ok", ErrorMsg: ""}
	_ = resp.GetKey(); _ = resp.GetVehiclePubkey(); _ = resp.GetSharedSecret(); _ = resp.GetErrorCode()
	_ = resp.GetErrorMsg()
}

func TestExhaustiveGetters_UnbindKey(t *testing.T) {
	req := &UnbindKeyRequest{KeyId: "k", TraceId: "t"}
	_ = req.GetKeyId(); _ = req.GetTraceId()

	resp := &UnbindKeyResponse{ErrorCode: "ok"}
	_ = resp.GetErrorCode()
}

func TestExhaustiveGetters_SuspendResume(t *testing.T) {
	sreq := &SuspendKeyRequest{KeyId: "k", Reason: "lost", TraceId: "t"}
	_ = sreq.GetKeyId(); _ = sreq.GetReason(); _ = sreq.GetTraceId()
	_ = (&SuspendKeyResponse{ErrorCode: "ok"}).GetErrorCode()

	rreq := &ResumeKeyRequest{KeyId: "k", TraceId: "t"}
	_ = rreq.GetKeyId(); _ = rreq.GetTraceId()
	_ = (&ResumeKeyResponse{ErrorCode: "ok"}).GetErrorCode()
}

func TestExhaustiveGetters_RevokeRenew(t *testing.T) {
	rreq := &RevokeKeyRequest{KeyId: "k", Reason: "lost", TraceId: "t"}
	_ = rreq.GetKeyId(); _ = rreq.GetReason(); _ = rreq.GetTraceId()
	_ = (&RevokeKeyResponse{ErrorCode: "ok"}).GetErrorCode()

	nreq := &RenewKeyRequest{KeyId: "k", ValidUntil: 1000, TraceId: "t"}
	_ = nreq.GetKeyId(); _ = nreq.GetValidUntil(); _ = nreq.GetTraceId()
	resp := &RenewKeyResponse{Key: &DigitalKey{KeyId: "k"}, ErrorCode: "ok"}
	_ = resp.GetKey(); _ = resp.GetErrorCode()
}

func TestExhaustiveGetters_GetListKey(t *testing.T) {
	greq := &GetKeyRequest{KeyId: "k"}
	_ = greq.GetKeyId()
	gresp := &GetKeyResponse{Key: &DigitalKey{KeyId: "k"}, ErrorCode: "ok"}
	_ = gresp.GetKey(); _ = gresp.GetErrorCode()

	lreq := &ListKeysRequest{VehicleId: "v", UserId: "u", Status: KeyStatus_ACTIVE, PageSize: 10, PageToken: "t"}
	_ = lreq.GetVehicleId(); _ = lreq.GetUserId(); _ = lreq.GetStatus(); _ = lreq.GetPageSize(); _ = lreq.GetPageToken()
	lresp := &ListKeysResponse{Keys: []*DigitalKey{{KeyId: "k"}}, NextToken: "n", Total: 1}
	_ = lresp.GetKeys(); _ = lresp.GetNextToken(); _ = lresp.GetTotal()
}

func TestExhaustiveGetters_ShareKey(t *testing.T) {
	creq := &CreateShareRequest{KeyId: "k", FromUserId: "fu", ToUserId: "tu", ToVendor: PhoneVendor_XIAOMI, AccessLevel: &AccessLevel{Lock: true}, ValidFrom: 100, ValidUntil: 200, MaxUses: 5, TraceId: "t"}
	_ = creq.GetKeyId(); _ = creq.GetFromUserId(); _ = creq.GetToUserId(); _ = creq.GetToVendor()
	_ = creq.GetAccessLevel(); _ = creq.GetValidFrom(); _ = creq.GetValidUntil(); _ = creq.GetMaxUses(); _ = creq.GetTraceId()
	cresp := &CreateShareResponse{ShareId: "s", ShareCode: "123456", ErrorCode: "ok"}
	_ = cresp.GetShareId(); _ = cresp.GetShareCode(); _ = cresp.GetErrorCode()

	areq := &AcceptShareRequest{ShareCode: "123456", DeviceId: "d", UserId: "u", Vendor: PhoneVendor_XIAOMI, DevicePubkey: []byte{0x01}, TraceId: "t"}
	_ = areq.GetShareCode(); _ = areq.GetDeviceId(); _ = areq.GetUserId(); _ = areq.GetVendor(); _ = areq.GetDevicePubkey(); _ = areq.GetTraceId()
	aresp := &AcceptShareResponse{Key: &DigitalKey{KeyId: "k"}, SharedSecret: []byte{0x02}, ErrorCode: "ok"}
	_ = aresp.GetKey(); _ = aresp.GetSharedSecret(); _ = aresp.GetErrorCode()

	creq2 := &CancelShareRequest{ShareId: "s", TraceId: "t"}
	_ = creq2.GetShareId(); _ = creq2.GetTraceId()
	_ = (&CancelShareResponse{ErrorCode: "ok"}).GetErrorCode()

	greq := &GetShareRequest{ShareId: "s"}
	_ = greq.GetShareId()
	gresp := &GetShareResponse{ShareId: "s", KeyId: "k", FromUserId: "fu", AccessLevel: &AccessLevel{Unlock: true}, ValidFrom: 100, ValidUntil: 200, ErrorCode: "ok"}
	_ = gresp.GetShareId(); _ = gresp.GetKeyId(); _ = gresp.GetFromUserId(); _ = gresp.GetAccessLevel()
	_ = gresp.GetValidFrom(); _ = gresp.GetValidUntil(); _ = gresp.GetErrorCode()
}

func TestExhaustiveGetters_Control(t *testing.T) {
	creq := &ControlCommandRequest{VehicleId: "v", UserId: "u", KeyId: "k", Action: "lock", Params: []byte("{}"), Source: 4, TraceId: "t"}
	_ = creq.GetVehicleId(); _ = creq.GetUserId(); _ = creq.GetKeyId(); _ = creq.GetAction()
	_ = creq.GetParams(); _ = creq.GetSource(); _ = creq.GetTraceId()
	cresp := &ControlCommandResponse{CmdId: "c", ResultCode: 0, ErrorMsg: ""}
	_ = cresp.GetCmdId(); _ = cresp.GetResultCode(); _ = cresp.GetErrorMsg()
}

func TestExhaustiveGetters_VehicleStatus(t *testing.T) {
	vreq := &VehicleStatusRequest{VehicleId: "v"}
	_ = vreq.GetVehicleId()
	vupd := &VehicleStatusUpdate{VehicleId: "v", LockStatus: 1, EngineStatus: 0, DoorStatus: 1, WindowStatus: 0, BatteryPct: 85, InteriorTemp: 22, AlarmStatus: 0, Latitude: 31.2, Longitude: 121.4, Timestamp: 1000}
	_ = vupd.GetVehicleId(); _ = vupd.GetLockStatus(); _ = vupd.GetEngineStatus(); _ = vupd.GetDoorStatus()
	_ = vupd.GetWindowStatus(); _ = vupd.GetBatteryPct(); _ = vupd.GetInteriorTemp(); _ = vupd.GetAlarmStatus()
	_ = vupd.GetLatitude(); _ = vupd.GetLongitude(); _ = vupd.GetTimestamp()
}

func TestExhaustiveGetters_ForwardCallback(t *testing.T) {
	freq := &ForwardRequest{Vendor: PhoneVendor_APPLE, Protocol: Protocol_ICCE, Operation: "bind", Payload: []byte{0x01}, TraceId: "t"}
	_ = freq.GetVendor(); _ = freq.GetProtocol(); _ = freq.GetOperation(); _ = freq.GetPayload(); _ = freq.GetTraceId()
	fresp := &ForwardResponse{Payload: []byte{0x02}, ErrorCode: "ok", ErrorMsg: ""}
	_ = fresp.GetPayload(); _ = fresp.GetErrorCode(); _ = fresp.GetErrorMsg()

	creq := &CallbackRequest{Vendor: PhoneVendor_SAMSUNG, Protocol: Protocol_CCC_DK3, Operation: "notify", Payload: []byte{0x03}, CallbackId: "cb"}
	_ = creq.GetVendor(); _ = creq.GetProtocol(); _ = creq.GetOperation(); _ = creq.GetPayload(); _ = creq.GetCallbackId()
	_ = (&CallbackResponse{ErrorCode: "ok"}).GetErrorCode()
}

func TestExhaustiveGetters_HealthCheck(t *testing.T) {
	_ = (&HealthCheckRequest{}).ProtoReflect()
	hresp := &HealthCheckResponse{Healthy: true, Adapters: []*AdapterStatus{{Vendor: "apple", Protocol: "CCC", Healthy: true, LastCheckMs: 100, ErrorMsg: ""}}}
	_ = hresp.GetHealthy(); _ = hresp.GetAdapters()
	if len(hresp.GetAdapters()) > 0 {
		a := hresp.GetAdapters()[0]
		_ = a.GetVendor(); _ = a.GetProtocol(); _ = a.GetHealthy(); _ = a.GetLastCheckMs(); _ = a.GetErrorMsg()
	}
}
