// Package service — 安全启动链测试
//
// 需求覆盖:
//   KSS-SHALL-08: 系统 SHALL 支持安全启动链：
//   Boot ROM → BootLoader(SE050验签) → TFM → Application 逐级校验
//
// 测试方法:
//   使用 Go crypto 模拟嵌入式安全启动链的逐级签名验证逻辑。
//   每一级固件由上一级或 OEM Root Key 签名，SE050（模拟）验证签名后才加载。

package service

import (
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rand"
	"crypto/sha256"
	"math/big"
	"testing"
)

// bootStage represents a stage in the secure boot chain.
type bootStage int

const (
	stageBootRom    bootStage = 0
	stageBootLoader bootStage = 1
	stageTFM        bootStage = 2
	stageApplication bootStage = 3
)

var stageNames = map[bootStage]string{
	stageBootRom:     "Boot ROM",
	stageBootLoader:  "BootLoader",
	stageTFM:         "TFM (Trusted Firmware-M)",
	stageApplication: "Application",
}

// firmwareImage represents a signed firmware image at a boot stage.
type firmwareImage struct {
	Stage     bootStage
	ImageHash []byte
	Signature []byte // ECDSA P-256 signature
	PublicKey *ecdsa.PublicKey // signer's public key for verification
}

// secureBootChain simulates the TCU secure boot chain:
// Boot ROM → BootLoader (SE050 verifies) → TFM → Application
type secureBootChain struct {
	// OEM Root Key — the hardware-rooted trust anchor (burned into SE050/OTP)
	oemRootKey *ecdsa.PrivateKey

	// Stage keys — each stage's private key for signing the next stage
	bootLoaderKey  *ecdsa.PrivateKey // signed by OEM Root Key
	tfmKey         *ecdsa.PrivateKey // signed by BootLoader
	appKey         *ecdsa.PrivateKey // signed by TFM

	// SE050 verification function (simulated hardware security module)
	verifyFn func(image *firmwareImage, trustedPubKey *ecdsa.PublicKey) bool
}

// newSecureBootChain creates a simulated secure boot chain.
func newSecureBootChain() (*secureBootChain, error) {
	oemRoot, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	if err != nil {
		return nil, err
	}
	blKey, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	if err != nil {
		return nil, err
	}
	tfmKey, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	if err != nil {
		return nil, err
	}
	appKey, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	if err != nil {
		return nil, err
	}

	return &secureBootChain{
		oemRootKey:    oemRoot,
		bootLoaderKey: blKey,
		tfmKey:        tfmKey,
		appKey:        appKey,
		verifyFn:      defaultVerify,
	}, nil
}

// defaultVerify is the default SE050 signature verification function.
func defaultVerify(image *firmwareImage, trustedPubKey *ecdsa.PublicKey) bool {
	if image == nil || trustedPubKey == nil {
		return false
	}
	if len(image.Signature) < 64 {
		return false
	}
	r := new(big.Int).SetBytes(image.Signature[:len(image.Signature)/2])
	s := new(big.Int).SetBytes(image.Signature[len(image.Signature)/2:])
	return ecdsa.Verify(trustedPubKey, image.ImageHash, r, s)
}

// signImage signs a firmware image with the given private key.
func signImage(stage bootStage, data []byte, key *ecdsa.PrivateKey) *firmwareImage {
	hash := sha256.Sum256(data)
	r, s, _ := ecdsa.Sign(rand.Reader, key, hash[:])
	sig := append(r.Bytes(), s.Bytes()...)
	return &firmwareImage{
		Stage:     stage,
		ImageHash: hash[:],
		Signature: sig,
		PublicKey: &key.PublicKey,
	}
}

