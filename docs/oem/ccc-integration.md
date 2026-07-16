# CCC OEM 对接文档 — yuleDKCS × CCC Digital Key 3.0

> **文档版本**: 1.0.0  
> **创建日期**: 2026-07-16  
> **协议标准**: CCC Digital Key 3.0 Release 1  
> **系统版本**: yuleDKCS v1.0.0  

---

## 1. CCC 协议概述

### 1.1 标准信息

| 项目 | 内容 |
|:-----|:------|
| 标准全称 | CCC Digital Key 3.0 Release 1 |
| 标准组织 | Car Connectivity Consortium (CCC) |
| 加密体系 | ECDSA P-256 / ECDH P-256 / AES-256-GCM / HKDF-SHA256 |
| 近场通信 | NFC OOB（必须）+ BLE GATT 0xFFD1（必须）+ UWB FiRa（可选） |
| 证书体系 | CCC Root CA → OEM 子 CA → 设备证书（X.509 v3） |
| 安全通道 | SCP03（GlobalPlatform v2.3.1 — 基于 SE050 真实实现） |

### 1.2 CCC 协议栈架构

```
┌─────────────────────────────────────────────┐
│         CCC Application Layer              │
│   Key Issuance · Key Management · Vehicle   │
├─────────────────────────────────────────────┤
│         CCC Security Layer                 │
│   ECDSA · ECDH · AES-256-GCM · HKDF       │
├─────────────────────────────────────────────┤
│         CCC Transport Layer                │
│   NFC OOB (ISO 7816-4) · BLE GATT (0xFFD1) │
│   UWB FiRa (IEEE 802.15.4z)                │
├─────────────────────────────────────────────┤
│         CCC Secure Channel (SCP03)         │
│   AES-CMAC · AES-128-ECB · Mutual Auth     │
│   ✅ 真实实现（1362 行, P0-1 已修复）     │
└─────────────────────────────────────────────┘
```

yuleDKCS 中 CCC 协议栈：
- **车端**: `embedded/ccc_protocol/` — 最完整的协议栈实现（~90% 覆盖度）
  - SCP03 安全通道（P0-1 已修复为 1362 行真实实现）
  - NFC OOB 交换、BLE GATT 0xFFD1、UWB FiRa 测距
  - CCC 10 状态机
- **移动端**: CCC 协议自适应（iOS 支持 Apple Wallet）
- **云端**: `adapter-ccc` Java Adapter 对接 OEM CCC TSP API

---

## 2. CCC TSP API 对接要求

### 2.1 接口总览

基于 CCC Digital Key 3.0 Release 1 规范，yuleDKCS CCC Adapter 需要 OEM TSP 暴露以下 API：

| 序号 | 接口 | 方法 | 路径 | 用途 | 优先级 |
|:----:|:-----|:----:|:-----|:-----|:------:|
| 1 | Key Issuance | POST | `/api/v1/keys/request` | 申请签发数字钥匙 | P0 |
| 2 | Key Binding | POST | `/api/v1/keys/bind` | 绑定钥匙到设备（含 ECDH） | P0 |
| 3 | Key Status | GET | `/api/v1/keys/{keyId}/status` | 查询钥匙当前状态 | P0 |
| 4 | Key Revocation | POST | `/api/v1/keys/revoke` | 吊销钥匙 | P0 |
| 5 | Key Unbinding | POST | `/api/v1/keys/unbind` | 解绑钥匙与设备 | P0 |
| 6 | Vehicle Info | GET | `/api/v1/users/{userId}/vehicles` | 获取用户车辆列表 | P0 |
| 7 | Event Push | POST | (callback) | 事件推送（状态变更） | P1 |
| 8 | Certificate Mgmt | POST | `/api/v1/certificates` | 证书签发与管理 | P1 |

### 2.2 接口一：Key Issuance（钥匙签发）

```
POST /api/v1/keys/request
```

**请求体：**

```json
{
  "userId": "string",
  "vehicleId": "string",
  "vin": "string",
  "keyType": "enum(OWNER | SUB_KEY | TEMPORARY | VALET)",
  "validFrom": 1710000000,
  "validTo": 1799999999,
  "maxUses": 100,
  "permissions": ["LOCK", "UNLOCK", "START", "OPEN_TRUNK"]
}
```

**响应体：**

```json
{
  "keyId": "ccc-key-uuid",
  "keyData": ["base64-encoded-key-material-packets"],
  "keySlot": 1,
  "status": "PENDING"
}
```

### 2.3 接口二：Key Binding（钥匙绑定）

```
POST /api/v1/keys/bind
```

