// Package common provides shared mock devices, certificate stores, and assertion
// helpers for CCC / ICCE compliance testing.
package common

import (
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rand"
	"crypto/x509"
	"crypto/x509/pkix"
	"encoding/pem"
	"fmt"
	"math/big"
	"sync"
	"time"
)

// ============================================================
// Compliance Test Device Abstractions
// ============================================================

// ProtocolType identifies which digital-key protocol a test targets.
type ProtocolType int

const (
	ProtocolCCC  ProtocolType = iota // CCC Digital Key 3.x
	ProtocolICCE                     // ICCE (Huawei)
	ProtocolICCOA                    // ICCOA Digital Key 3.0/4.0
)

func (p ProtocolType) String() string {
	switch p {
	case ProtocolCCC:
		return "CCC"
	case ProtocolICCE:
		return "ICCE"
	case ProtocolICCOA:
		return "ICCOA"
	default:
		return "UNKNOWN"
	}
}

// DeviceCapabilities represents the mandatory and optional capabilities
// a compliant device MUST / SHOULD support per the protocol specification.
type DeviceCapabilities struct {
	BLE        bool // BLE 5.x advertising & scanning
	UWB        bool // UWB FiRa ranging (passive entry)
	NFC        bool // NFC Type-4 tag emulation
	SecureElem bool // Secure Element (eSE / iSE)
	FiRa       bool // FiRa UWB stack

	// Protocol-specific version strings
	BLEVersion string // e.g. "5.3"
	UWBVersion string // e.g. "FiRa 1.0"
}

// CompliancePolicy defines the required capabilities for a given protocol.
type CompliancePolicy struct {
	Protocol      ProtocolType
	RequiredCaps  *DeviceCapabilities
	OptionalCaps  *DeviceCapabilities
	SpecRef       string // e.g. "CCC.TS.001 v3.1", "ICCE.TS.002 v2.0"
}

// CCCPolicy returns the policy that a CCC DK 3.x compliant device MUST meet.
func CCCPolicy() *CompliancePolicy {
	return &CompliancePolicy{
		Protocol: ProtocolCCC,
		RequiredCaps: &DeviceCapabilities{
			BLE: true, UWB: true, SecureElem: true, FiRa: true,
		},
		OptionalCaps: &DeviceCapabilities{
			NFC: true,
		},
		SpecRef: "CCC.TS.001 v3.1",
	}
}

// ICCOAPolicy returns the policy that an ICCOA compliant device MUST meet.
// ICCOA DK4.0要求: BLE+UWB+NFC+SE+FiRa 全部强制
func ICCOAPolicy() *CompliancePolicy {
	return &CompliancePolicy{
		Protocol: ProtocolICCOA,
		RequiredCaps: &DeviceCapabilities{
			BLE: true, UWB: true, NFC: true, SecureElem: true, FiRa: true,
		},
		OptionalCaps: &DeviceCapabilities{},
		SpecRef:      "ICCOA.DK.TS.001 v4.0",
	}
}

// ICCOAPolicyDK30 returns the policy for ICCOA DK3.0 compatible devices.
// DK3.0: BLE+NFC+SE 强制, UWB可选
func ICCOAPolicyDK30() *CompliancePolicy {
	return &CompliancePolicy{
		Protocol: ProtocolICCOA,
		RequiredCaps: &DeviceCapabilities{
			BLE: true, NFC: true, SecureElem: true,
		},
		OptionalCaps: &DeviceCapabilities{
			UWB: true, FiRa: true,
		},
		SpecRef: "ICCOA.DK.TS.001 v3.0",
	}
}

// ICCEPolicy returns the policy that an ICCE compliant device MUST meet.
func ICCEPolicy() *CompliancePolicy {
	return &CompliancePolicy{
		Protocol: ProtocolICCE,
		RequiredCaps: &DeviceCapabilities{
			BLE: true, UWB: true, NFC: true, SecureElem: true, FiRa: true,
		},
		OptionalCaps: &DeviceCapabilities{},
		SpecRef:      "ICCE.TS.002 v2.0",
	}
}

// ============================================================
// Mock Device (Phone) for Compliance Testing
// ============================================================

