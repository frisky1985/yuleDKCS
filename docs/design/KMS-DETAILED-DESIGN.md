# KMS 密钥管理服务详细设计

> **版本**: v1.0 | **日期**: 2026-07-27
> **基于**: PRD.md §3.3.2, SYSTEM_ARCHITECTURE.md §5, CLOUD-DEV-GUIDE.md §4.3
> **技术栈**: Go 1.22+ / HSM / HashiCorp Vault / gRPC / TiDB

---

## 1. KMS 核心职责

KMS (Key Management Service) 是 yuleDKCS 云端的安全基石，负责所有密钥材料的生命周期管理。KMS **不参与业务编排**，仅提供密码学原语和密钥存储服务。

### 1.1 核心原则

| 原则 | 说明 |
|------|------|
| **密钥材料不出 KMS** | 私钥始终在 HSM/Vault 内，应用层只拿到公钥和引用ID |
| **分层密钥体系** | 根密钥 → 主密钥 → 设备密钥 → 会话密钥，层层派生 |
| **双算法支持** | 同时支持 ICCE 国密(SM2/SM3/SM4) 和 CCC 国际(ECDSA/AES) |
| **密钥即服务** | 业务层通过 gRPC 调用 KMS，不直接访问密钥材料 |
| **可审计** | 所有密钥操作全量审计，保留 ≥ 3 年 |

### 1.2 架构定位

```
┌─────────────────────────────────────────────────────────────────┐
│                     业务服务层 (Service Layer)                     │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────────────┐   │
│  │  Key Svc │ │ Auth Svc │ │VehicleSvc│ │  Event Svc       │   │
│  └────┬─────┘ └────┬─────┘ └────┬─────┘ └────────┬─────────┘   │
│       │            │            │                 │              │
├───────┼────────────┼────────────┼─────────────────┼──────────────┤
│       │            │            │                 │              │
│       ▼            ▼            ▼                 ▼              │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                  KMS Service (密钥管理服务)                 │   │
│  │                                                          │   │
│  │  ┌──────────────────────┐  ┌────────────────────────┐   │   │
│  │  │  密钥生命周期管理     │  │  加密运算引擎           │   │   │
│  │  │  · 生成/导入/轮换    │  │  · 签名/验签            │   │   │
│  │  │  · 派生/分发          │  │  · 加密/解密            │   │   │
│  │  │  · 吊销/销毁          │  │  · 密钥派生(HKDF)      │   │   │
│  │  └──────────────────────┘  └────────────────────────┘   │   │
│  │                                                          │   │
│  │  ┌──────────────────────┐  ┌────────────────────────┐   │   │
│  │  │  CA 证书服务         │  │  密钥存储后端           │   │   │
│  │  │  · 国密 SM2 证书     │  │  · HSM (硬件密钥)       │   │   │
│  │  │  · X.509 ECDSA 证书  │  │  · Vault (软件密钥)     │   │   │
│  │  │  · CRL/OCSP          │  │  · SE050 代理          │   │   │
│  │  └──────────────────────┘  └────────────────────────┘   │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘

Depends on:
  ┌──────────┐  ┌──────────┐  ┌──────────┐
  │  TiDB    │  │  Redis   │  │  Kafka   │
  │ (密钥元  │  │ (缓存    │  │ (事件    │
  │  数据)   │  │  密钥ID) │  │  通知)   │
  └──────────┘  └──────────┘  └──────────┘
```

---

## 2. 密钥层级体系

### 2.1 四层密钥结构

