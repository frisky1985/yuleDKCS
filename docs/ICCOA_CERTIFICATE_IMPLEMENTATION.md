# ICCOA DK 4.0 证书系统实现总结

## 实现概述

为 yuleDKCS 数字钥匙系统实现了完整的 ICCOA/T 002-2024 标准证书管理系统，支持完整的证书生命周期管理。

## 项目结构

```
backend/internal/iccoa/cert/
├── models.go              # 证书模型定义 (204行)
├── generator.go           # 证书生成器 (757行)
├── generator_test.go      # 生成器测试 (475行)
├── validator.go           # 证书验证器 (384行)
├── validator_test.go      # 验证器测试 (447行)
├── service.go             # 证书服务层 (432行)
├── store.go               # 证书存储层 (796行)
├── example_test.go        # 使用示例 (270行)
├── migration.sql          # 数据库迁移脚本 (7095字节)
└── README.md              # 使用文档 (6266字节)
```

**总代码量:** 约 4,165 行 Go 代码 + 测试

## 功能特性

### 1. 支持的证书类型

| 类型 | 代码 | 说明 | 大小限制 |
|------|------|------|----------|
| A | CertTypeVehicleOemCA (0x01) | 车服务器 CA 证书 | 无限制 |
| B | CertTypeVehicle (0x02) | 车证书 | 无限制 |
| C | CertTypeOwnerDK (0x03) | 车主数字车钥匙证书 | ≤700字节 |
| D | CertTypeMidShare (0x04) | 中间分享证书 | ≤700字节 |
| E | CertTypeSharedDK (0x05) | 好友数字车钥匙证书 | ≤700字节 |
| K | CertTypeSharedDKV2 (0x0B) | 好友证书 V2 (非CA模式) | ≤800字节 |

### 2. 证书模式

- **CA模式 (0x00)**: 传统PKI证书链模式
  - 完整证书链: CA → Owner → MidShare → Friend
  - 符合标准X.509规范
  
- **非CA模式 (0x01)**: ICCOA 4.0 新增简化模式
  - 短证书链: CA → Owner, CA → Friend
  - 好友证书可直接由CA签发
  - 证书大小可放宽至800字节

### 3. 密码学标准

- **算法**: ECDSA P-256 (secp256r1)
- **哈希**: SHA-256
- **标准**: X.509 v3
- **编码**: DER (APDU传输), PEM (服务器间传输)

### 4. 证书有效期

| 证书类型 | 有效期 |
|----------|--------|
| 车服务器 CA | 30年 |
| 车证书 | 20年 |
| 车主钥匙 | 5年 |
| 中间分享 | ≤90天 |
| 好友钥匙 | 默认30天 |

### 5. ICCOA OID 定义

```
1.3.6.1.4.1.59129 - ICCOA 私有企业 OID
  ├── .1.1 - 车服务器CA证书类型
  ├── .1.2 - 车证书类型
  ├── .1.3 - 车主钥匙证书类型
  ├── .1.4 - 中间分享证书类型
  ├── .1.5 - 好友钥匙证书类型
  ├── .2.1 - 车辆唯一标识符
  ├── .2.2 - 钥匙唯一标识符
  ├── .2.4 - 钥匙权限
  ├── .2.5 - 车企唯一标识符
  └── .2.10 - 证书体系模式
```

## 核心组件

### CertGenerator (证书生成器)

```go
func NewCertGenerator() *CertGenerator

// 生成CA证书
func (cg *CertGenerator) GenerateVehicleOemCACert(config *VehicleOemCAConfig, caPrivKey *ecdsa.PrivateKey) (*CertKeyPair, error)

// 生成车辆证书
func (cg *CertGenerator) GenerateVehicleCert(config *VehicleCertConfig, vehiclePubKey *ecdsa.PublicKey, caCert *ICCOACertificate, caPrivKey *ecdsa.PrivateKey) (*ICCOACertificate, error)

// 生成车主钥匙证书
func (cg *CertGenerator) GenerateOwnerKeyCert(config *OwnerKeyCertConfig, ownerPubKey *ecdsa.PublicKey, caCert *ICCOACertificate, caPrivKey *ecdsa.PrivateKey) (*ICCOACertificate, error)

// 生成中间分享证书
func (cg *CertGenerator) GenerateMidShareCert(config *MidShareCertConfig, midPubKey *ecdsa.PublicKey, ownerCert *ICCOACertificate, ownerPrivKey *ecdsa.PrivateKey) (*ICCOACertificate, error)

// 生成好友钥匙证书
func (cg *CertGenerator) GenerateSharedKeyCert(config *SharedKeyCertConfig, friendPubKey *ecdsa.PublicKey, signerCert *ICCOACertificate, signerPrivKey *ecdsa.PrivateKey) (*ICCOACertificate, error)

// 生成EC密钥对 (P-256)
func (cg *CertGenerator) GenerateECKeyPair() (*ecdsa.PrivateKey, error)
```

