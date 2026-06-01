// E2E-02: 密钥绑定流程（手机↔TCU↔DKCS）
//
// 场景描述:
//   GIVEN 手机已通过BLE发现车辆
//   WHEN 用户发起密钥绑定
//   THEN 手机发送绑定请求到HUB
//   THEN HUB路由到厂商适配器
//   THEN 适配器完成密钥签发
//   THEN 手机存储密钥信息
//
// 覆盖协议: CCC DK3, ICCOA DK4, ICCE

package scenarios

import (
	"testing"
	"time"

	"github.com/digitalkey/yuledkcs/integration-tests/helpers"
	"github.com/digitalkey/yuledkcs/integration-tests/suite"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// TestE2E02_KeyBinding tests key binding across all protocols.
func TestE2E02_KeyBinding(t *testing.T) {
	report := helpers.NewTestReport("E2E-02 密钥绑定流程")
	harness := suite.NewTestHarness("E2E-02")
	harness.Start()

	// ── Test: CCC Key Binding (Apple) ──
	t.Run("E2E-02-01: CCC DK3密钥绑定 (Apple)", func(t *testing.T) {
		start := time.Now()

		applePhone := suite.CreateDefaultPhone("apple", "phone-apple-bind", "user-apple-01", "ccc_dk3")
		appleTCU := suite.CreateDefaultTCU("tcu-apple-bind", "veh-apple-bind", "LSVAAPPLEBIND001")
		harness.AddPhone(applePhone)
		harness.AddTCU(appleTCU)

		// BLE Discovery
		appleTCU.StartBLEAdvertising()
		info, err := applePhone.ReadTCUBLEAdvert(appleTCU.Config().VehicleID, "apple", -50)
		require.NoError(t, err)
		require.Equal(t, appleTCU.Config().VehicleID, info["vehicle_id"])

		// Bind key (simulates HUB → Adapter flow)
		result := applePhone.BindKeyWithHUB(appleTCU.Config().VehicleID)
		require.NotNil(t, result)
		assert.NotEmpty(t, result.KeyID, "Should generate a key ID")
		assert.Equal(t, "ACTIVE", result.Status, "Key should be ACTIVE")
		assert.Empty(t, result.Error, "No error expected")
		assert.True(t, applePhone.HasBoundKey(appleTCU.Config().VehicleID), "Phone should record bound key")
		assert.GreaterOrEqual(t, len(applePhone.BoundKeys()), 1, "Phone should have at least 1 key")

		report.Record("E2E-02-01: CCC DK3密钥绑定 (Apple)", true, time.Since(start), "", "E2E-02", "CCC")
	})

	// ── Test: ICCOA Key Binding ──
	t.Run("E2E-02-02: ICCOA DK4.0密钥绑定 (Xiaomi)", func(t *testing.T) {
		start := time.Now()

		miPhone := suite.CreateDefaultPhone("xiaomi", "phone-mi-bind", "user-mi-01", "iccoa_dk40")
		miTCU := suite.CreateDefaultTCU("tcu-mi-bind", "veh-mi-bind", "LSVAMI001BIND01")
		harness.AddPhone(miPhone)
		harness.AddTCU(miTCU)

		miTCU.StartBLEAdvertising()
		_, err := miPhone.ReadTCUBLEAdvert(miTCU.Config().VehicleID, "xiaomi", -48)
		require.NoError(t, err)

		result := miPhone.BindKeyWithHUB(miTCU.Config().VehicleID)
		require.NotNil(t, result)
		assert.NotEmpty(t, result.KeyID)
		assert.Equal(t, "ACTIVE", result.Status)
		assert.True(t, miPhone.HasBoundKey(miTCU.Config().VehicleID))

		report.Record("E2E-02-02: ICCOA DK4.0密钥绑定 (Xiaomi)", true, time.Since(start), "", "E2E-02", "ICCOA")
	})

	// ── Test: ICCE Key Binding ──
	t.Run("E2E-02-03: ICCE密钥绑定 (Huawei)", func(t *testing.T) {
		start := time.Now()

		hPhone := suite.CreateDefaultPhone("huawei", "phone-hw-bind", "user-hw-01", "icce")
		hTCU := suite.CreateDefaultTCU("tcu-hw-bind", "veh-hw-bind", "LSVAHW001BIND01")
		harness.AddPhone(hPhone)
		harness.AddTCU(hTCU)

		hTCU.StartBLEAdvertising()
		_, err := hPhone.ReadTCUBLEAdvert(hTCU.Config().VehicleID, "huawei", -52)
		require.NoError(t, err)

		result := hPhone.BindKeyWithHUB(hTCU.Config().VehicleID)
		require.NotNil(t, result)
		assert.NotEmpty(t, result.KeyID)

		report.Record("E2E-02-03: ICCE密钥绑定 (Huawei)", true, time.Since(start), "", "E2E-02", "ICCE")
	})

	// ── Test: Concurrent binding ──
	t.Run("E2E-02-04: 多厂商并发密钥绑定", func(t *testing.T) {
		start := time.Now()

		type bindReq struct {
			phone *suite.MockPhoneClient
			tcu   *suite.MockTCUAgent
		}
		vendors := []struct {
			vendor   string
			protocol string
		}{
			{"samsung", "ccc_dk3"},
			{"oppo", "iccoa_dk40"},
			{"vivo", "iccoa_dk40"},
		}

		var reqs []bindReq
		for _, v := range vendors {
			p := suite.CreateDefaultPhone(v.vendor, "phone-"+v.vendor+"-bind", "user-"+v.vendor, v.protocol)
			tcu := suite.CreateDefaultTCU("tcu-"+v.vendor+"-bind", "veh-"+v.vendor+"-bind", "LSVA"+v.vendor+"01BIND")
			harness.AddPhone(p)
			harness.AddTCU(tcu)
			reqs = append(reqs, bindReq{phone: p, tcu: tcu})
		}

		type result struct {
			vendor string
			keyID  string
		}
		ch := make(chan result, len(reqs))
		for _, req := range reqs {
			go func(r bindReq) {
				res := r.phone.BindKeyWithHUB(r.tcu.Config().VehicleID)
				ch <- result{vendor: r.phone.Config().Vendor, keyID: res.KeyID}
			}(req)
		}

		success := 0
		for i := 0; i < len(reqs); i++ {
			r := <-ch
			if r.keyID != "" {
				success++
			}
		}

		report.Record("E2E-02-04: 多厂商并发密钥绑定", success == len(reqs),
			time.Since(start), "", "E2E-02", "CCC/ICCOA")
		if success != len(reqs) {
			t.Errorf("Only %d/%d concurrent binds succeeded", success, len(reqs))
		}
	})

	report.GenerateHTML("test-output/integration-report.html")
}
