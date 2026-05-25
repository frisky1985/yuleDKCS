// Package iccoacert ICCOA DK 4.0 证书生成器
package iccoacert

import (
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rand"
	"crypto/sha1"
	"crypto/x509"
	"crypto/x509/pkix"
	"encoding/asn1"
	"encoding/hex"
	"encoding/pem"
	"fmt"
	"math/big"
	"time"
)

// CertGenerator 证书生成器
type CertGenerator struct{}

// NewCertGenerator 创建证书生成器
func NewCertGenerator() *CertGenerator {
	return &CertGenerator{}
}

// GenerateVehicleOemCACert 生成车服务器 CA 证书 (A类)
func (cg *CertGenerator) GenerateVehicleOemCACert(config *VehicleOemCAConfig, caPrivKey *ecdsa.PrivateKey) (*CertKeyPair, error) {
	if config.VehicleOemID == "" {
		return nil, fmt.Errorf("vehicle OEM ID is required")
	}

	// 生成序列号
	serialNumber, err := GenerateSerialNumber()
	if err != nil {
		return nil, fmt.Errorf("failed to generate serial number: %w", err)
	}

	// 设置有效期
	notBefore := time.Now()
	notAfter := notBefore.Add(config.ValidityPeriod)
	if config.ValidityPeriod == 0 {
		notAfter = notBefore.Add(VehicleOemCADefaultValidity)
	}

	// 构建 Subject
	subject := pkix.Name{
		CommonName:   config.CommonName,
		Organization: []string{config.Organization},
		Country:      []string{config.Country},
	}
	if subject.CommonName == "" {
		subject.CommonName = fmt.Sprintf("VEHICLE-OEM-CA-%s", config.VehicleOemID)
	}

	// 创建证书模板
	template := &x509.Certificate{
		SerialNumber:          serialNumber,
		Subject:               subject,
		NotBefore:             notBefore,
		NotAfter:              notAfter,
		KeyUsage:              x509.KeyUsageCertSign | x509.KeyUsageCRLSign | x509.KeyUsageDigitalSignature,
		BasicConstraintsValid: true,
		IsCA:                  true,
		MaxPathLen:            2, // 允许最多2层中间证书
	}

	// 添加 Subject Key Identifier
	subjectKeyID, err := generateSubjectKeyID(&caPrivKey.PublicKey)
	if err != nil {
		return nil, fmt.Errorf("failed to generate subject key ID: %w", err)
	}
	template.SubjectKeyId = subjectKeyID

	// 添加 ICCOA 扩展项
	extensions, err := cg.buildVehicleOemCAExtensions(config.VehicleOemID)
	if err != nil {
		return nil, fmt.Errorf("failed to build extensions: %w", err)
	}

	for _, ext := range extensions {
		template.ExtraExtensions = append(template.ExtraExtensions, pkix.Extension{
			Id:       ext.OID,
			Critical: ext.Critical,
			Value:    ext.Value,
		})
	}

	// 自签名证书
	certDER, err := x509.CreateCertificate(rand.Reader, template, template, &caPrivKey.PublicKey, caPrivKey)
	if err != nil {
		return nil, fmt.Errorf("failed to create certificate: %w", err)
	}

	// 解析证书
	x509Cert, err := x509.ParseCertificate(certDER)
	if err != nil {
		return nil, fmt.Errorf("failed to parse certificate: %w", err)
	}

	// 构建 ICCOA 证书
	iccoaCert := &ICCOACertificate{
		X509Cert:     x509Cert,
		Type:         CertTypeVehicleOemCA,
		Mode:         CertModeCA,
		VehicleOemID: config.VehicleOemID,
		DERData:      certDER,
	}

	// 转换为 PEM
	pemBlock := &pem.Block{
		Type:  "CERTIFICATE",
		Bytes: certDER,
	}
	iccoaCert.PEMData = pem.EncodeToMemory(pemBlock)

	return &CertKeyPair{
		Certificate: iccoaCert,
		PrivateKey:  caPrivKey,
		PublicKey:   &caPrivKey.PublicKey,
	}, nil
}

