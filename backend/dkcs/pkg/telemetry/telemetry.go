// Stub telemetry package matching github.com/digitalkey/dkcs/pkg/telemetry
package telemetry

import "time"

// Telemetry stub for build purposes
type Telemetry struct{}

func New() *Telemetry {
	return &Telemetry{}
}

func (t *Telemetry) IncCounter(name string, labels map[string]string) {}
func (t *Telemetry) RecordDuration(name string, d time.Duration)     {}
