package suite

import (
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rand"
	"fmt"
	"sync"
	"time"
)

// TCUConfig configures a mock vehicle TCU (Telematic Control Unit).
type TCUConfig struct {
	TcuID           string
	VehicleID       string
	VIN             string
	FirmwareVersion string
	BLE             bool
	UWB             bool
	NFC             bool
	SE              bool
	BLEMAC          string
	UWBID           string
	NFCUID          string
}

// TCUState represents the current simulated vehicle state.
type TCUState struct {
	DoorsLocked    bool
	EngineOn       bool
	LockStatus     uint8 // 0=unlocked, 1=locked
	EngineStatus   uint8 // 0=off, 1=running
	BatteryPct     int
	InteriorTemp   int     // 0.1°C
	OdometerKm     uint64
	SignalStrength int     // dBm
	Latitude       float64
	Longitude      float64
}

// TCUKey stores a digital key on the TCU.
type TCUKey struct {
	KeyID        string
	Protocol     uint8 // 1=CCC 2=ICCOA 3=ICCE
	KeyType      uint8 // 1=Owner
	AccessLevel  uint32
	SeKeyRef     string
	ValidFrom    time.Time
	ValidUntil   time.Time
	MaxUses      uint32
	UseCount     uint32
}

// MockTCUAgent simulates a vehicle's TCU.
type MockTCUAgent struct {
	config    TCUConfig
	privateKey *ecdsa.PrivateKey
	publicKey  []byte

	state     TCUState
	stateMu   sync.RWMutex
	boundKeys map[string]*TCUKey
	keysMu    sync.RWMutex

	receivedCmds []string
	cmdMu        sync.Mutex
	eventLog     []string
	eventMu      sync.Mutex

	isAdvertising bool
	advertMu      sync.Mutex
	loggerFn      func(string, ...interface{})
}

// NewMockTCUAgent creates a new mock TCU agent.
func NewMockTCUAgent(cfg TCUConfig, loggerFn func(string, ...interface{})) *MockTCUAgent {
	key, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	if err != nil {
		panic("failed to generate TCU ECDSA key: " + err.Error())
	}
	pubKey := elliptic.Marshal(elliptic.P256(), key.PublicKey.X, key.PublicKey.Y)

	if loggerFn == nil {
		loggerFn = func(format string, args ...interface{}) {}
	}

	return &MockTCUAgent{
		config:    cfg,
		privateKey: key,
		publicKey:  pubKey,
		state: TCUState{
			DoorsLocked:    true,
			EngineOn:       false,
			LockStatus:     1,
			BatteryPct:     85,
			InteriorTemp:   255,
			OdometerKm:     12345,
			SignalStrength: -65,
			Latitude:       31.2304,
			Longitude:      121.4737,
		},
		boundKeys: make(map[string]*TCUKey),
		loggerFn:  loggerFn,
	}
}

// ============================================================
// BLE Simulation
// ============================================================

// StartBLEAdvertising starts simulated BLE advertising from the vehicle.
func (t *MockTCUAgent) StartBLEAdvertising() {
	t.advertMu.Lock()
	defer t.advertMu.Unlock()
	t.isAdvertising = true
	t.logEvent("TCU BLE advertising started")
}

// StopBLEAdvertising stops BLE advertising.
func (t *MockTCUAgent) StopBLEAdvertising() {
	t.advertMu.Lock()
	defer t.advertMu.Unlock()
	t.isAdvertising = false
	t.logEvent("TCU BLE advertising stopped")
}

// IsAdvertising returns whether BLE advertising is active.
func (t *MockTCUAgent) IsAdvertising() bool {
	t.advertMu.Lock()
	defer t.advertMu.Unlock()
	return t.isAdvertising
}

// SimulatePhoneDiscovery simulates the TCU detecting a phone via BLE.
func (t *MockTCUAgent) SimulatePhoneDiscovery(phoneDeviceID string) (bool, error) {
	if !t.isAdvertising {
		t.StartBLEAdvertising()
	}
	t.logEvent("Phone discovered via BLE", "phone", phoneDeviceID)
	return true, nil
}

// ============================================================
// UWB Simulation
// ============================================================