```
┌─────────────────────────────────────────────────────────────────┐
│  Level 0: Root Key (根密钥)                                     │
│  ├── 存储位置: HSM / SE050                                        │
│  ├── 长度: AES-256 / SM4-128                                     │
│  ├── 用途: 派生所有下级密钥                                      │
│  ├── 可读性: ❌ 永远不可读取                                      │
│  ├── 轮换: 仅在安全事件后                                         │
│  └── 每个车辆/HSM 独立根密钥                                      │
│                                                                   │
│  Level 1: Master Key (主密钥)                                    │
│  ├── 存储位置: HSM / Vault                                        │
│  ├── 算法: AES-256-GCM / SM4-CTR                                 │
│  ├── 用途: 车辆级别密钥加密                                       │
│  ├── 派生: HKDF(RootKey, "master", salt, info)                   │
│  ├── 可读性: ❌ 不可读取明文                                       │
│  └── 轮换: 每6个月或安全事件后                                    │
│                                                                   │
│  Level 2: Device Key (设备密钥)                                  │
│  ├── 存储位置: HSM (云端引用) / 手机 SE/Keychain (设备端)         │
│  ├── 算法: ECDSA P-256 / SM2                                     │
│  ├── 类型: 非对称密钥对                                           │
│  ├── 用途: 设备身份认证                                           │
│  ├── 派生: HKDF(MasterKey, device_id + user_id)                  │
│  ├── 公钥: 可读取，用于验证签名                                    │
│  ├── 私钥: ❌ 云端不存储（由设备 SE 生成）或 HSM 管理               │
│  └── 轮换: 每12个月或设备重新配对时                                │
│                                                                   │
│  Level 3: Session Key (会话密钥)                                 │
│  ├── 存储位置: RAM (运行时)                                        │
│  ├── 算法: AES-256-GCM (CCC) / SM4-CTR (ICCE)                   │
│  ├── 用途: 单次会话通信加密                                       │
│  ├── 派生: ECDH(DeviceKey_Local, DeviceKey_Remote) || HKDF      │
│  ├── 可读性: 仅当前会话可用                                        │
│  └── 生命周期: 单次连接，断开即销毁                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 密钥类型

| 类型 | 层级 | 算法(ICCE) | 算法(CCC) | 存储位置 |
|------|------|-----------|----------|---------|
| 根密钥 (RK) | L0 | SM4-128 | AES-256 | HSM |
| 主密钥 (MK) | L1 | SM4-128 | AES-256 | HSM |
| 车辆身份密钥 (VIK) | L2 | SM2-256 | ECDSA P-256 | SE050(车端) |
| 设备身份密钥 (DIK) | L2 | SM2-256 | ECDSA P-256 | 手机SE/Keychain |
| 签名密钥 (SK) | L2 | SM2-256 | ECDSA P-256 | HSM/Vault |
| 会话密钥 (SessionK) | L3 | SM4-128 | AES-256 | RAM (临时) |
| MAC 密钥 (MAC-K) | L3 | SM3 HMAC | HMAC-SHA256 | RAM (临时) |

### 2.3 密钥派生关系

```
RootKey (HSM)
  │
  ├── HKDF(,"master") → MasterKey (车辆级)
  │     │
  │     ├── HKDF(,device_id+user_id) → DeviceKey (设备级)
  │     │     │
  │     │     └── ECDH(DeviceKey_A, DeviceKey_B) → SharedSecret
  │     │           │
  │     │           └── HKDF(,"session",nonce) → SessionKey (会话级)
  │     │
  │     └── HKDF(,"signing") → SigningKey (签名用)
  │
  └── HKDF(,"backup") → BackupKey (备份用)
```

---

## 3. 密钥生命周期管理

### 3.1 生命周期状态机

```
      ┌──────────┐
      │ Pending  │  ← 预生成（未分配）
      └────┬─────┘
           │ 分配
           ▼
      ┌──────────┐
      │ Active   │  ← 正常使用
      └────┬─────┘
           │
     ┌─────┼─────┐
     │     │     │
     ▼     ▼     ▼
  ┌────┐ ┌────┐ ┌──────┐
  │Rot │ │Comp│ │Suspen│  ← 轮换/泄密/挂起
  │ated│ │romi│ │ded   │
  └─┬──┘ │sed │ └──┬───┘
    │    └──┬──┘    │
    │       │       │
    └───────┼───────┘
            │
            ▼
       ┌──────────┐
       │ Revoked  │  ← 吊销（不可逆）
       └────┬─────┘
            │ 保留期
            ▼
       ┌──────────┐
       │ Destroyed│  ← 安全销毁（元数据保留）
       └──────────┘
```

### 3.2 密钥生成

```go
// 密钥生成流程

