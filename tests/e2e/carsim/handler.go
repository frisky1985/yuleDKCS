package main

import (
	"fmt"
	"log"
	"strings"
	"sync"
	"time"

	"github.com/yuleDKCS/tests/e2e/proto"
)

// Handler manages protocol-specific logic for the car simulator.
type Handler struct {
	se050   *SE050Mock
	vehicle *VehicleState

	mu         sync.Mutex
	sharedKeys map[string][]byte // deviceID -> shared secret for ECDH simulation
}

// NewHandler creates a new protocol handler.
func NewHandler(se *SE050Mock, vs *VehicleState) *Handler {
	return &Handler{
		se050:      se,
		vehicle:    vs,
		sharedKeys: make(map[string][]byte),
	}
}

// Handle processes an incoming frame and returns a response frame.
func (h *Handler) Handle(frame *proto.Frame) (*proto.Frame, error) {
	switch frame.Header.MsgType {
	case proto.MsgTypePairingRequest:
		return h.handlePairing(frame)
	case proto.MsgTypeSCP03Init:
		return h.handleSCP03Init(frame)
	case proto.MsgTypeSCP03Auth:
		return h.handleSCP03Auth(frame)
	case proto.MsgTypeKeyWrite:
		return h.handleKeyWrite(frame)
	case proto.MsgTypeChallengeReq:
		return h.handleChallenge(frame)
	case proto.MsgTypeChallengeResp:
		return h.handleChallengeResp(frame)
	case proto.MsgTypeUnlockCmd:
		return h.handleUnlock(frame)
	case proto.MsgTypeLockCmd:
		return h.handleLock(frame)
	case proto.MsgTypeEngineCmd:
		return h.handleEngine(frame)
	case proto.MsgTypeTrunkCmd:
		return h.handleTrunk(frame)
	case proto.MsgTypeWindowCmd:
		return h.handleWindow(frame)
	case proto.MsgTypeClimateCmd:
		return h.handleClimate(frame)
	case proto.MsgTypeFindCmd:
		return h.handleFind(frame)
	case proto.MsgTypeSeatCmd:
		return h.handleSeat(frame)
	case proto.MsgTypeHornLightCmd:
		return h.handleHornLight(frame)
	case proto.MsgTypeStatusReq:
		return h.handleStatusReq(frame)
	case proto.MsgTypeNfcRead:
		return h.handleNFCRead(frame)
	case proto.MsgTypeNfcAuthResp:
		return h.handleNFCAuthResp(frame)
	case proto.MsgTypeReplayFrame:
		return h.handleReplayFrame(frame)
	case proto.MsgTypeRevokeNotify:
		return h.handleRevokeNotify(frame)
	// Protocol-specific flows
	case proto.MsgTypeICCEPair:
		return h.handleICCEPair(frame)
	case proto.MsgTypeICCEEdge:
		return h.handleICCEEdge(frame)
	case proto.MsgTypeCCC_OOB_Init:
		return h.handleCccOobInit(frame)
	case proto.MsgTypeCCC_OOB_Data:
		return h.handleCccOobData(frame)
	case proto.MsgTypeCCC_UWB_Rng:
		return h.handleCccUwbRanging(frame)
	case proto.MsgTypeICCOA_DK30, proto.MsgTypeICCOA_DK40:
		return h.handleIcCoaFrame(frame)
	case proto.MsgTypeICCOA_HMAC:
		return h.handleIcCoaHmac(frame)
	// Status sync flows
	case proto.MsgTypeSyncPoll:
		return h.handleSyncPoll(frame)
	case proto.MsgTypeSyncEvent:
		return h.handleSyncEvent(frame)
	case proto.MsgTypeSyncHeartbeat:
		return h.handleSyncHeartbeat(frame)
	case proto.MsgTypeSyncOffline:
		return h.handleSyncOffline(frame)
	case proto.MsgTypeSyncConflict:
		return h.handleSyncConflict(frame)
	case proto.MsgTypeSyncBatch:
		return h.handleSyncBatch(frame)
	default:
		return h.makeError(frame, 100, fmt.Sprintf("unknown msg type: %d", frame.Header.MsgType)), nil
	}
}

func (h *Handler) makeError(req *proto.Frame, code uint32, msg string) *proto.Frame {
	return proto.NewFrame(proto.MsgTypeError, encodePayload(&proto.ErrorPayload{Code: code, Message: msg}))
}

// ── Pairing ──────────────────────────────────────────────────

func (h *Handler) handlePairing(frame *proto.Frame) (*proto.Frame, error) {
	var req proto.PairingPayload
	if err := decodePayload(frame.Payload, &req); err != nil {
		return h.makeError(frame, 101, "decode pairing request: "+err.Error()), nil
	}

	h.mu.Lock()
	h.vehicle.BondedDevices[req.DeviceID] = true

	// ECDH shared secret simulation
	sharedKey := GetSessionKeyECDH(req.PublicKey)
	h.sharedKeys[req.DeviceID] = sharedKey
	h.mu.Unlock()

	// Generate pairing response with SE050 public key
	respChallenge := GenerateChallenge()
	resp := &proto.PairingPayload{
		DeviceID:   "SIM_CAR_001",
		Vendor:     0,
		Protocol:   req.Protocol,
		PublicKey:  h.se050.GetPublicKey(),
		Challenge:  respChallenge,
		SessionKey: sharedKey,
	}

	log.Printf("[PAIRING] Device %s paired via protocol %d, shared key established", req.DeviceID, req.Protocol)
	return proto.NewFrame(proto.MsgTypePairingResponse, encodePayload(resp)), nil
}

