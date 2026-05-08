package bertlv

import "testing"

// ─────────────────────────────────────────────────────────────
// Encode —— 基本编码
// ─────────────────────────────────────────────────────────────

func TestEncode_ByteTagShortLength(t *testing.T) {
	// Tag=0xE0 (single byte Private class), Length=3, Value=[0x01,0x02,0x03]
	raw, err := Encode(0xE0, []byte{0x01, 0x02, 0x03})
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if len(raw) != 5 {
		t.Errorf("want 5 bytes, got %d", len(raw))
	}
	if raw[0] != 0xE0 || raw[1] != 0x03 {
		t.Errorf("unexpected raw: %x", raw)
	}
	decoded, err := Decode(raw)
	if err != nil {
		t.Fatalf("roundtrip failed: %v", err)
	}
	if decoded.Tag != 0xE0 || decoded.Length != 3 {
		t.Errorf("roundtrip mismatch: got tag=0x%X len=%d", decoded.Tag, decoded.Length)
	}
}

func TestEncode_MaxShortLength(t *testing.T) {
	raw, err := Encode(0xE0, make([]byte, 127))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if raw[1] != 0x7F {
		t.Errorf("short length 127: want 0x7F, got 0x%02X", raw[1])
	}
}

func TestEncode_LongLength(t *testing.T) {
	raw, err := Encode(0xE0, make([]byte, 200))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if raw[1] != 0x81 || raw[2] != 0xC8 {
		t.Errorf("long length: want [81 C8], got [%02X %02X]", raw[1], raw[2])
	}
}

func TestEncode_StringValue(t *testing.T) {
	raw, err := Encode(0x01, "hello")
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	decoded, err := Decode(raw)
	if err != nil {
		t.Fatalf("roundtrip failed: %v", err)
	}
	if string(decoded.Value) != "hello" {
		t.Errorf("string value: want 'hello', got '%s'", string(decoded.Value))
	}
}

func TestEncode_Uint64(t *testing.T) {
	raw, err := Encode(0x10, uint64(0x123456789ABCDEF0))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	decoded, err := Decode(raw)
	if err != nil {
		t.Fatalf("roundtrip failed: %v", err)
	}
	if len(decoded.Value) != 8 {
		t.Errorf("uint64 length: want 8, got %d", len(decoded.Value))
	}
}

func TestEncode_Int64(t *testing.T) {
	raw, err := Encode(0x11, int64(-1))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if len(raw) != 10 {
		t.Errorf("int64(-1) length: want 10 (Tag+Len+8), got %d", len(raw))
	}
	decoded, err := Decode(raw)
	if err != nil {
		t.Fatalf("roundtrip failed: %v", err)
	}
	if len(decoded.Value) != 8 {
		t.Errorf("int64 length: want 8, got %d", len(decoded.Value))
	}
}

func TestEncode_MultiByteTag(t *testing.T) {
	// Tag=0x9F01: Private class (b8-b7=10), tag > 31
	// Encoder writes [9F 01], Decoder reads [9F 01] → 0x9F01
	raw, err := Encode(Tag(0x9F01), []byte{0x01})
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	t.Logf("Encode(0x9F01, [01]) = % X (len=%d)", raw, len(raw))
	decoded, err := Decode(raw)
	if err != nil {
		t.Fatalf("decode failed: %v", err)
	}
	if decoded.Tag != Tag(0x9F01) {
		t.Errorf("roundtrip tag mismatch: want 0x9F01, got 0x%X", decoded.Tag)
	}
}

// ─────────────────────────────────────────────────────────────
// EncodeTLV —— 显式长度编码
// ─────────────────────────────────────────────────────────────

func TestEncodeTLV_Basic(t *testing.T) {
	raw, err := EncodeTLV(0xE0, 3, []byte{0x01, 0x02, 0x03})
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if len(raw) != 5 {
		t.Errorf("want 5 bytes, got %d", len(raw))
	}
	if raw[0] != 0xE0 || raw[1] != 0x03 {
		t.Errorf("unexpected raw: %x", raw)
	}
}

// ─────────────────────────────────────────────────────────────
// EncodeAll —— 批量编码
// ─────────────────────────────────────────────────────────────