// ICCE 模式 (SM2)
func (s *KMSService) GenerateSM2KeyPair(ctx context.Context, req *GenKeyRequest) (*GenKeyResponse, error) {
    // 1. 在 HSM 内生成 SM2 密钥对
    keyPair, err := s.hsm.GenerateKeyPair(ctx, &hsm.KeyGenRequest{
        Algorithm: hsm.SM2,
        Label:     fmt.Sprintf("device_key_%s_%s", req.UserID, req.DeviceID),
        Policy: &hsm.KeyPolicy{
            CanExport: false,       // 私钥不可导出
            CanSign:   true,
            CanVerify: true,
            NeedAuth:  true,        // 签名需要额外授权
        },
    })
    if err != nil {
        return nil, ErrKeyGeneration
    }

    // 2. 签发国密证书
    cert, err := s.ca.IssueCertificate(ctx, &IssueCertRequest{
        Subject:    req.UserID,
        PublicKey:  keyPair.PublicKey,
        Algorithm:  "SM2",
        Validity:   365 * 24 * time.Hour, // 1年
        Profile:    "icce-device",
    })

    // 3. 保存密钥元数据（不含私钥）
    metadata := &KeyMetadata{
        KeyID:        keyPair.KeyID,
        Algorithm:    "SM2",
        Level:        KeyLevelDevice,
        Status:       KeyStatusActive,
        PublicKey:    keyPair.PublicKey,
        CertSerial:   cert.Serial,
        OwnerID:      req.UserID,
        AssociatedID: req.DeviceID,
        CreatedAt:    time.Now(),
        ExpiresAt:    time.Now().Add(365 * 24 * time.Hour),
    }
    s.repo.SaveKeyMetadata(ctx, metadata)

    return &GenKeyResponse{
        KeyID:      keyPair.KeyID,
        PublicKey:  keyPair.PublicKey,
        CertSerial: cert.Serial,
    }, nil
}

// CCC 模式 (ECDSA P-256)
func (s *KMSService) GenerateECDSAKeyPair(ctx context.Context, req *GenKeyRequest) (*GenKeyResponse, error) {
    // Vault 作为软件 HSM 模式
    keyPath := fmt.Sprintf("digitalkey/keys/%s/%s", req.UserID, req.DeviceID)
    keyResp, err := s.vault.GenerateKey(ctx, keyPath, &vault.GenKeyRequest{
        Type:        vault.KeyType_ECDSA,
        Curve:       "P-256",
        Exportable:  false,
        RenewBefore: 30 * 24 * time.Hour,
    })
    // ... 同 SM2 流程
}
```

### 3.3 密钥分发

```
Key Generation Flow
====================
KMS                                  HSM/Vault
 │                                      │
 │  1. GenerateKeyPair                   │
 │ ───────────────────────────────────▶  │
 │                                      │ 2. HSM 内部生成密钥对
 │                                      │    私钥: HSM 内部存储
 │                                      │    公钥: 返回
 │  3. KeyID + PublicKey                │
 │ ◀───────────────────────────────────  │
 │                                      │
 │  4. 签发证书 (CA)                     │
 │     CertSerial + CertData            │
 │                                      │
 │  5. 保存 KeyMetadata 到 TiDB          │
 │     (KeyID, PublicKey, CertSerial,   │
 │      Algorithm, Status, Owner)       │
 │                                      │
 │  6. 返回 KeyID + PublicKey + Cert     │
 │     (私钥❌ 不出 KMS)                 │
 ▼                                      ▼
Key Distribution Flow
=====================
KMS → DKCS → Hub → 手机App
 │
 ├─ 公钥 + 证书 → Hub → App SDK
 │   用于手机端验证车端身份
 │
 ├─ KeyID → DKCS (元数据引用)
 │   车端用 KeyID 在 KMS 中引用密钥
 │
 └─ 私钥: ❌ 永不分发
     车端密钥：SE050 内部生成
     手机密钥：手机 SE/Keychain 内部生成
     云端 KMS：仅持有车厂主密钥/MAC密钥
```

### 3.4 密钥轮换

```go
type KeyRotationPolicy struct {
    // 自动轮换间隔
    Interval time.Duration

    // 轮换前多久开始预生成新密钥
    PreRotationWindow time.Duration

    // 新旧密钥重叠期（允许同时使用新旧密钥完成握手）
    GracePeriod time.Duration

    // 旧密钥保留期（允许延迟验证）
    RetentionPeriod time.Duration
}

