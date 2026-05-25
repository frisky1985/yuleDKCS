// Package iccoacert ICCOA DK 4.0 证书服务
package iccoacert

import (
	"context"
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rand"
	"encoding/hex"
	"fmt"
	"time"
)

// CertService 证书服务接口
type CertService interface {
	// CA 管理
	CreateVehicleOemCA(ctx context.Context, vehicleOemID string) (*CertKeyPair, error)
	GetVehicleOemCA(ctx context.Context, vehicleOemID string) (*ICCOACertificate, error)

	// 车证书管理
	CreateVehicleCert(ctx context.Context, vehicleID, vehicleOemID string) (*CertKeyPair, error)
	GetVehicleCert(ctx context.Context, vehicleID string) (*ICCOACertificate, error)

	// 车主钥匙证书管理
	CreateOwnerKeyCert(ctx context.Context, config OwnerKeyCertConfig, userID uint) (*CertKeyPair, error)
	GetOwnerKeyCert(ctx context.Context, keyID string) (*ICCOACertificate, error)

	// 分享钥匙证书管理
	CreateShareKeyCerts(ctx context.Context, ownerKeyID string, friendPubKey *ecdsa.PublicKey, validityPeriod time.Duration) (*ShareCertResult, error)
	ValidateSharedKeyCert(ctx context.Context, cert *ICCOACertificate, mode CertMode) error

	// 证书验证
	ValidateCertChain(ctx context.Context, chain *CertificateChain) error
	ValidateCertificate(ctx context.Context, cert *ICCOACertificate, caCert *ICCOACertificate) error

	// 证书撤销
	RevokeCertificate(ctx context.Context, certID string, reason string) error
}

// ShareCertResult 分享证书创建结果
type ShareCertResult struct {
	MidShareCert  *ICCOACertificate
	FriendKeyPair *CertKeyPair
}

// certService 证书服务实现
type certService struct {
	store     CertStore
	validator *CertValidator
	generator *CertGenerator
}

// NewCertService 创建证书服务实例
func NewCertService(store CertStore) CertService {
	return &certService{
		store:     store,
		validator: NewCertValidator(),
		generator: NewCertGenerator(),
	}
}

// CreateVehicleOemCA 创建车服务器 CA
func (s *certService) CreateVehicleOemCA(ctx context.Context, vehicleOemID string) (*CertKeyPair, error) {
	// 检查是否已存在
	existingCA, err := s.store.GetCAByVehicleOemID(ctx, vehicleOemID)
	if err == nil && existingCA != nil {
		return nil, fmt.Errorf("CA already exists for vehicle OEM ID: %s", vehicleOemID)
	}

	// 生成密钥对
	privKey, err := s.generator.GenerateECKeyPair()
	if err != nil {
		return nil, fmt.Errorf("failed to generate key pair: %w", err)
	}

	// 配置
	config := &VehicleOemCAConfig{
		VehicleOemID:   vehicleOemID,
		ValidityPeriod: VehicleOemCADefaultValidity,
		CommonName:     fmt.Sprintf("VEHICLE-OEM-CA-%s", vehicleOemID),
		Organization:   "Vehicle OEM",
		Country:        "CN",
	}

	// 生成证书
	keyPair, err := s.generator.GenerateVehicleOemCACert(config, privKey)
	if err != nil {
		return nil, fmt.Errorf("failed to generate CA certificate: %w", err)
	}

	// 存储证书
	_, err = s.store.StoreCA(ctx, keyPair.Certificate)
	if err != nil {
		return nil, fmt.Errorf("failed to store CA certificate: %w", err)
	}

	// 添加到验证器的可信任列表
	s.validator.AddTrustedCA(keyPair.Certificate)

	return keyPair, nil
}

// GetVehicleOemCA 获取车服务器 CA
func (s *certService) GetVehicleOemCA(ctx context.Context, vehicleOemID string) (*ICCOACertificate, error) {
	return s.store.GetCAByVehicleOemID(ctx, vehicleOemID)
}

