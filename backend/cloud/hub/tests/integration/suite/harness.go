// Package suite provides the integration test harness.
package suite

import (
	"context"
	"fmt"
	"sync"
	"time"
)

// TestHarness orchestrates mock phones, mock TCUs, and simulation flows.
type TestHarness struct {
	mu sync.RWMutex

	phones []*MockPhoneClient
	phonesMu sync.Mutex

	tcus  []*MockTCUAgent
	tcusMu sync.Mutex

	testName  string
	startedAt time.Time

	// Default logger
	logFn func(string, ...interface{})
}

// NewTestHarness creates a new test harness.
func NewTestHarness(name string) *TestHarness {
	return &TestHarness{
		testName: name,
		logFn:    func(format string, args ...interface{}) {},
	}
}

// AddPhone adds a mock phone to the harness.
func (h *TestHarness) AddPhone(p *MockPhoneClient) {
	h.phonesMu.Lock()
	defer h.phonesMu.Unlock()
	h.phones = append(h.phones, p)
}

// AddTCU adds a mock TCU to the harness.
func (h *TestHarness) AddTCU(t *MockTCUAgent) {
	h.tcusMu.Lock()
	defer h.tcusMu.Unlock()
	h.tcus = append(h.tcus, t)
}

// GetPhone returns a phone by vendor name.
func (h *TestHarness) GetPhone(vendor string) *MockPhoneClient {
	h.phonesMu.Lock()
	defer h.phonesMu.Unlock()
	for _, p := range h.phones {
		if p.Config().Vendor == vendor {
			return p
		}
	}
	return nil
}

// GetTCU returns a TCU by vehicle ID.
func (h *TestHarness) GetTCU(vehicleID string) *MockTCUAgent {
	h.tcusMu.Lock()
	defer h.tcusMu.Unlock()
	for _, tcu := range h.tcus {
		if tcu.Config().VehicleID == vehicleID {
			return tcu
		}
	}
	return nil
}

// AllPhones returns all registered phones.
func (h *TestHarness) AllPhones() []*MockPhoneClient {
	h.phonesMu.Lock()
	defer h.phonesMu.Unlock()
	cp := make([]*MockPhoneClient, len(h.phones))
	copy(cp, h.phones)
	return cp
}

// AllTCUs returns all registered TCUs.
func (h *TestHarness) AllTCUs() []*MockTCUAgent {
	h.tcusMu.Lock()
	defer h.tcusMu.Unlock()
	cp := make([]*MockTCUAgent, len(h.tcus))
	copy(cp, h.tcus)
	return cp
}

// ============================================================
// Simulation helpers
// ============================================================

// SimulateBLEDiscovery runs BLE discovery for all phones and TCUs.
func (h *TestHarness) SimulateBLEDiscovery() []string {
	var results []string
	for _, tcu := range h.AllTCUs() {
		tcu.StartBLEAdvertising()
		results = append(results, fmt.Sprintf(
			"TCU[%s] BLE advertising: vehicle=%s MAC=%s",
			tcu.Config().TcuID, tcu.Config().VehicleID, tcu.Config().BLEMAC))
	}
	for _, phone := range h.AllPhones() {
		phone.StartBLEAdvertising()
		results = append(results, fmt.Sprintf(
			"Phone[%s] BLE scanning: vendor=%s device=%s",
			phone.Config().DeviceID, phone.Config().Vendor, phone.Config().DeviceID))
	}
	return results
}

// ============================================================
// Factory functions
// ============================================================

// CreateDefaultPhone creates a phone with standard capabilities.
func CreateDefaultPhone(vendor, deviceID, userID, protocol string) *MockPhoneClient {
	cfg := PhoneConfig{
		Vendor:   vendor,
		OS:       "android",
		DeviceID: deviceID,
		UserID:   userID,
		Protocol: protocol,
		Capabilities: PhoneCapabilities{
			BLE: true, UWB: true, NFC: true, SE: true, FiRa: true,
		},
	}
	return NewMockPhoneClient(cfg, nil)
}

// CreateDefaultTCU creates a default TCU agent.
func CreateDefaultTCU(tcuID, vehicleID, vin string) *MockTCUAgent {
	cfg := TCUConfig{
		TcuID:           tcuID,
		VehicleID:       vehicleID,
		VIN:             vin,
		FirmwareVersion: "TCU-FW-2.1.0",
		BLE:             true,
		UWB:             true,
		NFC:             true,
		SE:              true,
		BLEMAC:          "AA:BB:CC:DD:EE:FF",
		UWBID:           "uwb-veh-001",
		NFCUID:          "nfc-veh-001",
	}
	return NewMockTCUAgent(cfg, nil)
}

// RunWithTimeout runs a function with a timeout context.
func (h *TestHarness) RunWithTimeout(timeout time.Duration, fn func(ctx context.Context) error) error {
	ctx, cancel := context.WithTimeout(context.Background(), timeout)
	defer cancel()
	return fn(ctx)
}

// Start records the test start time.
func (h *TestHarness) Start() { h.startedAt = time.Now() }

// Elapsed returns elapsed time since start.
func (h *TestHarness) Elapsed() time.Duration { return time.Since(h.startedAt) }