var DefaultRotationPolicies = map[KeyLevel]KeyRotationPolicy{
    KeyLevelRoot:   {Interval: 0, PreRotationWindow: 0, GracePeriod: 0, RetentionPeriod: 0}, // 按需
    KeyLevelMaster: {Interval: 180 * 24 * time.Hour, PreRotationWindow: 7 * 24 * time.Hour, GracePeriod: 24 * time.Hour, RetentionPeriod: 30 * 24 * time.Hour},
    KeyLevelDevice: {Interval: 365 * 24 * time.Hour, PreRotationWindow: 30 * 24 * time.Hour, GracePeriod: 72 * time.Hour, RetentionPeriod: 7 * 24 * time.Hour},
    KeyLevelSession:{Interval: 0, PreRotationWindow: 0, GracePeriod: 0, RetentionPeriod: 0}, // 每次连接
}
```

**轮换流程**:
1. 监控检查密钥有效期，距过期 < PreRotationWindow 时触发
2. 预生成新密钥对（新 KeyID），保持 Active 状态
3. 通知业务层使用新密钥（通过 Kafka 事件）
4. 进入 GracePeriod，新旧密钥同时可用
5. 过期后旧密钥标记为 Rotated，进入 RetentionPeriod
6. 保留期后安全销毁（HSM 删除私钥）

### 3.5 密钥吊销

```go
func (s *KMSService) RevokeKey(ctx context.Context, req *RevokeKeyRequest) error {
    metadata, err := s.repo.GetKeyMetadata(ctx, req.KeyID)
    if err != nil {
        return ErrKeyNotFound
    }

    // 1. 更新状态
    metadata.Status = KeyStatusRevoked
    metadata.RevokedAt = time.Now()
    metadata.RevokeReason = req.Reason
    s.repo.UpdateKeyMetadata(ctx, metadata)

    // 2. 吊销证书 (CRL / OCSP)
    certSerial := metadata.CertSerial
    s.ca.RevokeCertificate(ctx, &RevokeCertRequest{
        Serial:  certSerial,
        Reason:  req.Reason,
        RevokedAt: time.Now(),
    })

    // 3. 销毁 HSM/Vault 中的私钥
    switch metadata.Algorithm {
    case "SM2":
        s.hsm.DestroyKey(ctx, metadata.KeyID)
    case "ECDSA-P256":
        s.vault.DeleteKey(ctx, "digitalkey/keys/"+metadata.KeyID)
    }

    // 4. 发布密钥吊销事件
    s.eventPub.PublishKeyEvent(ctx, &KeyEvent{
        Type:      EventKeyRevoked,
        KeyID:     metadata.KeyID,
        UserID:    metadata.OwnerID,
        Reason:    req.Reason,
        Timestamp: time.Now().Unix(),
    })

    return nil
}
```

---

## 4. 与 SE/TEE 配合方式

### 4.1 车端 SE050 配合

```
KMS (云端)                              SE050 (车端)
 │                                          │
 │  1. 签发车辆证书                          │
 │     (SM2/ECDSA, 主题=车辆VIN)             │
 │                                          │
 │  2. 证书写入 (出厂预置/OTA)               │
 │ ───────────────────────────────────────▶ │
 │                                          │ 3. SE050 内部生成
 │                                          │    车辆身份密钥对
 │                                          │    私钥永不出SE
 │  4. 公钥 + Attestation                  │
 │ ◀─────────────────────────────────────── │
 │                                          │
 │  5. 验证 Attestation                      │
 │     确认 SE050 真伪                       │
 │                                          │
 │  6. 注册车辆公钥                          │
 │     存储 KeyMetadata                     │
 │                                          │
 │  7. 后续：通过 KeyID 引用                  │
 │     车端签名 → KMS 使用存储的公钥验签      │
 │     或 KMS 签名 → 车端 SE050 验签         │
 │                                          │
```

### 4.2 手机端 SE/TEE 配合

```
KMS (云端)                             手机 SE/TEE
 │                                          │
 │  1. 不参与手机密钥生成                     │
 │     手机本地生成密钥对                     │
 │     私钥存 SE/Keychain                    │
 │     公钥上传                              │
 │                                          │
 │  2. 接收设备公钥                          │
 │     签发设备证书                          │
 │     返回证书给手机                        │
 │                                          │
 │  3. 验签操作                              │
 │     手机用私钥签名 → KMS 用公钥验签        │
 │     或 KMS 用私钥签名 → 手机用公钥验签     │
 │                                          │
```

### 4.3 SE/TEE 接口定义

```go
// SE/TEE Attestation 验证
type AttestationVerifier interface {
    // VerifyAttestation 验证 SE/TEE 证明包
    // 确认设备运行在安全环境中
    VerifyAttestation(ctx context.Context, attestation *AttestationData) (*AttestationResult, error)
}

type AttestationData struct {
    // 厂商证明格式
    Format      string   // "android_key_attestation" | "ios_attestation" | "se050"
    Challenge   []byte   // 随机挑战
    CertChain   [][]byte // 证书链
    PublicKey   []byte   // 被证明的公钥
    Signature   []byte   // 签名
}

