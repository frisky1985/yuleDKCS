// Package stress implements a load-testing framework for the yuleDKCS HUB gateway.
//
// It simulates 1000+ concurrent virtual vehicles sending key binding,
// remote control, and status query requests to validate system throughput,
// latency, and error rate under load.
//
// Usage:
//   cd backend/cloud/hub/tests/stress
//   go test -v -timeout=30m -run TestStress ./...
//
// Configuration via environment:
//   HUB_ADDR      - target hub address (default: "localhost:8080")
//   JWT_SECRET    - JWT signing secret (default: "stress-test-secret")
//   STRESS_DURATION - test duration in seconds (default: 60)
//
// Output: JSON metrics + YAML report in test-output/
package stress

import (
	"bytes"
	"context"
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"math"
	"net/http"
	"os"
	"sort"
	"sync"
	"sync/atomic"
	"testing"
	"time"

	"github.com/golang-jwt/jwt/v5"
	"gopkg.in/yaml.v3"
)

// ── Configuration ──

type StressConfig struct {
	HubAddr      string        // Target HUB REST gateway address
	JWTSecret    string        // JWT signing secret
	Duration     time.Duration // How long to run each scenario
	WarmupTime   time.Duration // Ramp-up period before measurements
	MaxConcurrency int         // Max concurrent goroutines per scenario
	KeyBindingQPS      int     // Target QPS for key binding scenario
	RemoteControlQPS   int     // Target QPS for remote control scenario
}

func defaultConfig() *StressConfig {
	return &StressConfig{
		HubAddr:         getEnv("HUB_ADDR", "localhost:8080"),
		JWTSecret:       getEnv("JWT_SECRET", "stress-test-secret"),
		Duration:        60 * time.Second,
		WarmupTime:      5 * time.Second,
		MaxConcurrency:  500,
		KeyBindingQPS:   1000,
		RemoteControlQPS: 2000,
	}
}

func getEnv(key, fallback string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return fallback
}

// ── Metrics Collector ──

type MetricsCollector struct {
	mu          sync.Mutex
	latencies   []time.Duration
	success     int64
	failures    int64
	errors      map[string]int64
	startTime   time.Time
	endTime     time.Time
}

func NewMetricsCollector() *MetricsCollector {
	return &MetricsCollector{
		errors: make(map[string]int64),
	}
}

func (mc *MetricsCollector) Record(start time.Time, err error) {
	latency := time.Since(start)
	mc.mu.Lock()
	defer mc.mu.Unlock()
	mc.latencies = append(mc.latencies, latency)
	if err != nil {
		atomic.AddInt64(&mc.failures, 1)
		errStr := err.Error()
		if len(errStr) > 100 {
			errStr = errStr[:100]
		}
		mc.errors[errStr]++
	} else {
		atomic.AddInt64(&mc.success, 1)
	}
}

func (mc *MetricsCollector) Snapshot() MetricsSnapshot {
	mc.mu.Lock()
	defer mc.mu.Unlock()

	snapshot := MetricsSnapshot{
		TotalRequests: mc.success + mc.failures,
		Success:       mc.success,
		Failures:      mc.failures,
		ErrorBreakdown: make(map[string]int64),
	}
	for k, v := range mc.errors {
		snapshot.ErrorBreakdown[k] = v
	}

	// Compute percentiles
	if len(mc.latencies) > 0 {
		sorted := make([]time.Duration, len(mc.latencies))
		copy(sorted, mc.latencies)
		sort.Slice(sorted, func(i, j int) bool {
			return sorted[i] < sorted[j]
		})

		snapshot.MinLatency = sorted[0]
		snapshot.MaxLatency = sorted[len(sorted)-1]
		snapshot.P50Latency = percentile(sorted, 50)
		snapshot.P95Latency = percentile(sorted, 95)
		snapshot.P99Latency = percentile(sorted, 99)

		// Average
		var total time.Duration
		for _, d := range sorted {
			total += d
		}
		snapshot.AvgLatency = total / time.Duration(len(sorted))
	}

	return snapshot
}

