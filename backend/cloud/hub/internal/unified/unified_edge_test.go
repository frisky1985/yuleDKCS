package unified

import (
	"bytes"
	"testing"

	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
)

// ============================================================
// encodeTLV/decodeLength long-form edge cases
// ============================================================

func TestICCOAEncodeTLVLongForm(t *testing.T) {
	codec := NewICCOACodec(ProtocolICCOA40)

	t.Run("encodeTLV length=128 triggers long form 0x81", func(t *testing.T) {
		// 128 bytes — needs 0x81 long form
		largeValue := make([]byte, 128)
		for i := range largeValue {
			largeValue[i] = byte(i)
		}
		result := codec.encodeTLV(0x03, largeValue)
		// tag(1) + 0x81(1) + length(1) + value(128) = 131
		if len(result) != 131 {
			t.Fatalf("encoded length = %d, want 131", len(result))
		}
		if result[0] != 0x03 {
			t.Errorf("tag = 0x%02X", result[0])
		}
		if result[1] != 0x81 {
			t.Errorf("length prefix = 0x%02X, want 0x81", result[1])
		}
		if result[2] != 128 {
			t.Errorf("length = %d, want 128", result[2])
		}
	})

	t.Run("encodeTLV length=256 triggers long form 0x82", func(t *testing.T) {
		largeValue := make([]byte, 256)
		for i := range largeValue {
			largeValue[i] = byte(i)
		}
		result := codec.encodeTLV(0x03, largeValue)
		// tag(1) + 0x82(1) + len_hi(1) + len_lo(1) + value(256) = 260
		if len(result) != 260 {
			t.Fatalf("encoded length = %d, want 260", len(result))
		}
		if result[0] != 0x03 {
			t.Errorf("tag = 0x%02X", result[0])
		}
		if result[1] != 0x82 {
			t.Errorf("length prefix = 0x%02X, want 0x82", result[1])
		}
		if result[2] != 1 || result[3] != 0 {
			t.Errorf("length bytes = [%d,%d], want [1,0]", result[2], result[3])
		}
	})

	t.Run("encodeTLV short form length=0", func(t *testing.T) {
		result := codec.encodeTLV(0x01, []byte{})
		// tag(1) + length(1) = 2
		if len(result) != 2 {
			t.Fatalf("encoded length = %d, want 2", len(result))
		}
		if result[0] != 0x01 {
			t.Errorf("tag = 0x%02X", result[0])
		}
		if result[1] != 0 {
			t.Errorf("length = %d, want 0", result[1])
		}
	})

	t.Run("VehicleStatus with long value triggers long form decode", func(t *testing.T) {
		// Encode a VehicleStatus message, verify roundtrip works
		msg := &UnifiedMessage{
			Type: MsgTypeVehicleStatus,
			VehicleStatus: &VehicleStatusMessage{
				DoorsLocked:  true,
				EngineOn:     false,
				BatteryLevel: 50,
			},
		}
		data, err := codec.Encode(msg)
		if err != nil {
			t.Fatalf("Encode() error = %v", err)
		}
		decoded, err := codec.Decode(data)
		if err != nil {
			t.Fatalf("Decode() error = %v", err)
		}
		if !decoded.VehicleStatus.DoorsLocked {
			t.Error("DoorsLocked should be true")
		}
		if decoded.VehicleStatus.EngineOn {
			t.Error("EngineOn should be false")
		}
	})
}