// CreateVehicleCert 创建车证书
func (s *certService) CreateVehicleCert(ctx context.Context, vehicleID, vehicleOemID string) (*CertKeyPair, error) {
	// 检查是否已存在
	existingCert, err := s.store.GetVehicleCert(ctx, vehicleID)
	if err == nil && existingCert != nil {
		return nil, fmt.Errorf("vehicle certificate already exists: %s", vehicleID)
	}

	// 获取 CA 证书
	caCert, err := s.store.GetCAByVehicleOemID(ctx, vehicleOemID)
	if err != nil {
		return nil, fmt.Errorf("CA certificate not found: %w", err)
	}

	// 生成密钥对
	privKey, err := s.generator.GenerateECKeyPair()
	if err != nil {
		return nil, fmt.Errorf("failed to generate key pair: %w", err)
	}

	// 配置
	config := &VehicleCertConfig{
		VehicleID:      vehicleID,
		VehicleOemID:   vehicleOemID,
		ValidityPeriod: VehicleCertDefaultValidity,
	}

	// 生成证书 - 需要 CA 私钥，这里使用模拟
	// 实际中需要从安全存储中获取 CA 私钥
	caKeyPair, err := s.loadCAKeyPair(ctx, vehicleOemID)
	if err != nil {
		return nil, fmt.Errorf("failed to load CA key pair: %w", err)
	}

	cert, err := s.generator.GenerateVehicleCert(config, &privKey.PublicKey, caCert, caKeyPair.PrivateKey)
	if err != nil {
		return nil, fmt.Errorf("failed to generate vehicle certificate: %w", err)
	}

	// 存储证书
	_, err = s.store.StoreVehicleCert(ctx, cert, vehicleID)
	if err != nil {
		return nil, fmt.Errorf("failed to store vehicle certificate: %w", err)
	}

	return &CertKeyPair{
		Certificate: cert,
		PrivateKey:  privKey,
		PublicKey:   &privKey.PublicKey,
	}, nil
}

// GetVehicleCert 获取车证书
func (s *certService) GetVehicleCert(ctx context.Context, vehicleID string) (*ICCOACertificate, error) {
	return s.store.GetVehicleCert(ctx, vehicleID)
}

// CreateOwnerKeyCert 创建车主钥匙证书
func (s *certService) CreateOwnerKeyCert(ctx context.Context, config OwnerKeyCertConfig, userID uint) (*CertKeyPair, error) {
	// 检查是否已存在
	existingCert, err := s.store.GetOwnerKeyCert(ctx, config.KeyID)
	if err == nil && existingCert != nil {
		return nil, fmt.Errorf("owner key certificate already exists: %s", config.KeyID)
	}

	// 获取 CA 证书
	caCert, err := s.store.GetCAByVehicleOemID(ctx, config.VehicleOemID)
	if err != nil {
		return nil, fmt.Errorf("CA certificate not found: %w", err)
	}

	// 生成密钥对
	privKey, err := s.generator.GenerateECKeyPair()
	if err != nil {
		return nil, fmt.Errorf("failed to generate key pair: %w", err)
	}

	// 生成证书
	caKeyPair, err := s.loadCAKeyPair(ctx, config.VehicleOemID)
	if err != nil {
		return nil, fmt.Errorf("failed to load CA key pair: %w", err)
	}

	cert, err := s.generator.GenerateOwnerKeyCert(&config, &privKey.PublicKey, caCert, caKeyPair.PrivateKey)
	if err != nil {
		return nil, fmt.Errorf("failed to generate owner key certificate: %w", err)
	}

	// 存储证书
	_, err = s.store.StoreOwnerKeyCert(ctx, cert, config.KeyID, userID)
	if err != nil {
		return nil, fmt.Errorf("failed to store owner key certificate: %w", err)
	}

	return &CertKeyPair{
		Certificate: cert,
		PrivateKey:  privKey,
		PublicKey:   &privKey.PublicKey,
	}, nil
}

// GetOwnerKeyCert 获取车主钥匙证书
func (s *certService) GetOwnerKeyCert(ctx context.Context, keyID string) (*ICCOACertificate, error) {
	return s.store.GetOwnerKeyCert(ctx, keyID)
}

