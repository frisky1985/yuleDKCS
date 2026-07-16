package run

import (
	"context"
	"crypto/rand"
	"fmt"
	"sync"
	"time"
)

// ScenarioRunner 场景运行器
// 负责编排测试用例在设备上的执行逻辑，支持串行和并行模式。
type ScenarioRunner interface {
	// RunScenario 串行执行一个场景
	RunScenario(ctx context.Context, device TestDevice, cases []TestCase) (*TestRun, error)

	// RunParallel 在多个设备上并行执行场景
	RunParallel(ctx context.Context, devices []TestDevice, cases []TestCase) ([]TestRun, error)

	// StopRun 停止正在执行的压测
	StopRun(ctx context.Context, runID string) error
}

// ── Runner 实现 ──────────────────────────────────────────────────────────

// DefaultRunner 默认压测运行器
type DefaultRunner struct {
	provider DeviceProvider
	store    ResultStore
	mu       sync.RWMutex
	active   map[string]context.CancelFunc
	idSeq    uint64
}

// NewRunner 创建新的压测运行器
func NewRunner(provider DeviceProvider, store ResultStore) *DefaultRunner {
	return &DefaultRunner{
		provider: provider,
		store:    store,
		active:   make(map[string]context.CancelFunc),
	}
}

func (r *DefaultRunner) RunScenario(ctx context.Context, device TestDevice, cases []TestCase) (*TestRun, error) {
	run := &TestRun{
		RunID:     newRunID(),
		Device:    device,
		Cases:     cases,
		Status:    "pending",
		StartedAt: time.Now(),
		Results:   make([]TestResult, 0, len(cases)),
	}

	// 注册可取消上下文
	runCtx, cancel := context.WithCancel(ctx)
	r.mu.Lock()
	r.active[run.RunID] = cancel
	r.mu.Unlock()
	defer func() {
		r.mu.Lock()
		delete(r.active, run.RunID)
		r.mu.Unlock()
		cancel()
	}()

	// 连接设备
	if err := r.provider.ConnectDevice(runCtx, device.DeviceID); err != nil {
		run.Status = "failed"
		now := time.Now()
		run.CompletedAt = &now
		_ = r.saveRun(ctx, run)
		return nil, fmt.Errorf("connect device %s: %w", device.DeviceID, err)
	}
	defer func() { _ = r.provider.DisconnectDevice(context.Background(), device.DeviceID) }()

	run.Status = "running"

	for _, tc := range cases {
		select {
		case <-runCtx.Done():
			run.Status = "timeout"
			now := time.Now()
			run.CompletedAt = &now
			_ = r.saveRun(ctx, run)
			return run, fmt.Errorf("run %s cancelled/timeout", run.RunID)
		default:
		}

		result := r.executeCase(runCtx, tc, device.DeviceID)
		run.Results = append(run.Results, *result)

		if !result.Passed {
			run.Status = "failed"
		}
	}

	if run.Status == "running" {
		run.Status = "passed"
	}
	now := time.Now()
	run.CompletedAt = &now

	// 持久化
	if err := r.saveRun(ctx, run); err != nil {
		return run, fmt.Errorf("save run %s: %w", run.RunID, err)
	}

	return run, nil
}

func (r *DefaultRunner) executeCase(ctx context.Context, tc TestCase, deviceID string) *TestResult {
	caseCtx, cancel := context.WithTimeout(ctx, tc.Timeout)
	defer cancel()

	startedAt := time.Now()
	result := &TestResult{
		CaseID:    tc.ID,
		StartedAt: startedAt,
		Passed:    true,
	}

	for _, step := range tc.Steps {
		select {
		case <-caseCtx.Done():
			result.Passed = false
			result.Error = fmt.Sprintf("case %s timeout after step %d", tc.ID, step.Order)
			result.CompletedAt = time.Now()
			result.LatencyMs = result.CompletedAt.Sub(startedAt).Milliseconds()
			return result
		default:
		}

		stepResult, err := r.provider.ExecuteStep(caseCtx, deviceID, step)
		if err != nil {
			result.Passed = false
			result.Error = err.Error()
			result.CompletedAt = time.Now()
			result.LatencyMs = result.CompletedAt.Sub(startedAt).Milliseconds()
			return result
		}

		if !stepResult.Passed {
			result.Passed = false
			result.Error = stepResult.Error
			result.CompletedAt = time.Now()
			result.LatencyMs = result.CompletedAt.Sub(startedAt).Milliseconds()
			return result
		}
	}

	result.CompletedAt = time.Now()
	result.LatencyMs = result.CompletedAt.Sub(startedAt).Milliseconds()
	return result
}

func (r *DefaultRunner) RunParallel(ctx context.Context, devices []TestDevice, cases []TestCase) ([]TestRun, error) {
	if len(devices) == 0 {
		return nil, fmt.Errorf("no devices specified for parallel run")
	}

	type runResult struct {
		run *TestRun
		err error
	}

	results := make(chan runResult, len(devices))
	var wg sync.WaitGroup

	for _, device := range devices {
		wg.Add(1)
		dev := device // capture
		go func() {
			defer wg.Done()
			run, err := r.RunScenario(ctx, dev, cases)
			results <- runResult{run: run, err: err}
		}()
	}

	wg.Wait()
	close(results)

	runs := make([]TestRun, 0, len(devices))
	for res := range results {
		if res.err != nil {
			continue
		}
		if res.run != nil {
			runs = append(runs, *res.run)
		}
	}

	return runs, nil
}

func (r *DefaultRunner) StopRun(_ context.Context, runID string) error {
	r.mu.RLock()
	cancel, ok := r.active[runID]
	r.mu.RUnlock()
	if !ok {
		return fmt.Errorf("run %s not found or already completed", runID)
	}
	cancel()
	return nil
}

func (r *DefaultRunner) saveRun(ctx context.Context, run *TestRun) error {
	if r.store == nil {
		return nil // 无存储，静默处理
	}
	return r.store.SaveRun(ctx, run)
}

// newRunID 生成唯一运行 ID（不依赖第三方库）
func newRunID() string {
	b := make([]byte, 16)
	_, _ = rand.Read(b)
	return fmt.Sprintf("run-%x-%x-%x-%x-%x", b[0:4], b[4:6], b[6:8], b[8:10], b[10:])
}

// ── 别名函数（对外提供简洁入口） ──────────────────────────────────────────

// RunOnce 执行单次场景压测
func RunOnce(ctx context.Context, provider DeviceProvider, device TestDevice, cases []TestCase) (*TestRun, error) {
	runner := NewRunner(provider, nil)
	return runner.RunScenario(ctx, device, cases)
}

// RunBatch 批量执行并返回聚合结果
func RunBatch(ctx context.Context, provider DeviceProvider, devices []TestDevice, cases []TestCase) ([]TestRun, error) {
	runner := NewRunner(provider, nil)
	return runner.RunParallel(ctx, devices, cases)
}