type AttestationResult struct {
    Verified    bool     // 是否验证通过
    DeviceID    string   // 安全环境标识
    SecurityLevel string // "TEE" | "StrongBox" | "SE050"
    OSVersion   string   // 操作系统版本
}
```

---

## 5. 安全策略

### 5.1 访问控制

| 角色 | KMS 权限 |
|------|---------|
| Key Service | GenKeyPair, Sign, Verify, Encrypt, Decrypt |
| Auth Service | Encrypt(密码哈希), Decrypt |
| CA Service | IssueCert, RevokeCert |
| 运维管理员 | 密钥轮换配置、审计查询 |
| 安全管理 | 密钥吊销、安全事件响应 |
| 外部系统 | ❌ 无直接 KMS 访问 |

### 5.2 算法策略

```go
// 算法限制策略
type CryptoPolicy struct {
    // 允许的签名算法
    AllowedSignAlgorithms []string

    // 允许的加密算法
    AllowedEncAlgorithms []string

    // 最小密钥长度
    MinKeyLength int

    // 禁止的弱算法
    BlockedAlgorithms []string
}

var DefaultCryptoPolicy = CryptoPolicy{
    AllowedSignAlgorithms: []string{"SM2", "ECDSA-P256", "HMAC-SHA256", "SM3-HMAC"},
    AllowedEncAlgorithms:  []string{"AES-256-GCM", "SM4-CTR", "SM4-GCM"},
    MinKeyLength:         128,  // bits
    BlockedAlgorithms:    []string{"MD5", "SHA-1", "RC4", "DES", "3DES"},
}
```

### 5.3 审计策略

| 操作 | 审计级别 | 保留期 |
|------|---------|--------|
| 密钥生成 | 强制审计 | 永久 |
| 密钥轮换 | 强制审计 | 永久 |
| 密钥吊销 | 强制审计 | 永久 |
| 密钥销毁 | 强制审计 | 永久 |
| 签名操作 | 可选审计(1:100采样) | 3年 |
| 验签操作 | 不审计 | — |
| 加密操作 | 可选审计(1:1000采样) | 3年 |
| 解密操作 | 强制审计 | 3年 |

### 5.4 防泄漏策略

| 策略 | 实现 |
|------|------|
| 私钥不可导出 | HSM/Vault 策略 enforce |
| 密钥不落日志 | 日志过滤器中 Mask 密钥材料 |
| 内存保护 | mloc 锁定内存，禁止swap |
| RMA 防护 | 密钥使用后立即清零 |
| 侧信道防护 | HSM 硬件级防护 |
| 速率限制 | 签名/解密操作每秒不超过阈值 |

---

## 6. 接口定义

### 6.1 gRPC 服务定义

```protobuf
// api/kms/v1/kms_service.proto

syntax = "proto3";
package digitalkey.kms.v1;

service KMSService {
    // === 密钥生命周期 ===

    // 生成密钥对（SM2 / ECDSA P-256）
    rpc GenerateKeyPair(GenerateKeyPairRequest) returns (GenerateKeyPairResponse);

    // 导入密钥（从车端 SE050 导入公钥）
    rpc ImportKey(ImportKeyRequest) returns (ImportKeyResponse);

    // 获取密钥元数据
    rpc GetKeyMetadata(GetKeyMetadataRequest) returns (KeyMetadata);

    // 轮换密钥
    rpc RotateKey(RotateKeyRequest) returns (RotateKeyResponse);

    // 吊销密钥
    rpc RevokeKey(RevokeKeyRequest) returns (RevokeKeyResponse);

    // 销毁密钥
    rpc DestroyKey(DestroyKeyRequest) returns (DestroyKeyResponse);

    // === 密码学操作 ===

    // 签名
    rpc Sign(SignRequest) returns (SignResponse);

    // 验签
    rpc Verify(VerifyRequest) returns (VerifyResponse);

    // 加密
    rpc Encrypt(EncryptRequest) returns (EncryptResponse);

    // 解密
    rpc Decrypt(DecryptRequest) returns (DecryptResponse);

    // 密钥派生 (HKDF)
    rpc DeriveKey(DeriveKeyRequest) returns (DeriveKeyResponse);

    // === 证书管理 ===

    // 签发证书
    rpc IssueCertificate(IssueCertRequest) returns (IssueCertResponse);

    // 吊销证书
    rpc RevokeCertificate(RevokeCertRequest) returns (RevokeCertResponse);

    // 查询证书状态 (OCSP)
    rpc CheckCertificateStatus(CheckCertRequest) returns (CertStatusResponse);

    // === 管理 ===

    // 获取 KMS 健康状态
    rpc HealthCheck(HealthCheckRequest) returns (HealthCheckResponse);

    // 获取密钥统计信息
    rpc GetKeyStats(KeyStatsRequest) returns (KeyStatsResponse);
}

