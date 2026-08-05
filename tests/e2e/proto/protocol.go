// Package proto defines the wire protocol between Car Simulator and Mobile Test Client.
// This simulates BLE GATT characteristics over TCP with a structured binary frame.
package proto

import (
	"encoding/binary"
	"encoding/json"
	"fmt"
	"io"
	"time"
)

const (
	ProtocolVersion = uint16(1)

	// Message types
	MsgTypePairingRequest  = uint16(0x0001)
	MsgTypePairingResponse = uint16(0x0002)
	MsgTypeSCP03Init       = uint16(0x0010)
	MsgTypeSCP03Auth       = uint16(0x0011)
	MsgTypeSCP03Secure     = uint16(0x0012)
	MsgTypeKeyWrite        = uint16(0x0020)
	MsgTypeKeyWriteAck     = uint16(0x0021)
	MsgTypeChallengeReq    = uint16(0x0030)
	MsgTypeChallengeResp   = uint16(0x0031)
	MsgTypeUnlockCmd       = uint16(0x0040)
	MsgTypeLockCmd         = uint16(0x0041)
	MsgTypeEngineCmd       = uint16(0x0042)
	MsgTypeTrunkCmd        = uint16(0x0043)
	MsgTypeStatusReq       = uint16(0x0050)
	MsgTypeStatusResp      = uint16(0x0051)
	MsgTypeNfcRead         = uint16(0x0060)
	MsgTypeNfcCredential   = uint16(0x0061)
	MsgTypeNfcAuthResp     = uint16(0x0062)
	MsgTypeReplayFrame     = uint16(0x0070)
	MsgTypeReplayDetect    = uint16(0x0071)
	MsgTypeAlarmTrigger    = uint16(0x0072)
	MsgTypeRevokeNotify    = uint16(0x0080)
	MsgTypeError           = uint16(0xFFFF)

	// Protocol-specific message types
	MsgTypeICCEPair     = uint16(0x0800)
	MsgTypeICCEEdge     = uint16(0x0801) // ICCE edge compute trigger
	MsgTypeCCC_OOB_Init = uint16(0x0900) // CCC NFC OOB initial handshake
	MsgTypeCCC_OOB_Data = uint16(0x0901) // CCC OOB credential exchange
	MsgTypeCCC_UWB_Rng  = uint16(0x0902) // CCC UWB ranging session
	MsgTypeICCOA_DK30   = uint16(0x0A00) // ICCOA DK 3.0 frame
	MsgTypeICCOA_DK40   = uint16(0x0A01) // ICCOA DK 4.0 frame with HMAC
	MsgTypeICCOA_HMAC   = uint16(0x0A02) // ICCOA HMAC verification

	// Extended vehicle control
	MsgTypeWindowCmd     = uint16(0x0100)
	MsgTypeClimateCmd    = uint16(0x0101)
	MsgTypeFindCmd       = uint16(0x0102)
	MsgTypeSeatCmd       = uint16(0x0103)
	MsgTypeHornLightCmd  = uint16(0x0104)

	// Status sync strategies
	MsgTypeSyncPoll     = uint16(0x0200) // periodic poll
	MsgTypeSyncEvent    = uint16(0x0201) // event-driven push
	MsgTypeSyncHeartbeat= uint16(0x0202) // heartbeat-based
	MsgTypeSyncOffline  = uint16(0x0203) // offline buffer upload
	MsgTypeSyncConflict = uint16(0x0204) // conflict resolution
	MsgTypeSyncBatch    = uint16(0x0205) // batch status sync
)

// Protocol enums
type ProtocolEnum uint32

const (
	ProtoICCE    ProtocolEnum = 4
	ProtoCCC_DK3 ProtocolEnum = 1
	ProtoICCOA30 ProtocolEnum = 2
	ProtoICCOA40 ProtocolEnum = 3
)

// SyncMode enums
type SyncMode uint32

const (
	SyncPoll     SyncMode = 1
	SyncEvent    SyncMode = 2
	SyncHeartbeat SyncMode = 3
	SyncOffline   SyncMode = 4
)

// FrameHeader is the binary header for all messages.
type FrameHeader struct {
	Length    uint32 // payload length (big-endian)
	Version   uint16
	MsgType   uint16
	SeqNum    uint32
	Timestamp uint64 // unix milliseconds
}

const HeaderSize = 20 // 4+2+2+4+8

// Frame wraps a header with its payload.
type Frame struct {
	Header  FrameHeader
	Payload []byte
}