// ComplianceDevice models a phone-side device under compliance test.
type ComplianceDevice struct {
	mu sync.Mutex

	Vendor       string
	DeviceID     string
	UserID       string
	Protocol     ProtocolType
	Capabilities *DeviceCapabilities

	// Key material
	PrivateKey *ecdsa.PrivateKey
	PublicKey  []byte

	// Certificate chain (device attestation)
	DeviceCert     *x509.Certificate
	DeviceCertDER  []byte
	IssuerCertPool *x509.CertPool

	// Bound key storage
	boundKeys map[string]*BoundKeyInfo
}

// BoundKeyInfo captures the state of a bound digital key on the device.
type BoundKeyInfo struct {
	KeyID      string
	VehicleID  string
	AccessLevel uint32
	Status     string // ACTIVE / PENDING / REVOKED
	BoundAt    time.Time
	SharedSecret []byte
}

// NewComplianceDevice creates a new device for compliance testing with
// pre-generated ECDSA P-256 keys and a self-signed device certificate.
func NewComplianceDevice(vendor, deviceID, userID string, protocol ProtocolType, caps *DeviceCapabilities) (*ComplianceDevice, error) {
	key, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	if err != nil {
		return nil, fmt.Errorf("generate device key: %w", err)
	}
	pubKey := elliptic.Marshal(elliptic.P256(), key.PublicKey.X, key.PublicKey.Y)

	// Create a self-signed device certificate for attestation simulation
	template := &x509.Certificate{
		SerialNumber: big.NewInt(time.Now().UnixNano()),
		Subject: pkix.Name{
			CommonName:   deviceID,
			Organization: []string{vendor},
		},
		NotBefore:             time.Now().Add(-1 * time.Hour),
		NotAfter:              time.Now().Add(365 * 24 * time.Hour),
		KeyUsage:              x509.KeyUsageDigitalSignature,
		ExtKeyUsage:           []x509.ExtKeyUsage{x509.ExtKeyUsageClientAuth},
		BasicConstraintsValid: true,
	}
	certDER, err := x509.CreateCertificate(rand.Reader, template, template, &key.PublicKey, key)
	if err != nil {
		return nil, fmt.Errorf("create device cert: %w", err)
	}
	cert, err := x509.ParseCertificate(certDER)
	if err != nil {
		return nil, fmt.Errorf("parse device cert: %w", err)
	}

	return &ComplianceDevice{
		Vendor:       vendor,
		DeviceID:     deviceID,
		UserID:       userID,
		Protocol:     protocol,
		Capabilities: caps,
		PrivateKey:   key,
		PublicKey:    pubKey,
		DeviceCert:   cert,
		DeviceCertDER: certDER,
		boundKeys:    make(map[string]*BoundKeyInfo),
	}, nil
}

// BindKey records a key binding on the device.
func (d *ComplianceDevice) BindKey(vehicleID string, accessLevel uint32) *BoundKeyInfo {
	d.mu.Lock()
	defer d.mu.Unlock()

	secret := make([]byte, 32)
	rand.Read(secret)

	keyID := fmt.Sprintf("key-%s-%s-%d", d.Vendor, vehicleID, time.Now().UnixNano())
	info := &BoundKeyInfo{
		KeyID:        keyID,
		VehicleID:    vehicleID,
		AccessLevel:  accessLevel,
		Status:       "ACTIVE",
		BoundAt:      time.Now(),
		SharedSecret: secret,
	}
	d.boundKeys[keyID] = info
	return info
}

// GetBoundKey returns a bound key by ID.
func (d *ComplianceDevice) GetBoundKey(keyID string) *BoundKeyInfo {
	d.mu.Lock()
	defer d.mu.Unlock()
	return d.boundKeys[keyID]
}

// ListBoundKeys returns all bound keys.
func (d *ComplianceDevice) ListBoundKeys() []*BoundKeyInfo {
	d.mu.Lock()
	defer d.mu.Unlock()
	keys := make([]*BoundKeyInfo, 0, len(d.boundKeys))
	for _, v := range d.boundKeys {
		keys = append(keys, v)
	}
	return keys
}

// HasBoundKey returns true if at least one key for vehicleID exists.
func (d *ComplianceDevice) HasBoundKey(vehicleID string) bool {
	d.mu.Lock()
	defer d.mu.Unlock()
	for _, v := range d.boundKeys {
		if v.VehicleID == vehicleID {
			return true
		}
	}
	return false
}

