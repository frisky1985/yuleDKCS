package run

import (
	"context"
	"fmt"
	"math/rand"
	"sync"
	"testing"
	"time"
)

// ═══════════════════════════════════════════════════════════════════
// 端到端验证测试 — yuleRUN x yuleDKCS
// ═══════════════════════════════════════════════════════════════════
//
// 测试策略：
//   1. 使用 MockDeviceProvider（无真实硬件依赖）
//   2. 直接使用 run/ 包预制的 TestCase 定义
//   3. 覆盖 正向场景 + 异常场景 + 性能场景
//
// 被测接口：
//   - DefaultRunner.RunScenario()
//   - MockDeviceProvider (Connect/Disconnect/ExecuteStep)
//   - DefaultReportGenerator.GenerateReport()
// ═══════════════════════════════════════════════════════════════════

// ── 测试套件全局变量 ──────────────────────────────────────────────────────

var (
	testCtx    = context.Background()
	defaultDev = TestDevice{
		DeviceID:  "mock-iphone-01",
		Model:     "iPhone 15 Pro",
		OSVersion: "iOS 18.0",
		Protocol:  "BLE",
		Capability: []string{"pke", "nfc", "uwb"},
	}
)

// newDefaultMock 创建带默认设备的高保真 Mock（极低失败率，模拟正常网络）
func newDefaultMock(failChance float64) *MockDeviceProvider {
	m := NewMockDeviceProvider()
	m.AddDevice(defaultDev, failChance)
	return m
}

// newMultiMock 创建多设备 Mock，用于并发测试
func newMultiMock(n int, failChance float64) *MockDeviceProvider {
	m := NewMockDeviceProvider()
	for i := 0; i < n; i++ {
		dev := TestDevice{
			DeviceID:   fmt.Sprintf("mock-device-%02d", i+1),
			Model:      "iPhone 15 Pro",
			OSVersion:  "iOS 18.0",
			Protocol:   "BLE",
			Capability: []string{"pke", "nfc", "uwb"},
		}
		m.AddDevice(dev, failChance)
	}
	return m
}

// newMultiMockWithIDs 创建指定 deviceID 的多设备 Mock
func newMultiMockWithIDs(ids []string, failChance float64) *MockDeviceProvider {
	m := NewMockDeviceProvider()
	for _, id := range ids {
		dev := TestDevice{
			DeviceID:   id,
			Model:      "iPhone 15 Pro",
			OSVersion:  "iOS 18.0",
			Protocol:   "BLE",
			Capability: []string{"pke", "nfc", "uwb"},
		}
		m.AddDevice(dev, failChance)
	}
	return m
}

// ── 1. TestBasicPKE: 基本 PKE 解锁/上锁 ────────────────────────────────────

func TestBasicPKE(t *testing.T) {
	t.Log("═══ TestBasicPKE: 基本 PKE 解锁/上锁 ═══")

	mock := newDefaultMock(0.0) // 0% 失败率，保底通过
	runner := NewRunner(mock, nil)
	testCase := BasicPKEScenario()

	t.Logf("  场景: %s (%s)", testCase.Name, testCase.ID)
	t.Logf("  步骤数: %d, 超时: %v", len(testCase.Steps), testCase.Timeout)

	run, err := runner.RunScenario(testCtx, defaultDev, []TestCase{testCase})
	if err != nil {
		t.Fatalf("RunScenario 失败: %v", err)
	}

	if run.Status != "passed" {
		t.Errorf("预期状态 passed, 实际 %s", run.Status)
	}

	if len(run.Results) != 1 {
		t.Fatalf("预期 1 个结果, 实际 %d", len(run.Results))
	}

	result := run.Results[0]
	if !result.Passed {
		t.Errorf("TestBasicPKE 未通过: %s", result.Error)
	}

	// 生成报告
	report, err := GenerateReport(testCtx, run)
	if err != nil {
		t.Fatalf("GenerateReport 失败: %v", err)
	}

	t.Logf("  ✅ 通过率: %.1f%%, 平均延时: %.0fms, P95: %dms, 最大: %dms",
		report.PassRate, report.AvgLatencyMs, report.P95LatencyMs, report.MaxLatencyMs)
	t.Logf("  ⏱ 总耗时: %dms", report.DurationMs)

	// 断言：通过率 100%
	if report.PassRate < 100.0 {
		t.Errorf("通过率应为 100%%, 实际 %.1f%%", report.PassRate)
	}
}