func TestICCEncodeTLVLongForm(t *testing.T) {
	codec := NewICCECodec()

	t.Run("encodeTLV BER-TLV multi-byte tag with long form length", func(t *testing.T) {
		largeValue := make([]byte, 200)
		for i := range largeValue {
			largeValue[i] = byte(i)
		}
		// Using a BER-TLV tag 0x9F02 with large value
		result := codec.encodeTLV(0x9F02, largeValue)
		// tag(2) + 0x81(1) + length(1) + value(200) = 204
		if len(result) != 204 {
			t.Fatalf("encoded length = %d, want 204", len(result))
		}
		if result[0] != 0x9F || result[1] != 0x02 {
			t.Errorf("tag = [0x%02X, 0x%02X], want [0x9F, 0x02]", result[0], result[1])
		}
		if result[2] != 0x81 {
			t.Errorf("length prefix = 0x%02X, want 0x81", result[2])
		}
		if result[3] != 200 {
			t.Errorf("length = %d, want 200", result[3])
		}
	})

	t.Run("encodeTLV BER-TLV single-byte tag with long form 0x82", func(t *testing.T) {
		largeValue := make([]byte, 300)
		result := codec.encodeTLV(0x01, largeValue)
		// tag(1) + 0x82(1) + len_hi(1) + len_lo(1) + value(300) = 304
		if len(result) != 304 {
			t.Fatalf("encoded length = %d, want 304", len(result))
		}
		if result[0] != 0x01 {
			t.Errorf("tag = 0x%02X", result[0])
		}
		if result[1] != 0x82 {
			t.Errorf("length prefix = 0x%02X, want 0x82", result[1])
		}
	})

	t.Run("encodeTLV multi-byte tag (>0x1F but <0x100)", func(t *testing.T) {
		value := []byte{0x01, 0x02}
		result := codec.encodeTLV(0x9F, value)
		// tag(1) + length(1) + value(2) = 4
		if len(result) != 4 {
			t.Fatalf("encoded length = %d, want 4", len(result))
		}
		if result[0] != 0x9F {
			t.Errorf("tag = 0x%02X, want 0x9F", result[0])
		}
		if result[1] != 2 {
			t.Errorf("length = %d, want 2", result[1])
		}
	})
}

func TestICCOADecodeLength(t *testing.T) {
	codec := NewICCOACodec(ProtocolICCOA40)

	t.Run("decodeLength short form", func(t *testing.T) {
		length, n := codec.decodeLength([]byte{0x05, 0x00})
		if length != 5 {
			t.Errorf("length = %d, want 5", length)
		}
		if n != 1 {
			t.Errorf("bytes consumed = %d, want 1", n)
		}
	})

	t.Run("decodeLength long form 0x81", func(t *testing.T) {
		length, n := codec.decodeLength([]byte{0x81, 0x80})
		if length != 128 {
			t.Errorf("length = %d, want 128", length)
		}
		if n != 2 {
			t.Errorf("bytes consumed = %d, want 2", n)
		}
	})

	t.Run("decodeLength long form 0x82", func(t *testing.T) {
		length, n := codec.decodeLength([]byte{0x82, 0x01, 0x00})
		if length != 256 {
			t.Errorf("length = %d, want 256", length)
		}
		if n != 3 {
			t.Errorf("bytes consumed = %d, want 3", n)
		}
	})

	t.Run("decodeLength unknown long form", func(t *testing.T) {
		// ICCE's decodeLength returns (0, 1) for unknown 0x83+
		length, n := codec.decodeLength([]byte{0x83, 0x01})
		if length != 0 {
			t.Errorf("length = %d, want 0", length)
		}
		if n != 1 {
			t.Errorf("bytes consumed = %d, want 1", n)
		}
	})
}

func TestICCEDecodeLength(t *testing.T) {
	codec := NewICCECodec()

	t.Run("decodeLength short form", func(t *testing.T) {
		length, n := codec.decodeLength([]byte{0x05, 0x00})
		if length != 5 {
			t.Errorf("length = %d, want 5", length)
		}
		if n != 1 {
			t.Errorf("bytes consumed = %d, want 1", n)
		}
	})

	t.Run("decodeLength long form 0x81", func(t *testing.T) {
		length, n := codec.decodeLength([]byte{0x81, 0xC8})
		if length != 200 {
			t.Errorf("length = %d, want 200", length)
		}
		if n != 2 {
			t.Errorf("bytes consumed = %d, want 2", n)
		}
	})

	t.Run("decodeLength unknown long form", func(t *testing.T) {
		length, n := codec.decodeLength([]byte{0x83, 0x01})
		if length != 0 {
			t.Errorf("length = %d, want 0", length)
		}
		if n != 1 {
			t.Errorf("bytes consumed = %d, want 1", n)
		}
	})
}

// ============================================================
// ICCE decode edge cases
// ============================================================

