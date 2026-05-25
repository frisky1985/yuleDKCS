# ICCOA DK 4.0 证书管理系统

## 概述

本模块实现了符合 ICCOA/T 002-2024 标准的数字钥匙证书管理系统，支持完整的证书生命周期管理。

## 证书类型

| 类型 | 代码 | 说明 | 大小限制 |
|------|------|------|----------|
| A | CertTypeVehicleOemCA (0x01) | 车服务器 CA 证书 | 无限制 |
| B | CertTypeVehicle (0x02) | 车证书 | 无限制 |
| C | CertTypeOwnerDK (0x03) | 车主数字车钥匙证书 | ≤700字节 |
| D | CertTypeMidShare (0x04) | 中间分享证书 | ≤700字节 |
| E | CertTypeSharedDK (0x05) | 好友数字车钥匙证书 | ≤700字节 |
| K | CertTypeSharedDKV2 (0x0B) | 好友证书 V2 (非CA模式) | ≤800字节 |

## 证书模式

### CA 模式 (CertModeCA = 0x00)

传统的PKI证书链模式:
```
Root CA (A) → Owner (C) → MidShare (D) → Friend (E)
```

**优点:**
- 完整的证书链验证
- 符合标准PKI规范

**缺点:**
- 证书链较长
- 中间分享证书需要KeyCertSign权限

### 非 CA 模式 (CertModeNonCA = 0x01)

ICCOA 4.0 新增的简化模式:
```
Root CA (A) → Owner (C)
Root CA (A) → Friend (K)
```

**优点:**
- 证书链更短
- 好友证书可以直接由CA签发
- 无需中间分享证书
- 好友证书大小可放宽至800字节

**缺点:**
- 需要车端支持非CA模式验证

## 证书有效期

| 证书类型 | 有效期 |
|----------|--------|
| 车服务器 CA | 30年 |
| 车证书 | 20年 |
| 车主钥匙 | 5年 |
| 中间分享 | ≤90天 |
| 好友钥匙 | 默认30天 |

## 快速开始

### 1. 创建 CA 证书

```go
generator := iccoacert.NewCertGenerator()

// 生成密钥对
privKey, _ := generator.GenerateECKeyPair()

// 配置 CA
config := &iccoacert.VehicleOemCAConfig{
    VehicleOemID:   "0010",
    CommonName:     "MY-OEM-CA",
    Organization:   "My Vehicle OEM",
    Country:        "CN",
    ValidityPeriod: iccoacert.VehicleOemCADefaultValidity,
}

// 生成 CA 证书
caKeyPair, err := generator.GenerateVehicleOemCACert(config, privKey)
if err != nil {
    log.Fatal(err)
}
```

### 2. 创建车主钥匙证书

```go
// 生成车主密钥对
ownerPrivKey, _ := generator.GenerateECKeyPair()

// 配置车主证书
ownerConfig := &iccoacert.OwnerKeyCertConfig{
    KeyID:          hex.EncodeToString(keyID),  // 16字节
    VehicleID:      hex.EncodeToString(vehicleID),  // 16字节
    VehicleOemID:   "0010",
    ValidityPeriod: iccoacert.OwnerCertDefaultValidity,
    Permissions:    0xFFFFFFFF,  // 全部权限
    Mode:           iccoacert.CertModeNonCA,  // 使用非CA模式
}

// 生成车主证书
ownerCert, err := generator.GenerateOwnerKeyCert(
    ownerConfig, 
    &ownerPrivKey.PublicKey, 
    caKeyPair.Certificate, 
    caKeyPair.PrivateKey,
)
```

### 3. 创建分享钥匙证书 (非CA模式)

```go
// 好友公钥 (从好友设备获取)
friendPubKey := ...

// 配置好友证书
friendConfig := &iccoacert.SharedKeyCertConfig{
    KeyID:          hex.EncodeToString(friendKeyID),
    VehicleID:      vehicleID,
    VehicleOemID:   "0010",
    ValidityPeriod: 7 * 24 * time.Hour,  // 7天
    Permissions:    0x0000000F,  // 部分权限
    Mode:           iccoacert.CertModeNonCA,
}

// 生成好友证书 (直接由CA签发)
friendCert, err := generator.GenerateSharedKeyCert(
    friendConfig,
    friendPubKey,
    caKeyPair.Certificate,
    caKeyPair.PrivateKey,
)
```

### 4. 验证证书

```go
validator := iccoacert.NewCertValidator()

// 添加可信CA
validator.AddTrustedCA(caCert)

// 验证车主证书
err := validator.ValidateOwnerKeyCert(ownerCert, caCert)
if err != nil {
    log.Printf("证书验证失败: %v", err)
}

// 验证好友证书 (非CA模式)
err = validator.ValidateSharedKeyCert(friendCert, nil, caCert, iccoacert.CertModeNonCA)
```

### 5. 使用证书服务

```go
// 创建证书服务
certService := iccoacert.NewCertService(certStore)

// 创建 CA
caKeyPair, err := certService.CreateVehicleOemCA(ctx, "0010")

// 创建车主证书
ownerConfig := iccoacert.OwnerKeyCertConfig{
    KeyID:        keyID,
    VehicleID:    vehicleID,
    VehicleOemID: "0010",
    Mode:         iccoacert.CertModeNonCA,
}
ownerKeyPair, err := certService.CreateOwnerKeyCert(ctx, ownerConfig, userID)

// 创建分享钥匙
result, err := certService.CreateShareKeyCerts(
    ctx,
    ownerKeyID,
    friendPubKey,
    7*24*time.Hour,  // 有效期7天
)
```

## 数据库表结构

### iccoa_certificates

存储所有证书信息:
- id: 证书唯一标识符
- type: 证书类型 (1-5, 11)
- mode: 证书模式 (0=CA, 1=非CA)
- vehicle_oem_id: 车企ID
- vehicle_id: 车辆ID
- key_id: 钥匙ID
- der_data: DER格式证书数据
- status: 证书状态

### iccoa_cert_chains

存储证书链关系:
- entity_id: 实体标识
- chain_data: JSON格式的证书链数据

### iccoa_cert_revocations

证书撤销列表(CRL):
- cert_id: 被撤销证书ID
- serial_number: 序列号
- revoked_at: 撤销时间
- reason: 撤销原因

## 安全考虑

1. **私钥安全**: 所有私钥应存储在 HSM 或安全密钥存储中
2. **CA 密钥**: CA 私钥必须离线存储，使用硬件保护
3. **证书验证**: 车端必须严格验证证书链和签名
4. **有效期检查**: 注意时区问题，使用UTC时间
5. **权限控制**: 分享钥匙权限必须是原钥匙权限的子集

## ICCOA OID 定义

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
  └── .2.10 - 证书模式
```

## 测试

```bash
# 运行所有测试
go test -v ./...

# 运行生成器测试
go test -v -run TestCertGenerator

# 运行验证器测试
go test -v -run TestCertValidator
```

## 合规性

本实现符合以下标准要求:

- **ICCOA/T 002-2024** - 智慧车联产业生态联盟数字车钥匙规范
- **X.509 v3** - ITU-T X.509 证书标准
- **ECDSA P-256** - NIST FIPS 186-4 椭圆曲线数字签名算法
- **SHA-256** - FIPS 180-4 安全哈希算法

## 许可证

MIT License
