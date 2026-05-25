// Package iccoacert 证书生成器测试
package iccoacert

import (
	"crypto/x509"
	"encoding/hex"
	"testing"
	"time"
)

func TestCertGenerator_GenerateVehicleOemCACert(t *testing.T) {
	generator := NewCertGenerator()

	// 生成密钥对
	privKey, err := generator.GenerateECKeyPair()
	if err != nil {
		t.Fatalf("Failed to generate key pair: %v", err)
	}

	config := &VehicleOemCAConfig{
		VehicleOemID:   "0010",
		ValidityPeriod: VehicleOemCADefaultValidity,
		CommonName:     "TEST-CA",
		Organization:   "Test OEM",
		Country:        "CN",
	}

	keyPair, err := generator.GenerateVehicleOemCACert(config, privKey)
	if err != nil {
		t.Fatalf("Failed to generate CA certificate: %v", err)
	}

	// 验证证书
	if keyPair.Certificate == nil {
		t.Fatal("Certificate is nil")
	}

	if keyPair.PrivateKey == nil {
		t.Fatal("Private key is nil")
	}

	// 验证证书属性
	if !keyPair.Certificate.X509Cert.IsCA {
		t.Error("CA certificate should have IsCA=true")
	}

	if keyPair.Certificate.X509Cert.MaxPathLen != 2 {
		t.Errorf("Expected MaxPathLen=2, got %d", keyPair.Certificate.X509Cert.MaxPathLen)
	}

	if keyPair.Certificate.Type != CertTypeVehicleOemCA {
		t.Errorf("Expected type CertTypeVehicleOemCA, got %d", keyPair.Certificate.Type)
	}

	if keyPair.Certificate.VehicleOemID != "0010" {
		t.Errorf("Expected VehicleOemID=0010, got %s", keyPair.Certificate.VehicleOemID)
	}

	// 验证签名算法
	if keyPair.Certificate.X509Cert.SignatureAlgorithm != x509.ECDSAWithSHA256 {
		t.Errorf("Expected ECDSAWithSHA256, got %s", keyPair.Certificate.X509Cert.SignatureAlgorithm)
	}

	// 验证有效期
	expectedNotAfter := time.Now().Add(VehicleOemCADefaultValidity)
	if keyPair.Certificate.X509Cert.NotAfter.After(expectedNotAfter.Add(time.Hour)) {
		t.Error("Certificate validity period is incorrect")
	}

	t.Logf("Generated CA certificate: Subject=%s, Validity=%v to %v",
		keyPair.Certificate.X509Cert.Subject,
		keyPair.Certificate.X509Cert.NotBefore,
		keyPair.Certificate.X509Cert.NotAfter)
}

func TestCertGenerator_GenerateOwnerKeyCert(t *testing.T) {
	generator := NewCertGenerator()

	// 首先生成 CA 证书
	caPrivKey, _ := generator.GenerateECKeyPair()
	caConfig := &VehicleOemCAConfig{
		VehicleOemID:   "0010",
		ValidityPeriod: VehicleOemCADefaultValidity,
		CommonName:     "TEST-CA",
		Organization:   "Test OEM",
		Country:        "CN",
	}
	caKeyPair, _ := generator.GenerateVehicleOemCACert(caConfig, caPrivKey)

	// 生成车主密钥对
	ownerPrivKey, err := generator.GenerateECKeyPair()
	if err != nil {
		t.Fatalf("Failed to generate owner key pair: %v", err)
	}

	// 生成钥匙 ID
	keyID := make([]byte, 16)
	copy(keyID, []byte{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10})

	ownerConfig := &OwnerKeyCertConfig{
		KeyID:          hex.EncodeToString(keyID),
		VehicleID:      "112233445566778899AABBCCDDEEFF00",
		VehicleOemID:   "0010",
		ValidityPeriod: OwnerCertDefaultValidity,
		Permissions:    0xFFFFFFFF,
		Mode:           CertModeCA,
	}

	ownerCert, err := generator.GenerateOwnerKeyCert(ownerConfig, &ownerPrivKey.PublicKey, caKeyPair.Certificate, caKeyPair.PrivateKey)
	if err != nil {
		t.Fatalf("Failed to generate owner key certificate: %v", err)
	}

	// 验证证书
	if ownerCert == nil {
		t.Fatal("Owner certificate is nil")
	}

	if ownerCert.Type != CertTypeOwnerDK {
		t.Errorf("Expected type CertTypeOwnerDK, got %d", ownerCert.Type)
	}

	if ownerCert.Mode != CertModeCA {
		t.Errorf("Expected mode CertModeCA, got %d", ownerCert.Mode)
	}

	if ownerCert.VehicleOemID != "0010" {
		t.Errorf("Expected VehicleOemID=0010, got %s", ownerCert.VehicleOemID)
	}

	// 验证证书大小 (不超过700字节)
	if len(ownerCert.DERData) > MaxOwnerCertSize {
		t.Errorf("Owner certificate size %d exceeds maximum %d", len(ownerCert.DERData), MaxOwnerCertSize)
	}

	// CA 模式下验证是 CA
	if ownerConfig.Mode == CertModeCA {
		if !ownerCert.X509Cert.IsCA {
			t.Error("Owner certificate should be a CA in CA mode")
		}
	}

	t.Logf("Generated owner key certificate: Subject=%s, Size=%d bytes",
		ownerCert.X509Cert.Subject, len(ownerCert.DERData))
}

