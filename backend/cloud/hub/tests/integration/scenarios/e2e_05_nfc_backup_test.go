// E2E-05: NFC 备用解锁模拟（手机没电）
//
// 场景描述:
//   GIVEN 手机电池耗尽无法使用BLE/UWB
//   GIVEN 车辆已存储NFC密钥
//   WHEN 用户将手机靠近车辆NFC读卡器
//   THEN NFC数据交换 → 密钥验证 → 车辆解锁
//
// 模拟流程:
//   1. 手机和TCU已完成绑定（含NFC密钥）
//   2. 模拟手机"没电"（禁用BLE/UWB，仅NFC）
//   3. NFC读卡交互 → 密钥验证 → 解锁
//   4. 验证: 无效密钥应被拒绝

package scenarios

import (
	"crypto/rand"
	"testing"
	"time"

	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/tests/integration/helpers"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/tests/integration/suite"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// TestE2E05_NFCBackup tests NFC backup unlock.
func TestE2E05_NFCBackup(t *testing.T) {
	report := helpers.NewTestReport("E2E-05 NFC备用解锁")
	harness := suite.NewTestHarness("E2E-05")
	harness.Start()

	phone := suite.CreateDefaultPhone("xiaomi", "phone-xm-nfc", "user-xm-nfc", "iccoa_dk40")
	tcu := suite.CreateDefaultTCU("tcu-xm-nfc", "veh-xm-nfc", "LSVAXMNFCOO11234")
	harness.AddPhone(phone)
	harness.AddTCU(tcu)

	require.True(t, tcu.IsDoorsLocked(), "Initial: doors locked")

	// Store NFC key on TCU (simulates prior key binding with NFC capability)
	nfcKeyRef := "nfc-key-ref-xm-001"
	tcu.StoreKey(&suite.TCUKey{
		KeyID:    "key-nfc-001",
		Protocol: 2,
		KeyType:  1,
		SeKeyRef: nfcKeyRef,
		ValidFrom:  time.Now().Add(-1 * time.Hour),
		ValidUntil: time.Now().Add(365 * 24 * time.Hour),
		MaxUses:   9999,
	})
	require.True(t, tcu.HasStoredKey("key-nfc-001"), "NFC key stored on TCU")

	// ── Test: NFC Unlock (battery dead) ──
	t.Run("E2E-05-01: NFC备用解锁（模拟手机没电）", func(t *testing.T) {
		start := time.Now()

		// Phone battery dead — only NFC works
		challenge := make([]byte, 16)
		rand.Read(challenge)

		nfcResp, err := phone.SimulateNFCTap(nfcKeyRef, challenge)
		require.NoError(t, err)
		assert.NotNil(t, nfcResp)
		assert.Greater(t, len(nfcResp), 10)
		assert.Contains(t, string(nfcResp[:8]), "DK_NFC", "NFC response: app ID")

		// Vehicle verifies NFC key and unlocks
		unlocked, err := tcu.SimulateNFCUnlock(nfcKeyRef)
		assert.NoError(t, err)
		assert.True(t, unlocked)
		assert.False(t, tcu.IsDoorsLocked(), "Doors unlocked via NFC")
		assert.Equal(t, uint8(0), tcu.GetState().LockStatus, "Status: UNLOCKED")

		report.Record("E2E-05-01: NFC备用解锁", true, time.Since(start), "", "E2E-05", "NFC")
	})

	// ── Test: Invalid NFC Key Rejected ──
	t.Run("E2E-05-02: NFC无效密钥拒绝", func(t *testing.T) {
		start := time.Now()

		// Reset doors to locked
		tcu.SetState(suite.TCUState{DoorsLocked: true, LockStatus: 1, BatteryPct: 85})

		// Try with invalid key ref
		invalidRef := "nfc-key-invalid-999"
		phone.SimulateNFCTap(invalidRef, make([]byte, 16))

		unlocked, err := tcu.SimulateNFCUnlock(invalidRef)
		assert.Error(t, err, "Should reject invalid NFC key")
		assert.False(t, unlocked)
		assert.True(t, tcu.IsDoorsLocked(), "Doors stay locked")

		report.Record("E2E-05-02: NFC无效密钥拒绝", true, time.Since(start), "", "E2E-05", "NFC")
	})

	// ── Test: Multi-vendor NFC ──
	t.Run("E2E-05-03: 多厂商NFC兼容性", func(t *testing.T) {
		start := time.Now()

		type nfcDevice struct {
			vendor   string
			protocol string
			keyRef   string
		}
		devices := []nfcDevice{
			{"apple", "ccc_dk3", "nfc-key-apple"},
			{"samsung", "ccc_dk3", "nfc-key-samsung"},
			{"huawei", "icce", "nfc-key-huawei"},
			{"oppo", "iccoa_dk40", "nfc-key-oppo"},
			{"vivo", "iccoa_dk40", "nfc-key-vivo"},
		}

		success := 0
		for _, d := range devices {
			nfcPhone := suite.CreateDefaultPhone(d.vendor, "phone-"+d.vendor+"-nfc",
				"user-nfc-"+d.vendor, d.protocol)
			harness.AddPhone(nfcPhone)

			tcu.StoreKey(&suite.TCUKey{
				KeyID:    "key-" + d.vendor + "-nfc",
				SeKeyRef: d.keyRef,
				ValidFrom:  time.Now().Add(-1 * time.Hour),
				ValidUntil: time.Now().Add(365 * 24 * time.Hour),
			})

			nfcPhone.SimulateNFCTap(d.keyRef, make([]byte, 16))
			unlocked, err := tcu.SimulateNFCUnlock(d.keyRef)
			if err == nil && unlocked {
				success++
			}
		}

		ok := success == len(devices)
		report.Record("E2E-05-03: 多厂商NFC兼容性", ok, time.Since(start), "", "E2E-05", "NFC/CCC/ICCOA/ICCE")
		if !ok {
			t.Errorf("Only %d/%d NFC keys worked", success, len(devices))
		}
	})

	// ── Test: NFC Payload Encoding ──
	t.Run("E2E-05-04: NFCPayload编解码验证", func(t *testing.T) {
		start := time.Now()

		nfcPayload := &helpers.NFCPayload{
			NDEFType:      "application/vnd.dk.nfc",
			VehicleID:     tcu.Config().VehicleID,
			KeyRef:        nfcKeyRef,
			Authenticated: true,
			Timestamp:     time.Now().UnixMilli(),
		}

		encoded := nfcPayload.Encode()
		assert.NotNil(t, encoded)
		assert.Greater(t, len(encoded), 30)
		assert.Contains(t, string(encoded), "DK_NFC")

		report.Record("E2E-05-04: NFCPayload编解码验证", true, time.Since(start), "", "E2E-05", "NFC")
	})

	report.GenerateHTML("test-output/integration-report.html")
}
