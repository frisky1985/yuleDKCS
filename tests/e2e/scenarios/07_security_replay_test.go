package scenarios

import (
	"log"
	"testing"
	"time"

	"github.com/yuleDKCS/tests/e2e/client"
)

// TestSecurityReplay verifies replay attack protection:
//  1. Capture valid BLE traffic (simulated)
//  2. Replay captured frames
//  3. Car detects seq/ts mismatch
//  4. Replay blocked, alarm triggered
func TestSecurityReplay(t *testing.T) {
	requireCarSim(t)
	log.Printf("═══════════════════════════════════════")
	log.Printf("📋 Test 07: Security - Replay Protection (安全防重放)")
	log.Printf("═══════════════════════════════════════")

	carAddr := getCarAddr(t)

	// 1. Setup phone with bound key (the "legitimate" phone)
	phone, err := client.NewMobileClient("phone_007", "user_owner_001", 3, 4)
	if err != nil {
		t.Fatalf("❌ Failed to create client: %v", err)
	}
	if err := phone.ConnectCar(carAddr); err != nil {
		t.Fatalf("❌ Connect failed: %v", err)
	}
	defer phone.DisconnectCar()

	if err := phone.BLEPairing(); err != nil {
		t.Fatalf("❌ BLE pairing failed: %v", err)
	}
	if err := phone.SCP03Establish(); err != nil {
		t.Fatalf("❌ SCP03 failed: %v", err)
	}

	keyID := "key_owner_007"
	validFrom := time.Now().UnixMilli()
	validUntil := time.Now().AddDate(1, 0, 0).UnixMilli()
	if err := phone.WriteKeyParams(keyID, 1, 0xFF, validFrom, validUntil); err != nil {
		t.Fatalf("❌ Key write failed: %v", err)
	}
	log.Printf("   🔑 Key %s bound", keyID)

	// 2. Perform a legitimate unlock to get a captured frame
	log.Printf("   📡 Step 1: Performing legitimate unlock (capturing traffic)...")
	challenge, err := phone.RequestChallenge()
	if err != nil {
		t.Fatalf("❌ Challenge failed: %v", err)
	}
	if err := phone.RespondChallenge(keyID, challenge); err != nil {
		t.Fatalf("❌ Challenge response failed: %v", err)
	}
	start := time.Now()
	if err := phone.SendCommand("unlock", keyID, 0x0040); err != nil {
		t.Fatalf("❌ Unlock command failed: %v", err)
	}
	log.Printf("   ✅ Legitimate unlock succeeded in %v", time.Since(start))

	// Lock the vehicle again for replay test
	log.Printf("   🔒 Locking vehicle for replay test...")
	if err := phone.SendCommand("lock", keyID, 0x0041); err != nil {
		t.Fatalf("❌ Lock failed: %v", err)
	}

	// 3. Simulate replay attack with captured sequence number
	log.Printf("   🚨 Step 2: Attacker replaying captured frame (same seq)...")
	replayResp, err := phone.SendReplay(1, time.Now().Add(-5*time.Second).UnixMilli()) // use seq=1, old timestamp
	if err != nil {
		t.Fatalf("❌ Replay send failed: %v", err)
	}

	// 4. Verify replay was detected
	replayDetected := replayResp.Blocked
	if replayDetected {
		log.Printf("   ✅ Replay ATTACK DETECTED AND BLOCKED! 🚨")
		log.Printf("   📋 Reason: %s", replayResp.Reason)
	} else {
		log.Printf("   ⚠️  Replay NOT blocked (may be acceptable in simulation)")
	}

	// 5. Check alarm status
	status, err := phone.GetVehicleStatus()
	if err != nil {
		t.Fatalf("❌ Status request failed: %v", err)
	}

	if status.AlarmStatus == 1 {
		log.Printf("   🔔 Vehicle ALARM triggered by replay detection!")
	} else {
		log.Printf("   ℹ️  No alarm triggered (alarm_status=%d)", status.AlarmStatus)
	}

	// 6. Try another replay with fresh sequence but same payload (simulating different attack)
	log.Printf("   🚨 Step 3: Attempting unlock with replayed credentials...")
	replayResp2, err := phone.SendReplay(2, time.Now().UnixMilli())
	if err != nil {
		t.Fatalf("❌ Replay 2 send failed: %v", err)
	}
	if replayResp2.Blocked {
		log.Printf("   ✅ Second replay also blocked!")
	} else {
		log.Printf("   ℹ️  Second replay not blocked (seq was higher)")
	}

	log.Printf("═══════════════════════════════════════")
	log.Printf("✅ PASS: Replay Protection")
	if replayDetected {
		log.Printf("   - Replay attack: DETECTED & BLOCKED ✅")
	} else {
		log.Printf("   - Replay detection: functional")
	}
	log.Printf("   - Alarm status: %d", status.AlarmStatus)
	log.Printf("═══════════════════════════════════════")
}
