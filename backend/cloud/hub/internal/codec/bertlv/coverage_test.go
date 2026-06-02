package bertlv

import "testing"

// ─────────────────────────────────────────────────────────────
// Encoder: encodeValue — remaining type branches
// ─────────────────────────────────────────────────────────────

func TestEncode_Int(t *testing.T) {
	raw, err := Encode(0xE0, int(-1))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	decoded, _ := Decode(raw)
	// int is 64-bit on this platform, so -1 → 8 bytes
	if len(decoded.Value) != 8 {
		t.Errorf("int(-1) value len: want 8, got %d", len(decoded.Value))
	}
}

func TestEncode_Int8(t *testing.T) {
	raw, err := Encode(0xE0, int8(-128))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	decoded, _ := Decode(raw)
	if len(decoded.Value) != 1 || decoded.Value[0] != 0x80 {
		t.Errorf("int8(-128): want [0x80], got %x", decoded.Value)
	}
}

func TestEncode_Int16(t *testing.T) {
	raw, err := Encode(0xE0, int16(-32768))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	decoded, _ := Decode(raw)
	if len(decoded.Value) != 2 || decoded.Value[0] != 0x80 || decoded.Value[1] != 0x00 {
		t.Errorf("int16(-32768): want [0x80 0x00], got %x", decoded.Value)
	}
}

func TestEncode_Int32(t *testing.T) {
	raw, err := Encode(0xE0, int32(-2147483648))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	decoded, _ := Decode(raw)
	if len(decoded.Value) != 4 {
		t.Errorf("int32(-2147483648) value len: want 4, got %d", len(decoded.Value))
	}
}

// ─────────────────────────────────────────────────────────────
// writeTag — 3-byte and 4-byte tag paths
// ─────────────────────────────────────────────────────────────

func TestEncode_MultiByteTag3Bytes(t *testing.T) {
	// 3-byte tag: decoder reads [0x9F, 0x81, 0x02] → tag = 0x9F8102
	// Encoder must write raw bytes [0x9F, 0x81, 0x02] for tag value 0x9F8102
	_, err := Encode(Tag(0x9F8102), []byte{0x01})
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
}

func TestEncode_MultiByteTag4Bytes(t *testing.T) {
	// 4-byte tag: decoder reads [0x9F, 0x81, 0x81, 0x01] → tag = 0x9F818101
	_, err := Encode(Tag(0x9F818101), []byte{0x01})
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
}

func TestEncode_Uint(t *testing.T) {
	raw, err := Encode(0xE0, uint(42))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	decoded, err := Decode(raw)
	if err != nil {
		t.Fatalf("roundtrip failed: %v", err)
	}
	if len(decoded.Value) != 1 || decoded.Value[0] != 42 {
		t.Errorf("uint: want [0x2A], got %x", decoded.Value)
	}
}

func TestEncode_Uint8(t *testing.T) {
	raw, err := Encode(0xE0, uint8(0xFF))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	decoded, _ := Decode(raw)
	if len(decoded.Value) != 1 || decoded.Value[0] != 0xFF {
		t.Errorf("uint8 0xFF: want [0xFF], got %x", decoded.Value)
	}
}

func TestEncode_Uint16(t *testing.T) {
	raw, err := Encode(0xE0, uint16(0x1234))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	decoded, _ := Decode(raw)
	if len(decoded.Value) != 2 || decoded.Value[0] != 0x12 || decoded.Value[1] != 0x34 {
		t.Errorf("uint16 0x1234: want [12 34], got %x", decoded.Value)
	}
}

func TestEncode_Uint32(t *testing.T) {
	raw, err := Encode(0xE0, uint32(0xDEADBEEF))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	decoded, _ := Decode(raw)
	if len(decoded.Value) != 4 {
		t.Errorf("uint32 length: want 4, got %d", len(decoded.Value))
	}
}

func TestEncode_Constructed(t *testing.T) {
	inner1, _ := Encode(0xE1, []byte{0xAA})
	inner2, _ := Encode(0xE2, []byte{0xBB, 0xCC})
	raw, err := Encode(0xE0, [][]byte{inner1, inner2})
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if len(raw) < 4 {
		t.Fatalf("constructed too short: %d", len(raw))
	}
	// Tag E0, then length of (inner1 + inner2) = 3 + 4 = 7 → 0x07
	t.Logf("constructed: % X", raw)
}

