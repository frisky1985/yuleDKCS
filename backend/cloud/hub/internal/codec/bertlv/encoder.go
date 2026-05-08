// Package bertlv provides BER-TLV encoding/decoding
// Reference: ISO 7816-4, EMVco Book 3
package bertlv

import (
	"bytes"
	"encoding/binary"
	"fmt"
)

// Encoder BERTLV encoder
type Encoder struct {
	buf *bytes.Buffer
}

// NewEncoder creates a new encoder
func NewEncoder() *Encoder {
	return &Encoder{buf: new(bytes.Buffer)}
}

// Encode encodes a value with the given tag
// Supports int, uint, string, []byte and other basic types
func Encode(tag Tag, value interface{}) ([]byte, error) {
	enc := NewEncoder()
	if err := enc.encodeValue(tag, value); err != nil {
		return nil, err
	}
	return enc.buf.Bytes(), nil
}

// EncodeTLV encodes with explicit tag, length, and value
func EncodeTLV(tag Tag, length int, value []byte) ([]byte, error) {
	enc := NewEncoder()
	if err := enc.writeTag(tag); err != nil {
		return nil, err
	}
	if err := enc.writeLength(length); err != nil {
		return nil, err
	}
	enc.buf.Write(value)
	return enc.buf.Bytes(), nil
}

// encodeValue dispatches to the appropriate write method
func (e *Encoder) encodeValue(tag Tag, value interface{}) error {
	switch v := value.(type) {
	case int:
		return e.writeInt(tag, int64(v))
	case int8:
		return e.writeInt(tag, int64(v))
	case int16:
		return e.writeInt(tag, int64(v))
	case int32:
		return e.writeInt(tag, int64(v))
	case int64:
		return e.writeInt(tag, v)
	case uint:
		return e.writeUint(tag, uint64(v))
	case uint8:
		return e.writeUint(tag, uint64(v))
	case uint16:
		return e.writeUint(tag, uint64(v))
	case uint32:
		return e.writeUint(tag, uint64(v))
	case uint64:
		return e.writeUint(tag, v)
	case string:
		return e.writeTLV(tag, len(v), []byte(v))
	case []byte:
		return e.writeTLV(tag, len(v), v)
	case [][]byte:
		return e.writeConstructed(tag, v)
	case *Encoder:
		return e.writeTLV(tag, v.buf.Len(), v.buf.Bytes())
	default:
		return fmt.Errorf("unsupported value type: %T", value)
	}
}

// writeInt encodes a signed integer
func (e *Encoder) writeInt(tag Tag, value int64) error {
	bytesNeeded := 8 // int64 always 8 bytes
	buf := make([]byte, 8)
	binary.BigEndian.PutUint64(buf, uint64(value))
	return e.writeTLV(tag, bytesNeeded, buf)
}

// writeUint encodes an unsigned integer
func (e *Encoder) writeUint(tag Tag, value uint64) error {
	bytesNeeded := uintSize(value)
	buf := make([]byte, 8)
	binary.BigEndian.PutUint64(buf, value)
	return e.writeTLV(tag, bytesNeeded, buf[8-bytesNeeded:])
}

// writeTLV writes Tag + Length + Value
func (e *Encoder) writeTLV(tag Tag, length int, value []byte) error {
	if err := e.writeTag(tag); err != nil {
		return err
	}
	if err := e.writeLength(length); err != nil {
		return err
	}
	e.buf.Write(value)
	return nil
}

// writeTag encodes a tag per ISO 7816-4
// Single byte for 0x00-0x1F and 0x80-0x9E (b8-b7=10 and b5-b1<31).
// Multi-byte: [0x1F + low-5-bits, continuation bytes with b7=1 / b7=0].
func (e *Encoder) writeTag(tag Tag) error {
	t := uint32(tag)

	// Single-byte: 0x00-0x1F, 0x80-0x9E, or 0xC0-0xFF (first byte)
	if t <= 0x1F || (t >= 0x80 && t <= 0x9E) || byte(t) >= 0xC0 {
		return e.buf.WriteByte(byte(t))
	}

	// Multi-byte: count bytes needed (preserve original t for encoding)
	bytesNeeded := 0
	for tmp := t; ; {
		tmp >>= 7
		bytesNeeded++
		if tmp == 0 {
			break
		}
	}

	// Build bytes right-to-left, extracting 7 bits at a time from orig
	b := make([]byte, bytesNeeded)
	for i := bytesNeeded - 1; i >= 0; i-- {
		b[i] = byte(t>>uint(7*i)) & 0x7F
	}
	// Set continuation bit (b7=1) on all but the last byte
	for i := 0; i < bytesNeeded-1; i++ {
		b[i] |= 0x80
	}

	if _, err := e.buf.Write(b); err != nil {
		return err
	}
	return nil
}

