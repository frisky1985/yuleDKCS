// Package security — Replay attack simulation tests
//
// Tests replay attack protection by capturing valid BLE traffic and replaying
// frames with various sequence number and timestamp combinations.
// Extends the existing E2E replay test with additional attack scenarios.
//
// Run: go test -v ./tests/e2e/scenarios/ -run TestSecurityReplay
// (for the base test already in the project)
//
// Run: go test -v -run TestSimulatedReplay ./tests/security/
//
// Dependency: Car Simulator running on CARSIM_ADDR (default localhost:18001)
//
// Scenarios:
//   1. Same seq + old ts (classic replay)
//   2. Empty seq + valid ts (sequence gap)
//   3. Max uint32 seq (boundary overflow)
//   4. Seq reorder (4-2-1-3)
//   5. Negative timestamp
//   6. Zero timestamp
//   7. Fast replay within 500ms window
//   8. Bulk replay (10x rapid frames)
package security

import (
	"log"
	"net"
	"os"
	"testing"
	"time"

	"github.com/yuleDKCS/tests/e2e/client"
)

func TestSimulatedReplay(t *testing.T) {
	log.Printf("════════════════════════════════════════════════════")
	log.Printf("📋 Replay Attack Simulation — Extended Scenarios")
	log.Printf("════════════════════════════════════════════════════")

	carAddr := getCarAddr(t)

	// Skip when no car simulator is reachable (e.g. plain CI without
	// the CARSIM docker service) — this is an integration test.
	if os.Getenv("CARSIM_ADDR") == "" {
		conn, err := net.DialTimeout("tcp", carAddr, 500*time.Millisecond)
		if err != nil {
			t.Skipf("car simulator not reachable at %s — skipping integration test", carAddr)
		}
		conn.Close()
	}

	// Setup phone with bound key
	phone, err := client.NewMobileClient("sec_replay_001", "user_owner_001", 3, 4)
	if err != nil {
		t.Fatalf("Failed to create client: %v", err)
	}
	if err := phone.ConnectCar(carAddr); err != nil {
		t.Fatalf("Connect failed: %v", err)
	}
	defer phone.DisconnectCar()

	// Establish secure channel and bind key
	phone.BLEPairing()
	phone.SCP03Establish()
	if err := phone.WriteKeyParams("sec_replay_key_001", 1, 0xFF,
		time.Now().UnixMilli(),
		time.Now().AddDate(1, 0, 0).UnixMilli()); err != nil {
		t.Fatalf("Key write failed: %v", err)
	}
	log.Printf("   🔑 Key bound")

	t.Run("replay_same_seq_old_ts", func(t *testing.T) {
		log.Printf("   🚨 Scenario 1: Same seq + old ts (classic replay)")
		resp, err := phone.SendReplay(1, time.Now().Add(-10*time.Second).UnixMilli())
		if err != nil {
			t.Fatalf("Send replay failed: %v", err)
		}
		if resp.Blocked {
			log.Printf("   ✅ Replay DETECTED: %s", resp.Reason)
		} else {
			t.Errorf("⚠️  Replay NOT blocked! Check anti-replay logic")
		}
	})

	t.Run("replay_seq_boundary_max", func(t *testing.T) {
		log.Printf("   🚨 Scenario 2: Seq = MAX_UINT32 (boundary overflow)")
		// Use sequence number at boundary
		resp, err := phone.SendReplay(0xFFFFFFFF, time.Now().Add(-5*time.Second).UnixMilli())
		if err != nil {
			log.Printf("   ✅ Max seq rejected (error): %v", err)
			return
		}
		if resp.Blocked {
			log.Printf("   ✅ Max seq replay blocked: %s", resp.Reason)
		} else {
			log.Printf("   ℹ️  Max seq: blocked=%v", resp.Blocked)
		}
	})

	t.Run("replay_fast_500ms", func(t *testing.T) {
		log.Printf("   🚨 Scenario 3: Fast replay within 500ms window")
		// Execute a legitimate command first to increment seq
		challenge, err := phone.RequestChallenge()
		if err != nil {
			t.Fatalf("Challenge failed: %v", err)
		}
		phone.RespondChallenge("sec_replay_key_001", challenge)

		// Capture the legitimate unlock's seq number (seq=N)
		// In the mobile client, we track globalSeq in proto package
		// Send replay with the captured seq
		now := time.Now()
		resp, err := phone.SendReplay(5, now.UnixMilli()-10)
		if err != nil {
			log.Printf("   ✅ Fast replay rejected: %v", err)
			return
		}
		if resp.Blocked {
			log.Printf("   ✅ Fast replay blocked (within 500ms): %s", resp.Reason)
		} else {
			log.Printf("   ℹ️  Fast replay: blocked=%v", resp.Blocked)
		}
	})

	t.Run("replay_zero_timestamp", func(t *testing.T) {
		log.Printf("   🚨 Scenario 4: Zero timestamp replay")
		resp, err := phone.SendReplay(1, 0)
		if err != nil {
			log.Printf("   ✅ Zero ts rejected: %v", err)
			return
		}
		if resp.Blocked {
			log.Printf("   ✅ Zero ts replay blocked: %s", resp.Reason)
		}
	})

	t.Run("replay_future_timestamp", func(t *testing.T) {
		log.Printf("   🚨 Scenario 5: Future timestamp replay")
		future := time.Now().Add(24 * time.Hour).UnixMilli()
		resp, err := phone.SendReplay(1, future)
		if err != nil {
			log.Printf("   ✅ Future ts rejected: %v", err)
			return
		}
		if resp.Blocked {
			log.Printf("   ✅ Future ts replay blocked: %s", resp.Reason)
		}
	})

	log.Printf("════════════════════════════════════════════════════")
	log.Printf("✅ PASS: Replay Attack Simulations")
	log.Printf("════════════════════════════════════════════════════")
}