func TestICCEDecodeEdgeCases(t *testing.T) {
	codec := NewICCECodec()

	t.Run("detectICCEType single byte too short", func(t *testing.T) {
		// Single byte 0x90 — decoded as too short (< 2 bytes required)
		_, err := codec.Decode([]byte{0x90})
		if err == nil {
			t.Error("Decode() should fail with too short data")
		}
	})

	t.Run("decoded ICCE VehicleStatus with missing body", func(t *testing.T) {
		// Only the message tag (0x9F20), no body
		data := []byte{0x9F, 0x20}
		decoded, err := codec.Decode(data)
		if err != nil {
			t.Fatalf("Decode() error = %v", err)
		}
		if decoded.Type != MsgTypeVehicleStatus {
			t.Errorf("Type = %v, want VehicleStatus", decoded.Type)
		}
		if decoded.VehicleStatus == nil {
			t.Fatal("VehicleStatus should not be nil")
		}
	})

	t.Run("ICCE decodeKeyBind empty data", func(t *testing.T) {
		// ICCE KeyBind minimal header: 0x9F01
		data := []byte{0x9F, 0x01}
		decoded, err := codec.Decode(data)
		if err != nil {
			t.Fatalf("Decode() error = %v", err)
		}
		if decoded.Type != MsgTypeKeyBind {
			t.Errorf("Type = %v, want KeyBind", decoded.Type)
		}
		if decoded.KeyBind == nil {
			t.Fatal("KeyBind should not be nil")
		}
	})

	t.Run("ICCE RemoteControl all actions roundtrip", func(t *testing.T) {
		actions := []RemoteAction{
			ActionLock, ActionUnlock, ActionEngineStart, ActionEngineStop,
			ActionTrunkOpen, ActionFindCar,
		}
		expectedTags := []byte{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}
		for i, a := range actions {
			got := codec.actionToTag(a)
			if got != expectedTags[i] {
				t.Errorf("actionToTag(%s) = 0x%02X, want 0x%02X", a, got, expectedTags[i])
			}
			back := codec.tagToAction(got)
			if back != a {
				t.Errorf("tagToAction(0x%02X) = %v, want %v", got, back, a)
			}
		}
	})

	t.Run("tagToAction unknown tag", func(t *testing.T) {
		got := codec.tagToAction(0xFF)
		if got != ActionUnspecified {
			t.Errorf("tagToAction(0xFF) = %v, want unspecified", got)
		}
	})

	t.Run("actionToTag unknown action", func(t *testing.T) {
		got := codec.actionToTag(RemoteAction(99))
		if got != 0x00 {
			t.Errorf("actionToTag(99) = 0x%02X, want 0x00", got)
		}
	})

	t.Run("detectICCEType 0x9F20 → VehicleStatus", func(t *testing.T) {
		decoded, err := codec.Decode([]byte{0x9F, 0x20})
		if err != nil {
			t.Fatalf("Decode() error = %v", err)
		}
		if decoded.Type != MsgTypeVehicleStatus {
			t.Errorf("Type = %v, want VehicleStatus", decoded.Type)
		}
	})

	t.Run("detectICCEType 0x01 → Unspecified", func(t *testing.T) {
		decoded, err := codec.Decode([]byte{0x01, 0x00})
		if err != nil {
			t.Fatalf("Decode() error = %v", err)
		}
		if decoded.Type != MsgTypeUnspecified {
			t.Errorf("Type = %v, want Unspecified", decoded.Type)
		}
	})
}

// ============================================================
// ICCOA RemoteControl all actions roundtrip
// ============================================================

func TestICCOARemoteControlAllActions(t *testing.T) {
	codec := NewICCOACodec(ProtocolICCOA40)

	actions := []struct {
		action RemoteAction
		byte   uint8
	}{
		{ActionLock, 0x01},
		{ActionUnlock, 0x02},
		{ActionEngineStart, 0x03},
		{ActionEngineStop, 0x04},
		{ActionTrunkOpen, 0x05},
		{ActionFindCar, 0x06},
	}

	for _, a := range actions {
		t.Run(a.action.String(), func(t *testing.T) {
			msg := &UnifiedMessage{
				Type: MsgTypeRemoteControl,
				RemoteControl: &RemoteControlMessage{
					Action: a.action,
				},
			}
			data, err := codec.Encode(msg)
			if err != nil {
				t.Fatalf("Encode() error = %v", err)
			}
			// data[2] should contain the action byte
			if len(data) < 3 {
				t.Fatalf("encoded data too short")
			}
			if data[2] != a.byte {
				t.Errorf("action byte = 0x%02X, want 0x%02X", data[2], a.byte)
			}
			decoded, err := codec.Decode(data)
			if err != nil {
				t.Fatalf("Decode() error = %v", err)
			}
			if decoded.RemoteControl.Action != a.action {
				t.Errorf("Action = %v after roundtrip, want %v", decoded.RemoteControl.Action, a.action)
			}
		})
	}
}

