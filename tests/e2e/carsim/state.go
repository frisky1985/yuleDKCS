package main

import (
	"sync"
	"time"
)

// VehicleState represents the full state of the simulated vehicle.
type VehicleState struct {
	mu sync.RWMutex

	LockStatus   uint32 // 0=locked, 1=unlocked
	EngineStatus uint32 // 0=off, 1=on
	AlarmStatus  uint32 // 0=off, 1=triggered
	BatteryPct   uint32
	TrunkStatus  uint32 // 0=closed, 1=open

	Latitude  float64
	Longitude float64

	Rssi     int32 // simulated BLE RSSI
	Distance uint32

	// Key storage
	Keys              map[string]*StoredKey
	RevokedKeys       map[string]int64 // keyID -> revokedAt
	BondedDevices     map[string]bool  // deviceID -> paired
	SessionKeys       map[string][]byte // deviceID -> session key

	// Security state
	SeqCounter uint32
	LastSeqMap map[string]uint32 // deviceID -> last seq number

	// Cloud connectivity simulation
	CloudConnected bool
	OfflineMode    bool

	// Events log
	Events []*VehicleEvent
}

// StoredKey is a key stored on the vehicle.
type StoredKey struct {
	KeyID       string
	KeyType     uint32
	DeviceID    string
	UserID      string
	Permissions uint32 // bitmask
	KeyData     []byte
	PublicKey   []byte
	ValidFrom   int64
	ValidUntil  int64
	Status      uint32 // 1=active, 2=suspended, 3=revoked
	CreatedAt   int64
}

// VehicleEvent is a logged vehicle event.
type VehicleEvent struct {
	EventID   string
	EventType string // "unlock", "lock", "engine", "alarm", "key_bind", "auth_fail", "replay_detect"
	KeyID     string
	UserID    string
	Source    uint32 // 1=NFC 2=BLE 3=UWB 4=Remote 5=Edge
	Success   bool
	Timestamp int64
	Detail    string
}

// NewVehicleState creates a new vehicle in default locked state.
func NewVehicleState() *VehicleState {
	return &VehicleState{
		LockStatus:     0,
		EngineStatus:   0,
		AlarmStatus:    0,
		BatteryPct:     85,
		TrunkStatus:    0,
		Latitude:       31.2304,
		Longitude:      121.4737,
		Rssi:           -55,
		Distance:       5000, // 5m
		Keys:           make(map[string]*StoredKey),
		RevokedKeys:    make(map[string]int64),
		BondedDevices:  make(map[string]bool),
		SessionKeys:    make(map[string][]byte),
		LastSeqMap:     make(map[string]uint32),
		CloudConnected: true,
		OfflineMode:    false,
	}
}

func (vs *VehicleState) Lock()   { vs.mu.Lock(); vs.LockStatus = 0; vs.mu.Unlock() }
func (vs *VehicleState) Unlock() { vs.mu.Lock(); vs.LockStatus = 1; vs.mu.Unlock() }

func (vs *VehicleState) IsLocked() bool {
	vs.mu.RLock()
	defer vs.mu.RUnlock()
	return vs.LockStatus == 0
}

func (vs *VehicleState) TriggerAlarm() {
	vs.mu.Lock()
	vs.AlarmStatus = 1
	vs.mu.Unlock()
}

func (vs *VehicleState) ClearAlarm() {
	vs.mu.Lock()
	vs.AlarmStatus = 0
	vs.mu.Unlock()
}

func (vs *VehicleState) SetOffline(offline bool) {
	vs.mu.Lock()
	vs.OfflineMode = offline
	vs.CloudConnected = !offline
	vs.mu.Unlock()
}

func (vs *VehicleState) AddKey(key *StoredKey) {
	vs.mu.Lock()
	vs.Keys[key.KeyID] = key
	vs.mu.Unlock()
}

func (vs *VehicleState) GetKey(keyID string) *StoredKey {
	vs.mu.RLock()
	defer vs.mu.RUnlock()
	return vs.Keys[keyID]
}

func (vs *VehicleState) RemoveKey(keyID string) {
	vs.mu.Lock()
	delete(vs.Keys, keyID)
	vs.mu.Unlock()
}

func (vs *VehicleState) RevokeKey(keyID string, reason string) {
	vs.mu.Lock()
	vs.RevokedKeys[keyID] = time.Now().UnixMilli()
	if k, ok := vs.Keys[keyID]; ok {
		k.Status = 3 // revoked
	}
	vs.mu.Unlock()
}

func (vs *VehicleState) IsKeyRevoked(keyID string) bool {
	vs.mu.RLock()
	defer vs.mu.RUnlock()
	_, ok := vs.RevokedKeys[keyID]
	return ok
}

func (vs *VehicleState) IsKeyActive(keyID string) bool {
	vs.mu.RLock()
	defer vs.mu.RUnlock()
	k, ok := vs.Keys[keyID]
	if !ok {
		return false
	}
	now := time.Now().UnixMilli()
	return k.Status == 1 && k.ValidFrom <= now && k.ValidUntil >= now
}

func (vs *VehicleState) AddEvent(evt *VehicleEvent) {
	vs.mu.Lock()
	vs.Events = append(vs.Events, evt)
	vs.mu.Unlock()
}

// GetEventsSince returns events after the given timestamp.
func (vs *VehicleState) GetEventsSince(ts int64) []*VehicleEvent {
	vs.mu.RLock()
	defer vs.mu.RUnlock()
	var result []*VehicleEvent
	for _, e := range vs.Events {
		if e.Timestamp >= ts {
			result = append(result, e)
		}
	}
	return result
}

// CheckAndIncrementSeq checks for replay attacks based on sequence numbers.
func (vs *VehicleState) CheckAndIncrementSeq(deviceID string, seq uint32) bool {
	vs.mu.Lock()
	defer vs.mu.Unlock()
	lastSeq, ok := vs.LastSeqMap[deviceID]
	if ok && seq <= lastSeq {
		return false // possible replay
	}
	vs.LastSeqMap[deviceID] = seq
	return true
}

// CheckSCP03Secure verifies that a secure channel (SCP03) has been established.
func (vs *VehicleState) CheckSCP03Secure(deviceID string) bool {
	vs.mu.RLock()
	defer vs.mu.RUnlock()
	_, ok := vs.SessionKeys[deviceID]
	return ok
}

func (vs *VehicleState) GetDistance() uint32 {
	vs.mu.RLock()
	defer vs.mu.RUnlock()
	return vs.Distance
}

func (vs *VehicleState) GetStatusSnapshot() StatusPayload {
	vs.mu.RLock()
	defer vs.mu.RUnlock()
	return StatusPayload{
		LockStatus:   vs.LockStatus,
		EngineStatus: vs.EngineStatus,
		Rssi:         vs.Rssi,
		AlarmStatus:  vs.AlarmStatus,
		Distance:     vs.Distance,
		BatteryPct:   vs.BatteryPct,
		Latitude:     vs.Latitude,
		Longitude:    vs.Longitude,
	}
}

// StatusPayload mirrors the proto type for convenience.
type StatusPayload struct {
	LockStatus   uint32
	EngineStatus uint32
	Rssi         int32
	AlarmStatus  uint32
	Distance     uint32
	BatteryPct   uint32
	Latitude     float64
	Longitude    float64
}