// SimulateUWBUnlockZone simulates the vehicle detecting the phone entering
// the UWB unlock zone (typically 1-2m). Performs BLE auth + unlock.
func (t *MockTCUAgent) SimulateUWBUnlockZone(phoneDeviceID string, distanceMM uint32) bool {
	t.logEvent("UWB unlock zone entered", "phone", phoneDeviceID, "distance_mm", distanceMM)

	// Simulate BLE authentication handshake
	authenticated := t.performBLEAuth(phoneDeviceID)
	if !authenticated {
		t.logEvent("BLE auth failed", nil)
		return false
	}

	t.stateMu.Lock()
	t.state.DoorsLocked = false
	t.state.LockStatus = 0
	t.stateMu.Unlock()

	t.logEvent("Vehicle unlocked via UWB passive entry", "phone", phoneDeviceID)
	return true
}

// SimulateUWBLeaveZone simulates the phone moving away from the vehicle.
func (t *MockTCUAgent) SimulateUWBLeaveZone(phoneDeviceID string, distanceMM uint32) {
	t.logEvent("UWB leave zone", "phone", phoneDeviceID, "distance_mm", distanceMM)

	t.stateMu.Lock()
	t.state.DoorsLocked = true
	t.state.LockStatus = 1
	t.stateMu.Unlock()

	t.logEvent("Vehicle auto-locked after phone left zone")
}

// ============================================================
// NFC Simulation
// ============================================================

// SimulateNFCUnlock simulates unlocking via NFC (phone battery dead).
func (t *MockTCUAgent) SimulateNFCUnlock(keyRef string) (bool, error) {
	t.logEvent("NFC unlock attempt", "key_ref", keyRef)

	t.keysMu.RLock()
	_, exists := t.boundKeys[keyRef]
	t.keysMu.RUnlock()

	if !exists {
		// Also check by SeKeyRef
		found := false
		t.keysMu.RLock()
		for _, k := range t.boundKeys {
			if k.SeKeyRef == keyRef {
				found = true
				break
			}
		}
		t.keysMu.RUnlock()
		if !found {
			t.logEvent("NFC unlock failed: key not found", "key_ref", keyRef)
			return false, fmt.Errorf("NFC key not found: %s", keyRef)
		}
	}

	time.Sleep(15 * time.Millisecond) // NFC auth delay

	t.stateMu.Lock()
	t.state.DoorsLocked = false
	t.state.LockStatus = 0
	t.stateMu.Unlock()

	t.logEvent("Vehicle unlocked via NFC", "key_ref", keyRef)
	return true, nil
}

// ============================================================
// Command Handling (simulates MQTT subscribe)
// ============================================================

// HandleCommand processes an incoming vehicle command (simulates MQTT message).
func (t *MockTCUAgent) HandleCommand(command string) (string, error) {
	t.cmdMu.Lock()
	t.receivedCmds = append(t.receivedCmds, command)
	t.cmdMu.Unlock()

	t.logEvent("TCU received command", "command", command)

	switch command {
	case "unlock":
		return t.execLockCmd(false)
	case "lock":
		return t.execLockCmd(true)
	case "engine_start":
		return t.execEngineCmd(true)
	case "engine_stop":
		return t.execEngineCmd(false)
	case "trunk_open":
		return "ack:trunk_open:ok", nil
	case "find_car":
		return "ack:find_car:flashing_lights", nil
	default:
		return fmt.Sprintf("error: unknown command %s", command), nil
	}
}

func (t *MockTCUAgent) execLockCmd(locked bool) (string, error) {
	t.stateMu.Lock()
	t.state.DoorsLocked = locked
	if locked {
		t.state.LockStatus = 1
	} else {
		t.state.LockStatus = 0
	}
	t.stateMu.Unlock()
	status := "locked"
	if !locked {
		status = "unlocked"
	}
	return fmt.Sprintf("ack:doors_%sd", status), nil
}

func (t *MockTCUAgent) execEngineCmd(start bool) (string, error) {
	t.stateMu.Lock()
	t.state.EngineOn = start
	if start {
		t.state.EngineStatus = 1
	} else {
		t.state.EngineStatus = 0
	}
	t.stateMu.Unlock()
	return "ack:engine_ok", nil
}

// ============================================================
// Key Management (TCU-side / SE050)
// ============================================================

