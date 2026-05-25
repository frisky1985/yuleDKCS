// Package iccoacert 使用示例
package iccoacert

import (
	"fmt"
	"log"
	"testing"
	"time"

	"encoding/hex"
)

// Example 完整使用示例
func Example() {
	// 创建证书生成器
	generator := NewCertGenerator()

	// ============ 步骤1: 创建车服务器 CA ============
	fmt.Println("=== Step 1: Create Vehicle OEM CA ===")
	
	// 生成 CA 密钥对
	caPrivKey, err := generator.GenerateECKeyPair()
	if err != nil {
		log.Fatalf("Failed to generate CA key pair: %v", err)
	}

	// 配置 CA
	caConfig := &VehicleOemCAConfig{
		VehicleOemID:   "0010",
		ValidityPeriod: VehicleOemCADefaultValidity, // 30年
		CommonName:     "DEMO-OEM-CA",
		Organization:   "Demo Vehicle OEM",
		Country:        "CN",
	}

	// 生成 CA 证书
	caKeyPair, err := generator.GenerateVehicleOemCACert(caConfig, caPrivKey)
	if err != nil {
		log.Fatalf("Failed to generate CA certificate: %v", err)
	}

	fmt.Printf("CA Certificate:\n")
	fmt.Printf("  Subject: %s\n", caKeyPair.Certificate.X509Cert.Subject)
	fmt.Printf("  Validity: %s to %s\n", 
		caKeyPair.Certificate.X509Cert.NotBefore.Format("2006-01-02"),
		caKeyPair.Certificate.X509Cert.NotAfter.Format("2006-01-02"))
	fmt.Printf("  Size: %d bytes\n\n", len(caKeyPair.Certificate.DERData))

	// ============ 步骤2: 创建车主钥匙证书 (非CA模式) ============
	fmt.Println("=== Step 2: Create Owner Key Certificate (Non-CA Mode) ===")

	// 生成车主密钥对
	ownerPrivKey, err := generator.GenerateECKeyPair()
	if err != nil {
		log.Fatalf("Failed to generate owner key pair: %v", err)
	}

	// 生成钥匙ID
	ownerKeyID, _ := GenerateKeyID()
	
	// 生成车辆ID
	vehicleID := "112233445566778899AABBCCDDEEFF00"

	// 配置车主证书 (非CA模式 - 推荐使用)
	ownerConfig := &OwnerKeyCertConfig{
		KeyID:          hex.EncodeToString(ownerKeyID),
		VehicleID:      vehicleID,
		VehicleOemID:   "0010",
		ValidityPeriod: OwnerCertDefaultValidity, // 5年
		Permissions:    0xFFFFFFFF,               // 全部权限
		Mode:           CertModeNonCA,            // 使用非CA模式
	}

	// 生成车主证书
	ownerCert, err := generator.GenerateOwnerKeyCert(
		ownerConfig,
		&ownerPrivKey.PublicKey,
		caKeyPair.Certificate,
		caKeyPair.PrivateKey,
	)
	if err != nil {
		log.Fatalf("Failed to generate owner key certificate: %v", err)
	}

	fmt.Printf("Owner Key Certificate:\n")
	fmt.Printf("  Key ID: %s\n", ownerCert.KeyID)
	fmt.Printf("  Type: %d (Owner DK)\n", ownerCert.Type)
	fmt.Printf("  Mode: %d (%s)\n", ownerCert.Mode, modeToString(ownerCert.Mode))
	fmt.Printf("  Size: %d bytes (max %d)\n\n", len(ownerCert.DERData), MaxOwnerCertSize)

	// ============ 步骤3: 创建好友钥匙证书 (非CA模式) ============
	fmt.Println("=== Step 3: Create Friend Key Certificate (Non-CA Mode) ===")

	// 生成好友密钥对 (模拟好友设备生成的密钥)
	friendPrivKey, err := generator.GenerateECKeyPair()
	if err != nil {
		log.Fatalf("Failed to generate friend key pair: %v", err)
	}

	// 生成好友钥匙ID
	friendKeyID, _ := GenerateKeyID()

	// 配置好友证书
	friendConfig := &SharedKeyCertConfig{
		KeyID:          hex.EncodeToString(friendKeyID),
		VehicleID:      vehicleID,
		VehicleOemID:   "0010",
		ValidityPeriod: 7 * 24 * time.Hour, // 7天
		Permissions:    0x0000000F,          // 部分权限: unlock, lock, start, trunk
		Mode:           CertModeNonCA,       // 非CA模式
	}

	// 生成好友证书 (直接由CA签发)
	friendCert, err := generator.GenerateSharedKeyCert(
		friendConfig,
		&friendPrivKey.PublicKey,
		caKeyPair.Certificate,
		caKeyPair.PrivateKey,
	)
	if err != nil {
		log.Fatalf("Failed to generate friend key certificate: %v", err)
	}

	fmt.Printf("Friend Key Certificate:\n")
	fmt.Printf("  Key ID: %s\n", friendCert.KeyID)
	fmt.Printf("  Type: %d (%s)\n", friendCert.Type, typeToString(friendCert.Type))
	fmt.Printf("  Mode: %d (%s)\n", friendCert.Mode, modeToString(friendCert.Mode))
	fmt.Printf("  Permissions: 0x%08X\n", friendCert.Permissions)
	fmt.Printf("  Size: %d bytes (max %d for V2)\n\n", len(friendCert.DERData), MaxSharedCertV2Size)

	// ============ 步骤4: 验证证书 ============
	fmt.Println("=== Step 4: Validate Certificates ===")

	validator := NewCertValidator()

	// 添加可信CA
	err = validator.AddTrustedCA(caKeyPair.Certificate)
	if err != nil {
		log.Fatalf("Failed to add trusted CA: %v", err)
	}

	// 验证车主证书
	err = validator.ValidateOwnerKeyCert(ownerCert, caKeyPair.Certificate)
	if err != nil {
		log.Printf("Owner certificate validation FAILED: %v\n", err)
	} else {
		fmt.Println("✓ Owner key certificate is valid")
	}

	// 验证好友证书 (非CA模式)
	err = validator.ValidateSharedKeyCert(friendCert, nil, caKeyPair.Certificate, CertModeNonCA)
	if err != nil {
		log.Printf("Friend certificate validation FAILED: %v\n", err)
	} else {
		fmt.Println("✓ Friend key certificate is valid (Non-CA mode)")
	}

	// ============ 步骤5: 导出证书 ============
	fmt.Println("\n=== Step 5: Export Certificates ===")

	// 导出为 PEM 格式 (服务器间传输)
	fmt.Printf("CA Certificate (PEM):\n%s\n", string(caKeyPair.Certificate.PEMData))
	fmt.Printf("Owner Certificate (PEM):\n%s\n", string(ownerCert.PEMData))
	fmt.Printf("Friend Certificate (PEM):\n%s\n", string(friendCert.PEMData))

	// 导出为 DER 格式 (APDU命令)
	fmt.Printf("Owner Certificate (DER): %d bytes\n", len(ownerCert.DERData))
	fmt.Printf("Friend Certificate (DER): %d bytes\n", len(friendCert.DERData))
}

