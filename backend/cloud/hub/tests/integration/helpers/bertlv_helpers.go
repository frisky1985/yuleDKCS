// Package helpers provides BERTLV encoding/decoding helpers for integration tests.
package helpers

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"time"
)

// BERTLVTag represents a BER-TLV tag (can be 1 or 2 bytes).
type BERTLVTag struct {
	Bytes []byte
}

// Tag creates a 1-byte tag.
func Tag(t byte) BERTLVTag {
	return BERTLVTag{Bytes: []byte{t}}
}

// Tag2 creates a 2-byte tag.
func Tag2(t1, t2 byte) BERTLVTag {
	return BERTLVTag{Bytes: []byte{t1, t2}}
}

// BERTLVBuilder helps construct BER-TLV encoded messages.
type BERTLVBuilder struct {
	buf bytes.Buffer
}

// NewBERTLVBuilder creates a new builder.
func NewBERTLVBuilder() *BERTLVBuilder {
	return &BERTLVBuilder{}
}

// Append appends a TLV (Tag-Length-Value) entry.
func (b *BERTLVBuilder) Append(tag BERTLVTag, value []byte) {
	b.buf.Write(tag.Bytes)
	b.writeLength(len(value))
	b.buf.Write(value)
}

// AppendString appends a TLV entry with a string value.
func (b *BERTLVBuilder) AppendString(tag BERTLVTag, value string) {
	b.buf.Write(tag.Bytes)
	data := []byte(value)
	b.writeLength(len(data))
	b.buf.Write(data)
}

// AppendBCD appends a TLV entry with a BCD-encoded numeric string.
func (b *BERTLVBuilder) AppendBCD(tag BERTLVTag, bcdDigits string) {
	b.buf.Write(tag.Bytes)
	data := []byte(bcdDigits)
	b.writeLength(len(data))
	b.buf.Write(data)
}

// AppendUint16 appends a TLV entry with a big-endian uint16 value.
func (b *BERTLVBuilder) AppendUint16(tag BERTLVTag, val uint16) {
	bs := make([]byte, 2)
	binary.BigEndian.PutUint16(bs, val)
	b.buf.Write(tag.Bytes)
	b.buf.WriteByte(2)
	b.buf.Write(bs)
}

// AppendInt64 appends a TLV entry with a big-endian int64 value.
func (b *BERTLVBuilder) AppendInt64(tag BERTLVTag, val int64) {
	bs := make([]byte, 8)
	binary.BigEndian.PutUint64(bs, uint64(val))
	b.buf.Write(tag.Bytes)
	b.buf.WriteByte(byte(len(bs)))
	b.buf.Write(bs)
}

// AppendTimestamp appends a TLV entry with an N14 timestamp.
func (b *BERTLVBuilder) AppendTimestamp(tag BERTLVTag, t time.Time) {
	ts := t.Format("20060102150405")
	b.AppendString(tag, ts)
}

// Bytes returns the encoded bytes.
func (b *BERTLVBuilder) Bytes() []byte {
	return b.buf.Bytes()
}

// writeLength writes BER-TLV length encoding (1-3 bytes).
func (b *BERTLVBuilder) writeLength(length int) {
	switch {
	case length <= 0x7F:
		b.buf.WriteByte(byte(length))
	case length <= 0xFF:
		b.buf.Write([]byte{0x81, byte(length)})
	case length <= 0xFFFF:
		b.buf.Write([]byte{0x82, byte(length >> 8), byte(length)})
	default:
		b.buf.Write([]byte{0x83, byte(length >> 16), byte(length >> 8), byte(length)})
	}
}

// ============================================================
// Dummy BLE Advertisement payload for BLE simulation
// ============================================================

// BLEAdvertPayload builds a mock BLE advertisement payload.
type BLEAdvertPayload struct {
	VehicleID   string
	VendorName  string
	ProtocolVer string
	RSSI        int8
	TCUStatus   byte
}

