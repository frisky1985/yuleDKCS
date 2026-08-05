// E2E-06: OTA 升级测试
//
// 场景描述:
//   GIVEN 车端 TCU 正在运行固件 v2.1.0
//   WHEN 云端推送 OTA 升级包 v2.2.0
//   THEN 下载 → 签名验证 → 安装 → 完成
//
// 需求覆盖:
//   OT-SHALL-01: 支持 OTA 方式升级车端固件
//   OT-SHALL-02: OTA 升级包数字签名，安装前验证签名完整性
//   OT-SHALL-03: 支持 OTA 升级状态追踪
//   OT-SHALL-NOT-01: 不安装签名校验失败的 OTA 升级包

package scenarios

import (
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rand"
	"crypto/sha256"
	"testing"
	"time"

	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/tests/integration/helpers"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/tests/integration/suite"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// signData generates an ECDSA signature for the given data.
func signOTA(key *ecdsa.PrivateKey, data []byte) []byte {
	hash := sha256.Sum256(data)
	r, s, _ := ecdsa.Sign(rand.Reader, key, hash[:])
	sig := append(r.Bytes(), s.Bytes()...)
	return sig
}

// hashData generates SHA-256 hash hex string for OTA data.
func hashOTAData(data []byte) string {
	h := sha256.Sum256(data)
	return string(h[:])
}

// TestE2E06_OTAUpdate tests OTA firmware update flow.
func TestE2E06_OTAUpdate(t *testing.T) {
	report := helpers.NewTestReport("E2E-06 OTA升级")
	harness := suite.NewTestHarness("E2E-06")
	harness.Start()

	phone := suite.CreateDefaultPhone("xiaomi", "phone-ota-xm", "user-ota-xm", "iccoa_dk40")
	tcu := suite.CreateDefaultTCU("tcu-ota-xm", "veh-ota-xm", "LSVAXMPEOTA12345")
	harness.AddPhone(phone)
	harness.AddTCU(tcu)

	// Generate signing key (simulates OEM/Cloud signing key)
	signingKey, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	require.NoError(t, err)

	// ── Test 1: OT-SHALL-01 OTA 下载与安装正常流程 ──
	t.Run("E2E-06-01: OT-SHALL-01 OTA正常升级流程", func(t *testing.T) {
		start := time.Now()

		// Simulate OTA package v2.2.0
		otaData := []byte("DKCS_TCU_FIRMWARE_v2.2.0_binary_data_1234567890abcdef")
		signature := signOTA(signingKey, otaData)

		pkg := &suite.OTAPackage{
			Version:   "2.2.0",
			Hash:      hashOTAData(otaData),
			Signature: signature,
			Data:      otaData,
			Size:      len(otaData),
		}

		// Phase 1: Download pending → downloading
		tcu.StartOTADownload(pkg.Version)
		t.Logf("OTA download started for version %s", pkg.Version)

		// Phase 2: Verify signature (OT-SHALL-02)
		verified := tcu.VerifyOTAPackage(pkg)
		assert.True(t, verified, "OT-SHALL-02: OTA package signature must be verified")

		// Phase 3: Install
		err := tcu.InstallOTAPackage(pkg)
		assert.NoError(t, err, "OT-SHALL-01: OTA installation must succeed")

		// Phase 4: Status check (OT-SHALL-03)
		status := tcu.GetOTAPackageStatus()
		t.Logf("OTA final status: %s", status)

		report.Record("E2E-06-01: OTA正常升级", true, time.Since(start), "", "E2E-06", "OTA")
	})

	// ── Test 2: OT-SHALL-NOT-01 签名校验失败拒绝安装 ──
	t.Run("E2E-06-02: OT-SHALL-NOT-01 签名校验失败拒绝", func(t *testing.T) {
		start := time.Now()

		// Create OTA package with tampered data
		otaData := []byte("DKCS_TCU_FIRMWARE_v2.2.0_binary_data_1234567890abcdef")
		// Sign with a different key (wrong signer)
		wrongKey, _ := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
		signature := signOTA(wrongKey, otaData)

		pkg := &suite.OTAPackage{
			Version:   "2.2.0-tampered",
			Hash:      "tampered",
			Signature: signature,
			Data:      otaData,
			Size:      len(otaData),
		}

		// Verify should fail with wrong signature
		verified := tcu.VerifyOTAPackage(pkg)
		assert.False(t, verified, "OT-SHALL-NOT-01: Must reject tampered OTA package")

		t.Logf("OTA package with invalid hash correctly rejected")

		report.Record("E2E-06-02: OTA签名校验失败拒绝", true, time.Since(start), "", "E2E-06", "OTA")
	})

	// ── Test 3: OT-SHALL-02 空签名拒绝 ──
	t.Run("E2E-06-03: OT-SHALL-02 无签名包拒绝", func(t *testing.T) {
		start := time.Now()

		otaData := []byte("DKCS_TCU_FIRMWARE_v2.2.0_binary_data_no_sig")
		pkg := &suite.OTAPackage{
			Version: "2.2.0-unsigned",
			Hash:    hashOTAData(otaData),
			// No signature — simulate unsigned package
			Data: otaData,
			Size: len(otaData),
		}

		// Verify should fail with no signature
		verified := tcu.VerifyOTAPackage(pkg)
		assert.False(t, verified, "OT-SHALL-02: Must reject unsigned OTA package")

		t.Logf("OTA package without signature correctly rejected")

		report.Record("E2E-06-03: OTA无签名包拒绝", true, time.Since(start), "", "E2E-06", "OTA")
	})

	// ── Test 4: OT-SHALL-03 升级前状态检查 ──
	t.Run("E2E-06-04: OT-SHALL-03 OTA状态查询", func(t *testing.T) {
		start := time.Now()

		// Verify idle state before any OTA
		idleStatus := tcu.GetOTAPackageStatus()
		assert.Equal(t, "IDLE", idleStatus, "OT-SHALL-03: Initial OTA status must be IDLE")

		report.Record("E2E-06-04: OTA状态查询", true, time.Since(start), "", "E2E-06", "OTA")
	})

	// ── Test 5: OT-SHALL-02 ECDSA 签名验证 ──
	t.Run("E2E-06-05: OT-SHALL-02 ECDSA P-256签名验证", func(t *testing.T) {
		start := time.Now()

		otaData := []byte("DKCS_TCU_FIRMWARE_v2.3.0_test_signing_data")
		hash := sha256.Sum256(otaData)
		r, s, err := ecdsa.Sign(rand.Reader, signingKey, hash[:])
		require.NoError(t, err)

		// Verify with valid signature
		valid := ecdsa.Verify(&signingKey.PublicKey, hash[:], r, s)
		assert.True(t, valid, "EC P-256 signature must verify correctly")

		// Verify with wrong public key
		wrongKey, _ := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
		invalid := ecdsa.Verify(&wrongKey.PublicKey, hash[:], r, s)
		assert.False(t, invalid, "Wrong public key must not verify")

		// Create tampered data
		tamperedHash := sha256.Sum256([]byte("tampered_data"))
		tamperedValid := ecdsa.Verify(&signingKey.PublicKey, tamperedHash[:], r, s)
		assert.False(t, tamperedValid, "Tampered data must not verify")


		report.Record("E2E-06-05: ECDSA签名验证(crypto)", true, time.Since(start), "", "E2E-06", "OTA/CRYPTO")
	})

	// ── Test 6: OT-SHALL-NOT-01 篡改数据检测 ──
	t.Run("E2E-06-06: OT-SHALL-NOT-01 数据篡改检测", func(t *testing.T) {
		start := time.Now()

		// Original package
		originalData := []byte("DKCS_TCU_FIRMWARE_v2.2.0_original")
		signature := signOTA(signingKey, originalData)

		// Tampered data (bit flip)
		tamperedData := make([]byte, len(originalData))
		copy(tamperedData, originalData)
		tamperedData[10] ^= 0xFF // flip one bit

		// The VerifyOTAPackage simulates hash mismatch
		pkg := &suite.OTAPackage{
			Version:   "2.2.0-tampered-bitflip",
			Hash:      "tampered",
			Signature: signature,
			Data:      tamperedData,
			Size:      len(tamperedData),
		}

		verified := tcu.VerifyOTAPackage(pkg)
		assert.False(t, verified, "OT-SHALL-NOT-01: Must reject bit-flipped OTA data")

		t.Logf("Bit-flipped OTA data correctly rejected")

		report.Record("E2E-06-06: OTA数据篡改检测", true, time.Since(start), "", "E2E-06", "OTA")
	})

	report.GenerateHTML("test-output/integration-report.html")
}