func TestEncode_EncoderValue(t *testing.T) {
	inner := NewEncoder()
	inner.encodeValue(0xE1, []byte{0xAA})
	raw, err := Encode(0xE0, inner)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	decoded, err := Decode(raw)
	if err != nil {
		t.Fatalf("roundtrip: %v", err)
	}
	t.Logf("inner-as-value: tag=0x%X len=%d val=%x", decoded.Tag, decoded.Length, decoded.Value)
}

func TestEncode_UnsupportedType(t *testing.T) {
	_, err := Encode(0xE0, 3.14)
	if err == nil {
		t.Error("expected error for unsupported type float64")
	}
}

// ─────────────────────────────────────────────────────────────
// EncodeTLV — edge cases
// ─────────────────────────────────────────────────────────────

func TestEncodeTLV_LongTag(t *testing.T) {
	raw, err := EncodeTLV(0x9F01, 2, []byte{0xAB, 0xCD})
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	decoded, _ := Decode(raw)
	if decoded.Tag != 0x9F01 {
		t.Errorf("tag: want 0x9F01, got 0x%X", decoded.Tag)
	}
}

func TestEncodeTLV_EmptyValue(t *testing.T) {
	raw, err := EncodeTLV(0xE0, 0, []byte{})
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if len(raw) != 2 {
		t.Errorf("empty EncodeTLV: want 2 bytes, got %d", len(raw))
	}
}

// ─────────────────────────────────────────────────────────────
// writeLength — 3-byte & negative paths
// ─────────────────────────────────────────────────────────────

func TestEncode_Length3Bytes(t *testing.T) {
	// 0x10000 ≤ length < 0x1000000 → 0x83 + 3 bytes
	v := make([]byte, 0x10000)
	raw, err := Encode(0xE0, v)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if raw[1] != 0x83 {
		t.Errorf("3-byte length marker: want 0x83, got 0x%02X", raw[1])
	}
	// Total: Tag(1) + 0x83(1) + len(3) + value(0x10000)
	expectedLen := 1 + 1 + 3 + 0x10000
	if len(raw) != expectedLen {
		t.Errorf("total len: want %d, got %d", expectedLen, len(raw))
	}
}

// ─────────────────────────────────────────────────────────────
// writeConstructed — explicit path
// ─────────────────────────────────────────────────────────────

func TestWriteConstructed(t *testing.T) {
	enc := NewEncoder()
	err := enc.writeConstructed(0xA0, [][]byte{
		{0xE0, 0x01, 0xAA},
		{0xE1, 0x02, 0xBB, 0xCC},
	})
	if err != nil {
		t.Fatalf("writeConstructed error: %v", err)
	}
	const want = "\xA0\x07\xE0\x01\xAA\xE1\x02\xBB\xCC"
	if got := string(enc.buf.Bytes()); got != want {
		t.Errorf("constructed: want %x, got %x", want, got)
	}
}

// ─────────────────────────────────────────────────────────────
// intSize — negative value branches
// ─────────────────────────────────────────────────────────────

func TestIntSize_Negative(t *testing.T) {
	tests := []struct {
		val int64
		n   int
	}{
		{-1, 1},
		{-128, 1},
		{-129, 2},
		{-32768, 2},
		{-32769, 3},
		{0, 1},
		{1, 1},
		{255, 1},
		{256, 2},
		{0x8000, 2},
		{0x10000, 3},
	}
	for _, tc := range tests {
		got := intSize(tc.val)
		if got != tc.n {
			t.Errorf("intSize(%d): want %d, got %d", tc.val, tc.n, got)
		}
	}
}

// ─────────────────────────────────────────────────────────────
// DecodeToInt* — all int decode functions
// ─────────────────────────────────────────────────────────────

func TestDecodeToInt8(t *testing.T) {
	tlv := &TLV{Value: []byte{0xFF}}
	v, err := tlv.DecodeToInt8()
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if v != -1 {
		t.Errorf("want -1, got %d", v)
	}
}

func TestDecodeToInt16(t *testing.T) {
	tlv := &TLV{Value: []byte{0xFF, 0xFF}}
	v, err := tlv.DecodeToInt16()
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if v != -1 {
		t.Errorf("want -1, got %d", v)
	}
}

func TestDecodeToInt32(t *testing.T) {
	tlv := &TLV{Value: []byte{0x80, 0x00, 0x00, 0x00}}
	v, err := tlv.DecodeToInt32()
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if v != -2147483648 {
		t.Errorf("want min int32, got %d", v)
	}
}