// === 消息定义 ===

message GenerateKeyPairRequest {
    string user_id = 1;
    string device_id = 2;
    string vehicle_id = 3;
    Algorithm algorithm = 4;
    CertProfile cert_profile = 5;
    int64 validity_seconds = 6;
    map<string, string> labels = 7;
}

message GenerateKeyPairResponse {
    string key_id = 1;
    string public_key = 2;      // PEM 编码
    string cert_serial = 3;
    string cert_pem = 4;
    int64 created_at = 5;
    int64 expires_at = 6;
}

message SignRequest {
    string key_id = 1;
    Algorithm algorithm = 2;
    bytes data = 3;
    bytes context = 4;          // 可选上下文绑定
}

message SignResponse {
    bytes signature = 1;
    string key_id = 2;
    int64 signed_at = 3;
}

message VerifyRequest {
    string key_id = 1;
    Algorithm algorithm = 2;
    bytes data = 3;
    bytes signature = 4;
    bytes public_key = 5;       // 可选：直接传公钥（用于设备端签名）
}

message VerifyResponse {
    bool valid = 1;
    string key_id = 2;
}

message EncryptRequest {
    string key_id = 1;
    Algorithm algorithm = 2;
    bytes plaintext = 3;
    bytes aad = 4;              // 附加认证数据
}

message EncryptResponse {
    bytes ciphertext = 1;
    bytes iv = 2;               // 初始向量
    bytes tag = 3;              // 认证标签
    string key_id = 4;
}

message DecryptRequest {
    string key_id = 1;
    Algorithm algorithm = 2;
    bytes ciphertext = 3;
    bytes iv = 4;
    bytes tag = 5;
    bytes aad = 6;
}

message DecryptResponse {
    bytes plaintext = 1;
    string key_id = 2;
}

message RotateKeyRequest {
    string key_id = 1;
    string reason = 2;
}

message RotateKeyResponse {
    string new_key_id = 1;
    string old_key_id = 2;
    int64 rotated_at = 3;
    int64 grace_period_end = 4;
}

message RevokeKeyRequest {
    string key_id = 1;
    string reason = 2;
    bool emergency = 3;
}

message RevokeKeyResponse {
    string key_id = 1;
    string status = 2;
    int64 revoked_at = 3;
}

// === 枚举 ===

enum Algorithm {
    ALGORITHM_UNSPECIFIED = 0;
    SM2 = 1;              // ICCE 国密签名
    SM3 = 2;              // ICCE 国密哈希
    SM4_CTR = 3;          // ICCE 国密对称加密 (CTR模式)
    SM4_GCM = 4;          // ICCE 国密对称加密 (GCM模式)
    ECDSA_P256 = 10;      // CCC 椭圆曲线签名
    AES_256_GCM = 11;     // CCC 对称加密
    HMAC_SHA256 = 12;     // CCC 消息认证
}

enum CertProfile {
    CERT_PROFILE_UNSPECIFIED = 0;
    ICCE_DEVICE = 1;      // ICCE 设备证书
    ICCE_VEHICLE = 2;     // ICCE 车辆证书
    CCC_DEVICE = 10;      // CCC 设备证书
    CCC_VEHICLE = 11;     // CCC 车辆证书
}
```

### 6.2 关键方法签名（Go 实现）

```go
// internal/service/kms_svc.go  — 已实现

type KMSService struct {
    hsmClient  *hsm.Client    // 硬件安全模块客户端
    vault      *vault.Client  // HashiCorp Vault 客户端
    caService  *CAService     // CA 证书服务
    keyRepo    KeyMetadataRepository
    eventPub   EventPublisher
}

// GenerateKeyPair — 已实现，支持 SM2 和 ECDSA P-256
func (s *KMSService) GenerateKeyPair(ctx context.Context, req *kms.GenKeyRequest) (*kms.GenKeyResponse, error)

// Sign — 已实现，支持 SM2 和 ECDSA 签名
func (s *KMSService) Sign(ctx context.Context, req *kms.SignRequest) (*kms.SignResponse, error)

// VerifySignature — 已实现，支持 SM2 和 ECDSA 验签
func (s *KMSService) VerifySignature(ctx context.Context, req *kms.VerifyRequest) (*kms.VerifyResponse, error)
```

### 6.3 证书 API

```go
type CAService struct {
    // ICCE CA 证书 (SM2)
    icceRootCA *x509.Certificate
    icceRootKey crypto.PrivateKey

    // CCC CA 证书 (ECDSA P-256)
    cccRootCA *x509.Certificate
    cccRootKey crypto.PrivateKey

    // CRL 管理器
    crlManager *CRLManager
}

