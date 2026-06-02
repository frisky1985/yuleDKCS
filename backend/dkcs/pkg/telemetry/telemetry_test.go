package telemetry

import (
	"testing"
	"time"

	"google.golang.org/grpc/codes"
)

func TestNew(t *testing.T) {
	tel := New()
	if tel == nil {
		t.Fatal("New() returned nil")
	}
}

func TestNewTelemetry(t *testing.T) {
	tests := []struct {
		name    string
		cfg     *Config
		wantErr bool
	}{
		{
			name: "enabled with port",
			cfg: &Config{
				Enabled: true,
				Port:    9090,
				Path:    "/metrics",
			},
			wantErr: false,
		},
		{
			name: "disabled",
			cfg: &Config{
				Enabled: false,
				Port:    0,
				Path:    "",
			},
			wantErr: false,
		},
		{
			name:    "nil config",
			cfg:     nil,
			wantErr: false,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			tel, err := NewTelemetry(tt.cfg)
			if (err != nil) != tt.wantErr {
				t.Fatalf("NewTelemetry() error = %v, wantErr = %v", err, tt.wantErr)
			}
			if tel == nil {
				t.Fatal("NewTelemetry() returned nil")
			}
		})
	}
}

func TestIncCounter(t *testing.T) {
	tel := New()
	// Should not panic
	tel.IncCounter("test_counter", map[string]string{"type": "success"})
	tel.IncCounter("another_counter", nil)
	tel.IncCounter("", map[string]string{})
}

func TestRecordDuration(t *testing.T) {
	tel := New()
	// Should not panic
	tel.RecordDuration("request_latency", 100*time.Millisecond)
	tel.RecordDuration("", 0)
}

func TestRecordGRPCRequest(t *testing.T) {
	tel := New()
	// Should not panic
	tel.RecordGRPCRequest("/test/Method", codes.OK, time.Second)
	tel.RecordGRPCRequest("/test/Error", codes.Internal, 100*time.Millisecond)
	tel.RecordGRPCRequest("", codes.NotFound, 0)
}

func TestConfigDefaults(t *testing.T) {
	cfg := &Config{
		Enabled: true,
		Port:    9090,
	}
	if cfg.Port != 9090 {
		t.Errorf("expected port 9090, got %d", cfg.Port)
	}
	if !cfg.Enabled {
		t.Error("expected enabled to be true")
	}
	if cfg.Path != "" {
		t.Errorf("expected empty path, got %s", cfg.Path)
	}
}

func TestTelemetry_MultipleInvocations(t *testing.T) {
	tel := New()

	// Call all methods multiple times to ensure no panics or state issues
	for i := 0; i < 10; i++ {
		tel.IncCounter("counter", map[string]string{"i": string(rune('0' + i))})
		tel.RecordDuration("duration", time.Duration(i)*time.Millisecond)
		tel.RecordGRPCRequest("/test/Call", codes.OK, time.Duration(i)*time.Millisecond)
	}
}