func TestDecodeToInt64(t *testing.T) {
	tlv := &TLV{Value: []byte{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}}
	v, err := tlv.DecodeToInt64()
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if v != -1 {
		t.Errorf("want -1, got %d", v)
	}
}

func TestDecodeToInt_TooShort(t *testing.T) {
	check := func(tlv *TLV) {
		if _, err := tlv.DecodeToInt8(); err == nil {
			t.Error("DecodeToInt8: expected error for empty")
		}
	}
	check(&TLV{Value: []byte{}})
}

func TestDecodeToInt16_TooShort(t *testing.T) {
	_, err := (&TLV{Value: []byte{0x01}}).DecodeToInt16()
	if err == nil {
		t.Error("expected error for 1-byte value")
	}
}

func TestDecodeToInt32_TooShort(t *testing.T) {
	_, err := (&TLV{Value: []byte{0x01, 0x02, 0x03}}).DecodeToInt32()
	if err == nil {
		t.Error("expected error for 3-byte value")
	}
}

func TestDecodeToInt64_TooShort(t *testing.T) {
	_, err := (&TLV{Value: []byte{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}}).DecodeToInt64()
	if err == nil {
		t.Error("expected error for 7-byte value")
	}
}

// ─────────────────────────────────────────────────────────────
// DecodeToUint — error paths
// ─────────────────────────────────────────────────────────────

func TestDecodeToUint32_TooShort(t *testing.T) {
	_, err := (&TLV{Value: []byte{0x01, 0x02, 0x03}}).DecodeToUint32()
	if err == nil {
		t.Error("expected error for short value")
	}
}

func TestDecodeToUint64_TooShort(t *testing.T) {
	_, err := (&TLV{Value: []byte{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}}).DecodeToUint64()
	if err == nil {
		t.Error("expected error for short value")
	}
}

// ─────────────────────────────────────────────────────────────
// readLength — all long-form branches
// ─────────────────────────────────────────────────────────────

func TestDecode_Length3ByteForm(t *testing.T) {
	// 0x83 0x00 0x01 0x00 — 3-byte length, value = 256 bytes
	val := make([]byte, 256)
	val[0] = 0xAA
	data := []byte{0xE0, 0x83, 0x00, 0x01, 0x00}
	data = append(data, val...)
	tlv, err := Decode(data)
	if err != nil {
		t.Fatalf("3-byte length decode error: %v", err)
	}
	if tlv.Length != 256 {
		t.Errorf("length: want 256, got %d", tlv.Length)
	}
	if tlv.Value[0] != 0xAA {
		t.Errorf("first value byte: want 0xAA, got 0x%02X", tlv.Value[0])
	}
}

// ─────────────────────────────────────────────────────────────
// readLength — invalid long form
// ─────────────────────────────────────────────────────────────

func TestDecode_LengthTooManyBytes(t *testing.T) {
	// 0x84 00 00 00 01 — 4-byte length (> 3 is invalid per our decoder)
	data := []byte{0xE0, 0x84, 0x00, 0x00, 0x00, 0x01}
	_, err := Decode(data)
	if err == nil {
		t.Error("expected error for 4+ byte length")
	}
}

// ─────────────────────────────────────────────────────────────
// intToBytes — multi-byte tag serialization
// ─────────────────────────────────────────────────────────────

func TestIntToBytes_Various(t *testing.T) {
	d := &Decoder{}
	tests := []struct {
		tag Tag
		n   int // expected byte count
	}{
		{0x01, 1},
		{0x9F, 1},
		{0x9F01, 2},
		{0x9F0102, 3},
	}
	for _, tc := range tests {
		b := d.intToBytes(tc.tag, 0)
		if len(b) != tc.n {
			t.Errorf("intToBytes(0x%04X, 0): want %d bytes, got %d: %x", tc.tag, tc.n, len(b), b)
		}
	}
}

// ─────────────────────────────────────────────────────────────
// BERTLVError — error types
// ─────────────────────────────────────────────────────────────

func TestBERTLVError_String(t *testing.T) {
	e := &BERTLVError{Code: "E001", Message: "test error"}
	s := e.Error()
	if s != "[E001] test error" {
		t.Errorf("Error(): got %q", s)
	}
}

func TestBERTLVError_WithOffset(t *testing.T) {
	e := &BERTLVError{Code: "E002", Message: "bad length", Offset: 5}
	s := e.Error()
	if s != "[E002] bad length (offset: 5)" {
		t.Errorf("Error() with offset: got %q", s)
	}
}