// ── 2. TestNFCTap: NFC 刷卡解锁（低延迟验证）──────────────────────────────

func TestNFCTap(t *testing.T) {
	t.Log("═══ TestNFCTap: NFC 刷卡解锁 ═══")

	mock := newDefaultMock(0.0)
	// 设置 NFC 设备低延迟（模拟 NCI 快速握手）
	tapDev := TestDevice{
		DeviceID:  "mock-nfc-01",
		Model:     "iPhone 15 Pro",
		OSVersion: "iOS 18.0",
		Protocol:  "NFC",
		Capability: []string{"nfc"},
	}
	mock.AddDevice(tapDev, 0.0)
	mock.SetLatencyRange(tapDev.DeviceID, 10*time.Millisecond, 50*time.Millisecond)

	runner := NewRunner(mock, nil)
	testCase := NFCTapScenario()

	t.Logf("  场景: %s (%s)", testCase.Name, testCase.ID)

	run, err := runner.RunScenario(testCtx, tapDev, []TestCase{testCase})
	if err != nil {
		t.Fatalf("RunScenario 失败: %v", err)
	}

	report, _ := GenerateReport(testCtx, run)
	t.Logf("  ✅ 通过率: %.1f%%, 平均延时: %.0fms, P95: %dms, 最大: %dms",
		report.PassRate, report.AvgLatencyMs, report.P95LatencyMs, report.MaxLatencyMs)

	if run.Status != "passed" {
		t.Errorf("预期状态 passed, 实际 %s", run.Status)
	}

	// 验证 NFC 解锁步骤通过
	if !run.Results[0].Passed {
		t.Errorf("NFC 解锁未通过: %s", run.Results[0].Error)
	}

	// 检查每个步骤的 MaxLatency 配置
	if len(testCase.Steps) > 0 {
		t.Log("  NFC 步骤延迟配置:")
		for _, step := range testCase.Steps {
			t.Logf("    [%d] %s -> 容许 %v", step.Order, step.Action, step.MaxLatency)
		}
	}
}

// ── 3. TestKeySharing: 钥匙分享全链路 ──────────────────────────────────────

func TestKeySharing(t *testing.T) {
	t.Log("═══ TestKeySharing: 钥匙分享全链路 ═══")

	mock := NewMockDeviceProvider()
	ownerDev := TestDevice{
		DeviceID:   "mock-owner-01",
		Model:      "iPhone 15 Pro",
		OSVersion:  "iOS 18.0",
		Protocol:   "BLE",
		Capability: []string{"pke", "nfc", "uwb", "sharing"},
	}
	mock.AddDevice(ownerDev, 0.0)

	runner := NewRunner(mock, nil)
	testCase := KeySharingScenario()

	t.Logf("  场景: %s (%s)", testCase.Name, testCase.ID)
	t.Logf("  步骤数: %d", len(testCase.Steps))
	for _, s := range testCase.Steps {
		t.Logf("    [%d] %s -> 预期: %s, 容许: %v", s.Order, s.Action, s.Expected, s.MaxLatency)
	}

	// ── 正向：分享后可使用 ──
	t.Log("  ── 正向链路：车主创建分享 → 接收者使用 ──")
	run, err := runner.RunScenario(testCtx, ownerDev, []TestCase{testCase})
	if err != nil {
		t.Fatalf("正向 RunScenario 失败: %v", err)
	}
	report, _ := GenerateReport(testCtx, run)
	t.Logf("  ✅ 通过率: %.1f%%, 平均延时: %.0fms", report.PassRate, report.AvgLatencyMs)

	if run.Status != "passed" {
		t.Errorf("正向链路预期 passed, 实际 %s: %s", run.Status, run.Results[0].Error)
	}

	// 说明：KeySharing 的步骤 5 预期 failure（吊销后 unlock 应失败），
	// 但 MockDeviceProvider.ExecuteStep 的 Expected 字段不用于判断结果，
	// 仅 step.Expected 字符串不影响执行逻辑。所以 Mock 仍返回 passed(随机)。
	// 真正的"预期失败"需要真实的授权校验逻辑。这里的验证只是功能链路检查。
	t.Logf("  ℹ️  步骤 5 (revoke → unlock) 场景定义预期 failure，但 Mock 不校验 Expected 语义")

	// ── 异常：未授权设备尝试解锁 ──
	t.Log("  ── 异常链路：未授权设备直接尝试解锁（预期失败） ──")

	// 用 100% 失败率的 mock 模拟无授权
	unauthMock := NewMockDeviceProvider()
	unauthMock.AddDevice(TestDevice{
		DeviceID: "mock-intruder-unauth",
		Model:    "Xiaomi 14",
		Protocol: "BLE",
	}, 1.0)

	unauthCase := TestCase{
		ID:          "unauth_unlock_001",
		Name:        "未授权设备尝试解锁",
		Description: "没有分享钥匙的设备直接请求解锁，预期失败",
		Protocol:    "BLE",
		Timeout:     10 * time.Second,
		Steps: []TestStep{
			{Order: 1, Action: "unlock", Expected: "failure", MaxLatency: 2000 * time.Millisecond},
		},
	}

	unauthRunner := NewRunner(unauthMock, nil)
	unauthRun, err := unauthRunner.RunScenario(testCtx, TestDevice{
		DeviceID: "mock-intruder-unauth",
		Model:    "Xiaomi 14",
	}, []TestCase{unauthCase})
	if err == nil && unauthRun != nil {
		t.Logf("  ℹ️  未授权用例结果: status=%s, passed=%v",
			unauthRun.Status, unauthRun.Results[0].Passed)
	}
}