// EncodeAsBERTLV encodes the BLE advertisement as a BERTLV payload.
func (b *BLEAdvertPayload) EncodeAsBERTLV() []byte {
	builder := NewBERTLVBuilder()
	builder.AppendString(Tag(0xC1), b.VehicleID)
	builder.AppendString(Tag(0xC2), b.VendorName)
	builder.AppendString(Tag(0xC3), b.ProtocolVer)
	builder.Append(Tag(0xC4), []byte{byte(b.RSSI)})
	builder.Append(Tag(0xC5), []byte{b.TCUStatus})
	return builder.Bytes()
}

// ============================================================
// NFC Payload for NFC simulation
// ============================================================

// NFCPayload builds a mock NFC data payload (NDEF-like).
type NFCPayload struct {
	NDEFType      string
	VehicleID     string
	KeyRef        string
	Challenge     []byte
	Authenticated bool
	Timestamp     int64
}

// Encode encodes the NFC payload as BERTLV.
func (n *NFCPayload) Encode() []byte {
	builder := NewBERTLVBuilder()
	builder.AppendString(Tag(0xE1), "DK_NFC")
	builder.AppendString(Tag(0xE2), n.VehicleID)
	builder.AppendString(Tag(0xE3), n.KeyRef)
	challenge := n.Challenge
	if challenge == nil {
		challenge = make([]byte, 16)
		for i := range challenge {
			challenge[i] = byte(i)
		}
	}
	builder.Append(Tag(0xE4), challenge)
	auth := byte(0)
	if n.Authenticated {
		auth = 1
	}
	builder.Append(Tag(0xE5), []byte{auth})
	builder.AppendInt64(Tag(0xE6), n.Timestamp)
	return builder.Bytes()
}

// ============================================================
// UWB Ranging payload
// ============================================================

// UWBRangingResult simulates a UWB ranging measurement.
type UWBRangingResult struct {
	DeviceID     string
	DistanceMM   uint32
	Confidence   uint8
	Phase        string
	ErrorMessage string
}

// Encode encodes UWB ranging data as BERTLV.
func (u *UWBRangingResult) Encode() []byte {
	builder := NewBERTLVBuilder()
	builder.AppendString(Tag(0xD0), u.DeviceID)
	b := make([]byte, 4)
	binary.BigEndian.PutUint32(b, u.DistanceMM)
	builder.Append(Tag(0xD1), b)
	builder.Append(Tag(0xD2), []byte{u.Confidence})
	builder.AppendString(Tag(0xD3), u.Phase)
	if u.ErrorMessage != "" {
		builder.AppendString(Tag(0xD4), u.ErrorMessage)
	}
	return builder.Bytes()
}

// ============================================================
// Message header builders for HUB-DKCS / DKCS-TCU protocols
// ============================================================

// BuildHUBMsgHeader builds a BERTLV message header matching the HUB protocol spec.
func BuildHUBMsgHeader(msgType string, seqNo uint32, deviceID, sessionID string) []byte {
	builder := NewBERTLVBuilder()
	builder.AppendBCD(Tag2(0xE1, 0x01), "0100")
	builder.AppendTimestamp(Tag2(0xE1, 0x02), time.Now())
	builder.AppendString(Tag2(0xE1, 0x03), msgType)
	builder.AppendString(Tag2(0xE1, 0x04), fmt.Sprintf("%08d", seqNo))
	builder.AppendString(Tag2(0xE1, 0x05), deviceID)
	if sessionID != "" {
		builder.AppendString(Tag2(0xE1, 0x06), sessionID)
	}
	return builder.Bytes()
}

// BuildTCUMsgHeader builds a BERTLV message header for TCU-DKCS protocol.
func BuildTCUMsgHeader(msgType string, seqNo uint32, tcuID, vehicleID, keyID string) []byte {
	builder := NewBERTLVBuilder()
	builder.AppendBCD(Tag2(0xE1, 0x01), "0100")
	builder.AppendTimestamp(Tag2(0xE1, 0x02), time.Now())
	builder.AppendString(Tag2(0xE1, 0x03), msgType)
	builder.AppendString(Tag2(0xE1, 0x04), fmt.Sprintf("%06d", seqNo))
	builder.AppendString(Tag2(0xE1, 0x05), tcuID)
	builder.AppendString(Tag2(0xE1, 0x06), vehicleID)
	if keyID != "" {
		builder.AppendString(Tag2(0xE1, 0x07), keyID)
	}
	return builder.Bytes()
}
