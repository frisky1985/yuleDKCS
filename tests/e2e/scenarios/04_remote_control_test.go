package scenarios

import (
	"log"
	"testing"
	"time"

	"github.com/yuleDKCS/tests/e2e/client"
)

// TestRemoteControl verifies remote lock/unlock/engine commands:
//  1. Phone → Cloud: RemoteControl(LOCK) via gRPC (simulated)
//  2. Cloud → Car: relay command
//  3. Car → Cloud: ACK
//  4. Cloud → Phone: Response
//  5. Verify command executed
//
// Note: In E2E without cloud, this simulates the remote control
// as a direct car command (simulating cloud relay).
func TestRemoteControl(t *testing.T) {
	log.Printf("═══════════════════════════════════════")
	log.Printf("📋 Test 04: Remote Control (远程控车)")
	log.Printf("═══════════════════════════════════════")

	carAddr := getCarAddr(t)

	// Setup: phone with bound key
	phone, err := client.NewMobileClient("phone_004", "user_owner_001", 3, 4)
	if err != nil {
		t.Fatalf("❌ Failed to create client: %v", err)
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

	keyID := "key_owner_004"
	validFrom := time.Now().UnixMilli()
	validUntil := time.Now().AddDate(1, 0, 0).UnixMilli()
	if err := phone.WriteKeyParams(keyID, 1, 0xFF, validFrom, validUntil); err != nil {
		t.Fatalf("❌ Key write failed: %v", err)
	}

	// Ensure vehicle is locked initially
	status, _ := phone.GetVehicleStatus()
	log.Printf("   📊 Initial state: lock=%d", status.LockStatus)

	// Test 1: Remote Unlock
	log.Printf("   📡 Step 1: Remote UNLOCK...")
	start := time.Now()
	if err := phone.SendCommand("unlock", keyID, 0x0040); err != nil {
		t.Fatalf("❌ Remote unlock failed: %v", err)
	}
	unlockTime := time.Since(start)

	status, _ = phone.GetVehicleStatus()
	if status.LockStatus != 1 {
		t.Errorf("❌ Vehicle should be UNLOCKED")
	} else {
		log.Printf("   ✅ Remote UNLOCK successful (%v)", unlockTime)
	}

	// Test 2: Remote Lock
	log.Printf("   📡 Step 2: Remote LOCK...")
	start = time.Now()
	if err := phone.SendCommand("lock", keyID, 0x0041); err != nil {
		t.Fatalf("❌ Remote lock failed: %v", err)
	}
	lockTime := time.Since(start)

	status, _ = phone.GetVehicleStatus()
	if status.LockStatus != 0 {
		t.Errorf("❌ Vehicle should be LOCKED")
	} else {
		log.Printf("   ✅ Remote LOCK successful (%v)", lockTime)
	}

	// Test 3: Remote Engine Start (after unlock)
	log.Printf("   📡 Step 3: Remote ENGINE START...")
	if err := phone.SendCommand("unlock", keyID, 0x0040); err != nil {
		t.Fatalf("❌ Unlock before engine start failed: %v", err)
	}
	start = time.Now()
	if err := phone.SendCommand("engine_on", keyID, 0x0042); err != nil {
		t.Fatalf("❌ Remote engine start failed: %v", err)
	}
	engineTime := time.Since(start)

	status, _ = phone.GetVehicleStatus()
	if status.EngineStatus != 1 {
		t.Errorf("❌ Engine should be ON")
	} else {
		log.Printf("   ✅ Remote ENGINE START successful (%v)", engineTime)
	}

	// Test 4: Remote Engine Stop
	log.Printf("   📡 Step 4: Remote ENGINE STOP...")
	if err := phone.SendCommand("engine_off", keyID, 0x0042); err != nil {
		t.Fatalf("❌ Remote engine stop failed: %v", err)
	}

	status, _ = phone.GetVehicleStatus()
	if status.EngineStatus != 0 {
		t.Errorf("❌ Engine should be OFF")
	} else {
		log.Printf("   ✅ Remote ENGINE STOP successful")
	}

	log.Printf("═══════════════════════════════════════")
	log.Printf("✅ PASS: Remote Control - All 4 commands executed")
	log.Printf("   - Unlock: %v", unlockTime)
	log.Printf("   - Lock: %v", lockTime)
	log.Printf("   - Engine Start: %v", engineTime)
	log.Printf("═══════════════════════════════════════")
}