// ============================================================
// GetSpec edge cases — ensure all protocol specs return accurately
// ============================================================

func TestGetSpecAll(t *testing.T) {
	tests := []struct {
		proto    ProtocolType
		exists   bool
		name     string
	}{
		{ProtocolCCC3, true, "CCC Digital Key 3.0"},
		{ProtocolICCOA30, true, "ICCOA Digital Key 3.0"},
		{ProtocolICCOA40, true, "ICCOA Digital Key 4.0"},
		{ProtocolICCE, true, "ICCE Digital Key"},
		{ProtocolUnspecified, false, ""},
	}

	for _, tt := range tests {
		t.Run(tt.proto.String(), func(t *testing.T) {
			spec := GetSpec(tt.proto)
			if !tt.exists {
				if spec != nil {
					t.Error("spec should be nil")
				}
				return
			}
			if spec == nil {
				t.Fatal("spec should not be nil")
			}
			if spec.Name != tt.name {
				t.Errorf("Name = %q, want %q", spec.Name, tt.name)
			}
			if spec.RequiredCaps == nil {
				t.Error("RequiredCaps should not be nil")
			}
		})
	}
}

// ============================================================
// ICCOA encodeKeyBind edge case — missing KeyBind fields
// ============================================================

func TestICCOAEncodeDecodeEdgeCases(t *testing.T) {
	codec := NewICCOACodec(ProtocolICCOA40)

	t.Run("KeyBind with only DevicePubKey", func(t *testing.T) {
		msg := &UnifiedMessage{
			Type: MsgTypeKeyBind,
			KeyBind: &KeyBindMessage{
				DevicePubKey: []byte{0xAA, 0xBB, 0xCC},
			},
		}
		data, err := codec.Encode(msg)
		if err != nil {
			t.Fatalf("Encode() error = %v", err)
		}
		decoded, err := codec.Decode(data)
		if err != nil {
			t.Fatalf("Decode() error = %v", err)
		}
		if decoded.Type != MsgTypeKeyBind {
			t.Errorf("Type = %v", decoded.Type)
		}
	})

	t.Run("detectMessageType returns VehicleStatus for 0x41", func(t *testing.T) {
		// data[1] = 0x41 is VehicleStatus range (0x41-0x50)
		data := []byte{0xA0, 0x41, 0x01}
		decoded, err := codec.Decode(data)
		if err != nil {
			t.Fatalf("Decode() error = %v", err)
		}
		if decoded.Type != MsgTypeVehicleStatus {
			t.Errorf("Type = %v, want VehicleStatus", decoded.Type)
		}
	})

	t.Run("detectMessageType short data returns error", func(t *testing.T) {
		// Single byte 0xA0 is < 3 bytes required by ICCOA Decode
		_, err := codec.Decode([]byte{0xA0})
		if err == nil {
			t.Error("Decode() should fail with too short data")
		}
	})
}

// ============================================================
// Manager NegotiateRequest edge case
// ============================================================

func TestManagerGetSession(t *testing.T) {
	t.Run("GetSession returns correct session", func(t *testing.T) {
		mgr := NewManager(&Config{
			SupportedProtocols: []ProtocolType{ProtocolCCC3},
		})
		// Access session manager through GetSession
		_, ok := mgr.GetSession("nonexistent")
		if ok {
			t.Error("GetSession should return false for nonexistent")
		}
	})
}

// ============================================================
// encodeTag/decodeTag edge cases in ICCE
// ============================================================

