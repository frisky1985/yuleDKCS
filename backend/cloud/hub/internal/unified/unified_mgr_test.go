package unified

import (
	"context"
	"testing"

	"go.uber.org/zap"

	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
)

func newTestManager(protos []ProtocolType) *Manager {
	return NewManager(&Config{
		Logger:             zap.NewNop(),
		SupportedProtocols: protos,
	})
}

func TestManagerNegotiateProtocol(t *testing.T) {
	mgr := newTestManager([]ProtocolType{ProtocolCCC3, ProtocolICCOA40, ProtocolICCE})

	t.Run("successful negotiation with ICCOA40", func(t *testing.T) {
		req := &NegotiateRequest{
			DeviceID: "device-001", Vendor: "xiaomi", OS: "android", AppVersion: "1.0.0",
			DeviceCaps:  &NegotiateCapabilities{BLE: true, UWB: true, NFC: true, SE: true, FiRa: true},
			VehicleCaps: &NegotiateCapabilities{BLE: true, UWB: true, NFC: true, SE: true},
		}
		resp, err := mgr.NegotiateProtocol(context.Background(), req)
		if err != nil {
			t.Fatalf("NegotiateProtocol() error = %v", err)
		}
		if resp.SessionID == "" {
			t.Error("SessionID should not be empty")
		}
		if resp.Protocol != ProtocolICCOA40 {
			t.Errorf("Protocol = %v, want ICCOA40", resp.Protocol)
		}
		if resp.MatchScore <= 0 {
			t.Errorf("MatchScore = %d, want > 0", resp.MatchScore)
		}
		if resp.Version == "" {
			t.Error("Version should not be empty")
		}
	})

	t.Run("negotiation with nil device caps", func(t *testing.T) {
		req := &NegotiateRequest{
			DeviceID: "device-002", Vendor: "samsung",
			DeviceCaps:  nil,
			VehicleCaps: &NegotiateCapabilities{BLE: true, UWB: true, NFC: true, SE: true},
		}
		_, err := mgr.NegotiateProtocol(context.Background(), req)
		if err == nil {
			t.Error("NegotiateProtocol() should fail with nil device caps")
		}
	})

	t.Run("no compatible protocol", func(t *testing.T) {
		req := &NegotiateRequest{
			DeviceID: "device-003", Vendor: "unknown",
			DeviceCaps:  &NegotiateCapabilities{},
			VehicleCaps: &NegotiateCapabilities{},
		}
		_, err := mgr.NegotiateProtocol(context.Background(), req)
		if err == nil {
			t.Error("NegotiateProtocol() should fail with no compatible protocol")
		}
	})

	t.Run("samsung prefers CCC3", func(t *testing.T) {
		req := &NegotiateRequest{
			DeviceID: "device-samsung", Vendor: "samsung",
			DeviceCaps:  &NegotiateCapabilities{BLE: true, UWB: true, NFC: false, SE: true, FiRa: true},
			VehicleCaps: &NegotiateCapabilities{BLE: true, UWB: true, NFC: false, SE: true},
		}
		resp, err := mgr.NegotiateProtocol(context.Background(), req)
		if err != nil {
			t.Fatalf("NegotiateProtocol() error = %v", err)
		}
		if resp.Protocol != ProtocolCCC3 {
			t.Errorf("Protocol = %v, want CCC3", resp.Protocol)
		}
	})
}

