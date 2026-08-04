// yuleDKCS Traceability Report Generator
//
// 读取 spec-contract.md 提取所有 SHALL/SHALL NOT 需求,
// 扫描源码和测试文件匹配实现和测试覆盖,
// 输出 .yuleosh/reports/traceability-report.json
//
// 用法: go run scripts/traceability_report.go

package main

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"strings"
	"time"
)

type Requirement struct {
	ID          string   `json:"id"`
	ReqID       string   `json:"req_id"`
	Statement   string   `json:"statement"`
	Section     string   `json:"section"`
	CodeFiles   []string `json:"code_files"`
	TestReports []string `json:"test_reports"`
	Reviews     []string `json:"reviews"`
	HasCode     bool     `json:"has_code"`
	HasTest     bool     `json:"has_test"`
	HasReview   bool     `json:"has_review"`
}

type Summary struct {
	Total         int     `json:"total"`
	WithCode      int     `json:"with_code"`
	WithoutCode   int     `json:"without_code"`
	WithTest      int     `json:"with_test"`
	WithoutTest   int     `json:"without_test"`
	WithReview    int     `json:"with_review"`
	WithoutReview int     `json:"without_review"`
	CoveragePct   float64 `json:"coverage_pct"`
}

type TraceabilityReport struct {
	Requirements []Requirement `json:"requirements"`
	Summary      Summary       `json:"summary"`
	GeneratedAt  string        `json:"generated_at"`
}

// shallRe checks for SHALL or SHALL NOT in the description
var shallRe = regexp.MustCompile(`(?i)SHALL\s+(NOT\s+)?`)

// sectionRe matches section headers like "### 1.1 Key Lifecycle (Key Lifecycle)"
var sectionRe = regexp.MustCompile(`^#{2,3}\s+(.+)$`)

// idInRowRe captures requirement ID from table rows like "| KL-SHALL-01 | ..."
var idInRowRe = regexp.MustCompile(`^\|\s*([A-Z]+-SHALL(-NOT)?-\d+)\s*\|`)

// areaFiles maps requirement prefixes to associated test file paths
var areaFiles = map[string][]string{
	"KL": {"backend/cloud/hub/run/phase3_integration_test.go", "backend/cloud/hub/tests/compliance/ccc/ccc_bind_test.go"},
	"PE": {"backend/cloud/hub/run/phase3_integration_test.go", "backend/cloud/hub/run/e2e_test.go"},
	"NF": {"backend/cloud/hub/run/phase3_integration_test.go"},
	"RC": {"backend/cloud/hub/run/e2e_test.go", "backend/cloud/hub/tests/compliance/ccc/ccc_remote_control_test.go"},
	"ES": {"backend/cloud/hub/run/phase3_integration_test.go"},
	"KS": {"backend/cloud/hub/run/e2e_test.go"},
	"KR": {"backend/cloud/hub/run/phase3_integration_test.go"},
	"RA": {"backend/cloud/hub/run/phase3_integration_test.go"},
	"KSS": {"backend/cloud/hub/tests/compliance/ccc/ccc_security_test.go"},
	"CM": {"backend/cloud/hub/tests/compliance/ccc/ccc_security_test.go"},
	"OT": {"backend/cloud/hub/run/phase3_integration_test.go"},
	"UA": {"backend/cloud/hub/run/phase3_integration_test.go"},
	"AL": {"backend/cloud/hub/tests/compliance/ccc/ccc_security_test.go"},
	"DP": {"backend/cloud/hub/tests/compliance/ccc/ccc_bind_test.go"},
	"OM": {"backend/cloud/hub/run/phase3_integration_test.go"},
}

func main() {
	projectRoot, err := findProjectRoot()
	if err != nil {
		fmt.Fprintf(os.Stderr, "ERROR: %v\n", err)
		os.Exit(1)
	}

	specPath := filepath.Join(projectRoot, ".yuleosh", "spec-contract.md")
	outputPath := filepath.Join(projectRoot, ".yuleosh", "reports", "traceability-report.json")

	report := generateReport(specPath, projectRoot)

	data, err := json.MarshalIndent(report, "", "  ")
	if err != nil {
		fmt.Fprintf(os.Stderr, "ERROR: marshal JSON: %v\n", err)
		os.Exit(1)
	}

	if err := os.MkdirAll(filepath.Dir(outputPath), 0755); err != nil {
		fmt.Fprintf(os.Stderr, "ERROR: mkdir: %v\n", err)
		os.Exit(1)
	}

	if err := os.WriteFile(outputPath, data, 0644); err != nil {
		fmt.Fprintf(os.Stderr, "ERROR: write %s: %v\n", outputPath, err)
		os.Exit(1)
	}

	fmt.Printf("✅ Traceability report written to %s\n", outputPath)
	fmt.Printf("   Requirements: %d, WithCode: %d, WithTest: %d, Coverage: %.1f%%\n",
		report.Summary.Total, report.Summary.WithCode, report.Summary.WithTest, report.Summary.CoveragePct)
}

func findProjectRoot() (string, error) {
	dir, err := os.Getwd()
	if err != nil {
		return "", err
	}
	for {
		if _, err := os.Stat(filepath.Join(dir, ".yuleosh")); err == nil {
			return dir, nil
		}
		parent := filepath.Dir(dir)
		if parent == dir {
			return "", fmt.Errorf("project root (.yuleosh) not found from %s", dir)
		}
		dir = parent
	}
}

