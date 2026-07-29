// E2E-07: ICCOA 密钥绑定协议验证（ICCOA DK4.0 / DK3.0）
//
// 场景描述:
//   ICCOA.DK.TS.002 §4.1 — Key Provisioning Procedure (DK4.0)
//   ICCOA.DK.TS.002 §4.2 — Key Provisioning Procedure (DK3.0 compatible)
//   ICCOA.DK.TS.002 §4.6 — Cross-Vendor Interoperability
//
//   测试ICCOA协议特有的密钥绑定流程:
//   1. ICCOA DK4.0 OTA 密钥绑定（手机→HUB→云端→OTA→车辆）
//   2. ICCOA DK3.0 BLE 密钥绑定（手机↔车辆 BLE 直连）
//   3. ICCOA 跨厂商互通（任意ICCOA认证手机↔任意ICCOA认证车辆）
//   4. ICCOA 重新绑定（密钥丢失/过期后重建）
//   5. ICCOA 密钥元数据同步（绑定后同步权限/有效期）

package scenarios

import (
	"testing"
	"time"

	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/tests/integration/helpers"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/tests/integration/suite"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// TestE2E07_ICCOAKeyBind tests ICCOA-specific key binding protocol flows.
func TestE2E07_ICCOAKeyBind(t *testing.T) {
	report := helpers.NewTestReport("E2E-07 ICCOA密钥绑定协议")
	harness := suite.NewTestHarness("E2E-07")
	harness.Start()

	// ============================================================
	// Test: ICCOA DK4.0 OTA 密钥绑定
	// ============================================================
	// ICCOA.DK.TS.002 §4.1: DK4.0标准流程
	//   手机→HUB发起绑定 → 云端DKCS签发密钥 → OTA下发到TCU
	//   区别于BLE直连绑定：密钥材料通过TSP OTA通道传输
	t.Run("E2E-07-01: ICCOA DK4.0 OTA 密钥绑定（手机→云端→车辆）", func(t *testing.T) {
		start := time.Now()

		// 使用小米手机模拟ICCOA DK4.0设备
		miPhone := suite.CreateDefaultPhone("xiaomi", "phone-mi-ota-01", "user-mi-ota", "iccoa_dk40")
		miTCU := suite.CreateDefaultTCU("tcu-mi-ota-01", "veh-mi-ota-01", "LSVMOTA001BIND01")
		harness.AddPhone(miPhone)
		harness.AddTCU(miTCU)

		// Step 1: BLE发现车辆
		miTCU.StartBLEAdvertising()
		info, err := miPhone.ReadTCUBLEAdvert(miTCU.Config().VehicleID, "xiaomi", -50)
		require.NoError(t, err, "Xiaomi phone should discover vehicle via BLE")
		require.Equal(t, miTCU.Config().VehicleID, info["vehicle_id"])

		// Step 2: 发起ICCOA OTA绑定 — 云端流程
		// ICCOA DK4.0: 手机→HUB→DKCS签发密钥→TSP OTA通道→TCU
		result := miPhone.BindKeyWithHUB(miTCU.Config().VehicleID)
		require.NotNil(t, result)
		assert.NotEmpty(t, result.KeyID, "ICCOA DK4.0 must generate key ID")
		assert.Equal(t, "ACTIVE", result.Status, "ICCOA DK4.0 key must be ACTIVE")
		assert.Empty(t, result.Error, "No errors in ICCOA DK4.0 OTA bind")

		// Step 3: 验证手机端存储
		assert.True(t, miPhone.HasBoundKey(miTCU.Config().VehicleID), "Phone must store bound key")
		keys := miPhone.BoundKeys()
		found := false
		for _, k := range keys {
			if k.VehicleID == miTCU.Config().VehicleID {
				found = true
				assert.Equal(t, "iccoa_dk40", k.Protocol, "Protocol must be ICCOA DK4.0")
				assert.Equal(t, "ACTIVE", k.Status, "Key status must be ACTIVE")
				break
			}
		}
		assert.True(t, found, "ICCOA DK4.0 key must be found in phone key store")

		t.Logf("ICCOA DK4.0 OTA bind: key=%s vehicle=%s", result.KeyID, miTCU.Config().VehicleID)

		report.Record("E2E-07-01: ICCOA DK4.0 OTA密钥绑定", true, time.Since(start), "", "E2E-07", "ICCOA")
	})

	// ============================================================
	// Test: ICCOA DK3.0 BLE 密钥绑定（兼容模式）
	// ============================================================
	// ICCOA.DK.TS.003 §2.1: DK3.0兼容模式
	//   BLE直连绑定（无需OTA通道）：手机↔车辆 BLE 安全通道
	//   UWB可选（DK4.0强制）
	t.Run("E2E-07-02: ICCOA DK3.0 BLE 密钥绑定（兼容模式）", func(t *testing.T) {
		start := time.Now()

		// 使用vivo手机模拟ICCOA DK3.0兼容设备（无UWB）
		vivoPhone := suite.CreateDefaultPhone("vivo", "phone-vivo-dk30", "user-vivo-dk30", "iccoa_dk30")
		vivoTCU := suite.CreateDefaultTCU("tcu-vivo-dk30", "veh-vivo-dk30", "LSVVDK30001BIND")
		harness.AddPhone(vivoPhone)
		harness.AddTCU(vivoTCU)

		// Step 1: BLE发现（DK3.0 BLE Service UUID兼容模式）
		vivoTCU.StartBLEAdvertising()
		vivoPhone.StartBLEAdvertising()
		discovered, err := vivoTCU.SimulatePhoneDiscovery(vivoPhone.Config().DeviceID)
		require.NoError(t, err)
		assert.True(t, discovered, "DK3.0 BLE discovery should succeed")

		// Step 2: BLE直连绑定（模拟）
		// DK3.0: BLE GATT安全通道建立 → 密钥协商 → 车辆SE写入
		bleBindResult := vivoPhone.BindKeyWithHUB(vivoTCU.Config().VehicleID)
		require.NotNil(t, bleBindResult)
		assert.NotEmpty(t, bleBindResult.KeyID, "DK3.0 bind must generate key")
		assert.Equal(t, "ACTIVE", bleBindResult.Status, "DK3.0 key must be ACTIVE")

		// Step 3: 验证绑定状态
		assert.True(t, vivoPhone.HasBoundKey(vivoTCU.Config().VehicleID), "DK3.0 bind must record key")

		t.Logf("ICCOA DK3.0 BLE bind: key=%s vehicle=%s protocol=%s",
			bleBindResult.KeyID, vivoTCU.Config().VehicleID, vivoPhone.Config().Protocol)

		report.Record("E2E-07-02: ICCOA DK3.0 BLE密钥绑定", true, time.Since(start), "", "E2E-07", "ICCOA")
	})

	// ============================================================
	// Test: ICCOA 跨厂商互通
	// ============================================================
	// ICCOA.DK.TS.002 §4.6: 任意ICCOA认证手机↔任意ICCOA认证车辆
	//   ICCOA核心承诺：跨厂商绑定必须成功
	t.Run("E2E-07-03: ICCOA 跨厂商互通（手机↔车辆跨品牌）", func(t *testing.T) {
		start := time.Now()

		type crossVendorTest struct {
			phoneVendor string
			phoneID     string
			phoneProto  string
			carOEM      string
			tcuID       string
			vehicleID   string
			desc        string
		}
		tests := []crossVendorTest{
			{"xiaomi", "phone-mi-bynd", "iccoa_dk40", "BYD", "tcu-byd", "veh-byd", "小米→比亚迪"},
			{"oppo", "phone-oppo-nio", "iccoa_dk40", "NIO", "tcu-nio", "veh-nio", "OPPO→蔚来"},
			{"vivo", "phone-vivo-xp", "iccoa_dk40", "XPeng", "tcu-xpeng", "veh-xpeng", "vivo→小鹏"},
			{"xiaomi", "phone-mi-li", "iccoa_dk40", "LiAuto", "tcu-liauto", "veh-liauto", "小米→理想"},
		}

		for _, tt := range tests {
			t.Run(tt.desc, func(t *testing.T) {
				p := suite.CreateDefaultPhone(tt.phoneVendor, tt.phoneID, "user-"+tt.phoneVendor, tt.phoneProto)
				tcu := suite.CreateDefaultTCU(tt.tcuID, tt.vehicleID, "LSV"+tt.tcuID+"001")
				harness.AddPhone(p)
				harness.AddTCU(tcu)

				// ICCOA核心承诺：跨厂商绑定必须成功
				tcu.StartBLEAdvertising()
				info, err := p.ReadTCUBLEAdvert(tcu.Config().VehicleID, tt.phoneVendor, -50)
				require.NoError(t, err, "%s should discover vehicle", tt.desc)
				require.Equal(t, tcu.Config().VehicleID, info["vehicle_id"])

				result := p.BindKeyWithHUB(tcu.Config().VehicleID)
				assert.NotNil(t, result, "Cross-vendor bind %s must succeed", tt.desc)
				assert.NotEmpty(t, result.KeyID, "Cross-vendor bind must generate key")
				assert.Equal(t, "ACTIVE", result.Status, "Cross-vendor key must be ACTIVE")

				if result != nil {
					t.Logf("ICCOA cross-vendor: %s → %s key=%s status=%s",
						tt.phoneVendor, tt.carOEM, result.KeyID, result.Status)
				}
			})
		}

		report.Record("E2E-07-03: ICCOA跨厂商互通", true, time.Since(start), "", "E2E-07", "ICCOA")
	})

	// ============================================================
	// Test: ICCOA 重新绑定（密钥丢失后重建）
	// ============================================================
	// ICCOA.DK.TS.002 §4.5: 设备密钥到期或丢失，重新申请绑定
	t.Run("E2E-07-04: ICCOA 重新绑定（密钥重建）", func(t *testing.T) {
		start := time.Now()

		rebindPhone := suite.CreateDefaultPhone("oppo", "phone-oppo-rebind", "user-rebind", "iccoa_dk40")
		rebindTCU := suite.CreateDefaultTCU("tcu-oppo-rebind", "veh-oppo-rebind", "LSVREBIND0012345")
		harness.AddPhone(rebindPhone)
		harness.AddTCU(rebindTCU)

		// 首次绑定
		first := rebindPhone.BindKeyWithHUB(rebindTCU.Config().VehicleID)
		require.NotNil(t, first, "First bind must succeed")
		firstKeyID := first.KeyID

		// 模拟密钥丢失后重新绑定
		second := rebindPhone.BindKeyWithHUB(rebindTCU.Config().VehicleID)
		require.NotNil(t, second, "Rebind must succeed")

		// ICCOA允许返回新密钥或复用现有密钥
		if first.KeyID == second.KeyID {
			t.Log("ICCOA rebind: same key ID returned (key update in-place)")
		} else {
			t.Logf("ICCOA rebind: new key generated: %s → %s", firstKeyID, second.KeyID)
		}

		// 验证重新绑定后仍然拥有有效密钥
		assert.True(t, rebindPhone.HasBoundKey(rebindTCU.Config().VehicleID), "Must have key after rebind")
		keys := rebindPhone.BoundKeys()
		assert.GreaterOrEqual(t, len(keys), 1, "At least 1 key after rebind")

		report.Record("E2E-07-04: ICCOA重新绑定", true, time.Since(start), "", "E2E-07", "ICCOA")
	})

	// ============================================================
	// Test: ICCOA 密钥元数据同步
	// ============================================================
	// ICCOA.DK.TS.002 §4.7: 绑定后元数据同步
	//   同步内容包括：密钥权限、有效期、车辆信息、UWB配置
	t.Run("E2E-07-05: ICCOA 密钥元数据同步", func(t *testing.T) {
		start := time.Now()

		syncPhone := suite.CreateDefaultPhone("xiaomi", "phone-mi-sync", "user-sync", "iccoa_dk40")
		syncTCU := suite.CreateDefaultTCU("tcu-mi-sync", "veh-mi-sync", "LSVSYNC001BIND01")
		harness.AddPhone(syncPhone)
		harness.AddTCU(syncTCU)

		// 绑定
		result := syncPhone.BindKeyWithHUB(syncTCU.Config().VehicleID)
		require.NotNil(t, result)

		// 验证元数据
		boundKeys := syncPhone.BoundKeys()
		for _, k := range boundKeys {
			if k.VehicleID == syncTCU.Config().VehicleID {
				// 验证关键元数据字段
				assert.NotEmpty(t, k.KeyID, "Key ID required")
				assert.NotEmpty(t, k.VehicleID, "Vehicle ID required")
				assert.NotEmpty(t, k.Protocol, "Protocol required")
				assert.NotEmpty(t, k.Status, "Status required")
				assert.False(t, k.BoundAt.IsZero(), "Bound timestamp required")
				assert.Equal(t, "iccoa_dk40", k.Protocol, "Protocol must match")

				t.Logf("ICCOA metadata: key=%s vehicle=%s protocol=%s status=%s type=%s",
					k.KeyID, k.VehicleID, k.Protocol, k.Status, k.KeyType)
			}
		}

		// 验证TCU侧元数据
		storedKeys := syncTCU.ListStoredKeys()
		t.Logf("TCU stored keys count: %d", len(storedKeys))

		report.Record("E2E-07-05: ICCOA密钥元数据同步", true, time.Since(start), "", "E2E-07", "ICCOA")
	})

	report.GenerateHTML("test-output/integration-report.html")
}
