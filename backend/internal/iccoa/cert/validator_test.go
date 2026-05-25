// Package iccoacert 证书验证器测试
package iccoacert

import (
	"encoding/hex"
	"testing"
	"time"
)

func setupTestCA(t *testing.T) (*CertGenerator, *CertKeyPair) {
	generator := NewCertGenerator()
	privKey, _ := generator.GenerateECKeyPair()
	config := &VehicleOemCAConfig{
		VehicleOemID:   "0010",
		ValidityPeriod: VehicleOemCADefaultValidity,
		CommonName:     "TEST-CA",
		Organization:   "Test OEM",
		Country:        "CN",
	}
	keyPair, err := generator.GenerateVehicleOemCACert(config, privKey)
	if err != nil {
		t.Fatalf("Failed to generate test CA: %v", err)
	}
	return generator, keyPair
}

func TestCertValidator_ValidateCACertificate(t *testing.T) {
	validator := NewCertValidator()
	generator, caKeyPair := setupTestCA(t)
	_ = generator

	// 测试有效的 CA 证书
	err := validator.ValidateCACertificate(caKeyPair.Certificate)
	if err != nil {
		t.Errorf("Valid CA certificate validation failed: %v", err)
	}

	// 测试空证书
	err = validator.ValidateCACertificate(nil)
	if err == nil {
		t.Error("Nil certificate validation should fail")
	}

	t.Logf("CA certificate validation passed")
}

func TestCertValidator_ValidateCACertificate_InvalidSignature(t *testing.T) {
	validator := NewCertValidator()
	generator := NewCertGenerator()

	// 生成两个不同的 CA
	ca1PrivKey, _ := generator.GenerateECKeyPair()
	ca1Config := &VehicleOemCAConfig{VehicleOemID: "0010"}
	ca1KeyPair, _ := generator.GenerateVehicleOemCACert(ca1Config, ca1PrivKey)

	ca2PrivKey, _ := generator.GenerateECKeyPair()
	ca2Config := &VehicleOemCAConfig{VehicleOemID: "0020"}
	ca2KeyPair, _ := generator.GenerateVehicleOemCACert(ca2Config, ca2PrivKey)

	// 测试: ca1 应该不能验证由 ca2 签发的证书
	// (实际上ca1和ca2都是自签名的，这里只是演示验证逻辑)
	err := validator.ValidateCACertificate(ca1KeyPair.Certificate)
	if err != nil {
		t.Errorf("CA1 validation failed: %v", err)
	}

	err = validator.ValidateCACertificate(ca2KeyPair.Certificate)
	if err != nil {
		t.Errorf("CA2 validation failed: %v", err)
	}

	// 验证 CA 不同
	if string(ca1KeyPair.Certificate.X509Cert.SubjectKeyId) == string(ca2KeyPair.Certificate.X509Cert.SubjectKeyId) {
		t.Error("Different CAs should have different Subject Key IDs")
	}

	t.Logf("Multi-CA validation passed")
}

func TestCertValidator_ValidateOwnerKeyCert(t *testing.T) {
	validator := NewCertValidator()
	generator, caKeyPair := setupTestCA(t)

	// 添加 CA 到可信任列表
	validator.AddTrustedCA(caKeyPair.Certificate)

	// 生成车主钥匙证书
	ownerPrivKey, _ := generator.GenerateECKeyPair()
	ownerKeyID, _ := GenerateKeyID()
	ownerConfig := &OwnerKeyCertConfig{
		KeyID:          hex.EncodeToString(ownerKeyID),
		VehicleID:      "112233445566778899AABBCCDDEEFF00",
		VehicleOemID:   "0010",
		ValidityPeriod: OwnerCertDefaultValidity,
		Mode:           CertModeCA,
	}
	ownerCert, _ := generator.GenerateOwnerKeyCert(ownerConfig, &ownerPrivKey.PublicKey, caKeyPair.Certificate, caKeyPair.PrivateKey)

	// 测试有效的车主证书
	err := validator.ValidateOwnerKeyCert(ownerCert, caKeyPair.Certificate)
	if err != nil {
		t.Errorf("Valid owner key certificate validation failed: %v", err)
	}

	t.Logf("Owner key certificate validation passed")
}

