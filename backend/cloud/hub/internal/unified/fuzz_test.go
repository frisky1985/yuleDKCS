// Package unified fuzz tests — brute-force production bug detection for all codec Encode/Decode methods.
//
// Go fuzz testing generates random inputs (nil fields, empty strings, negative enum values,
// huge arrays, etc.) and verifies the code never panics regardless of input.
// Every panic discovered here is a real production bug that would otherwise escape.
package unified

import (
	"testing"

	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
)

// FuzzICCOACodecEncode fuzzes ICCOACodec.Encode with randomized UnifiedMessage fields.
// Must never panic — errors are acceptable, crashes are not.
func FuzzICCOACodecEncode(f *testing.F) {
	// Seed corpus: valid messages for each message type
	f.Add(uint8(0), uint8(0), uint8(0), uint8(0), uint8(0), uint8(1), uint8(0)) // KeyBind, all fields
	f.Add(uint8(1), uint8(1), uint8(1), uint8(1), uint8(0), uint8(3), uint8(2)) // KeyShare
	f.Add(uint8(2), uint8(0), uint8(1), uint8(1), uint8(1), uint8(5), uint8(4)) // RemoteControl
	f.Add(uint8(3), uint8(1), uint8(0), uint8(0), uint8(0), uint8(0), uint8(6)) // VehicleStatus
	f.Add(uint8(9), uint8(1), uint8(1), uint8(1), uint8(1), uint8(0), uint8(7)) // Heartbeat (unsupported)
	f.Add(uint8(11), uint8(0), uint8(0), uint8(0), uint8(0), uint8(9), uint8(0)) // out-of-range

	f.Fuzz(func(t *testing.T, msgTypeIdx uint8, hasBind uint8, hasShare uint8, hasRemote uint8, hasStatus uint8, actionIdx uint8, keyTypeIdx uint8) {
		codec30 := NewICCOACodec(ProtocolICCOA30)
		codec40 := NewICCOACodec(ProtocolICCOA40)

		msg := &UnifiedMessage{
			Type:      MessageType(msgTypeIdx % 12),
			Sequence:  uint64(msgTypeIdx) * 0x100000000,
			Timestamp: int64(msgTypeIdx) * 1000,
		}

		if hasBind%2 == 1 {
			msg.KeyBind = &KeyBindMessage{
				VehicleID:    randomString(int(msgTypeIdx) % 64),
				UserID:       randomString(int(actionIdx) % 64),
				KeyType:      randomKeyType(keyTypeIdx),
				DevicePubKey: randomBytes(int(msgTypeIdx) * 7),
				ValidFrom:    int64(msgTypeIdx) * 100000,
				ValidUntil:   int64(msgTypeIdx) * 200000,
			}
			if msgTypeIdx%3 == 0 {
				msg.KeyBind.DevicePubKey = nil
			}
			if msgTypeIdx%5 == 0 {
				msg.KeyBind.VehicleID = ""
			}
		}

		if hasShare%2 == 1 {
			msg.KeyShare = &KeyShareMessage{
				KeyID:       randomString(int(actionIdx) % 64),
				ShareID:     randomString(int(msgTypeIdx) % 64),
				RecipientID: randomString(int(keyTypeIdx) % 64),
				ValidUntil:  int64(keyTypeIdx) * 300000,
			}
		}

		if hasRemote%2 == 1 {
			msg.RemoteControl = &RemoteControlMessage{
				KeyID:     randomString(int(msgTypeIdx) % 32),
				VehicleID: randomString(int(actionIdx) % 32),
				Action:    RemoteAction(actionIdx % 12),
				Timestamp: int64(keyTypeIdx) * 50000,
			}
		}

		if hasStatus%2 == 1 {
			msg.VehicleStatus = &VehicleStatusMessage{
				VehicleID:    randomString(int(msgTypeIdx) % 32),
				KeyID:        randomString(int(actionIdx) % 32),
				DoorsLocked:  msgTypeIdx%2 == 0,
				EngineOn:     actionIdx%2 == 0,
				BatteryLevel: int(msgTypeIdx) * 50,
				Timestamp:    int64(actionIdx) * 10000,
			}
		}

		for _, codec := range []*ICCOACodec{codec30, codec40} {
			_, err := codec.Encode(msg)
			_ = err
		}
	})
}

