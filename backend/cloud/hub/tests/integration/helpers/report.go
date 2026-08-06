// Package helpers provides utilities for integration test report generation.
package helpers

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"testing"
	"time"
)

// TestCaseResult holds the outcome of a single test case.
type TestCaseResult struct {
	Name      string
	Passed    bool
	Skipped   bool
	Duration  time.Duration
	Error     string
	Scenario  string // E2E-01, E2E-02, etc.
	Protocol  string // CCC, ICCOA, ICCE, NFC
	Timestamp time.Time
}

// TestReport collects all test results and generates an HTML report.
type TestReport struct {
	mu        sync.Mutex
	results   []TestCaseResult
	startTime time.Time
	suiteName string
}

// sharedReport is a process-wide singleton: all tests in one binary
// (scenarios or the top-level hub API tests) record into the SAME report,
// so the final HTML aggregates every recorded case instead of only the
// last test file's rows.
var (
	reportOnce  sync.Once
	sharedReport *TestReport
)

// NewTestReport returns the shared report collector (created on first call).
// The suite name of the first caller wins — subsequent callers reuse the
// collector so that GenerateHTML writes the full aggregated report.
func NewTestReport(suiteName string) *TestReport {
	reportOnce.Do(func() {
		sharedReport = &TestReport{
			suiteName: suiteName,
			startTime: time.Now(),
			results:   make([]TestCaseResult, 0),
		}
	})
	return sharedReport
}

// Record adds a test result to the report.
func (r *TestReport) Record(name string, passed bool, duration time.Duration, err string, scenario, protocol string) {
	r.mu.Lock()
	defer r.mu.Unlock()
	r.results = append(r.results, TestCaseResult{
		Name:      name,
		Passed:    passed,
		Duration:  duration,
		Error:     err,
		Scenario:  scenario,
		Protocol:  protocol,
		Timestamp: time.Now(),
	})
}

// RecordScenario defers a pass/fail record for a scenario test function.
// Scenario test files that do not record per-subtest rows can call this as
// their first statement to ensure the aggregated report covers them:
//
//	start := time.Now()
//	defer helpers.RecordScenario(t, "E2E-11: Mailbox 生命周期", "E2E-11", "gRPC", start)
func RecordScenario(t *testing.T, name, scenario, protocol string, start time.Time) {
	report := NewTestReport("yuleDKCS 场景集成测试")
	report.Record(name, !t.Failed(), time.Since(start), "", scenario, protocol)
}

// RecordSkipped adds a skipped (best-effort, environment-dependent) result.
func (r *TestReport) RecordSkipped(name, reason, scenario, protocol string, duration time.Duration) {
	r.mu.Lock()
	defer r.mu.Unlock()
	r.results = append(r.results, TestCaseResult{
		Name:      name,
		Skipped:   true,
		Duration:  duration,
		Error:     reason,
		Scenario:  scenario,
		Protocol:  protocol,
		Timestamp: time.Now(),
	})
}

// Results returns a copy of all recorded results.
func (r *TestReport) Results() []TestCaseResult {
	r.mu.Lock()
	defer r.mu.Unlock()
	cp := make([]TestCaseResult, len(r.results))
	copy(cp, r.results)
	return cp
}

// Summary returns pass/fail/skip counts.
func (r *TestReport) Summary() (total, passed, failed, skipped int) {
	r.mu.Lock()
	defer r.mu.Unlock()
	total = len(r.results)
	for _, tr := range r.results {
		if tr.Skipped {
			skipped++
		} else if tr.Passed {
			passed++
		} else {
			failed++
		}
	}
	return
}