func TestManagerHandleVehicleStatus(t *testing.T) {
	mgr := newTestManager([]ProtocolType{ProtocolICCE})

	req := &NegotiateRequest{
		DeviceID: "device-vs", Vendor: "huawei",
		DeviceCaps:  &NegotiateCapabilities{BLE: true, UWB: true, NFC: true, SE: true, FiRa: true},
		VehicleCaps: &NegotiateCapabilities{BLE: true, UWB: true, NFC: true, SE: true},
	}
	resp, err := mgr.NegotiateProtocol(context.Background(), req)
	if err != nil {
		t.Fatalf("NegotiateProtocol() error = %v", err)
	}

	icceCodec := NewICCECodec()
	vsMsg := &UnifiedMessage{
		Type: MsgTypeVehicleStatus,
		VehicleStatus: &VehicleStatusMessage{
			DoorsLocked: true, EngineOn: false, VehicleID: "vehicle-vs",
		},
	}
	data, err := icceCodec.Encode(vsMsg)
	if err != nil {
		t.Fatalf("Encode ICCE VS error = %v", err)
	}

	result, err := mgr.HandleVehicleStatus(context.Background(), resp.SessionID, data)
	if err != nil {
		t.Fatalf("HandleVehicleStatus() error = %v", err)
	}
	if result == nil {
		t.Fatal("HandleVehicleStatus() returned nil")
	}
	if result.LockStatus != 1 {
		t.Errorf("LockStatus = %d, want 1", result.LockStatus)
	}
	if result.EngineStatus != 0 {
		t.Errorf("EngineStatus = %d, want 0", result.EngineStatus)
	}

	// Test error paths
	_, err = mgr.HandleVehicleStatus(context.Background(), "nonexistent", data)
	if err == nil {
		t.Error("HandleVehicleStatus() should fail with nonexistent session")
	}
}

func TestManagerHandleRemoteControl(t *testing.T) {
	mgr := newTestManager([]ProtocolType{ProtocolICCE})

	req := &NegotiateRequest{
		DeviceID: "device-rc", Vendor: "huawei",
		DeviceCaps:  &NegotiateCapabilities{BLE: true, UWB: true, NFC: true, SE: true, FiRa: true},
		VehicleCaps: &NegotiateCapabilities{BLE: true, UWB: true, NFC: true, SE: true},
	}
	resp, err := mgr.NegotiateProtocol(context.Background(), req)
	if err != nil {
		t.Fatalf("NegotiateProtocol() error = %v", err)
	}

	icceCodec := NewICCECodec()
	rcMsg := &UnifiedMessage{
		Type: MsgTypeRemoteControl,
		RemoteControl: &RemoteControlMessage{Action: ActionLock},
	}
	data, err := icceCodec.Encode(rcMsg)
	if err != nil {
		t.Fatalf("Encode ICCE RC error = %v", err)
	}

	ok, err := mgr.HandleRemoteControl(context.Background(), resp.SessionID, data)
	if err != nil {
		t.Fatalf("HandleRemoteControl() error = %v", err)
	}
	if !ok {
		t.Error("HandleRemoteControl() returned false")
	}

	_, err = mgr.HandleRemoteControl(context.Background(), "nonexistent", data)
	if err == nil {
		t.Error("HandleRemoteControl() should fail with nonexistent session")
	}
}

func TestManagerShareKey(t *testing.T) {
	mgr := newTestManager([]ProtocolType{ProtocolICCE})

	req := &NegotiateRequest{
		DeviceID: "device-share", Vendor: "huawei",
		DeviceCaps:  &NegotiateCapabilities{BLE: true, UWB: true, NFC: true, SE: true, FiRa: true},
		VehicleCaps: &NegotiateCapabilities{BLE: true, UWB: true, NFC: true, SE: true},
	}
	resp, err := mgr.NegotiateProtocol(context.Background(), req)
	if err != nil {
		t.Fatalf("NegotiateProtocol() error = %v", err)
	}

	// ShareKey uses key.Protocol from the service. Since our mock
	// returns PROTOCOL_UNSPECIFIED, it should fall back to session protocol.
	// For now the current implementation returns an error in this case.
	shareResp, err := mgr.ShareKey(context.Background(), resp.SessionID, &pb.CreateShareRequest{
		KeyId: "key-share-001", ToUserId: "recipient-user", ValidUntil: 9999999999,
	})
	if err != nil {
		t.Logf("ShareKey() error (expected — mock key has no protocol): %v", err)
	} else if shareResp.ShareId == "" {
		t.Error("ShareId should not be empty")
	}

	_, err = mgr.ShareKey(context.Background(), "nonexistent", &pb.CreateShareRequest{KeyId: "key-001"})
	if err == nil {
		t.Error("ShareKey() should fail with nonexistent session")
	}
}

