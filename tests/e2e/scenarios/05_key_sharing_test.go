package scenarios

import (
	"log"
	"testing"
	"time"

	"github.com/yuleDKCS/tests/e2e/client"
)

// TestKeySharing verifies the key sharing flow:
//  1. Owner shares key with friend (7 days)
//  2. Cloud notifies friend
//  3. Friend accepts share
//  4. Friend can unlock vehicle
//  5. Owner key still active
func TestKeySharing(t *testing.T) {
	log.Printf("═══════════════════════════════════════")
	log.Printf("📋 Test 05: Key Sharing (钥匙分享)")
	log.Printf("═══════════════════════════════════════")

	carAddr := getCarAddr(t)

	// 1. Setup owner phone with bound key
	owner, err := client.NewMobileClient("phone_owner_005", "user_owner_001", 3, 4)
	if err != nil {
		t.Fatalf("❌ Failed to create owner client: %v", err)
	}
	if err := owner.ConnectCar(carAddr); err != nil {
		t.Fatalf("❌ Owner connect failed: %v", err)
	}
	defer owner.DisconnectCar()

	if err := owner.BLEPairing(); err != nil {
		t.Fatalf("❌ Owner BLE pairing failed: %v", err)
	}
	if err := owner.SCP03Establish(); err != nil {
		t.Fatalf("❌ Owner SCP03 failed: %v", err)
	}

	ownerKeyID := "key_owner_005"
	validFrom := time.Now().UnixMilli()
	validUntil := time.Now().AddDate(1, 0, 0).UnixMilli()
	if err := owner.WriteKeyParams(ownerKeyID, 1, 0xFF, validFrom, validUntil); err != nil {
		t.Fatalf("❌ Owner key write failed: %v", err)
	}
	log.Printf("   👤 Owner key %s bound", ownerKeyID)

	// 2. Simulate key sharing: owner creates a friend key
	log.Printf("   🤝 Step 1: Owner sharing key with friend (7 days)...")
	friendKeyID := "key_friend_005"
	shareValidFrom := time.Now().UnixMilli()
	shareValidUntil := time.Now().Add(7 * 24 * time.Hour).UnixMilli()

	// In simulation, owner writes friend key to car directly
	// (In real life, this goes through cloud → car)
	if err := owner.WriteKeyParams(friendKeyID, 2, 0x07, shareValidFrom, shareValidUntil); err != nil { // keyType=2 (FRIEND)
		t.Fatalf("❌ Friend key write failed: %v", err)
	}
	log.Printf("   ✅ Friend key %s created (valid for 7 days)", friendKeyID)

	// 3. Friend connects and authenticates with shared key
	log.Printf("   👤 Step 2: Friend connecting with shared key...")
	friend, err := client.NewMobileClient("phone_friend_005", "user_friend_001", 2, 3) // Samsung, ICCOA
	if err != nil {
		t.Fatalf("❌ Failed to create friend client: %v", err)
	}
	if err := friend.ConnectCar(carAddr); err != nil {
		t.Fatalf("❌ Friend connect failed: %v", err)
	}
	defer friend.DisconnectCar()

	if err := friend.BLEPairing(); err != nil {
		t.Fatalf("❌ Friend BLE pairing failed: %v", err)
	}
	if err := friend.SCP03Establish(); err != nil {
		t.Fatalf("❌ Friend SCP03 failed: %v", err)
	}

	// Friend doesn't have the key on their device (simplified: they use owner's key write)
	// In real flow, friend receives key material via cloud
	// For simulation, friend uses the shared key that owner wrote
	friend.Keys[friendKeyID] = owner.Keys[friendKeyID]

	// 4. Friend unlocks vehicle
	log.Printf("   🚗 Step 3: Friend attempting unlock...")
	challenge, err := friend.RequestChallenge()
	if err != nil {
		t.Fatalf("❌ Friend challenge request failed: %v", err)
	}
	if err := friend.RespondChallenge(friendKeyID, challenge); err != nil {
		t.Fatalf("❌ Friend challenge response failed: %v", err)
	}
	if err := friend.SendCommand("unlock", friendKeyID, 0x0040); err != nil {
		t.Fatalf("❌ Friend unlock failed: %v", err)
	}

	status, _ := friend.GetVehicleStatus()
	if status.LockStatus == 1 {
		log.Printf("   ✅ Friend successfully unlocked vehicle!")
	} else {
		t.Errorf("❌ Friend could not unlock vehicle")
	}

	// 5. Verify owner key still works
	log.Printf("   👤 Step 4: Verifying owner key still active...")
	challenge, err = owner.RequestChallenge()
	if err != nil {
		t.Fatalf("❌ Owner challenge request failed: %v", err)
	}
	if err := owner.RespondChallenge(ownerKeyID, challenge); err != nil {
		t.Fatalf("❌ Owner challenge response failed: %v", err)
	}
	if err := owner.SendCommand("lock", ownerKeyID, 0x0041); err != nil {
		t.Fatalf("❌ Owner lock failed: %v", err)
	}

	status, _ = owner.GetVehicleStatus()
	if status.LockStatus == 0 {
		log.Printf("   ✅ Owner key still active and working!")
	} else {
		t.Errorf("❌ Owner key should still work")
	}

	log.Printf("═══════════════════════════════════════")
	log.Printf("✅ PASS: Key Sharing")
	log.Printf("   - Owner key: %s (ACTIVE)", ownerKeyID)
	log.Printf("   - Friend key: %s (ACTIVE, 7 days)", friendKeyID)
	log.Printf("   - Both keys can control the vehicle")
	log.Printf("═══════════════════════════════════════")
}