// ── SCP03 ────────────────────────────────────────────────────

func (h *Handler) handleSCP03Init(frame *proto.Frame) (*proto.Frame, error) {
	var req proto.SCP03Payload
	if err := decodePayload(frame.Payload, &req); err != nil {
		return h.makeError(frame, 102, "decode SCP03 init: "+err.Error()), nil
	}

	cardChallenge, err := h.se050.StartSCP03(req.HostChallenge)
	if err != nil {
		return h.makeError(frame, 103, "SCP03 init: "+err.Error()), nil
	}

	resp := &proto.SCP03Payload{
		Step:          1,
		HostChallenge: req.HostChallenge,
		CardChallenge: cardChallenge,
		Success:       true,
	}

	log.Printf("[SCP03] Init complete, host_challenge=%x, card_challenge=%x", req.HostChallenge, cardChallenge)
	return proto.NewFrame(proto.MsgTypeSCP03Secure, encodePayload(resp)), nil
}

func (h *Handler) handleSCP03Auth(frame *proto.Frame) (*proto.Frame, error) {
	var req proto.SCP03Payload
	if err := decodePayload(frame.Payload, &req); err != nil {
		return h.makeError(frame, 102, "decode SCP03 auth: "+err.Error()), nil
	}

	ok, err := h.se050.AuthenticateSCP03(req.Mac)
	if err != nil || !ok {
		log.Printf("[SCP03] Auth FAILED: err=%v, ok=%v", err, ok)
		resp := &proto.SCP03Payload{Step: 2, Success: false}
		return proto.NewFrame(proto.MsgTypeSCP03Secure, encodePayload(resp)), nil
	}

	log.Printf("[SCP03] Auth SUCCESS, secure channel established")
	resp := &proto.SCP03Payload{Step: 2, Success: true}
	return proto.NewFrame(proto.MsgTypeSCP03Secure, encodePayload(resp)), nil
}

// ── Key Write ────────────────────────────────────────────────

func (h *Handler) handleKeyWrite(frame *proto.Frame) (*proto.Frame, error) {
	var req proto.KeyPayload
	if err := decodePayload(frame.Payload, &req); err != nil {
		return h.makeError(frame, 104, "decode key write: "+err.Error()), nil
	}

	// Key writes succeed if BLE pairing was done on this connection
	log.Printf("[KEY_WRITE] Received key write request: keyID=%s, type=%d, perms=%d",
		req.KeyID, req.KeyType, req.Permissions)

	// Store encrypted key material in SE050 mock
	if err := h.se050.StoreKey(req.KeyID, req.KeyData); err != nil {
		return h.makeError(frame, 106, "SE050 store key: "+err.Error()), nil
	}

	now := time.Now().UnixMilli()
	storedKey := &StoredKey{
		KeyID:       req.KeyID,
		KeyType:     req.KeyType,
		DeviceID:    extractDeviceIDFromKey(req.KeyID),
		UserID:      extractUserIDFromKey(req.KeyID),
		Permissions: req.Permissions,
		KeyData:     req.KeyData,
		ValidFrom:   now,
		ValidUntil:  now + 365*24*3600*1000, // 1 year default
		Status:      1, // active
		CreatedAt:   now,
	}
	if req.ValidFrom > 0 {
		storedKey.ValidFrom = req.ValidFrom
	}
	if req.ValidUntil > 0 {
		storedKey.ValidUntil = req.ValidUntil
	}

	h.vehicle.AddKey(storedKey)
	h.vehicle.AddEvent(&VehicleEvent{
		EventID:   fmt.Sprintf("evt_keybind_%s_%d", req.KeyID, now),
		EventType: "key_bind",
		KeyID:     req.KeyID,
		UserID:    storedKey.UserID,
		Source:    2, // BLE
		Success:   true,
		Timestamp: now,
		Detail:    fmt.Sprintf("key_type=%d permissions=%d", req.KeyType, req.Permissions),
	})

	log.Printf("[KEY_WRITE] Key %s (type=%d) bound successfully, permissions=%d", req.KeyID, req.KeyType, req.Permissions)

	ack := &proto.KeyPayload{KeyID: req.KeyID, Status: 1, KeyData: h.se050.GetPublicKey()}
	return proto.NewFrame(proto.MsgTypeKeyWriteAck, encodePayload(ack)), nil
}

// ── Challenge/Response (Passive Entry) ───────────────────────

func (h *Handler) handleChallenge(frame *proto.Frame) (*proto.Frame, error) {
	challenge := GenerateChallenge()
	log.Printf("[CHALLENGE] Sent challenge: %x", challenge)

	resp := &proto.AuthPayload{
		Challenge: challenge,
		Counter:   h.vehicle.SeqCounter,
		Timestamp: time.Now().UnixMilli(),
	}
	return proto.NewFrame(proto.MsgTypeChallengeResp, encodePayload(resp)), nil
}