// FuzzICCECodecEncode fuzzes ICCECodec.Encode with randomized messages.
func FuzzICCECodecEncode(f *testing.F) {
	f.Add(uint8(0), uint8(1), uint8(1), uint8(1), uint8(1), uint8(0))
	f.Add(uint8(1), uint8(0), uint8(0), uint8(1), uint8(3), uint8(2))
	f.Add(uint8(2), uint8(1), uint8(0), uint8(0), uint8(5), uint8(4))
	f.Add(uint8(3), uint8(0), uint8(1), uint8(0), uint8(0), uint8(6))

	f.Fuzz(func(t *testing.T, msgTypeIdx uint8, hasBind uint8, hasRemote uint8, hasStatus uint8, actionIdx uint8, keyTypeIdx uint8) {
		codec := NewICCECodec()
		msg := &UnifiedMessage{
			Type: MessageType(msgTypeIdx % 12),
		}

		if hasBind%2 == 1 {
			msg.KeyBind = &KeyBindMessage{
				UserID:       randomString(int(actionIdx) % 64),
				DevicePubKey: randomBytes(int(msgTypeIdx) * 5),
			}
			if msgTypeIdx%3 == 0 {
				msg.KeyBind.DevicePubKey = nil
			}
			if msgTypeIdx%5 == 0 {
				msg.KeyBind.UserID = ""
			}
		}

		if hasRemote%2 == 1 {
			msg.RemoteControl = &RemoteControlMessage{
				Action: RemoteAction(actionIdx % 12),
			}
		}

		if hasStatus%2 == 1 {
			msg.VehicleStatus = &VehicleStatusMessage{
				DoorsLocked: msgTypeIdx%2 == 0,
			}
		}

		_, err := codec.Encode(msg)
		_ = err
	})
}

// FuzzCCCCodecEncode fuzzes CCCCodec.Encode with randomized messages.
func FuzzCCCCodecEncode(f *testing.F) {
	f.Add(uint8(0), uint8(1), uint8(1), uint8(1), uint8(1))
	f.Add(uint8(1), uint8(0), uint8(0), uint8(1), uint8(3))
	f.Add(uint8(2), uint8(1), uint8(0), uint8(0), uint8(5))
	f.Add(uint8(3), uint8(0), uint8(1), uint8(0), uint8(0))

	f.Fuzz(func(t *testing.T, msgTypeIdx uint8, hasBind uint8, hasRemote uint8, hasStatus uint8, actionIdx uint8) {
		codec := NewCCCCodec()
		msg := &UnifiedMessage{
			Type: MessageType(msgTypeIdx % 12),
		}

		if hasBind%2 == 1 {
			msg.KeyBind = &KeyBindMessage{
				VehicleID: randomString(int(actionIdx) % 64),
				UserID:    randomString(int(msgTypeIdx) % 64),
			}
		}

		if hasRemote%2 == 1 {
			msg.RemoteControl = &RemoteControlMessage{
				Action: RemoteAction(actionIdx % 12),
			}
		}

		if hasStatus%2 == 1 {
			msg.VehicleStatus = &VehicleStatusMessage{
				DoorsLocked: msgTypeIdx%2 == 0,
			}
		}

		_, err := codec.Encode(msg)
		_ = err
	})
}

// FuzzICCOACodecDecode fuzzes ICCOACodec.Decode with random byte slices.
func FuzzICCOACodecDecode(f *testing.F) {
	f.Add([]byte{0xA0, 0x11, 0x00, 0x01})
	f.Add([]byte{0xA0, 0x11, 0x00, 0x01, 0x01, 0x03, 'k', 'e', 'y'})
	f.Add([]byte{})
	f.Add([]byte{0xA0})
	f.Add([]byte{0xA0, 0x41, 0x01, 0x01})
	f.Add([]byte{0x00, 0x00})
	f.Add([]byte{0xA0, 0x51, 0x00})
	f.Add([]byte{0xA0, 0x11, 0x00, 0x01, 0x81, 0x05, 0x01, 0x02, 0x03, 0x04, 0x05})

	f.Fuzz(func(t *testing.T, data []byte) {
		codec30 := NewICCOACodec(ProtocolICCOA30)
		codec40 := NewICCOACodec(ProtocolICCOA40)
		for _, codec := range []*ICCOACodec{codec30, codec40} {
			_, err := codec.Decode(data)
			_ = err
		}
	})
}