func TestCertGenerator_GenerateOwnerKeyCert_NonCAMode(t *testing.T) {
	generator := NewCertGenerator()

	// 生成 CA 证书
	caPrivKey, _ := generator.GenerateECKeyPair()
	caConfig := &VehicleOemCAConfig{
		VehicleOemID:   "0010",
		ValidityPeriod: VehicleOemCADefaultValidity,
		CommonName:     "TEST-CA",
		Organization:   "Test OEM",
	}
	caKeyPair, _ := generator.GenerateVehicleOemCACert(caConfig, caPrivKey)

	// 生成车主密钥对
	ownerPrivKey, _ := generator.GenerateECKeyPair()
	keyID, _ := GenerateKeyID()

	ownerConfig := &OwnerKeyCertConfig{
		KeyID:          hex.EncodeToString(keyID),
		VehicleID:      "112233445566778899AABBCCDDEEFF00",
		VehicleOemID:   "0010",
		ValidityPeriod: OwnerCertDefaultValidity,
		Permissions:    0xFFFFFFFF,
		Mode:           CertModeNonCA, // 非 CA 模式
	}

	ownerCert, err := generator.GenerateOwnerKeyCert(ownerConfig, &ownerPrivKey.PublicKey, caKeyPair.Certificate, caKeyPair.PrivateKey)
	if err != nil {
		t.Fatalf("Failed to generate owner key certificate: %v", err)
	}

	// 验证证书模式
	if ownerCert.Mode != CertModeNonCA {
		t.Errorf("Expected mode CertModeNonCA, got %d", ownerCert.Mode)
	}

	// 检查扩展项是否包含模式标志
	found := false
	for _, ext := range ownerCert.X509Cert.Extensions {
		if ext.Id.Equal(OIDDigitalKeyMode) {
			found = true
			if len(ext.Value) >= 3 && ext.Value[2] != byte(CertModeNonCA) {
				t.Errorf("Expected mode byte 0x01, got 0x%02x", ext.Value[2])
			}
			break
		}
	}
	if !found {
		t.Error("DigitalKeyMode extension not found")
	}

	t.Logf("Generated owner key certificate (Non-CA mode): Mode=%d", ownerCert.Mode)
}