func (h *Handler) handleChallengeResp(frame *proto.Frame) (*proto.Frame, error) {
	var req proto.AuthPayload
	if err := decodePayload(frame.Payload, &req); err != nil {
		return h.makeError(frame, 107, "decode challenge resp: "+err.Error()), nil
	}

	// Verify key exists and is active
	key := h.vehicle.GetKey(req.KeyID)
	if key == nil {
		log.Printf("[CHALLENGE_RESP] Key %s not found on vehicle", req.KeyID)
		return h.makeError(frame, 108, "key not found"), nil
	}

	// Check revocation
	if h.vehicle.IsKeyRevoked(req.KeyID) {
		log.Printf("[CHALLENGE_RESP] Key %s is REVOKED, access denied", req.KeyID)
		h.vehicle.TriggerAlarm()
		h.vehicle.AddEvent(&VehicleEvent{
			EventID:   fmt.Sprintf("evt_denied_%s_%d", req.KeyID, time.Now().UnixMilli()),
			EventType: "auth_fail",
			KeyID:     req.KeyID,
			Success:   false,
			Timestamp: time.Now().UnixMilli(),
			Detail:    "key_revoked",
		})
		return h.makeError(frame, 109, "key revoked: access denied"), nil
	}

	// Check if key is active
	if !h.vehicle.IsKeyActive(req.KeyID) {
		return h.makeError(frame, 110, "key not active (expired or invalid)"), nil
	}

	// Verify signature (simplified: accept any valid-length payload)
	if len(req.Signature) == 0 {
		log.Printf("[CHALLENGE_RESP] Missing signature (sig len=%d)", len(req.Signature))
		return h.makeError(frame, 111, "missing signature"), nil
	}

	log.Printf("[CHALLENGE_RESP] Key %s authenticated successfully (%d bytes sig)", req.KeyID, len(req.Signature))
	// Return success ACK so client can proceed with unlock command
	return proto.NewFrame(proto.MsgTypeChallengeResp, encodePayload(&proto.AuthPayload{
		KeyID:     req.KeyID,
		Signature: req.Signature,
	})), nil
}

// ── Commands: Unlock, Lock, Engine, Trunk ────────────────────

func (h *Handler) checkAuth(req *proto.Frame, keyID string) bool {
	// Check if device has an active secure channel or key on vehicle
	if keyID == "" {
		return false
	}
	// For simulation: any key that exists on the vehicle is OK for auth
	key := h.vehicle.GetKey(keyID)
	if key == nil {
		return false
	}
	if h.vehicle.IsKeyRevoked(keyID) {
		return false
	}
	return h.vehicle.IsKeyActive(keyID)
}

func (h *Handler) executeAction(action string, keyID string, source uint32) (uint32, string) {
	key := h.vehicle.GetKey(keyID)
	if key == nil {
		return 1, "key not found"
	}

	now := time.Now().UnixMilli()
	eventType := ""

	switch strings.ToLower(action) {
	case "unlock":
		h.vehicle.Unlock()
		eventType = "unlock"
		log.Printf("[ACTION] ✅ Unlock via key %s", keyID)
	case "lock":
		h.vehicle.Lock()
		eventType = "lock"
		log.Printf("[ACTION] 🔒 Lock via key %s", keyID)
	case "engine_on":
		h.vehicle.mu.Lock()
		h.vehicle.EngineStatus = 1
		h.vehicle.mu.Unlock()
		eventType = "engine"
		log.Printf("[ACTION] 🚗 Engine ON via key %s", keyID)
	case "engine_off":
		h.vehicle.mu.Lock()
		h.vehicle.EngineStatus = 0
		h.vehicle.mu.Unlock()
		eventType = "engine"
		log.Printf("[ACTION] 🚗 Engine OFF via key %s", keyID)
	case "trunk":
		h.vehicle.mu.Lock()
		h.vehicle.TrunkStatus = 1
		h.vehicle.mu.Unlock()
		eventType = "action"
		log.Printf("[ACTION] 🚪 Trunk open via key %s", keyID)
	default:
		return 1, "unknown action: " + action
	}

	h.vehicle.AddEvent(&VehicleEvent{
		EventID:   fmt.Sprintf("evt_%s_%s_%d", eventType, keyID, now),
		EventType: eventType,
		KeyID:     keyID,
		UserID:    key.UserID,
		Source:    source,
		Success:   true,
		Timestamp: now,
		Detail:    fmt.Sprintf("action=%s source=%d", action, source),
	})

	return 0, ""
}

func (h *Handler) handleUnlock(frame *proto.Frame) (*proto.Frame, error) {
	var req proto.CommandPayload
	if err := decodePayload(frame.Payload, &req); err != nil {
		return h.makeError(frame, 112, "decode unlock: "+err.Error()), nil
	}

	// Check replay
	deviceID := extractDeviceIDFromKey(req.KeyID)
	if !h.vehicle.CheckAndIncrementSeq(deviceID, frame.Header.SeqNum) {
		log.Printf("[REPLAY] 🚨 Replay detected for key %s, seq %d", req.KeyID, frame.Header.SeqNum)
		h.vehicle.TriggerAlarm()
		return h.makeError(frame, 113, "replay attack detected"), nil
	}

	if !h.checkAuth(frame, req.KeyID) {
		return h.makeError(frame, 114, "authentication failed"), nil
	}

	resultCode, errMsg := h.executeAction("unlock", req.KeyID, 2)
	resp := &proto.CommandPayload{
		Action:   "unlock",
		KeyID:    req.KeyID,
		Result:   resultCode,
		ErrorMsg: errMsg,
	}
	return proto.NewFrame(proto.MsgTypeUnlockCmd, encodePayload(resp)), nil
}