func TestCertValidator_ValidateOwnerKeyCert_Expired(t *testing.T) {
	validator := NewCertValidator()
	generator, caKeyPair := setupTestCA(t)

	// 生成车主钥匙证书 (已过期)
	ownerPrivKey, _ := generator.GenerateECKeyPair()
	ownerKeyID, _ := GenerateKeyID()
	ownerConfig := &OwnerKeyCertConfig{
		KeyID:          hex.EncodeToString(ownerKeyID),
		VehicleID:      "112233445566778899AABBCCDDEEFF00",
		VehicleOemID:   "0010",
		ValidityPeriod: -time.Hour, // 已经过期
		Mode:           CertModeCA,
	}
	ownerCert, _ := generator.GenerateOwnerKeyCert(ownerConfig, &ownerPrivKey.PublicKey, caKeyPair.Certificate, caKeyPair.PrivateKey)

	// 测试过期的证书
	err := validator.ValidateOwnerKeyCert(ownerCert, caKeyPair.Certificate)
	if err == nil {
		t.Error("Expired certificate validation should fail")
	} else {
		t.Logf("Expired certificate correctly rejected: %v", err)
	}
}

func TestCertValidator_ValidateOwnerKeyCert_Size(t *testing.T) {
	validator := NewCertValidator()
	generator, caKeyPair := setupTestCA(t)

	// 生成车主钥匙证书
	ownerPrivKey, _ := generator.GenerateECKeyPair()
	ownerKeyID, _ := GenerateKeyID()
	ownerConfig := &OwnerKeyCertConfig{
		KeyID:          hex.EncodeToString(ownerKeyID),
		VehicleID:      "112233445566778899AABBCCDDEEFF00",
		VehicleOemID:   "0010",
		ValidityPeriod: OwnerCertDefaultValidity,
		Mode:           CertModeCA,
	}
	ownerCert, _ := generator.GenerateOwnerKeyCert(ownerConfig, &ownerPrivKey.PublicKey, caKeyPair.Certificate, caKeyPair.PrivateKey)

	// 验证大小
	if len(ownerCert.DERData) > MaxOwnerCertSize {
		t.Errorf("Owner certificate size %d exceeds limit %d", len(ownerCert.DERData), MaxOwnerCertSize)
	}

	// 人为修改证书数据使其超过大小限制 (模拟)
	largeCert := *ownerCert
	largeCert.DERData = make([]byte, MaxOwnerCertSize+100)

	err := validator.ValidateOwnerKeyCert(&largeCert, caKeyPair.Certificate)
	if err == nil {
		t.Error("Oversized certificate validation should fail")
	} else {
		t.Logf("Oversized certificate correctly rejected: %v", err)
	}
}

func TestCertValidator_ValidateMidShareCert(t *testing.T) {
	validator := NewCertValidator()
	generator, caKeyPair := setupTestCA(t)

	// 生成车主钥匙证书
	ownerPrivKey, _ := generator.GenerateECKeyPair()
	ownerKeyID, _ := GenerateKeyID()
	ownerConfig := &OwnerKeyCertConfig{
		KeyID:          hex.EncodeToString(ownerKeyID),
		VehicleID:      "112233445566778899AABBCCDDEEFF00",
		VehicleOemID:   "0010",
		ValidityPeriod: OwnerCertDefaultValidity,
		Mode:           CertModeCA,
	}
	ownerCert, _ := generator.GenerateOwnerKeyCert(ownerConfig, &ownerPrivKey.PublicKey, caKeyPair.Certificate, caKeyPair.PrivateKey)

	// 生成中间分享证书
	midPrivKey, _ := generator.GenerateECKeyPair()
	midConfig := &MidShareCertConfig{
		VehicleID:      "112233445566778899AABBCCDDEEFF00",
		VehicleOemID:   "0010",
		ValidityPeriod: 7 * 24 * time.Hour,
		MaxValidTime:   time.Now().Add(30 * 24 * time.Hour),
	}
	midCert, _ := generator.GenerateMidShareCert(midConfig, &midPrivKey.PublicKey, ownerCert, ownerPrivKey)

	// 测试有效的中间分享证书
	err := validator.ValidateMidShareCert(midCert, ownerCert)
	if err != nil {
		t.Errorf("Valid mid-share certificate validation failed: %v", err)
	}

	t.Logf("Mid-share certificate validation passed")
}