### CertValidator (证书验证器)

```go
func NewCertValidator() *CertValidator

// 添加可信CA
func (cv *CertValidator) AddTrustedCA(cert *ICCOACertificate) error

// 验证CA证书
func (cv *CertValidator) ValidateCACertificate(cert *ICCOACertificate) error

// 验证车主钥匙证书
func (cv *CertValidator) ValidateOwnerKeyCert(cert *ICCOACertificate, caCert *ICCOACertificate) error

// 验证中间分享证书
func (cv *CertValidator) ValidateMidShareCert(cert *ICCOACertificate, ownerCert *ICCOACertificate) error

// 验证好友钥匙证书
func (cv *CertValidator) ValidateSharedKeyCert(cert *ICCOACertificate, midCert *ICCOACertificate, caCert *ICCOACertificate, mode CertMode) error

// 验证证书链
func (cv *CertValidator) ValidateCertificateChain(chain *CertificateChain) error
```

### CertService (证书服务)

```go
func NewCertService(store CertStore) CertService

// CA管理
func (s *certService) CreateVehicleOemCA(ctx context.Context, vehicleOemID string) (*CertKeyPair, error)
func (s *certService) GetVehicleOemCA(ctx context.Context, vehicleOemID string) (*ICCOACertificate, error)

// 车证书管理
func (s *certService) CreateVehicleCert(ctx context.Context, vehicleID, vehicleOemID string) (*CertKeyPair, error)

// 车主钥匙证书管理
func (s *certService) CreateOwnerKeyCert(ctx context.Context, config OwnerKeyCertConfig, userID uint) (*CertKeyPair, error)

// 分享钥匙证书管理
func (s *certService) CreateShareKeyCerts(ctx context.Context, ownerKeyID string, friendPubKey *ecdsa.PublicKey, validityPeriod time.Duration) (*ShareCertResult, error)
```

### CertStore (证书存储)

```go
func NewCertStore(db *sql.DB) CertStore

// CA证书管理
func (s *certStore) StoreCA(ctx context.Context, cert *ICCOACertificate) (string, error)
func (s *certStore) GetCAByVehicleOemID(ctx context.Context, vehicleOemID string) (*ICCOACertificate, error)

// 车主钥匙证书管理
func (s *certStore) StoreOwnerKeyCert(ctx context.Context, cert *ICCOACertificate, keyID string, userID uint) (string, error)
func (s *certStore) GetOwnerKeyCert(ctx context.Context, keyID string) (*ICCOACertificate, error)

// 好友钥匙证书管理
func (s *certStore) StoreSharedKeyCert(ctx context.Context, cert *ICCOACertificate, keyID string, friendUserID uint) (string, error)
func (s *certStore) GetSharedKeyCert(ctx context.Context, keyID string) (*ICCOACertificate, error)

// 证书链管理
func (s *certStore) StoreCertChain(ctx context.Context, chain *CertificateChain, entityID string) error
func (s *certStore) GetCertChain(ctx context.Context, entityID string) (*CertificateChain, error)

// 证书撤销
func (s *certStore) RevokeCertificate(ctx context.Context, certID string, reason string) error
func (s *certStore) IsRevoked(ctx context.Context, certID string) (bool, error)
```

## 测试覆盖率

### 生成器测试
- ✅ 车服务器CA证书生成
- ✅ 车主钥匙证书生成 (CA/非CA模式)
- ✅ 中间分享证书生成
- ✅ 好友钥匙证书生成 (CA/非CA模式)
- ✅ 证书大小限制验证
- ✅ 证书有效期验证