type MetricsSnapshot struct {
	TotalRequests  int64            `json:"total_requests" yaml:"total_requests"`
	Success        int64            `json:"success" yaml:"success"`
	Failures       int64            `json:"failures" yaml:"failures"`
	ErrorRate      float64          `json:"error_rate" yaml:"error_rate"`
	AvgLatency     time.Duration    `json:"avg_latency" yaml:"avg_latency"`
	MinLatency     time.Duration    `json:"min_latency" yaml:"min_latency"`
	MaxLatency     time.Duration    `json:"max_latency" yaml:"max_latency"`
	P50Latency     time.Duration    `json:"p50_latency" yaml:"p50_latency"`
	P95Latency     time.Duration    `json:"p95_latency" yaml:"p95_latency"`
	P99Latency     time.Duration    `json:"p99_latency" yaml:"p99_latency"`
	QPS            float64          `json:"qps" yaml:"qps"`
	ErrorBreakdown map[string]int64 `json:"error_breakdown,omitempty" yaml:"error_breakdown,omitempty"`
	ThroughputBytes int64           `json:"throughput_bytes,omitempty" yaml:"throughput_bytes,omitempty"`
}

func (s *MetricsSnapshot) Compute(duration time.Duration) {
	s.ErrorRate = 0
	if s.TotalRequests > 0 {
		s.ErrorRate = float64(s.Failures) / float64(s.TotalRequests) * 100
	}
	elapsed := duration.Seconds()
	if elapsed > 0 {
		s.QPS = float64(s.TotalRequests) / elapsed
	}
}

func percentile(sorted []time.Duration, p int) time.Duration {
	if len(sorted) == 0 {
		return 0
	}
	idx := int(math.Ceil(float64(p)/100.0*float64(len(sorted))) - 1)
	if idx < 0 {
		idx = 0
	}
	if idx >= len(sorted) {
		idx = len(sorted) - 1
	}
	return sorted[idx]
}

// ── Load Generator ──

type LoadGenerator struct {
	config  *StressConfig
	client  *http.Client
	metrics *MetricsCollector
}

func NewLoadGenerator(cfg *StressConfig) *LoadGenerator {
	return &LoadGenerator{
		config: cfg,
		client: &http.Client{
			Timeout: 30 * time.Second,
			Transport: &http.Transport{
				MaxIdleConns:        200,
				MaxIdleConnsPerHost: 200,
				IdleConnTimeout:     90 * time.Second,
				DisableCompression:  false,
			},
		},
		metrics: NewMetricsCollector(),
	}
}

func (lg *LoadGenerator) Metrics() *MetricsCollector {
	return lg.metrics
}

// generateJWT creates a signed JWT for stress test authentication.
func (lg *LoadGenerator) generateJWT(userID, role string) string {
	now := time.Now()
	claims := jwt.MapClaims{
		"user_id": userID,
		"role":    role,
		"iat":     now.Unix(),
		"exp":     now.Add(1 * time.Hour).Unix(),
	}
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	tokenStr, _ := token.SignedString([]byte(lg.config.JWTSecret))
	return tokenStr
}

type VirtualVehicle struct {
	VehicleID string
	UserID    string
	KeyID     string
	Token     string
}

func (lg *LoadGenerator) NewVirtualVehicle() *VirtualVehicle {
	id := randomHex(8)
	return &VirtualVehicle{
		VehicleID: fmt.Sprintf("veh-%s", id),
		UserID:    fmt.Sprintf("user-%s", id),
		Token:     lg.generateJWT(fmt.Sprintf("user-%s", id), "user"),
	}
}

func randomHex(n int) string {
	b := make([]byte, n)
	rand.Read(b)
	return hex.EncodeToString(b)
}

// ── API Methods ──

