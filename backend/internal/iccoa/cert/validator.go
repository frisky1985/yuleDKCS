// Package iccoacert ICCOA DK 4.0 证书验证
package iccoacert

import (
	"crypto/ecdsa"
	"crypto/x509"
	"encoding/asn1"
	"errors"
	"fmt"
	"time"
)

// CertValidator 证书验证器
type CertValidator struct {
	trustedCACerts map[string]*ICCOACertificate // 可信任的 CA 证书
}

// NewCertValidator 创建证书验证器
func NewCertValidator() *CertValidator {
	return &CertValidator{
		trustedCACerts: make(map[string]*ICCOACertificate),
	}
}

// AddTrustedCA 添加可信任的 CA 证书
func (cv *CertValidator) AddTrustedCA(cert *ICCOACertificate) error {
	if cert.Type != CertTypeVehicleOemCA {
		return errors.New("only Vehicle OEM CA certificates can be added as trusted")
	}
	
	// 验证 CA 证书
	if err := cv.ValidateCACertificate(cert); err != nil {
		return fmt.Errorf("invalid CA certificate: %w", err)
	}
	
	// 使用 Subject Key ID 作为键
	keyID := fmt.Sprintf("%x", cert.X509Cert.SubjectKeyId)
	cv.trustedCACerts[keyID] = cert
	
	return nil
}

// ValidateCACertificate 验证 CA 证书
func (cv *CertValidator) ValidateCACertificate(cert *ICCOACertificate) error {
	if cert == nil || cert.X509Cert == nil {
		return errors.New("certificate is nil")
	}
	
	// 验证基本约束
	if !cert.X509Cert.IsCA {
		return errors.New("CA certificate must have IsCA=true")
	}
	
	// 验证签名算法
	if cert.X509Cert.SignatureAlgorithm != x509.ECDSAWithSHA256 {
		return fmt.Errorf("CA certificate must use ECDSA-SHA256, got %s", cert.X509Cert.SignatureAlgorithm)
	}
	
	// 验证公钥类型
	_, ok := cert.X509Cert.PublicKey.(*ecdsa.PublicKey)
	if !ok {
		return errors.New("CA certificate must use ECDSA public key")
	}
	
	// 验证有效期
	if time.Now().Before(cert.X509Cert.NotBefore) {
		return errors.New("CA certificate is not yet valid")
	}
	if time.Now().After(cert.X509Cert.NotAfter) {
		return errors.New("CA certificate has expired")
	}
	
	// 验证缺失的扩展项
	if len(cert.X509Cert.SubjectKeyId) == 0 {
		return errors.New("CA certificate must have Subject Key Identifier")
	}
	
	return nil
}

// ValidateCertificateChain 验证证书链
func (cv *CertValidator) ValidateCertificateChain(chain *CertificateChain) error {
	if chain == nil {
		return errors.New("certificate chain is nil")
	}
	
	if chain.RootCA == nil {
		return errors.New("root CA is missing")
	}
	
	if chain.EndEntity == nil {
		return errors.New("end entity certificate is missing")
	}
	
	// 验证根 CA 证书是否可信任
	rootKeyID := fmt.Sprintf("%x", chain.RootCA.X509Cert.SubjectKeyId)
	trustedCA, ok := cv.trustedCACerts[rootKeyID]
	if !ok {
		// 如果不在可信任列表中，验证自签名
		if err := cv.ValidateCACertificate(chain.RootCA); err != nil {
			return fmt.Errorf("root CA is not trusted: %w", err)
		}
	} else {
		// 使用可信任的 CA 证书
		chain.RootCA = trustedCA
	}
	
	// 验证证书链的完整性
	certs := []*ICCOACertificate{chain.RootCA}
	certs = append(certs, chain.Intermediate...)
	certs = append(certs, chain.EndEntity)
	
	for i := 0; i < len(certs)-1; i++ {
		issuer := certs[i]
		subject := certs[i+1]
		
		// 验证签名
		if err := cv.verifySignature(subject, issuer); err != nil {
			return fmt.Errorf("signature verification failed at level %d: %w", i, err)
		}
		
		// 验证时间
		if time.Now().Before(subject.X509Cert.NotBefore) {
			return fmt.Errorf("certificate at level %d is not yet valid", i+1)
		}
		if time.Now().After(subject.X509Cert.NotAfter) {
			return fmt.Errorf("certificate at level %d has expired", i+1)
		}
		
		// 验证有效期不能超过颁发者
		if subject.X509Cert.NotAfter.After(issuer.X509Cert.NotAfter) {
			return fmt.Errorf("certificate at level %d expires after issuer", i+1)
		}
	}
	
	return nil
}