func TestCertGenerator_GenerateMidShareCert(t *testing.T) {
	generator := NewCertGenerator()

	// 生成 CA 和车主证书
	caPrivKey, _ := generator.GenerateECKeyPair()
	caConfig := &VehicleOemCAConfig{
		VehicleOemID: "0010",
	}
	caKeyPair, _ := generator.GenerateVehicleOemCACert(caConfig, caPrivKey)

	ownerPrivKey, _ := generator.GenerateECKeyPair()
	keyID, _ := GenerateKeyID()
	ownerConfig := &OwnerKeyCertConfig{
		KeyID:        hex.EncodeToString(keyID),
		VehicleID:    "112233445566778899AABBCCDDEEFF00",
		VehicleOemID: "0010",
		Mode:         CertModeCA,
	}
	ownerCert, _ := generator.GenerateOwnerKeyCert(ownerConfig, &ownerPrivKey.PublicKey, caKeyPair.Certificate, caKeyPair.PrivateKey)

	// 生成中间分享密钥对
	midPrivKey, err := generator.GenerateECKeyPair()
	if err != nil {
		t.Fatalf("Failed to generate mid-share key pair: %v", err)
	}

	midConfig := &MidShareCertConfig{
		VehicleID:      "112233445566778899AABBCCDDEEFF00",
		VehicleOemID:   "0010",
		ValidityPeriod: 7 * 24 * time.Hour, // 7天
		MaxValidTime:   time.Now().Add(30 * 24 * time.Hour),
	}

	midCert, err := generator.GenerateMidShareCert(midConfig, &midPrivKey.PublicKey, ownerCert, ownerPrivKey)
	if err != nil {
		t.Fatalf("Failed to generate mid-share certificate: %v", err)
	}

	// 验证证书
	if midCert == nil {
		t.Fatal("Mid-share certificate is nil")
	}

	if midCert.Type != CertTypeMidShare {
		t.Errorf("Expected type CertTypeMidShare, got %d", midCert.Type)
	}

	// 验证有效期不超过90天
	validity := midCert.X509Cert.NotAfter.Sub(midCert.X509Cert.NotBefore)
	if validity > MidShareCertMaxValidity+time.Hour {
		t.Errorf("Mid-share validity %v exceeds maximum %v", validity, MidShareCertMaxValidity)
	}

	// 验证大小
	if len(midCert.DERData) > MaxMidShareCertSize {
		t.Errorf("Mid-share certificate size %d exceeds maximum %d", len(midCert.DERData), MaxMidShareCertSize)
	}

	// CA 模式下是中间 CA
	if midCert.X509Cert.IsCA {
		t.Log("Mid-share certificate is a CA (required for signing SharedDK certs)")
	}

	t.Logf("Generated mid-share certificate: Validity=%v, Size=%d bytes",
		validity, len(midCert.DERData))
}

func TestCertGenerator_GenerateSharedKeyCert(t *testing.T) {
	generator := NewCertGenerator()

	// 生成必要的证书
	caPrivKey, _ := generator.GenerateECKeyPair()
	caConfig := &VehicleOemCAConfig{VehicleOemID: "0010"}
	caKeyPair, _ := generator.GenerateVehicleOemCACert(caConfig, caPrivKey)

	ownerPrivKey, _ := generator.GenerateECKeyPair()
	ownerKeyID, _ := GenerateKeyID()
	ownerConfig := &OwnerKeyCertConfig{
		KeyID:        hex.EncodeToString(ownerKeyID),
		VehicleID:    "112233445566778899AABBCCDDEEFF00",
		VehicleOemID: "0010",
		Mode:         CertModeCA,
	}
	ownerCert, _ := generator.GenerateOwnerKeyCert(ownerConfig, &ownerPrivKey.PublicKey, caKeyPair.Certificate, caKeyPair.PrivateKey)

	midPrivKey, _ := generator.GenerateECKeyPair()
	midConfig := &MidShareCertConfig{
		VehicleID:    "112233445566778899AABBCCDDEEFF00",
		VehicleOemID: "0010",
	}
	midCert, _ := generator.GenerateMidShareCert(midConfig, &midPrivKey.PublicKey, ownerCert, ownerPrivKey)

	// 生成好友密钥对
	friendPrivKey, err := generator.GenerateECKeyPair()
	if err != nil {
		t.Fatalf("Failed to generate friend key pair: %v", err)
	}

	friendKeyID, _ := GenerateKeyID()
	friendConfig := &SharedKeyCertConfig{
		KeyID:          hex.EncodeToString(friendKeyID),
		VehicleID:      "112233445566778899AABBCCDDEEFF00",
		VehicleOemID:   "0010",
		ValidityPeriod: 7 * 24 * time.Hour,
		Permissions:    0x0000000F, // 部分权限
		Mode:           CertModeCA,
	}

	friendCert, err := generator.GenerateSharedKeyCert(friendConfig, &friendPrivKey.PublicKey, midCert, midPrivKey)
	if err != nil {
		t.Fatalf("Failed to generate friend key certificate: %v", err)
	}

	// 验证证书
	if friendCert == nil {
		t.Fatal("Friend certificate is nil")
	}

	if friendCert.Type != CertTypeSharedDK {
		t.Errorf("Expected type CertTypeSharedDK, got %d", friendCert.Type)
	}

	// 验证大小
	if len(friendCert.DERData) > MaxSharedCertSize {
		t.Errorf("Friend certificate size %d exceeds maximum %d", len(friendCert.DERData), MaxSharedCertSize)
	}

	// 验证不是 CA
	if friendCert.X509Cert.IsCA {
		t.Error("Friend certificate should not be a CA")
	}

	t.Logf("Generated friend key certificate: Size=%d bytes, Permissions=%08X",
		len(friendCert.DERData), friendCert.Permissions)
}