func (h *Handler) handleLock(frame *proto.Frame) (*proto.Frame, error) {
	var req proto.CommandPayload
	if err := decodePayload(frame.Payload, &req); err != nil {
		return h.makeError(frame, 112, "decode lock: "+err.Error()), nil
	}

	deviceID := extractDeviceIDFromKey(req.KeyID)
	if !h.vehicle.CheckAndIncrementSeq(deviceID, frame.Header.SeqNum) {
		log.Printf("[REPLAY] 🚨 Replay detected for lock key %s", req.KeyID)
		h.vehicle.TriggerAlarm()
		return h.makeError(frame, 113, "replay attack detected"), nil
	}

	if !h.checkAuth(frame, req.KeyID) {
		return h.makeError(frame, 114, "authentication failed"), nil
	}

	resultCode, errMsg := h.executeAction("lock", req.KeyID, 2)
	resp := &proto.CommandPayload{
		Action:   "lock",
		KeyID:    req.KeyID,
		Result:   resultCode,
		ErrorMsg: errMsg,
	}
	return proto.NewFrame(proto.MsgTypeLockCmd, encodePayload(resp)), nil
}

func (h *Handler) handleEngine(frame *proto.Frame) (*proto.Frame, error) {
	var req proto.CommandPayload
	if err := decodePayload(frame.Payload, &req); err != nil {
		return h.makeError(frame, 112, "decode engine: "+err.Error()), nil
	}

	if !h.checkAuth(frame, req.KeyID) {
		return h.makeError(frame, 114, "authentication failed"), nil
	}

	action := "engine_on"
	if req.Action == "engine_off" {
		action = "engine_off"
	}
	resultCode, errMsg := h.executeAction(action, req.KeyID, 4)
	resp := &proto.CommandPayload{
		Action:   action,
		KeyID:    req.KeyID,
		Result:   resultCode,
		ErrorMsg: errMsg,
	}
	return proto.NewFrame(proto.MsgTypeEngineCmd, encodePayload(resp)), nil
}

func (h *Handler) handleTrunk(frame *proto.Frame) (*proto.Frame, error) {
	var req proto.CommandPayload
	if err := decodePayload(frame.Payload, &req); err != nil {
		return h.makeError(frame, 112, "decode trunk: "+err.Error()), nil
	}

	if !h.checkAuth(frame, req.KeyID) {
		return h.makeError(frame, 114, "authentication failed"), nil
	}

	resultCode, errMsg := h.executeAction("trunk", req.KeyID, 4)
	resp := &proto.CommandPayload{
		Action:   "trunk",
		KeyID:    req.KeyID,
		Result:   resultCode,
		ErrorMsg: errMsg,
	}
	return proto.NewFrame(proto.MsgTypeTrunkCmd, encodePayload(resp)), nil
}

// ── Status ───────────────────────────────────────────────────

func (h *Handler) handleStatusReq(frame *proto.Frame) (*proto.Frame, error) {
	status := h.vehicle.GetStatusSnapshot()
	resp := &proto.StatusPayload{
		LockStatus:   status.LockStatus,
		EngineStatus: status.EngineStatus,
		Rssi:         status.Rssi,
		AlarmStatus:  status.AlarmStatus,
		Distance:     status.Distance,
		BatteryPct:   status.BatteryPct,
		Latitude:     status.Latitude,
		Longitude:    status.Longitude,
	}
	return proto.NewFrame(proto.MsgTypeStatusResp, encodePayload(resp)), nil
}

// ── NFC ──────────────────────────────────────────────────────

func (h *Handler) handleNFCRead(frame *proto.Frame) (*proto.Frame, error) {
	var req proto.NFCReadPayload
	if err := decodePayload(frame.Payload, &req); err != nil {
		return h.makeError(frame, 120, "decode NFC read: "+err.Error()), nil
	}

	// Simulate NFC tag read: vehicle sends credential data
	credentialData := GenerateChallenge()
	resp := &proto.NFCReadPayload{
		CredentialID: req.CredentialID,
		KeyID:        req.KeyID,
		Data:         credentialData,
		Metadata: map[string]string{
			"nfc_tag_type": "NTAG_424_DNA",
			"protocol":     "ICCE",
			"reader":       "NFC_Reader_01",
		},
	}

	log.Printf("[NFC] Read credential %s for key %s", req.CredentialID, req.KeyID)
	return proto.NewFrame(proto.MsgTypeNfcCredential, encodePayload(resp)), nil
}