// GenerateHTML writes a comprehensive HTML integration test report.
func (r *TestReport) GenerateHTML(outputPath string) error {
	r.mu.Lock()
	defer r.mu.Unlock()

	total := len(r.results)
	passed, failed, skipped := 0, 0, 0
	for _, tr := range r.results {
		if tr.Skipped {
			skipped++
		} else if tr.Passed {
			passed++
		} else {
			failed++
		}
	}
	passRate := 0.0
	if total > 0 {
		passRate = float64(passed) / float64(total) * 100
	}

	// Group by scenario
	type scenarioGroup struct {
		scenario string
		cases    []TestCaseResult
	}
	scenarioMap := make(map[string][]TestCaseResult)
	for _, tr := range r.results {
		scenarioMap[tr.Scenario] = append(scenarioMap[tr.Scenario], tr)
	}
	var groups []scenarioGroup
	for _, key := range []string{"E2E-01", "E2E-02", "E2E-03", "E2E-04", "E2E-05", "E2E-06", "E2E-07", "E2E-08", "E2E-09", "E2E-10", "E2E-11", "E2E-12", "E2E-13", "E2E-14", "E2E-15", "HUB-API"} {
		if cases, ok := scenarioMap[key]; ok {
			groups = append(groups, scenarioGroup{scenario: key, cases: cases})
		}
	}
	for key, cases := range scenarioMap {
		found := false
		for _, g := range groups {
			if g.scenario == key {
				found = true
				break
			}
		}
		if !found {
			groups = append(groups, scenarioGroup{scenario: key, cases: cases})
		}
	}

	var rowsHTML strings.Builder
	scenarioColor := map[string]string{
		"E2E-01": "#e3f2fd",
		"E2E-02": "#e8f5e9",
		"E2E-03": "#fff3e0",
		"E2E-04": "#f3e5f5",
		"E2E-05": "#fce4ec",
		"E2E-06": "#ede7f6",
		"E2E-07": "#e0f2f1",
		"E2E-08": "#fbe9e7",
		"E2E-09": "#fff8e1",
		"E2E-10": "#f1f8e9",
	}

	for _, g := range groups {
		for i, tr := range g.cases {
			status := `<span class="pass">✅ PASS</span>`
			detail := ""
			if tr.Skipped {
				status = `<span class="skip">⚠️ SKIP</span>`
				detail = fmt.Sprintf(`<div class="error-detail">%s</div>`, escapeHTML(tr.Error))
			} else if !tr.Passed {
				status = `<span class="fail">❌ FAIL</span>`
				detail = fmt.Sprintf(`<div class="error-detail">%s</div>`, escapeHTML(tr.Error))
			}
			bgColor := scenarioColor[tr.Scenario]
			if bgColor == "" {
				bgColor = "#ffffff"
			}
			rowPrefix := ""
			if i == 0 {
				rowPrefix = fmt.Sprintf(`<td rowspan="%d" style="background:%s;font-weight:bold;text-align:center">%s</td>`, len(g.cases), bgColor, g.scenario)
			}
			rowsHTML.WriteString(fmt.Sprintf(`<tr>
				%s
				<td>%s</td>
				<td>%s</td>
				<td>%s</td>
				<td>%s</td>
				<td>%s</td>
			</tr>`, rowPrefix, escapeHTML(tr.Name), tr.Protocol, fmtDuration(tr.Duration), status, detail))
		}
	}

	html := fmt.Sprintf(`<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>yuleDKCS 集成测试报告</title>
<style>
* { margin: 0; padding: 0; box-sizing: border-box; }
body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #f5f7fa; color: #333; padding: 20px; }
.container { max-width: 1200px; margin: 0 auto; }
.header { background: linear-gradient(135deg, #667eea 0%%, #764ba2 100%%); color: white; padding: 30px; border-radius: 12px; margin-bottom: 24px; }
.header h1 { font-size: 28px; margin-bottom: 8px; }
.header .subtitle { opacity: 0.85; font-size: 14px; }
.summary-grid { display: grid; grid-template-columns: repeat(4, 1fr); gap: 16px; margin-bottom: 24px; }
.summary-card { background: white; border-radius: 10px; padding: 20px; box-shadow: 0 2px 8px rgba(0,0,0,0.06); text-align: center; }
.summary-card .num { font-size: 36px; font-weight: 700; }
.summary-card .label { font-size: 13px; color: #888; margin-top: 4px; }
.card-total .num { color: #667eea; }
.card-pass .num { color: #4caf50; }
.card-fail .num { color: #f44336; }
.card-rate .num { color: #ff9800; }
table { width: 100%%; border-collapse: collapse; background: white; border-radius: 10px; overflow: hidden; box-shadow: 0 2px 8px rgba(0,0,0,0.06); }
th { background: #f8f9fa; padding: 14px 12px; text-align: left; font-weight: 600; font-size: 13px; text-transform: uppercase; color: #666; border-bottom: 2px solid #eee; }
td { padding: 12px; border-bottom: 1px solid #eee; font-size: 14px; vertical-align: middle; }
tr:hover td { background: #fafafa; }
.pass { color: #4caf50; font-weight: 600; }
.fail { color: #f44336; font-weight: 600; }
.skip { color: #ff9800; font-weight: 600; }
.error-detail { font-size: 12px; color: #d32f2f; margin-top: 4px; background: #ffebee; padding: 6px 10px; border-radius: 4px; max-width: 400px; word-break: break-all; }
.footer { text-align: center; padding: 24px; color: #999; font-size: 12px; }
.progress-bar { width: 100%%; height: 8px; background: #e0e0e0; border-radius: 4px; margin-top: 8px; overflow: hidden; }
.progress-fill { height: 100%%; background: linear-gradient(90deg, #4caf50, #8bc34a); border-radius: 4px; transition: width 0.3s; }
@media (max-width: 768px) {
	.summary-grid { grid-template-columns: repeat(2, 1fr); }
	table { font-size: 12px; }
	.header h1 { font-size: 22px; }
}
</style>
</head>
<body>
<div class="container">
	<div class="header">
		<h1>🔑 yuleDKCS 集成测试报告</h1>
		<div class="subtitle">
			测试套件: %s |
			运行时间: %s |
			总耗时: %s
		</div>
	</div>

	<div class="summary-grid">
		<div class="summary-card card-total">
			<div class="num">%d</div>
			<div class="label">总用例数</div>
		</div>
		<div class="summary-card card-pass">
			<div class="num">%d</div>
			<div class="label">通过</div>
		</div>
		<div class="summary-card card-fail">
			<div class="num">%d</div>
			<div class="label">失败</div>
		</div>
		<div class="summary-card card-rate">
			<div class="num">%d</div>
			<div class="label">跳过 (best-effort)</div>
		</div>
		<div class="summary-card card-rate">
			<div class="num">%.1f%%</div>
			<div class="label">通过率</div>
			<div class="progress-bar"><div class="progress-fill" style="width:%.1f%%"></div></div>
		</div>
	</div>

	<table>
	<thead>
	<tr>
		<th style="width:80px">场景</th>
		<th>测试名称</th>
		<th>协议</th>
		<th>耗时</th>
		<th>状态</th>
		<th>详情</th>
	</tr>
	</thead>
	<tbody>
	%s
	</tbody>
	</table>

	<div class="footer">
		<p>yuleDKCS Integration Test Framework | Generated: %s</p>
		<p>三端集成测试：嵌入式 C / Android+ iOS SDK / 云端 Go+Java</p>
	</div>
</div>
</body>
</html>`,
		r.suiteName,
		r.startTime.Format("2006-01-02 15:04:05"),
		fmtDuration(time.Since(r.startTime)),
		total, passed, failed, skipped, passRate, passRate,
		rowsHTML.String(),
		time.Now().Format("2006-01-02 15:04:05 MST"),
	)

	dir := filepath.Dir(outputPath)
	if err := os.MkdirAll(dir, 0755); err != nil {
		return fmt.Errorf("create output directory: %w", err)
	}
	return os.WriteFile(outputPath, []byte(html), 0644)
}

func escapeHTML(s string) string {
	s = strings.ReplaceAll(s, "&", "&amp;")
	s = strings.ReplaceAll(s, "<", "&lt;")
	s = strings.ReplaceAll(s, ">", "&gt;")
	s = strings.ReplaceAll(s, "\"", "&quot;")
	s = strings.ReplaceAll(s, "'", "&#39;")
	return s
}

func fmtDuration(d time.Duration) string {
	if d < time.Millisecond {
		return fmt.Sprintf("%dµs", d.Microseconds())
	}
	if d < time.Second {
		return fmt.Sprintf("%.0fms", float64(d.Microseconds())/1000)
	}
	return fmt.Sprintf("%.2fs", d.Seconds())
}