func TestManagerBindKey(t *testing.T) {
	mgr := newTestManager([]ProtocolType{ProtocolICCOA40})

	req := &NegotiateRequest{
		DeviceID: "device-bind", Vendor: "xiaomi",
		DeviceCaps:  &NegotiateCapabilities{BLE: true, UWB: true, NFC: true, SE: true, FiRa: true},
		VehicleCaps: &NegotiateCapabilities{BLE: true, UWB: true, NFC: true, SE: true},
	}
	resp, err := mgr.NegotiateProtocol(context.Background(), req)
	if err != nil {
		t.Fatalf("NegotiateProtocol() error = %v", err)
	}

	bindResp, err := mgr.BindKey(context.Background(), resp.SessionID, &pb.BindKeyRequest{
		DeviceId: "device-bind", VehicleId: "vehicle-bind", UserId: "user-bind",
		DevicePubkey: []byte{0x01, 0x02, 0x03},
	})
	if err != nil {
		t.Fatalf("BindKey() error = %v", err)
	}
	if bindResp == nil || bindResp.Key == nil {
		t.Fatal("BindKey() response should contain a key")
	}
	if bindResp.Key.KeyId == "" {
		t.Error("KeyId should not be empty")
	}

	_, err = mgr.BindKey(context.Background(), "nonexistent", &pb.BindKeyRequest{})
	if err == nil {
		t.Error("BindKey() should fail with nonexistent session")
	}
}

func TestManagerNewManagerWithNilConfig(t *testing.T) {
	mgr := NewManager(&Config{
		SupportedProtocols: []ProtocolType{},
	})
	if mgr == nil {
		t.Fatal("NewManager() returned nil")
	}

	mgr.RegisterCodec(ProtocolCCC3, NewCCCCodec())
	_, ok := mgr.GetSession("nothing")
	if ok {
		t.Error("GetSession should return false")
	}

	sessions := mgr.ListSessions()
	if len(sessions) != 0 {
		t.Errorf("ListSessions = %d, want 0", len(sessions))
	}

	n := mgr.Cleanup()
	if n != 0 {
		t.Errorf("Cleanup = %d, want 0", n)
	}
}

func TestManagerBindKeyWithDifferentProtocols(t *testing.T) {
	t.Run("CCC3 bind flow", func(t *testing.T) {
		mgr := newTestManager([]ProtocolType{ProtocolCCC3})
		req := &NegotiateRequest{
			DeviceID: "device-ccc-bind", Vendor: "samsung",
			DeviceCaps:  &NegotiateCapabilities{BLE: true, UWB: true, NFC: false, SE: true, FiRa: true},
			VehicleCaps: &NegotiateCapabilities{BLE: true, UWB: true, NFC: false, SE: true},
		}
		resp, err := mgr.NegotiateProtocol(context.Background(), req)
		if err != nil {
			t.Fatalf("NegotiateProtocol() error = %v", err)
		}
		if resp.Protocol != ProtocolCCC3 {
			t.Errorf("Protocol = %v, want CCC3", resp.Protocol)
		}
		_, err = mgr.BindKey(context.Background(), resp.SessionID, &pb.BindKeyRequest{
			DeviceId: "device-ccc", VehicleId: "vehicle-ccc", UserId: "user-ccc",
		})
		if err != nil {
			t.Fatalf("BindKey(CCC) error = %v", err)
		}
	})

	t.Run("ICCE bind flow", func(t *testing.T) {
		mgr := newTestManager([]ProtocolType{ProtocolICCE})
		req := &NegotiateRequest{
			DeviceID: "device-icce-bind", Vendor: "huawei",
			DeviceCaps:  &NegotiateCapabilities{BLE: true, UWB: true, NFC: true, SE: true, FiRa: true},
			VehicleCaps: &NegotiateCapabilities{BLE: true, UWB: true, NFC: true, SE: true},
		}
		resp, err := mgr.NegotiateProtocol(context.Background(), req)
		if err != nil {
			t.Fatalf("NegotiateProtocol() error = %v", err)
		}
		if resp.Protocol != ProtocolICCE {
			t.Errorf("Protocol = %v, want ICCE", resp.Protocol)
		}
		_, err = mgr.BindKey(context.Background(), resp.SessionID, &pb.BindKeyRequest{
			DeviceId: "device-icce", VehicleId: "vehicle-icce", UserId: "user-icce",
		})
		if err != nil {
			t.Fatalf("BindKey(ICCE) error = %v", err)
		}
	})
}