func TestCertGenerator_GenerateSharedKeyCert_NonCAMode(t *testing.T) {
	generator := NewCertGenerator()

	// 生成 CA
	caPrivKey, _ := generator.GenerateECKeyPair()
	caConfig := &VehicleOemCAConfig{VehicleOemID: "0010"}
	caKeyPair, _ := generator.GenerateVehicleOemCACert(caConfig, caPrivKey)

	// 生成好友密钥对
	friendPrivKey, _ := generator.GenerateECKeyPair()
	friendKeyID, _ := GenerateKeyID()

	friendConfig := &SharedKeyCertConfig{
		KeyID:          hex.EncodeToString(friendKeyID),
		VehicleID:      "112233445566778899AABBCCDDEEFF00",
		VehicleOemID:   "0010",
		ValidityPeriod: 7 * 24 * time.Hour,
		Permissions:    0x0000000F,
		Mode:           CertModeNonCA,
	}

	friendCert, err := generator.GenerateSharedKeyCert(friendConfig, &friendPrivKey.PublicKey, caKeyPair.Certificate, caKeyPair.PrivateKey)
	if err != nil {
		t.Fatalf("Failed to generate friend key certificate (Non-CA mode): %v", err)
	}

	// 验证证书类型
	if friendCert.Type != CertTypeSharedDKV2 {
		t.Errorf("Expected type CertTypeSharedDKV2, got %d", friendCert.Type)
	}

	// 验证大小 (非 CA 模式最大800字节)
	if len(friendCert.DERData) > MaxSharedCertV2Size {
		t.Errorf("Friend certificate (V2) size %d exceeds maximum %d", len(friendCert.DERData), MaxSharedCertV2Size)
	}

	t.Logf("Generated friend key certificate (Non-CA mode): Type=%d, Size=%d bytes",
		friendCert.Type, len(friendCert.DERData))
}

func TestGenerateSerialNumber(t *testing.T) {
	serialNumber, err := GenerateSerialNumber()
	if err != nil {
		t.Fatalf("Failed to generate serial number: %v", err)
	}

	// 验证序列号长度 (20字节 = 160位)
	serialBytes := serialNumber.Bytes()
	if len(serialBytes) > 20 {
		t.Errorf("Serial number length %d exceeds 20 bytes", len(serialBytes))
	}

	t.Logf("Generated serial number: %s (%d bytes)", serialNumber.String(), len(serialBytes))
}

func TestGenerateKeyID(t *testing.T) {
	keyID, err := GenerateKeyID()
	if err != nil {
		t.Fatalf("Failed to generate key ID: %v", err)
	}

	if len(keyID) != 16 {
		t.Errorf("Expected key ID length 16, got %d", len(keyID))
	}

	t.Logf("Generated key ID: %s", hex.EncodeToString(keyID))
}