// ValidateOwnerKeyCert 验证车主钥匙证书
func (cv *CertValidator) ValidateOwnerKeyCert(cert *ICCOACertificate, caCert *ICCOACertificate) error {
	if cert == nil || cert.X509Cert == nil {
		return errors.New("certificate is nil")
	}
	
	// 验证证书类型
	if cert.Type != CertTypeOwnerDK {
		return fmt.Errorf("expected owner key certificate, got %d", cert.Type)
	}
	
	// 验证签名
	if err := cv.verifySignature(cert, caCert); err != nil {
		return fmt.Errorf("signature verification failed: %w", err)
	}
	
	// 验证有效期
	if err := cv.validateValidity(cert); err != nil {
		return err
	}
	
	// 验证扩展项
	if err := cv.validateOwnerCertExtensions(cert); err != nil {
		return fmt.Errorf("extension validation failed: %w", err)
	}
	
	// 验证大小 (不超过700字节)
	if len(cert.DERData) > MaxOwnerCertSize {
		return fmt.Errorf("owner certificate size %d exceeds maximum %d", len(cert.DERData), MaxOwnerCertSize)
	}
	
	return nil
}

// ValidateSharedKeyCert 验证好友钥匙证书
func (cv *CertValidator) ValidateSharedKeyCert(cert *ICCOACertificate, midCert *ICCOACertificate, caCert *ICCOACertificate, mode CertMode) error {
	if cert == nil || cert.X509Cert == nil {
		return errors.New("certificate is nil")
	}
	
	// 验证证书类型
	expectedType := CertTypeSharedDK
	maxSize := MaxSharedCertSize
	if mode == CertModeNonCA {
		expectedType = CertTypeSharedDKV2
		maxSize = MaxSharedCertV2Size
	}
	
	if cert.Type != expectedType {
		return fmt.Errorf("expected shared key certificate type %d, got %d", expectedType, cert.Type)
	}
	
	// CA 模式验证证书链: friend <- mid <- owner <- CA
	// 非 CA 模式验证: friend <- CA
	if mode == CertModeCA {
		// 验证中间证书签名
		if err := cv.verifySignature(cert, midCert); err != nil {
			return fmt.Errorf("signature verification from mid-share failed: %w", err)
		}
		// 验证中间证书有效期 (车端忽略)
		// ...
	} else {
		// 非 CA 模式直接验证 CA 签名
		if err := cv.verifySignature(cert, caCert); err != nil {
			return fmt.Errorf("signature verification from CA failed: %w", err)
		}
	}
	
	// 验证有效期
	if err := cv.validateValidity(cert); err != nil {
		return err
	}
	
	// 验证大小
	if len(cert.DERData) > maxSize {
		return fmt.Errorf("shared certificate size %d exceeds maximum %d", len(cert.DERData), maxSize)
	}
	
	return nil
}

// ValidateMidShareCert 验证中间分享证书
func (cv *CertValidator) ValidateMidShareCert(cert *ICCOACertificate, ownerCert *ICCOACertificate) error {
	if cert == nil || cert.X509Cert == nil {
		return errors.New("certificate is nil")
	}
	
	// 验证证书类型
	if cert.Type != CertTypeMidShare {
		return fmt.Errorf("expected mid-share certificate, got %d", cert.Type)
	}
	
	// 验证签名
	if err := cv.verifySignature(cert, ownerCert); err != nil {
		return fmt.Errorf("signature verification failed: %w", err)
	}
	
	// 验证有效期 (不超过90天)
	if err := cv.validateValidity(cert); err != nil {
		return err
	}
	
	validity := cert.X509Cert.NotAfter.Sub(cert.X509Cert.NotBefore)
	if validity > MidShareCertMaxValidity {
		return fmt.Errorf("mid-share certificate validity %v exceeds maximum %v", validity, MidShareCertMaxValidity)
	}
	
	// 验证大小
	if len(cert.DERData) > MaxMidShareCertSize {
		return fmt.Errorf("mid-share certificate size %d exceeds maximum %d", len(cert.DERData), MaxMidShareCertSize)
	}
	
	return nil
}