// writeLength encodes length per BER rules
func (e *Encoder) writeLength(length int) error {
	if length < 0 {
		return fmt.Errorf("negative length")
	}
	// Short form: 0x00-0x7F
	if length <= 0x7F {
		return e.buf.WriteByte(byte(length))
	}
	// Long form: 0x80+ means subsequent bytes hold length value
	if length <= 0xFF && length > 0x7F {
		if _, err := e.buf.Write([]byte{0x81, byte(length)}); err != nil {
			return err
		}
		return nil
	}
	if length <= 0xFFFF {
		buf := make([]byte, 3)
		buf[0] = 0x82
		binary.BigEndian.PutUint16(buf[1:], uint16(length))
		if _, err := e.buf.Write(buf); err != nil {
			return err
		}
		return nil
	}
	// 3-byte length (max 0xFFFFFF)
	if length > 0xFFFFFF {
		return fmt.Errorf("length %d exceeds maximum 16777215", length)
	}
	buf := make([]byte, 4)
	buf[0] = 0x83
	buf[1] = byte(length >> 16)
	buf[2] = byte(length >> 8)
	buf[3] = byte(length)
	if _, err := e.buf.Write(buf); err != nil {
		return err
	}
	return nil
}

// writeConstructed encodes a constructed TLV
func (e *Encoder) writeConstructed(tag Tag, tlvList [][]byte) error {
	totalLen := 0
	for _, b := range tlvList {
		totalLen += len(b)
	}
	if err := e.writeTag(tag); err != nil {
		return err
	}
	if err := e.writeLength(totalLen); err != nil {
		return err
	}
	for _, b := range tlvList {
		e.buf.Write(b)
	}
	return nil
}

func intSize(x int64) int {
	if x >= 0 {
		return uintSize(uint64(x))
	}
	// Negative: find minimum bytes needed (big-endian two's complement)
	mag := uint64(-x - 1)
	if mag < 0x80 {
		return 1
	}
	bytes := 1
	for mag >= 0x80 {
		mag >>= 8
		bytes++
	}
	return bytes
}

func uintSize(x uint64) int {
	if x == 0 {
		return 1
	}
	bytesNeeded := 0
	for x > 0 {
		x >>= 8
		bytesNeeded++
	}
	return bytesNeeded
}

// EncodeAll encodes multiple TLVs into a single byte slice
func EncodeAll(tlvs []*TLV) ([]byte, error) {
	var result []byte
	for _, t := range tlvs {
		raw, err := EncodeTLV(t.Tag, t.Length, t.Value)
		if err != nil {
			return nil, err
		}
		result = append(result, raw...)
	}
	return result, nil
}

// NewTLV creates a TLV from a tag and value
func NewTLV(tag Tag, value interface{}) (*TLV, error) {
	var val []byte
	switch v := value.(type) {
	case []byte:
		val = v
	case string:
		val = []byte(v)
	case int, int8, int16, int32, int64:
		var i int64
		switch vv := value.(type) {
		case int:
			i = int64(vv)
		case int8:
			i = int64(vv)
		case int16:
			i = int64(vv)
		case int32:
			i = int64(vv)
		case int64:
			i = vv
		}
		n := intSize(i)
		buf := make([]byte, 8)
		binary.BigEndian.PutUint64(buf, uint64(i))
		val = buf[8-n:]
	case uint, uint8, uint16, uint32, uint64:
		var u uint64
		switch vv := value.(type) {
		case uint:
			u = uint64(vv)
		case uint8:
			u = uint64(vv)
		case uint16:
			u = uint64(vv)
		case uint32:
			u = uint64(vv)
		case uint64:
			u = vv
		}
		n := uintSize(u)
		buf := make([]byte, 8)
		binary.BigEndian.PutUint64(buf, u)
		val = buf[8-n:]
	default:
		return nil, fmt.Errorf("unsupported value type: %T", value)
	}
	return &TLV{Tag: tag, Length: len(val), Value: val}, nil
}

// DeepCopy creates an independent copy of the TLV
func (t *TLV) DeepCopy() *TLV {
	valCopy := make([]byte, len(t.Value))
	copy(valCopy, t.Value)
	return &TLV{
		Tag:     t.Tag,
		Length:  t.Length,
		Value:   valCopy,
		RawData: append([]byte(nil), t.RawData...),
	}
}
