package telemetry

import (
	"time"

	"google.golang.org/grpc/codes"
)

// Config telemetry configuration
type Config struct {
	Enabled bool
	Port    int
	Path    string
}

// Telemetry stub for build purposes
type Telemetry struct{}

func New() *Telemetry {
	return &Telemetry{}
}

// NewTelemetry creates a new telemetry from config
func NewTelemetry(cfg *Config) (*Telemetry, error) {
	return &Telemetry{}, nil
}

func (t *Telemetry) IncCounter(name string, labels map[string]string)                                {}
func (t *Telemetry) RecordDuration(name string, d time.Duration)                                     {}
func (t *Telemetry) RecordGRPCRequest(method string, code codes.Code, duration time.Duration)          {}