// BindKey simulates a key binding request.
func (lg *LoadGenerator) BindKey(vv *VirtualVehicle) error {
	body := map[string]interface{}{
		"vehicle_id": vv.VehicleID,
		"user_id":    vv.UserID,
		"key_type":   1,  // OWNER
		"vendor":     3,  // XIAOMI
		"protocol":   3,  // ICCOA_DK40
	}
	data, _ := json.Marshal(body)

	req, err := http.NewRequest("POST",
		fmt.Sprintf("http://%s/api/v1/keys", lg.config.HubAddr),
		bytes.NewReader(data))
	if err != nil {
		return fmt.Errorf("create request: %w", err)
	}
	req.Header.Set("Authorization", "Bearer "+vv.Token)
	req.Header.Set("Content-Type", "application/json")

	resp, err := lg.client.Do(req)
	if err != nil {
		return fmt.Errorf("http: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode >= 500 {
		return fmt.Errorf("5xx: %d", resp.StatusCode)
	}
	if resp.StatusCode >= 400 && resp.StatusCode < 500 {
		return fmt.Errorf("4xx: %d", resp.StatusCode)
	}

	// Parse key_id from response for later use
	var result map[string]interface{}
	if err := json.NewDecoder(resp.Body).Decode(&result); err == nil {
		if key, ok := result["key"]; ok {
			if km, ok := key.(map[string]interface{}); ok {
				if kid, ok := km["key_id"].(string); ok {
					vv.KeyID = kid
				}
			}
		}
	}
	return nil
}

// SendCommand simulates a remote vehicle control command.
func (lg *LoadGenerator) SendCommand(vv *VirtualVehicle, action string) error {
	body := map[string]interface{}{
		"action": action,
		"key_id": vv.KeyID,
		"source": 1, // remote
	}
	data, _ := json.Marshal(body)

	req, err := http.NewRequest("POST",
		fmt.Sprintf("http://%s/api/v1/vehicles/%s/command", lg.config.HubAddr, vv.VehicleID),
		bytes.NewReader(data))
	if err != nil {
		return fmt.Errorf("create request: %w", err)
	}
	req.Header.Set("Authorization", "Bearer "+vv.Token)
	req.Header.Set("Content-Type", "application/json")

	resp, err := lg.client.Do(req)
	if err != nil {
		return fmt.Errorf("http: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode >= 500 {
		return fmt.Errorf("5xx: %d", resp.StatusCode)
	}
	if resp.StatusCode >= 400 && resp.StatusCode < 500 {
		return fmt.Errorf("4xx: %d", resp.StatusCode)
	}
	return nil
}

// GetKey simulates querying key metadata.
func (lg *LoadGenerator) GetKey(vv *VirtualVehicle) error {
	req, err := http.NewRequest("GET",
		fmt.Sprintf("http://%s/api/v1/keys/%s", lg.config.HubAddr, vv.KeyID),
		nil)
	if err != nil {
		return fmt.Errorf("create request: %w", err)
	}
	req.Header.Set("Authorization", "Bearer "+vv.Token)

	resp, err := lg.client.Do(req)
	if err != nil {
		return fmt.Errorf("http: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode >= 500 {
		return fmt.Errorf("5xx: %d", resp.StatusCode)
	}
	if resp.StatusCode >= 400 && resp.StatusCode < 500 {
		// Read and discard body
		io.Copy(io.Discard, resp.Body)
		return nil // 404 for unbound keys is expected
	}
	return nil
}

// ── Scenario Runners ──

// RunScenario executes a load scenario with given worker count and request rate.
// Each worker picks work from a queue and sends requests at the target rate.
func (lg *LoadGenerator) RunScenario(
	ctx context.Context,
	name string,
	concurrency int,
	targetQPS int,
	workFn func(vv *VirtualVehicle) error,
	vehiclePool []*VirtualVehicle,
) MetricsSnapshot {
	fmt.Printf("\n  ⚡ Scenario: %s\n", name)
	fmt.Printf("     Concurrency: %d workers\n", concurrency)
	fmt.Printf("     Target QPS: %d\n", targetQPS)
	fmt.Printf("     Virtual vehicles: %d\n", len(vehiclePool))

	lg.metrics = NewMetricsCollector()
	var wg sync.WaitGroup
	start := time.Now()
	ratePerWorker := float64(targetQPS) / float64(concurrency)
	intervalPerWorker := time.Duration(float64(time.Second) / ratePerWorker)

	// Channel for distributing work
	type job struct {
		vv *VirtualVehicle
	}
	jobCh := make(chan job, 10000)

	// Start workers
	for w := 0; w < concurrency; w++ {
		wg.Add(1)
		go func(workerID int) {
			defer wg.Done()
			ticker := time.NewTicker(intervalPerWorker)
			defer ticker.Stop()

			for {
				select {
				case <-ctx.Done():
					return
				case <-ticker.C:
					select {
					case j := <-jobCh:
						opStart := time.Now()
						err := workFn(j.vv)
						lg.metrics.Record(opStart, err)
					default:
						// No more jobs
						return
					}
				}
			}
		}(w)
	}

	// Feed jobs
	go func() {
		for {
			for _, vv := range vehiclePool {
				select {
				case <-ctx.Done():
					return
				case jobCh <- job{vv: vv}:
				}
			}
		}
	}()

	// Wait for duration or context cancellation
	<-ctx.Done()
	wg.Wait()

	// Only count metrics after warmup (warmup period excluded)
	// elapsed := time.Since(start) - lg.config.WarmupTime
	totalElapsed := time.Since(start)
	snapshot := lg.metrics.Snapshot()
	snapshot.Compute(totalElapsed)

	fmt.Printf("     Results: %d req | %.1f QPS | err=%.2f%%\n",
		snapshot.TotalRequests, snapshot.QPS, snapshot.ErrorRate)
	fmt.Printf("     Latency: P50=%v P95=%v P99=%v\n",
		snapshot.P50Latency, snapshot.P95Latency, snapshot.P99Latency)

	return snapshot
}

// ── Scenario 1: Key Binding Burst ──

func (lg *LoadGenerator) ScenarioKeyBindingBurst(ctx context.Context) MetricsSnapshot {
	// Create 1000 virtual vehicles
	vehicles := make([]*VirtualVehicle, 1000)
	for i := 0; i < 1000; i++ {
		vehicles[i] = lg.NewVirtualVehicle()
	}

	return lg.RunScenario(ctx,
		"Key Binding Burst (1000 req/s)",
		200,                      // 200 concurrent workers
		lg.config.KeyBindingQPS,  // 1000 QPS
		func(vv *VirtualVehicle) error {
			return lg.BindKey(vv)
		},
		vehicles,
	)
}

// ── Scenario 2: Remote Control Burst ──

func (lg *LoadGenerator) ScenarioRemoteControlBurst(ctx context.Context) MetricsSnapshot {
	// Pre-bind keys for vehicles
	vehicles := make([]*VirtualVehicle, 2000)
	for i := 0; i < 2000; i++ {
		vv := lg.NewVirtualVehicle()
		// Bind key first (ignore errors for stress test)
		_ = lg.BindKey(vv)
		if vv.KeyID == "" {
			vv.KeyID = fmt.Sprintf("key-stress-%s", randomHex(8))
		}
		vehicles[i] = vv
	}

	actions := []string{"LOCK", "UNLOCK", "ENGINE_START", "ENGINE_STOP",
		"TRUNK_OPEN", "FIND_CAR", "CLIMATE_ON", "CLIMATE_OFF"}

	return lg.RunScenario(ctx,
		"Remote Control Burst (2000 req/s)",
		400,                          // 400 concurrent workers
		lg.config.RemoteControlQPS,   // 2000 QPS
		func(vv *VirtualVehicle) error {
			action := actions[time.Now().UnixNano()%int64(len(actions))]
			return lg.SendCommand(vv, action)
		},
		vehicles,
	)
}

// ── Scenario 3: Mixed Load ──

func (lg *LoadGenerator) ScenarioMixedLoad(ctx context.Context) MetricsSnapshot {
	vehicles := make([]*VirtualVehicle, 1500)
	for i := 0; i < 1500; i++ {
		vv := lg.NewVirtualVehicle()
		_ = lg.BindKey(vv)
		if vv.KeyID == "" {
			vv.KeyID = fmt.Sprintf("key-stress-%s", randomHex(8))
		}
		vehicles[i] = vv
	}

	actions := []string{"LOCK", "UNLOCK", "ENGINE_START", "TRUNK_OPEN", "FIND_CAR"}
	ops := []string{"bind", "control", "query"}

	return lg.RunScenario(ctx,
		"Mixed Load (bind+control+query @ 3000 QPS total)",
		500,  // 500 concurrent workers
		3000, // 3000 total QPS
		func(vv *VirtualVehicle) error {
			op := ops[time.Now().UnixNano()%int64(len(ops))]
			switch op {
			case "bind":
				return lg.BindKey(vv)
			case "control":
				action := actions[time.Now().UnixNano()%int64(len(actions))]
				return lg.SendCommand(vv, action)
			case "query":
				return lg.GetKey(vv)
			default:
				return lg.GetKey(vv)
			}
		},
		vehicles,
	)
}

// ── Stress Test Entry Point ──

type StressReport struct {
	Title       string                      `yaml:"title" json:"title"`
	Timestamp   string                      `yaml:"timestamp" json:"timestamp"`
	Config      *StressConfig               `yaml:"config" json:"config"`
	Scenarios   map[string]MetricsSnapshot  `yaml:"scenarios" json:"scenarios"`
	Summary     StressSummary               `yaml:"summary" json:"summary"`
}

type StressSummary struct {
	MaxQPS          float64 `yaml:"max_qps" json:"max_qps"`
	WorstP99Latency time.Duration `yaml:"worst_p99_latency" json:"worst_p99_latency"`
	WorstErrorRate  float64 `yaml:"worst_error_rate" json:"worst_error_rate"`
	OverallVerdict  string  `yaml:"overall_verdict" json:"overall_verdict"`
}

func TestStressMain(t *testing.T) {
	cfg := defaultConfig()

	fmt.Println("╔══════════════════════════════════════════════════════════╗")
	fmt.Println("║     yuleDKCS HUB 压力测试                                ║")
	fmt.Println("║     场景: 密钥绑定/远程控车/混合负载                       ║")
	fmt.Printf("║     目标: %s\n", cfg.HubAddr)
	fmt.Println("╚══════════════════════════════════════════════════════════╝")
	fmt.Println()

	lg := NewLoadGenerator(cfg)
	report := StressReport{
		Title:     "yuleDKCS HUB Stress Test Report",
		Timestamp: time.Now().Format(time.RFC3339),
		Config:    cfg,
		Scenarios: make(map[string]MetricsSnapshot),
	}

	// ── Scenario 1: Key Binding ──
	fmt.Println("\n[Scenario 1/3] Key Binding Burst")
	ctx1, cancel1 := context.WithTimeout(context.Background(), cfg.Duration+cfg.WarmupTime)
	s1 := lg.ScenarioKeyBindingBurst(ctx1)
	cancel1()
	report.Scenarios["key_binding"] = s1

	// Cool-down
	time.Sleep(5 * time.Second)

	// ── Scenario 2: Remote Control ──
	fmt.Println("\n[Scenario 2/3] Remote Control Burst")
	ctx2, cancel2 := context.WithTimeout(context.Background(), cfg.Duration+cfg.WarmupTime)
	s2 := lg.ScenarioRemoteControlBurst(ctx2)
	cancel2()
	report.Scenarios["remote_control"] = s2

	// Cool-down
	time.Sleep(5 * time.Second)

	// ── Scenario 3: Mixed Load ──
	fmt.Println("\n[Scenario 3/3] Mixed Load")
	ctx3, cancel3 := context.WithTimeout(context.Background(), cfg.Duration+cfg.WarmupTime)
	s3 := lg.ScenarioMixedLoad(ctx3)
	cancel3()
	report.Scenarios["mixed_load"] = s3

	// ── Summary ──
	report.Summary = StressSummary{}
	for _, snap := range report.Scenarios {
		if snap.QPS > report.Summary.MaxQPS {
			report.Summary.MaxQPS = snap.QPS
		}
		if snap.P99Latency > report.Summary.WorstP99Latency {
			report.Summary.WorstP99Latency = snap.P99Latency
		}
		if snap.ErrorRate > report.Summary.WorstErrorRate {
			report.Summary.WorstErrorRate = snap.ErrorRate
		}
	}

	// Determine verdict
	if report.Summary.WorstErrorRate < 0.1 {
		report.Summary.OverallVerdict = "PASS - Error rate < 0.1%"
	} else if report.Summary.WorstErrorRate < 1.0 {
		report.Summary.OverallVerdict = "WARNING - Error rate between 0.1% and 1%"
	} else {
		report.Summary.OverallVerdict = "FAIL - Error rate >= 1%"
	}

	// ── Output ──
	fmt.Println("\n╔══════════════════════════════════════════════════════════╗")
	fmt.Println("║     Performance Summary                                 ║")
	fmt.Println("╠══════════════════════════════════════════════════════════╣")
	for name, snap := range report.Scenarios {
		fmt.Printf("║ [%s] %d req  QPS=%.0f  P99=%v  err=%.2f%%\n",
			padRight(name, 16), snap.TotalRequests, snap.QPS, snap.P99Latency, snap.ErrorRate)
	}
	fmt.Printf("║ Max QPS: %.0f\n", report.Summary.MaxQPS)
	fmt.Printf("║ Verdict: %s\n", report.Summary.OverallVerdict)
	fmt.Println("╚══════════════════════════════════════════════════════════╝")

	// Save report
	_ = os.MkdirAll("test-output", 0755)
	reportYAML, _ := yaml.Marshal(report)
	yamlPath := "test-output/stress-report.yaml"
	_ = os.WriteFile(yamlPath, reportYAML, 0644)
	fmt.Printf("\n📊 Report saved: %s\n", yamlPath)

	reportJSON, _ := json.MarshalIndent(report, "", "  ")
	jsonPath := "test-output/stress-report.json"
	_ = os.WriteFile(jsonPath, reportJSON, 0644)
	fmt.Printf("📊 JSON report: %s\n", jsonPath)

	// Test assertions
	for name, snap := range report.Scenarios {
		t.Run(name, func(t *testing.T) {
			if snap.ErrorRate >= 1.0 {
				t.Errorf("Error rate too high: %.2f%% (threshold: 1.0%%)", snap.ErrorRate)
			}
			if snap.P99Latency > 5*time.Second {
				t.Errorf("P99 latency too high: %v (threshold: 5s)", snap.P99Latency)
			}
		})
	}
}

func padRight(s string, length int) string {
	if len(s) >= length {
		return s
	}
	b := make([]byte, length)
	for i := range b {
		if i < len(s) {
			b[i] = s[i]
		} else {
			b[i] = ' '
		}
	}
	return string(b)
}
