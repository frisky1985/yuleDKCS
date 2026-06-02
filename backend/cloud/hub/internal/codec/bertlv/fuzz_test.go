// Package bertlv fuzz tests
package bertlv

import (
	"testing"
)

// FuzzDecode fuzzes the Decode function.
// It must never panic regardless of input.
func FuzzDecode(f *testing.F) {
	// Seed corpus: valid TLV-encoded data from known test cases
	f.Add([]byte{0x01, 0x01, 0x00})                      // simple TLV: Tag=0x01, Len=1, Val=[0x00]
	f.Add([]byte{0x9F, 0x01, 0x03, 0x01, 0x02, 0x03})   // multi-byte tag (0x9F01), Len=3
	f.Add([]byte{})                                       // empty data
	f.Add([]byte{0xE0, 0x03, 0x01, 0x02, 0x03})          // single-byte tag, short length
	f.Add([]byte{0xE0, 0x7F})                             // max short length, no value
	f.Add([]byte{0xE0, 0x81, 0xC8})                       // long length (1 byte), no value
	f.Add([]byte{0xE0, 0x82, 0x01, 0x00})                 // long length (2 bytes), no value
	f.Add([]byte{0xDF, 0x3A, 0x02, 0xAB, 0xCD})           // multi-byte tag with value
	f.Add([]byte{0xE0})                                   // truncated (tag only)
	f.Add([]byte{0xE0, 0x05, 0x01})                       // truncated (expects 5 bytes, has 1)
	f.Add([]byte{0xE0, 0x85, 0x00, 0x00, 0x00, 0x00, 0x01}) // invalid length format (5 bytes)
	f.Add([]byte{0xDF})                                   // truncated multi-byte tag

	f.Fuzz(func(t *testing.T, data []byte) {
		decoder := NewDecoder(data)
		_, err := decoder.Decode()
		// fuzz must never panic — errors are acceptable, crashes are not
		_ = err
	})
}

// FuzzDecodeAll fuzzes the DecodeAll function (decodes all TLVs in a buffer).
// This is more commonly used in production than single Decode.
func FuzzDecodeAll(f *testing.F) {
	// Seed corpus
	f.Add([]byte{0xE0, 0x01, 0xAA, 0xE1, 0x02, 0xBB, 0xCC}) // two consecutive TLVs
	f.Add([]byte{})                                            // empty
	f.Add([]byte{0xE0, 0x01, 0xAA})                            // single TLV
	f.Add([]byte{0xE0, 0x01, 0xAA, 0xE1, 0x05, 0xBB})         // first OK, second truncated
	f.Add([]byte{0xE0, 0x81, 0xFF})                            // long length with trailing data

	f.Fuzz(func(t *testing.T, data []byte) {
		_, err := DecodeAll(data)
		// fuzz must never panic — errors are acceptable
		_ = err
	})
}

// FuzzEncodeDecode roundtrip fuzz: encode arbitrary payloads then decode them.
// Verifies the encoder never produces data the decoder can't handle.
func FuzzEncodeDecode(f *testing.F) {
	// Seed corpus: various payload shapes
	f.Add([]byte{0x01, 0x02, 0x03})       // short payload
	f.Add([]byte("hello"))                 // string-as-bytes
	f.Add(make([]byte, 1000))              // large payload (1000 bytes)

	f.Add([]byte{})                        // empty payload
	f.Add([]byte{0xFF})                    // single byte
	f.Add([]byte{0x00, 0x00, 0x00, 0x00}) // all zeros

	f.Fuzz(func(t *testing.T, payload []byte) {
		// Encode with a single-byte tag (< 0x1F low-5 bits to avoid multi-byte tag ambiguity)
		tag := Tag(0xE0)
		encoded, err := Encode(tag, payload)
		if err != nil {
			// Some payloads (nil, unsupported types) can legitimately fail Encode
			return
		}
		if len(encoded) == 0 {
			t.Error("Encode returned empty result with nil error")
			return
		}

		// Decode roundtrip
		decoder := NewDecoder(encoded)
		result, err := decoder.Decode()
		if err != nil {
			t.Errorf("roundtrip decode failed: %v\nencoded=%x", err, encoded)
			return
		}
		// Roundtrip sanity checks
		if result.Tag != tag {
			t.Errorf("tag mismatch: want 0x%X, got 0x%X", tag, result.Tag)
		}
		if result.Length != len(payload) {
			t.Errorf("length mismatch: want %d, got %d", len(payload), result.Length)
		}
	})
}
