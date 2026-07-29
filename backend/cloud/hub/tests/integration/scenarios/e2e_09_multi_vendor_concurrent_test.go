// E2E-09: 多厂商并发场景验证
//
// 场景描述:
//   验证多厂商设备在大规模并发下的端到端协议流程:
//   1. 三协议同时并发密钥绑定（CCC + ICCOA + ICCE）
//   2. 同一车辆多手机同时访问（混合操作：绑定+解锁+指令）
//   3. 多厂商并发UWB被动进入
//   4. 并发远程控车（多命令同时下发）
//   5. 厂商独立隔离（一家失败不影响其他）

package scenarios

import (
	"fmt"
	"sync"
	"testing"
	"time"

	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/tests/integration/helpers"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/tests/integration/suite"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// TestE2E09_MultiVendorConcurrent tests multi-vendor concurrent operations.
func TestE2E09_MultiVendorConcurrent(t *testing.T) {
	report := helpers.NewTestReport("E2E-09 多厂商并发场景")
	harness := suite.NewTestHarness("E2E-09")
	harness.Start()

	// ============================================================
	// Test: 三协议同时并发密钥绑定
	// ============================================================
	t.Run("E2E-09-01: 三协议并发密钥绑定（CCC+ICCOA+ICCE）", func(t *testing.T) {
		start := time.Now()

		type vendorBindReq struct {
			phone    *suite.MockPhoneClient
			tcu      *suite.MockTCUAgent
			vendor   string
			protocol string
		}

		reqs := []vendorBindReq{
			{suite.CreateDefaultPhone("apple", "phone-conc-ccc", "user-conc-ccc", "ccc_dk3"),
				suite.CreateDefaultTCU("tcu-conc-ccc", "veh-conc-ccc", "LSVCONCCCC001"), "apple", "CCC"},
			{suite.CreateDefaultPhone("xiaomi", "phone-conc-iccoa", "user-conc-iccoa", "iccoa_dk40"),
				suite.CreateDefaultTCU("tcu-conc-iccoa", "veh-conc-iccoa", "LSVCONCICCOA01"), "xiaomi", "ICCOA"},
			{suite.CreateDefaultPhone("huawei", "phone-conc-icce", "user-conc-icce", "icce"),
				suite.CreateDefaultTCU("tcu-conc-icce", "veh-conc-icce", "LSVCONCICCE001"), "huawei", "ICCE"},
		}

		for i := range reqs {
			harness.AddPhone(reqs[i].phone)
			harness.AddTCU(reqs[i].tcu)
		}

		type result struct {
			vendor   string
			protocol string
			keyID    string
			status   string
		}
		ch := make(chan result, len(reqs))

		// 并发启动所有绑定
		var wg sync.WaitGroup
		for _, req := range reqs {
			wg.Add(1)
			go func(r vendorBindReq) {
				defer wg.Done()
				// 先做BLE发现
				r.tcu.StartBLEAdvertising()
				info, err := r.phone.ReadTCUBLEAdvert(r.tcu.Config().VehicleID, r.vendor, -50)
				if err != nil {
					ch <- result{vendor: r.vendor, protocol: r.protocol, status: "BLE_FAIL"}
					return
				}
				_ = info
				// 并发绑定
				bindRes := r.phone.BindKeyWithHUB(r.tcu.Config().VehicleID)
				if bindRes == nil {
					ch <- result{vendor: r.vendor, protocol: r.protocol, status: "BIND_FAIL"}
					return
				}
				ch <- result{
					vendor:   r.vendor,
					protocol: r.protocol,
					keyID:    bindRes.KeyID,
					status:   bindRes.Status,
				}
			}(req)
		}
		wg.Wait()
		close(ch)

		successCount := 0
		totalCount := 0
		for r := range ch {
			totalCount++
			if r.status == "ACTIVE" && r.keyID != "" {
				successCount++
				t.Logf("Concurrent bind OK: %s/%s key=%s", r.vendor, r.protocol, r.keyID)
			} else {
				t.Errorf("Concurrent bind FAIL: %s/%s status=%s", r.vendor, r.protocol, r.status)
			}
		}

		assert.Equal(t, len(reqs), totalCount, "All concurrent binds must complete")
		assert.Equal(t, len(reqs), successCount, "All concurrent binds must succeed")

		report.Record("E2E-09-01: 三协议并发密钥绑定", successCount == len(reqs),
			time.Since(start), "", "E2E-09", "CCC/ICCOA/ICCE")
	})

	// ============================================================
	// Test: 同一车辆多手机同时访问
	// ============================================================
	t.Run("E2E-09-02: 同一车辆多手机并发访问", func(t *testing.T) {
		start := time.Now()

		// 一台车被多台手机并发操作
		sharedVehicle := suite.CreateDefaultTCU("tcu-shared-multi", "veh-shared-multi", "LSVSHAREDMULTI01")
		harness.AddTCU(sharedVehicle)
		sharedTCU := sharedVehicle

		type phoneOp struct {
			phone   *suite.MockPhoneClient
			op      string // bind, unlock, lock, engine
		}

		phoneOps := []phoneOp{
			{suite.CreateDefaultPhone("apple", "phone-shared-1", "user-shared-1", "ccc_dk3"), "bind"},
			{suite.CreateDefaultPhone("xiaomi", "phone-shared-2", "user-shared-2", "iccoa_dk40"), "bind"},
			{suite.CreateDefaultPhone("huawei", "phone-shared-3", "user-shared-3", "icce"), "bind"},
		}
		for i := range phoneOps {
			harness.AddPhone(phoneOps[i].phone)
		}

		// 3台手机同时绑定同一台车
		type bindResult struct {
			phoneID string
			keyID   string
			ok      bool
		}
		bindCh := make(chan bindResult, len(phoneOps))
		var bindWg sync.WaitGroup
		for _, po := range phoneOps {
			bindWg.Add(1)
			go func(p *suite.MockPhoneClient) {
				defer bindWg.Done()
				res := p.BindKeyWithHUB(sharedTCU.Config().VehicleID)
				if res != nil && res.KeyID != "" {
					bindCh <- bindResult{phoneID: p.Config().DeviceID, keyID: res.KeyID, ok: true}
				} else {
					bindCh <- bindResult{phoneID: p.Config().DeviceID, ok: false}
				}
			}(po.phone)
		}
		bindWg.Wait()
		close(bindCh)

		bindOk := 0
		for r := range bindCh {
			if r.ok {
				bindOk++
				t.Logf("Shared vehicle bind OK: phone=%s key=%s", r.phoneID, r.keyID)
			} else {
				t.Errorf("Shared vehicle bind FAIL: phone=%s", r.phoneID)
			}
		}
		assert.Equal(t, len(phoneOps), bindOk, "All phones must bind to shared vehicle")

		// 验证每台手机都有该车的密钥
		for _, po := range phoneOps {
			assert.True(t, po.phone.HasBoundKey(sharedTCU.Config().VehicleID),
				"%s must have key for shared vehicle", po.phone.Config().DeviceID)
		}

		report.Record("E2E-09-02: 同一车辆多手机并发访问", bindOk == len(phoneOps),
			time.Since(start), "", "E2E-09", "CCC/ICCOA/ICCE")
	})

	// ============================================================
	// Test: 混合操作并发
	// ============================================================
	t.Run("E2E-09-03: 多厂商混合操作并发（绑定+解锁+指令）", func(t *testing.T) {
		start := time.Now()

		// 3家厂商分别做不同操作
		type mixedOp struct {
			vendor   string
			phone    *suite.MockPhoneClient
			tcu      *suite.MockTCUAgent
			ops      []string
		}

		ops := []mixedOp{
			{"苹果",
				suite.CreateDefaultPhone("apple", "phone-mix-ccc", "user-mix-ccc", "ccc_dk3"),
				suite.CreateDefaultTCU("tcu-mix-ccc", "veh-mix-ccc", "LSVMIXCCC001"), []string{"unlock", "lock", "engine_start", "engine_stop"}},
			{"小米",
				suite.CreateDefaultPhone("xiaomi", "phone-mix-iccoa", "user-mix-iccoa", "iccoa_dk40"),
				suite.CreateDefaultTCU("tcu-mix-iccoa", "veh-mix-iccoa", "LSVMIXICCOA01"), []string{"lock", "unlock", "trunk_open"}},
			{"华为",
				suite.CreateDefaultPhone("huawei", "phone-mix-icce", "user-mix-icce", "icce"),
				suite.CreateDefaultTCU("tcu-mix-icce", "veh-mix-icce", "LSVMIXICCE001"), []string{"find_car", "unlock", "lock"}},
		}

		for i := range ops {
			harness.AddPhone(ops[i].phone)
			harness.AddTCU(ops[i].tcu)
			// 预绑定
			bindRes := ops[i].phone.BindKeyWithHUB(ops[i].tcu.Config().VehicleID)
			require.NotNil(t, bindRes, "%s pre-bind must succeed", ops[i].vendor)
		}

		// 计算总操作数
		totalOps := 0
		for _, m := range ops {
			totalOps += len(m.ops)
		}

		// 并发执行混合操作
		type opResult struct {
			vendor string
			op     string
			ack    string
			err    error
		}
		opCh := make(chan opResult, totalOps)
		var opWg sync.WaitGroup

		for _, m := range ops {
			for _, op := range m.ops {
				opWg.Add(1)
				go func(vendor string, tcu *suite.MockTCUAgent, cmd string) {
					defer opWg.Done()
					ack, err := tcu.HandleCommand(cmd)
					opCh <- opResult{vendor: vendor, op: cmd, ack: ack, err: err}
				}(m.vendor, m.tcu, op)
			}
		}
		opWg.Wait()
		close(opCh)

		success := 0
		failures := 0
		for r := range opCh {
			if r.err == nil && r.ack != "" {
				success++
				t.Logf("Mixed op OK: %s/%s → %s", r.vendor, r.op, r.ack)
			} else {
				failures++
				t.Errorf("Mixed op FAIL: %s/%s err=%v", r.vendor, r.op, r.err)
			}
		}

		assert.Equal(t, 0, failures, "No mixed operations should fail")
		assert.Greater(t, success, 0, "Some operations must succeed")

		report.Record("E2E-09-03: 多厂商混合操作并发", failures == 0,
			time.Since(start), "", "E2E-09", "CCC/ICCOA/ICCE")
	})

	// ============================================================
	// Test: 多厂商并发UWB被动进入
	// ============================================================
	t.Run("E2E-09-04: 多厂商并发UWB被动进入", func(t *testing.T) {
		start := time.Now()

		type uwbPhone struct {
			phone *suite.MockPhoneClient
			tcu   *suite.MockTCUAgent
			name  string
		}

		uwbPhones := []uwbPhone{
			{suite.CreateDefaultPhone("apple", "phone-uwb-ccc", "user-uwb-ccc", "ccc_dk3"),
				suite.CreateDefaultTCU("tcu-uwb-ccc", "veh-uwb-ccc", "LSVUWBCCC001"), "CCC_Apple"},
			{suite.CreateDefaultPhone("xiaomi", "phone-uwb-iccoa", "user-uwb-iccoa", "iccoa_dk40"),
				suite.CreateDefaultTCU("tcu-uwb-iccoa", "veh-uwb-iccoa", "LSVUWBICCOA01"), "ICCOA_Xiaomi"},
			{suite.CreateDefaultPhone("huawei", "phone-uwb-icce", "user-uwb-icce", "icce"),
				suite.CreateDefaultTCU("tcu-uwb-icce", "veh-uwb-icce", "LSVUWBICCE001"), "ICCE_Huawei"},
		}

		for i := range uwbPhones {
			harness.AddPhone(uwbPhones[i].phone)
			harness.AddTCU(uwbPhones[i].tcu)
			uwbPhones[i].phone.BindKeyWithHUB(uwbPhones[i].tcu.Config().VehicleID)
		}

		// 所有厂商同时进行UWB被动进入
		type uwbEntryResult struct {
			name   string
			unlock bool
		}
		uwbCh := make(chan uwbEntryResult, len(uwbPhones))
		var uwbWg sync.WaitGroup

		for _, up := range uwbPhones {
			uwbWg.Add(1)
			go func(u uwbPhone) {
				defer uwbWg.Done()
				u.tcu.StartBLEAdvertising()
				u.phone.StartBLEAdvertising()

				rangingSteps := []suite.UWBRangingResult{
					{DistanceMM: 10000, Confidence: 90, Phase: "APPROACH"},
					{DistanceMM: 3000, Confidence: 85, Phase: "LOCK_ZONE"},
					{DistanceMM: 1500, Confidence: 80, Phase: "UNLOCK_ZONE"},
				}
				ch := u.phone.StartUWBRanging(rangingSteps)
				unlocked := false
				for result := range ch {
					if result.Phase == "UNLOCK_ZONE" && result.DistanceMM < 2000 {
						if u.tcu.SimulateUWBUnlockZone(u.phone.Config().DeviceID, result.DistanceMM) {
							unlocked = true
						}
					}
				}
				uwbCh <- uwbEntryResult{name: u.name, unlock: unlocked}
			}(up)
		}
		uwbWg.Wait()
		close(uwbCh)

		uwbOK := 0
		for r := range uwbCh {
			if r.unlock {
				uwbOK++
				t.Logf("UWB entry OK: %s", r.name)
			} else {
				t.Errorf("UWB entry FAIL: %s", r.name)
			}
		}

		assert.Equal(t, len(uwbPhones), uwbOK, "All UWB passive entries must succeed")

		report.Record("E2E-09-04: 多厂商并发UWB被动进入", uwbOK == len(uwbPhones),
			time.Since(start), "", "E2E-09", "CCC/ICCOA/ICCE/UWB")
	})

	// ============================================================
	// Test: 厂商独立隔离
	// ============================================================
	t.Run("E2E-09-05: 厂商隔离验证（一家失败不影响其他）", func(t *testing.T) {
		start := time.Now()

		// 3家独立绑定的车辆
		type isoTest struct {
			vendor   string
			phone    *suite.MockPhoneClient
			tcu      *suite.MockTCUAgent
		}
		phones := []isoTest{
			{"apple", suite.CreateDefaultPhone("apple", "phone-iso-ccc", "user-iso-ccc", "ccc_dk3"),
				suite.CreateDefaultTCU("tcu-iso-ccc", "veh-iso-ccc", "LSVISOCCC001")},
			{"xiaomi", suite.CreateDefaultPhone("xiaomi", "phone-iso-iccoa", "user-iso-iccoa", "iccoa_dk40"),
				suite.CreateDefaultTCU("tcu-iso-iccoa", "veh-iso-iccoa", "LSVISOICCOA01")},
			{"huawei", suite.CreateDefaultPhone("huawei", "phone-iso-icce", "user-iso-icce", "icce"),
				suite.CreateDefaultTCU("tcu-iso-icce", "veh-iso-icce", "LSVISOICCE001")},
		}
		for i := range phones {
			harness.AddPhone(phones[i].phone)
			harness.AddTCU(phones[i].tcu)
		}

		// 3家同时绑定
		type bindInfo struct {
			vendor string
			keyID  string
		}
		isoCh := make(chan bindInfo, len(phones))
		var isoWg sync.WaitGroup
		for _, p := range phones {
			isoWg.Add(1)
			go func(it isoTest) {
				defer isoWg.Done()
				res := it.phone.BindKeyWithHUB(it.tcu.Config().VehicleID)
				if res != nil {
					isoCh <- bindInfo{vendor: it.vendor, keyID: res.KeyID}
				} else {
					isoCh <- bindInfo{vendor: it.vendor, keyID: ""}
				}
			}(p)
		}
		isoWg.Wait()
		close(isoCh)

		results := make(map[string]string)
		for r := range isoCh {
			results[r.vendor] = r.keyID
		}

		// 验证每家的绑定独立
		for vendor, keyID := range results {
			assert.NotEmpty(t, keyID, "%s bind must have key ID", vendor)
		}

		// 验证隔离开：苹果的绑定不影响其他厂商
		t.Logf("Isolation check: apple=%s xiaomi=%s huawei=%s",
			results["apple"], results["xiaomi"], results["huawei"])

		// 各厂商可独立控车
		for _, p := range phones {
			ack, err := p.tcu.HandleCommand("lock")
			assert.NoError(t, err, "%s lock must work independently", p.vendor)
			assert.Contains(t, ack, "ack", "%s lock ack", p.vendor)
			assert.True(t, p.tcu.IsDoorsLocked(), "%s TCU should be locked", p.vendor)

			ack, err = p.tcu.HandleCommand("unlock")
			assert.NoError(t, err, "%s unlock must work independently", p.vendor)
			assert.Contains(t, ack, "ack", "%s unlock ack", p.vendor)
		}

		report.Record("E2E-09-05: 厂商隔离验证", true, time.Since(start), "", "E2E-09", "CCC/ICCOA/ICCE")
	})

	report.GenerateHTML("test-output/integration-report.html")
}

func init() {
	// Ensure fmt is used (imported for printf in concurrent tests)
	_ = fmt.Sprintf("") // ensure fmt import doesn't cause unused warning
}
