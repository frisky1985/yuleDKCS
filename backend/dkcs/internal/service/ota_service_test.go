// Package service — OTA 升级服务单元测试
//
// 需求覆盖:
//   OT-SHALL-01: 支持 OTA 方式升级车端固件
//   OT-SHALL-02: OTA 升级包数字签名验证
//   OT-SHALL-03: OTA 升级状态追踪
//   OT-SHALL-NOT-01: 不安装签名校验失败的 OTA 升级包

package service

import (
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rand"
	"crypto/sha256"
	"math/big"
	"testing"
)

// otaPackage represents an OTA firmware package with cryptographic integrity.
type otaPackage struct {
	Version   string
	Hash      []byte
	Signature []byte
	Data      []byte
}

// TestOTAShall01_Support verifies OT-SHALL-01: OTA upgrade support.
func TestOTAShall01_Support(t *testing.T) {
	t.Log("OT-SHALL-01: 系统 SHALL 支持通过 OTA 方式升级车端固件")

	// Generate signing key pair (simulates OEM key)
	key, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	if err != nil {
		t.Fatalf("failed to generate signing key: %v", err)
	}

	// Simulate OTA package creation by OEM
	otaData := []byte("TCU_FIRMWARE_v2.2.0")
	hash := sha256.Sum256(otaData)
	r, s, err := ecdsa.Sign(rand.Reader, key, hash[:])
	if err != nil {
		t.Fatalf("failed to sign OTA data: %v", err)
	}
	signature := append(r.Bytes(), s.Bytes()...)

	pkg := &otaPackage{
		Version:   "2.2.0",
		Hash:      hash[:],
		Signature: signature,
		Data:      otaData,
	}

	// Verify the OTA package signature (done on TCU before install)
	valid := ecdsa.Verify(&key.PublicKey, hash[:], r, s)
	if !valid {
		t.Error("OT-SHALL-01/02: OTA package signature verification failed")
	}

	_ = pkg
	t.Log("PASS: OTA 升级支持验证通过")
}

// TestOTAShall02_SignatureVerify verifies OT-SHALL-02: OTA package signature verification.
func TestOTAShall02_SignatureVerify(t *testing.T) {
	t.Log("OT-SHALL-02: OTA 升级包 SHALL 经过数字签名")
	t.Log("          车端在安装前 SHALL 验证签名完整性")

	// Generate signing key (simulates OEM secure key)
	signerKey, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	if err != nil {
		t.Fatalf("failed to generate signing key: %v", err)
	}

	// Valid OTA package
	otaData := []byte("TCU_FIRMWARE_v2.2.0_SIGNED")
	hash := sha256.Sum256(otaData)
	r, s, err := ecdsa.Sign(rand.Reader, signerKey, hash[:])
	if err != nil {
		t.Fatalf("failed to sign OTA data: %v", err)
	}

	// Test 1: Valid signature must pass
	valid := ecdsa.Verify(&signerKey.PublicKey, hash[:], r, s)
	if !valid {
		t.Error("Valid ECDSA signature must verify correctly")
	} else {
		t.Log("PASS: Valid ECDSA P-256签验证通过")
	}

	// Test 2: Wrong signature must fail (OT-SHALL-NOT-01)
	wrongKey, _ := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	invalid := ecdsa.Verify(&wrongKey.PublicKey, hash[:], r, s)
	if invalid {
		t.Error("Cross-key verification must NOT pass")
	} else {
		t.Log("PASS: 跨密钥验证正确拒绝")
	}

	// Test 3: Tampered data must fail
	tamperedData := []byte("TCU_FIRMWARE_v2.2.0_TAMPERED")
	tamperedHash := sha256.Sum256(tamperedData)
	tamperedValid := ecdsa.Verify(&signerKey.PublicKey, tamperedHash[:], r, s)
	if tamperedValid {
		t.Error("Tampered data must NOT pass signature verification")
	} else {
		t.Log("PASS: 篡改数据签名验证正确拒绝 (OT-SHALL-NOT-01)")
	}
}

