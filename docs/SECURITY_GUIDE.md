# yuleDKCS 安全指南

**版本**: 1.0.0
**日期**: 2026-05-06

---

## 1. 安全架构概览

### 1.1 安全分层

```
┌─────────────────────────────────────────────────────────────┐
│                     应用安全层                               │
│  身份认证 │ 访问控制 │ 数据保护 │ 审计追踪                   │
├─────────────────────────────────────────────────────────────┤
│                     通信安全层                               │
│  TLS 1.3 │ 双向认证 │ 证书管理 │ 密钥协商                   │
├─────────────────────────────────────────────────────────────┤
│                     数据安全层                               │
│  加密存储 │ 完整性校验 │ 密钥管理 │ 安全删除                │
├─────────────────────────────────────────────────────────────┤
│                     硬件安全层                               │
│  SE050 │ TFM │ 安全启动 │ 防篡改                            │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. 身份认证

### 2.1 JWT 认证

**Token 结构**:
```json
{
  "header": {
    "alg": "RS256",
    "typ": "JWT",
    "kid": "key-id-xxx"
  },
  "payload": {
    "sub": "user-uuid-xxx",
    "iss": "dkcs",
    "aud": "yuledkcs",
    "exp": 1714966800,
    "iat": 1714963200,
    "jti": "token-uuid-xxx",
    "scope": ["key:read", "key:write", "vehicle:control"]
  }
}
```

**验证流程**:
1. 客户端请求携带 `Authorization: Bearer <token>`
2. API Gateway 验证签名、过期时间、签发者
3. 提取用户信息和权限范围
4. 注入到请求上下文

### 2.2 设备认证

**设备注册流程**:
1. 设备生成唯一设备ID (UUID)
2. 生成设备密钥对 (ECDSA P-256)
3. 上报公钥到服务器
4. 服务器绑定设备与用户

**设备认证流程**:
1. 设备使用私钥签名挑战值
2. 服务器使用存储的公钥验签
3. 验证设备状态 (是否激活、是否被封禁)

### 2.3 密钥认证

**车端密钥验证**:
1. 手机发送鉴权请求 (包含密钥ID)
2. TCU 从 SE050 读取对应密钥
3. 验证密钥状态 (有效、未过期、权限匹配)
4. 执行距离校验 (UWB测距)
5. 执行签名验证

---

## 3. 访问控制

### 3.1 权限模型

**RBAC (基于角色的访问控制)**:

| 角色 | 权限 |
|------|------|
| owner | 所有权限 + 分享权限 |
| family | 解锁、闭锁、启动、空调 |
| friend | 解锁、闭锁 |
| service | 解锁、闭锁 (限时) |
| temporary | 解锁 (限次、限时) |

**权限检查流程**:
```go
func CheckPermission(key *Key, action string) bool {
    // 1. 检查密钥状态
    if key.State != KeyStateActive {
        return false
    }

    // 2. 检查过期时间
    if time.Now().After(key.ExpiresAt) {
        return false
    }

    // 3. 检查权限列表
    if !contains(key.Permissions, action) {
        return false
    }

    // 4. 检查使用次数
    if key.MaxUses > 0 && key.UseCount >= key.MaxUses {
        return false
    }

    return true
}
```

### 3.2 距离限制

**UWB 距离校验**:
```go
func CheckDistance(distanceMm int, action string) bool {
    maxDistance := map[string]int{
        "unlock":       2000,  // 2米内可解锁
        "lock":         5000,  // 5米内可闭锁
        "engine_start": 1000,  // 1米内可启动
        "trunk_open":   2000,  // 2米内可开后备箱
    }

    return distanceMm <= maxDistance[action]
}
```

---

## 4. 通信安全

### 4.1 TLS 配置

**推荐配置** (TLS 1.3):
```yaml
tls:
  min_version: TLS1.3
  cipher_suites:
    - TLS_AES_256_GCM_SHA384
    - TLS_CHACHA20_POLY1305_SHA256
    - TLS_AES_128_GCM_SHA256
  curve_preferences:
    - X25519
    - CurveP256