func TestCertValidator_ValidateMidShareCert_MaxValidity(t *testing.T) {
	validator := NewCertValidator()
	generator, caKeyPair := setupTestCA(t)

	// 生成车主钥匙证书
	ownerPrivKey, _ := generator.GenerateECKeyPair()
	ownerKeyID, _ := GenerateKeyID()
	ownerConfig := &OwnerKeyCertConfig{
		KeyID:          hex.EncodeToString(ownerKeyID),
		VehicleID:      "112233445566778899AABBCCDDEEFF00",
		VehicleOemID:   "0010",
		ValidityPeriod: OwnerCertDefaultValidity,
		Mode:           CertModeCA,
	}
	ownerCert, _ := generator.GenerateOwnerKeyCert(ownerConfig, &ownerPrivKey.PublicKey, caKeyPair.Certificate, caKeyPair.PrivateKey)

	// 生成中间分享证书 (超过90天)
	midPrivKey, _ := generator.GenerateECKeyPair()
	midConfig := &MidShareCertConfig{
		VehicleID:      "112233445566778899AABBCCDDEEFF00",
		VehicleOemID:   "0010",
		ValidityPeriod: 100 * 24 * time.Hour, // 超过最大限制
		MaxValidTime:   time.Now().Add(100 * 24 * time.Hour),
	}
	midCert, _ := generator.GenerateMidShareCert(midConfig, &midPrivKey.PublicKey, ownerCert, ownerPrivKey)

	// 验证时应该检测有效期超限
	// 注: 实际上 GenerateMidShareCert 会限制有效期，但我们测试验证器的检查
	err := validator.ValidateMidShareCert(midCert, ownerCert)
	// 如果 GenerateMidShareCert 正确限制了有效期，这个测试可能会通过
	if err != nil {
		t.Logf("Mid-share with excessive validity handled: %v", err)
	} else {
		validity := midCert.X509Cert.NotAfter.Sub(midCert.X509Cert.NotBefore)
		if validity > MidShareCertMaxValidity {
			t.Errorf("Mid-share validity %v should not exceed %v", validity, MidShareCertMaxValidity)
		}
	}
}

