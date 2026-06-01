// Package suite provides mock clients and test harness for integration tests.
package suite

import (
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rand"
	"fmt"
	"sync"
	"time"
)

// PhoneCapabilities represents a simulated phone's hardware capabilities.
type PhoneCapabilities struct {
	BLE  bool
	UWB  bool
	NFC  bool
	SE   bool
	FiRa bool
}

// PhoneConfig configures a mock phone client.
type PhoneConfig struct {
	Vendor       string // apple, samsung, xiaomi, oppo, vivo, huawei
	OS           string // ios, android
	DeviceID     string
	UserID       string
	Capabilities PhoneCapabilities
	Protocol     string // ccc_dk3, iccoa_dk40, iccoa_dk30, icce
}

// BoundKey represents a digital key stored on the mock phone.
type BoundKey struct {
	KeyID       string
	VehicleID   string
	KeyType     string   // OWNER, FRIEND, SERVICE, TEMPORARY
	AccessLevel uint32
	Protocol    string
	Status      string   // ACTIVE, PENDING, REVOKED
	ValidFrom   int64
	ValidUntil  int64
	BoundAt     time.Time
}

// UWBRangingResult represents a simulated UWB ranging measurement.
type UWBRangingResult struct {
	DistanceMM uint32
	Confidence uint8
	Phase      string // APPROACH, LOCK_ZONE, UNLOCK_ZONE, AWAY
}

// BindResult represents the outcome of a simulated key binding.
type BindResult struct {
	KeyID     string
	VehicleID string
	Status    string
	Error     string
}

// RemoteCommandResult represents the outcome of a remote command.
type RemoteCommandResult struct {
	CmdID      string
	ResultCode int
	Error      string
}

// MockPhoneClient simulates a mobile phone acting as a digital car key device.
type MockPhoneClient struct {
	config     PhoneConfig
	privateKey *ecdsa.PrivateKey
	publicKey  []byte

	// State tracking
	boundKeys map[string]*BoundKey
	keysMu    sync.RWMutex
	loggerFn  func(format string, args ...interface{})

	// Event log
	eventLog []string
	eventMu  sync.Mutex

	// BLE simulation
	advertising bool
	advertMu    sync.Mutex

	// HUB backend URLs (for real gRPC connections)
	HubAddr string
}

// NewMockPhoneClient creates a new mock phone client.
func NewMockPhoneClient(cfg PhoneConfig, loggerFn func(string, ...interface{})) *MockPhoneClient {
	key, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	if err != nil {
		panic("failed to generate phone ECDSA key: " + err.Error())
	}
	pubKey := elliptic.Marshal(elliptic.P256(), key.PublicKey.X, key.PublicKey.Y)

	if loggerFn == nil {
		loggerFn = func(format string, args ...interface{}) {}
	}

	return &MockPhoneClient{
		config:    cfg,
		privateKey: key,
		publicKey:  pubKey,
		boundKeys:  make(map[string]*BoundKey),
		loggerFn:   loggerFn,
	}
}

// ============================================================
// BLE Simulation
// ============================================================

// StartBLEAdvertising simulates the phone sending BLE advertisements.
func (p *MockPhoneClient) StartBLEAdvertising() []byte {
	p.advertMu.Lock()
	defer p.advertMu.Unlock()
	p.advertising = true

	// Allocate enough space: 8 header bytes + device ID
	devID := []byte(p.config.DeviceID)
	advPayload := make([]byte, 8+len(devID))
	advPayload[0] = 0x02
	advPayload[1] = 0x01
	advPayload[2] = 0x06
	advPayload[3] = byte(4 + len(devID))
	advPayload[4] = 0xFF
	advPayload[5] = 0x4C
	advPayload[6] = 0x00
	advPayload[7] = 0x12
	copy(advPayload[8:], devID)

	p.logEvent("BLE advertising started", "device_id", p.config.DeviceID, "vendor", p.config.Vendor)
	return advPayload
}

// StopBLEAdvertising stops simulated BLE advertising.
func (p *MockPhoneClient) StopBLEAdvertising() {
	p.advertMu.Lock()
	defer p.advertMu.Unlock()
	p.advertising = false
	p.logEvent("BLE advertising stopped")
}

// ReadTCUBLEAdvert simulates the phone reading a vehicle's BLE advertisement.
// Returns info about the discovered TCU.
func (p *MockPhoneClient) ReadTCUBLEAdvert(vehicleID, vendorName string, rssi int8) (map[string]interface{}, error) {
	info := map[string]interface{}{
		"vehicle_id":  vehicleID,
		"vendor_name": vendorName,
		"rssi":        int(rssi),
		"protocol":    p.config.Protocol,
		"timestamp":   time.Now().UnixMilli(),
	}
	p.logEvent("BLE advert read from TCU", "vehicle", vehicleID, "rssi", rssi)
	return info, nil
}

// ============================================================
// UWB Ranging Simulation
// ============================================================

// StartUWBRanging simulates UWB distance measurement.
func (p *MockPhoneClient) StartUWBRanging(steps []UWBRangingResult) <-chan UWBRangingResult {
	ch := make(chan UWBRangingResult, len(steps))
	go func() {
		for _, step := range steps {
			select {
			case ch <- step:
			default:
			}
			time.Sleep(5 * time.Millisecond)
		}
		close(ch)
	}()
	p.logEvent("UWB ranging started", "steps", len(steps))
	return ch
}