// Sign simulates signing a challenge with the device attestation key.
func (d *ComplianceDevice) Sign(challenge []byte) ([]byte, error) {
	return ecdsa.SignASN1(rand.Reader, d.PrivateKey, challenge[:])
}

// DeviceCertPEM returns the device certificate in PEM format.
func (d *ComplianceDevice) DeviceCertPEM() []byte {
	block := &pem.Block{Type: "CERTIFICATE", Bytes: d.DeviceCertDER}
	return pem.EncodeToMemory(block)
}

// ============================================================
// Mock Vehicle (TCU) for Compliance Testing
// ============================================================

// ComplianceVehicle models a vehicle-side TCU under compliance test.
type ComplianceVehicle struct {
	mu sync.Mutex

	TcuID     string
	VehicleID string
	VIN       string

	// Key material
	PrivateKey *ecdsa.PrivateKey
	PublicKey  []byte

	// Certificate chain (vehicle attestation)
	VehicleCert    *x509.Certificate
	VehicleCertDER []byte

	// SE050 simulated key store
	seKeys map[string]*VehicleKey

	// State
	State VehicleState
}

// VehicleKey represents a digital key stored in the vehicle SE.
type VehicleKey struct {
	KeyID       string
	AccessLevel uint32
	Protocol    uint8 // 1=CCC, 2=ICCE
	SeKeyRef    string
	ValidFrom   time.Time
	ValidUntil  time.Time
}

// VehicleState represents the current vehicle state during tests.
type VehicleState struct {
	DoorsLocked bool
	EngineOn    bool
	LockStatus  uint8 // 0=unlocked, 1=locked
	EngineStatus uint8
}

// NewComplianceVehicle creates a new mock vehicle for compliance testing.
func NewComplianceVehicle(tcuID, vehicleID, vin string) (*ComplianceVehicle, error) {
	key, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	if err != nil {
		return nil, fmt.Errorf("generate vehicle key: %w", err)
	}
	pubKey := elliptic.Marshal(elliptic.P256(), key.PublicKey.X, key.PublicKey.Y)

	// Self-signed vehicle certificate
	template := &x509.Certificate{
		SerialNumber: big.NewInt(time.Now().UnixNano()),
		Subject: pkix.Name{
			CommonName:   vehicleID,
			Organization: []string{"Vehicle-OEM"},
		},
		NotBefore:             time.Now().Add(-1 * time.Hour),
		NotAfter:              time.Now().Add(365 * 24 * time.Hour),
		KeyUsage:              x509.KeyUsageDigitalSignature,
		ExtKeyUsage:           []x509.ExtKeyUsage{x509.ExtKeyUsageServerAuth},
		BasicConstraintsValid: true,
	}
	certDER, err := x509.CreateCertificate(rand.Reader, template, template, &key.PublicKey, key)
	if err != nil {
		return nil, fmt.Errorf("create vehicle cert: %w", err)
	}
	cert, err := x509.ParseCertificate(certDER)
	if err != nil {
		return nil, fmt.Errorf("parse vehicle cert: %w", err)
	}

	return &ComplianceVehicle{
		TcuID:         tcuID,
		VehicleID:     vehicleID,
		VIN:           vin,
		PrivateKey:    key,
		PublicKey:     pubKey,
		VehicleCert:   cert,
		VehicleCertDER: certDER,
		seKeys:        make(map[string]*VehicleKey),
		State: VehicleState{
			DoorsLocked:  true,
			LockStatus:   1,
			EngineOn:     false,
			EngineStatus: 0,
		},
	}, nil
}

// StoreKey stores a digital key in the simulated SE.
func (v *ComplianceVehicle) StoreKey(key *VehicleKey) {
	v.mu.Lock()
	defer v.mu.Unlock()
	v.seKeys[key.KeyID] = key
	if key.SeKeyRef != "" && key.SeKeyRef != key.KeyID {
		v.seKeys[key.SeKeyRef] = key
	}
}

// GetKey retrieves a stored key.
func (v *ComplianceVehicle) GetKey(keyID string) *VehicleKey {
	v.mu.Lock()
	defer v.mu.Unlock()
	return v.seKeys[keyID]
}

// HasKey returns true if the key exists.
func (v *ComplianceVehicle) HasKey(keyID string) bool {
	v.mu.Lock()
	defer v.mu.Unlock()
	_, ok := v.seKeys[keyID]
	return ok
}