// FuzzICCECodecDecode fuzzes ICCECodec.Decode with random byte slices.
func FuzzICCECodecDecode(f *testing.F) {
	f.Add([]byte{0x9F, 0x01, 0x03, 0x01, 0x02, 0x03})
	f.Add([]byte{0x9F, 0x10, 0x02, 0x01})
	f.Add([]byte{0x9F, 0x20, 0x02, 0x01})
	f.Add([]byte{})
	f.Add([]byte{0x9F})
	f.Add([]byte{0x01, 0x00})
	f.Add([]byte{0x9F, 0x01, 0x81, 0xFF})

	f.Fuzz(func(t *testing.T, data []byte) {
		codec := NewICCECodec()
		_, err := codec.Decode(data)
		_ = err
	})
}

// FuzzCCCCodecDecode fuzzes CCCCodec.Decode with random byte slices.
func FuzzCCCCodecDecode(f *testing.F) {
	f.Add([]byte{0x5C, 0x10})
	f.Add([]byte{0x5C, 0x30, 0x01})
	f.Add([]byte{0x5C, 0x40, 0x01})
	f.Add([]byte{})
	f.Add([]byte{0x5C})
	f.Add([]byte{0x00, 0x10})
	f.Add([]byte{0x5C, 0x50})
	f.Add([]byte{0x5C, 0x10, 0x01, 0x02, 0x03, 0x04, 0x05})

	f.Fuzz(func(t *testing.T, data []byte) {
		codec := NewCCCCodec()
		_, err := codec.Decode(data)
		_ = err
	})
}

// FuzzUnifiedCodecDecodeAuto fuzzes UnifiedCodec.DecodeAuto with random data.
func FuzzUnifiedCodecDecodeAuto(f *testing.F) {
	registry := NewCodecRegistry()
	registry.Register(ProtocolICCOA40, NewICCOACodec(ProtocolICCOA40))
	registry.Register(ProtocolICCOA30, NewICCOACodec(ProtocolICCOA30))
	registry.Register(ProtocolICCE, NewICCECodec())
	registry.Register(ProtocolCCC3, NewCCCCodec())
	uc := NewUnifiedCodec(registry)

	f.Add([]byte{0x30, 0x01, 0x00})
	f.Add([]byte{0x80, 0x05, 0x01})
	f.Add([]byte{0x5C, 0x10})
	f.Add([]byte{0x5F, 0x01})
	f.Add([]byte{})
	f.Add([]byte{0x01})
	f.Add([]byte{0xA0, 0x11, 0x00})
	f.Add([]byte{0x9F, 0x01, 0x03, 0x01, 0x02, 0x03})
	f.Add([]byte{0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF})
	f.Add([]byte{0xD5, 0x01, 0x02, 0x03})

	f.Fuzz(func(t *testing.T, data []byte) {
		_, err := uc.DecodeAuto(data)
		_ = err
	})
}

// --- helpers for fuzz tests ---

func randomString(n int) string {
	if n <= 0 {
		return ""
	}
	if n > 256 {
		n = 256
	}
	const letters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
	b := make([]byte, n)
	for i := range b {
		b[i] = letters[(i*7+13)%len(letters)]
	}
	return string(b)
}

func randomBytes(n int) []byte {
	if n <= 0 {
		return nil
	}
	if n > 512 {
		n = 512
	}
	b := make([]byte, n)
	for i := range b {
		b[i] = byte((i*17 + 31) % 256)
	}
	return b
}

func randomKeyType(n uint8) pb.KeyType {
	types := []pb.KeyType{0, 1, 2, 3, 4, 5}
	return types[int(n)%len(types)]
}
