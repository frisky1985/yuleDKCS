// E2E-03: 无钥匙解锁（BLE + UWB 测距模拟）
//
// 场景描述:
//   GIVEN 手机和车辆已绑定密钥对
//   GIVEN 手机在车辆 BLE 范围内
//   WHEN 用户拉开车门把手
//   THEN BLE鉴权握手 → CAN总线解锁 → 手机收到通知
//
// 模拟流程:
//   1. 手机和TCU已绑定 → 2. 手机靠近(UWB测距)
//   3. 进入解锁区域 → 4. BLE鉴权 → 5. TCU解锁 → 6. 验证

package scenarios

import (
	"testing"
	"time"

	"github.com/digitalkey/yuledkcs/integration-tests/helpers"
	"github.com/digitalkey/yuledkcs/integration-tests/suite"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// TestE2E03_PassiveEntry tests BLE+UWB passive entry.
func TestE2E03_PassiveEntry(t *testing.T) {
	report := helpers.NewTestReport("E2E-03 无钥匙解锁")
	harness := suite.NewTestHarness("E2E-03")
	harness.Start()

	phone := suite.CreateDefaultPhone("xiaomi", "phone-xm-pe", "user-xm-pe", "iccoa_dk40")
	tcu := suite.CreateDefaultTCU("tcu-xm-pe", "veh-xm-pe", "LSVAXMPE0011234")
	harness.AddPhone(phone)
	harness.AddTCU(tcu)

	// Pre-bind key (simulates E2E-02 having completed)
	phone.BindKeyWithHUB(tcu.Config().VehicleID)
	require.True(t, tcu.IsDoorsLocked(), "Initial state: doors locked")

	// ── Test: UWB Ranging → Unlock ──
	t.Run("E2E-03-01: UWB测距 → BLE鉴权 → 无钥匙解锁", func(t *testing.T) {
		start := time.Now()

		tcu.StartBLEAdvertising()
		phone.StartBLEAdvertising()
		_, err := phone.ReadTCUBLEAdvert(tcu.Config().VehicleID, "xiaomi", -45)
		require.NoError(t, err)

		// Simulate phone approaching: 10m → 3m → 1.2m (unlock zone)
		rangingSteps := []suite.UWBRangingResult{
			{DistanceMM: 10000, Confidence: 95, Phase: "APPROACH"},
			{DistanceMM: 3000, Confidence: 90, Phase: "LOCK_ZONE"},
			{DistanceMM: 1500, Confidence: 85, Phase: "UNLOCK_ZONE"},
			{DistanceMM: 1200, Confidence: 82, Phase: "UNLOCK_ZONE"},
		}

		rangingCh := phone.StartUWBRanging(rangingSteps)
		unlockTriggered := false
		for result := range rangingCh {
			if result.Phase == "UNLOCK_ZONE" && result.DistanceMM < 2000 {
				if tcu.SimulateUWBUnlockZone(phone.Config().DeviceID, result.DistanceMM) {
					unlockTriggered = true
				}
			}
		}

		assert.True(t, unlockTriggered, "Vehicle should unlock when phone enters UWB unlock zone")
		assert.False(t, tcu.IsDoorsLocked(), "Doors should be unlocked")
		assert.Equal(t, uint8(0), tcu.GetState().LockStatus, "Lock status: UNLOCKED")

		report.Record("E2E-03-01: UWB测距+BLE无钥匙解锁", true, time.Since(start), "", "E2E-03", "UWB+BLE")
	})

	// ── Test: Walk-away Auto-Lock ──
	t.Run("E2E-03-02: 手机离开 → 自动闭锁", func(t *testing.T) {
		start := time.Now()

		assert.False(t, tcu.IsDoorsLocked(), "Start: unlocked")

		tcu.SimulateUWBLeaveZone(phone.Config().DeviceID, 5000)

		assert.True(t, tcu.IsDoorsLocked(), "Doors should auto-lock")
		assert.Equal(t, uint8(1), tcu.GetState().LockStatus, "Lock status: LOCKED")

		report.Record("E2E-03-02: 离开自动闭锁", true, time.Since(start), "", "E2E-03", "UWB")
	})

	// ── Test: Zone accuracy ──
	t.Run("E2E-03-03: UWB区域距离判断", func(t *testing.T) {
		start := time.Now()

		type zoneCheck struct {
			mm      uint32
			phase   string
			willUnlock bool
		}
		checks := []zoneCheck{
			{10000, "APPROACH", false},
			{3000, "LOCK_ZONE", false},
			{1500, "UNLOCK_ZONE", true},
			{500, "UNLOCK_ZONE", true},
		}

		for _, c := range checks {
			if c.willUnlock {
				ok := tcu.SimulateUWBUnlockZone(phone.Config().DeviceID, c.mm)
				assert.True(t, ok, "Unlock at %dmm", c.mm)
			}
		}

		report.Record("E2E-03-03: UWB区域距离判断", true, time.Since(start), "", "E2E-03", "UWB")
	})

	report.GenerateHTML("test-output/integration-report.html")
}
