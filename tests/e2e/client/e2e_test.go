package client

import (
	"log"
	"net"
	"os"
	"os/exec"
	"testing"
	"time"
)

// TestE2EAll runs all 8 E2E test scenarios sequentially.
func TestE2EAll(t *testing.T) {
	log.Printf("╔══════════════════════════════════════════════════╗")
	log.Printf("║     🔥 yuleDKCS E2E Verification Suite        ║")
	log.Printf("║     8 Scenarios · Full Protocol Stack          ║")
	log.Printf("╚══════════════════════════════════════════════════╝")
	log.Printf("")

	carAddr := getCarAddr()
	if carAddr == "" {
		carAddr = "localhost:18001"
	}

	log.Printf("🔍 Checking car simulator at %s...", carAddr)
	if err := checkCarSimulator(carAddr); err != nil {
		t.Skipf("Car simulator not running at %s: %v", carAddr, err)
		return
	}
	log.Printf("✅ Car simulator is running at %s", carAddr)

	checkCloudServices()

	runScenarioSubTests(t)
}

func runScenarioSubTests(t *testing.T) {
	scenarios := []struct {
		name string
		run  string
	}{
		{"01_KeyBinding", "TestKeyBinding"},
		{"02_PassiveUnlock", "TestPassiveUnlock"},
		{"03_NFCUnlock", "TestNFCUnlock"},
		{"04_RemoteControl", "TestRemoteControl"},
		{"05_KeySharing", "TestKeySharing"},
		{"06_KeyRevocation", "TestKeyRevocation"},
		{"07_ReplayProtection", "TestSecurityReplay"},
		{"08_OfflineMode", "TestOfflineMode"},
	}

	for _, sc := range scenarios {
		t.Run(sc.name, func(t *testing.T) {
			cmd := exec.Command("go", "test", "-run", sc.run, "-v", "-count=1", "-timeout", "30s", "../scenarios/")
			cmd.Env = append(os.Environ(), "CARSIM_ADDR="+getCarAddr())
			out, err := cmd.CombinedOutput()
			log.Printf("📋 Scenario %s output:\n%s", sc.name, string(out))
			if err != nil {
				t.Errorf("❌ Scenario %s FAILED: %v", sc.name, err)
			}
		})
	}
}

func getCarAddr() string {
	addr := os.Getenv("CARSIM_ADDR")
	if addr == "" {
		return "localhost:18001"
	}
	return addr
}

func checkCarSimulator(addr string) error {
	conn, err := net.DialTimeout("tcp", addr, 2*time.Second)
	if err != nil {
		return err
	}
	conn.Close()
	return nil
}

func checkCloudServices() {
	services := []string{
		"localhost:8080", // Hub gRPC
		"localhost:5432", // PostgreSQL
		"localhost:6379", // Redis
	}
	for _, s := range services {
		conn, err := net.DialTimeout("tcp", s, 500*time.Millisecond)
		if err != nil {
			log.Printf("ℹ️  Cloud service %s not available: %v", s, err)
		} else {
			conn.Close()
			log.Printf("✅ Cloud service %s available", s)
		}
	}
}