// ExampleWithDatabase 展示如何与数据库一起使用
func ExampleWithDatabase() {
	// 注意: 这是示代码，需要实际的数据库连接
	// db, err := sql.Open("mysql", "user:password@/dbname")
	// if err != nil {
	//     log.Fatal(err)
	// }
	// defer db.Close()

	// 创建证书存储
	// store := NewCertStore(db)

	// 创建证书服务
	// certService := NewCertService(store)

	// 创建 CA
	// ctx := context.Background()
	// caKeyPair, err := certService.CreateVehicleOemCA(ctx, "0010")
	// if err != nil {
	//     log.Fatalf("Failed to create CA: %v", err)
	// }

	fmt.Println("Database integration example (commented out)")
}

// 辅助函数
func modeToString(mode CertMode) string {
	if mode == CertModeCA {
		return "CA Mode"
	}
	return "Non-CA Mode"
}

func typeToString(certType CertType) string {
	switch certType {
	case CertTypeVehicleOemCA:
		return "Vehicle OEM CA"
	case CertTypeVehicle:
		return "Vehicle"
	case CertTypeOwnerDK:
		return "Owner DK"
	case CertTypeMidShare:
		return "Mid-Share"
	case CertTypeSharedDK:
		return "Shared DK"
	case CertTypeSharedDKV2:
		return "Shared DK V2"
	default:
		return "Unknown"
	}
}

// BenchmarkGenerateOwnerKeyCert 性能测试
func BenchmarkGenerateOwnerKeyCert(b *testing.B) {
	generator := NewCertGenerator()
	caPrivKey, _ := generator.GenerateECKeyPair()
	caConfig := &VehicleOemCAConfig{VehicleOemID: "0010"}
	caKeyPair, _ := generator.GenerateVehicleOemCACert(caConfig, caPrivKey)

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		ownerPrivKey, _ := generator.GenerateECKeyPair()
		ownerKeyID, _ := GenerateKeyID()
		ownerConfig := &OwnerKeyCertConfig{
			KeyID:          hex.EncodeToString(ownerKeyID),
			VehicleID:      "112233445566778899AABBCCDDEEFF00",
			VehicleOemID:   "0010",
			ValidityPeriod: OwnerCertDefaultValidity,
			Mode:           CertModeNonCA,
		}
		generator.GenerateOwnerKeyCert(ownerConfig, &ownerPrivKey.PublicKey, caKeyPair.Certificate, caKeyPair.PrivateKey)
	}
}

// BenchmarkValidateCertificate 验证性能测试
func BenchmarkValidateCertificate(b *testing.B) {
	generator := NewCertGenerator()
	caPrivKey, _ := generator.GenerateECKeyPair()
	caConfig := &VehicleOemCAConfig{VehicleOemID: "0010"}
	caKeyPair, _ := generator.GenerateVehicleOemCACert(caConfig, caPrivKey)

	ownerPrivKey, _ := generator.GenerateECKeyPair()
	ownerKeyID, _ := GenerateKeyID()
	ownerConfig := &OwnerKeyCertConfig{
		KeyID:          hex.EncodeToString(ownerKeyID),
		VehicleID:      "112233445566778899AABBCCDDEEFF00",
		VehicleOemID:   "0010",
		ValidityPeriod: OwnerCertDefaultValidity,
		Mode:           CertModeNonCA,
	}
	ownerCert, _ := generator.GenerateOwnerKeyCert(ownerConfig, &ownerPrivKey.PublicKey, caKeyPair.Certificate, caKeyPair.PrivateKey)

	validator := NewCertValidator()
	validator.AddTrustedCA(caKeyPair.Certificate)

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		validator.ValidateOwnerKeyCert(ownerCert, caKeyPair.Certificate)
	}
}