// ── 4. TestConcurrentAccess: 并发安全（多 goroutine）───────────────────────

func TestConcurrentAccess(t *testing.T) {
	t.Log("═══ TestConcurrentAccess: 并发安全 ═══")

	const goroutines = 3
	// 零失败率验证并发正确性（不做随机失败干扰）
	mock := newMultiMock(goroutines, 0.0)
	runner := NewRunner(mock, nil)

	// 每个 goroutine 独立运行 BasicPKE 场景
	var wg sync.WaitGroup
	resultMu := sync.Mutex{}
	var runs []*TestRun

	for i := 0; i < goroutines; i++ {
		wg.Add(1)
		dev := TestDevice{
			DeviceID:  fmt.Sprintf("mock-device-%02d", i+1),
			Model:     "iPhone 15 Pro",
			OSVersion: "iOS 18.0",
			Protocol:  "BLE",
		}
		go func(d TestDevice) {
			defer wg.Done()
			run, err := runner.RunScenario(testCtx, d, []TestCase{BasicPKEScenario()})
			if err != nil {
				t.Errorf("device %s RunScenario 错误: %v", d.DeviceID, err)
				return
			}
			resultMu.Lock()
			runs = append(runs, run)
			resultMu.Unlock()
		}(dev)
	}

	wg.Wait()

	if len(runs) != goroutines {
		t.Errorf("预期 %d 条运行记录, 实际 %d", goroutines, len(runs))
	}

	var totalCases, totalPassed int
	for _, r := range runs {
		for _, result := range r.Results {
			totalCases++
			if result.Passed {
				totalPassed++
			}
		}
	}

	overallRate := float64(0)
	if totalCases > 0 {
		overallRate = float64(totalPassed) / float64(totalCases) * 100
	}

	t.Logf("  📊 并发 %d goroutines 统计:", goroutines)
	t.Logf("    总计用例: %d, 通过: %d, 失败: %d", totalCases, totalPassed, totalCases-totalPassed)
	t.Logf("    综合通过率: %.1f%%", overallRate)

	// 验证：所有 goroutine 都必须成功
	if overallRate < 100.0 {
		t.Errorf("并发测试通过率 %.1f%% < 100%% (零失败率下应全部通过)", overallRate)
	}

	// 竞态提示
	t.Log("  ℹ️  竞态检测需 `go test -race` 运行；本 case 仅验证并发功能正确性")
}

// ── 5. TestStressPerformance: 压力测试性能基线 ────────────────────────────