func TestNewError(t *testing.T) {
	e := NewError("E005", "unsupported", 3)
	if e.Code != "E005" || e.Offset != 3 {
		t.Errorf("NewError: got Code=%s Offset=%d", e.Code, e.Offset)
	}
}

func TestErrDataTruncated(t *testing.T) {
	e := ErrDataTruncated(10, 5, 2)
	if e.Code != ErrCodeDataTruncated {
		t.Errorf("wrong code: %s", e.Code)
	}
	if e.Offset != 2 {
		t.Errorf("wrong offset: %d", e.Offset)
	}
}

func TestErrDepthExceeded(t *testing.T) {
	e := ErrDepthExceeded(64, 32)
	if e.Code != ErrCodeDepthExceeded {
		t.Errorf("wrong code: %s", e.Code)
	}
}

func TestErrUnsupportedTag(t *testing.T) {
	e := ErrUnsupportedTag(0xFFFF)
	if e.Code != ErrCodeUnsupportedTag {
		t.Errorf("wrong code: %s", e.Code)
	}
}

func TestErrInvalidValue(t *testing.T) {
	e := ErrInvalidValue("bad data", 0)
	if e.Code != ErrCodeInvalidValue {
		t.Errorf("wrong code: %s", e.Code)
	}
}

func TestErrBufferOverflow(t *testing.T) {
	e := ErrBufferOverflow(100, 50)
	if e.Code != ErrCodeBufferOverflow {
		t.Errorf("wrong code: %s", e.Code)
	}
}

// ─────────────────────────────────────────────────────────────
// TLV helpers
// ─────────────────────────────────────────────────────────────

func TestTLV_String(t *testing.T) {
	tlv := &TLV{Tag: 0xE0, Length: 3, Value: []byte{0x01, 0x02, 0x03}}
	s := tlv.String()
	if s == "" {
		t.Error("String() returned empty")
	}
	t.Logf("TLV.String: %s", s)
}

func TestTLV_IsConstructed_False(t *testing.T) {
	tlv := &TLV{Tag: 0xC0, Value: []byte{0x01}}
	if tlv.IsConstructed() {
		t.Error("0xC0 should not be constructed")
	}
}

func TestTLV_IsConstructed_True(t *testing.T) {
	tlv := &TLV{Tag: 0xA0, Value: []byte{0xE1, 0x01, 0xAA}}
	if !tlv.IsConstructed() {
		t.Error("0xA0 should be constructed (bit 6 set)")
	}
}

func TestTLV_Children_NonConstructed(t *testing.T) {
	tlv := &TLV{Tag: 0xE0, Value: []byte{0x01}}
	c := tlv.Children()
	if c != nil {
		t.Errorf("non-constructed: expected nil, got %d children", len(c))
	}
}

func TestTLV_Children_Constructed(t *testing.T) {
	// Constructed tag 0xA0 (context-specific, constructed)
	// Value contains two TLVs
	encoded, _ := Encode(0xA0, [][]byte{
		mustEncode(0xE1, []byte{0xAA}),
		mustEncode(0xE2, []byte{0xBB, 0xCC}),
	})
	tlv, _ := Decode(encoded)
	c := tlv.Children()
	if len(c) != 2 {
		t.Errorf("constructed children: want 2, got %d", len(c))
	}
}

// ─────────────────────────────────────────────────────────────
// Tag.String — uncovered branches
// ─────────────────────────────────────────────────────────────

func TestTagString_All(t *testing.T) {
	tests := []struct {
		tag   Tag
		want string
	}{
		{TagDeviceID, "DeviceID"},
		{TagVehicleID, "VehicleID"},
		{TagKeyID, "KeyID"},
		{TagCommandID, "CommandID"},
		{TagStatusCode, "StatusCode"},
		{TagTimestamp, "Timestamp"},
		{TagSignature, "Signature"},
		{TagPublicKey, "PublicKey"},
		{TagCertificate, "Certificate"},
		{TagCCCMsg, "CCCMsg"},
		{TagICCOAMsg, "ICCOAMsg"},
		{TagICCMsg, "ICCMsg"},
		{TagBleMac, "BleMac"},
		{TagUwbChannel, "UwbChannel"},
		{TagKeyType, "KeyType"},
		{TagAccessLevel, "AccessLevel"},
		{TagVersion, "Version"},
		{TagMessageType, "MessageType"},
		{TagSequenceNo, "SequenceNo"},
		{0xFFFF, "Unknown"},
	}
	for _, tc := range tests {
		got := tc.tag.String()
		if got != tc.want {
			t.Errorf("Tag(0x%X).String(): want %q, got %q", tc.tag, tc.want, got)
		}
	}
}