// StoreKey simulates storing a digital key on the vehicle SE.
func (t *MockTCUAgent) StoreKey(key *TCUKey) error {
	t.keysMu.Lock()
	defer t.keysMu.Unlock()
	t.boundKeys[key.KeyID] = key
	// Also index by SeKeyRef
	if key.SeKeyRef != "" && key.SeKeyRef != key.KeyID {
		t.boundKeys[key.SeKeyRef] = key
	}
	t.logEvent("Key stored on TCU", "key_id", key.KeyID)
	return nil
}

// GetStoredKey retrieves a key by ID or reference.
func (t *MockTCUAgent) GetStoredKey(keyID string) *TCUKey {
	t.keysMu.RLock()
	defer t.keysMu.RUnlock()
	return t.boundKeys[keyID]
}

// ListStoredKeys returns all keys stored on the TCU.
func (t *MockTCUAgent) ListStoredKeys() []*TCUKey {
	t.keysMu.RLock()
	defer t.keysMu.RUnlock()
	var keys []*TCUKey
	for _, k := range t.boundKeys {
		keys = append(keys, k)
	}
	return keys
}

// HasStoredKey checks if a key exists on the TCU.
func (t *MockTCUAgent) HasStoredKey(keyID string) bool {
	t.keysMu.RLock()
	defer t.keysMu.RUnlock()
	_, ok := t.boundKeys[keyID]
	return ok
}

// RemoveKey removes a key from the TCU.
func (t *MockTCUAgent) RemoveKey(keyID string) {
	t.keysMu.Lock()
	defer t.keysMu.Unlock()
	delete(t.boundKeys, keyID)
	t.logEvent("Key removed from TCU", "key_id", keyID)
}

// ============================================================
// State Management
// ============================================================

// SetState sets the TCU state.
func (t *MockTCUAgent) SetState(s TCUState) {
	t.stateMu.Lock()
	defer t.stateMu.Unlock()
	t.state = s
}

// GetState returns current TCU state.
func (t *MockTCUAgent) GetState() TCUState {
	t.stateMu.RLock()
	defer t.stateMu.RUnlock()
	return t.state
}

// IsDoorsLocked returns whether doors are locked.
func (t *MockTCUAgent) IsDoorsLocked() bool {
	t.stateMu.RLock()
	defer t.stateMu.RUnlock()
	return t.state.DoorsLocked
}

// SendHeartbeat simulates sending a heartbeat to DKCS.
func (t *MockTCUAgent) SendHeartbeat() {
	t.logEvent("Heartbeat sent", "battery_pct", t.GetState().BatteryPct)
}

// ============================================================
// Internal
// ============================================================

func (t *MockTCUAgent) performBLEAuth(phoneDeviceID string) bool {
	time.Sleep(5 * time.Millisecond)
	return true
}

// ============================================================
// Logging
// ============================================================

func (t *MockTCUAgent) logEvent(event string, keyvals ...interface{}) {
	t.eventMu.Lock()
	defer t.eventMu.Unlock()
	entry := fmt.Sprintf("[%s] %s", time.Now().Format("15:04:05.000"), event)
	for i := 0; i < len(keyvals); i += 2 {
		if i+1 < len(keyvals) {
			entry += fmt.Sprintf(" | %v=%v", keyvals[i], keyvals[i+1])
		}
	}
	t.eventLog = append(t.eventLog, entry)
	t.loggerFn("[TCU ] %s", entry)
}

// EventLog returns the event log.
func (t *MockTCUAgent) EventLog() []string {
	t.eventMu.Lock()
	defer t.eventMu.Unlock()
	cp := make([]string, len(t.eventLog))
	copy(cp, t.eventLog)
	return cp
}

// Config returns TCU configuration.
func (t *MockTCUAgent) Config() TCUConfig { return t.config }

// PublicKey returns TCU public key.
func (t *MockTCUAgent) PublicKey() []byte { return t.publicKey }

// ReceivedCommands returns received command strings.
func (t *MockTCUAgent) ReceivedCommands() []string {
	t.cmdMu.Lock()
	defer t.cmdMu.Unlock()
	cp := make([]string, len(t.receivedCmds))
	copy(cp, t.receivedCmds)
	return cp
}
