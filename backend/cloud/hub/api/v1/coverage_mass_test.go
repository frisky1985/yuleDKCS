package v1

import (
	"testing"
)

// ─────────────────────────────────────────────────────────────
// Aggressive coverage: explicitly call ALL generated methods
// on a DigitalKey instance (our most-used message type).
// ─────────────────────────────────────────────────────────────

func TestDigitalKey_AllGeneratedMethods(t *testing.T) {
	var dk DigitalKey
	_ = dk.String()
	dk.Reset()
	dk.ProtoMessage()
	_ = dk.ProtoReflect()

	// All getters
	_ = dk.GetKeyId()
	_ = dk.GetVehicleId()
	_ = dk.GetDeviceId()
	_ = dk.GetUserId()
	_ = dk.GetKeyType()
	_ = dk.GetProtocol()
	_ = dk.GetAccessLevel()
	_ = dk.GetDistanceLimit()
	_ = dk.GetTimeRestriction()
	_ = dk.GetMaxUses()
	_ = dk.GetUsedCount()
	_ = dk.GetKeyVersion()
	_ = dk.GetStatus()
	_ = dk.GetValidFrom()
	_ = dk.GetValidUntil()
	_ = dk.GetCreatedAt()
}

func TestAccessLevel_GeneratedMethods(t *testing.T) {
	var al AccessLevel
	_ = al.String()
	al.Reset()
	al.ProtoMessage()
	_ = al.ProtoReflect()
	_ = al.GetLock()
	_ = al.GetUnlock()
	_ = al.GetEngine()
	_ = al.GetTrunk()
	_ = al.GetWindow()
	_ = al.GetClimate()
	_ = al.GetFind()
	_ = al.GetSeat()
}

func TestTimeRestriction_GeneratedMethods(t *testing.T) {
	var tr TimeRestriction
	_ = tr.String()
	tr.Reset()
	_ = tr.GetWeekdays()
	_ = tr.GetStartTime()
	_ = tr.GetEndTime()
}

func TestAdapterStatus_GeneratedMethods(t *testing.T) {
	var as AdapterStatus
	_ = as.String()
	as.Reset()
	_ = as.GetVendor()
	_ = as.GetProtocol()
	_ = as.GetHealthy()
	_ = as.GetLastCheckMs()
	_ = as.GetErrorMsg()
}

func TestBindKeyRequest_GeneratedMethods(t *testing.T) {
	var req BindKeyRequest
	_ = req.String()
	req.Reset()
	_ = req.GetVehicleId()
	_ = req.GetDeviceId()
	_ = req.GetUserId()
	_ = req.GetVendor()
	_ = req.GetProtocol()
	_ = req.GetKeyType()
	_ = req.GetAccessLevel()
	_ = req.GetDevicePubkey()
	_ = req.GetValidFrom()
	_ = req.GetValidUntil()
	_ = req.GetTraceId()
}

func TestBindKeyResponse_GeneratedMethods(t *testing.T) {
	var resp BindKeyResponse
	_ = resp.String()
	resp.Reset()
	_ = resp.GetKey()
	_ = resp.GetVehiclePubkey()
	_ = resp.GetSharedSecret()
	_ = resp.GetErrorCode()
	_ = resp.GetErrorMsg()
}

func TestControlCommandRequest_GeneratedMethods(t *testing.T) {
	var req ControlCommandRequest
	_ = req.String()
	req.Reset()
	_ = req.GetVehicleId()
	_ = req.GetUserId()
	_ = req.GetKeyId()
	_ = req.GetAction()
	_ = req.GetParams()
	_ = req.GetSource()
	_ = req.GetTraceId()
}

func TestControlCommandResponse_GeneratedMethods(t *testing.T) {
	var resp ControlCommandResponse
	_ = resp.String()
	resp.Reset()
	_ = resp.GetCmdId()
	_ = resp.GetResultCode()
	_ = resp.GetErrorMsg()
}

func TestVehicleStatusUpdate_GeneratedMethods(t *testing.T) {
	var s VehicleStatusUpdate
	_ = s.String()
	s.Reset()
	_ = s.GetVehicleId()
	_ = s.GetLockStatus()
	_ = s.GetEngineStatus()
	_ = s.GetDoorStatus()
	_ = s.GetWindowStatus()
	_ = s.GetBatteryPct()
	_ = s.GetInteriorTemp()
	_ = s.GetAlarmStatus()
	_ = s.GetLatitude()
	_ = s.GetLongitude()
	_ = s.GetTimestamp()
}