// HandleCommand simulates vehicle CAN command execution.
func (v *ComplianceVehicle) HandleCommand(cmd string) (string, error) {
	v.mu.Lock()
	defer v.mu.Unlock()
	switch cmd {
	case "lock":
		v.State.DoorsLocked = true
		v.State.LockStatus = 1
		return "ack:lock:ok", nil
	case "unlock":
		v.State.DoorsLocked = false
		v.State.LockStatus = 0
		return "ack:unlock:ok", nil
	case "engine_start":
		v.State.EngineOn = true
		v.State.EngineStatus = 1
		return "ack:engine_start:ok", nil
	case "engine_stop":
		v.State.EngineOn = false
		v.State.EngineStatus = 0
		return "ack:engine_stop:ok", nil
	default:
		return "", fmt.Errorf("unknown command: %s", cmd)
	}
}

// ============================================================
// Compliance Assertion Helpers
// ============================================================

// AssertCapabilities checks that the device meets the required compliance policy.
func AssertCapabilities(policy *CompliancePolicy, device *ComplianceDevice) []string {
	var failures []string
	check := func(name string, required, actual bool) {
		if required && !actual {
			failures = append(failures, fmt.Sprintf(
				"%s: %s is required but not supported", policy.SpecRef, name))
		}
	}

	if req := policy.RequiredCaps; req != nil {
		check("BLE", req.BLE, device.Capabilities.BLE)
		check("UWB", req.UWB, device.Capabilities.UWB)
		check("NFC", req.NFC, device.Capabilities.NFC)
		check("SecureElement", req.SecureElem, device.Capabilities.SecureElem)
		check("FiRa", req.FiRa, device.Capabilities.FiRa)
	}
	return failures
}

// DefaultCCCDevice creates a standard CCC-compliant device for test use.
func DefaultCCCDevice(vendor, deviceID, userID string) *ComplianceDevice {
	dev, err := NewComplianceDevice(vendor, deviceID, userID, ProtocolCCC, &DeviceCapabilities{
		BLE: true, UWB: true, NFC: true, SecureElem: true, FiRa: true,
		BLEVersion: "5.3",
		UWBVersion: "FiRa 1.0",
	})
	if err != nil {
		panic("DefaultCCCDevice: " + err.Error())
	}
	return dev
}

// DefaultICCOADevice creates a standard ICCOA DK4.0-compliant device for test use.
func DefaultICCOADevice(vendor, deviceID, userID string) *ComplianceDevice {
	dev, err := NewComplianceDevice(vendor, deviceID, userID, ProtocolICCOA, &DeviceCapabilities{
		BLE: true, UWB: true, NFC: true, SecureElem: true, FiRa: true,
		BLEVersion: "5.3",
		UWBVersion: "FiRa 1.0",
	})
	if err != nil {
		panic("DefaultICCOADevice: " + err.Error())
	}
	return dev
}

// DefaultICCOADeviceDK30 creates an ICCOA DK3.0-compatible device (UWB optional).
func DefaultICCOADeviceDK30(vendor, deviceID, userID string) *ComplianceDevice {
	dev, err := NewComplianceDevice(vendor, deviceID, userID, ProtocolICCOA, &DeviceCapabilities{
		BLE: true, UWB: false, NFC: true, SecureElem: true, FiRa: false,
		BLEVersion: "5.2",
	})
	if err != nil {
		panic("DefaultICCOADeviceDK30: " + err.Error())
	}
	return dev
}

// DefaultICCEDevice creates a standard ICCE-compliant device for test use.
func DefaultICCEDevice(vendor, deviceID, userID string) *ComplianceDevice {
	dev, err := NewComplianceDevice(vendor, deviceID, userID, ProtocolICCE, &DeviceCapabilities{
		BLE: true, UWB: true, NFC: true, SecureElem: true, FiRa: true,
		BLEVersion: "5.3",
		UWBVersion: "FiRa 1.0",
	})
	if err != nil {
		panic("DefaultICCEDevice: " + err.Error())
	}
	return dev
}

// DefaultVehicle creates a standard vehicle for compliance test use.
func DefaultVehicle(tcuID, vehicleID, vin string) *ComplianceVehicle {
	v, err := NewComplianceVehicle(tcuID, vehicleID, vin)
	if err != nil {
		panic("DefaultVehicle: " + err.Error())
	}
	return v
}
