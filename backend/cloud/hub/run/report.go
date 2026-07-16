package run

import (
	"context"
	"fmt"
	"sort"
)

// ReportGenerator 报告生成器
// 负责将 TestRun 转换为结构化 TestReport，以及多运行对比报告。
type ReportGenerator interface {
	// GenerateReport 生成单次压测报告
	GenerateReport(ctx context.Context, run *TestRun) (*TestReport, error)

	// CompareReports 生成多设备/多运行的对比报告
	CompareReports(ctx context.Context, runs []*TestRun) (*ComparisonReport, error)
}

// ── 报告生成实现 ──────────────────────────────────────────────────────────

// DefaultReportGenerator 默认报告生成器
type DefaultReportGenerator struct{}

// NewReportGenerator 创建默认报告生成器
func NewReportGenerator() *DefaultReportGenerator {
	return &DefaultReportGenerator{}
}

func (g *DefaultReportGenerator) GenerateReport(_ context.Context, run *TestRun) (*TestReport, error) {
	if run == nil {
		return nil, fmt.Errorf("run is nil")
	}

	totalCases := len(run.Cases)
	passedCases := 0
	var latencies []int64

	for _, r := range run.Results {
		if r.Passed {
			passedCases++
		}
		latencies = append(latencies, r.LatencyMs)
	}

	failedCases := totalCases - passedCases

	var passRate float64
	if totalCases > 0 {
		passRate = float64(passedCases) / float64(totalCases) * 100
	}

	report := &TestReport{
		RunID:       run.RunID,
		DeviceModel: run.Device.Model,
		TotalCases:  totalCases,
		PassedCases: passedCases,
		FailedCases: failedCases,
		PassRate:    passRate,
		StartedAt:   run.StartedAt,
		Results:     run.Results,
	}

	// 计算延时时长
	if run.CompletedAt != nil {
		report.DurationMs = run.CompletedAt.Sub(run.StartedAt).Milliseconds()
	}

	// 统计延时
	if len(latencies) > 0 {
		report.AvgLatencyMs = avg(latencies)
		report.MaxLatencyMs = maxVal(latencies)
		report.P95LatencyMs = p95(latencies)
	}

	return report, nil
}

func (g *DefaultReportGenerator) CompareReports(_ context.Context, runs []*TestRun) (*ComparisonReport, error) {
	if len(runs) == 0 {
		return nil, fmt.Errorf("no runs to compare")
	}

	reports := make([]*TestReport, 0, len(runs))
	for _, run := range runs {
		report, err := g.GenerateReport(context.Background(), run)
		if err != nil {
			return nil, fmt.Errorf("generate report for run %s: %w", run.RunID, err)
		}
		reports = append(reports, report)
	}

	comp := &ComparisonReport{
		Runs:        reports,
		DeviceCount: len(reports),
	}

	// 计算 benchmark
	if len(reports) > 0 {
		bench := &BenchmarkSummary{}
		bestIdx, worstIdx := 0, 0

		for i, r := range reports {
			if r.PassRate > reports[bestIdx].PassRate {
				bestIdx = i
			}
			if r.PassRate < reports[worstIdx].PassRate {
				worstIdx = i
			}
		}

		bench.BestDevice = reports[bestIdx].DeviceModel
		bench.WorstDevice = reports[worstIdx].DeviceModel
		bench.BestPassRate = reports[bestIdx].PassRate
		bench.WorstPassRate = reports[worstIdx].PassRate

		// 按平均延时找最优/最差
		minLatIdx, maxLatIdx := 0, 0
		for i, r := range reports {
			if r.AvgLatencyMs < reports[minLatIdx].AvgLatencyMs {
				minLatIdx = i
			}
			if r.AvgLatencyMs > reports[maxLatIdx].AvgLatencyMs {
				maxLatIdx = i
			}
		}
		bench.MinAvgLatency = reports[minLatIdx].AvgLatencyMs
		bench.MaxAvgLatency = reports[maxLatIdx].AvgLatencyMs

		comp.Benchmark = bench
	}

	return comp, nil
}

// ── 统计工具函数 ──────────────────────────────────────────────────────────

func avg(vals []int64) float64 {
	if len(vals) == 0 {
		return 0
	}
	var sum int64
	for _, v := range vals {
		sum += v
	}
	return float64(sum) / float64(len(vals))
}

func maxVal(vals []int64) int64 {
	if len(vals) == 0 {
		return 0
	}
	m := vals[0]
	for _, v := range vals[1:] {
		if v > m {
			m = v
		}
	}
	return m
}

// p95 计算 P95 百分位延时
func p95(vals []int64) int64 {
	if len(vals) == 0 {
		return 0
	}
	sorted := make([]int64, len(vals))
	copy(sorted, vals)
	sort.Slice(sorted, func(i, j int) bool { return sorted[i] < sorted[j] })

	idx := int(float64(len(sorted))*0.95 + 0.5)
	if idx >= len(sorted) {
		idx = len(sorted) - 1
	}
	return sorted[idx]
}

// ── 便捷函数 ──────────────────────────────────────────────────────────────

// GenerateReport 便捷函数：快速生成报告
func GenerateReport(ctx context.Context, run *TestRun) (*TestReport, error) {
	gen := NewReportGenerator()
	return gen.GenerateReport(ctx, run)
}

// CompareReports 便捷函数：快速生成对比报告
func CompareReports(ctx context.Context, runs []*TestRun) (*ComparisonReport, error) {
	gen := NewReportGenerator()
	return gen.CompareReports(ctx, runs)
}
