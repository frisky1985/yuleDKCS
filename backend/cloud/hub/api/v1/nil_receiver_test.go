package v1

import (
	"testing"
)

// ─────────────────────────────────────────────────────────────
// Nil receiver tests — exercises the nil guard clause in every
// generated Getter method, covering the default-return path.
// ─────────────────────────────────────────────────────────────

func TestNilBindKeyRequest(t *testing.T) {
	var req *BindKeyRequest
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

func TestNilBindKeyResponse(t *testing.T) {
	var resp *BindKeyResponse
	_ = resp.GetKey()
	_ = resp.GetVehiclePubkey()
	_ = resp.GetSharedSecret()
	_ = resp.GetErrorCode()
	_ = resp.GetErrorMsg()
}

func TestNilControlCommandRequest(t *testing.T) {
	var req *ControlCommandRequest
	_ = req.GetVehicleId()
	_ = req.GetUserId()
	_ = req.GetKeyId()
	_ = req.GetAction()
	_ = req.GetParams()
	_ = req.GetSource()
	_ = req.GetTraceId()
}

func TestNilControlCommandResponse(t *testing.T) {
	var resp *ControlCommandResponse
	_ = resp.GetCmdId()
	_ = resp.GetResultCode()
	_ = resp.GetErrorMsg()
}

func TestNilVehicleStatusUpdate(t *testing.T) {
	var s *VehicleStatusUpdate
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

func TestNilDigitalKey(t *testing.T) {
	var dk *DigitalKey
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

func TestNilAccessLevel(t *testing.T) {
	var al *AccessLevel
	_ = al.GetLock()
	_ = al.GetUnlock()
	_ = al.GetEngine()
	_ = al.GetTrunk()
	_ = al.GetWindow()
	_ = al.GetClimate()
	_ = al.GetFind()
	_ = al.GetSeat()
}

func TestNilTimeRestriction(t *testing.T) {
	var tr *TimeRestriction
	_ = tr.GetWeekdays()
	_ = tr.GetStartTime()
	_ = tr.GetEndTime()
}

func TestNilAdapterStatus(t *testing.T) {
	var as *AdapterStatus
	_ = as.GetVendor()
	_ = as.GetProtocol()
	_ = as.GetHealthy()
	_ = as.GetLastCheckMs()
	_ = as.GetErrorMsg()
}

func TestNilListKeysRequest(t *testing.T) {
	var req *ListKeysRequest
	_ = req.GetVehicleId()
	_ = req.GetUserId()
	_ = req.GetStatus()
	_ = req.GetPageSize()
	_ = req.GetPageToken()
}

func TestNilListKeysResponse(t *testing.T) {
	var resp *ListKeysResponse
	_ = resp.GetKeys()
	_ = resp.GetNextToken()
	_ = resp.GetTotal()
}

func TestNilCreateShareRequest(t *testing.T) {
	var req *CreateShareRequest
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

func TestNilShareResponses(t *testing.T) {
	var cs *CreateShareResponse
	_ = cs.GetShareId()
	_ = cs.GetShareCode()
	_ = cs.GetErrorCode()

	var areq *AcceptShareRequest
	_ = areq.GetShareCode()
	_ = areq.GetDeviceId()
	_ = areq.GetUserId()
	_ = areq.GetVendor()
	_ = areq.GetDevicePubkey()
	_ = areq.GetTraceId()

	var aresp *AcceptShareResponse
	_ = aresp.GetKey()
	_ = aresp.GetSharedSecret()
	_ = aresp.GetErrorCode()

	var creq *CancelShareRequest
	_ = creq.GetShareId()
	_ = creq.GetTraceId()
}

func TestNilGetShareResponse(t *testing.T) {
	var resp *GetShareResponse
	_ = resp.GetShareId()
	_ = resp.GetKeyId()
	_ = resp.GetFromUserId()
	_ = resp.GetAccessLevel()
	_ = resp.GetValidFrom()
	_ = resp.GetValidUntil()
	_ = resp.GetErrorCode()
}

func TestNilForwardCallback(t *testing.T) {
	var freq *ForwardRequest
	_ = freq.GetVendor()
	_ = freq.GetProtocol()
	_ = freq.GetOperation()
	_ = freq.GetPayload()
	_ = freq.GetTraceId()

	var fresp *ForwardResponse
	_ = fresp.GetPayload()
	_ = fresp.GetErrorCode()
	_ = fresp.GetErrorMsg()

	var creq *CallbackRequest
	_ = creq.GetVendor()
	_ = creq.GetProtocol()
	_ = creq.GetOperation()
	_ = creq.GetPayload()
	_ = creq.GetCallbackId()

	var cresp *CallbackResponse
	_ = cresp.GetErrorCode()
}

func TestNilHealthCheck(t *testing.T) {
	var hcr *HealthCheckResponse
	_ = hcr.GetHealthy()
	_ = hcr.GetAdapters()
}

func TestNilSuspendResumeRevokeRenew(t *testing.T) {
	var sreq *SuspendKeyRequest
	_ = sreq.GetKeyId()
	_ = sreq.GetReason()
	_ = sreq.GetTraceId()

	var rreq *RevokeKeyRequest
	_ = rreq.GetKeyId()
	_ = rreq.GetReason()
	_ = rreq.GetTraceId()

	var nreq *RenewKeyRequest
	_ = nreq.GetKeyId()
	_ = nreq.GetValidUntil()
	_ = nreq.GetTraceId()
}

func TestNilVehicleStatusRequest(t *testing.T) {
	var req *VehicleStatusRequest
	_ = req.GetVehicleId()
}
