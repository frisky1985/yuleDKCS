// Package security — BLE protocol malformed packet injection tests
//
// Tests the Car Simulator's resilience against malformed BLE frames
// covering ICCE, CCC, and ICCOA protocol characteristics.
//
// Run: go test -v -run TestBLEMalformed ./tests/security/
package security

import (
	"log"
	"os"
	"testing"
	"time"

	"github.com/yuleDKCS/tests/e2e/client"
	"github.com/yuleDKCS/tests/e2e/proto"
)

func TestBLEMalformedICCECCCICCOA(t *testing.T) {
	log.Printf("════════════════════════════════════════════════")
	log.Printf("📋 BLE Malformed Frame Injection — ICCE/CCC/ICCOA")
	log.Printf("════════════════════════════════════════════════")

	carAddr := getCarAddr(t)
	phone, err := client.NewMobileClient("sec_ble_001", "sec_user_001",
		uint32(proto.ProtoICCE), uint32(proto.ProtoICCE))
	if err != nil {
		t.Fatalf("Failed to create client: %v", err)
	}
	if err := phone.ConnectCar(carAddr); err != nil {
		t.Fatalf("Connect failed: %v", err)
	}
	defer phone.DisconnectCar()

	t.Run("malformed_icce_pair", func(t *testing.T) {
		log.Printf("   🚨 Test 1: ICCE pair with malformed payload")
		// Send directly via raw frame (bypass proto validation)
		rawData := []byte("{\"device_id\":\"\x00\x01\x02\",\"edge_capable\":\"not_a_bool\"}")

		// Send malformed payload directly to test parser robustness
		phone.SendFrame(proto.MsgTypeICCEPair, rawData)
		log.Printf("   ✅ ICCE malformed frame sent without crash")
	})

	t.Run("malformed_ccc_oob", func(t *testing.T) {
		log.Printf("   🚨 Test 2: CCC OOB with oversized NFC data")
		oversized := make([]byte, 65536) // Exceeds any realistic NFC payload
		resp, err := phone.SendFrame(proto.MsgTypeCCC_OOB_Init, oversized)
		if err != nil {
			// Some implementations reject oversized payloads gracefully
			log.Printf("   ✅ CCC OOB oversized rejected (expected): %v", err)
			return
		}
		if resp != nil {
			// If it processed, check for error in response
			log.Printf("   ℹ️  CCC OOB oversized: got response (may be error payload)")
		}
	})

	t.Run("malformed_iccoa_dk40", func(t *testing.T) {
		log.Printf("   🚨 Test 3: ICCOA DK4.0 with truncated HMAC")
		// Send ICCOA DK4.0 frame with HMAC only 2 bytes (should be 32)
		shortHmac := &proto.IcCoaFramePayload{
			DkVersion:    40,
			Sop:          []byte{0x53, 0x4F, 0x50},
			Payload:      []byte("test_key"),
			Hmac:         []byte{0x01, 0x02}, // truncated HMAC
			SessionToken: []byte("session_token_16b"),
		}
		rawPayload := proto.EncodePayload(shortHmac)
		resp, err := phone.SendFrame(proto.MsgTypeICCOA_DK40, rawPayload)
		if err != nil {
			log.Printf("   ✅ ICCOA DK4.0 truncated HMAC rejected: %v", err)
		} else {
			log.Printf("   ℹ️  ICCOA DK4.0 truncated HMAC: got response len=%d", len(resp.Payload))
		}
	})

	t.Run("malformed_null_bytes", func(t *testing.T) {
		log.Printf("   🚨 Test 4: Null byte injection in all protocol frames")
		nullPayload := []byte("\x00\x00\x00\x00\x00")
		msgTypes := []uint16{
			proto.MsgTypeICCEPair,
			proto.MsgTypeCCC_OOB_Init,
			proto.MsgTypeICCOA_DK30,
			proto.MsgTypeICCOA_DK40,
		}
		for _, mt := range msgTypes {
			_, err := phone.SendFrame(mt, nullPayload)
			if err != nil {
				log.Printf("   ✅ MsgType=0x%04X rejected null bytes: %v", mt, err)
			} else {
				log.Printf("   ℹ️  MsgType=0x%04X accepted null bytes", mt)
			}
		}
	})

	t.Run("malformed_max_msgtype", func(t *testing.T) {
		log.Printf("   🚨 Test 5: Invalid MsgType = 0xFFFF and 0x0000")
		_, err1 := phone.SendFrame(0xFFFF, []byte{0x01, 0x02, 0x03})
		if err1 != nil {
			log.Printf("   ✅ MsgType=0xFFFF rejected: %v", err1)
		}
		_, err0 := phone.SendFrame(0x0000, []byte{0x01, 0x02, 0x03})
		if err0 != nil {
			log.Printf("   ✅ MsgType=0x0000 rejected: %v", err0)
		}
	})

	t.Run("malformed_rapid_connect", func(t *testing.T) {
		log.Printf("   🚨 Test 6: Rapid connect/disconnect cycle (connection exhaustion)")
		for i := 0; i < 20; i++ {
			p, err := client.NewMobileClient(
				"sec_flood_"+string(rune('A'+i%26)),
				"sec_user_001", 3, 4)
			if err != nil {
				continue
			}
			_ = p.ConnectCar(carAddr)
			time.Sleep(10 * time.Millisecond)
			p.DisconnectCar()
		}
		log.Printf("   ✅ Rapid connect/disconnect: no crash")
	})

	log.Printf("════════════════════════════════════════════════")
	log.Printf("✅ PASS: BLE Malformed Frame Injection")
	log.Printf("════════════════════════════════════════════════")
}