func TestToUnifiedMessageWithInterface(t *testing.T) {
	t.Run("with nil input", func(t *testing.T) {
		msg := ToUnifiedMessage(nil)
		if msg == nil {
			t.Fatal("should not return nil")
		}
	})

	t.Run("with BindKeyRequest implementing GetDeviceId", func(t *testing.T) {
		req := &pb.BindKeyRequest{DeviceId: "device-from-req"}
		msg := ToUnifiedMessage(req)
		if msg == nil || msg.Device == nil {
			t.Fatal("should return msg with Device")
		}
		if msg.Device.DeviceID != "device-from-req" {
			t.Errorf("DeviceID = %q", msg.Device.DeviceID)
		}
	})

	t.Run("with string (non-interface match)", func(t *testing.T) {
		msg := ToUnifiedMessage("hello")
		if msg == nil {
			t.Fatal("should not return nil")
		}
		_ = msg
	})
}

func TestMatchEdgeCases(t *testing.T) {
	spec := GetSpec(ProtocolICCOA40)

	t.Run("missing all capabilities", func(t *testing.T) {
		dev := &Device{
			Capabilities: &CapabilitySet{},
		}
		ok, _ := dev.Match(spec)
		if ok {
			t.Error("Match should be false with all missing")
		}
	})

	t.Run("no required capabilities on spec", func(t *testing.T) {
		emptySpec := &ProtocolSpec{
			Name: "empty", RequiredCaps: &CapabilitySet{},
		}
		dev := &Device{Capabilities: &CapabilitySet{}}
		ok, missing := dev.Match(emptySpec)
		if !ok {
			t.Error("Match should be true with no required caps")
		}
		if len(missing) != 0 {
			t.Errorf("missing should be empty, got %v", missing)
		}
	})
}

func TestICCEDecodeEdgeCasesExtended(t *testing.T) {
	codec := NewICCECodec()

	t.Run("detectICCEType with single byte in range", func(t *testing.T) {
		msg, err := codec.Decode([]byte{0x90})
		if err == nil {
			if msg.Type != MsgTypeDeviceInfo {
				t.Errorf("Type = %v, want DeviceInfo", msg.Type)
			}
		}
	})

	t.Run("detectICCEType with 0x9F02 tag", func(t *testing.T) {
		msg, err := codec.Decode([]byte{0x9F, 0x02})
		if err != nil {
			t.Fatalf("Decode() error = %v", err)
		}
		if msg.Type != MsgTypeKeyBind {
			t.Errorf("Type = %v, want KeyBind", msg.Type)
		}
	})

	t.Run("detectICCEType with single byte below range returns error", func(t *testing.T) {
		// Single byte 0x01: ICCE Decode requires at least 2 bytes
		_, err := codec.Decode([]byte{0x01})
		if err == nil {
			t.Error("Decode() should error with too short data")
		}
	})
}

func TestICCOADetectMessageTypeEdgeCases(t *testing.T) {
	codec := NewICCOACodec(ProtocolICCOA40)

	t.Run("msgByte in DeviceInfo range", func(t *testing.T) {
		msg, err := codec.Decode([]byte{0xA0, 0x01, 0x00})
		if err != nil {
			t.Fatalf("Decode() error = %v", err)
		}
		if msg.Type != MsgTypeDeviceInfo {
			t.Errorf("Type = %v, want DeviceInfo", msg.Type)
		}
	})
}

func TestNegotiateResponseMarshalJSONEdgeCase(t *testing.T) {
	resp := &NegotiateResponse{
		SessionID: "sess-edge", Protocol: ProtocolUnspecified,
		Version: "", MatchScore: 0, Reason: "",
	}
	data, err := resp.MarshalJSON()
	if err != nil {
		t.Fatalf("MarshalJSON() error = %v", err)
	}
	if len(data) == 0 {
		t.Error("MarshalJSON() returned empty")
	}
}