// verifyBootChain runs the full secure boot chain verification.
// Each stage's firmware image is signed by the PREVIOUS stage's private key
// and verified using that stage's public key stored in hardware.
//
// Chain: Boot ROM (OEM Root Key burned in OTP)
//   → BootLoader (signed by OEM Root, verified by Boot ROM using OEM Root Pub)
//   → TFM (signed by BootLoader, verified by BootLoader using its own Pub)
//   → Application (signed by TFM, verified by TFM using its own Pub)
// Returns (success, failedStage).
func (bc *secureBootChain) verifyBootChain() (bool, bootStage) {
	// Stage 1: BootLoader image is signed by the OEM Root Key
	// Boot ROM verifies BootLoader using the OEM Root Public Key (burned into hardware)
	bootLoaderFirmware := []byte("DKCS_BOOTLOADER_v2.1_SE050_SIGNED")
	// The BootLoader image is signed by the OEM (root key holder)
	blImage := signImage(stageBootLoader, bootLoaderFirmware, bc.oemRootKey)
	// Boot ROM verifies the signature using the OEM Root Public Key
	if !bc.verifyFn(blImage, &bc.oemRootKey.PublicKey) {
		return false, stageBootLoader
	}

	// Stage 2: TFM image is signed by the BootLoader's private key
	// BootLoader verifies TFM using its own public key
	tfmFirmware := []byte("DKCS_TFM_v2.1_TRUSTED_FIRMWARE")
	tfmImage := signImage(stageTFM, tfmFirmware, bc.bootLoaderKey)
	if !bc.verifyFn(tfmImage, &bc.bootLoaderKey.PublicKey) {
		return false, stageTFM
	}

	// Stage 3: Application image is signed by the TFM's private key
	// TFM verifies Application using its own public key
	appFirmware := []byte("DKCS_APPLICATION_v2.1_DIGITAL_KEY_CORE")
	appImage := signImage(stageApplication, appFirmware, bc.tfmKey)
	if !bc.verifyFn(appImage, &bc.tfmKey.PublicKey) {
		return false, stageApplication
	}

	return true, -1
}

// TestKSSShall08_SecureBootChain verifies KSS-SHALL-08:
// Secure boot chain from Boot ROM → BootLoader → TFM → Application.
func TestKSSShall08_SecureBootChain(t *testing.T) {
	t.Log("KSS-SHALL-08: 安全启动链 — Boot ROM → BootLoader(SE050验签) → TFM → Application")

	bc, err := newSecureBootChain()
	if err != nil {
		t.Fatalf("Failed to create secure boot chain: %v", err)
	}

	// Full chain verification
	success, failedStage := bc.verifyBootChain()
	if !success {
		t.Errorf("Secure boot chain failed at stage %s", stageNames[failedStage])
	}
	t.Logf("PASS: 完整安全启动链验证通过 (Boot ROM → BootLoader → TFM → Application)")
}

// TestKSSShall08_BootLoaderSignatureFail verifies boot chain halts
// when BootLoader signature is invalid.
func TestKSSShall08_BootLoaderSignatureFail(t *testing.T) {
	t.Log("KSS-SHALL-08: 场景 S-11 — BootLoader 签名校验失败，启动终止")

	bc, err := newSecureBootChain()
	if err != nil {
		t.Fatalf("Failed to create secure boot chain: %v", err)
	}

	// BootLoader image should be signed by OEM Root Key.
	// Tamper: sign with a random key (not OEM Root)
	wrongKey, _ := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	tamperedBLData := []byte("DKCS_BOOTLOADER_v2.1_TAMPERED_SIGNATURE")
	// Sign with wrong key — breaks chain
	blImage := signImage(stageBootLoader, tamperedBLData, wrongKey)

	// Boot ROM verifies BootLoader with OEM Root Key — should fail
	verified := bc.verifyFn(blImage, &bc.oemRootKey.PublicKey)
	if verified {
		t.Error("BootLoader with invalid signature must NOT be verified by OEM Root Key")
	}
	t.Log("PASS: BootLoader 篡改签名校验正确拒绝，启动终止")
}