// CreateShareKeyCerts 创建分享钥匙证书
func (s *certService) CreateShareKeyCerts(ctx context.Context, ownerKeyID string, friendPubKey *ecdsa.PublicKey, validityPeriod time.Duration) (*ShareCertResult, error) {
	// 获取车主钥匙证书
	ownerCert, err := s.store.GetOwnerKeyCert(ctx, ownerKeyID)
	if err != nil {
		return nil, fmt.Errorf("owner key certificate not found: %w", err)
	}

	// 生成好友钥匙唯一标识符
	friendKeyID, err := GenerateKeyID()
	if err != nil {
		return nil, fmt.Errorf("failed to generate friend key ID: %w", err)
	}

	result := &ShareCertResult{}

	if ownerCert.Mode == CertModeCA {
		// CA 模式: 需要生成中间分享证书
		midPrivKey, err := s.generator.GenerateECKeyPair()
		if err != nil {
			return nil, fmt.Errorf("failed to generate mid-share key pair: %w", err)
		}

		midConfig := &MidShareCertConfig{
			VehicleID:      ownerCert.VehicleID,
			VehicleOemID:   ownerCert.VehicleOemID,
			ValidityPeriod: validityPeriod,
			MaxValidTime:   time.Now().Add(validityPeriod),
		}

		// 加载车主私钥
		ownerKeyPair, err := s.loadOwnerKeyPair(ctx, ownerKeyID)
		if err != nil {
			return nil, fmt.Errorf("failed to load owner key pair: %w", err)
		}

		midCert, err := s.generator.GenerateMidShareCert(midConfig, &midPrivKey.PublicKey, ownerCert, ownerKeyPair.PrivateKey)
		if err != nil {
			return nil, fmt.Errorf("failed to generate mid-share certificate: %w", err)
		}

		_, err = s.store.StoreMidShareCert(ctx, midCert, hex.EncodeToString(friendKeyID))
		if err != nil {
			return nil, fmt.Errorf("failed to store mid-share certificate: %w", err)
		}

		result.MidShareCert = midCert

		// 生成好友钥匙证书
		friendConfig := &SharedKeyCertConfig{
			KeyID:          hex.EncodeToString(friendKeyID),
			VehicleID:      ownerCert.VehicleID,
			VehicleOemID:   ownerCert.VehicleOemID,
			ValidityPeriod: validityPeriod,
			Mode:           CertModeCA,
		}

		friendCert, err := s.generator.GenerateSharedKeyCert(friendConfig, friendPubKey, midCert, midPrivKey)
		if err != nil {
			return nil, fmt.Errorf("failed to generate friend key certificate: %w", err)
		}

		_, err = s.store.StoreSharedKeyCert(ctx, friendCert, friendConfig.KeyID, 0)
		if err != nil {
			return nil, fmt.Errorf("failed to store friend key certificate: %w", err)
		}

		result.FriendKeyPair = &CertKeyPair{
			Certificate: friendCert,
			PublicKey:   friendPubKey,
		}
	} else {
		// 非 CA 模式: 直接由 CA 签发
		caCert, err := s.store.GetCAByVehicleOemID(ctx, ownerCert.VehicleOemID)
		if err != nil {
			return nil, fmt.Errorf("CA certificate not found: %w", err)
		}

		friendConfig := &SharedKeyCertConfig{
			KeyID:          hex.EncodeToString(friendKeyID),
			VehicleID:      ownerCert.VehicleID,
			VehicleOemID:   ownerCert.VehicleOemID,
			ValidityPeriod: validityPeriod,
			Mode:           CertModeNonCA,
		}

		caKeyPair, err := s.loadCAKeyPair(ctx, ownerCert.VehicleOemID)
		if err != nil {
			return nil, fmt.Errorf("failed to load CA key pair: %w", err)
		}

		friendCert, err := s.generator.GenerateSharedKeyCert(friendConfig, friendPubKey, caCert, caKeyPair.PrivateKey)
		if err != nil {
			return nil, fmt.Errorf("failed to generate friend key certificate: %w", err)
		}

		_, err = s.store.StoreSharedKeyCert(ctx, friendCert, friendConfig.KeyID, 0)
		if err != nil {
			return nil, fmt.Errorf("failed to store friend key certificate: %w", err)
		}

		result.FriendKeyPair = &CertKeyPair{
			Certificate: friendCert,
			PublicKey:   friendPubKey,
		}
	}

	return result, nil
}

// ValidateSharedKeyCert 验证好友钥匙证书
func (s *certService) ValidateSharedKeyCert(ctx context.Context, cert *ICCOACertificate, mode CertMode) error {
	var caCert *ICCOACertificate
	var midCert *ICCOACertificate
	var err error

	if mode == CertModeCA {
		// CA 模式: 验证证书链
		// 获取中间证书
		// (实际实现中需要从证书链中获取)

		// 获取 CA 证书
		caCert, err = s.store.GetCAByVehicleOemID(ctx, cert.VehicleOemID)
		if err != nil {
			return fmt.Errorf("CA certificate not found: %w", err)
		}

		return s.validator.ValidateSharedKeyCert(cert, midCert, caCert, mode)
	} else {
		// 非 CA 模式: 直接验证 CA 签名
		caCert, err = s.store.GetCAByVehicleOemID(ctx, cert.VehicleOemID)
		if err != nil {
			return fmt.Errorf("CA certificate not found: %w", err)
		}

		return s.validator.ValidateSharedKeyCert(cert, nil, caCert, mode)
	}
}

// ValidateCertChain 验证证书链
func (s *certService) ValidateCertChain(ctx context.Context, chain *CertificateChain) error {
	return s.validator.ValidateCertificateChain(chain)
}

