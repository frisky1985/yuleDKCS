# yuleDKCS 数字钥匙系统 — 架构与安全审查报告

**审查日期**: 2026-05-31  
**审查范围**: 全栈三端 + 三协议  
**审查深度**: 源码级逐行分析

---

## 1. 系统架构完整性

### 1.1 三端边界定义

| 层级 | 边界 | 清晰度 | 说明 |
|------|------|--------|------|
| 手机端↔云端 | HTTPS/TLS 1.3 | ✅ 清晰 | `rest_gateway.go` 中 REST API 框架已搭建 |
| HUB↔DKCS | gRPC 双向流 | ⚠️ 中危 | `middleware.go` JWT 鉴权有实现，但 `rest_gateway.go` 全部 handler 是 stub |
| DKCS↔TCU | MQTT/TLS 或私有协议 | ⚠️ 中危 | 协议定义在 `hub-dkcs-protocol.md`，但 TCU 侧 MQTT 客户端代码缺失 |
| 手机↔TCU | BLE+UWB+NFC | ✅ 清晰 | 三协议栈均有完整的 C/Kotlin/Swift 实现 |

### 1.2 数据流断点

**严重问题 — REST 网关全部实现为空** (`backend/cloud/hub/internal/gateway/rest_gateway.go:111-174`)

```go
func (g *RESTGateway) bindKey(c *gin.Context) {
    // TODO: Parse JSON → gRPC BindKeyRequest → call KeyManagementService.BindKey
    c.JSON(200, gin.H{"message": "bindKey"})  // 永远返回成功！
}
```

所有 16 个 REST 端点（密钥绑定/解绑/吊销/分享、车控指令、状态查询）**全部是 stub**——任何请求无身份验证、无参数校验直接返回 200 OK。这是系统级集成断点，云端防护层不存在。

**风险等级**: 🔴 **高危** — 代码已部署则任何人可访问所有 API

### 1.3 通信协议选型

| 链路 | 选型 | 评估 |
|------|------|------|
| 手机↔TCU (近场) | BLE 5.0 + UWB FiRa + NFC | ✅ 行业标准组合，CCC/ICCOA/ICCE 均兼容 |
| 手机↔云端 (远程) | HTTPS/TLS 1.3 | ✅ 合理 |
| 云端↔TCU (远程) | MQTT/TLS | ✅ 车联网标准 |
| 内部微服务 | gRPC 双向流 | ✅ 高性能 |
| 内部数据格式 | BER-TLV | ✅ 紧凑标准化，ISO 7816-4 |

**问题**: NFC HCE 通信在 Android 端缺少安全通道加密——APDU 命令直接明文发送 (`NfcManager.kt:180-213`)。

---

## 2. 安全漏洞

### 2.1 密钥管理链路

#### 🟢 **SE050 安全芯片链路 — 设计合理**

`security.c:38-94` 的 SE050 通过 SCP03 安全通道通信，密钥永不离开安全元件。Root/Master/Device Key 三层派生设计符合行业最佳实践。

#### 🔴 **高危：CreateKey 响应明文返回 Secret** (`backend/dkcs/internal/service/key_service.go:94-102`)

```go
key := &repository.Key{
    ...
    Secret: secret,  // 32字节随机密钥
}
return &pb.CreateKeyResponse{
    KeyId:  keyID,
    Secret: secret,  // ← 明文返回客户端！！！
    Status: "pending",
}, nil
```

密钥以 hex 明文通过 gRPC 返回。如果 gRPC 未启用 mTLS（当前 `rest_gateway.go:78-83` 只有普通 HTTP），密钥在传输层可能被截获。

#### 🔴 **高危：Android 密钥元数据明文存储** (`frontend/android/src/main/kotlin/com/digitalkey/sdk/key/KeyManager.kt:516-557`)

```kotlin
private fun loadKeysFromStorage() {
    val prefs = context.getSharedPreferences("digital_keys", Context.MODE_PRIVATE)
    val keysJson = prefs.getString("keys", null)
    // JSON中包含：keyType, status, validFrom, validUntil, maxUses, usedCount, shareCode ...
}
```

关键数据以 **JSON 明文** 存储在 SharedPreferences 中，包括 `shareCode`（分享码）、权限配置。虽然 Android 的设备加密可以保护磁盘，但备份、日志、调试工具均可读取。攻击者读取 SharedPreferences 后可直接获取 key 元数据。