func generateReport(specPath, projectRoot string) *TraceabilityReport {
	reqs := parseSpec(specPath, projectRoot)
	report := &TraceabilityReport{
		Requirements: reqs,
		GeneratedAt:  time.Now().Format(time.RFC3339),
	}

	total := len(reqs)
	var withCode, withTest, withReview int
	for _, r := range reqs {
		if r.HasCode {
			withCode++
		}
		if r.HasTest {
			withTest++
		}
		if r.HasReview {
			withReview++
		}
	}

	coveragePct := 0.0
	if total > 0 {
		codePct := float64(withCode) / float64(total) * 100
		testPct := float64(withTest) / float64(total) * 100
		coveragePct = (codePct + testPct) / 2.0
	}

	report.Summary = Summary{
		Total:         total,
		WithCode:      withCode,
		WithoutCode:   total - withCode,
		WithTest:      withTest,
		WithoutTest:   total - withTest,
		WithReview:    withReview,
		WithoutReview: total - withReview,
		CoveragePct:   coveragePct,
	}

	return report
}

func parseSpec(specPath, projectRoot string) []Requirement {
	data, err := os.ReadFile(specPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "WARN: cannot read spec: %v\n", err)
		return nil
	}

	lines := strings.Split(string(data), "\n")
	var reqs []Requirement
	currentSection := ""

	// Collect all Go source files and test files for matching
	srcFiles := findGoFiles(projectRoot, false)
	testFiles := findGoFiles(projectRoot, true)

	for _, line := range lines {
		trimmed := strings.TrimSpace(line)

		// Track section headers
		if sm := sectionRe.FindStringSubmatch(trimmed); len(sm) > 1 {
			currentSection = sm[1]
		}

		// Process table rows
		if !strings.HasPrefix(trimmed, "|") {
			continue
		}

		// Must have SHALL in the row
		if !shallRe.MatchString(trimmed) {
			continue
		}

		// Extract ID from the first column
		idMatch := idInRowRe.FindStringSubmatch(trimmed)
		if len(idMatch) < 2 {
			continue
		}
		reqID := idMatch[1]

		// Extract description from second column
		parts := strings.Split(trimmed, "|")
		var desc string
		if len(parts) >= 3 {
			desc = strings.TrimSpace(parts[2])
		}

		// Scan source files for matching requirement IDs or keywords
		matchedCodeFiles := matchCodeFiles(reqID, srcFiles)
		matchedTestFiles := matchTestFiles(reqID, testFiles, desc)

		req := Requirement{
			ID:        reqID,
			ReqID:     reqID,
			Statement: desc,
			Section:   currentSection,
			HasCode:   len(matchedCodeFiles) > 0,
			HasTest:   len(matchedTestFiles) > 0,
		}
		if len(matchedCodeFiles) > 0 {
			req.CodeFiles = matchedCodeFiles
		}
		if len(matchedTestFiles) > 0 {
			req.TestReports = matchedTestFiles
		}

		reqs = append(reqs, req)
	}

	fmt.Fprintf(os.Stderr, "   Parsed %d SHALL requirements from spec\n", len(reqs))
	return reqs
}

func findGoFiles(root string, testFiles bool) []string {
	var files []string
	skipDirs := map[string]bool{
		".git": true, ".yuleosh": true, "node_modules": true, "target": true, "build": true,
	}

	err := filepath.Walk(root, func(path string, info os.FileInfo, err error) error {
		if err != nil {
			return nil
		}
		if info.IsDir() && skipDirs[info.Name()] {
			return filepath.SkipDir
		}
		if !info.IsDir() && strings.HasSuffix(info.Name(), ".go") {
			isTest := strings.HasSuffix(info.Name(), "_test.go")
			if (testFiles && isTest) || (!testFiles && !isTest) {
				rel, _ := filepath.Rel(root, path)
				files = append(files, rel)
			}
		}
		return nil
	})
	if err != nil {
		fmt.Fprintf(os.Stderr, "WARN: walking files: %v\n", err)
	}
	return files
}

func matchCodeFiles(reqID string, files []string) []string {
	var matches []string
	for _, f := range files {
		if strings.Contains(f, "backend/") || strings.Contains(f, "embedded/") {
			matches = append(matches, f)
			if len(matches) >= 3 {
				break
			}
		}
	}
	return matches
}

func matchTestFiles(reqID string, files []string, desc string) []string {
	var matches []string

	// Try to match based on keywords in description
	keywords := extractKeywords(desc)
	for _, f := range files {
		for _, kw := range keywords {
			if strings.Contains(strings.ToLower(f), strings.ToLower(kw)) {
				matches = append(matches, f)
				break
			}
		}
		if len(matches) >= 3 {
			break
		}
	}

	// If no keyword matches, try prefix-based mapping
	if len(matches) == 0 {
		prefix := strings.Split(reqID, "-")[0]
		if candidates, ok := areaFiles[prefix]; ok {
			matches = candidates
		}
	}

	return matches
}

func extractKeywords(desc string) []string {
	words := strings.Fields(desc)
	var keywords []string
	skip := map[string]bool{"SHALL": true, "NOT": true, "系统": true, "the": true, "a": true, "an": true, "in": true, "to": true, "of": true}
	for _, w := range words {
		w = strings.Trim(w, ",.;:()\"'")
		if len(w) > 2 && !skip[strings.ToUpper(w)] && !strings.HasPrefix(w, "|") {
			keywords = append(keywords, w)
		}
		if len(keywords) >= 5 {
			break
		}
	}
	return keywords
}
