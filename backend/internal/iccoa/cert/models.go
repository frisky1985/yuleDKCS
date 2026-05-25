// Package iccoacert ICCOA DK 4.0 证书管理
// 符合 ICCOA/T 002-2024 标准 - 第6章 证书要求
package iccoacert

import (
	"crypto/ecdsa"
	"crypto/x509"
	"crypto/x509/pkix"
	"encoding/asn1"
	"time"
)

// ICCOA 证书类型定义
type CertType uint8

const (
	CertTypeVehicleOemCA   CertType = 0x01 // A: 车服务器 CA 证书
	CertTypeVehicle        CertType = 0x02 // B: 车证书
	CertTypeOwnerDK        CertType = 0x03 // C: 车主数字车钥匙证书
	CertTypeMidShare       CertType = 0x04 // D: 中间分享证书
	CertTypeSharedDK       CertType = 0x05 // E: 好友数字车钥匙证书
	CertTypeSharedDKV2     CertType = 0x0B // K: 好友证书 V2 (非CA模式)
)

// ICCOA OID 定义
// 1.3.6.1.4.1.59129 - ICCOA 私有企业 OID
var (
	// 证书类型 OID
	OIDCertTypeVehicleOemCA = asn1.ObjectIdentifier{1, 3, 6, 1, 4, 1, 59129, 1, 1}
	OIDCertTypeVehicle      = asn1.ObjectIdentifier{1, 3, 6, 1, 4, 1, 59129, 1, 2}
	OIDCertTypeOwnerDK      = asn1.ObjectIdentifier{1, 3, 6, 1, 4, 1, 59129, 1, 3}
	OIDCertTypeMidShare     = asn1.ObjectIdentifier{1, 3, 6, 1, 4, 1, 59129, 1, 4}
	OIDCertTypeSharedDK     = asn1.ObjectIdentifier{1, 3, 6, 1, 4, 1, 59129, 1, 5}
	OIDCertTypeSharedDKV2   = asn1.ObjectIdentifier{1, 3, 6, 1, 4, 1, 59129, 1, 0x0B}
	
	// 扩展信息 OID
	OIDVehicleOemID       = asn1.ObjectIdentifier{1, 3, 6, 1, 4, 1, 59129, 2, 5} // 车企唯一标识符
	OIDVehicleID          = asn1.ObjectIdentifier{1, 3, 6, 1, 4, 1, 59129, 2, 1} // 车辆唯一标识符
	OIDDigitalKeyID       = asn1.ObjectIdentifier{1, 3, 6, 1, 4, 1, 59129, 2, 2} // 数字车钥匙唯一标识符
	OIDDigitalKeyAuth     = asn1.ObjectIdentifier{1, 3, 6, 1, 4, 1, 59129, 2, 4} // 数字车钥匙权限
	OIDDigitalKeyMode     = asn1.ObjectIdentifier{1, 3, 6, 1, 4, 1, 59129, 2, 10} // 证书体系模式 (00=CA, 01=非CA)
)

// 证书模式
type CertMode uint8

const (
	CertModeCA    CertMode = 0x00 // CA 模式
	CertModeNonCA CertMode = 0x01 // 非 CA 模式 (ICCOA 4.0 新增)
)

// 证书大小限制 (字节)
const (
	MaxOwnerCertSize    = 700  // 车主证书最大 700 字节
	MaxMidShareCertSize = 700  // 中间分享证书最大 700 字节
	MaxSharedCertSize   = 700  // 好友证书最大 700 字节
	MaxSharedCertV2Size = 800  // 好友证书 V2 最大 800 字节
)

// 证书有效期常量
const (
	VehicleOemCADefaultValidity    = 30 * 365 * 24 * time.Hour // 车服务器 CA 默认 30 年
	VehicleCertDefaultValidity     = 20 * 365 * 24 * time.Hour // 车证书默认 20 年
	OwnerCertDefaultValidity       = 5 * 365 * 24 * time.Hour  // 车主证书默认 5 年
	MidShareCertMaxValidity        = 90 * 24 * time.Hour       // 中间分享证书最大 90 天
	SharedCertDefaultValidity      = 30 * 24 * time.Hour       // 好友证书默认 30 天
)