```

**证书要求**:
- 使用 CA 签名证书 (Let's Encrypt 或企业 CA)
- 证书有效期 ≤ 90 天
- 启用 OCSP Stapling
- 启用 HSTS

### 4.2 双向认证 (mTLS)

**服务间通信**:
- DKCS ↔ HUB: mTLS 双向认证
- DKCS ↔ Java Adapters: mTLS 双向认证
- DKCS ↔ TCU: mTLS 双向认证

**配置示例**:
```yaml
mtls:
  enabled: true
  ca_cert: /etc/certs/ca.crt
  server_cert: /etc/certs/server.crt
  server_key: /etc/certs/server.key
  client_cert: /etc/certs/client.crt
  client_key: /etc/certs/client.key
  verify_client: true
```

### 4.3 协议加密

**BER-TLV 消息加密**:
```
原始消息 → BER-TLV 编码 → AES-256-GCM 加密 → 添加认证标签 → 发送
```

**加密流程**:
```go
func EncryptMessage(plaintext []byte, key []byte) ([]byte, error) {
    block, _ := aes.NewCipher(key)
    gcm, _ := cipher.NewGCM(block)

    nonce := make([]byte, gcm.NonceSize())
    rand.Read(nonce)

    ciphertext := gcm.Seal(nil, nonce, plaintext, nil)
    return append(nonce, ciphertext...), nil
}
```

---

## 5. 数据安全

### 5.1 敏感数据分类

| 级别 | 数据类型 | 保护措施 |
|------|----------|----------|
| 绝密 | Root Key, Master Key | SE050 硬件保护 |
| 机密 | Device Key, Session Key | SE050 / 加密存储 |
| 敏感 | 用户手机号、邮箱 | 数据库加密 |
| 内部 | 操作日志、事件记录 | 访问控制 |
| 公开 | 车辆型号、协议版本 | 无 |

### 5.2 数据库加密

**字段级加密**:
```sql
-- 用户表
CREATE TABLE users (
    user_id UUID PRIMARY KEY,
    phone_encrypted BYTEA,      -- AES 加密
    phone_hash BYTEA,           -- 用于查询
    email_encrypted BYTEA,
    email_hash BYTEA,
    created_at TIMESTAMP
);
```

**加密存储流程**:
```go
func StorePhone(phone string) {
    encrypted := AES_GCM_Encrypt(phone, key)
    hash := SHA256(phone + salt)

    db.Exec("INSERT INTO users (phone_encrypted, phone_hash) VALUES (?, ?)",
        encrypted, hash)
}
```

### 5.3 密钥存储

**SE050 密钥存储**:
```c
// 密钥对象 ID 分配
#define KEY_ID_ROOT       0x0001
#define KEY_ID_MASTER     0x0010
#define KEY_ID_DEVICE_1   0x0100
#define KEY_ID_DEVICE_2   0x0101
// ...

// 密钥存储 (SE050 内部)
SE05x_WriteKey(keyId, keyData, keyLen, SE05X_KEY_TYPE_AES_256);
```

**密钥永不出明文**:
- 密钥生成在 SE050 内部完成
- 密钥运算在 SE050 内部执行
- 只有密钥引用 (Key ID) 暴露给应用层

### 5.4 安全删除

**密钥销毁**:
```c
// SE050 密钥销毁
SE05x_DeleteObject(keyId);

// 内存清零
memset(keyBuffer, 0, sizeof(keyBuffer));
```

**数据删除**:
```sql
-- 安全删除 (先覆盖再删除)
UPDATE keys SET key_data = RANDOM_BYTES(32) WHERE key_id = ?;
DELETE FROM keys WHERE key_id = ?;
```

---

## 6. 密钥管理

### 6.1 密钥层级

```
Root Key (SE050, 不可读)
    │
    ├── Master Key (SE050, 派生自 Root)
    │       │
    │       ├── Device Key 1 (SE050)
    │       ├── Device Key 2 (SE050)
    │       └── ...
    │
    └── Session Keys (RAM, 运行时生成)
```

### 6.2 密钥派生

```go
// HKDF 密钥派生
func DeriveKey(masterKey []byte, context string) []byte {
    // HKDF-SHA256
    hkdf := hkdf.New(sha256.New, masterKey, nil, []byte(context))
    derivedKey := make([]byte, 32)
    hkdf.Read(derivedKey)
    return derivedKey
}