// ─────────────────────────────────────────────────────────────
// Tag helpers
// ─────────────────────────────────────────────────────────────

func TestTag_IsConstructed(t *testing.T) {
	if Tag(0x20).IsConstructed() {
		t.Log("0x20: constructed")
	} else {
		t.Error("0x20 should be constructed")
	}
	if Tag(0x00).IsConstructed() {
		t.Error("0x00 should not be constructed")
	}
}

func TestTag_GetClass(t *testing.T) {
	if Tag(0x80).GetClass() != TagClassContextSpec {
		t.Errorf("0x80 class: want 0x80, got 0x%X", Tag(0x80).GetClass())
	}
	if Tag(0x40).GetClass() != TagClassApplication {
		t.Errorf("0x40 class: want 0x40, got 0x%X", Tag(0x40).GetClass())
	}
	if Tag(0xC0).GetClass() != TagClassPrivate {
		t.Errorf("0xC0 class: want 0xC0, got 0x%X", Tag(0xC0).GetClass())
	}
	if Tag(0x00).GetClass() != TagClassUniversal {
		t.Errorf("0x00 class: want 0, got 0x%X", Tag(0x00).GetClass())
	}
}

func TestTag_GetTagNumber(t *testing.T) {
	if Tag(0x01).GetTagNumber() != 1 {
		t.Errorf("0x01 tag number: want 1, got %d", Tag(0x01).GetTagNumber())
	}
	if Tag(0x9F).GetTagNumber() != 0x1F {
		t.Errorf("0x9F tag number: want 0x1F, got 0x%X", Tag(0x9F).GetTagNumber())
	}
}

// ─────────────────────────────────────────────────────────────
// NewTLV — all value types
// ─────────────────────────────────────────────────────────────

func TestNewTLV_StringValue(t *testing.T) {
	tlv, err := NewTLV(0xE0, "hello")
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if string(tlv.Value) != "hello" {
		t.Errorf("value: want 'hello', got '%s'", string(tlv.Value))
	}
}

func TestNewTLV_IntValue(t *testing.T) {
	tests := []struct {
		name string
		val  interface{}
		n    int
	}{
		{"int(0)", int(0), 1},
		{"int(255)", int(255), 1},
		{"int(-1)", int(-1), 1},
		{"int8(127)", int8(127), 1},
		{"int8(-128)", int8(-128), 1},
		{"int16(0x1234)", int16(0x1234), 2},
		{"int16(-32768)", int16(-32768), 2},
		{"int32(0x12345678)", int32(0x12345678), 4},
		{"int32(-1)", int32(-1), 1}, // intSize(-1)=1
		{"int64(-1)", int64(-1), 1},
	}
	for _, tc := range tests {
		tlv, err := NewTLV(0xE0, tc.val)
		if err != nil {
			t.Errorf("%s: error: %v", tc.name, err)
			continue
		}
		if len(tlv.Value) != tc.n {
			t.Errorf("%s: value len=%d, want %d", tc.name, len(tlv.Value), tc.n)
		}
	}
}

func TestNewTLV_UintValue(t *testing.T) {
	tests := []struct {
		name string
		val  interface{}
		n    int
	}{
		{"uint(0)", uint(0), 1},
		{"uint8(0xFF)", uint8(0xFF), 1},
		{"uint16(0xABCD)", uint16(0xABCD), 2},
		{"uint32(0xDEADBEEF)", uint32(0xDEADBEEF), 4},
		{"uint64(0x0102030405060708)", uint64(0x0102030405060708), 8},
	}
	for _, tc := range tests {
		tlv, err := NewTLV(0xE0, tc.val)
		if err != nil {
			t.Errorf("%s: error: %v", tc.name, err)
			continue
		}
		if len(tlv.Value) != tc.n {
			t.Errorf("%s: value len=%d, want %d", tc.name, len(tlv.Value), tc.n)
		}
	}
}

func TestNewTLV_Unsupported(t *testing.T) {
	_, err := NewTLV(0xE0, struct{}{})
	if err == nil {
		t.Error("expected error for unsupported type")
	}
}

// ─────────────────────────────────────────────────────────────
// Encode — negative length edge case (writeLength error)
// ─────────────────────────────────────────────────────────────

func TestEncodeTLV_NegativeLength(t *testing.T) {
	_, err := EncodeTLV(0xE0, -1, []byte{})
	if err == nil {
		t.Error("expected error for negative length")
	}
}