func (h *Handler) handleNFCAuthResp(frame *proto.Frame) (*proto.Frame, error) {
	var req proto.NFCReadPayload
	if err := decodePayload(frame.Payload, &req); err != nil {
		return h.makeError(frame, 120, "decode NFC auth: "+err.Error()), nil
	}

	key := h.vehicle.GetKey(req.KeyID)
	if key == nil {
		return h.makeError(frame, 121, "NFC auth: key not found"), nil
	}

	if h.vehicle.IsKeyRevoked(req.KeyID) {
		log.Printf("[NFC] Key %s is REVOKED, NFC auth denied", req.KeyID)
		resp := &proto.NFCReadPayload{AuthSuccess: false}
		return proto.NewFrame(proto.MsgTypeNfcAuthResp, encodePayload(resp)), nil
	}

	// Verify auth data (simplified: accept any non-empty auth data)
	if len(req.AuthData) == 0 {
		resp := &proto.NFCReadPayload{AuthSuccess: false}
		return proto.NewFrame(proto.MsgTypeNfcAuthResp, encodePayload(resp)), nil
	}

	h.vehicle.Unlock()
	h.vehicle.AddEvent(&VehicleEvent{
		EventID:   fmt.Sprintf("evt_nfc_unlock_%s_%d", req.KeyID, time.Now().UnixMilli()),
		EventType: "unlock",
		KeyID:     req.KeyID,
		Source:    1, // NFC
		Success:   true,
		Timestamp: time.Now().UnixMilli(),
		Detail:    "nfc_auth_success",
	})

	log.Printf("[NFC] ✅ Unlock via NFC auth for key %s", req.KeyID)
	resp := &proto.NFCReadPayload{
		CredentialID: req.CredentialID,
		KeyID:        req.KeyID,
		AuthSuccess:  true,
	}
	return proto.NewFrame(proto.MsgTypeNfcAuthResp, encodePayload(resp)), nil
}

// ── Replay Detection ─────────────────────────────────────────

func (h *Handler) handleReplayFrame(frame *proto.Frame) (*proto.Frame, error) {
	var req proto.ReplayPayload
	if err := decodePayload(frame.Payload, &req); err != nil {
		return h.makeError(frame, 130, "decode replay: "+err.Error()), nil
	}

	// Check if the original SeqNum has already been seen (simulate replay detection)
	deviceID := fmt.Sprintf("replay_%d", req.OriginalSeq)
	if h.vehicle.CheckAndIncrementSeq(deviceID, req.ReplayedSeq) {
		// This specific logic: if seq <= last, it's a replay
		h.vehicle.mu.RLock()
		lastSeq := h.vehicle.LastSeqMap[deviceID]
		h.vehicle.mu.RUnlock()

		if req.ReplayedSeq <= lastSeq {
			h.vehicle.TriggerAlarm()
			h.vehicle.AddEvent(&VehicleEvent{
				EventID:   fmt.Sprintf("evt_replay_%d", time.Now().UnixMilli()),
				EventType: "replay_detect",
				Source:    2,
				Success:   false,
				Timestamp: time.Now().UnixMilli(),
				Detail:    fmt.Sprintf("replay: original_seq=%d replayed_seq=%d", req.OriginalSeq, req.ReplayedSeq),
			})

			log.Printf("[REPLAY_DETECT] 🚨 Replay attack blocked: seq=%d, original=%d", req.ReplayedSeq, req.OriginalSeq)
			resp := &proto.ReplayPayload{
				OriginalSeq: req.OriginalSeq,
				ReplayedSeq: req.ReplayedSeq,
				Blocked:     true,
				Reason:      "duplicate sequence number",
			}
			return proto.NewFrame(proto.MsgTypeReplayDetect, encodePayload(resp)), nil
		}
	}

	resp := &proto.ReplayPayload{
		OriginalSeq: req.OriginalSeq,
		ReplayedSeq: req.ReplayedSeq,
		Blocked:     false,
	}
	return proto.NewFrame(proto.MsgTypeReplayDetect, encodePayload(resp)), nil
}

// ── Revocation Notify ────────────────────────────────────────

func (h *Handler) handleRevokeNotify(frame *proto.Frame) (*proto.Frame, error) {
	var req proto.RevokeNotifyPayload
	if err := decodePayload(frame.Payload, &req); err != nil {
		return h.makeError(frame, 140, "decode revoke: "+err.Error()), nil
	}

	h.vehicle.RevokeKey(req.KeyID, req.Reason)
	log.Printf("[REVOKE] 🔑 Key %s revoked: %s", req.KeyID, req.Reason)

	h.vehicle.AddEvent(&VehicleEvent{
		EventID:   fmt.Sprintf("evt_revoke_%s_%d", req.KeyID, time.Now().UnixMilli()),
		EventType: "revoke",
		KeyID:     req.KeyID,
		Success:   true,
		Timestamp: time.Now().UnixMilli(),
		Detail:    req.Reason,
	})

	resp := &proto.RevokeNotifyPayload{
		KeyID:     req.KeyID,
		RevokedAt: time.Now().UnixMilli(),
		Reason:    req.Reason,
	}
	return proto.NewFrame(proto.MsgTypeRevokeNotify, encodePayload(resp)), nil
}

// ── Helpers ──────────────────────────────────────────────────

func extractDeviceIDFromKey(keyID string) string {
	parts := strings.Split(keyID, "_")
	if len(parts) >= 1 {
		return parts[0]
	}
	return "unknown"
}

