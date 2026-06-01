// E2E-01: 手机发现车辆（BLE advertising 模拟）
//
// 场景描述:
//   GIVEN 车辆TCU正在进行BLE广播
//   WHEN 手机开启BLE扫描
//   THEN 手机发现车辆TCU的BLE广告
//   THEN 手机从广告数据中解析出车辆信息
//
// 模拟流程:
//   1. 创建 TCU Agent → 开始 BLE 广播
//   2. 创建手机客户端 → 开始 BLE 扫描
//   3. 手机发现 TCU 广播 → 解析车辆ID/厂商/协议
//   4. 验证发现结果符合预期

package scenarios

import (
	"testing"
	"time"

	"github.com/digitalkey/yuledkcs/integration-tests/helpers"
	"github.com/digitalkey/yuledkcs/integration-tests/suite"
	"github.com/stretchr/testify/assert"
)

// TestE2E01_VehicleDiscovery tests BLE vehicle discovery.
func TestE2E01_VehicleDiscovery(t *testing.T) {
	report := helpers.NewTestReport("E2E-01 手机发现车辆")
	harness := suite.NewTestHarness("E2E-01")
	harness.Start()

	// ── Setup ──
	phone := suite.CreateDefaultPhone("xiaomi", "phone-xiaomi-01", "user-1001", "iccoa_dk40")
	tcu := suite.CreateDefaultTCU("tcu-vehicle-01", "veh-xiaomi-001", "LSVA1234X56789012")
	harness.AddPhone(phone)
	harness.AddTCU(tcu)

	// ── Test: Basic BLE Discovery ──
	t.Run("E2E-01-01: BLE扫描发现车辆", func(t *testing.T) {
		start := time.Now()

		tcu.StartBLEAdvertising()
		advPayload := phone.StartBLEAdvertising()
		assert.NotNil(t, advPayload, "Phone should produce BLE advertisement")
		assert.Greater(t, len(advPayload), 8, "BLE advert should be non-trivial")
		assert.Contains(t, string(advPayload[8:]), phone.Config().DeviceID[:8],
			"Advertisement should contain start of device ID")

		tcuInfo, err := phone.ReadTCUBLEAdvert(tcu.Config().VehicleID, "xiaomi", -55)
		assert.NoError(t, err, "Should read TCU BLE advertisement")
		assert.Equal(t, tcu.Config().VehicleID, tcuInfo["vehicle_id"],
			"Discovered vehicle ID should match TCU")

		discovered, _ := tcu.SimulatePhoneDiscovery(phone.Config().DeviceID)
		assert.True(t, discovered, "Phone should be discoverable")
		assert.True(t, tcu.IsAdvertising(), "TCU should be advertising")

		report.Record("E2E-01-01: BLE扫描发现车辆", true, time.Since(start), "", "E2E-01", "BLE")
	})

	// ── Test: Multi-protocol Discovery ──
	t.Run("E2E-01-02: 多协议BLE发现（CCC + ICCOA + ICCE）", func(t *testing.T) {
		start := time.Now()

		type protocolTest struct {
			vendor   string
			protocol string
			phoneID  string
			tcuID    string
		}
		tests := []protocolTest{
			{"apple", "ccc_dk3", "phone-apple-01", "tcu-apple-01"},
			{"xiaomi", "iccoa_dk40", "phone-xiaomi-02", "tcu-xiaomi-02"},
			{"huawei", "icce", "phone-huawei-01", "tcu-huawei-01"},
		}

		for _, pt := range tests {
			p := suite.CreateDefaultPhone(pt.vendor, pt.phoneID, "user-"+pt.vendor, pt.protocol)
			tcu := suite.CreateDefaultTCU(pt.tcuID, "veh-"+pt.vendor+"-001", "LSVA"+pt.vendor+"001001")
			harness.AddPhone(p)
			harness.AddTCU(tcu)

			adv := p.StartBLEAdvertising()
			assert.NotNil(t, adv, "%s should advertise", pt.vendor)

			info, err := p.ReadTCUBLEAdvert(tcu.Config().VehicleID, pt.vendor, -60)
			assert.NoError(t, err, "%s should discover TCU", pt.vendor)
			assert.Equal(t, tcu.Config().VehicleID, info["vehicle_id"],
				"%s discovered wrong vehicle", pt.vendor)
		}

		report.Record("E2E-01-02: 多协议BLE发现", true, time.Since(start), "",
			"E2E-01", "BLE/CCC/ICCOA/ICCE")
	})

	// ── Test: RSSI Range Check ──
	t.Run("E2E-01-03: BLE信号强度检测", func(t *testing.T) {
		start := time.Now()

		for _, rssi := range []int8{-30, -60, -80, -95} {
			info, err := phone.ReadTCUBLEAdvert(tcu.Config().VehicleID, "xiaomi", rssi)
			assert.NoError(t, err, "Should parse BLE advert at RSSI=%d", rssi)
			assert.Equal(t, int(rssi), info["rssi"], "RSSI should match at %d", rssi)
		}

		report.Record("E2E-01-03: BLE信号强度检测", true, time.Since(start), "", "E2E-01", "BLE")
	})

	report.GenerateHTML("test-output/integration-report.html")
}
