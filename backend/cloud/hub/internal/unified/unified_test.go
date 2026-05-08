package unified

import (
	"testing"
)

// ============================================================
// 协议协商测试
// ============================================================

func TestNegotiator_Negotiate(t *testing.T) {
	negotiator := NewNegotiator([]ProtocolType{
		ProtocolCCC3, ProtocolICCOA40, ProtocolICCOA30, ProtocolICCE,
	})

	tests := []struct {
		name       string
		deviceCaps *CapabilitySet
		vehicleCaps *CapabilitySet
		wantProto  ProtocolType
		wantScore  int
	}{
		{
			name: "Xiaomi with full capabilities",
			deviceCaps: &CapabilitySet{
				BLE: true, UWB: true, NFC: true, SE: true, FiRa: true,
			},
			vehicleCaps: &CapabilitySet{
				BLE: true, UWB: true, NFC: true, SE: true,
			},
			wantProto: ProtocolICCOA40,
			wantScore: 100,
		},
		{
			name: "Samsung - CCC preferred",
			deviceCaps: &CapabilitySet{
				BLE: true, UWB: true, NFC: false, SE: true, FiRa: true,
			},
			vehicleCaps: &CapabilitySet{
				BLE: true, UWB: true, NFC: false, SE: true,
			},
			wantProto: ProtocolCCC3,
			wantScore: 100,
		},
		{
			name: "Huawei - ICCE preferred",
			deviceCaps: &CapabilitySet{
				BLE: true, UWB: true, NFC: true, SE: true, FiRa: true,
			},
			vehicleCaps: &CapabilitySet{
				BLE: true, UWB: true, NFC: true, SE: true,
			},
			wantProto: ProtocolICCE,
			wantScore: 100,
		},
		{
			name: "BLE only - limited protocol",
			deviceCaps: &CapabilitySet{
				BLE: true, UWB: false, NFC: false, SE: false, FiRa: false,
			},
			vehicleCaps: &CapabilitySet{
				BLE: true, UWB: true, NFC: true, SE: true,
			},
			wantProto: ProtocolICCOA30,
			wantScore: 30,  // no UWB/SE: deducts 30+30
		},
		{
			name: "Apple - CCC only",
			deviceCaps: &CapabilitySet{
				BLE: true, UWB: true, NFC: false, SE: true, FiRa: true,
			},
			vehicleCaps: &CapabilitySet{
				BLE: true, UWB: true, NFC: true, SE: true,
			},
			wantProto: ProtocolCCC3,
			wantScore: 100,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result, err := negotiator.Negotiate(tt.deviceCaps, tt.vehicleCaps)
			if err != nil {
				t.Fatalf("Negotiate() error = %v", err)
			}
			if result.Protocol != tt.wantProto {
				t.Errorf("Protocol = %v, want %v", result.Protocol, tt.wantProto)
			}
			if result.MatchScore != tt.wantScore {
				t.Errorf("MatchScore = %v, want %v", result.MatchScore, tt.wantScore)
			}
		})
	}
}

func TestAutoDetectProtocol(t *testing.T) {
	tests := []struct {
		vendor     string
		wantProto  ProtocolType
	}{
		{"xiaomi", ProtocolICCOA40},
		{"oppo", ProtocolICCOA40},
		{"vivo", ProtocolICCOA40},
		{"huawei", ProtocolICCE},
		{"samsung", ProtocolCCC3},
		{"apple", ProtocolCCC3},
		{"unknown", ProtocolICCOA40},
	}

	for _, tt := range tests {
		t.Run(tt.vendor, func(t *testing.T) {
			got := AutoDetectProtocol(tt.vendor, "android", "1.0.0")
			if got != tt.wantProto {
				t.Errorf("AutoDetectProtocol(%s) = %v, want %v", tt.vendor, got, tt.wantProto)
			}
		})
	}
}

// ============================================================
// 协议编解码测试
// ============================================================

