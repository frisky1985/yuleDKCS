// Package integration contains the yuleDKCS end-to-end integration test suite.
//
// The framework simulates three endpoints communicating with real HUB + DKCS services:
//   - Phone client:     Simulates mobile app BLE/UWB/NFC communication
//   - TCU agent:        Simulates vehicle TCU communication (MQTT + CAN bus)
//   - Cloud services:   HUB gRPC → DKCS → Adapter flow (simulated in-process)
//
// Run all tests:
//   cd backend/cloud/hub/tests/integration
//   go test -v -count=1 ./...
//
// Run a single scenario:
//   go test -v -run TestE2E02 ./...
//
// Generate HTML report (auto-generated after each test):
//   go test -v -count=1 ./... && open test-output/integration-report.html
//
// NOTE: the top-level hub API tests (TestHealthEndpoint, TestGrpcConnectivity,
// TestLoginEndpoint, TestAuthProtectedEndpoint) are BEST-EFFORT: they probe for
// a running yuleHUB first, otherwise they build and start their own hub
// instance on alternate ports. When the hub binary cannot be built/started
// they SKIP instead of failing the suite (see requireHub).
package integration

import (
	"fmt"
	"os"
	"testing"
	"time"

	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/tests/integration/helpers"
)

// recordHubTest defers a real pass/fail/skip + duration entry for a hub API
// test into the shared report. Each hub API test calls it as its first
// statement:
//
//	start := time.Now()
//	defer recordHubTest(t, "gRPC 连通性", "HUB-API", "gRPC", start)
func recordHubTest(t *testing.T, name, scenario, protocol string, start time.Time) {
	report := helpers.NewTestReport("yuleDKCS HUB API 集成测试")
	switch {
	case t.Skipped():
		report.RecordSkipped(name, "SKIPPED — 环境无运行中 hub 服务 (best-effort)", scenario, protocol, time.Since(start))
	case t.Failed():
		report.Record(name, false, time.Since(start), "FAILED", scenario, protocol)
	default:
		report.Record(name, true, time.Since(start), "", scenario, protocol)
	}
}

// TestMain is the integration test suite entry point.
func TestMain(m *testing.M) {
	fmt.Println("╔══════════════════════════════════════════════════════════╗")
	fmt.Println("║       yuleDKCS 集成测试套件                               ║")
	fmt.Println("║       三端集成: 手机 ↔ HUB ↔ DKCS ↔ MQTT ↔ TCU            ║")
	fmt.Println("║       模拟 BLE/UWB/NFC 无线通信                           ║")
	fmt.Println("╚══════════════════════════════════════════════════════════╝")
	fmt.Println()

	startTime := time.Now()
	exitCode := m.Run()
	elapsed := time.Since(startTime)

	fmt.Println()
	fmt.Println("╔══════════════════════════════════════════════════════════╗")
	fmt.Printf("║  测试完成: %s", padRight(fmt.Sprintf("总耗时 %s", fmtDuration(elapsed)), 45))
	fmt.Println("║")
	fmt.Println("╚══════════════════════════════════════════════════════════╝")

	// Generate combined report from ACTUAL test results (each hub API test
	// records its own pass/fail + duration via recordHubTest). No hardcoded
	// rows — the report reflects what really ran.
	report := helpers.NewTestReport("yuleDKCS HUB API 集成测试 (真实结果)")
	if err := report.GenerateHTML("test-output/integration-report.html"); err != nil {
		fmt.Fprintf(os.Stderr, "Warning: failed to generate HTML report: %v\n", err)
	} else {
		fmt.Println("📊 集成测试报告: test-output/integration-report.html")
	}

	os.Exit(exitCode)
}

func padRight(s string, length int) string {
	if len(s) >= length {
		return s
	}
	b := make([]byte, length)
	for i := range b {
		if i < len(s) {
			b[i] = s[i]
		} else {
			b[i] = ' '
		}
	}
	return string(b)
}

func fmtDuration(d time.Duration) string {
	if d < time.Second {
		return fmt.Sprintf("%dms", d.Milliseconds())
	}
	m := int(d.Minutes())
	s := int(d.Seconds()) % 60
	if m > 0 {
		return fmt.Sprintf("%dm%ds", m, s)
	}
	return fmt.Sprintf("%.1fs", d.Seconds())
}