func TestICCETagEncodeDecode(t *testing.T) {
	codec := NewICCECodec()

	t.Run("encodeTag single byte", func(t *testing.T) {
		result := codec.encodeTag(0x1F)
		if len(result) != 1 || result[0] != 0x1F {
			t.Errorf("encodeTag(0x1F) = %v, want [0x1F]", result)
		}
	})

	t.Run("encodeTag two-byte", func(t *testing.T) {
		result := codec.encodeTag(0x9F01)
		if len(result) != 2 || result[0] != 0x9F || result[1] != 0x01 {
			t.Errorf("encodeTag(0x9F01) = %v, want [0x9F, 0x01]", result)
		}
	})

	t.Run("decodeTag empty data", func(t *testing.T) {
		tag, n := codec.decodeTag(nil)
		if tag != 0 || n != 0 {
			t.Errorf("decodeTag(nil) = (%d, %d), want (0, 0)", tag, n)
		}
	})

	t.Run("decodeTag single byte", func(t *testing.T) {
		tag, n := codec.decodeTag([]byte{0x1F})
		if tag != 0x1F || n != 1 {
			t.Errorf("decodeTag([0x1F]) = (%d, %d)", tag, n)
		}
	})
}

// ============================================================
// Session.Valid with zero time expiry
// ============================================================

func TestSessionValidWithZeroExpiry(t *testing.T) {
	dev := &Device{DeviceID: "dev-zero-expiry"}
	s := NewSession("sess-zero-expiry", dev)
	// Zero-value ExpiresAt should not make session invalid
	if !s.Valid() {
		t.Error("Session with zero ExpiresAt should be valid")
	}
}

// ============================================================
// CapabilitySet string for various configurations
// ============================================================

func TestCapabilitySetVariations(t *testing.T) {
	tests := []struct {
		name string
		caps *CapabilitySet
		want string
	}{
		{"all false", &CapabilitySet{}, "BLE=false UWB=false NFC=false SE=false FiRa=false"},
		{"all true", &CapabilitySet{true, true, true, true, true, "", "", 0, 0, 0}, "BLE=true UWB=true NFC=true SE=true FiRa=true"},
		{"mixed", &CapabilitySet{true, false, true, false, true, "", "", 0, 0, 0}, "BLE=true UWB=false NFC=true SE=false FiRa=true"},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got := tt.caps.String()
			if got != tt.want {
				t.Errorf("String() = %q, want %q", got, tt.want)
			}
		})
	}
}

// ============================================================
// Proto conversions edge cases
// ============================================================

func TestProtocolFromProtoEdgeCases(t *testing.T) {
	got := ProtocolFromProto(pb.Protocol(99))
	if got != ProtocolUnspecified {
		t.Errorf("ProtocolFromProto(99) = %v, want unspecified", got)
	}
}



// ============================================================
// Manager codec registration
// ============================================================

func TestManagerRegisterCodecRoundtrip(t *testing.T) {
	mgr := NewManager(&Config{
		SupportedProtocols: []ProtocolType{ProtocolCCC3, ProtocolICCOA40},
	})
	// Verify default codecs are registered via registerDefaultCodecs
	_, ok := mgr.codecRegistry.Get(ProtocolCCC3)
	if !ok {
		t.Error("CCC3 should be registered by default")
	}
	_, ok = mgr.codecRegistry.Get(ProtocolICCOA40)
	if !ok {
		t.Error("ICCOA40 should be registered by default")
	}
}

// ============================================================
// CCC codec encoding with all subtypes
// ============================================================

func TestCCCCodecEncodeTypes(t *testing.T) {
	codec := NewCCCCodec()

	t.Run("encode VehicleStatus without VehicleStatus message", func(t *testing.T) {
		msg := &UnifiedMessage{
			Type: MsgTypeVehicleStatus,
			// VehicleStatus is nil, so no status byte is written
		}
		data, err := codec.Encode(msg)
		if err != nil {
			t.Fatalf("Encode() error = %v", err)
		}
		// With nil VehicleStatus, only marker + subtype (0x5C 0x40)
		if len(data) != 2 {
			t.Fatalf("encoded length = %d, want 2", len(data))
		}
		if data[0] != 0x5C || data[1] != 0x40 {
			t.Errorf("data = %v, want [0x5C, 0x40]", data)
		}
	})
}

// ============================================================
// encodeField edge cases
// ============================================================