// ============================================================
// NFC Simulation
// ============================================================

// SimulateNFCTap simulates the phone being tapped against an NFC reader.
func (p *MockPhoneClient) SimulateNFCTap(keyRef string, challenge []byte) ([]byte, error) {
	response := make([]byte, 64)
	response[0] = 0xE1
	response[1] = 0x06
	copy(response[2:8], []byte("DK_NFC"))
	response[8] = 0xE3
	response[9] = byte(len(keyRef))
	copy(response[10:], []byte(keyRef))

	p.logEvent("NFC tap simulated", "key_ref", keyRef)
	return response, nil
}

// ============================================================
// Cloud Operations (simulated via local backend)
// ============================================================

// BindKeyWithHUB simulates a key binding request flowing through HUB.
// Uses local simulation of the HUB → DKCS → Adapter flow.
func (p *MockPhoneClient) BindKeyWithHUB(vehicleID string) *BindResult {
	p.logEvent("BindKey initiated", "vehicle_id", vehicleID, "vendor", p.config.Vendor)

	// Simulate HUB receiving BindKeyRequest
	hubReceipt := fmt.Sprintf("hub:received key_bind from vendor=%s device=%s vehicle=%s",
		p.config.Vendor, p.config.DeviceID, vehicleID)
	_ = hubReceipt

	// Simulate HUB routing to adapter (vendor-specific adapter)
	adapterRoute := fmt.Sprintf("hub:routed to adapter %s/%s", p.config.Vendor, p.config.Protocol)
	_ = adapterRoute

	// Simulate adapter completing the bind
	keyID := fmt.Sprintf("key-%s-%d", p.config.Vendor, time.Now().UnixMilli())
	result := &BindResult{
		KeyID:     keyID,
		VehicleID: vehicleID,
		Status:    "ACTIVE",
	}

	// Record locally
	bk := &BoundKey{
		KeyID:     keyID,
		VehicleID: vehicleID,
		KeyType:   "OWNER",
		Protocol:  p.config.Protocol,
		Status:    "ACTIVE",
		BoundAt:   time.Now(),
	}
	p.keysMu.Lock()
	p.boundKeys[keyID] = bk
	p.keysMu.Unlock()

	p.logEvent("Key bound successfully", "key_id", keyID, "status", "ACTIVE")
	return result
}

// SendRemoteCommand simulates sending a remote control command through HUB.
// Flow: Phone → HTTPS → HUB → DKCS → MQTT → TCU
func (p *MockPhoneClient) SendRemoteCommand(vehicleID, keyID, action string) *RemoteCommandResult {
	p.logEvent("Remote command sent", "vehicle_id", vehicleID, "action", action)

	// Simulate HUB authorization check
	if !p.HasBoundKey(vehicleID) {
		return &RemoteCommandResult{
			Error: "no bound key for vehicle " + vehicleID,
		}
	}

	// Simulate DKCS MQTT dispatch
	cmdID := fmt.Sprintf("cmd-%s-%s-%d", vehicleID, action, time.Now().UnixMilli())
	return &RemoteCommandResult{
		CmdID:      cmdID,
		ResultCode: 0,
	}
}

// ============================================================
// Helpers
// ============================================================

func (p *MockPhoneClient) logEvent(event string, keyvals ...interface{}) {
	p.eventMu.Lock()
	defer p.eventMu.Unlock()
	entry := fmt.Sprintf("[%s] %s", time.Now().Format("15:04:05.000"), event)
	for i := 0; i < len(keyvals); i += 2 {
		if i+1 < len(keyvals) {
			entry += fmt.Sprintf(" | %v=%v", keyvals[i], keyvals[i+1])
		}
	}
	p.eventLog = append(p.eventLog, entry)
	p.loggerFn("[PHONE] %s", entry)
}

// EventLog returns the event log.
func (p *MockPhoneClient) EventLog() []string {
	p.eventMu.Lock()
	defer p.eventMu.Unlock()
	cp := make([]string, len(p.eventLog))
	copy(cp, p.eventLog)
	return cp
}

// Config returns the phone's configuration.
func (p *MockPhoneClient) Config() PhoneConfig { return p.config }

// PublicKey returns the phone's public key bytes.
func (p *MockPhoneClient) PublicKey() []byte { return p.publicKey }

// BoundKeys returns all keys bound to this phone.
func (p *MockPhoneClient) BoundKeys() []*BoundKey {
	p.keysMu.RLock()
	defer p.keysMu.RUnlock()
	var keys []*BoundKey
	for _, k := range p.boundKeys {
		keys = append(keys, k)
	}
	return keys
}

// GetBoundKey returns a specific bound key by ID.
func (p *MockPhoneClient) GetBoundKey(keyID string) *BoundKey {
	p.keysMu.RLock()
	defer p.keysMu.RUnlock()
	return p.boundKeys[keyID]
}

// HasBoundKey checks if a key for the given vehicle is bound.
func (p *MockPhoneClient) HasBoundKey(vehicleID string) bool {
	p.keysMu.RLock()
	defer p.keysMu.RUnlock()
	for _, k := range p.boundKeys {
		if k.VehicleID == vehicleID {
			return true
		}
	}
	return false
}

// String returns an identifier for this phone.
func (p *MockPhoneClient) String() string {
	return fmt.Sprintf("%s/%s (%s)", p.config.Vendor, p.config.DeviceID, p.config.OS)
}
