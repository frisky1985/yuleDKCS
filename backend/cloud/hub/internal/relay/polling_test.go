package relay

import (
	"testing"
	"time"
)

func TestPollingStrategy_Phase1_JustCreated(t *testing.T) {
	// age = 0 (刚创建) → 5s
	s := &PollingStrategy{}
	got := s.NextInterval(0)
	want := 5 * time.Second
	if got != want {
		t.Errorf("NextInterval(0) = %v, want %v", got, want)
	}
}

func TestPollingStrategy_Phase1_AtBoundary(t *testing.T) {
	// age = 30s (边界值，≤30s 仍为阶段1) → 5s
	s := &PollingStrategy{}
	got := s.NextInterval(30 * time.Second)
	want := 5 * time.Second
	if got != want {
		t.Errorf("NextInterval(30s) = %v, want %v", got, want)
	}
}

func TestPollingStrategy_Phase2_JustAfterBoundary(t *testing.T) {
	// age = 31s (>30s, ≤2m) → 10s
	s := &PollingStrategy{}
	got := s.NextInterval(31 * time.Second)
	want := 10 * time.Second
	if got != want {
		t.Errorf("NextInterval(31s) = %v, want %v", got, want)
	}
}

func TestPollingStrategy_Phase2_Midpoint(t *testing.T) {
	// age = 45s (阶段2中间) → 10s
	s := &PollingStrategy{}
	got := s.NextInterval(45 * time.Second)
	want := 10 * time.Second
	if got != want {
		t.Errorf("NextInterval(45s) = %v, want %v", got, want)
	}
}

func TestPollingStrategy_Phase2_AtBoundary(t *testing.T) {
	// age = 2m (边界值，≤2m 仍为阶段2) → 10s
	s := &PollingStrategy{}
	got := s.NextInterval(2 * time.Minute)
	want := 10 * time.Second
	if got != want {
		t.Errorf("NextInterval(2m) = %v, want %v", got, want)
	}
}

func TestPollingStrategy_Phase3_JustAfterBoundary(t *testing.T) {
	// age = 2m1s (>2m, ≤10m) → 30s
	s := &PollingStrategy{}
	got := s.NextInterval(2*time.Minute + 1*time.Second)
	want := 30 * time.Second
	if got != want {
		t.Errorf("NextInterval(2m1s) = %v, want %v", got, want)
	}
}

func TestPollingStrategy_Phase3_Midpoint(t *testing.T) {
	// age = 5m (阶段3中间) → 30s
	s := &PollingStrategy{}
	got := s.NextInterval(5 * time.Minute)
	want := 30 * time.Second
	if got != want {
		t.Errorf("NextInterval(5m) = %v, want %v", got, want)
	}
}

func TestPollingStrategy_Phase3_AtBoundary(t *testing.T) {
	// age = 10m (边界值，≤10m 仍为阶段3) → 30s
	s := &PollingStrategy{}
	got := s.NextInterval(10 * time.Minute)
	want := 30 * time.Second
	if got != want {
		t.Errorf("NextInterval(10m) = %v, want %v", got, want)
	}
}

func TestPollingStrategy_Phase4_JustAfterBoundary(t *testing.T) {
	// age = 10m1s (>10m) → 60s
	s := &PollingStrategy{}
	got := s.NextInterval(10*time.Minute + 1*time.Second)
	want := 60 * time.Second
	if got != want {
		t.Errorf("NextInterval(10m1s) = %v, want %v", got, want)
	}
}

func TestPollingStrategy_Phase4_LongLived(t *testing.T) {
	// age = 15m (长时间未更新) → 60s
	s := &PollingStrategy{}
	got := s.NextInterval(15 * time.Minute)
	want := 60 * time.Second
	if got != want {
		t.Errorf("NextInterval(15m) = %v, want %v", got, want)
	}
}

func TestPollingStrategy_Phase4_Hour(t *testing.T) {
	// age = 1h (长期) → 60s
	s := &PollingStrategy{}
	got := s.NextInterval(1 * time.Hour)
	want := 60 * time.Second
	if got != want {
		t.Errorf("NextInterval(1h) = %v, want %v", got, want)
	}
}
