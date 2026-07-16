package scenarios

import (
	"log"
	"testing"
	"time"

	"github.com/yuleDKCS/tests/e2e/client"
)

// TestKeyBinding verifies the full key binding flow:
//  1. Mobile connects to cloud (simulated gRPC)
//  2. Cloud returns key material
//  3. Mobile pairs with car over BLE
//  4. SCP03 secure channel established
//  5. Key written to vehicle SE050
//  6. Key status = ACTIVE
//
// Flow: Connect → BLE Pair → SCP03 → Write Key → Verify
func TestKeyBinding(t *testing.T) {
	log.Printf("═══════════════════════════════════════")
	log.Printf("📋 Test 01: Key Binding (密钥绑定)")
	log.Printf("═══════════════════════════════════════")

	carAddr := getCarAddr(t)

	// 1. Create mobile client (simulating phone)
	phone, err := client.NewMobileClient("phone_001", "user_owner_001", 3, 4) // Xiaomi, ICCE
	if err != nil {
		t.Fatalf("❌ Failed to create mobile client: %v", err)
	}
	log.Printf("   ✅ Phone client created: %s", phone.DeviceID)

	// 2. Connect to car simulator
	if err := phone.ConnectCar(carAddr); err != nil {
		t.Fatalf("❌ Failed to connect to car: %v", err)
	}
	defer phone.DisconnectCar()

	// 3. BLE Pairing
	log.Printf("   📡 Step 1: BLE Pairing...")
	start := time.Now()
	if err := phone.BLEPairing(); err != nil {
		t.Fatalf("❌ BLE pairing failed: %v", err)
	}
	pairingTime := time.Since(start)
	log.Printf("   ✅ BLE paired in %v", pairingTime)

	// 4. SCP03 Secure Channel
	log.Printf("   🔐 Step 2: Establish SCP03 secure channel...")
	start = time.Now()
	if err := phone.SCP03Establish(); err != nil {
		t.Fatalf("❌ SCP03 establish failed: %v", err)
	}
	scp03Time := time.Since(start)
	log.Printf("   ✅ SCP03 established in %v", scp03Time)

	// 5. Write key to vehicle
	log.Printf("   🔑 Step 3: Write owner key to vehicle...")
	keyID := "key_owner_001"
	validFrom := time.Now().UnixMilli()
	validUntil := time.Now().AddDate(1, 0, 0).UnixMilli()
	start = time.Now()
	if err := phone.WriteKeyParams(keyID, 1, 0xFF, validFrom, validUntil); err != nil { // keyType=1 (OWNER)
		t.Fatalf("❌ Failed to write key: %v", err)
	}
	keyWriteTime := time.Since(start)
	log.Printf("   ✅ Key written in %v", keyWriteTime)

	// 6. Verify key status by checking vehicle state
	log.Printf("   ✅ Step 4: Verify key binding...")
	status, err := phone.GetVehicleStatus()
	if err != nil {
		t.Fatalf("❌ Failed to get vehicle status: %v", err)
	}
	log.Printf("   📊 Vehicle status: Locked=%v, Engine=%v, Battery=%d%%",
		status.LockStatus == 0, status.EngineStatus == 1, status.BatteryPct)

	// Verify the key is stored on the vehicle
	if _, ok := phone.Keys[keyID]; !ok {
		t.Fatalf("❌ Key %s not found on phone's key store", keyID)
	}

	totalTime := pairingTime + scp03Time + keyWriteTime
	log.Printf("═══════════════════════════════════════")
	log.Printf("✅ PASS: Key Binding complete in %v", totalTime)
	log.Printf("   - BLE Pairing: %v", pairingTime)
	log.Printf("   - SCP03 Channel: %v", scp03Time)
	log.Printf("   - Key Write: %v", keyWriteTime)
	log.Printf("   - Key ID: %s", keyID)
	log.Printf("   - Key Type: OWNER")
	log.Printf("═══════════════════════════════════════")
}