// 设备密钥派生
deviceKey := DeriveKey(masterKey, "device-"+deviceId)
```

### 6.3 密钥轮换

**轮换策略**:
- Root Key: 永不轮换 (硬件生命周期)
- Master Key: 每年轮换一次
- Device Key: 设备重新绑定时轮换
- Session Key: 每次会话新生成

**轮换流程**:
```
1. 生成新密钥
2. 新旧密钥并存期 (双写)
3. 所有服务切换到新密钥
4. 旧密钥安全删除
```

---

## 7. 安全启动

### 7.1 启动链验证

```
Boot ROM (不可变)
    │ 验证 BootLoader 签名
    ▼
BootLoader (SE050 验证)
    │ 验证 TFM 签名
    ▼
TFM (可信固件)
    │ 验证 Application 签名
    ▼
Application
```

### 7.2 签名验证

```c
// 安全启动验证
int VerifyImage(uint8_t *image, size_t len, uint8_t *signature) {
    // 1. 从 SE050 读取公钥
    SE05x_ReadPublicKey(KEY_ID_BOOT, publicKey);

    // 2. 计算镜像哈希
    SHA256(image, len, hash);

    // 3. 验证签名
    return ECDSA_Verify(publicKey, hash, signature);
}
```

---

## 8. 审计与监控

### 8.1 审计日志

**审计事件**:
- 密钥创建、激活、吊销
- 车控指令发送、执行
- 用户登录、登出
- 设备注册、移除
- 权限变更

**日志格式**:
```json
{
  "timestamp": "2026-05-06T10:00:00Z",
  "event_type": "key_activated",
  "user_id": "user-xxx",
  "device_id": "device-xxx",
  "key_id": "key-xxx",
  "vehicle_id": "VIN123",
  "result": "success",
  "trace_id": "trace-xxx",
  "source_ip": "1.2.3.4",
  "user_agent": "DKApp/1.0"
}
```

### 8.2 异常检测

**检测规则**:
- 短时间内多次认证失败 → 账户锁定
- 异常地理位置登录 → 风险告警
- 大规模密钥操作 → 审计审查
- 非工作时间敏感操作 → 告警

### 8.3 安全告警

**告警类型**:
| 类型 | 级别 | 触发条件 |
|------|------|----------|
| 认证失败风暴 | 高 | 5分钟内 > 100次失败 |
| 异常登录 | 中 | 异常IP或设备登录 |
| 密钥异常吊销 | 高 | 车主密钥被吊销 |
| 服务异常 | 高 | 安全服务不可用 |

---

## 9. 安全配置清单

### 9.1 必须配置

- [ ] 启用 TLS 1.3
- [ ] 配置强 JWT 密钥 (≥ 256 bit)
- [ ] 启用数据库连接加密
- [ ] 配置 SE050 安全芯片
- [ ] 启用安全启动
- [ ] 配置审计日志

### 9.2 推荐配置

- [ ] 启用 mTLS 服务间认证
- [ ] 配置证书自动轮换
- [ ] 启用 HSTS
- [ ] 配置 WAF
- [ ] 启用速率限制
- [ ] 配置安全告警

### 9.3 禁止配置

- [ ] 禁止明文存储密钥
- [ ] 禁止禁用 TLS
- [ ] 禁止使用弱密码
- [ ] 禁止跳过签名验证
- [ ] 禁止禁用审计日志

---

## 10. 安全响应

### 10.1 漏洞响应流程

```
漏洞发现
    │
    ▼
评估严重程度
    │
    ├── 严重 → 24小时内修复
    ├── 高危 → 7天内修复
    ├── 中危 → 30天内修复
    └── 低危 → 下版本修复
    │
    ▼
修复验证
    │
    ▼
发布安全公告
```

### 10.2 应急响应

**密钥泄露应急**:
```bash
# 1. 立即吊销泄露密钥
curl -X POST https://api.yuledkcs.com/v1/keys/{key_id}/revoke

# 2. 通知受影响用户
curl -X POST https://api.yuledkcs.com/v1/notifications/security-alert

# 3. 强制设备重新绑定
curl -X POST https://api.yuledkcs.com/v1/devices/{device_id}/force-rebind
```

---

## 11. 版本历史

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0.0 | 2026-05-06 | 初始版本 |