// GenerateVehicleCert 生成车证书 (B类)
func (cg *CertGenerator) GenerateVehicleCert(config *VehicleCertConfig, vehiclePubKey *ecdsa.PublicKey, caCert *ICCOACertificate, caPrivKey *ecdsa.PrivateKey) (*ICCOACertificate, error) {
	if config.VehicleID == "" {
		return nil, fmt.Errorf("vehicle ID is required")
	}

	// 生成序列号
	serialNumber, err := GenerateSerialNumber()
	if err != nil {
		return nil, fmt.Errorf("failed to generate serial number: %w", err)
	}

	// 设置有效期
	notBefore := time.Now()
	notAfter := notBefore.Add(config.ValidityPeriod)
	if config.ValidityPeriod == 0 {
		notAfter = notBefore.Add(VehicleCertDefaultValidity)
	}

	// 检查 CA 证书有效期
	if notAfter.After(caCert.X509Cert.NotAfter) {
		notAfter = caCert.X509Cert.NotAfter
	}

	// 构建 Subject
	subject := pkix.Name{
		CommonName: fmt.Sprintf("VEHICLE-%s", config.VehicleID),
	}

	// 创建证书模板
	template := &x509.Certificate{
		SerialNumber:          serialNumber,
		Subject:               subject,
		NotBefore:             notBefore,
		NotAfter:              notAfter,
		KeyUsage:              x509.KeyUsageDigitalSignature,
		BasicConstraintsValid: true,
		IsCA:                  false,
	}

	// 添加 Subject Key Identifier
	subjectKeyID, err := generateSubjectKeyID(vehiclePubKey)
	if err != nil {
		return nil, fmt.Errorf("failed to generate subject key ID: %w", err)
	}
	template.SubjectKeyId = subjectKeyID

	// 添加 Authority Key Identifier
	if len(caCert.X509Cert.SubjectKeyId) > 0 {
		template.AuthorityKeyId = caCert.X509Cert.SubjectKeyId
	}

	// 添加 ICCOA 扩展项
	extensions, err := cg.buildVehicleCertExtensions(config.VehicleID, config.VehicleOemID)
	if err != nil {
		return nil, fmt.Errorf("failed to build extensions: %w", err)
	}

	for _, ext := range extensions {
		template.ExtraExtensions = append(template.ExtraExtensions, pkix.Extension{
			Id:       ext.OID,
			Critical: ext.Critical,
			Value:    ext.Value,
		})
	}

	// 签发证书
	certDER, err := x509.CreateCertificate(rand.Reader, template, caCert.X509Cert, vehiclePubKey, caPrivKey)
	if err != nil {
		return nil, fmt.Errorf("failed to create certificate: %w", err)
	}

	// 解析证书
	x509Cert, err := x509.ParseCertificate(certDER)
	if err != nil {
		return nil, fmt.Errorf("failed to parse certificate: %w", err)
	}

	return &ICCOACertificate{
		X509Cert:     x509Cert,
		Type:         CertTypeVehicle,
		Mode:         CertModeCA,
		VehicleOemID: config.VehicleOemID,
		VehicleID:    config.VehicleID,
		DERData:      certDER,
		PEMData:      pem.EncodeToMemory(&pem.Block{Type: "CERTIFICATE", Bytes: certDER}),
	}, nil
}