**建议**: 使用 EncryptedSharedPreferences 或 Jetpack DataStore + 加密。

#### ⚠️ **中危：Android KeyManager 密钥类型错误** (`KeyManager.kt:464-482`)

```kotlin
val keyGen = KeyGenerator.getInstance(
    KeyProperties.KEY_ALGORITHM_AES,   // ← 数字钥匙需要 ECDSA P-256
    ANDROID_KEYSTORE
)
val spec = KeyGenParameterSpec.Builder(alias, ...)
    .setKeySize(256)
    .build()
keyGen.init(spec)
keyGen.generateKey()
```

生成的是 **AES 对称密钥** 而不是 **ECDSA P-256 非对称密钥对**。数字钥匙需要非对称签名，用 AES 密钥做签名会导致：
1. 签名验证失败
2. `KeyStore.getEntry()` 时 `getSecretKey()` 无法正确处理（第 484 行 `as? KeyStore.SecretKeyEntry`）
3. 无法与车端 SE050 做 ECDH 密钥协商

这代表 **Android SDK 的基本密钥生成逻辑是错误的**，直接导致所有近场认证功能不可用。

### 2.2 三协议认证握手

#### 🟢 **CCC 协议 — SE050 SCP03 安全通道** (`security.c:39-52`)

设计符合 CCC 3.0 规范，SE050 提供硬件级安全。但 `sec_scp03_open()` 函数体为空（`(void)ch; /* Platform-specific implementation */`）——SCP03 实际未实现。

#### ⚠️ **中危：ICCOA 认证缺少时间校验** (`iccoa_auth.c:52-57`)

```c
/* Check validity period */
/* TODO: compare current timestamp vs valid_from/valid_until */
```

权限有效性检查标有 TODO 未实现——已吊销/过期密钥在 ICCOA 协议上仍可认证通过。

#### ⚠️ **中危：ICCE 认证中所有时间戳为 0** (`security_auth.c`)

```c
challenge->timestamp = 0;  // TODO: 获取实际时间 (line 109)
uint32_t current_time = 0;  // TODO (lines 130, 222, 490)
```

整个 `security_auth.c` 中，**所有时间戳均为 0**：
- 挑战过期检查永远无效（`current_time > challenge->expiry` → `0 > (0+30000)` → false）
- 会话过期清理不会触发
- Nonce 缓存清理同样失效

**攻击向量**: 攻击者可重放 30 天前的挑战响应，系统无法检测过期。

### 2.3 BerTLV 编解码安全问题

#### 🟢 **Go 版 BerTLV 解码器 — 较好的边界检查**

`decoder.go` 在 `readTag()` 和 `readLength()` 中均有 `Len()` 边界检查，整体实现较为安全。

#### ⚠️ **中危：Go 版 readTag 潜在无限循环** (`decoder.go:202-216`)

```go
if firstByte&0x1F == 0x1F {
    for {
        if d.reader.Len() == 0 {
            return 0, ErrInvalidTag
        }
        nextByte, err := d.reader.ReadByte()
        ...
        tag = (tag << 8) | Tag(nextByte)
        if nextByte&0x80 == 0 {
            break
        }
    }
}
```

如果攻击者构造连续高位置 1 的字节序列，虽然 `Len() == 0` 时会退出，但在此之前 `tag` 变量会不断左移，当移位超过 `Tag` 类型（应为 `int`）宽度时可能溢出。虽不会 crash，但可能导致逻辑绕过。

#### 🟢 **Android BerTLV 解码器 — 边界检查较为完善**

`BertlvDecoder.kt` 对每种 Length 格式（`0x81`/`0x82`/`0x83`/`0x84`）都有显式的 `offset >= data.size` 边界检查。

#### 🟢 **iOS BerTLV 解码器 — 较为安全**

`BertlvDecoder.swift` 使用 `context.read()` 抛出异常的安全模式，且有 `maxExtraBytes=3` 限制防无限循环。

#### ⚠️ **中危：各平台 BerTLV 都对 0x84（4字节 Length）的支持差异**

| 平台 | 0x84 支持 | 差异风险 |
|------|-----------|---------|
| Go 编码器 | ❌ 只到 `0x83` (3字节) | 编码和解码标准不统一 |
| Android | ✅ 支持 `0x84` | 解码器和编码器可能不匹配 |
| iOS | ✅ 支持 `numBytes > 4` 检查 | 相对安全 |