func TestCertGenerator_CertificateSize(t *testing.T) {
	generator := NewCertGenerator()

	// 生成 CA
	caPrivKey, _ := generator.GenerateECKeyPair()
	caConfig := &VehicleOemCAConfig{VehicleOemID: "0010"}
	caKeyPair, _ := generator.GenerateVehicleOemCACert(caConfig, caPrivKey)

	// 生成车主证书
	ownerPrivKey, _ := generator.GenerateECKeyPair()
	ownerKeyID, _ := GenerateKeyID()
	ownerConfig := &OwnerKeyCertConfig{
		KeyID:        hex.EncodeToString(ownerKeyID),
		VehicleID:    "112233445566778899AABBCCDDEEFF00",
		VehicleOemID: "0010",
		Mode:         CertModeCA,
	}
	ownerCert, _ := generator.GenerateOwnerKeyCert(ownerConfig, &ownerPrivKey.PublicKey, caKeyPair.Certificate, caKeyPair.PrivateKey)

	t.Logf("Certificate sizes:")
	t.Logf("  CA Certificate: %d bytes (no limit)", len(caKeyPair.Certificate.DERData))
	t.Logf("  Owner Certificate: %d bytes (max %d)", len(ownerCert.DERData), MaxOwnerCertSize)

	if len(ownerCert.DERData) > MaxOwnerCertSize {
		t.Errorf("Owner certificate size %d exceeds limit %d", len(ownerCert.DERData), MaxOwnerCertSize)
	}
}

func TestCertGenerator_CertificateValidity(t *testing.T) {
	generator := NewCertGenerator()

	// 生成 CA
	caPrivKey, _ := generator.GenerateECKeyPair()
	caConfig := &VehicleOemCAConfig{
		VehicleOemID:   "0010",
		ValidityPeriod: 10 * 365 * 24 * time.Hour, // 10年
	}
	caKeyPair, _ := generator.GenerateVehicleOemCACert(caConfig, caPrivKey)

	// 检查 CA 有效期
	caValidity := caKeyPair.Certificate.X509Cert.NotAfter.Sub(caKeyPair.Certificate.X509Cert.NotBefore)
	if caValidity < 10*365*24*time.Hour-time.Hour {
		t.Errorf("CA certificate validity %v is less than expected", caValidity)
	}

	// 生成车主证书 (有效期不能超过 CA)
	ownerPrivKey, _ := generator.GenerateECKeyPair()
	ownerKeyID, _ := GenerateKeyID()
	ownerConfig := &OwnerKeyCertConfig{
		KeyID:          hex.EncodeToString(ownerKeyID),
		VehicleID:      "112233445566778899AABBCCDDEEFF00",
		VehicleOemID:   "0010",
		ValidityPeriod: 20 * 365 * 24 * time.Hour, // 尝试生成20年 (超过 CA)
		Mode:           CertModeCA,
	}
	ownerCert, _ := generator.GenerateOwnerKeyCert(ownerConfig, &ownerPrivKey.PublicKey, caKeyPair.Certificate, caKeyPair.PrivateKey)

	// 车主证书有效期应该被限制为 CA 的有效期
	if ownerCert.X509Cert.NotAfter.After(caKeyPair.Certificate.X509Cert.NotAfter) {
		t.Errorf("Owner certificate expires after CA: Owner=%v, CA=%v",
			ownerCert.X509Cert.NotAfter, caKeyPair.Certificate.X509Cert.NotAfter)
	}

	t.Logf("Certificate validity:")
	t.Logf("  CA: %v to %v (%v)",
		caKeyPair.Certificate.X509Cert.NotBefore,
		caKeyPair.Certificate.X509Cert.NotAfter,
		caKeyPair.Certificate.X509Cert.NotAfter.Sub(caKeyPair.Certificate.X509Cert.NotBefore))
	t.Logf("  Owner: %v to %v (%v)",
		ownerCert.X509Cert.NotBefore,
		ownerCert.X509Cert.NotAfter,
		ownerCert.X509Cert.NotAfter.Sub(ownerCert.X509Cert.NotBefore))
}