CCC 绑定流程要求 ECDH P-256 密钥协商：

**请求体：**

```json
{
  "userId": "string",
  "vehicleId": "string",
  "vin": "string",
  "deviceId": "string",
  "keyId": "string",
  "devicePublicKey": "base64(ecdsa_p256_device_ephemeral_pubkey)",
  "attestationToken": "base64(device_attestation)",
  "deviceCertChain": ["base64(cert1)", "base64(cert2)"]
}
```

**响应体：**

```json
{
  "keyId": "ccc-key-uuid",
  "sharedSecret": "base64(ecdh_shared_secret)",
  "tspPublicKey": "base64(tsp_ephemeral_pubkey)",
  "sessionId": "session-uuid",
  "keySlot": 1,
  "keyData": ["base64-encoded-key-material"]
}
```

**⚠️ 关键要求：**
- `sharedSecret` 必须使用 ECDH P-256 派生
- `tspPublicKey` 必须是有效的 ECDSA P-256 公钥
- `deviceCertChain` 需通过 CCC Root CA 链验证

### 2.4 接口三：Key Status（钥匙状态）

```
GET /api/v1/keys/{keyId}/status
```

**响应体：**

```json
{
  "data": {
    "keyId": "ccc-key-uuid",
    "status": "enum(ACTIVE | PENDING | REVOKED | EXPIRED | SUSPENDED)",
    "boundDeviceId": "device-uuid",
    "createdAt": 1710000000,
    "expiresAt": 1799999999,
    "lastUsedAt": 1715000000,
    "metadata": {}
  }
}
```

### 2.5 接口四：Key Revocation（钥匙吊销）

```
POST /api/v1/keys/revoke
```

**请求体：**

```json
{
  "keyId": "ccc-key-uuid",
  "reason": "enum(LOST | STOLEN | OWNER_REQUEST | ADMIN)",
  "userId": "string",
  "signature": "base64(ecdsa_sign(userId||keyId||reason))"
}
```

### 2.6 接口五：Key Unbinding（钥匙解绑）

```
POST /api/v1/keys/unbind
```

**请求体：**

```json
{
  "keyId": "ccc-key-uuid",
  "deviceId": "device-uuid",
  "reason": "string"
}
```

### 2.7 接口六：Vehicle Information（车辆信息）

```
GET /api/v1/users/{userId}/vehicles
```

**响应体：**

```json
{
  "vehicles": [
    {
      "vehicleId": "string",
      "vin": "WBA3A5C5XDF123456",
      "make": "OEM",
      "model": "Model S",
      "modelYear": 2026,
      "cccSupported": true,
      "capabilities": ["BLE", "NFC", "UWB"]
    }
  ]
}
```

---

## 3. 证书链要求

### 3.1 CCC 证书层次

```
                 CCC Root CA (自签名)
                       │
              ┌────────┴────────┐
              │                 │
         OEM 子 CA           OEM 子 CA (备用)
              │
     ┌────────┴────────┐
     │                 │
  车端证书          设备证书
  (SE050)          (手机 TEE/SE)
```

### 3.2 证书属性要求

| 证书 | 密钥算法 | 证书用途 | 有效期 |
|:-----|:---------|:---------|:------:|
| **CCC Root CA** | ECDSA P-384 | CA:TRUE, KeyCertSign, CRLSign | 20 年 |
| **OEM 子 CA** | ECDSA P-384 | CA:TRUE, KeyCertSign, CRLSign | 10 年 |
| **车端证书** | ECDSA P-256 | DigitalSignature, KeyAgreement | 5 年 |
| **设备证书** | ECDSA P-256 | DigitalSignature, KeyAgreement | 1–2 年 |

### 3.3 证书验证流程

```
1. 设备端验证 TSP 证书：
   → TSP 证书 -> OEM 子 CA -> CCC Root CA
   → CRL 检查（OCSP Stapling）
   
2. TSP 端验证设备证书：
   → 设备证书 -> OEM 子 CA -> CCC Root CA
   → Check KeyUsage = DigitalSignature
   
3. 双向证书认证：
   → mTLS handshake 同时验证双方证书链
```

### 3.4 CRL / OCSP 要求

| 项目 | 要求 |
|:-----|:------|
| CRL 更新频率 | 至少每日更新一次 |
| CRL 分发点 | CCC Root CA CRL + OEM 子 CA CRL |
| OCSP 响应器 | 可选，但建议部署 |
| 吊销场景 | 设备丢失、密钥泄露、用户注销 |

### 3.5 mTLS 配置建议

Adapter 到 TSP 的通信推荐使用 mTLS：