如果 Go 端生成用 `0x83` 编码的消息被 Android 段收到，解码一致。但如果消息中出现了 `0x84`（Android 收到外来消息），Go 端编码器会拒绝构造。跨协议互操作时存在不一致。

### 2.4 gRPC/REST API 鉴权和授权

#### 🔴 **高危：REST 网关无鉴权** (`rest_gateway.go:111-174`)

如前所述，所有 handler 均为 stub（返回固定 JSON）。但在架构设计层面，`middleware.go:16-50` 已实现 JWT 验证逻辑。**问题在于 JWT middleware 未挂载到 REST 网关**。

`rest_gateway.go:28-31` 只有 `gin.Recovery()` 和日志中间件：

```go
r := gin.New()
r.Use(gin.Recovery())
r.Use(g.loggerMiddleware())
// 没有 JWT Auth middleware！
```

#### ⚠️ **中危：gRPC AuthInterceptor 实现可绕过**

`middleware.go:19-21` 存在逻辑缺陷：

```go
if strings.HasSuffix(info.FullMethod, "Health/Check") {
    return handler(ctx, req)
}
```

任何以 `"Health/Check"` 结尾的方法名会跳过认证。攻击者若构造了 `FakeHealth/Check` 方法名（proto 定义中不存在的方法），gRPC 框架会拒绝但理论上仍有风险。

#### ⚠️ **中危：Token Bucket goroutine 泄露** (`middleware.go:186-196`)

```go
go func() {
    ticker := time.NewTicker(tb.refillRate)
    for range ticker.C {
        // ... 没有退出机制
    }
}()
```

每次调用 `NewTokenBucket` 会启动一个 goroutine 且**永不退出**。每次 gRPC 客户端连接都会触发一个新的限流器实例，长期运行可能导致 goroutine 堆积。

### 2.5 离线决策逻辑

#### ⚠️ **中危：离线决策时区验证全失** (`offline_decision.c`)

与 `security_auth.c` 相同的问题——`uint32_t current_time = 0;  // TODO` 出现 6 次以上：

| 位置 | 行号 | 影响 |
|------|------|------|
| 密钥过期检查 | L166-167 | `0 > expiry_time` 总是 false，过期密钥可用 |
| 速率限制 | L428 | 所有请求都在同一时间窗口 |
| 风险评估 | L485 | `offline_duration = 0 - last_sync_time` 溢出为极大值 |
| 权限时间窗 | L392-395 | `0 < valid_from` 总是 true |

**攻击向量**: 在无网络环境下（如地下车库），攻击者可以使用已吊销密钥持续操控车辆，因为所有时间检查因 `current_time=0` 而失效。

#### ⚠️ **中危：速率限制槽溢出时默认放行** (`offline_decision.c:451-460`)

```c
for (int i = 0; i < 32; i++) {
    if (g_decision.rate_limits[i].user_id == 0) {
        // ... 分配新槽
        return 0;
    }
}
return 0;  // ← 没有空闲槽，返回成功（放行！）
```

当 32 个速限槽全部用满时，后续请求自动放行——速率限制完全失效。应该返回 -1 拒绝请求。

---

## 3. 协议兼容性

### 3.1 三协议优先级和降级策略

**Negotiator** (`router.go:32-66`) 的优先级设计：
1. ICCOA DK 4.0（+10 分）
2. CCC 3.0（+5 分）
3. ICCOA DK 3.0（0 分）
4. ICCE（0 分）

**合理但缺少**:
- 明确的降级触发条件（超时时间、重试次数）
- 协议切换的安全性（降级协议是否意味着降级安全？）
- 降级后的协商持久化（避免每次都重协商）
- 风险等级：⚠️ **中危**

### 3.2 ICCOA DK 3.0 ↔ DK 4.0 共存

| 维度 | DK 3.0 | DK 4.0 | 差异风险 |
|------|--------|--------|---------|
| 帧头 | SOP/CMD/SEQ/LEN/CS/EOP 9字节 | Magic/Ver/MsgType/.../Token 14字节 | 完全不同 |
| 帧结构 | `iccoa_dk30_frame_t` 固定 251 字节 | `iccoa_dk40_frame_t` 固定 268 字节 | ✅ |
| 认证方式 | 简单XOR校验 | HMAC-SHA256 | 🔴 向下兼容不明确 |

DK 3.0 使用 XOR checksum 做完整性校验，DK 4.0 使用 HMAC。**DK 4.0 降级到 DK 3.0 时安全强度骤降**——攻击者可篡改 DK 3.0 帧内容并重算 XOR 校验。