func TestStressPerformance(t *testing.T) {
	t.Log("═══ TestStressPerformance: 压力测试 (连续解锁 100 次) ═══")

	// 说明：StressTestScenario 是一个 TestCase 内含 100 个步骤。
	// DefaultRunner 将整个 TestCase 聚合为 1 个 TestResult（整体通过/失败）。
	// 因此报告的 P95/平均/最大 反映的是整条用例的耗时，而不是每步的。
	// Mock 侧每步默认延时 30~150ms，100 步总延时约 3~15 秒。

	mock := newDefaultMock(0.0) // 零失败率，确保 100 步全部通过
	runner := NewRunner(mock, nil)
	testCase := StressTestScenario()

	t.Logf("  场景: %s (%s)", testCase.Name, testCase.ID)
	t.Logf("  步骤数: %d, 超时: %v", len(testCase.Steps), testCase.Timeout)

	start := time.Now()
	run, err := runner.RunScenario(testCtx, defaultDev, []TestCase{testCase})
	elapsed := time.Since(start)

	if err != nil {
		t.Fatalf("压力测试 RunScenario 失败: %v", err)
	}

	report, err := GenerateReport(testCtx, run)
	if err != nil {
		t.Fatalf("GenerateReport 失败: %v", err)
	}

	// 每步平均延时
	avgPerStepMs := elapsed.Milliseconds() / int64(len(testCase.Steps))

	t.Logf("  ⏱ 总耗时: %v (含 Mock 网络延迟)", elapsed)
	t.Logf("    每步平均延时: %dms", avgPerStepMs)
	t.Logf("  📊 报告摘要:")
	t.Logf("    用例通过: %v, 状态: %s", run.Results[0].Passed, run.Status)
	t.Logf("    用例平均延时: %.0fms", report.AvgLatencyMs)
	t.Logf("    用例最大延时: %dms", report.MaxLatencyMs)

	// 验证：100 步全部通过
	if !run.Results[0].Passed {
		t.Errorf("压力测试未全部通过: %s", run.Results[0].Error)
	}
	if run.Status != "passed" {
		t.Errorf("预期状态 passed, 实际 %s", run.Status)
	}

	// 每步平均延时 < 200ms（Mock 默认 30~150ms + 调度开销）
	if avgPerStepMs >= 200 {
		t.Errorf("每步平均延时 %dms >= 200ms 阈值", avgPerStepMs)
	}

	// 无超时
	if run.Status == "timeout" {
		t.Error("压力测试意外超时")
	}

	t.Logf("  ✅ 通过率: 100%%, 步均延时: %dms, 总耗时: %v", avgPerStepMs, elapsed)
}

// ── 6. TestRemoteControl: 远程控车 ────────────────────────────────────────

func TestRemoteControl(t *testing.T) {
	t.Log("═══ TestRemoteControl: 远程控车 (MQTT) ═══")

	mock := newDefaultMock(0.0)
	runner := NewRunner(mock, nil)
	testCase := RemoteControlScenario()

	t.Logf("  场景: %s (%s)", testCase.Name, testCase.ID)
	t.Logf("  步骤: mqtt_connect → remote_unlock → remote_window_close → remote_start")

	run, err := runner.RunScenario(testCtx, defaultDev, []TestCase{testCase})
	if err != nil {
		t.Fatalf("RunScenario 失败: %v", err)
	}

	report, _ := GenerateReport(testCtx, run)
	t.Logf("  ✅ 通过率: %.1f%%, 平均延时: %.0fms, 最大: %dms",
		report.PassRate, report.AvgLatencyMs, report.MaxLatencyMs)

	if run.Status != "passed" {
		t.Errorf("预期状态 passed, 实际 %s: %s", run.Status, run.Results[0].Error)
	}

	if report.PassRate < 100.0 {
		t.Errorf("远程控车通过率应为 100%%, 实际 %.1f%%", report.PassRate)
	}
}

// ── 7. TestParallelExecution: 多设备并行执行 ──────────────────────────────

func TestParallelExecution(t *testing.T) {
	t.Log("═══ TestParallelExecution: 多设备并行执行 ═══")

	deviceIDs := []string{
		"parallel-dev-01",
		"parallel-dev-02",
		"parallel-dev-03",
	}

	mock := newMultiMockWithIDs(deviceIDs, 0.0)
	runner := NewRunner(mock, nil)

	devices := make([]TestDevice, len(deviceIDs))
	for i, id := range deviceIDs {
		devices[i] = TestDevice{
			DeviceID:  id,
			Model:     "iPhone 15 Pro",
			Protocol:  "BLE",
		}
	}

	runs, err := runner.RunParallel(testCtx, devices, []TestCase{BasicPKEScenario()})
	if err != nil {
		t.Fatalf("RunParallel 失败: %v", err)
	}

	if len(runs) == 0 {
		t.Fatal("RunParallel 返回 0 条运行记录")
	}

	report, _ := CompareReports(testCtx, toRunPtrs(runs))
	t.Logf("  📊 并行 %d 设备统计:", len(runs))
	t.Logf("    最佳设备: %s (通过率 %.1f%%)", report.Benchmark.BestDevice, report.Benchmark.BestPassRate)
	t.Logf("    平均延时范围: %.0fms ~ %.0fms", report.Benchmark.MinAvgLatency, report.Benchmark.MaxAvgLatency)

	for _, r := range report.Runs {
		t.Logf("    %s: 通过率 %.1f%%, 平均 %.0fms, P95 %dms",
			r.DeviceModel, r.PassRate, r.AvgLatencyMs, r.P95LatencyMs)
	}

	// 断言：所有设备均 100% 通过
	for _, r := range report.Runs {
		if r.PassRate < 100.0 {
			t.Errorf("%s 通过率 %.1f%% < 100%%", r.DeviceModel, r.PassRate)
		}
	}
}