func (s *CAService) IssueCertificate(ctx context.Context, req *IssueCertRequest) (*Certificate, error)
func (s *CAService) RevokeCertificate(ctx context.Context, req *RevokeCertRequest) error
func (s *CAService) GetCRL(ctx context.Context) (*CRL, error)
func (s *CAService) OCSPRespond(ctx context.Context, req *OCSPRequest) (*OCSPResponse, error)
```

---

## 7. 数据模型

### 7.1 密钥元数据表 (key_metadata)

| 字段 | 类型 | 说明 |
|------|------|------|
| key_id | VARCHAR(64) PK | 密钥唯一标识 |
| algorithm | VARCHAR(20) | SM2 / SM3 / SM4 / ECDSA-P256 / AES-256-GCM / HMAC-SHA256 |
| key_level | TINYINT | 0=Root / 1=Master / 2=Device / 3=Session |
| key_type | VARCHAR(20) | signing / encryption / mac / wrapping |
| status | TINYINT | 0=Pending / 1=Active / 2=Rotated / 3=Suspended / 4=Revoked / 5=Destroyed |
| public_key | TEXT | 公钥 (PEM Base64) |
| cert_serial | VARCHAR(64) | 证书序列号 |
| owner_id | VARCHAR(64) | 所有者 ID (user_id / vehicle_id) |
| associated_id | VARCHAR(64) | 关联实体 ID (device_id / tcu_id) |
| created_at | DATETIME | 创建时间 |
| activated_at | DATETIME | 激活时间 |
| rotated_at | DATETIME | 最近轮换时间 |
| revoked_at | DATETIME | 吊销时间 |
| expires_at | DATETIME | 过期时间 |
| revoke_reason | TEXT | 吊销原因 |
| labels | JSON | K-V 标签 |
| hsm_ref | VARCHAR(128) | HSM 内部引用标识 |

### 7.2 密钥版本表 (key_versions)

| 字段 | 类型 | 说明 |
|------|------|------|
| version_id | BIGINT PK | 版本ID |
| key_id | VARCHAR(64) FK | 关联密钥ID |
| version | INT | 版本号 |
| status | TINYINT | 当前/历史 |
| created_at | DATETIME | 创建时间 |
| rotated_from | VARCHAR(64) | 从哪个版本轮换 |
| hsm_key_ref | VARCHAR(128) | HSM 中该版本的引用 |

### 7.3 证书表 (certificates)

| 字段 | 类型 | 说明 |
|------|------|------|
| serial | VARCHAR(64) PK | 证书序列号 |
| profile | VARCHAR(32) | ICCE_DEVICE / ICCE_VEHICLE / CCC_DEVICE / CCC_VEHICLE |
| algorithm | VARCHAR(20) | SM2 / ECDSA-P256 |
| subject | VARCHAR(128) | 主题 (user_id / vehicle_id) |
| issuer | VARCHAR(128) | 签发者 |
| not_before | DATETIME | 生效时间 |
| not_after | DATETIME | 失效时间 |
| status | TINYINT | 0=Active / 1=Revoked / 2=Expired |
| revoked_at | DATETIME | 吊销时间 |
| revocation_reason | VARCHAR(64) | 吊销原因 |
| cert_pem | TEXT | 证书 PEM 编码 |
| crl_entry | TINYINT | 0=未加入CRL / 1=已加入CRL |

### 7.4 证书吊销列表表 (crl)

| 字段 | 类型 | 说明 |
|------|------|------|
| crl_id | BIGINT PK | CRL 条目ID |
| cert_serial | VARCHAR(64) | 吊销的证书序列号 |
| revoked_at | DATETIME | 吊销时间 |
| reason | VARCHAR(64) | 吊销原因 |
| issuer | VARCHAR(128) | 签发 CA |
| crl_number | BIGINT | CRL 序列号(单调递增) |

### 7.5 审计日志表 (kms_audit)

| 字段 | 类型 | 说明 |
|------|------|------|
| audit_id | BIGINT PK | 审计ID |
| operation | VARCHAR(32) | gen_key / sign / verify / encrypt / decrypt / rotate / revoke / destroy |
| key_id | VARCHAR(64) | 关联密钥ID |
| algorithm | VARCHAR(20) | 使用的算法 |
| caller | VARCHAR(64) | 调用方服务名 |
| caller_ip | VARCHAR(45) | 调用方IP |
| result | TINYINT | 0=Success / 1=Failure |
| error_msg | TEXT | 错误信息 |
| duration_ms | INT | 操作耗时 |
| timestamp | DATETIME | 操作时间 |

---

## 8. 部署与高可用

### 8.1 部署架构

```
┌─────────────────────────────────────────────────────────────────┐
│                     Kubernetes Cluster                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                   │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │              KMS Service (3+ replicas)                    │   │
│  │  · 无状态（密钥状态在后端HSM/Vault/DB）                   │   │
│  │  · gRPC 端口 9300                                        │   │
│  │  · 健康检查 /liveliness /readiness                        │   │
│  └──────────────────┬───────────────────────────────────────┘   │
│                      │                                           │
│  ┌───────────────────┼───────────────────────────────────┐       │
│  │                   ▼                                   │       │
│  │  ┌──────────────────────────────────────────────┐    │       │
│  │  │          HSM Cluster (硬件安全模块)             │    │       │
│  │  │  · 主备模式（Active-Passive）                   │    │       │
│  │  │  · PKCS#11 / KMIP 协议                         │    │       │
│  │  │  · FIPS 140-2 Level 3 / 国密认证               │    │       │
│  │  └──────────────────────────────────────────────┘    │       │
│  │                                                      │       │
│  │  ┌──────────────────────────────────────────────┐    │       │
│  │  │       HashiCorp Vault (软件密钥后备)          │    │       │
│  │  │  · 高可用模式（Raft 存储后端）                  │    │       │
│  │  │  · 自动解封（Auto-Unseal via HSM）            │    │       │
│  │  └──────────────────────────────────────────────┘    │       │
│  └──────────────────────────────────────────────────────┘       │
│                                                                   │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐  │
│  │   TiDB       │  │   Redis      │  │   Kafka (事件)       │  │
│  │  (密钥元数据) │  │  (缓存KeyID) │  │  (key.lifecycle)    │  │
│  └──────────────┘  └──────────────┘  └──────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