---

## 4. 错误处理和边界情况

### 4.1 断网/重连/并发的异常处理

| 场景 | 状态 | 问题 |
|------|------|------|
| BLE 连接断开重连 | ⚠️ 中危 | BLE 断开后会话密钥在 RAM 中残留 (`security_auth.c:420-423`) |
| 云端 gRPC 断连 | ⚠️ 中危 | 无明确的自动重连策略和退避算法 |
| ICCOA 旧 seq_num 重复 | ⚠️ 中危 | `iccoa_dk30.c:8` 的 `g_seq_num` 单调递增但无防回放验证 |
| 并发密钥更新 | ⚠️ 中危 | Android `keysCache` 是 `MutableMap` 非线程安全 |

### 4.2 数字钥匙吊销流程

云端的 `RevokeKey` 实现存在重大问题：

**DKCS 层** (`key_service.go:143-178`): 有基本吊销逻辑（状态更新），但未通知 TCU 和撤消手机本地密钥。

**HUB 层** (`key_management.go:74-78`):
```go
func (s *KeyManagementService) RevokeKey(...) {
    s.logger.Info("RevokeKey", ...)
    return &pb.RevokeKeyResponse{}, nil  // 永远返回空成功
}
```

**问题**: 吊销操作在 HUB 层完全未实现——既未通知手机端，也未通知车端 TCU。已吊销密钥在本地缓存中可能继续有效。

**风险等级**: 🔴 **高危**

### 4.3 时间同步攻击面

如前所述，**Embedded C 代码中所有时间戳均为 0（TODO）**。这意味着：

1. **无时间源**: TCU 没有 RTC 或 NTP 同步机制
2. **挑战响应可重放**: 无有效的时间窗口检查
3. **会话永不过期**: `SESSION_EXPIRY_MS` (8小时) 形同虚设
4. **日志无法审计**: 所有事件时间戳为 0

**风险等级**: 🔴 **高危** — 离线场景下攻击者可使用永远有效的会话密钥

---

## 5. 代码层面潜在问题

### 5.1 Embedded C 内存安全问题

#### 🔴 **高危：ICCOA DK3.0 栈缓冲区溢出** (`iccoa_dk30.c:95-109`, `iccoa_digital_key.h:70-78`)

```c
typedef struct __attribute__((packed)) {
    uint8_t  sop;
    uint8_t  cmd_id;
    uint16_t seq_num;
    uint16_t payload_len;
    uint8_t  payload[ICCOA_MAX_PAYLOAD];  // 244 字节
    uint8_t  checksum;
    uint8_t  eop;
} iccoa_dk30_frame_t;  // sizeof = 251
```

`iccoa_dk30_send_response()` 中的操作：

```c
frame.payload_len = len;        // 无上限检查！
if (len > 0 && payload) {
    memcpy(frame.payload, payload, len);  // 栈溢出！
}
frame.checksum = checksum((&frame + 1), 4 + len);  // OOB 读取！
```

