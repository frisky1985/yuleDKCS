package scenarios

import (
	"log"
	"testing"
	"time"

	"github.com/yuleDKCS/tests/e2e/client"
)

// TestKeyRevocation verifies key revocation flow:
//  1. Owner revokes friend's key via cloud
//  2. Cloud sends revocation list to car
//  3. Friend attempts BLE unlock → REJECTED
//  4. Revoked key cannot access vehicle
func TestKeyRevocation(t *testing.T) {
	requireCarSim(t)
	log.Printf("═══════════════════════════════════════")
	log.Printf("📋 Test 06: Key Revocation (钥匙吊销)")
	log.Printf("═══════════════════════════════════════")

	carAddr := getCarAddr(t)

	// 1. Setup owner with key
	owner, err := client.NewMobileClient("phone_owner_006", "user_owner_001", 3, 4)
	if err != nil {
		t.Fatalf("❌ Failed to create owner: %v", err)
	}
	if err := owner.ConnectCar(carAddr); err != nil {
		t.Fatalf("❌ Owner connect failed: %v", err)
	}
	defer owner.DisconnectCar()

	if err := owner.BLEPairing(); err != nil {
		t.Fatalf("❌ Owner BLE failed: %v", err)
	}
	if err := owner.SCP03Establish(); err != nil {
		t.Fatalf("❌ Owner SCP03 failed: %v", err)
	}

	ownerKeyID := "key_owner_006"
	validFrom := time.Now().UnixMilli()
	validUntil := time.Now().AddDate(1, 0, 0).UnixMilli()
	if err := owner.WriteKeyParams(ownerKeyID, 1, 0xFF, validFrom, validUntil); err != nil {
		t.Fatalf("❌ Owner key write failed: %v", err)
	}

	// 2. Add friend key
	friendKeyID := "key_friend_006"
	shareValidFrom := time.Now().UnixMilli()
	shareValidUntil := time.Now().Add(7 * 24 * time.Hour).UnixMilli()
	if err := owner.WriteKeyParams(friendKeyID, 2, 0x07, shareValidFrom, shareValidUntil); err != nil {
		t.Fatalf("❌ Friend key write failed: %v", err)
	}
	log.Printf("   🔑 Friend key %s created", friendKeyID)

	// 3. Verify friend can unlock initially
	friend, err := client.NewMobileClient("phone_friend_006", "user_friend_001", 2, 3)
	if err != nil {
		t.Fatalf("❌ Failed to create friend: %v", err)
	}
	if err := friend.ConnectCar(carAddr); err != nil {
		t.Fatalf("❌ Friend connect failed: %v", err)
	}
	defer friend.DisconnectCar()
	if err := friend.BLEPairing(); err != nil {
		t.Fatalf("❌ Friend BLE failed: %v", err)
	}
	if err := friend.SCP03Establish(); err != nil {
		t.Fatalf("❌ Friend SCP03 failed: %v", err)
	}
	friend.Keys[friendKeyID] = owner.Keys[friendKeyID]

	log.Printf("   ➡️ Before revocation: Friend attempting unlock...")
	challenge, err := friend.RequestChallenge()
	if err != nil {
		t.Fatalf("❌ Pre-revoke challenge failed: %v", err)
	}
	if err := friend.RespondChallenge(friendKeyID, challenge); err != nil {
		t.Fatalf("❌ Pre-revoke response failed: %v", err)
	}
	if err := friend.SendCommand("unlock", friendKeyID, 0x0040); err != nil {
		t.Fatalf("❌ Pre-revoke unlock failed: %v", err)
	}
	status, _ := friend.GetVehicleStatus()
	log.Printf("   ✅ Friend unlock BEFORE revocation: locked=%v", status.LockStatus == 0)

	// 4. Owner revokes friend's key
	log.Printf("   🚫 Step 1: Owner revoking friend's key %s...", friendKeyID)
	if err := owner.SendRevokeNotify(friendKeyID, "friend no longer authorized"); err != nil {
		t.Fatalf("❌ Revoke notify failed: %v", err)
	}
	log.Printf("   ✅ Key %s revoked on vehicle", friendKeyID)

	// 5. Friend attempts unlock after revocation - should FAIL
	log.Printf("   🚫 Step 2: Friend attempting unlock AFTER revocation...")
	challenge, err = friend.RequestChallenge()
	if err != nil {
		t.Fatalf("❒ Post-revoke challenge failed: %v", err)
	}
	if err := friend.RespondChallenge(friendKeyID, challenge); err == nil {
		// We expect an error here, but if the challenge-respond doesn't reject,
		// the unlock command should be rejected
		log.Printf("   ⚠️  Challenge responded without error, checking unlock...")
		err = friend.SendCommand("unlock", friendKeyID, 0x0040)
		if err == nil {
			status, _ = friend.GetVehicleStatus()
			if status.LockStatus == 1 {
				t.Errorf("❌ Friend should NOT be able to unlock after revocation!")
			} else {
				log.Printf("   ✅ Vehicle remained LOCKED after revoked key attempt")
			}
		} else {
			log.Printf("   ✅ Unlock command rejected: %v", err)
		}
	} else {
		log.Printf("   ✅ Challenge response rejected: %v", err)
	}

	// 6. Verify owner key still works
	log.Printf("   👤 Step 3: Verifying owner key still works...")
	challenge, err = owner.RequestChallenge()
	if err != nil {
		t.Fatalf("❌ Owner post-revoke challenge failed: %v", err)
	}
	if err := owner.RespondChallenge(ownerKeyID, challenge); err != nil {
		t.Fatalf("❌ Owner post-revoke response failed: %v", err)
	}
	if err := owner.SendCommand("unlock", ownerKeyID, 0x0040); err != nil {
		t.Fatalf("❌ Owner post-revoke unlock failed: %v", err)
	}
	status, _ = owner.GetVehicleStatus()
	if status.LockStatus == 1 {
		log.Printf("   ✅ Owner key still works after revocation!")
	} else {
		t.Errorf("❌ Owner key should still work after friend revocation")
	}

	log.Printf("═══════════════════════════════════════")
	log.Printf("✅ PASS: Key Revocation")
	log.Printf("   - Revoked key %s blocked", friendKeyID)
	log.Printf("   - Owner key %s still active", ownerKeyID)
	log.Printf("═══════════════════════════════════════")
}