func extractUserIDFromKey(keyID string) string {
	// Simplified: user ID derived from key ID
	return "user_" + strings.ReplaceAll(keyID, "-", "_")
}

// encodePayload delegates to proto.EncodePayload
func encodePayload(v interface{}) []byte {
	return proto.EncodePayload(v)
}

// decodePayload delegates to proto.DecodePayload
func decodePayload(data []byte, v interface{}) error {
	return proto.DecodePayload(data, v)
}

// ── ICCE Protocol Handlers ──────────────────────────────────

func (h *Handler) handleICCEPair(frame *proto.Frame) (*proto.Frame, error) {
	var req proto.ICCEPairPayload
	if err := decodePayload(frame.Payload, &req); err != nil {
		return h.makeError(frame, 301, "decode ICCE pair: "+err.Error()), nil
	}
	log.Printf("[ICCE] Pairing request: device=%s edge=%v", req.DeviceID, req.EdgeCapable)
	resp := &proto.ICCEPairPayload{
		DeviceID:  "SIM_CAR_001",
		Challenge: GenerateChallenge(),
	}
	return proto.NewFrame(proto.MsgTypeICCEPair, encodePayload(resp)), nil
}

func (h *Handler) handleICCEEdge(frame *proto.Frame) (*proto.Frame, error) {
	var req proto.ICCEEdgePayload
	if err := decodePayload(frame.Payload, &req); err != nil {
		return h.makeError(frame, 302, "decode ICCE edge: "+err.Error()), nil
	}
	distance := h.vehicle.GetDistance()
	result := uint32(1)
	if distance < 3000 {
		result = 0
	}
	log.Printf("[ICCE-EDGE] Zone=%d distance=%dmm action=%s result=%d", req.Zone, distance, req.Action, result)
	resp := &proto.ICCEEdgePayload{
		RuleID:   req.RuleID,
		Zone:     req.Zone,
		Event:    req.Event,
		Distance: distance,
		Action:   req.Action,
		Result:   result,
	}
	return proto.NewFrame(proto.MsgTypeICCEEdge, encodePayload(resp)), nil
}

// ── CCC Protocol Handlers ───────────────────────────────────

func (h *Handler) handleCccOobInit(frame *proto.Frame) (*proto.Frame, error) {
	var req proto.CccOobPayload
	if err := decodePayload(frame.Payload, &req); err != nil {
		return h.makeError(frame, 401, "decode CCC OOB init: "+err.Error()), nil
	}
	log.Printf("[CCC-OOB] Step %d: NFC OOB handshake", req.Step)
	resp := &proto.CccOobPayload{
		Step:         req.Step + 1,
		Certificate:  []byte("ccc_oob_cert_sim"),
		EphemeralKey: []byte("eph_key_sim_32bytes_________"),
	}
	if req.Step >= 2 {
		resp.Success = true
	}
	return proto.NewFrame(proto.MsgTypeCCC_OOB_Data, encodePayload(resp)), nil
}

func (h *Handler) handleCccOobData(frame *proto.Frame) (*proto.Frame, error) {
	var req proto.CccOobPayload
	if err := decodePayload(frame.Payload, &req); err != nil {
		return h.makeError(frame, 402, "decode CCC OOB data: "+err.Error()), nil
	}
	log.Printf("[CCC-OOB] NFC OOB exchange complete, success=%v", req.Success)
	resp := &proto.CccOobPayload{
		Step:         3,
		Success:      true,
		NfcData:      []byte("oob_exchange_done"),
		Signature:    []byte("sig_verified_ok"),
	}
	return proto.NewFrame(proto.MsgTypeCCC_OOB_Data, encodePayload(resp)), nil
}

func (h *Handler) handleCccUwbRanging(frame *proto.Frame) (*proto.Frame, error) {
	var req proto.CccUwbRangingPayload
	if err := decodePayload(frame.Payload, &req); err != nil {
		return h.makeError(frame, 403, "decode UWB ranging: "+err.Error()), nil
	}
	distance := float64(h.vehicle.GetDistance()) / 1000.0
	resp := &proto.CccUwbRangingPayload{
		SessionID:      req.SessionID,
		SlotIndex:      req.SlotIndex + 1,
		Distance:       distance,
		Rssi:           -45 + int32(req.SlotIndex)*3,
		AngleAzimuth:   12.5 + float64(req.SlotIndex)*1.5,
		AngleElevation: -3.0,
		Status:         1,
	}
	log.Printf("[CCC-UWB] Ranging: %.2fm angle_az=%.1f° rssi=%d", distance, resp.AngleAzimuth, resp.Rssi)
	return proto.NewFrame(proto.MsgTypeCCC_UWB_Rng, encodePayload(resp)), nil
}

// ── ICCOA Protocol Handlers ─────────────────────────────────

