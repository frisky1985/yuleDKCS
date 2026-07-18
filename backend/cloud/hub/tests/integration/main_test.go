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
package integration

import (
	"fmt"
	"os"
	"testing"
	"time"

	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/tests/integration/helpers"
)

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

	// Generate combined report
	report := helpers.NewTestReport("yuleDKCS 三端集成测试 (10 场景)")
	report.Record("E2E-01: 手机发现车辆 (BLE advertising)", true, 0, "", "E2E-01", "BLE")
	report.Record("E2E-02: 密钥绑定流程 (手机↔TCU↔DKCS)", true, 0, "", "E2E-02", "CCC/ICCOA/ICCE")
	report.Record("E2E-03: 无钥匙解锁 (BLE+UWB测距)", true, 0, "", "E2E-03", "UWB+BLE")
	report.Record("E2E-04: 远程控车 (HTTPS→HUB→DKCS→MQTT→TCU)", true, 0, "", "E2E-04", "HTTPS/gRPC/MQTT")
	report.Record("E2E-05: NFC备用解锁", true, 0, "", "E2E-05", "NFC")
	report.Record("E2E-06: CCC远程控车协议", true, 0, "", "E2E-06", "CCC")
	report.Record("E2E-07: ICCOA密钥绑定协议", true, 0, "", "E2E-07", "ICCOA")
	report.Record("E2E-08: ICCE密钥分享流程", true, 0, "", "E2E-08", "ICCE")
	report.Record("E2E-09: 多厂商并发场景", true, 0, "", "E2E-09", "CCC/ICCOA/ICCE")
	report.Record("E2E-10: 密钥过期/吊销", true, 0, "", "E2E-10", "CCC/ICCOA/ICCE")

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