// Marshal serializes a Frame to bytes.
func (f *Frame) Marshal() []byte {
	f.Header.Length = uint32(len(f.Payload))
	buf := make([]byte, HeaderSize+len(f.Payload))
	binary.BigEndian.PutUint32(buf[0:4], f.Header.Length)
	binary.BigEndian.PutUint16(buf[4:6], f.Header.Version)
	binary.BigEndian.PutUint16(buf[6:8], f.Header.MsgType)
	binary.BigEndian.PutUint32(buf[8:12], f.Header.SeqNum)
	binary.BigEndian.PutUint64(buf[12:], f.Header.Timestamp)
	copy(buf[HeaderSize:], f.Payload)
	return buf
}

// ReadFrame reads one Frame from a reader.
func ReadFrame(r io.Reader) (*Frame, error) {
	headerBuf := make([]byte, HeaderSize)
	if _, err := io.ReadFull(r, headerBuf); err != nil {
		return nil, fmt.Errorf("read header: %w", err)
	}
	h := FrameHeader{
		Length:    binary.BigEndian.Uint32(headerBuf[0:4]),
		Version:   binary.BigEndian.Uint16(headerBuf[4:6]),
		MsgType:   binary.BigEndian.Uint16(headerBuf[6:8]),
		SeqNum:    binary.BigEndian.Uint32(headerBuf[8:12]),
		Timestamp: binary.BigEndian.Uint64(headerBuf[12:]),
	}
	payload := make([]byte, h.Length)
	if h.Length > 0 {
		if _, err := io.ReadFull(r, payload); err != nil {
			return nil, fmt.Errorf("read payload: %w", err)
		}
	}
	return &Frame{Header: h, Payload: payload}, nil
}

// NewFrame creates a Frame with the given message type and payload, setting version, seq and timestamp.
var globalSeq uint32

func NewFrame(msgType uint16, payload []byte) *Frame {
	globalSeq++
	return &Frame{
		Header: FrameHeader{
			Version:   ProtocolVersion,
			MsgType:   msgType,
			SeqNum:    globalSeq,
			Timestamp: uint64(time.Now().UnixMilli()),
		},
		Payload: payload,
	}
}

// ── Payload helpers ──────────────────────────────────────────

// PairingPayload is sent in pairing requests/responses.
type PairingPayload struct {
	DeviceID    string `json:"device_id"`
	Vendor      uint32 `json:"vendor"`      // PhoneVendor enum
	Protocol    uint32 `json:"protocol"`    // Protocol enum
	PublicKey   []byte `json:"public_key"`  // DER-encoded pubkey
	Challenge   []byte `json:"challenge"`
	SessionKey  []byte `json:"session_key,omitempty"`
}

// SCP03Payload carries SCP03 secure channel data.
type SCP03Payload struct {
	Step          uint32 `json:"step"`
	HostChallenge []byte `json:"host_challenge"`
	CardChallenge []byte `json:"card_challenge"`
	Mac           []byte `json:"mac"`
	EncryptedData []byte `json:"encrypted_data"`
	Success       bool   `json:"success"`
}

// AuthPayload carries challenge/response auth data.
type AuthPayload struct {
	KeyID     string `json:"key_id"`
	Challenge []byte `json:"challenge"`
	Signature []byte `json:"signature"`
	Counter   uint32 `json:"counter"`
	Timestamp int64  `json:"timestamp"`
}

// KeyPayload carries key material.
type KeyPayload struct {
	KeyID       string `json:"key_id"`
	KeyType     uint32 `json:"key_type"`
	Permissions uint32 `json:"permissions"` // bitmask
	KeyData     []byte `json:"key_data"`
	ValidFrom   int64  `json:"valid_from"`
	ValidUntil  int64  `json:"valid_until"`
	Status      uint32 `json:"status"` // 1=active, 2=suspended, 3=revoked
}

// CommandPayload carries lock/unlock/etc commands.
type CommandPayload struct {
	Action    string `json:"action"`
	KeyID     string `json:"key_id"`
	Params    []byte `json:"params"`
	Result    uint32 `json:"result"` // 0=ok, 1=fail
	ErrorMsg  string `json:"error_msg"`
}

// StatusPayload carries vehicle state.
type StatusPayload struct {
	LockStatus   uint32  `json:"lock_status"`   // 0=locked, 1=unlocked
	EngineStatus uint32  `json:"engine_status"` // 0=off, 1=on
	Rssi         int32   `json:"rssi"`          // simulated BLE RSSI
	AlarmStatus  uint32  `json:"alarm_status"`  // 0=off, 1=triggered
	Distance     uint32  `json:"distance"`      // mm
	BatteryPct   uint32  `json:"battery_pct"`
	Latitude     float64 `json:"latitude"`
	Longitude    float64 `json:"longitude"`
}