```yaml
adapter:
  ccc:
    tls:
      enabled: true
      client-cert-path: /certs/ccc-client.pem
      client-key-path: /certs/ccc-client-key.pem
      ca-cert-path: /certs/ccc-root-ca.pem
```

---

## 4. SCP03 安全通道

### 4.1 SCP03 协议概述

CCC 协议的安全基础是 SCP03（Secure Channel Protocol 03），基于 GlobalPlatform v2.3.1：

| 阶段 | 操作 | 密钥 |
|:-----|:------|:-----|
| INITIALIZE UPDATE | 发送 Host Challenge，接收 Card Challenge + Card Cryptogram + Sequence Counter | S-MAC, S-ENC, S-RMAC |
| EXTERNAL AUTHENTICATE | 发送 Host Cryptogram + C-MAC | S-MAC |
| Secure Session | 加密 APDU 通信 (ENC + MAC) | S-ENC + S-MAC |

yuleDKCS **P0-1 已修复** SCP03 安全通道为 1362 行真实实现（`se050_scp03.c`），包括：
- AES-128 ECB 引擎（FIPS 197 标准）
- AES-CMAC（NIST SP 800-38B）
- I2C APDU 传输（含 100 次轮询重试）
- 密钥派生（D_i = 0x01 || counter || 0x00*6 || seq_counter || 0x80 || 0x00*5）
- 安全清理（`secure_zero()` 使用 volatile 指针防止优化移除）

### 4.2 SE050 SCP03 集成架构

```
┌─────────────────┐
│   security.c     │  ← sec_init(), sec_scp03_open(), sec_scp03_close()
├─────────────────┤
│ se050_scp03.c    │  ← 1362 行真实 SCP03 实现
│  (SCP03 握手)    │
├─────────────────┤
│  i2c_transfer()  │  ← 平台 HAL 层 I2C 驱动
├─────────────────┤
│  NXP SE050       │  ← CC EAL 6+ 安全芯片
└─────────────────┘
```

---

## 5. CCC BLE / NFC / UWB 协议

### 5.1 CCC BLE GATT

| 服务 | UUID |
|:-----|:------|
| CCC Digital Key Service | `0000FFD1-0000-1000-8000-00805F9B34FB` |
| CCC Control Point | `0000FFD2-0000-1000-8000-00805F9B34FB` |
| CCC Data Stream | `0000FFD3-0000-1000-8000-00805F9B34FB` |

### 5.2 CCC NFC OOB

NFC OOB（Out-of-Band）是 CCC 的强制配对机制：

| 操作 | APDU 指令 | 数据格式 |
|:-----|:----------|:---------|
| 选择应用 | `00 A4 04 00` | AID = `A0000008594343444B` |
| 读取证书 | `00 CA 01 00` | 返回车端 X.509 证书 |
| 密钥交换 | `00 87 01 00` | 携带 ECDH 公钥 |
| 建立通道 | `00 70 00 00` | SCP03 安全通道建立 |

### 5.3 CCC UWB FiRa

UWB 使用 FiRa 规范（IEEE 802.15.4z HRP）：

| 参数 | 值 |
|:-----|:----|
| 信道 | 5 (6.5 GHz) / 9 (8.0 GHz) |
| 脉冲重复频率 | 64 MHz |
| STS 模式 | 静态 STS + 动态 STS 混合 |
| 测距精度 | ±10 cm |
| 安全等级 | PHY-level 认证 + 双向测距 |

> ⚠️ 当前 FiRa 测距为 stub，生产部署需补全真实驱动。

---

## 6. 对接 CheckList（25 项）

### 6.1 环境与凭据（5 项）

- [ ] **C-P1** OEM 提供 CCC TSP API 端点（HTTPS URL）
- [ ] **C-P2** OEM 提供 OAuth2 Client ID 与 Client Secret
- [ ] **C-P3** 确认 OAuth2 Token 端点 URL 和 Grant Type
- [ ] **C-P4** 网络连通性 + TLS 1.2+ 验证通过
- [ ] **C-P5** 确认 API 限流策略

### 6.2 证书链（5 项）

- [ ] **C-C1** OEM 提供 CCC Root CA 证书（PEM/DER）
- [ ] **C-C2** OEM 提供 OEM 子 CA 证书
- [ ] **C-C3** 设备证书链结构验证通过（设备 → OEM 子 CA → CCC Root CA）
- [ ] **C-C4** CRL 分发点（CDP）可用
- [ ] **C-C5** 双向 mTLS 配置完成（Adapter ↔ TSP）

### 6.3 核心接口（10 项）