// GenerateOwnerKeyCert 生成车主钥匙证书 (C类)
func (cg *CertGenerator) GenerateOwnerKeyCert(config *OwnerKeyCertConfig, ownerPubKey *ecdsa.PublicKey, caCert *ICCOACertificate, caPrivKey *ecdsa.PrivateKey) (*ICCOACertificate, error) {
	if config.KeyID == "" || config.VehicleID == "" {
		return nil, fmt.Errorf("key ID and vehicle ID are required")
	}

	// 生成序列号
	serialNumber, err := GenerateSerialNumber()
	if err != nil {
		return nil, fmt.Errorf("failed to generate serial number: %w", err)
	}

	// 设置有效期
	notBefore := time.Now()
	notAfter := notBefore.Add(config.ValidityPeriod)
	if config.ValidityPeriod == 0 {
		notAfter = notBefore.Add(OwnerCertDefaultValidity)
	}

	// 检查 CA 证书有效期
	if notAfter.After(caCert.X509Cert.NotAfter) {
		notAfter = caCert.X509Cert.NotAfter
	}

	// 构建 Subject
	subject := pkix.Name{
		CommonName: fmt.Sprintf("OWNER-%s", config.KeyID),
	}

	// 创建证书模板
	template := &x509.Certificate{
		SerialNumber:          serialNumber,
		Subject:               subject,
		NotBefore:             notBefore,
		NotAfter:              notAfter,
		KeyUsage:              x509.KeyUsageDigitalSignature | x509.KeyUsageCertSign,
		BasicConstraintsValid: true,
		IsCA:                  false,
	}

	// CA 模式时允许签发下级证书
	if config.Mode == CertModeCA {
		template.IsCA = true
		template.MaxPathLen = 1
	}

	// 非 CA 模式时不是 CA
	if config.Mode == CertModeNonCA {
		template.IsCA = false
	}

	// 添加 Subject Key Identifier
	subjectKeyID, err := generateSubjectKeyID(ownerPubKey)
	if err != nil {
		return nil, fmt.Errorf("failed to generate subject key ID: %w", err)
	}
	template.SubjectKeyId = subjectKeyID

	// 添加 Authority Key Identifier
	if len(caCert.X509Cert.SubjectKeyId) > 0 {
		template.AuthorityKeyId = caCert.X509Cert.SubjectKeyId
	}

	// 添加 ICCOA 扩展项
	extensions, err := cg.buildOwnerCertExtensions(config)
	if err != nil {
		return nil, fmt.Errorf("failed to build extensions: %w", err)
	}

	for _, ext := range extensions {
		template.ExtraExtensions = append(template.ExtraExtensions, pkix.Extension{
			Id:       ext.OID,
			Critical: ext.Critical,
			Value:    ext.Value,
		})
	}

	// 签发证书
	certDER, err := x509.CreateCertificate(rand.Reader, template, caCert.X509Cert, ownerPubKey, caPrivKey)
	if err != nil {
		return nil, fmt.Errorf("failed to create certificate: %w", err)
	}

	// 解析证书
	x509Cert, err := x509.ParseCertificate(certDER)
	if err != nil {
		return nil, fmt.Errorf("failed to parse certificate: %w", err)
	}

	return &ICCOACertificate{
		X509Cert:     x509Cert,
		Type:         CertTypeOwnerDK,
		Mode:         config.Mode,
		VehicleOemID: config.VehicleOemID,
		VehicleID:    config.VehicleID,
		KeyID:        config.KeyID,
		Permissions:  config.Permissions,
		DERData:      certDER,
		PEMData:      pem.EncodeToMemory(&pem.Block{Type: "CERTIFICATE", Bytes: certDER}),
	}, nil
}