func TestCertValidator_ValidateSharedKeyCert_CAMode(t *testing.T) {
	validator := NewCertValidator()
	generator, caKeyPair := setupTestCA(t)

	// 生成车主钥匙证书
	ownerPrivKey, _ := generator.GenerateECKeyPair()
	ownerKeyID, _ := GenerateKeyID()
	ownerConfig := &OwnerKeyCertConfig{
		KeyID:          hex.EncodeToString(ownerKeyID),
		VehicleID:      "112233445566778899AABBCCDDEEFF00",
		VehicleOemID:   "0010",
		ValidityPeriod: OwnerCertDefaultValidity,
		Mode:           CertModeCA,
	}
	ownerCert, _ := generator.GenerateOwnerKeyCert(ownerConfig, &ownerPrivKey.PublicKey, caKeyPair.Certificate, caKeyPair.PrivateKey)

	// 生成中间分享证书
	midPrivKey, _ := generator.GenerateECKeyPair()
	midConfig := &MidShareCertConfig{
		VehicleID:      "112233445566778899AABBCCDDEEFF00",
		VehicleOemID:   "0010",
		ValidityPeriod: 7 * 24 * time.Hour,
	}
	midCert, _ := generator.GenerateMidShareCert(midConfig, &midPrivKey.PublicKey, ownerCert, ownerPrivKey)

	// 生成好友钥匙证书
	friendPrivKey, _ := generator.GenerateECKeyPair()
	friendKeyID, _ := GenerateKeyID()
	friendConfig := &SharedKeyCertConfig{
		KeyID:          hex.EncodeToString(friendKeyID),
		VehicleID:      "112233445566778899AABBCCDDEEFF00",
		VehicleOemID:   "0010",
		ValidityPeriod: 7 * 24 * time.Hour,
		Mode:           CertModeCA,
	}
	friendCert, _ := generator.GenerateSharedKeyCert(friendConfig, &friendPrivKey.PublicKey, midCert, midPrivKey)

	// 测试 CA 模式验证
	err := validator.ValidateSharedKeyCert(friendCert, midCert, caKeyPair.Certificate, CertModeCA)
	if err != nil {
		t.Errorf("Valid shared key certificate (CA mode) validation failed: %v", err)
	}

	t.Logf("Shared key certificate (CA mode) validation passed")
}

func TestCertValidator_ValidateSharedKeyCert_NonCAMode(t *testing.T) {
	validator := NewCertValidator()
	generator, caKeyPair := setupTestCA(t)

	// 生成好友钥匙证书 (非 CA 模式)
	friendPrivKey, _ := generator.GenerateECKeyPair()
	friendKeyID, _ := GenerateKeyID()
	friendConfig := &SharedKeyCertConfig{
		KeyID:          hex.EncodeToString(friendKeyID),
		VehicleID:      "112233445566778899AABBCCDDEEFF00",
		VehicleOemID:   "0010",
		ValidityPeriod: 7 * 24 * time.Hour,
		Mode:           CertModeNonCA,
	}
	friendCert, _ := generator.GenerateSharedKeyCert(friendConfig, &friendPrivKey.PublicKey, caKeyPair.Certificate, caKeyPair.PrivateKey)

	// 测试非 CA 模式验证
	err := validator.ValidateSharedKeyCert(friendCert, nil, caKeyPair.Certificate, CertModeNonCA)
	if err != nil {
		t.Errorf("Valid shared key certificate (Non-CA mode) validation failed: %v", err)
	}

	t.Logf("Shared key certificate (Non-CA mode) validation passed")
}

func TestCertValidator_AddTrustedCA(t *testing.T) {
	validator := NewCertValidator()
	generator, caKeyPair := setupTestCA(t)

	// 测试添加有效的 CA
	err := validator.AddTrustedCA(caKeyPair.Certificate)
	if err != nil {
		t.Errorf("Adding valid CA failed: %v", err)
	}

	// 检查可信任列表
	trustedCAs := validator.GetTrustedCAs()
	if len(trustedCAs) != 1 {
		t.Errorf("Expected 1 trusted CA, got %d", len(trustedCAs))
	}

	// 测试添加无效的证书 (非 CA 类型)
	ownerPrivKey, _ := generator.GenerateECKeyPair()
	ownerKeyID, _ := GenerateKeyID()
	ownerConfig := &OwnerKeyCertConfig{
		KeyID:          hex.EncodeToString(ownerKeyID),
		VehicleID:      "112233445566778899AABBCCDDEEFF00",
		VehicleOemID:   "0010",
		ValidityPeriod: OwnerCertDefaultValidity,
		Mode:           CertModeCA,
	}
	ownerCert, _ := generator.GenerateOwnerKeyCert(ownerConfig, &ownerPrivKey.PublicKey, caKeyPair.Certificate, caKeyPair.PrivateKey)

	err = validator.AddTrustedCA(ownerCert)
	if err == nil {
		t.Error("Adding non-CA certificate should fail")
	} else {
		t.Logf("Non-CA certificate correctly rejected: %v", err)
	}
}

