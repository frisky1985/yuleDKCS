package run

import (
	"context"
	"fmt"
	"sync"
	"time"
)

// ResultStore 结果存储接口
// 支持压测结果的持久化和查询，可对接 PostgreSQL、MongoDB 等后端。
type ResultStore interface {
	// SaveRun 保存压测运行记录
	SaveRun(ctx context.Context, run *TestRun) error

	// GetRun 获取单次压测运行记录
	GetRun(ctx context.Context, runID string) (*TestRun, error)

	// ListRuns 查询运行记录列表（支持按设备和时间过滤）
	ListRuns(ctx context.Context, filter *RunFilter) ([]*TestRun, error)

	// DeleteRun 删除运行记录
	DeleteRun(ctx context.Context, runID string) error
}

// RunFilter 运行记录查询过滤条件
type RunFilter struct {
	DeviceID  string     `json:"device_id,omitempty"`
	Model     string     `json:"model,omitempty"`
	Status    string     `json:"status,omitempty"`
	Protocol  string     `json:"protocol,omitempty"`
	From      *time.Time `json:"from,omitempty"`
	To        *time.Time `json:"to,omitempty"`
	Limit     int        `json:"limit,omitempty"`
	Offset    int        `json:"offset,omitempty"`
}

// ── 内存存储实现 ──────────────────────────────────────────────────────────

// MemoryStore 内存结果存储
// 适合开发和单机测试环境，不支持持久化（进程重启后丢失）。
type MemoryStore struct {
	mu    sync.RWMutex
	runs  map[string]*TestRun
}

// NewMemoryStore 创建内存结果存储
func NewMemoryStore() *MemoryStore {
	return &MemoryStore{
		runs: make(map[string]*TestRun),
	}
}

func (s *MemoryStore) SaveRun(_ context.Context, run *TestRun) error {
	if run == nil {
		return fmt.Errorf("run is nil")
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	clone := cloneRun(run)
	s.runs[run.RunID] = clone
	return nil
}

func (s *MemoryStore) GetRun(_ context.Context, runID string) (*TestRun, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	run, ok := s.runs[runID]
	if !ok {
		return nil, fmt.Errorf("run %s not found", runID)
	}
	return cloneRun(run), nil
}

func (s *MemoryStore) ListRuns(_ context.Context, filter *RunFilter) ([]*TestRun, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()

	result := make([]*TestRun, 0, len(s.runs))
	for _, run := range s.runs {
		if !matchFilter(run, filter) {
			continue
		}
		result = append(result, cloneRun(run))
	}

	// 排序（按开始时间倒序）
	for i := 0; i < len(result); i++ {
		for j := i + 1; j < len(result); j++ {
			if result[j].StartedAt.After(result[i].StartedAt) {
				result[i], result[j] = result[j], result[i]
			}
		}
	}

	// 分页
	if filter != nil {
		offset := filter.Offset
		limit := filter.Limit
		if limit <= 0 {
			limit = 100
		}
		if offset >= len(result) {
			return nil, nil
		}
		end := offset + limit
		if end > len(result) {
			end = len(result)
		}
		result = result[offset:end]
	}

	return result, nil
}

func (s *MemoryStore) DeleteRun(_ context.Context, runID string) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	if _, ok := s.runs[runID]; !ok {
		return fmt.Errorf("run %s not found", runID)
	}
	delete(s.runs, runID)
	return nil
}

// ── 辅助函数 ──────────────────────────────────────────────────────────────

func matchFilter(run *TestRun, filter *RunFilter) bool {
	if filter == nil {
		return true
	}
	if filter.DeviceID != "" && run.Device.DeviceID != filter.DeviceID {
		return false
	}
	if filter.Model != "" && run.Device.Model != filter.Model {
		return false
	}
	if filter.Status != "" && run.Status != filter.Status {
		return false
	}
	if filter.Protocol != "" && run.Device.Protocol != filter.Protocol {
		return false
	}
	if filter.From != nil && run.StartedAt.Before(*filter.From) {
		return false
	}
	if filter.To != nil && run.StartedAt.After(*filter.To) {
		return false
	}
	return true
}

func cloneRun(run *TestRun) *TestRun {
	if run == nil {
		return nil
	}
	clone := &TestRun{
		RunID:     run.RunID,
		Device:    run.Device,
		Status:    run.Status,
		StartedAt: run.StartedAt,
		Results:   make([]TestResult, len(run.Results)),
	}
	if run.CompletedAt != nil {
		t := *run.CompletedAt
		clone.CompletedAt = &t
	}
	copy(clone.Results, run.Results)
	clone.Cases = make([]TestCase, len(run.Cases))
	copy(clone.Cases, run.Cases)
	return clone
}