func (h *Handler) handleIcCoaFrame(frame *proto.Frame) (*proto.Frame, error) {
	var req proto.IcCoaFramePayload
	if err := decodePayload(frame.Payload, &req); err != nil {
		return h.makeError(frame, 501, "decode ICCOA frame: "+err.Error()), nil
	}
	version := "DK3.0"
	if req.DkVersion == 40 {
		version = "DK4.0"
	}
	log.Printf("[ICCOA-%s] Processing frame, payload_len=%d", version, len(req.Payload))

	resp := &proto.IcCoaFramePayload{
		DkVersion: req.DkVersion,
		Sop:       []byte{0x53, 0x4F, 0x50}, // "SOP" marker
		Payload:   []byte("iccoa_response_payload"),
		Checksum:  0xDEAD,
	}
	if req.DkVersion == 40 {
		resp.SessionToken = []byte("dk40_session_token_16b")
		resp.Hmac = []byte("hmac_for_iccoa_dk40______")
	}
	msgType := proto.MsgTypeICCOA_DK30
	if req.DkVersion == 40 {
		msgType = proto.MsgTypeICCOA_DK40
	}
	return proto.NewFrame(msgType, encodePayload(resp)), nil
}

func (h *Handler) handleIcCoaHmac(frame *proto.Frame) (*proto.Frame, error) {
	var req proto.IcCoaFramePayload
	if err := decodePayload(frame.Payload, &req); err != nil {
		return h.makeError(frame, 502, "decode ICCOA HMAC: "+err.Error()), nil
	}
	verified := len(req.Hmac) == 16
	log.Printf("[ICCOA-HMAC] Verifying HMAC (len=%d) → verified=%v", len(req.Hmac), verified)
	resp := &proto.IcCoaFramePayload{
		DkVersion: req.DkVersion,
		Checksum:  1,
	}
	return proto.NewFrame(proto.MsgTypeICCOA_HMAC, encodePayload(resp)), nil
}

// ── Extended Vehicle Control ────────────────────────────────

func (h *Handler) handleWindow(frame *proto.Frame) (*proto.Frame, error) {
	var req proto.CommandPayload
	if err := decodePayload(frame.Payload, &req); err != nil {
		return h.makeError(frame, 601, "decode window: "+err.Error()), nil
	}
	status := uint32(0)
	switch req.Action {
	case "window_up":
		status = 0
	case "window_down":
		status = 1
	case "window_vent":
		status = 2
	default:
		return h.makeError(frame, 602, "unknown window action: "+req.Action), nil
	}
	h.vehicle.mu.Lock()
	h.vehicle.TrunkStatus = status
	h.vehicle.mu.Unlock()
	log.Printf("[VEHICLE] Window: %s → status=%d", req.Action, status)
	resp := &proto.CommandPayload{Action: req.Action, Result: 0}
	return proto.NewFrame(proto.MsgTypeWindowCmd, encodePayload(resp)), nil
}

func (h *Handler) handleClimate(frame *proto.Frame) (*proto.Frame, error) {
	var req proto.CommandPayload
	if err := decodePayload(frame.Payload, &req); err != nil {
		return h.makeError(frame, 603, "decode climate: "+err.Error()), nil
	}
	log.Printf("[VEHICLE] Climate: %s", req.Action)
	resp := &proto.CommandPayload{Action: req.Action, Result: 0}
	return proto.NewFrame(proto.MsgTypeClimateCmd, encodePayload(resp)), nil
}

func (h *Handler) handleFind(frame *proto.Frame) (*proto.Frame, error) {
	var req proto.CommandPayload
	if err := decodePayload(frame.Payload, &req); err != nil {
		return h.makeError(frame, 604, "decode find: "+err.Error()), nil
	}
	h.vehicle.mu.Lock()
	h.vehicle.AlarmStatus = 3 // honk + flash
	h.vehicle.mu.Unlock()
	log.Printf("[VEHICLE] Find-my-car triggered: honk+flash")
	resp := &proto.CommandPayload{Action: "find_my_car", Result: 0}
	return proto.NewFrame(proto.MsgTypeFindCmd, encodePayload(resp)), nil
}

func (h *Handler) handleSeat(frame *proto.Frame) (*proto.Frame, error) {
	var req proto.CommandPayload
	if err := decodePayload(frame.Payload, &req); err != nil {
		return h.makeError(frame, 605, "decode seat: "+err.Error()), nil
	}
	log.Printf("[VEHICLE] Seat adjustment: %s", req.Action)
	resp := &proto.CommandPayload{Action: req.Action, Result: 0}
	return proto.NewFrame(proto.MsgTypeSeatCmd, encodePayload(resp)), nil
}

func (h *Handler) handleHornLight(frame *proto.Frame) (*proto.Frame, error) {
	var req proto.CommandPayload
	if err := decodePayload(frame.Payload, &req); err != nil {
		return h.makeError(frame, 606, "decode horn/light: "+err.Error()), nil
	}
	log.Printf("[VEHICLE] Horn/Light: %s", req.Action)
	resp := &proto.CommandPayload{Action: req.Action, Result: 0}
	return proto.NewFrame(proto.MsgTypeHornLightCmd, encodePayload(resp)), nil
}

// ── Status Sync Strategy Handlers ───────────────────────────