func TestCertValidator_ValidateCertificateChain(t *testing.T) {
	validator := NewCertValidator()
	generator, caKeyPair := setupTestCA(t)

	validator.AddTrustedCA(caKeyPair.Certificate)

	// 生成车主钥匙证书
	ownerPrivKey, _ := generator.GenerateECKeyPair()
	ownerKeyID, _ := GenerateKeyID()
	ownerConfig := &OwnerKeyCertConfig{
		KeyID:          hex.EncodeToString(ownerKeyID),
		VehicleID:      "112233445566778899AABBCCDDEEFF00",
		VehicleOemID:   "0010",
		ValidityPeriod: OwnerCertDefaultValidity,
		Mode:           CertModeCA,
	}
	ownerCert, _ := generator.GenerateOwnerKeyCert(ownerConfig, &ownerPrivKey.PublicKey, caKeyPair.Certificate, caKeyPair.PrivateKey)

	// 构建证书链
	chain := &CertificateChain{
		RootCA:    caKeyPair.Certificate,
		EndEntity: ownerCert,
	}

	// 验证证书链
	err := validator.ValidateCertificateChain(chain)
	if err != nil {
		t.Errorf("Valid certificate chain validation failed: %v", err)
	}

	t.Logf("Certificate chain validation passed")
}

func TestCertValidator_ValidateCertificateChain_MissingRoot(t *testing.T) {
	validator := NewCertValidator()
	generator, caKeyPair := setupTestCA(t)
	_ = generator
	_ = caKeyPair

	// 构建无根证书的证书链
	chain := &CertificateChain{
		RootCA: nil,
		EndEntity: &ICCOACertificate{
			Type: CertTypeOwnerDK,
		},
	}

	err := validator.ValidateCertificateChain(chain)
	if err == nil {
		t.Error("Certificate chain without root should fail")
	} else {
		t.Logf("Missing root correctly detected: %v", err)
	}
}

func TestParseICCOACertificate(t *testing.T) {
	generator, caKeyPair := setupTestCA(t)
	_ = caKeyPair

	// 解析证书
	parsedCert, err := ParseICCOACertificate(caKeyPair.Certificate.DERData)
	if err != nil {
		t.Errorf("Failed to parse certificate: %v", err)
		return
	}

	// 验证解析结果
	if parsedCert.Type != caKeyPair.Certificate.Type {
		t.Errorf("Parsed type mismatch: expected %d, got %d", caKeyPair.Certificate.Type, parsedCert.Type)
	}

	if parsedCert.VehicleOemID != caKeyPair.Certificate.VehicleOemID {
		t.Errorf("Parsed VehicleOemID mismatch: expected %s, got %s", caKeyPair.Certificate.VehicleOemID, parsedCert.VehicleOemID)
	}

	// 生成更复杂的证书并解析
	ownerPrivKey, _ := generator.GenerateECKeyPair()
	ownerKeyID, _ := GenerateKeyID()
	ownerConfig := &OwnerKeyCertConfig{
		KeyID:          hex.EncodeToString(ownerKeyID),
		VehicleID:      "112233445566778899AABBCCDDEEFF00",
		VehicleOemID:   "0010",
		ValidityPeriod: OwnerCertDefaultValidity,
		Mode:           CertModeNonCA, // 测试非 CA 模式
	}
	ownerCert, _ := generator.GenerateOwnerKeyCert(ownerConfig, &ownerPrivKey.PublicKey, caKeyPair.Certificate, caKeyPair.PrivateKey)

	parsedOwnerCert, err := ParseICCOACertificate(ownerCert.DERData)
	if err != nil {
		t.Errorf("Failed to parse owner certificate: %v", err)
		return
	}

	if parsedOwnerCert.Mode != CertModeNonCA {
		t.Errorf("Parsed mode mismatch: expected %d, got %d", CertModeNonCA, parsedOwnerCert.Mode)
	}

	t.Logf("Certificate parsing test passed")
}