如果 `len > 244 (`ICCOA_MAX_PAYLOAD`)，`memcpy` 超出栈缓冲区向高地址写入，导致**栈损坏、RIP 劫持**。在车辆 TCU 上可通过 BLE 远程触发——攻击者向 BLE 特征值写入超长 payload 即可触发。

**攻击链**: BLE 连接 → 发送长度 > 244 的 ICCOA 帧 → 触发栈溢出 → RCE 控制 TCU

#### 🟠 **中危：ICCOA DK3.0 输入帧 payload_len 未验证** (`iccoa_dk30.c:60-92`)

```c
int32_t iccoa_dk30_process(const uint8_t *raw, uint16_t len)
{
    ...
    const iccoa_dk30_frame_t *frame = (const iccoa_dk30_frame_t *)raw;
    uint16_t payload_len = frame->payload_len;  // 从网络读入
    ...
    uint8_t cs = iccoa_dk30_checksum(raw + 1, 4 + payload_len);  // OOB 读
    ...
    return handle_auth_request(frame->payload, payload_len);  // 传递给子函数
}
```

`payload_len` 直接从帧数据读取，未与 `len`（接收缓冲区真实长度）比较。如果 `payload_len > len - 6`，导致：
1. `checksum()` 内存越界读取
2. `handle_*` 函数获取到被篡改的 payload

#### 🟠 **中危：ICCOA `handle_ctrl_request` 直接信任 payload 内容** (`iccoa_dk30.c:47-58`)

```c
iccoa_ctrl_cmd_e cmd = (iccoa_ctrl_cmd_e)payload[0];
uint8_t param = payload[1];
int32_t ret = iccoa_ctrl_execute(cmd, param);  // ← 传递到车辆控制总线！
```

验证了 `len < 2` 但未验证 `cmd` 在有效枚举范围——任意数值都可能传递到车辆控制总线（CAN）。

### 5.2 Android/iOS 密钥泄露风险

#### 🔴 **高危：Android 密钥元数据泄露**（已在上文 2.1 中详述）

#### ⚠️ **中危：Android NfcManager 使用 Mutable PendingIntent** (`NfcManager.kt:67`)

```kotlin
val flags = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
    PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_MUTABLE
}
```

`FLAG_MUTABLE` 在 Android 12+ 上允许 Intent 被修改，可能被恶意应用劫持 NFC Intent。

#### ⚠️ **中危：iOS SDK API Key 存储在内存中** (`DigitalKeySDK.swift:53-74`)

```swift
public struct SdkConfig {
    public let apiKey: String  // 明文存储
}
```

`apiKey` 在网络请求中会重复使用，但并未使用 iOS Keychain 保护。App 被挂起后内存转储可能泄露。

### 5.3 云端 Go 服务并发安全

#### 🟢 **Adapter Registry RWMutex 使用不当** (`registry.go:67-79`)

```go
func (r *Registry) GetByVendor(vendor string) (Adapter, bool) {
    r.mu.RLock()
    defer r.mu.RUnlock()
    for k, a := range r.adapters {
        if a.Vendor() == vendor {
            r.mu.RUnlock()  // 显式解锁
            _ = k
            r.mu.RLock()    // 重新加锁
            return a, true
        }
    }
    return nil, false
}
```

这段代码存在 **双重释放/锁竞争风险**：
1. `r.mu.RUnlock()` 显式调用 + `defer r.mu.RUnlock()` → **计数减 2 次**（实际只加锁 1 次）
2. 另一个 goroutine 可能在此间隙写入，中断迭代

这会导致 `fatal error: sync: RUnlock of unlocked RWMutex` 或数据竞争。

#### ⚠️ **中危：Go 服务缺少限流保护**

- `CreateKey()`（key_service.go）无用户级限制——攻击者可批量创建密钥耗尽数据库
- `ListKeys()` 无分页大小上限——可能被用于超大型查询导致 OOM
- 仅 `middleware.go` 有 Token Bucket 但粒度太粗

---

## 完整风险矩阵

| 编号 | 漏洞描述 | 位置 | 风险等级 |
|------|---------|------|---------|
| V-01 | REST 网关全部 Handler 为 Stub，无鉴权 | `rest_gateway.go:111-174` | 🔴 高危 |
| V-02 | ICCOA DK3.0 栈缓冲区溢出（memcpy 无边界检查） | `iccoa_dk30.c:103` | 🔴 高危 |
| V-03 | CreateKey 响应明文返回密钥 Secret | `key_service.go:94-102` | 🔴 高危 |
| V-04 | Android KeyManager 密钥元数据明文 SharedPreferences | `KeyManager.kt:516-557` | 🔴 高危 |
| V-05 | 嵌入式 C 所有时间戳为 0（TODO 未实现） | `security_auth.c`, `offline_decision.c` | 🔴 高危 |
| V-06 | Android KeyManager 生成 AES 密钥而非 ECDSA | `KeyManager.kt:465` | 🔴 高危 |
| V-07 | HUB 层 RevokeKey 完全未实现（空函数） | `key_management.go:74-78` | 🔴 高危 |
| V-08 | ICCOA DK3.0 帧 payload_len 不验证真实性 | `iccoa_dk30.c:67-68` | 🟠 中危 |
| V-09 | HCtrl 指令直接传递到 CAN 总线，无命令枚举验证 | `iccoa_dk30.c:52,55` | 🟠 中危 |
| V-10 | ICCE 认证挑战过期检查因时间戳=0 失效 | `security_auth.c:130` | 🟠 中危 |
| V-11 | ICCOA 认证权限时间检查未实现（TODO） | `iccoa_auth.c:52-53` | 🟠 中危 |
| V-12 | 离线决策速率限制槽满默认放行 | `offline_decision.c:460` | 🟠 中危 |
| V-13 | Go BerTLV 编码器 writeInt 固定 8 字节 | `encoder.go:81-86` | 🟡 低危 |
| V-14 | Adapter Registry RWMutex 锁操作错误 | `registry.go:72-73` | 🟠 中危 |
| V-15 | Token Bucket goroutine 泄露（永不退出） | `middleware.go:186-196` | 🟡 低危 |
| V-16 | ICCOA DK4.0 降级到 DK3.0 时安全降级（XOR vs HMAC） | 协议设计 | 🟠 中危 |
| V-17 | NFC APDU 明文传输，无会话加密 | `NfcManager.kt:180` | 🟠 中危 |
| V-18 | Android NfcManager PendingIntent FLAG_MUTABLE | `NfcManager.kt:67` | 🟡 低危 |
| V-19 | gRPC AuthInterceptor Health/Check 跳过认证匹配过于宽松 | `middleware.go:19` | 🟡 低危 |
| V-20 | ICCOA seq_num 单调递增但无防回放验证 | `iccoa_dk30.c:8` | 🟡 低危 |

---

## 修复建议（按优先级）

### P0 — 立即修复

1. **V-02: ICCOA DK3.0 栈缓冲区溢出**
   - 在 `iccoa_dk30_send_response()` 中添加 `if (len > ICCOA_MAX_PAYLOAD) return ICCOA_ERR_PARAM;`
   - 在 `iccoa_dk30_process()` 中添加 `if (payload_len > len - sizeof(iccoa_dk30_frame_t) + ICCOA_MAX_PAYLOAD) return ICCOA_ERR_PARAM`

2. **V-06: Android 密钥类型错误**
   - 将 `KeyGenerator` 替换为 `KeyPairGenerator.getInstance("EC", "AndroidKeyStore")`
   - 设置 `KeyProperties.PURPOSE_SIGN | KeyProperties.PURPOSE_VERIFY`

3. **V-01: REST 网关鉴权**
   - 立即挂载 JWT Auth Middleware 到 gin Router
   - 实现所有 handler 的 gRPC 转发逻辑
   - **在修复前不应部署到生产环境**

4. **V-05: 嵌入式时间戳**
   - 集成 RTC 驱动或从 BLE 连接获取安全时间戳
   - 暂时使用单调计数器确保至少 Nonce 回放防护

### P1 — 高优先级

5. **V-03: 密钥 Secret 加密封装**
   - CreateKey 响应中使用短期会话密钥加密 secret
   - 或在服务端用 hash 代替明文返回（手机端使用 ECDH 派生相同密钥）

6. **V-04: Android 密钥存储**
   - 迁移到 EncryptedSharedPreferences
   - 分享码使用额外加密层

7. **V-07: RevokeKey 完整实现**
   - 同步通知 TCU（MQTT 消息）
   - 推送通知到手机端清理本地缓存

8. **V-12: 速率限制槽满处理**
   - 将 `return 0` 改为 `return -1`

### P2 — 中优先级

9. **V-08: payload_len 验证**
10. **V-14: Registry 锁修复**
11. **V-16: DK3.0↔DK4.0 降级安全策略**
12. **V-13: BerTLV 编码一致性调整**
13. **V-17: NFC 通道加密**

### P3 — 低优先级

14. **V-15: Goroutine 泄露修复**
15. **V-19: AuthInterceptor 匹配加固**
16. **V-20: seq_num 回放防护**

---

## 结论

yuleDKCS 项目在**架构设计层面**展现了良好的规范性和安全意识（SE050 硬件保护、三层密钥体系、mTLS 设计、RBAC 权限模型），体现了对汽车数字钥匙行业的深刻理解。

但在**代码实现层面**存在若干严重问题：

1. **嵌入式 C 最危险**：`iccoa_dk30.c` 的栈缓冲区溢出可以在 BLE 上远程触发，直接影响车辆安全
2. **云端安全层缺失**：REST 网关完全未实现鉴权，如果被部署将成为灾难
3. **Android SDK 密钥类型错误**：使用 AES 代替 ECDSA 导致所有近场认证功能不可用
4. **时间机制全线空缺**：从 embedded 到 cloud，所有 TODO 时间戳导致过期检查、回放防护、会话管理全部失效

上述问题多数属于**设计已考虑但实现未完成**的状态。建议按 P0→P3 优先级依次修复，修复后需进行完整的集成测试和安全测试。