// GenerateMidShareCert 生成中间分享证书 (D类)
func (cg *CertGenerator) GenerateMidShareCert(config *MidShareCertConfig, midPubKey *ecdsa.PublicKey, ownerCert *ICCOACertificate, ownerPrivKey *ecdsa.PrivateKey) (*ICCOACertificate, error) {
	if config.VehicleID == "" {
		return nil, fmt.Errorf("vehicle ID is required")
	}

	// 生成序列号
	serialNumber, err := GenerateSerialNumber()
	if err != nil {
		return nil, fmt.Errorf("failed to generate serial number: %w", err)
	}

	// 设置有效期 (不超过90天)
	notBefore := time.Now()
	validityPeriod := config.ValidityPeriod
	if validityPeriod == 0 || validityPeriod > MidShareCertMaxValidity {
		validityPeriod = MidShareCertMaxValidity
	}
	notAfter := notBefore.Add(validityPeriod)

	// 不能超过最大有效时间
	if !config.MaxValidTime.IsZero() && config.MaxValidTime.Before(notAfter) {
		notAfter = config.MaxValidTime
	}

	// 检查车主证书有效期
	if notAfter.After(ownerCert.X509Cert.NotAfter) {
		notAfter = ownerCert.X509Cert.NotAfter
	}

	// 构建 Subject
	subject := pkix.Name{
		CommonName: fmt.Sprintf("MIDSHARE-%s", config.VehicleID),
	}

	// 创建证书模板
	template := &x509.Certificate{
		SerialNumber:          serialNumber,
		Subject:               subject,
		NotBefore:             notBefore,
		NotAfter:              notAfter,
		KeyUsage:              x509.KeyUsageCertSign | x509.KeyUsageDigitalSignature,
		BasicConstraintsValid: true,
		IsCA:                  true, // 中间分享证书是 CA (RFC 5280: keyCertSign 必须配合 IsCA=true)
		MaxPathLen:            0,
	}

	// 添加 Subject Key Identifier
	subjectKeyID, err := generateSubjectKeyID(midPubKey)
	if err != nil {
		return nil, fmt.Errorf("failed to generate subject key ID: %w", err)
	}
	template.SubjectKeyId = subjectKeyID

	// 添加 Authority Key Identifier
	if len(ownerCert.X509Cert.SubjectKeyId) > 0 {
		template.AuthorityKeyId = ownerCert.X509Cert.SubjectKeyId
	}

	// 添加 ICCOA 扩展项
	extensions, err := cg.buildMidShareCertExtensions(config)
	if err != nil {
		return nil, fmt.Errorf("failed to build extensions: %w", err)
	}

	for _, ext := range extensions {
		template.ExtraExtensions = append(template.ExtraExtensions, pkix.Extension{
			Id:       ext.OID,
			Critical: ext.Critical,
			Value:    ext.Value,
		})
	}

	// 签发证书 (由车主钥匙证书签发)
	certDER, err := x509.CreateCertificate(rand.Reader, template, ownerCert.X509Cert, midPubKey, ownerPrivKey)
	if err != nil {
		return nil, fmt.Errorf("failed to create certificate: %w", err)
	}

	// 解析证书
	x509Cert, err := x509.ParseCertificate(certDER)
	if err != nil {
		return nil, fmt.Errorf("failed to parse certificate: %w", err)
	}

	return &ICCOACertificate{
		X509Cert:     x509Cert,
		Type:         CertTypeMidShare,
		Mode:         CertModeCA,
		VehicleOemID: config.VehicleOemID,
		VehicleID:    config.VehicleID,
		DERData:      certDER,
		PEMData:      pem.EncodeToMemory(&pem.Block{Type: "CERTIFICATE", Bytes: certDER}),
	}, nil
}