// TestKSSShall08_TFMSignatureFail verifies boot chain halts
// when TFM signature is invalid.
func TestKSSShall08_TFMSignatureFail(t *testing.T) {
	t.Log("KSS-SHALL-08: TFM 签名校验失败，启动终止")

	bc, err := newSecureBootChain()
	if err != nil {
		t.Fatalf("Failed to create secure boot chain: %v", err)
	}

	// Stage 1: BootLoader verification passes (valid)
	// BootLoader is signed by OEM Root Key
	blData := []byte("DKCS_BOOTLOADER_v2.1_VALID")
	blImage := signImage(stageBootLoader, blData, bc.oemRootKey)
	blVerified := bc.verifyFn(blImage, &bc.oemRootKey.PublicKey)
	if !blVerified {
		t.Fatal("BootLoader verification should pass at this point")
	}
	t.Log("Stage 1: Boot ROM → BootLoader √")

	// Stage 2: TFM verification with wrong key — should fail
	wrongTfmKey, _ := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	tamperedTFMData := []byte("DKCS_TFM_v2.1_WRONG_SIGNER")
	tfmImage := signImage(stageTFM, tamperedTFMData, wrongTfmKey)
	tfmVerified := bc.verifyFn(tfmImage, &bc.bootLoaderKey.PublicKey)
	if tfmVerified {
		t.Error("TFM with invalid signer must NOT be verified by BootLoader key")
	}
	t.Log("Stage 2: BootLoader → TFM ✗ (签名校验失败，启动终止)")
	t.Log("PASS: TFM 篡改签名校验正确拒绝")
}

// TestKSSShall08_AppSignatureFail verifies boot chain halts
// when Application signature is invalid.
func TestKSSShall08_AppSignatureFail(t *testing.T) {
	t.Log("KSS-SHALL-08: Application 签名校验失败，启动终止")

	bc, err := newSecureBootChain()
	if err != nil {
		t.Fatalf("Failed to create secure boot chain: %v", err)
	}

	// Stage 1: BootLoader ✓ (signed by OEM Root)
	blData := []byte("DKCS_BOOTLOADER_v2.1")
	blImage := signImage(stageBootLoader, blData, bc.oemRootKey)
	if !bc.verifyFn(blImage, &bc.oemRootKey.PublicKey) {
		t.Fatal("Stage 1 should pass")
	}
	t.Log("Stage 1: Boot ROM → BootLoader √")

	// Stage 2: TFM ✓ (signed by BootLoader)
	tfmData := []byte("DKCS_TFM_v2.1")
	tfmImage := signImage(stageTFM, tfmData, bc.bootLoaderKey)
	if !bc.verifyFn(tfmImage, &bc.bootLoaderKey.PublicKey) {
		t.Fatal("Stage 2 should pass")
	}
	t.Log("Stage 2: BootLoader → TFM √")

	// Stage 3: Application ✗ — unsigned
	unsignedAppData := []byte("DKCS_APP_v2.1_UNSIGNED")
	appHash := sha256.Sum256(unsignedAppData)
	unsignedApp := &firmwareImage{
		Stage:     stageApplication,
		ImageHash: appHash[:],
		Signature: nil, // no signature
		PublicKey: nil,
	}
	appVerified := bc.verifyFn(unsignedApp, &bc.tfmKey.PublicKey)
	if appVerified {
		t.Error("Unsigned Application must NOT be verified by TFM key")
	}
	t.Log("Stage 3: TFM → Application ✗ (无签名，启动终止)")
	t.Log("PASS: 无签名 Application 正确拒绝")
}