func (h *Handler) handleSyncPoll(frame *proto.Frame) (*proto.Frame, error) {
	h.vehicle.mu.RLock()
	resp := &proto.SyncPayload{
		SyncMode:     1,
		LockStatus:   h.vehicle.LockStatus,
		EngineStatus: h.vehicle.EngineStatus,
		AlarmStatus:  h.vehicle.AlarmStatus,
		TrunkStatus:  h.vehicle.TrunkStatus,
		BatteryPct:   h.vehicle.BatteryPct,
		Latitude:     h.vehicle.Latitude,
		Longitude:    h.vehicle.Longitude,
		Timestamp:    time.Now().UnixMilli(),
	}
	h.vehicle.mu.RUnlock()
	log.Printf("[SYNC-POLL] Status snapshot sent: lock=%d engine=%d bat=%d%%",
		resp.LockStatus, resp.EngineStatus, resp.BatteryPct)
	return proto.NewFrame(proto.MsgTypeSyncPoll, encodePayload(resp)), nil
}

func (h *Handler) handleSyncEvent(frame *proto.Frame) (*proto.Frame, error) {
	var req proto.SyncPayload
	if err := decodePayload(frame.Payload, &req); err != nil {
		return h.makeError(frame, 701, "decode sync event: "+err.Error()), nil
	}
	// Event-driven: car pushes status on change
	h.vehicle.mu.RLock()
	resp := &proto.SyncPayload{
		SyncMode:     2,
		DeviceID:     req.DeviceID,
		LockStatus:   h.vehicle.LockStatus,
		EngineStatus: h.vehicle.EngineStatus,
		EventSeq:     h.vehicle.SeqCounter,
		Timestamp:    time.Now().UnixMilli(),
	}
	h.vehicle.mu.RUnlock()
	log.Printf("[SYNC-EVENT] Push: device=%s seq=%d", resp.DeviceID, resp.EventSeq)
	return proto.NewFrame(proto.MsgTypeSyncEvent, encodePayload(resp)), nil
}

func (h *Handler) handleSyncHeartbeat(frame *proto.Frame) (*proto.Frame, error) {
	var req proto.SyncPayload
	if err := decodePayload(frame.Payload, &req); err != nil {
		return h.makeError(frame, 702, "decode sync heartbeat: "+err.Error()), nil
	}
	h.vehicle.mu.RLock()
	resp := &proto.SyncPayload{
		SyncMode:     3,
		LockStatus:   h.vehicle.LockStatus,
		EngineStatus: h.vehicle.EngineStatus,
		BatteryPct:   h.vehicle.BatteryPct,
		Latitude:     h.vehicle.Latitude,
		Longitude:    h.vehicle.Longitude,
		Timestamp:    time.Now().UnixMilli(),
	}
	h.vehicle.mu.RUnlock()
	return proto.NewFrame(proto.MsgTypeSyncHeartbeat, encodePayload(resp)), nil
}

func (h *Handler) handleSyncOffline(frame *proto.Frame) (*proto.Frame, error) {
	var req proto.SyncPayload
	if err := decodePayload(frame.Payload, &req); err != nil {
		return h.makeError(frame, 703, "decode sync offline: "+err.Error()), nil
	}
	log.Printf("[SYNC-OFFLINE] Upload %d buffered events from %s", req.OfflineCount, req.DeviceID)
	resp := &proto.SyncPayload{
		SyncMode:     4,
		DeviceID:     req.DeviceID,
		OfflineCount: req.OfflineCount,
		Timestamp:    time.Now().UnixMilli(),
	}
	return proto.NewFrame(proto.MsgTypeSyncOffline, encodePayload(resp)), nil
}

func (h *Handler) handleSyncConflict(frame *proto.Frame) (*proto.Frame, error) {
	var req proto.SyncPayload
	if err := decodePayload(frame.Payload, &req); err != nil {
		return h.makeError(frame, 704, "decode sync conflict: "+err.Error()), nil
	}
	h.vehicle.mu.Lock()
	h.vehicle.SeqCounter = max(h.vehicle.SeqCounter, req.ConflictSeq)
	h.vehicle.mu.Unlock()
	log.Printf("[SYNC-CONFLICT] Resolved: seq=%d last_event=%d -> resolved", req.ConflictSeq, req.EventSeq)
	resp := &proto.SyncPayload{
		SyncMode:    4,
		DeviceID:    req.DeviceID,
		EventSeq:    h.vehicle.SeqCounter,
		ConflictSeq: req.ConflictSeq,
		ResolvedBy:  "last_writer_wins",
		Timestamp:   time.Now().UnixMilli(),
	}
	return proto.NewFrame(proto.MsgTypeSyncConflict, encodePayload(resp)), nil
}

func (h *Handler) handleSyncBatch(frame *proto.Frame) (*proto.Frame, error) {
	var req proto.SyncPayload
	if err := decodePayload(frame.Payload, &req); err != nil {
		return h.makeError(frame, 705, "decode sync batch: "+err.Error()), nil
	}
	h.vehicle.mu.RLock()
	resp := &proto.SyncPayload{
		SyncMode:     5,
		LockStatus:   h.vehicle.LockStatus,
		EngineStatus: h.vehicle.EngineStatus,
		BatteryPct:   h.vehicle.BatteryPct,
		Timestamp:    time.Now().UnixMilli(),
	}
	h.vehicle.mu.RUnlock()
	log.Printf("[SYNC-BATCH] Batch status sent")
	return proto.NewFrame(proto.MsgTypeSyncBatch, encodePayload(resp)), nil
}