// GenerateSharedKeyCert 生成好友钥匙证书 (E类或K类)
func (cg *CertGenerator) GenerateSharedKeyCert(config *SharedKeyCertConfig, friendPubKey *ecdsa.PublicKey, signerCert *ICCOACertificate, signerPrivKey *ecdsa.PrivateKey) (*ICCOACertificate, error) {
	if config.KeyID == "" || config.VehicleID == "" {
		return nil, fmt.Errorf("key ID and vehicle ID are required")
	}

	// 生成序列号
	serialNumber, err := GenerateSerialNumber()
	if err != nil {
		return nil, fmt.Errorf("failed to generate serial number: %w", err)
	}

	// 设置有效期
	notBefore := time.Now()
	notAfter := notBefore.Add(config.ValidityPeriod)
	if config.ValidityPeriod == 0 {
		notAfter = notBefore.Add(SharedCertDefaultValidity)
	}

	// 检查签发者证书有效期
	if notAfter.After(signerCert.X509Cert.NotAfter) {
		notAfter = signerCert.X509Cert.NotAfter
	}

	// 确定证书类型
	certType := CertTypeSharedDK
	if config.Mode == CertModeNonCA {
		certType = CertTypeSharedDKV2
	}

	// 构建 Subject
	subject := pkix.Name{
		CommonName: fmt.Sprintf("FRIEND-%s", config.KeyID),
	}

	// 创建证书模板
	template := &x509.Certificate{
		SerialNumber:          serialNumber,
		Subject:               subject,
		NotBefore:             notBefore,
		NotAfter:              notAfter,
		KeyUsage:              x509.KeyUsageDigitalSignature,
		BasicConstraintsValid: true,
		IsCA:                  false,
	}

	// 添加 Subject Key Identifier
	subjectKeyID, err := generateSubjectKeyID(friendPubKey)
	if err != nil {
		return nil, fmt.Errorf("failed to generate subject key ID: %w", err)
	}
	template.SubjectKeyId = subjectKeyID

	// 添加 Authority Key Identifier
	if len(signerCert.X509Cert.SubjectKeyId) > 0 {
		template.AuthorityKeyId = signerCert.X509Cert.SubjectKeyId
	}

	// 添加 ICCOA 扩展项
	extensions, err := cg.buildSharedCertExtensions(config)
	if err != nil {
		return nil, fmt.Errorf("failed to build extensions: %w", err)
	}

	for _, ext := range extensions {
		template.ExtraExtensions = append(template.ExtraExtensions, pkix.Extension{
			Id:       ext.OID,
			Critical: ext.Critical,
			Value:    ext.Value,
		})
	}

	// 签发证书
	certDER, err := x509.CreateCertificate(rand.Reader, template, signerCert.X509Cert, friendPubKey, signerPrivKey)
	if err != nil {
		return nil, fmt.Errorf("failed to create certificate: %w", err)
	}

	// 解析证书
	x509Cert, err := x509.ParseCertificate(certDER)
	if err != nil {
		return nil, fmt.Errorf("failed to parse certificate: %w", err)
	}

	return &ICCOACertificate{
		X509Cert:     x509Cert,
		Type:         certType,
		Mode:         config.Mode,
		VehicleOemID: config.VehicleOemID,
		VehicleID:    config.VehicleID,
		KeyID:        config.KeyID,
		Permissions:  config.Permissions,
		DERData:      certDER,
		PEMData:      pem.EncodeToMemory(&pem.Block{Type: "CERTIFICATE", Bytes: certDER}),
	}, nil
}

// GenerateECKeyPair 生成 ECC 密钥对 (P-256)
func (cg *CertGenerator) GenerateECKeyPair() (*ecdsa.PrivateKey, error) {
	return ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
}

// generateSubjectKeyID 生成 Subject Key Identifier (RFC 5280 §4.2.1.2)
// 使用公钥的 SHA-1 哈希的前 20 字节作为 key identifier
func generateSubjectKeyID(pubKey *ecdsa.PublicKey) ([]byte, error) {
	pubKeyBytes := elliptic.Marshal(pubKey.Curve, pubKey.X, pubKey.Y)
	// 按照 RFC 5280 标准: Subject Key Identifier = SHA-1(公钥的 DER 编码)
	hash := sha1.Sum(pubKeyBytes)
	return hash[:], nil
}

// buildVehicleOemCAExtensions 构建车服务器 CA 扩展项
func (cg *CertGenerator) buildVehicleOemCAExtensions(vehicleOemID string) ([]ICCOAExtension, error) {
	extensions := []ICCOAExtension{}

	// 证书类型扩展 (OID 1.3.6.1.4.1.59129.1.1)
	// 值: '3003020120'H (ASN.1 INTEGER 32)
	certTypeValue := []byte{0x30, 0x03, 0x02, 0x01, 0x20}
	extensions = append(extensions, ICCOAExtension{
		OID:      OIDCertTypeVehicleOemCA,
		Critical: true,
		Value:    certTypeValue,
	})

	// 车企唯一标识符扩展 (OID 1.3.6.1.4.1.59129.2.5)
	oemIDBytes, err := hex.DecodeString(vehicleOemID)
	if err != nil {
		return nil, fmt.Errorf("invalid vehicle OEM ID: %w", err)
	}
	// OCTET STRING 编码
	oemIDValue := append([]byte{0x04, byte(len(oemIDBytes))}, oemIDBytes...)
	extensions = append(extensions, ICCOAExtension{
		OID:      OIDVehicleOemID,
		Critical: true,
		Value:    oemIDValue,
	})

	return extensions, nil
}