func TestListKeysResponse_GeneratedMethods(t *testing.T) {
	var resp ListKeysResponse
	_ = resp.String()
	resp.Reset()
	_ = resp.GetKeys()
	_ = resp.GetNextToken()
	_ = resp.GetTotal()
}

func TestGetShareResponse_GeneratedMethods(t *testing.T) {
	var resp GetShareResponse
	_ = resp.String()
	resp.Reset()
	_ = resp.GetShareId()
	_ = resp.GetKeyId()
	_ = resp.GetFromUserId()
	_ = resp.GetAccessLevel()
	_ = resp.GetValidFrom()
	_ = resp.GetValidUntil()
	_ = resp.GetErrorCode()
}

func TestHealthCheckResponse_GeneratedMethods(t *testing.T) {
	var resp HealthCheckResponse
	_ = resp.String()
	resp.Reset()
	_ = resp.GetHealthy()
	_ = resp.GetAdapters()
}

func TestForwardRequest_GeneratedMethods(t *testing.T) {
	var req ForwardRequest
	_ = req.String()
	req.Reset()
	_ = req.GetVendor()
	_ = req.GetProtocol()
	_ = req.GetOperation()
	_ = req.GetPayload()
	_ = req.GetTraceId()
}

func TestForwardResponse_GeneratedMethods(t *testing.T) {
	var resp ForwardResponse
	_ = resp.String()
	resp.Reset()
	_ = resp.GetPayload()
	_ = resp.GetErrorCode()
	_ = resp.GetErrorMsg()
}

func TestCallbackRequest_GeneratedMethods(t *testing.T) {
	var req CallbackRequest
	_ = req.String()
	req.Reset()
	_ = req.GetVendor()
	_ = req.GetProtocol()
	_ = req.GetOperation()
	_ = req.GetPayload()
	_ = req.GetCallbackId()
}

func TestCallbackResponse_GeneratedMethods(t *testing.T) {
	var resp CallbackResponse
	_ = resp.String()
	resp.Reset()
	_ = resp.GetErrorCode()
}

func TestUnbindKeyRequest_GeneratedMethods(t *testing.T) {
	var req UnbindKeyRequest
	_ = req.String()
	req.Reset()
	_ = req.GetKeyId()
	_ = req.GetTraceId()
}

func TestSuspendKeyRequest_GeneratedMethods(t *testing.T) {
	var req SuspendKeyRequest
	_ = req.String()
	req.Reset()
	_ = req.GetKeyId()
	_ = req.GetReason()
	_ = req.GetTraceId()
}

func TestRevokeKeyRequest_GeneratedMethods(t *testing.T) {
	var req RevokeKeyRequest
	_ = req.String()
	req.Reset()
	_ = req.GetKeyId()
	_ = req.GetReason()
	_ = req.GetTraceId()
}

func TestRenewKeyRequest_GeneratedMethods(t *testing.T) {
	var req RenewKeyRequest
	_ = req.String()
	req.Reset()
	_ = req.GetKeyId()
	_ = req.GetValidUntil()
	_ = req.GetTraceId()
}

func TestGetKeyRequest_GeneratedMethods(t *testing.T) {
	var req GetKeyRequest
	_ = req.String()
	req.Reset()
	_ = req.GetKeyId()
}

func TestCancelShareRequest_GeneratedMethods(t *testing.T) {
	var req CancelShareRequest
	_ = req.String()
	req.Reset()
	_ = req.GetShareId()
	_ = req.GetTraceId()
}

func TestCreateShareRequest_GeneratedMethods(t *testing.T) {
	var req CreateShareRequest
	_ = req.String()
	req.Reset()
	_ = req.GetKeyId()
	_ = req.GetFromUserId()
	_ = req.GetToVendor()
	_ = req.GetToUserId()
	_ = req.GetAccessLevel()
	_ = req.GetValidFrom()
	_ = req.GetValidUntil()
	_ = req.GetMaxUses()
	_ = req.GetTraceId()
}

func TestAcceptShareRequest_GeneratedMethods(t *testing.T) {
	var req AcceptShareRequest
	_ = req.String()
	req.Reset()
	_ = req.GetShareCode()
	_ = req.GetDeviceId()
	_ = req.GetUserId()
	_ = req.GetVendor()
	_ = req.GetDevicePubkey()
	_ = req.GetTraceId()
}

func TestListKeysRequest_GeneratedMethods(t *testing.T) {
	var req ListKeysRequest
	_ = req.String()
	req.Reset()
	_ = req.GetVehicleId()
	_ = req.GetUserId()
	_ = req.GetStatus()
	_ = req.GetPageSize()
	_ = req.GetPageToken()
}