### 8.2 性能指标

| 操作 | 目标 P99 | HSM 模式 | Vault 模式 |
|------|---------|---------|-----------|
| GenerateKeyPair(SM2) | ≤ 100ms | ✓ | ✓ |
| GenerateKeyPair(ECDSA) | ≤ 50ms | ✓ | ✓ |
| Sign(SM2) | ≤ 30ms | ✓ | ✓ |
| Sign(ECDSA) | ≤ 10ms | ✓ | ✓ |
| Verify(SM2) | ≤ 20ms | ✓ | ✓ |
| Verify(ECDSA) | ≤ 5ms | ✓ | ✓ |
| Encrypt/Decrypt | ≤ 5ms | ✓ | ✓ |
| IssueCertificate | ≤ 200ms | — | ✓ |

---

## 9. 与 Hub 和 DKCS 的交互

### 9.1 接口调用关系

```
Hub                          DKCS                          KMS
 │                            │                            │
 │   KeyBindRequest           │                            │
 │ ─────────────────────────▶ │                            │
 │                            │   GenerateKeyPair          │
 │                            │ ─────────────────────────▶ │
 │                            │                            ├── HSM 生成密钥对
 │                            │                            ├── CA 签发证书
 │                            │ ◀───────────────────────── │
 │                            │   KeyID + PublicKey + Cert  │
 │                            │                            │
 │                            │   保存 KeyMetadata          │
 │                            │                            │
 │ ◀───────────────────────── │                            │
 │   KeyBindResponse          │                            │
 │                            │                            │
 │                            │   Sign (消息签名)           │
 │ ◀─────────────────────────│───────────────────────────▶ │
 │                            │                            │
 │   Verify (验签手机签名)      │                            │
 │ ─────────────────────────▶ │ ─────────────────────────▶ │
 │                            │ ◀───────────────────────── │
 │ ◀───────────────────────── │                            │
```

---

## 10. 风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| HSM 性能瓶颈 | 密钥操作延迟增加 | HSM 集群 + Vault 降级路径 |
| Vault 不可用 | 软件密钥操作失败 | Vault 高可用(3节点Raft) + 自动故障转移 |
| 密钥泄露 | 安全事件 | 密钥版本化，轮换策略，HSM 不可导出 |
| CA 私钥泄露 | 信任链崩塌 | HSM 存储 CA 私钥，定期轮换，CRL 即时生效 |
| 证书验证延迟 | OCSP 响应慢 | OCSP Stapling + CRL 缓存 |