// buildVehicleCertExtensions 构建车证书扩展项
func (cg *CertGenerator) buildVehicleCertExtensions(vehicleID, vehicleOemID string) ([]ICCOAExtension, error) {
	extensions := []ICCOAExtension{}

	// 证书类型扩展 (OID 1.3.6.1.4.1.59129.1.2)
	certTypeValue := []byte{0x30, 0x03, 0x02, 0x01, 0x20}
	extensions = append(extensions, ICCOAExtension{
		OID:      OIDCertTypeVehicle,
		Critical: true,
		Value:    certTypeValue,
	})

	// 车辆唯一标识符扩展 (OID 1.3.6.1.4.1.59129.2.1)
	vehicleIDBytes, err := hex.DecodeString(vehicleID)
	if err != nil {
		return nil, fmt.Errorf("invalid vehicle ID: %w", err)
	}
	vehicleIDValue := append([]byte{0x04, byte(len(vehicleIDBytes))}, vehicleIDBytes...)
	extensions = append(extensions, ICCOAExtension{
		OID:      OIDVehicleID,
		Critical: true,
		Value:    vehicleIDValue,
	})

	return extensions, nil
}

// buildOwnerCertExtensions 构建车主钥匙证书扩展项
func (cg *CertGenerator) buildOwnerCertExtensions(config *OwnerKeyCertConfig) ([]ICCOAExtension, error) {
	extensions := []ICCOAExtension{}

	// 证书类型扩展 (OID 1.3.6.1.4.1.59129.1.3)
	certTypeValue := []byte{0x30, 0x03, 0x02, 0x01, 0x20}
	extensions = append(extensions, ICCOAExtension{
		OID:      OIDCertTypeOwnerDK,
		Critical: true,
		Value:    certTypeValue,
	})

	// 车辆唯一标识符扩展
	vehicleIDBytes, err := hex.DecodeString(config.VehicleID)
	if err != nil {
		return nil, fmt.Errorf("invalid vehicle ID: %w", err)
	}
	vehicleIDValue := append([]byte{0x04, byte(len(vehicleIDBytes))}, vehicleIDBytes...)
	extensions = append(extensions, ICCOAExtension{
		OID:      OIDVehicleID,
		Critical: true,
		Value:    vehicleIDValue,
	})

	// 数字车钥匙唯一标识符扩展
	keyIDBytes, err := hex.DecodeString(config.KeyID)
	if err != nil {
		return nil, fmt.Errorf("invalid key ID: %w", err)
	}
	keyIDValue := append([]byte{0x04, byte(len(keyIDBytes))}, keyIDBytes...)
	extensions = append(extensions, ICCOAExtension{
		OID:      OIDDigitalKeyID,
		Critical: true,
		Value:    keyIDValue,
	})

	// 数字车钥匙权限扩展
	permsBytes := make([]byte, 4)
	permsBytes[0] = byte(config.Permissions >> 24)
	permsBytes[1] = byte(config.Permissions >> 16)
	permsBytes[2] = byte(config.Permissions >> 8)
	permsBytes[3] = byte(config.Permissions)
	permsValue := append([]byte{0x04, 0x04}, permsBytes...)
	extensions = append(extensions, ICCOAExtension{
		OID:      OIDDigitalKeyAuth,
		Critical: false,
		Value:    permsValue,
	})

	// 证书模式扩展 (非 CA 模式时)
	if config.Mode == CertModeNonCA {
		modeValue := []byte{0x04, 0x01, byte(config.Mode)}
		extensions = append(extensions, ICCOAExtension{
			OID:      OIDDigitalKeyMode,
			Critical: true,
			Value:    modeValue,
		})
	}

	return extensions, nil
}