// verifySignature 验证证书签名
func (cv *CertValidator) verifySignature(cert *ICCOACertificate, issuer *ICCOACertificate) error {
	// 验证 Authority Key ID 匹配
	if len(cert.X509Cert.AuthorityKeyId) > 0 && len(issuer.X509Cert.SubjectKeyId) > 0 {
		if fmt.Sprintf("%x", cert.X509Cert.AuthorityKeyId) != fmt.Sprintf("%x", issuer.X509Cert.SubjectKeyId) {
			return errors.New("authority key ID does not match issuer subject key ID")
		}
	}
	
	// 使用 Go 标准库验证签名
	err := cert.X509Cert.CheckSignatureFrom(issuer.X509Cert)
	if err != nil {
		return fmt.Errorf("signature check failed: %w", err)
	}
	
	return nil
}

// validateValidity 验证证书有效期
func (cv *CertValidator) validateValidity(cert *ICCOACertificate) error {
	now := time.Now()
	
	if now.Before(cert.X509Cert.NotBefore) {
		return errors.New("certificate is not yet valid")
	}
	
	if now.After(cert.X509Cert.NotAfter) {
		return errors.New("certificate has expired")
	}
	
	return nil
}

// validateOwnerCertExtensions 验证车主钥匙证书扩展项
func (cv *CertValidator) validateOwnerCertExtensions(cert *ICCOACertificate) error {
	// 检查必需的扩展项
	requiredOIDs := []asn1.ObjectIdentifier{
		OIDCertTypeOwnerDK,
		OIDVehicleID,
		OIDDigitalKeyID,
	}
	
	extMap := make(map[string]bool)
	for _, ext := range cert.X509Cert.Extensions {
		extMap[ext.Id.String()] = true
	}
	
	for _, oid := range requiredOIDs {
		if !extMap[oid.String()] {
			return fmt.Errorf("required extension %s is missing", oid.String())
		}
	}
	
	return nil
}

// ParseICCOACertificate 解析 ICCOA 证书
func ParseICCOACertificate(derData []byte) (*ICCOACertificate, error) {
	if len(derData) == 0 {
		return nil, errors.New("DER data is empty")
	}
	
	x509Cert, err := x509.ParseCertificate(derData)
	if err != nil {
		return nil, fmt.Errorf("failed to parse X.509 certificate: %w", err)
	}
	
	// 解析 ICCOA 特定字段
	cert := &ICCOACertificate{
		X509Cert: x509Cert,
		DERData:  derData,
	}
	
	// 从扩展项中提取 ICCOA 信息
	for _, ext := range x509Cert.Extensions {
		switch {
		case ext.Id.Equal(OIDCertTypeVehicleOemCA):
			cert.Type = CertTypeVehicleOemCA
		case ext.Id.Equal(OIDCertTypeVehicle):
			cert.Type = CertTypeVehicle
		case ext.Id.Equal(OIDCertTypeOwnerDK):
			cert.Type = CertTypeOwnerDK
		case ext.Id.Equal(OIDCertTypeMidShare):
			cert.Type = CertTypeMidShare
		case ext.Id.Equal(OIDCertTypeSharedDK):
			cert.Type = CertTypeSharedDK
		case ext.Id.Equal(OIDVehicleID):
			cert.VehicleID = extractOCTETString(ext.Value)
		case ext.Id.Equal(OIDDigitalKeyID):
			cert.KeyID = extractOCTETString(ext.Value)
		case ext.Id.Equal(OIDVehicleOemID):
			cert.VehicleOemID = extractOCTETString(ext.Value)
		case ext.Id.Equal(OIDDigitalKeyMode):
			if len(ext.Value) >= 3 {
				cert.Mode = CertMode(ext.Value[2])
			}
		}
	}
	
	return cert, nil
}

// extractOCTETString 从 ASN.1 OCTET STRING 中提取数据
func extractOCTETString(data []byte) string {
	if len(data) < 2 {
		return ""
	}
	// 简单解析: 跳过标签和长度
	if data[0] == 0x04 {
		length := int(data[1])
		if len(data) >= 2+length {
			return fmt.Sprintf("%x", data[2:2+length])
		}
	}
	return fmt.Sprintf("%x", data)
}

// GetTrustedCAs 获取所有可信任的 CA 证书
func (cv *CertValidator) GetTrustedCAs() []*ICCOACertificate {
	certs := make([]*ICCOACertificate, 0, len(cv.trustedCACerts))
	for _, cert := range cv.trustedCACerts {
		certs = append(certs, cert)
	}
	return certs
}

// RemoveTrustedCA 移除可信任的 CA 证书
func (cv *CertValidator) RemoveTrustedCA(subjectKeyID []byte) {
	keyID := fmt.Sprintf("%x", subjectKeyID)
	delete(cv.trustedCACerts, keyID)
}