// TestKSSShall08_ChainIntegrity verifies the complete chain:
// any single stage failure prevents the entire boot.
func TestKSSShall08_ChainIntegrity(t *testing.T) {
	t.Log("KSS-SHALL-08: 逐级校验 — 任何一级签名失败都阻止启动")

	bc, err := newSecureBootChain()
	if err != nil {
		t.Fatalf("Failed to create secure boot chain: %v", err)
	}

	tests := []struct {
		name         string
		tamperStage  bootStage // which stage to tamper with (-1 = none)
		expectBoot   bool
	}{
		{"全部签名有效", -1, true},
		{"BootLoader签名篡改", stageBootLoader, false},
		{"TFM签名篡改", stageTFM, false},
		{"Application签名篡改", stageApplication, false},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			// Create tampered firmware data at the specified stage
			// Create firmware data for each stage
			blData := []byte("DKCS_BOOTLOADER_v2.1")
			tfmData := []byte("DKCS_TFM_v2.1")
			appData := []byte("DKCS_APP_v2.1")

			// Select the signing keys based on tamper target.
			// Normal chain: OEM Root signs BL → BL signs TFM → TFM signs App
			blSigner := bc.oemRootKey    // normal: OEM Root signs BootLoader
			tfmSigner := bc.bootLoaderKey // normal: BootLoader signs TFM
			appSigner := bc.tfmKey        // normal: TFM signs Application

			// Tamper with the specified stage's signer
			switch tt.tamperStage {
			case stageBootLoader:
				wrongKey, _ := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
				blSigner = wrongKey // BootLoader signed by wrong key
			case stageTFM:
				wrongKey, _ := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
				tfmSigner = wrongKey // TFM signed by wrong key
			case stageApplication:
				wrongKey, _ := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
				appSigner = wrongKey // App signed by wrong key
			}

			// Stage 1: Boot ROM verifies BootLoader (verifier: OEM Root pub key)
			blImage := signImage(stageBootLoader, blData, blSigner)
			blOk := bc.verifyFn(blImage, &bc.oemRootKey.PublicKey)
			if !blOk {
				if tt.expectBoot {
					t.Error("BootLoader verification failed but was expected to pass")
				}
				return // chain halts
			}

			// Stage 2: BootLoader verifies TFM (verifier: BootLoader pub key)
			tfmImage := signImage(stageTFM, tfmData, tfmSigner)
			tfmOk := bc.verifyFn(tfmImage, &bc.bootLoaderKey.PublicKey)
			if !tfmOk {
				if tt.expectBoot {
					t.Error("TFM verification failed but was expected to pass")
				}
				return
			}

			// Stage 3: TFM verifies Application (verifier: TFM pub key)
			appImage := signImage(stageApplication, appData, appSigner)
			appOk := bc.verifyFn(appImage, &bc.tfmKey.PublicKey)
			if !appOk && tt.expectBoot {
				t.Error("Application verification failed but was expected to pass")
			}
		})
	}
}

// TestKSSShall08_Se050Verification verifies SE050-level verification.
func TestKSSShall08_Se050Verification(t *testing.T) {
	t.Log("KSS-SHALL-08: SE050 验签函数 — defaultVerify 边界条件")

	bc, err := newSecureBootChain()
	if err != nil {
		t.Fatalf("Failed to create secure boot chain: %v", err)
	}

	// Test boundary conditions for the verify function
	t.Run("nil image rejected", func(t *testing.T) {
		if bc.verifyFn(nil, &bc.oemRootKey.PublicKey) {
			t.Error("nil image must be rejected")
		}
	})

	t.Run("nil key rejected", func(t *testing.T) {
		blData := []byte("bootloader")
		blImage := signImage(stageBootLoader, blData, bc.bootLoaderKey)
		if bc.verifyFn(blImage, nil) {
			t.Error("nil public key must be rejected")
		}
	})

	t.Run("short signature rejected", func(t *testing.T) {
		blImage := &firmwareImage{
			Stage:     stageBootLoader,
			ImageHash: make([]byte, 32),
			Signature: []byte("short"), // less than 64 bytes
		}
		if bc.verifyFn(blImage, &bc.oemRootKey.PublicKey) {
			t.Error("short signature must be rejected")
		}
	})
}
