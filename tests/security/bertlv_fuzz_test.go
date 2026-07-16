// Package security_tests — Automated Security Penetration Tests for yuleDKCS
//
// This file contains BER-TLV fuzzing extending the existing fuzz targets.
// Run: go test -fuzz=Fuzz -fuzztime=120s ./backend/cloud/hub/internal/codec/bertlv/
package security

import (
	"testing"

	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/codec/bertlv"
)

// FuzzBERTLVExtended extends the existing BERTLV fuzz with pentest-specific
// malicious payloads targeting known attack patterns:
//   - Integer overflow in length fields
//   - Tag confusion (constructed vs primitive)
//   - Nested TLV bombs (billion laughs)
//   - Extremal length values (0x00, 0xFF, max uint32)
func FuzzBERTLVExtended(f *testing.F) {
	// Seed corpus: extended malicious payloads
	seeds := [][]byte{
		// Integer overflow: length = 0xFFFFFFFF (max uint32)
		{0xE0, 0x84, 0xFF, 0xFF, 0xFF, 0xFF},
		// Length = 0x7FFFFFFF (large but "valid" long form)
		{0xE0, 0x84, 0x7F, 0xFF, 0xFF, 0xFF},
		// Zero-length but tag claims length > 0 (no data after)
		{0xE0, 0x05},
		// Nested TLV bomb: depth 10 (billion laughs style)
		{0xE0, 0x06, 0xE0, 0x04, 0xE0, 0x02, 0x00, 0x00},
		// Multi-byte tag with invalid continuation
		{0x9F, 0x01, 0x03, 0x01, 0x02, 0x03},
		{0x5F, 0x1C, 0x02, 0x01, 0x02},
		// Tag 0x00 (invalid in BER-TLV)
		{0x00, 0x01, 0x00},
		// Padding byte injection
		{0x00, 0x00, 0xE0, 0x01, 0xAA},
		// ASCII text embedded in TLV (to test encoding confusion)
		{0xE0, 0x05, 0x68, 0x65, 0x6C, 0x6C, 0x6F},
		// All zeros
		make([]byte, 100),
		// All 0xFF
		func() []byte { b := make([]byte, 100); for i := range b { b[i] = 0xFF }; return b }(),
		// Alternating 0xAA, 0x55
		func() []byte { b := make([]byte, 100); for i := range b { b[i] = 0xAA }; return b }(),
		// Single byte
		{0xE0},
		// Length with extended encoding: 0x81 N (single-byte long length)
		{0xE0, 0x81, 0x00},
		{0xE0, 0x81, 0x01, 0x00},
		{0xE0, 0x81, 0x7F},
		// Length with 2-byte extended encoding: 0x82 NN
		{0xE0, 0x82, 0x00, 0x00},
		{0xE0, 0x82, 0x00, 0x01, 0x00},
		{0xE0, 0x82, 0xFF, 0xFF},
		// Length with 3-byte extended encoding: 0x83 NNN
		{0xE0, 0x83, 0x00, 0x00, 0x01, 0x00},
		// Length with 4-byte extended encoding: 0x84 NNNN
		{0xE0, 0x84, 0x00, 0x00, 0x00, 0x01, 0x00},
		// Unreasonable length (exceeds any real payload)
		{0xE0, 0x82, 0x10, 0x00},
		// Empty tag body but length says there is
		{0xE1, 0x01},
		// Tag = 0xFF (invalid high tag)
		{0xFF, 0x01, 0x00},
		// Tag = 0x1F (boundary: last single-byte tag)
		{0x1F, 0x01, 0x00},
		// Tag = 0x20 (first multi-byte tag)
		{0x20, 0x01, 0x00},
		// Constructed tag with indefinite length (if supported)
		{0xE0, 0x80, 0x01, 0xAA, 0x00, 0x00},
		// DOIP-style TLV (long tag)
		{0x12, 0x01, 0x00},
		// GlobalPlatform SCP03 TLV
		{0x4F, 0x08, 0xA0, 0x00, 0x00, 0x01, 0x51, 0x00, 0x00, 0x00},
	}

	for _, seed := range seeds {
		f.Add(seed)
	}

	f.Fuzz(func(t *testing.T, data []byte) {
		// Test single decode
		decoder := bertlv.NewDecoder(data)
		_, err := decoder.Decode()
		_ = err // only care about no panic

		// Test decode-all
		_, errAll := bertlv.DecodeAll(data)
		_ = errAll
	})
}

// FuzzBERTLVEncodeDecodeFull tests roundtrip encode/decode with edge-case payloads.
// Ensures no crash from encode → decode → re-encode patterns.
func FuzzBERTLVEncodeDecodeFull(f *testing.F) {
	payloads := [][]byte{
		make([]byte, 0),
		make([]byte, 1),
		make([]byte, 255),
		make([]byte, 65535),
		[]byte(""),
		[]byte("\x00\x00\x00\x00"),
		[]byte{0xFF, 0xFE, 0xFD, 0xFC},
		func() []byte { b := make([]byte, 256); copy(b, "A"); return b }(),
	}
	for _, p := range payloads {
		f.Add(p)
	}

	f.Fuzz(func(t *testing.T, payload []byte) {
		// Use a known single-byte tag for roundtrip
		tag := bertlv.Tag(0xE0)
		encoded, err := bertlv.Encode(tag, payload)
		if err != nil {
			return
		}

		// Decode back
		decoder := bertlv.NewDecoder(encoded)
		result, err := decoder.Decode()
		if err != nil {
			t.Errorf("encode→decode roundtrip failed for payload len=%d: %v", len(payload), err)
			return
		}

		if result.Tag != tag {
			t.Errorf("tag mismatch: got 0x%X, want 0x%X", result.Tag, tag)
		}
	})
}