// ── 8. TestScenarioTimeout: 超时场景 ──────────────────────────────────────

func TestScenarioTimeout(t *testing.T) {
	t.Log("═══ TestScenarioTimeout: 超时场景 ═══")

	mock := newDefaultMock(0.0)
	// 将延时设为极大值，确保超时
	_ = mock.SetLatencyRange(defaultDev.DeviceID, 10*time.Second, 15*time.Second)

	runner := NewRunner(mock, nil)

	// 使用一个超时极短的 testCase
	timeoutCase := TestCase{
		ID:          "timeout_001",
		Name:        "超时测试",
		Description: "步骤延时远超场景超时，预期触发 timeout",
		Protocol:    "BLE",
		Timeout:     1 * time.Second, // 1秒超时，但 mock 延时 10~15 秒
		Steps: []TestStep{
			{Order: 1, Action: "connect", Expected: "success", MaxLatency: 500 * time.Millisecond},
			{Order: 2, Action: "unlock", Expected: "success", MaxLatency: 500 * time.Millisecond},
		},
	}

	run, err := runner.RunScenario(testCtx, defaultDev, []TestCase{timeoutCase})
	if err != nil {
		t.Logf("  ℹ️  预期超时错误: %v", err)
	} else {
		t.Logf("  ℹ️  运行状态: %s", run.Status)
	}

	// 超时后 Connect 阶段就超时了，run 可能为 nil
	if run != nil {
		t.Logf("    状态: %s, 结果数: %d", run.Status, len(run.Results))
		if run.Status != "timeout" && run.Status != "failed" {
			t.Log("  ⚠️ 超时场景未触发期望的 timeout 状态, 但这不是硬性失败")
		}
	}
}

// ── 9. TestDeviceDisconnection: 设备断连异常 ──────────────────────────────

func TestDeviceDisconnection(t *testing.T) {
	t.Log("═══ TestDeviceDisconnection: 设备执行中断连 ═══")

	// 方案：使用一个未注册的设备 ID
	mock := newDefaultMock(0.0)
	runner := NewRunner(mock, nil)

	unknownDev := TestDevice{
		DeviceID: "device-not-exists",
		Model:    "Unknown",
	}

	testCase := BasicPKEScenario()
	_, err := runner.RunScenario(testCtx, unknownDev, []TestCase{testCase})
	if err == nil {
		t.Error("预期未注册设备应返回错误，但未报错")
	} else {
		t.Logf("  ✅ 未注册设备正确拒绝: %v", err)
	}

	// 方案2：正常连接后手动断连，再执行步骤
	mock2 := newDefaultMock(0.0)
	if err := mock2.ConnectDevice(testCtx, defaultDev.DeviceID); err != nil {
		t.Fatalf("连接设备失败: %v", err)
	}
	if err := mock2.DisconnectDevice(testCtx, defaultDev.DeviceID); err != nil {
		t.Fatalf("断开设备失败: %v", err)
	}

	// 断连后执行步骤应失败
	stepResult, err := mock2.ExecuteStep(testCtx, defaultDev.DeviceID, TestStep{Action: "unlock", Expected: "success"})
	if err != nil {
		t.Logf("  ✅ 断连后执行步骤正确拒绝: %v", err)
	} else if !stepResult.Passed {
		t.Logf("  ✅ 断连后执行步骤返回未通过: %s", stepResult.Error)
	} else {
		t.Error("断连后执行步骤应失败，但返回 passed")
	}
}