func TestICCOACodec_EncodeDecode(t *testing.T) {
	codec := NewICCOACodec(ProtocolICCOA40)

	t.Run("KeyBind roundtrip", func(t *testing.T) {
		msg := &UnifiedMessage{
			Type: MsgTypeKeyBind,
			KeyBind: &KeyBindMessage{
				VehicleID:    "vehicle-123",
				UserID:      "user-456",
				DevicePubKey: []byte{0x01, 0x02, 0x03},
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
			t.Errorf("Type = %v, want %v", decoded.Type, MsgTypeKeyBind)
		}
		if decoded.KeyBind.VehicleID != "vehicle-123" {
			t.Errorf("VehicleID = %v, want %v", decoded.KeyBind.VehicleID, "vehicle-123")
		}
	})

	t.Run("RemoteControl roundtrip", func(t *testing.T) {
		msg := &UnifiedMessage{
			Type: MsgTypeRemoteControl,
			RemoteControl: &RemoteControlMessage{
				VehicleID: "vehicle-123",
				Action:    ActionLock,
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

		if decoded.RemoteControl.Action != ActionLock {
			t.Errorf("Action = %v, want %v", decoded.RemoteControl.Action, ActionLock)
		}
	})
}

func TestICCECodec_EncodeDecode(t *testing.T) {
	codec := NewICCECodec()

	t.Run("KeyBind roundtrip", func(t *testing.T) {
		msg := &UnifiedMessage{
			Type: MsgTypeKeyBind,
			KeyBind: &KeyBindMessage{
				UserID:      "user-icce",
				DevicePubKey: []byte{0x04, 0x05, 0x06},
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
			t.Errorf("Type = %v, want %v", decoded.Type, MsgTypeKeyBind)
		}
		if decoded.KeyBind.UserID != "user-icce" {
			t.Errorf("UserID = %v, want %v", decoded.KeyBind.UserID, "user-icce")
		}
	})

	t.Run("RemoteControl roundtrip", func(t *testing.T) {
		msg := &UnifiedMessage{
			Type: MsgTypeRemoteControl,
			RemoteControl: &RemoteControlMessage{
				VehicleID: "vehicle-icce",
				Action:    ActionUnlock,
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

		if decoded.RemoteControl.Action != ActionUnlock {
			t.Errorf("Action = %v, want %v", decoded.RemoteControl.Action, ActionUnlock)
		}
	})
}

func TestCCCCodec_EncodeDecode(t *testing.T) {
	codec := NewCCCCodec()

	t.Run("KeyBind roundtrip", func(t *testing.T) {
		msg := &UnifiedMessage{
			Type: MsgTypeKeyBind,
			KeyBind: &KeyBindMessage{
				VehicleID: "vehicle-ccc",
				UserID:   "user-ccc",
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
			t.Errorf("Type = %v, want %v", decoded.Type, MsgTypeKeyBind)
		}
	})

	t.Run("RemoteControl roundtrip", func(t *testing.T) {
		msg := &UnifiedMessage{
			Type: MsgTypeRemoteControl,
			RemoteControl: &RemoteControlMessage{
				VehicleID: "vehicle-ccc",
				Action:    ActionEngineStart,
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

		if decoded.RemoteControl.Action != ActionEngineStart {
			t.Errorf("Action = %v, want %v", decoded.RemoteControl.Action, ActionEngineStart)
		}
	})
}

// ============================================================
// 会话状态机测试
// ============================================================

func TestSessionStateMachine(t *testing.T) {
	sm := NewStateMachine()

	tests := []struct {
		from   SessionState
		event  Event
		to     SessionState
		shouldFail bool
	}{
		{StateInit, EventNegotiateStart, StateNegotiating, false},
		{StateNegotiating, EventNegotiateDone, StateDeviceVerified, false},
		{StateDeviceVerified, EventBindStart, StateKeyBinding, false},
		{StateKeyBinding, EventBindSuccess, StateActive, false},
		{StateActive, EventShareStart, StateSharing, false},
		{StateSharing, EventShareDone, StateActive, false},
		{StateActive, EventSuspend, StateSuspended, false},
		{StateSuspended, EventResume, StateActive, false},
		{StateActive, EventRevoke, StateRevoked, false},
		// Invalid transitions
		{StateInit, EventBindStart, StateInit, true},
		{StateRevoked, EventResume, StateRevoked, true},
	}

	for _, tt := range tests {
		t.Run(tt.from.String()+" + "+tt.event.String(), func(t *testing.T) {
			got, err := sm.Next(tt.from, tt.event)
			if tt.shouldFail {
				if err == nil {
					t.Errorf("Next() should fail but got %v", got)
				}
			} else {
				if err != nil {
					t.Errorf("Next() error = %v", err)
				}
				if got != tt.to {
					t.Errorf("Next() = %v, want %v", got, tt.to)
				}
			}
		})
	}
}

func TestSessionManager(t *testing.T) {
	mgr := NewSessionManager(timeout)
	
	t.Run("Create and get session", func(t *testing.T) {
		device := &Device{DeviceID: "test-device"}
		session := mgr.CreateSession("session-1", device)
		
		if session.SessionID != "session-1" {
			t.Errorf("SessionID = %v, want %v", session.SessionID, "session-1")
		}
		if session.State != StateInit {
			t.Errorf("State = %v, want %v", session.State, StateInit)
		}
		
		got, ok := mgr.GetSession("session-1")
		if !ok {
			t.Error("GetSession() not found")
		}
		if got.SessionID != "session-1" {
			t.Errorf("Got SessionID = %v", got.SessionID)
		}
	})

	t.Run("State transitions", func(t *testing.T) {
		device := &Device{DeviceID: "test-device-2"}
		session := mgr.CreateSession("session-2", device)
		
		// Negotiate -> DeviceVerified
		err := mgr.Transition("session-2", EventNegotiateStart)
		if err != nil {
			t.Errorf("Transition(negotiate) error = %v", err)
		}
		if session.GetState() != StateNegotiating {
			t.Errorf("State = %v, want %v", session.GetState(), StateNegotiating)
		}
		
		// NegotiateDone -> Bind
		err = mgr.Transition("session-2", EventNegotiateDone)
		if err != nil {
			t.Errorf("Transition(negotiateDone) error = %v", err)
		}
		if session.GetState() != StateDeviceVerified {
			t.Errorf("State = %v, want %v", session.GetState(), StateDeviceVerified)
		}
	})

	t.Run("GetOrCreate", func(t *testing.T) {
		device := &Device{DeviceID: "test-device-3"}
		
		// Get existing
		s1 := mgr.GetOrCreate("session-existing", device)
		if s1.SessionID != "session-existing" {
			t.Errorf("SessionID = %v", s1.SessionID)
		}
		
		// Create new
		s2 := mgr.GetOrCreate("session-new", device)
		if s2.SessionID != "session-new" {
			t.Errorf("SessionID = %v", s2.SessionID)
		}
	})

	t.Run("Remove", func(t *testing.T) {
		device := &Device{DeviceID: "test-device-4"}
		mgr.CreateSession("session-to-remove", device)
		
		mgr.Remove("session-to-remove")
		
		_, ok := mgr.GetSession("session-to-remove")
		if ok {
			t.Error("Session should be removed")
		}
	})
}

func TestCapabilitySet(t *testing.T) {
	caps := &CapabilitySet{
		BLE: true, UWB: true, NFC: false, SE: true, FiRa: false,
	}

	s := caps.String()
	expected := "BLE=true UWB=true NFC=false SE=true FiRa=false"
	if s != expected {
		t.Errorf("String() = %v, want %v", s, expected)
	}
}

func TestProtocolType(t *testing.T) {
	tests := []struct {
		proto ProtocolType
		str   string
	}{
		{ProtocolCCC3, "ccc_dk3"},
		{ProtocolICCOA30, "iccoa_dk30"},
		{ProtocolICCOA40, "iccoa_dk40"},
		{ProtocolICCE, "icce"},
		{ProtocolType(99), "unknown"},
	}

	for _, tt := range tests {
		t.Run(tt.str, func(t *testing.T) {
			if tt.proto.String() != tt.str {
				t.Errorf("String() = %v, want %v", tt.proto.String(), tt.str)
			}
		})
	}
}

func TestGetCodecForProtocol(t *testing.T) {
	tests := []struct {
		proto ProtocolType
		isNil bool
	}{
		{ProtocolICCOA30, false},
		{ProtocolICCOA40, false},
		{ProtocolICCE, false},
		{ProtocolCCC3, false},
		{ProtocolUnspecified, true},
	}

	for _, tt := range tests {
		t.Run(tt.proto.String(), func(t *testing.T) {
			codec := GetCodecForProtocol(tt.proto)
			if tt.isNil && codec != nil {
				t.Error("codec should be nil")
			}
			if !tt.isNil && codec == nil {
				t.Error("codec should not be nil")
			}
		})
	}
}

func TestGetSpec(t *testing.T) {
	spec := GetSpec(ProtocolICCOA40)
	if spec == nil {
		t.Fatal("spec should not be nil")
	}
	if spec.Name != "ICCOA Digital Key 4.0" {
		t.Errorf("Name = %v", spec.Name)
	}
	if !spec.RequiredCaps.BLE || !spec.RequiredCaps.UWB || !spec.RequiredCaps.NFC {
		t.Error("ICCOA 4.0 should require BLE+UWB+NFC")
	}
}

var timeout = 30*time.Minute // imported from testing package
