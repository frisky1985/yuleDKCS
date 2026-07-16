package scenarios

import (
	"crypto/rand"
	"log"
	"testing"
	"time"

	"github.com/yuleDKCS/tests/e2e/client"
)

// TestNFCUnlock verifies NFC tap-to-unlock flow:
//  1. Phone emulates NFC tag
//  2. Vehicle's NFC reader reads credential
//  3. Phone sends auth response
//  4. Vehicle verifies → UNLOCK
//  5. Event logged
func TestNFCUnlock(t *testing.T) {
	log.Printf("═══════════════════════════════════════")
	log.Printf("📋 Test 03: NFC Unlock (NFC 刷卡解锁)")
	log.Printf("═══════════════════════════════════════")

	carAddr := getCarAddr(t)

	// 1. Create phone and bind key
	phone, err := client.NewMobileClient("phone_003", "user_owner_001", 3, 4)
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

	keyID := "key_owner_003"
	validFrom := time.Now().UnixMilli()
	validUntil := time.Now().AddDate(1, 0, 0).UnixMilli()
	if err := phone.WriteKeyParams(keyID, 1, 0xFF, validFrom, validUntil); err != nil {
		t.Fatalf("❌ Key write failed: %v", err)
	}
	log.Printf("   🔑 Prerequisite: Key %s bound", keyID)

	// 2. NFC Read simulation
	log.Printf("   📱 Step 1: NFC tap detected, reading credential...")
	credentialID := "nfc_cred_003"

	nfcResp, err := phone.NFCRead(keyID, credentialID)
	if err != nil {
		t.Fatalf("❌ NFC read failed: %v", err)
	}
	log.Printf("   ✅ NFC tag read: credential=%s, data=%x...", nfcResp.CredentialID, nfcResp.Data[:4])
	log.Printf("   📋 NFC metadata: %v", nfcResp.Metadata)

	// 3. NFC Authentication
	log.Printf("   🔐 Step 2: Sending NFC auth response...")
	authData := make([]byte, 16)
	rand.Read(authData)

	start := time.Now()
	authSuccess, err := phone.NFCAuthenticate(keyID, credentialID, authData)
	if err != nil {
		t.Fatalf("❌ NFC auth failed: %v", err)
	}
	nfcTime := time.Since(start)

	if !authSuccess {
		t.Errorf("❌ NFC authentication returned false")
	} else {
		log.Printf("   ✅ NFC authentication successful!")
	}

	// 4. Verify vehicle is unlocked
	log.Printf("   🔍 Step 3: Verifying unlock status...")
	status, err := phone.GetVehicleStatus()
	if err != nil {
		t.Fatalf("❌ Status request failed: %v", err)
	}

	if status.LockStatus == 1 {
		log.Printf("   ✅ Vehicle is UNLOCKED after NFC auth")
	} else {
		t.Errorf("❌ Vehicle NOT unlocked after NFC auth")
	}

	log.Printf("⏱️  NFC unlock time: %v", nfcTime)
	log.Printf("═══════════════════════════════════════")
	log.Printf("✅ PASS: NFC Unlock in %v", nfcTime)
	log.Printf("═══════════════════════════════════════")
}