// buildMidShareCertExtensions 构建中间分享证书扩展项
func (cg *CertGenerator) buildMidShareCertExtensions(config *MidShareCertConfig) ([]ICCOAExtension, error) {
	extensions := []ICCOAExtension{}

	// 证书类型扩展 (OID 1.3.6.1.4.1.59129.1.4)
	certTypeValue := []byte{0x30, 0x03, 0x02, 0x01, 0x20}
	extensions = append(extensions, ICCOAExtension{
		OID:      OIDCertTypeMidShare,
		Critical: true,
		Value:    certTypeValue,
	})

	// 车辆唯一标识符扩展
	vehicleIDBytes, err := hex.DecodeString(config.VehicleID)
	if err != nil {
		return nil, fmt.Errorf("invalid vehicle ID: %w", err)
	}
	vehicleIDValue := append([]byte{0x04, byte(len(vehicleIDBytes))}, vehicleIDBytes...)
	extensions = append(extensions, ICCOAExtension{
		OID:      OIDVehicleID,
		Critical: true,
		Value:    vehicleIDValue,
	})

	return extensions, nil
}

// buildSharedCertExtensions 构建好友钥匙证书扩展项
func (cg *CertGenerator) buildSharedCertExtensions(config *SharedKeyCertConfig) ([]ICCOAExtension, error) {
	extensions := []ICCOAExtension{}

	// 证书类型扩展
	var oid asn1.ObjectIdentifier
	if config.Mode == CertModeNonCA {
		oid = OIDCertTypeSharedDKV2
	} else {
		oid = OIDCertTypeSharedDK
	}
	certTypeValue := []byte{0x30, 0x03, 0x02, 0x01, 0x20}
	extensions = append(extensions, ICCOAExtension{
		OID:      oid,
		Critical: true,
		Value:    certTypeValue,
	})

	// 车辆唯一标识符扩展
	vehicleIDBytes, err := hex.DecodeString(config.VehicleID)
	if err != nil {
		return nil, fmt.Errorf("invalid vehicle ID: %w", err)
	}
	vehicleIDValue := append([]byte{0x04, byte(len(vehicleIDBytes))}, vehicleIDBytes...)
	extensions = append(extensions, ICCOAExtension{
		OID:      OIDVehicleID,
		Critical: true,
		Value:    vehicleIDValue,
	})

	// 数字车钥匙唯一标识符扩展
	keyIDBytes, err := hex.DecodeString(config.KeyID)
	if err != nil {
		return nil, fmt.Errorf("invalid key ID: %w", err)
	}
	keyIDValue := append([]byte{0x04, byte(len(keyIDBytes))}, keyIDBytes...)
	extensions = append(extensions, ICCOAExtension{
		OID:      OIDDigitalKeyID,
		Critical: true,
		Value:    keyIDValue,
	})

	// 数字车钥匙权限扩展
	permsBytes := make([]byte, 4)
	permsBytes[0] = byte(config.Permissions >> 24)
	permsBytes[1] = byte(config.Permissions >> 16)
	permsBytes[2] = byte(config.Permissions >> 8)
	permsBytes[3] = byte(config.Permissions)
	permsValue := append([]byte{0x04, 0x04}, permsBytes...)
	extensions = append(extensions, ICCOAExtension{
		OID:      OIDDigitalKeyAuth,
		Critical: false,
		Value:    permsValue,
	})

	return extensions, nil
}

// GenerateSerialNumber 生成证书序列号 (20字节随机数)
func GenerateSerialNumber() (*big.Int, error) {
	serialNumberLimit := new(big.Int).Lsh(big.NewInt(1), 160) // 2^160
	serialNumber, err := rand.Int(rand.Reader, serialNumberLimit)
	if err != nil {
		return nil, err
	}
	return serialNumber, nil
}

// GenerateKeyID 生成钥匙唯一标识符 (16字节)
func GenerateKeyID() ([]byte, error) {
	keyID := make([]byte, 16)
	_, err := rand.Read(keyID)
	if err != nil {
		return nil, err
	}
	return keyID, nil
}

// GenerateVehicleID 生成车辆唯一标识符 (16字节)
func GenerateVehicleID() ([]byte, error) {
	vehicleID := make([]byte, 16)
	_, err := rand.Read(vehicleID)
	if err != nil {
		return nil, err
	}
	return vehicleID, nil
}

// GenerateVehicleOemID 生成车企唯一标识符 (2字节)
func GenerateVehicleOemID() ([]byte, error) {
	oemID := make([]byte, 2)
	_, err := rand.Read(oemID)
	if err != nil {
		return nil, err
	}
	return oemID, nil
}
