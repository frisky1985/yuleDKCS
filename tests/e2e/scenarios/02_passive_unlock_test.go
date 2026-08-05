package scenarios

import (
	"log"
	"testing"
	"time"

	"github.com/yuleDKCS/tests/e2e/client"
)

// TestPassiveUnlock verifies the passive (hands-free) unlock flow:
//  1. Phone already paired with key bound
//  2. BLE advertisement detected → car sends challenge
//  3. Phone signs challenge with key
//  4. Car verifies signature → UNLOCK
//  5. Verify latency < 1s
func TestPassiveUnlock(t *testing.T) {
	requireCarSim(t)
	log.Printf("═══════════════════════════════════════")
	log.Printf("📋 Test 02: Passive Unlock (被动无感解锁)")
	log.Printf("═══════════════════════════════════════")

	carAddr := getCarAddr(t)

	// 1. Create phone and bind key (prerequisite)
	phone, err := client.NewMobileClient("phone_002", "user_owner_001", 3, 4)
	if err != nil {
		t.Fatalf("❌ Failed to create mobile client: %v", err)
	}
	if err := phone.ConnectCar(carAddr); err != nil {
		t.Fatalf("❌ Failed to connect: %v", err)
	}
	defer phone.DisconnectCar()

	if err := phone.BLEPairing(); err != nil {
		t.Fatalf("❌ BLE pairing failed: %v", err)
	}
	if err := phone.SCP03Establish(); err != nil {
		t.Fatalf("❌ SCP03 failed: %v", err)
	}

	keyID := "key_owner_002"
	validFrom := time.Now().UnixMilli()
	validUntil := time.Now().AddDate(1, 0, 0).UnixMilli()
	if err := phone.WriteKeyParams(keyID, 1, 0xFF, validFrom, validUntil); err != nil {
		t.Fatalf("❌ Key write failed: %v", err)
	}
	log.Printf("   🔑 Prerequisite: Key %s bound", keyID)

	// 2. Simulate approaching vehicle: BLE advertisement triggers challenge
	log.Printf("   📡 Step 1: BLE advertisement detected → request challenge...")
	start := time.Now()

	challenge, err := phone.RequestChallenge()
	if err != nil {
		t.Fatalf("❌ Challenge request failed: %v", err)
	}
	log.Printf("   🔐 Challenge received: %x...", challenge.Challenge[:4])

	// 3. Sign and respond to challenge
	log.Printf("   ✍️ Step 2: Signing challenge with key...")
	if err := phone.RespondChallenge(keyID, challenge); err != nil {
		t.Fatalf("❌ Challenge response failed: %v", err)
	}

	// 4. Send unlock command
	log.Printf("   🚗 Step 3: Sending unlock command...")
	if err := phone.SendCommand("unlock", keyID, 0x0040); err != nil { // MsgTypeUnlockCmd
		t.Fatalf("❌ Unlock command failed: %v", err)
	}
	unlockTime := time.Since(start)

	// 5. Verify vehicle is unlocked
	log.Printf("   🔍 Step 4: Verifying unlock...")
	status, err := phone.GetVehicleStatus()
	if err != nil {
		t.Fatalf("❌ Status request failed: %v", err)
	}

	if status.LockStatus != 1 {
		t.Errorf("❌ Vehicle NOT unlocked: lock_status=%d", status.LockStatus)
	} else {
		log.Printf("   ✅ Vehicle is UNLOCKED")
	}

	// 6. Latency check
	log.Printf("⏱️  Total unlock latency: %v", unlockTime)
	if unlockTime > 1*time.Second {
		t.Errorf("❌ Latency %v exceeds 1s threshold", unlockTime)
	} else {
		log.Printf("   ✅ Latency within 1s threshold")
	}

	log.Printf("═══════════════════════════════════════")
	log.Printf("✅ PASS: Passive Unlock in %v", unlockTime)
	log.Printf("═══════════════════════════════════════")
}