// NFCReadPayload carries NFC tag data.
type NFCReadPayload struct {
	CredentialID string            `json:"credential_id"`
	KeyID        string            `json:"key_id"`
	Data         []byte            `json:"data"`
	AuthData     []byte            `json:"auth_data"`
	AuthSuccess  bool              `json:"auth_success"`
	Metadata     map[string]string `json:"metadata,omitempty"`
}

// ReplayPayload captures a frame that (pretends to be) replayed.
type ReplayPayload struct {
	OriginalSeq  uint32 `json:"original_seq"`
	OriginalTs   int64  `json:"original_ts"`
	ReplayedSeq  uint32 `json:"replayed_seq"`
	ReplayedTs   int64  `json:"replayed_ts"`
	Blocked      bool   `json:"blocked"`
	Reason       string `json:"reason"`
}

// RevokeNotifyPayload carries revocation notification.
type RevokeNotifyPayload struct {
	KeyID      string `json:"key_id"`
	RevokedAt  int64  `json:"revoked_at"`
	Reason     string `json:"reason"`
}

// ── Protocol-specific payloads ────────────────────────────

// ICCEPairPayload carries ICCE-specific pairing data.
type ICCEPairPayload struct {
	DeviceID    string `json:"device_id"`
	PublicKey   []byte `json:"public_key"`
	EdgeCapable bool   `json:"edge_capable"`
	Challenge   []byte `json:"challenge"`
}

// ICCEEdgePayload carries ICCE edge compute trigger.
type ICCEEdgePayload struct {
	RuleID   string `json:"rule_id"`
	Zone     uint32 `json:"zone"`
	Event    string `json:"event"`
	Distance uint32 `json:"distance"`
	Action   string `json:"action"`
	Result   uint32 `json:"result"`
}

// CccOobPayload carries CCC NFC Out-Of-Band exchange data.
type CccOobPayload struct {
	Step          uint32 `json:"step"`
	NfcData       []byte `json:"nfc_data"`
	Certificate   []byte `json:"certificate"`
	EphemeralKey  []byte `json:"ephemeral_key"`
	Signature     []byte `json:"signature"`
	Success       bool   `json:"success"`
}

// CccUwbRangingPayload carries CCC UWB ranging session data.
type CccUwbRangingPayload struct {
	SessionID   uint32  `json:"session_id"`
	SlotIndex   uint32  `json:"slot_index"`
	Distance    float64 `json:"distance"`
	Rssi        int32   `json:"rssi"`
	AngleAzimuth float64 `json:"angle_azimuth"`
	AngleElevation float64 `json:"angle_elevation"`
	Status      uint32  `json:"status"`
}

// IcCoaFramePayload carries ICCOA DK3.0/DK4.0 message.
type IcCoaFramePayload struct {
	DkVersion   uint32 `json:"dk_version"`  // 30 or 40
	Sop         []byte `json:"sop"`
	Payload     []byte `json:"payload"`
	Hmac        []byte `json:"hmac,omitempty"`
	SessionToken []byte `json:"session_token,omitempty"`
	Checksum    uint32 `json:"checksum"`
}

// ── Status Sync payloads ───────────────────────────────────

// SyncPayload carries vehicle status sync data with strategy metadata.
type SyncPayload struct {
	SyncMode      uint32  `json:"sync_mode"`       // 1=poll, 2=event, 3=heartbeat, 4=offline
	DeviceID      string  `json:"device_id"`
	LockStatus    uint32  `json:"lock_status"`
	EngineStatus  uint32  `json:"engine_status"`
	AlarmStatus   uint32  `json:"alarm_status"`
	TrunkStatus   uint32  `json:"trunk_status"`
	WindowStatus  uint32  `json:"window_status"`
	BatteryPct    uint32  `json:"battery_pct"`
	Latitude      float64 `json:"latitude"`
	Longitude     float64 `json:"longitude"`
	EventSeq      uint32  `json:"event_seq"`
	Timestamp     int64   `json:"timestamp"`
	OfflineCount  uint32  `json:"offline_count,omitempty"`
	OfflineEvents []byte  `json:"offline_events,omitempty"`
	ConflictSeq   uint32  `json:"conflict_seq,omitempty"`
	ResolvedBy    string  `json:"resolved_by,omitempty"`
}

// ErrorPayload carries error info.
type ErrorPayload struct {
	Code    uint32 `json:"code"`
	Message string `json:"message"`
}

// EncodePayload encodes v to JSON bytes for transport.
func EncodePayload(v interface{}) []byte {
	buf, err := json.Marshal(v)
	if err != nil {
		return nil
	}
	return buf
}

// DecodePayload decodes JSON bytes into v.
func DecodePayload(data []byte, v interface{}) error {
	if len(data) == 0 {
		return nil
	}
	return json.Unmarshal(data, v)
}