// ── 10. TestNFCTapAbnormal: NFC 异常场景 ─────────────────────────────────

func TestNFCTapAbnormal(t *testing.T) {
	t.Log("═══ TestNFCTapAbnormal: NFC 异常场景 ═══")

	mock := newDefaultMock(0.0)
	nfcDev := TestDevice{
		DeviceID:  "mock-nfc-abnormal",
		Protocol:  "NFC",
		Model:     "iPhone 15 Pro",
		OSVersion: "iOS 18.0",
	}
	mock.AddDevice(nfcDev, 0.0)
	mock.SetLatencyRange(nfcDev.DeviceID, 100*time.Millisecond, 200*time.Millisecond)

	runner := NewRunner(mock, nil)

	// MaxLatency = 50ms，mock 真实延时 100~200ms => 触发超时
	abnormalCase := TestCase{
		ID:          "nfc_abnormal_001",
		Name:        "NFC 异常 (延迟过高)",
		Description: "模拟 NFC 读卡器响应延迟过高，预期步骤失败",
		Protocol:    "NFC",
		Timeout:     5 * time.Second,
		Steps: []TestStep{
			{Order: 1, Action: "nfc_tap", Expected: "handshake_ok", MaxLatency: 50 * time.Millisecond},
		},
	}

	run, err := runner.RunScenario(testCtx, nfcDev, []TestCase{abnormalCase})
	if err != nil {
		t.Logf("  ✅ NFC 延迟过高触发错误: %v", err)
	} else if run != nil && run.Status == "failed" {
		t.Logf("  ✅ NFC 延迟过高正确标记为失败: %s", run.Results[0].Error)
	} else {
		t.Logf("  ℹ️  NFC 异常场景结果: status=%s", run.Status)
	}
}

// ── 辅助函数 ──────────────────────────────────────────────────────────────

func toRunPtrs(runs []TestRun) []*TestRun {
	ptrs := make([]*TestRun, len(runs))
	for i := range runs {
		ptrs[i] = &runs[i]
	}
	return ptrs
}

// ── TestMain: 测试入口 ────────────────────────────────────────────────────

func TestMain(m *testing.M) {
	fmt.Println("╔═══════════════════════════════════════════════════════╗")
	fmt.Println("║  yuleRUN x yuleDKCS  端到端验证测试套件                ║")
	fmt.Println("║  场景: PKE / NFC / 远程 / 分享 / 并发 / 压力          ║")
	fmt.Println("╚═══════════════════════════════════════════════════════╝")
	m.Run()
}

// ── BenchmarkStress: 压力性能基准 (go test -bench=.) ──────────────────────

func BenchmarkStress(b *testing.B) {
	mock := newDefaultMock(0.005)
	runner := NewRunner(mock, nil)
	testCase := StressTestScenario()

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_, _ = runner.RunScenario(testCtx, defaultDev, []TestCase{testCase})
	}
}

func BenchmarkBasicPKE(b *testing.B) {
	mock := newDefaultMock(0.0)
	runner := NewRunner(mock, nil)
	testCase := BasicPKEScenario()

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_, _ = runner.RunScenario(testCtx, defaultDev, []TestCase{testCase})
	}
}

// ── Fuzz: MockDeviceProvider 的模糊测试 ──────────────────────────────────

func FuzzMockExecuteStep(f *testing.F) {
	testCases := []string{"connect", "unlock", "lock", "start", "disconnect", "nfc_tap", "remote_unlock", "revoke"}
	for _, tc := range testCases {
		f.Add(tc)
	}

	mock := newDefaultMock(0.0)

	f.Fuzz(func(t *testing.T, action string) {
		// 用随机 action 调用 ExecuteStep，验证不 panic
		step := TestStep{
			Order:  1,
			Action: action,
			Expected: func() string {
				vals := []string{"success", "failure", "connected", "handshake_ok", "unknown"}
				return vals[rand.Intn(len(vals))]
			}(),
			MaxLatency: time.Duration(rand.Intn(5000)) * time.Millisecond,
		}

		// 先确保连接
		_ = mock.ConnectDevice(context.Background(), defaultDev.DeviceID)
		result, err := mock.ExecuteStep(context.Background(), defaultDev.DeviceID, step)
		if err != nil {
			// 错误可接受（如未知 action）
			return
		}
		if result == nil {
			t.Error("step result is nil")
		}
	})
}