- [ ] **C-I1** OAuth2 Token 获取成功（`grant_type=client_credentials`）
- [ ] **C-I2** `getVehicles` 返回正确的车辆列表
- [ ] **C-I3** `requestKeys` 成功签发钥匙
- [ ] **C-I4** `bindKey` 调用成功，`sharedSecret` 非空且为 ECDH P-256 派生
- [ ] **C-I5** `bindKey` 返回的 `tspPublicKey` 为有效 ECDSA P-256 公钥
- [ ] **C-I6** `getKeyStatus` 对已绑定钥匙返回 `ACTIVE`
- [ ] **C-I7** `revokeKeys` 成功吊销钥匙，状态变更为 `REVOKED`
- [ ] **C-I8** `unbindKey` 成功解绑钥匙
- [ ] **C-I9** 吊销后解锁指令被拒绝
- [ ] **C-I10** 所有请求/响应的 ECDSA 签名验证通过

### 6.4 安全通道（5 项）

- [ ] **C-S1** SCP03 INITIALIZE UPDATE 握手成功
- [ ] **C-S2** SCP03 EXTERNAL AUTHENTICATE 通过
- [ ] **C-S3** 加密 APDU 通信正确加解密
- [ ] **C-S4** 会话密钥安全清理（`secure_zero()` 验证）
- [ ] **C-S5** 多次会话密钥派生独立且随机

---

## 7. CCC Adapter 配置参考

### 7.1 application-ccc.yml

```yaml
adapter:
  ccc:
    enabled: true
    api-url: ${CCC_API_URL:https://ccc-tsp.oem.com}
    client-id: ${CCC_CLIENT_ID}
    client-secret: ${CCC_CLIENT_SECRET}
    connection-timeout: 10000
    read-timeout: 30000
```

### 7.2 OAuth2 Token 配置

若 OEM TSP 使用标准 OAuth2：

```yaml
oauth2:
  token-uri: ${CCC_OAUTH_TOKEN_URL:https://auth.oem.com/token}
  client-id: ${CCC_CLIENT_ID}
  client-secret: ${CCC_CLIENT_SECRET}
  grant-type: client_credentials
  scope: digital_key_api
```

### 7.3 启动与验证

```bash
# 启动 CCC Adapter
java -jar adapter-grpc-server.jar \
  --spring.profiles.active=ccc,prod

# 健康检查
curl http://localhost:8080/actuator/health

# 预期返回
# {"status":"UP","adapters":{"total":1,"enabled":1,"ccc":"UP"}}
```

---

## 8. 状态机说明

CCC 定义 10 状态钥匙生命周期，yuleDKCS 全部实现：

```
                     ┌─────────┐
                     │ CREATED │
                     └────┬────┘
                          │ bind
                     ┌────▼────┐
              ┌──────│ ACTIVE  │──────┐
              │      └────┬────┘      │
              │ suspend   │ resume    │
         ┌────▼───┐  ┌────┴────┐   ┌─▼──────┐
         │SUSPENDED│  │ ACTIVE  │   │TEMPORARY│
         └────┬───┘  └────┬────┘   └─┬──────┘
              │           │          │
              └───────────┼──────────┘
                          │ revoke/expire
                     ┌────▼────┐
                     │EXPIRED/ │
                     │REVOKED  │
                     └─────────┘
```

---

## 9. 常见问题

### Q1: CCC Root CA 证书从哪里获取？

A: CCC 联盟规范中定义了标准 Root CA。联调时 OEM 应提供其证书链（CCC Root → OEM 子 CA）。若 OEM 未加入 CCC 联盟，可将 Root CA 替换为 OEM 自签名 Root CA 进行 POC 测试。

### Q2: SCP03 通道在 POC 中必须完整实现吗？

A: **强烈建议完整实现。** yuleDKCS 的 SCP03 实现已通过专家评审（P0-1 修复），直接可用。它消除了 stub 模式的高安全风险。

### Q3: CCC 绑定流程中 `deviceCertChain` 如何处理？

A: Adapter 收到设备证书链后，应进行链式验证（device cert → OEM CA → CCC Root CA），验证 KeyUsage、有效期、CRL 状态。验证通过后再执行 ECDH 密钥协商。

### Q4: 如果 OEM TSP 不支持 `deviceCertChain` 字段？

A: POC 阶段可降级处理：TSP 仅接收 `attestationToken`，跳过证书链验证。生产部署时必须启用证书链验证。

---

*本文档应与 [POC 联调指南](poc-guide.md)、[Demo 脚本](demo-script.md) 配套使用。*
