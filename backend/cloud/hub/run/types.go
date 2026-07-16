package run

import "time"

// ── 被测设备 ──────────────────────────────────────────────────────────────

// TestDevice 被测设备
// 用于压测场景中标记执行测试的具体物理设备或模拟器。
// DeviceID 是唯一标识，Model/OSVersion 用于报告分组。
type TestDevice struct {
	DeviceID   string   `json:"device_id"`
	Model      string   `json:"model"`       // iPhone 15 Pro, Xiaomi 14, etc.
	OSVersion  string   `json:"os_version"`
	Protocol   string   `json:"protocol"`    // "BLE", "UWB", "NFC"
	Capability []string `json:"capability"`
}

// ── 测试用例定义 ──────────────────────────────────────────────────────────

// TestCase 测试用例
// 一个用例包含多个串行步骤，所有步骤通过即为整体通过。
type TestCase struct {
	ID          string        `json:"id"`
	Name        string        `json:"name"`
	Description string        `json:"description"`
	Protocol    string        `json:"protocol"`
	Steps       []TestStep    `json:"steps"`
	Timeout     time.Duration `json:"timeout"`
	Expectation string        `json:"expectation"`
}

// TestStep 测试步骤
// Order 决定步骤执行顺序，Action 对应数字钥匙典型操作。
// Params 提供灵活的参数扩展（如 NFC 卡号、分享目标等）。
type TestStep struct {
	Order      int            `json:"order"`
	Action     string         `json:"action"`      // "connect", "unlock", "lock", "start", "share", "revoke"
	Params     map[string]any `json:"params"`
	Expected   string         `json:"expected"`
	MaxLatency time.Duration  `json:"max_latency_ms"`
}

// StepResult 单步执行结果
type StepResult struct {
	Passed     bool          `json:"passed"`
	LatencyMs  int64         `json:"latency_ms"`
	Error      string        `json:"error,omitempty"`
	StartedAt  time.Time     `json:"started_at"`
	CompletedAt time.Time    `json:"completed_at"`
}

// ── 压测执行 ──────────────────────────────────────────────────────────────

// TestRun 一次压测运行
// 记录从开始到完成的完整上下文，包含所有测试用例结果。
type TestRun struct {
	RunID       string        `json:"run_id"`
	Device      TestDevice    `json:"device"`
	Cases       []TestCase    `json:"cases"`
	Status      string        `json:"status"`        // "pending", "running", "passed", "failed", "timeout"
	StartedAt   time.Time     `json:"started_at"`
	CompletedAt *time.Time    `json:"completed_at"`
	Results     []TestResult  `json:"results"`
}

// TestResult 单条测试结果
type TestResult struct {
	CaseID      string    `json:"case_id"`
	Passed      bool      `json:"passed"`
	LatencyMs   int64     `json:"latency_ms"`
	Error       string    `json:"error,omitempty"`
	StartedAt   time.Time `json:"started_at"`
	CompletedAt time.Time `json:"completed_at"`
}

// ── 测试报告 ──────────────────────────────────────────────────────────────

// TestReport 压测报告
// 包含单次运行的所有统计分析：通过率、延时分布（avg/max/P95）。
type TestReport struct {
	RunID        string        `json:"run_id"`
	DeviceModel  string        `json:"device_model"`
	TotalCases   int           `json:"total_cases"`
	PassedCases  int           `json:"passed_cases"`
	FailedCases  int           `json:"failed_cases"`
	PassRate     float64       `json:"pass_rate"`
	AvgLatencyMs float64       `json:"avg_latency_ms"`
	MaxLatencyMs int64         `json:"max_latency_ms"`
	P95LatencyMs int64         `json:"p95_latency_ms"`
	StartedAt    time.Time     `json:"started_at"`
	DurationMs   int64         `json:"duration_ms"`
	Results      []TestResult  `json:"results"`
}

// ComparisonReport 对比报告
// 支持多机型/多协议的压测结果横向对比。
type ComparisonReport struct {
	Runs         []*TestReport         `json:"runs"`
	DeviceCount  int                   `json:"device_count"`
	Benchmark    *BenchmarkSummary     `json:"benchmark"`
}

// BenchmarkSummary 基准对比摘要
type BenchmarkSummary struct {
	BestDevice    string  `json:"best_device"`
	WorstDevice   string  `json:"worst_device"`
	BestPassRate  float64 `json:"best_pass_rate"`
	WorstPassRate float64 `json:"worst_pass_rate"`
	MinAvgLatency float64 `json:"min_avg_latency"`
	MaxAvgLatency float64 `json:"max_avg_latency"`
}
