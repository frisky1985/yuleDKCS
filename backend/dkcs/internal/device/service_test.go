package device

import (
	"testing"

	"go.uber.org/zap"
)

func TestNewService(t *testing.T) {
	logger := zap.NewNop()
	s := NewService(logger)
	if s == nil {
		t.Fatal("NewService returned nil")
	}
}

func TestNewServiceWithLogger(t *testing.T) {
	logger := zap.NewNop()
	s := NewService(logger)
	if s == nil {
		t.Fatal("NewService returned nil")
	}
	// Verify service is properly initialized by calling through logger
	// (no exported methods to call, but the struct itself is valid)
	_ = s
}

func TestNewServiceMultipleInstances(t *testing.T) {
	logger := zap.NewNop()
	s1 := NewService(logger)
	s2 := NewService(logger)

	if s1 == nil || s2 == nil {
		t.Fatal("NewService returned nil")
	}

	// Each should be a distinct instance
	if s1 == s2 {
		t.Error("expected different service instances")
	}
}

func TestNewServiceWithFieldLogging(t *testing.T) {
	logger := zap.NewNop()
	s := NewService(logger)

	// Verify logger field "service" was added by accessing through
	// the public logger (if any) or just ensuring no panic
	_ = s
}