// ValidateCertificate 验证证书
func (s *certService) ValidateCertificate(ctx context.Context, cert *ICCOACertificate, caCert *ICCOACertificate) error {
	switch cert.Type {
	case CertTypeVehicleOemCA:
		return s.validator.ValidateCACertificate(cert)
	case CertTypeVehicle:
		return s.validator.ValidateCACertificate(cert)
	case CertTypeOwnerDK:
		return s.validator.ValidateOwnerKeyCert(cert, caCert)
	case CertTypeMidShare:
		return s.validator.ValidateMidShareCert(cert, caCert)
	case CertTypeSharedDK, CertTypeSharedDKV2:
		return s.validator.ValidateSharedKeyCert(cert, nil, caCert, cert.Mode)
	default:
		return fmt.Errorf("unknown certificate type: %d", cert.Type)
	}
}

// RevokeCertificate 撤销证书
func (s *certService) RevokeCertificate(ctx context.Context, certID string, reason string) error {
	return s.store.RevokeCertificate(ctx, certID, reason)
}

// loadCAKeyPair 加载 CA 密钥对
// NOTE: 生产环境应从 HSM 或安全存储中加载，当前仅用于开发/测试
func (s *certService) loadCAKeyPair(ctx context.Context, vehicleOemID string) (*CertKeyPair, error) {
	caCert, err := s.store.GetCAByVehicleOemID(ctx, vehicleOemID)
	if err != nil {
		return nil, fmt.Errorf("CA certificate not found: %w", err)
	}

	// 开发模式：生成临时密钥对用于证书签发
	// NOTE: 替换为从 HSM/SE050 加载真实 CA 私钥
	privateKey, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	if err != nil {
		return nil, fmt.Errorf("failed to generate CA key pair: %w", err)
	}

	return &CertKeyPair{
		Certificate: caCert,
		PrivateKey:  privateKey,
		PublicKey:   &privateKey.PublicKey,
	}, nil
}

// loadOwnerKeyPair 加载车主密钥对
// NOTE: 生产环境应从安全存储中加载，当前仅用于开发/测试
func (s *certService) loadOwnerKeyPair(ctx context.Context, keyID string) (*CertKeyPair, error) {
	ownerCert, err := s.store.GetOwnerKeyCert(ctx, keyID)
	if err != nil {
		return nil, fmt.Errorf("owner key certificate not found: %w", err)
	}

	// 开发模式：生成临时密钥对用于证书签发
	// NOTE: 替换为从手机安全环境(SE/TEE)加载真实车主私钥
	privateKey, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	if err != nil {
		return nil, fmt.Errorf("failed to generate owner key pair: %w", err)
	}

	return &CertKeyPair{
		Certificate: ownerCert,
		PrivateKey:  privateKey,
		PublicKey:   &privateKey.PublicKey,
	}, nil
}

// BuildCertChain 构建证书链
func (s *certService) BuildCertChain(ctx context.Context, endEntityCert *ICCOACertificate) (*CertificateChain, error) {
	chain := &CertificateChain{
		EndEntity: endEntityCert,
	}

	// 获取根 CA
	caCert, err := s.store.GetCAByVehicleOemID(ctx, endEntityCert.VehicleOemID)
	if err != nil {
		return nil, fmt.Errorf("CA certificate not found: %w", err)
	}
	chain.RootCA = caCert

	// 根据证书类型构建中间证书链
	switch endEntityCert.Type {
	case CertTypeSharedDK:
		// CA 模式分享钥匙: 需要中间分享证书
		// (实际实现中需要从存储中获取)
	}

	return chain, nil
}

// ExportCertToPEM 导出证书为 PEM 格式
func (s *certService) ExportCertToPEM(cert *ICCOACertificate) string {
	return string(cert.PEMData)
}

// ExportCertToDER 导出证书为 DER 格式
func (s *certService) ExportCertToDER(cert *ICCOACertificate) []byte {
	return cert.DERData
}

// GetCertInfo 获取证书信息
func (s *certService) GetCertInfo(cert *ICCOACertificate) map[string]interface{} {
	return map[string]interface{}{
		"type":           cert.Type,
		"mode":           cert.Mode,
		"vehicle_oem_id": cert.VehicleOemID,
		"vehicle_id":     cert.VehicleID,
		"key_id":         cert.KeyID,
		"subject":        cert.X509Cert.Subject.String(),
		"issuer":         cert.X509Cert.Issuer.String(),
		"serial_number":  cert.X509Cert.SerialNumber.String(),
		"not_before":     cert.X509Cert.NotBefore,
		"not_after":      cert.X509Cert.NotAfter,
		"size":           len(cert.DERData),
	}
}

// ParseCertificate 解析证书
func (s *certService) ParseCertificate(derData []byte) (*ICCOACertificate, error) {
	return ParseICCOACertificate(derData)
}