// ICCOACertificate ICCOA 证书结构
type ICCOACertificate struct {
	// X.509 证书
	X509Cert *x509.Certificate
	
	// ICCOA 特定字段
	Type        CertType  // 证书类型
	Mode        CertMode  // 证书模式 (CA/非CA)
	VehicleOemID string   // 车企唯一标识符 (2字节十六进制)
	VehicleID   string    // 车辆唯一标识符 (16字节十六进制)
	KeyID       string    // 数字车钥匙唯一标识符 (16字节十六进制)
	Permissions uint32    // 数字车钥匙权限
	
	// 原始数据
	DERData  []byte // DER 编码
	PEMData  []byte // PEM 编码
}

// ICCOACSR ICCOA 证书签名请求
type ICCOACSR struct {
	CSR        *x509.CertificateRequest
	Type       CertType
	VehicleID  string
	KeyID      string
	PublicKey  *ecdsa.PublicKey
	DERData    []byte
}

// CertificateChain 证书链
type CertificateChain struct {
	RootCA      *ICCOACertificate   // 根 CA 证书
	Intermediate []*ICCOACertificate // 中间证书列表
	EndEntity   *ICCOACertificate   // 终端实体证书
}

// CertKeyPair 证书密钥对
type CertKeyPair struct {
	Certificate *ICCOACertificate
	PrivateKey  *ecdsa.PrivateKey
	PublicKey   *ecdsa.PublicKey
}

// VehicleOemCAConfig 车服务器 CA 配置
type VehicleOemCAConfig struct {
	VehicleOemID    string        // 车企唯一标识符 (2字节十六进制)
	ValidityPeriod  time.Duration // 有效期
	CommonName      string        // CN
	Organization    string        // O
	Country         string        // C
}

// VehicleCertConfig 车证书配置
type VehicleCertConfig struct {
	VehicleID       string        // 车辆唯一标识符 (16字节十六进制)
	VehicleOemID    string        // 车企唯一标识符
	ValidityPeriod  time.Duration // 有效期
}

// OwnerKeyCertConfig 车主钥匙证书配置
type OwnerKeyCertConfig struct {
	KeyID           string        // 钥匙唯一标识符 (16字节十六进制)
	VehicleID       string        // 车辆唯一标识符
	VehicleOemID    string        // 车企唯一标识符
	ValidityPeriod  time.Duration // 有效期
	Permissions     uint32        // 权限位图
	Mode            CertMode      // CA 模式或非 CA 模式
}

// MidShareCertConfig 中间分享证书配置
type MidShareCertConfig struct {
	VehicleID       string        // 车辆唯一标识符
	VehicleOemID    string        // 车企唯一标识符
	MaxValidTime    time.Time     // 好友钥匙有效期最大截止时间
	ValidityPeriod  time.Duration // 有效期 (不超过90天)
}

// SharedKeyCertConfig 好友钥匙证书配置
type SharedKeyCertConfig struct {
	KeyID           string        // 钥匙唯一标识符
	VehicleID       string        // 车辆唯一标识符
	VehicleOemID    string        // 车企唯一标识符
	ValidityPeriod  time.Duration // 有效期
	Permissions     uint32        // 权限位图 (子集)
	Mode            CertMode      // CA 模式或非 CA 模式
}

// SubjectInfo 证书主题信息
type SubjectInfo struct {
	CommonName         string
	Organization       string
	OrganizationalUnit string
	Country            string
	Locality           string
	Province           string
}

// ToPKIXName 转换为 pkix.Name
func (s *SubjectInfo) ToPKIXName() pkix.Name {
	return pkix.Name{
		CommonName:         s.CommonName,
		Organization:       []string{s.Organization},
		OrganizationalUnit: []string{s.OrganizationalUnit},
		Country:            []string{s.Country},
		Locality:           []string{s.Locality},
		Province:           []string{s.Province},
	}
}

// ICCOAKeyUsage 扩展项 (ICCOA 定义)
type ICCOAKeyUsage struct {
	KeyCertSign      bool // 证书签发
	DigitalSignature bool // 数字签名
}

// ICCOABasicConstraints 基本约束扩展
type ICCOABasicConstraints struct {
	IsCA       bool
	MaxPathLen int  // -1 表示不限制, 0/1/2 表示中间证书层数
}

// ICCOAExtension ICCOA 自定义扩展
type ICCOAExtension struct {
	OID      asn1.ObjectIdentifier
	Critical bool
	Value    []byte
}

// EncodeICCOAExtensionValue 编码 ICCOA 扩展值 (OCTET STRING)
func EncodeICCOAExtensionValue(data []byte) ([]byte, error) {
	// ICCOA 扩展值使用 OCTET STRING 包装
	value := struct {
		Data []byte `asn1:"octet"`
	}{
		Data: data,
	}
	return asn1.Marshal(value)
}