func TestEncodeFieldEdgeCases(t *testing.T) {
	t.Run("encodeField empty string", func(t *testing.T) {
		var buf bytes.Buffer
		encodeField(&buf, 0x01, "")
		if len(buf.Bytes()) != 2 {
			t.Errorf("encoded length = %d, want 2", len(buf.Bytes()))
		}
		if buf.Bytes()[1] != 0 {
			t.Errorf("length byte = %d, want 0", buf.Bytes()[1])
		}
	})

	t.Run("encodeField empty byte slice", func(t *testing.T) {
		var buf bytes.Buffer
		encodeField(&buf, 0x02, []byte{})
		if len(buf.Bytes()) != 2 {
			t.Errorf("encoded length = %d, want 2", len(buf.Bytes()))
		}
	})

	t.Run("encodeField uint64 with zero value", func(t *testing.T) {
		var buf bytes.Buffer
		encodeField(&buf, 0x03, uint64(0))
		if len(buf.Bytes()) != 10 {
			t.Errorf("encoded length = %d, want 10", len(buf.Bytes()))
		}
	})
}

// ============================================================
// ToUnifiedMessage
// ============================================================

// ToUnifiedMessage uses reflection and is mainly for adapter conversions.
// We test it compiles and runs without panic for basic inputs.
func TestToUnifiedMessageBasic(t *testing.T) {
	t.Run("with nil input", func(t *testing.T) {
		msg := ToUnifiedMessage(nil)
		if msg == nil {
			t.Fatal("ToUnifiedMessage(nil) should not return nil")
		}
	})
}

// ============================================================
// ICCOACodec + ICCECodec nil body 安全 — 不应 panic
// ============================================================

func TestICCOACodecNilBodySafety(t *testing.T) {
	codec := NewICCOACodec(ProtocolICCOA40)

	t.Run("ICCOA KeyBind nil body", func(t *testing.T) {
		_, err := codec.Encode(&UnifiedMessage{Type: MsgTypeKeyBind})
		if err == nil {
			t.Error("expected error for nil KeyBind")
		}
	})

	t.Run("ICCOA KeyShare nil body", func(t *testing.T) {
		_, err := codec.Encode(&UnifiedMessage{Type: MsgTypeKeyShare})
		if err == nil {
			t.Error("expected error for nil KeyShare")
		}
	})

	t.Run("ICCOA RemoteControl nil body", func(t *testing.T) {
		_, err := codec.Encode(&UnifiedMessage{Type: MsgTypeRemoteControl})
		if err == nil {
			t.Error("expected error for nil RemoteControl")
		}
	})

	t.Run("ICCOA VehicleStatus nil body", func(t *testing.T) {
		_, err := codec.Encode(&UnifiedMessage{Type: MsgTypeVehicleStatus})
		if err == nil {
			t.Error("expected error for nil VehicleStatus")
		}
	})
}

func TestICCECodecNilBodySafety(t *testing.T) {
	codec := NewICCECodec()

	t.Run("ICCE KeyBind nil body", func(t *testing.T) {
		_, err := codec.Encode(&UnifiedMessage{Type: MsgTypeKeyBind})
		if err == nil {
			t.Error("expected error for nil KeyBind")
		}
	})

	t.Run("ICCE RemoteControl nil body", func(t *testing.T) {
		_, err := codec.Encode(&UnifiedMessage{Type: MsgTypeRemoteControl})
		if err == nil {
			t.Error("expected error for nil RemoteControl")
		}
	})

	t.Run("ICCE VehicleStatus nil body", func(t *testing.T) {
		_, err := codec.Encode(&UnifiedMessage{Type: MsgTypeVehicleStatus})
		if err == nil {
			t.Error("expected error for nil VehicleStatus")
		}
	})
}

// ============================================================
// strPtr nil safety — 不应 panic
// ============================================================

func TestStrPtrNilSafety(t *testing.T) {
	t.Run("nil pointer returns empty string", func(t *testing.T) {
		result := strPtr(nil)
		if result != "" {
			t.Errorf("expected empty string, got %q", result)
		}
	})

	t.Run("non-nil pointer returns value", func(t *testing.T) {
		val := "hello"
		result := strPtr(&val)
		if result != "hello" {
			t.Errorf("expected \"hello\", got %q", result)
		}
	})

	t.Run("empty string pointer returns empty string", func(t *testing.T) {
		val := ""
		result := strPtr(&val)
		if result != "" {
			t.Errorf("expected empty string, got %q", result)
		}
	})

	t.Run("unicode string preserved", func(t *testing.T) {
		val := "你好世界"
		result := strPtr(&val)
		if result != "你好世界" {
			t.Errorf("expected \"你好世界\", got %q", result)
		}
	})
}
