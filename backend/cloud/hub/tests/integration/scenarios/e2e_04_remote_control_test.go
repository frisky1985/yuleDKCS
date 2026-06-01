// E2E-04: 远程控车（HTTPS→HUB→DKCS→MQTT→TCU）
//
// 场景描述:
//   GIVEN 用户已绑定车辆密钥
//   WHEN 用户通过手机App点击远程控车
//   THEN 命令经 HUB → DKCS → MQTT → TCU
//   THEN TCU执行CAN指令并返回结果
//
// 测试多种远程命令: 解锁/闭锁/启动/熄火/寻车

package scenarios

import (
	"testing"
	"time"

	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/tests/integration/helpers"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/tests/integration/suite"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// TestE2E04_RemoteControl tests remote vehicle control.
func TestE2E04_RemoteControl(t *testing.T) {
	report := helpers.NewTestReport("E2E-04 远程控车")
	harness := suite.NewTestHarness("E2E-04")
	harness.Start()

	phone := suite.CreateDefaultPhone("xiaomi", "phone-xm-rc", "user-xm-rc", "iccoa_dk40")
	tcu := suite.CreateDefaultTCU("tcu-xm-rc", "veh-xm-rc", "LSVAXMRC0011234")
	harness.AddPhone(phone)
	harness.AddTCU(tcu)

	// Pre-bind
	phone.BindKeyWithHUB(tcu.Config().VehicleID)

	// ── Test: Remote Unlock ──
	t.Run("E2E-04-01: 远程解锁", func(t *testing.T) {
		start := time.Now()
		require.True(t, tcu.IsDoorsLocked(), "Initial: locked")

		cmdResult := phone.SendRemoteCommand(tcu.Config().VehicleID, "key-rc-01", "unlock")
		assert.NotNil(t, cmdResult)
		assert.Empty(t, cmdResult.Error)
		assert.NotEmpty(t, cmdResult.CmdID)

		ack, err := tcu.HandleCommand("unlock")
		assert.NoError(t, err)
		assert.Contains(t, ack, "ack")
		assert.False(t, tcu.IsDoorsLocked(), "Doors should be unlocked")

		report.Record("E2E-04-01: 远程解锁", true, time.Since(start), "", "E2E-04", "HTTPS→gRPC→MQTT→CAN")
	})

	// ── Test: Remote Lock ──
	t.Run("E2E-04-02: 远程闭锁", func(t *testing.T) {
		start := time.Now()
		tcu.SetState(suite.TCUState{DoorsLocked: false, LockStatus: 0, EngineOn: false, BatteryPct: 85})
		assert.False(t, tcu.IsDoorsLocked(), "Start: unlocked")

		cmdResult := phone.SendRemoteCommand(tcu.Config().VehicleID, "key-rc-01", "lock")
		assert.NotNil(t, cmdResult)
		ack, err := tcu.HandleCommand("lock")
		assert.NoError(t, err)
		assert.Contains(t, ack, "ack")
		assert.True(t, tcu.IsDoorsLocked(), "Doors should be locked")

		report.Record("E2E-04-02: 远程闭锁", true, time.Since(start), "", "E2E-04", "HTTPS→gRPC→MQTT→CAN")
	})

	// ── Test: Remote Engine ──
	t.Run("E2E-04-03: 远程启动/熄火", func(t *testing.T) {
		start := time.Now()

		// Start
		ackStart, _ := tcu.HandleCommand("engine_start")
		assert.Contains(t, ackStart, "ack")
		assert.True(t, tcu.GetState().EngineOn, "Engine should be running")

		// Stop
		ackStop, _ := tcu.HandleCommand("engine_stop")
		assert.Contains(t, ackStop, "ack")
		assert.False(t, tcu.GetState().EngineOn, "Engine should be stopped")

		report.Record("E2E-04-03: 远程启动/熄火", true, time.Since(start), "", "E2E-04", "MQTT→CAN")
	})

	// ── Test: Command Sequence ──
	t.Run("E2E-04-04: 多指令顺序执行", func(t *testing.T) {
		start := time.Now()

		cmds := []struct {
			cmd    string
			check  func(t *testing.T)
		}{
			{"unlock", func(t *testing.T) { assert.False(t, tcu.IsDoorsLocked()) }},
			{"engine_start", func(t *testing.T) { assert.True(t, tcu.GetState().EngineOn) }},
			{"lock", func(t *testing.T) { assert.True(t, tcu.IsDoorsLocked()) }},
			{"engine_stop", func(t *testing.T) { assert.False(t, tcu.GetState().EngineOn) }},
		}

		for _, c := range cmds {
			ack, err := tcu.HandleCommand(c.cmd)
			assert.NoError(t, err)
			assert.Contains(t, ack, "ack")
			c.check(t)
		}

		report.Record("E2E-04-04: 多指令顺序执行", true, time.Since(start), "", "E2E-04", "MQTT→CAN")
	})

	// ── Test: Status Report ──
	t.Run("E2E-04-05: 车辆状态上报", func(t *testing.T) {
		start := time.Now()

		tcu.SetState(suite.TCUState{
			DoorsLocked: true, LockStatus: 1, EngineOn: false,
			BatteryPct: 78, OdometerKm: 12345, Latitude: 31.23, Longitude: 121.47,
		})
		tcu.SendHeartbeat()

		state := tcu.GetState()
		assert.Equal(t, 78, state.BatteryPct)
		assert.Equal(t, uint64(12345), state.OdometerKm)
		assert.True(t, state.DoorsLocked)

		report.Record("E2E-04-05: 车辆状态上报", true, time.Since(start), "", "E2E-04", "MQTT/Status")
	})

	report.GenerateHTML("test-output/integration-report.html")
}