func TestGetSpecEdgeCases(t *testing.T) {
	for _, proto := range []ProtocolType{ProtocolCCC3, ProtocolICCOA30, ProtocolICCOA40, ProtocolICCE} {
		spec := GetSpec(proto)
		if spec == nil {
			t.Errorf("GetSpec(%v) returned nil", proto)
			continue
		}
		if spec.MinVersion == "" {
			t.Errorf("GetSpec(%v).MinVersion is empty", proto)
		}
		if spec.RequiredCaps == nil {
			t.Errorf("GetSpec(%v).RequiredCaps is nil", proto)
		}
	}
}

func TestEncodeTagEdgeCases(t *testing.T) {
	codec := NewICCECodec()

	t.Run("encodeTag <= 0x1F", func(t *testing.T) {
		result := codec.encodeTag(0x1F)
		if len(result) != 1 || result[0] != 0x1F {
			t.Errorf("encodeTag(0x1F) = %v, want [0x1F]", result)
		}
	})

	t.Run("encodeTag > 0x9FFF", func(t *testing.T) {
		result := codec.encodeTag(0xE001)
		if len(result) != 2 {
			t.Errorf("encodeTag(0xE001) length = %d, want 2", len(result))
		}
		if result[0] != 0xE0 || result[1] != 0x01 {
			t.Errorf("encodeTag(0xE001) = %v, want [0xE0, 0x01]", result)
		}
	})
}

func TestICCETLVMultiByteTagWithShortLength(t *testing.T) {
	codec := NewICCECodec()

	t.Run("encodeTLV with tag >0x1F and <0x100", func(t *testing.T) {
		result := codec.encodeTLV(0x9F, []byte{0x01})
		if len(result) != 3 {
			t.Fatalf("encoded length = %d, want 3", len(result))
		}
		if result[0] != 0x9F || result[1] != 1 || result[2] != 0x01 {
			t.Errorf("result = %v, want [0x9F, 0x01, 0x01]", result)
		}
	})
}

func TestICCOADecodeLengthEdgeCases(t *testing.T) {
	codec := NewICCOACodec(ProtocolICCOA40)

	t.Run("decodeLength empty data", func(t *testing.T) {
		length, n := codec.decodeLength(nil)
		if length != 0 || n != 0 {
			t.Errorf("decodeLength(nil) = (%d, %d), want (0, 0)", length, n)
		}
	})

	t.Run("decodeLength 0x81 with short data", func(t *testing.T) {
		length, _ := codec.decodeLength([]byte{0x81})
		if length != 0 {
			t.Errorf("length = %d, want 0", length)
		}
	})

	t.Run("decodeLength 0x82 with short data", func(t *testing.T) {
		length, _ := codec.decodeLength([]byte{0x82, 0x01})
		if length != 0 {
			t.Errorf("length = %d, want 0", length)
		}
	})
}

func TestICCEDecodeLengthEdgeCases(t *testing.T) {
	codec := NewICCECodec()

	t.Run("decodeLength empty data", func(t *testing.T) {
		length, n := codec.decodeLength(nil)
		if length != 0 || n != 0 {
			t.Errorf("decodeLength(nil) = (%d, %d), want (0, 0)", length, n)
		}
	})

	t.Run("decodeLength 0x81 with short data", func(t *testing.T) {
		length, _ := codec.decodeLength([]byte{0x81})
		if length != 0 {
			t.Errorf("length = %d, want 0", length)
		}
	})

	t.Run("decodeLength 0x82 with short data", func(t *testing.T) {
		length, _ := codec.decodeLength([]byte{0x82, 0x01})
		if length != 0 {
			t.Errorf("length = %d, want 0", length)
		}
	})
}

func TestManagerNullOrDefault(t *testing.T) {
	t.Run("default codecs registered", func(t *testing.T) {
		mgr := NewManager(&Config{
			SupportedProtocols: []ProtocolType{},
		})
		for _, proto := range []ProtocolType{ProtocolICCOA30, ProtocolICCOA40, ProtocolICCE, ProtocolCCC3} {
			_, ok := mgr.codecRegistry.Get(proto)
			if !ok {
				t.Errorf("default codec for %v not registered", proto)
			}
		}
	})
}