### 验证器测试
- ✅ CA证书验证
- ✅ 车主钥匙证书验证
- ✅ 中间分享证书验证
- ✅ 好友钥匙证书验证 (CA/非CA模式)
- ✅ 证书链验证
- ✅ 过期证书检测
- ✅ 超大小证书检测

## 使用示例

### 创建CA证书
```go
generator := iccoacert.NewCertGenerator()
privKey, _ := generator.GenerateECKeyPair()

config := &iccoacert.VehicleOemCAConfig{
    VehicleOemID:   "0010",
    CommonName:     "MY-OEM-CA",
    Organization:   "My Vehicle OEM",
    Country:        "CN",
    ValidityPeriod: iccoacert.VehicleOemCADefaultValidity,
}

caKeyPair, _ := generator.GenerateVehicleOemCACert(config, privKey)
```

### 创建车主钥匙证书 (推荐非CA模式)
```go
ownerPrivKey, _ := generator.GenerateECKeyPair()
ownerKeyID, _ := iccoacert.GenerateKeyID()

ownerConfig := &iccoacert.OwnerKeyCertConfig{
    KeyID:          hex.EncodeToString(ownerKeyID),
    VehicleID:      "112233445566778899AABBCCDDEEFF00",
    VehicleOemID:   "0010",
    ValidityPeriod: iccoacert.OwnerCertDefaultValidity,
    Permissions:    0xFFFFFFFF,
    Mode:           iccoacert.CertModeNonCA,  // 使用非CA模式
}

ownerCert, _ := generator.GenerateOwnerKeyCert(
    ownerConfig,
    &ownerPrivKey.PublicKey,
    caKeyPair.Certificate,
    caKeyPair.PrivateKey,
)
```

### 创建好友钥匙证书 (非CA模式)
```go
friendConfig := &iccoacert.SharedKeyCertConfig{
    KeyID:          hex.EncodeToString(friendKeyID),
    VehicleID:      vehicleID,
    VehicleOemID:   "0010",
    ValidityPeriod: 7 * 24 * time.Hour,
    Permissions:    0x0000000F,
    Mode:           iccoacert.CertModeNonCA,
}

friendCert, _ := generator.GenerateSharedKeyCert(
    friendConfig,
    friendPubKey,
    caKeyPair.Certificate,
    caKeyPair.PrivateKey,
)
```

### 验证证书
```go
validator := iccoacert.NewCertValidator()
validator.AddTrustedCA(caCert)

// 验证车主证书
err := validator.ValidateOwnerKeyCert(ownerCert, caCert)

// 验证好友证书 (非CA模式)
err = validator.ValidateSharedKeyCert(friendCert, nil, caCert, iccoacert.CertModeNonCA)
```

## 数据库表结构

### iccoa_certificates
存储所有证书信息，包含字段:
- id, type, mode, vehicle_oem_id, vehicle_id, key_id
- serial_number, subject, issuer
- not_before, not_after, der_data, pem_data
- status, revoked_at, revoke_reason

### iccoa_cert_chains
存储证书链关系，支持证书链的快速检索

### iccoa_cert_revocations
证书撤销列表(CRL)，记录所有被撤销的证书

## 合规性

本实现符合以下标准要求:
- ✅ **ICCOA/T 002-2024** - 智慧车联产业生态联盟数字车钥匙规范
- ✅ **X.509 v3** - ITU-T X.509 证书标准
- ✅ **ECDSA P-256** - NIST FIPS 186-4 椭圆曲线数字签名算法
- ✅ **SHA-256** - FIPS 180-4 安全哈希算法

## 安全考虑

1. **私钥安全**: 所有私钥应存储在HSM或安全密钥存储中
2. **CA私钥**: CA私钥必须离线存储，使用硬件保护
3. **证书验证**: 车端必须严格验证证书链和签名
4. **有效期检查**: 注意时区问题，使用UTC时间
5. **权限控制**: 分享钥匙权限必须是原钥匙权限的子集

## 后续优化建议

1. 集成HSM进行私钥管理
2. 实现证书吊销列表(CRL)分发
3. 添加证书状态在线查询(OCSP)
4. 优化证书存储性能(缓存层)
5. 添加证书生成性能监控
6. 实现批量证书生成API

---

**实现完成日期**: 2026-05-15  
**版本**: v1.0  
**作者**: Hermes Agent (Full-Stack Expert)