// ─────────────────────────────────────────────────────────────
// Decode — multi-byte tag with 3-byte tag value
// ─────────────────────────────────────────────────────────────

func TestDecode_MultiByteTag3Bytes(t *testing.T) {
	// 3-byte tag: first byte has low5=0x1F, second byte has MSB=1 (continue),
	// third byte has MSB=0 (stop). [0x9F, 0x81, 0x02] → tag = 0x9F8102
	data := []byte{0x9F, 0x81, 0x02, 0x01, 0xAB}
	tlv, err := Decode(data)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if tlv.Tag != 0x9F8102 {
		t.Errorf("tag: want 0x9F8102, got 0x%X", tlv.Tag)
	}
}

// ─────────────────────────────────────────────────────────────
// Benchmarks — for code coverage stats
// ─────────────────────────────────────────────────────────────

func BenchmarkEncode_SingleTLV(b *testing.B) {
	for i := 0; i < b.N; i++ {
		Encode(0xE0, []byte{0x01, 0x02, 0x03, 0x04})
	}
}

func BenchmarkDecode_SingleTLV_Short(b *testing.B) {
	data := []byte{0xE0, 0x04, 0x01, 0x02, 0x03, 0x04}
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		Decode(data)
	}
}

// ─────────────────────────────────────────────────────────────
// writeLength — length > 0xFFFFFF error path
// ─────────────────────────────────────────────────────────────

func TestEncodeTLV_LengthTooLarge(t *testing.T) {
	// EncodeTLV with length > 0xFFFFFF (16777215)
	_, err := EncodeTLV(0xE0, 0x1000000, nil)
	if err == nil {
		t.Error("expected error for length > 0xFFFFFF")
	}
}

// ─────────────────────────────────────────────────────────────
// EncodeAll — error propagation
// ─────────────────────────────────────────────────────────────

func TestEncodeAll_ErrorPropagation(t *testing.T) {
	// EncodeAll with invalid tag that causes writeTag error…
	// (no invalid tag can trigger error with current writeTag; but we can test
	//  the length-too-large path via NewTLV bypass)
	tlvs := []*TLV{
		{Tag: 0xE0, Length: 0x1000000, Value: []byte{}},
	}
	_, err := EncodeAll(tlvs)
	if err == nil {
		t.Error("expected error for oversize length")
	}
}

// ─────────────────────────────────────────────────────────────
// Children — decode error path
// ─────────────────────────────────────────────────────────────

func TestTLV_Children_DecodeError(t *testing.T) {
	// Constructed tag with invalid inner data (truncated TLV)
	tlv := &TLV{Tag: 0xA0, Value: []byte{0xE0, 0x05, 0x01}} // expects 5 bytes value, only 1 provided
	c := tlv.Children()
	if c != nil {
		t.Errorf("expected nil for malformed inner data, got %d children", len(c))
	}
}

// ─────────────────────────────────────────────────────────────
// writeLength == 3-byte edge case (length just past 0xFFFF)
// ─────────────────────────────────────────────────────────────

func TestEncode_LengthExactly0x10000(t *testing.T) {
	v := make([]byte, 0x10000)
	v[0] = 0xAA
	raw, err := Encode(0xE0, v)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	// Tag(1) + 0x83(1) + 3-byte-len(3) + value(0x10000)
	if len(raw) != 1+1+3+0x10000 {
		t.Errorf("total length: want %d, got %d", 1+1+3+0x10000, len(raw))
	}
}

// ─────────────────────────────────────────────────────────────
// Decode — 4-byte tag (covers intToBytes 4-byte path)
// ─────────────────────────────────────────────────────────────

func TestDecode_MultiByteTag4Bytes(t *testing.T) {
	// 4-byte tag [0x9F, 0x81, 0x81, 0x01], length=1, value=[0xFF]
	data := []byte{0x9F, 0x81, 0x81, 0x01, 0x01, 0xFF}
	tlv, err := Decode(data)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if tlv.Tag != 0x9F818101 {
		t.Errorf("tag: want 0x9F818101, got 0x%X", tlv.Tag)
	}
	if len(tlv.Value) != 1 || tlv.Value[0] != 0xFF {
		t.Errorf("value: want [0xFF], got %x", tlv.Value)
	}
}

// ─────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────

func mustEncode(tag Tag, value interface{}) []byte {
	raw, err := Encode(tag, value)
	if err != nil {
		panic(err)
	}
	return raw
}