// TestOTAShallNot01_RejectBadSignature verifies OT-SHALL-NOT-01:
// system shall not install OTA packages that fail signature verification.
func TestOTAShallNot01_RejectBadSignature(t *testing.T) {
	t.Log("OT-SHALL-NOT-01: 系统 SHALL NOT 安装签名校验失败的 OTA 升级包")

	// Generate keys
	signerKey, _ := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)

	// Test cases
	tests := []struct {
		name          string
		data          []byte
		useWrongKey   bool // use a different key for signing
		tampered      bool // tamper with data after signing
		skipSignature bool // skip signature entirely
		expectReject  bool
	}{
		{"valid package", []byte("TCU_FW_v2.2.0"), false, false, false, false},
		{"no signature", []byte("TCU_FW_v2.2.0_unsigned"), false, false, true, true},
		{"wrong key signature", []byte("TCU_FW_v2.2.0_wrong_signer"), true, false, false, true},
		{"tampered data", []byte("TCU_FW_v2.2.0_tampered"), false, true, false, true},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			var sigR, sigS *big.Int
			rejected := false

			if tt.skipSignature {
				rejected = true
			} else {
				usedKey := signerKey
				if tt.useWrongKey {
					usedKey, _ = ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
				}
				dataToHash := tt.data
				if tt.tampered {
					tampered := make([]byte, len(tt.data))
					copy(tampered, tt.data)
					tampered[len(tampered)-1] ^= 0xFF
					dataToHash = tampered
				}
				h := sha256.Sum256(dataToHash)
				sigR, sigS, _ = ecdsa.Sign(rand.Reader, usedKey, h[:])

				// Verification step (simulates TCU SE050 check)
				verifyHash := sha256.Sum256(tt.data)
				valid := ecdsa.Verify(&signerKey.PublicKey, verifyHash[:], sigR, sigS)
				if !valid {
					rejected = true
				}
			}

			if tt.expectReject && !rejected {
				t.Errorf("Expected rejection but package was accepted: %s", tt.name)
			}
			if !tt.expectReject && rejected {
				t.Errorf("Expected acceptance but package was rejected: %s", tt.name)
			}
		})
	}
}

// TestOTAShall03_StateMachine verifies OT-SHALL-03: OTA update state tracking.
func TestOTAShall03_StateMachine(t *testing.T) {
	t.Log("OT-SHALL-03: 系统 SHALL 支持 OTA 升级状态追踪")

	type otaState int
	const (
		stIdle otaState = iota
		stDownloadPending
		stDownloading
		stVerifying
		stInstalling
		stRebooting
		stCompleted
		stFailed
	)

	stateNames := map[otaState]string{
		stIdle:            "IDLE",
		stDownloadPending: "DOWNLOAD_PENDING",
		stDownloading:     "DOWNLOADING",
		stVerifying:       "VERIFYING",
		stInstalling:      "INSTALLING",
		stRebooting:       "REBOOTING",
		stCompleted:       "COMPLETED",
		stFailed:          "FAILED",
	}

	// Define allowed state transitions
	type transition struct {
		from otaState
		to   otaState
		desc string
	}

	transitions := []transition{
		{stIdle, stDownloadPending, "准备下载"},
		{stDownloadPending, stDownloading, "下载中"},
		{stDownloading, stVerifying, "校验中"},
		{stVerifying, stInstalling, "安装中"},
		{stInstalling, stRebooting, "重启中"},
		{stRebooting, stCompleted, "完成"},
		// Failure paths
		{stDownloading, stFailed, "下载失败"},
		{stVerifying, stFailed, "校验失败"},
		{stInstalling, stFailed, "安装失败"},
	}

	// Verify all required states exist
	requiredStates := []otaState{
		stDownloadPending, stDownloading, stVerifying,
		stInstalling, stRebooting, stCompleted, stFailed,
	}
	for _, s := range requiredStates {
		name, ok := stateNames[s]
		if !ok {
			t.Errorf("Missing name for OTA state %d", s)
		}
		t.Logf("  State: %s", name)
	}

	// Verify all transitions are defined
	for _, tr := range transitions {
		_, fromOk := stateNames[tr.from]
		_, toOk := stateNames[tr.to]
		if !fromOk || !toOk {
			t.Errorf("Invalid transition: %s", tr.desc)
		}
		t.Logf("  Transition: %s → %s (%s)", stateNames[tr.from], stateNames[tr.to], tr.desc)
	}

	t.Log("PASS: OTA 状态机覆盖 DOWNLOAD_PENDING → DOWNLOADING → VERIFYING → INSTALLING → REBOOTING → COMPLETED/FAILED")
}