func TestEncodeAll_Basic(t *testing.T) {
	tlvs := []*TLV{
		{Tag: 0xE0, Length: 3, Value: []byte{0x01, 0x02, 0x03}},
		{Tag: 0xE1, Length: 2, Value: []byte{0x04, 0x05}},
	}
	raw, err := EncodeAll(tlvs)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	// Tag(1)+Len(1)+Val(3) + Tag(1)+Len(1)+Val(2) = 5+4 = 9
	if len(raw) != 9 {
		t.Errorf("want 9 bytes, got %d", len(raw))
	}
}

func TestEncodeAll_Empty(t *testing.T) {
	raw, err := EncodeAll([]*TLV{})
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if len(raw) != 0 {
		t.Errorf("empty EncodeAll: want 0 bytes, got %d", len(raw))
	}
}

// ─────────────────────────────────────────────────────────────
// NewTLV —— 创建TLV对象
// ─────────────────────────────────────────────────────────────

func TestNewTLV(t *testing.T) {
	tlv, err := NewTLV(0xE0, []byte{0x01, 0x02, 0x03})
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if tlv.Tag != 0xE0 || tlv.Length != 3 {
		t.Errorf("NewTLV: tag=0x%X len=%d", tlv.Tag, tlv.Length)
	}
}

// ─────────────────────────────────────────────────────────────
// DeepCopy —— 深拷贝
// ─────────────────────────────────────────────────────────────

func TestTLV_DeepCopy(t *testing.T) {
	tlv := &TLV{Tag: 0xE0, Length: 3, Value: []byte{0x01, 0x02, 0x03}}
	c := tlv.DeepCopy()
	c.Value[0] = 0xFF
	if tlv.Value[0] == 0xFF {
		t.Error("DeepCopy did not create independent copy")
	}
}

// ─────────────────────────────────────────────────────────────
// Encoder 实例
// ─────────────────────────────────────────────────────────────

func TestNewEncoder(t *testing.T) {
	e := NewEncoder()
	if e == nil {
		t.Fatal("NewEncoder returned nil")
	}
	if e.buf == nil {
		t.Error("buf is nil")
	}
}

func TestEncoder_Reset(t *testing.T) {
	e := NewEncoder()
	Encode(0xE0, []byte{0x01})
	e.buf.Reset()
	if e.buf.Len() != 0 {
		t.Errorf("after Reset: buf.Len()=%d, want 0", e.buf.Len())
	}
}

// ─────────────────────────────────────────────────────────────
// 边界条件
// ─────────────────────────────────────────────────────────────

func TestEncode_EmptyValue(t *testing.T) {
	raw, err := Encode(0xE0, []byte{})
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	// Tag(1) + Length(1) + Value(0) = 2
	if len(raw) != 2 {
		t.Errorf("empty value: want 2 bytes, got %d", len(raw))
	}
}

func TestEncode_BufferGrowing(t *testing.T) {
	large := make([]byte, 1000)
	for i := range large {
		large[i] = byte(i)
	}
	raw, err := Encode(0xE0, large)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	// Tag(1) + LenLongForm3bytes(1+2=3) + Value(1000) = 1004
	if len(raw) != 1004 {
		t.Errorf("large value: want 1004 bytes, got %d", len(raw))
	}
}

func TestEncode_MultiByteTag_ISO7816_4(t *testing.T) {
	// TagICCOAMsg = 0x9F01
	raw, err := Encode(TagICCOAMsg, []byte{0x01})
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	decoded, err := Decode(raw)
	if err != nil {
		t.Fatalf("decode failed: %v", err)
	}
	if decoded.Tag != TagICCOAMsg {
		t.Errorf("tag mismatch: want 0x%X, got 0x%X", TagICCOAMsg, decoded.Tag)
	}
}

func TestRoundtrip_RandomTags(t *testing.T) {
	// Single-byte tags only (0x00-0x1F, 0x80-0xBF, 0xC0-0xFF)
	tags := []Tag{
		0x01, 0x1F, 0x20, 0x7F,   // Universal class
		0x80, 0x9E, 0x9F, 0xC0,   // Context/Private class
		0xE0, 0xFF, 0xE1, 0xE2,   // Private class
	}
	for _, tag := range tags {
		raw, err := Encode(tag, []byte{0xAB, 0xCD})
		if err != nil {
			t.Errorf("tag 0x%04X: Encode error: %v", tag, err)
			continue
		}
		decoded, err := Decode(raw)
		if err != nil {
			t.Errorf("tag 0x%04X: Decode error: %v", tag, err)
			continue
		}
		if decoded.Tag != tag {
			t.Errorf("tag 0x%04X: roundtrip mismatch, got 0x%04X", tag, decoded.Tag)
		}
	}
}
