# yuleDKCS 安全修复代码审查报告

**审查日期**: 2026-05-31
**审查范围**: 18 个变更文件 (Embedded C 6 + Android 4 + iOS 2 + Go Backend 6)
**审查工具**: Hermes Agent (DeepSeek V4 Flash)

---

## 总体评价

修复质量整体良好。20 个已报告漏洞中，**16 个得到完全验证 ✅**。
C 代码的内存安全改进是最彻底的（`iccoa_dk30.c` 基本重写了帧解析逻辑）。
Android/iOS 层的密钥存储修复正确。Go 后端的 JWT 中间件和 RevokeKey 实现填补了主要空白。

但发现了 **3 个新漏洞（CR-1、CR-2、CR-3）**，其中 2 个高危。

**总计：16/20 修复通过 ✅ | 3 个新发现问题 🔴 | 需要立即修复 CR-1/CR-2/CR-3**

---

## 一、嵌入式C（6 个文件）

### 1. sys_time.h (新增) — 时间抽象层 ✅
**修复漏洞**: V-05 时间戳基础设施

| 检查项 | 结果 |
|--------|------|
| 是否解决了对应漏洞 | ✅ 提供了 `sys_tick_get_ms()` 单调递增毫秒计数器 |
| 新引入安全问题 | ✅ 无 |
| 内存安全 | ✅ 无动态分配，纯 header-only + 声明 |
| 边界条件 | ✅ 明确注释了 uint32_t 49.7 天回绕，且指出无符号减法正确处理回绕 |

**评价**: 设计干净。未来可考虑添加 `sys_tick_get_ms64()` 支持长运行系统。

---

### 2. security_auth.c — ICCE 时间戳替换 ✅
**修复漏洞**: V-05 (高危) / V-10 (中危)

| 检查项 | 结果 |
|--------|------|
| 是否解决了对应漏洞 | ✅ 全部 `uint32_t current_time = 0` 替换为 `sys_tick_get_ms()` |
| 新增安全问题 | ✅ 无 |
| 会话过期 | ✅ `cleanup_expired_sessions()` 正确使用系统时间 |
| Nonce 清理 | ✅ `cleanup_expired_nonces()` 60 秒老化 |
| 密钥清除 | ✅ `security_destroy_session()` 正确 memset 会话密钥 |

**⚠️ 小问题**: `security_decrypt()` 在 `ciphertext_len < 28 (12+16)` 时 `enc_len` 会下溢。虽然后续检查会触发错误，但严格来说应该在减法前做完整校验。

---

### 3. offline_decision.c — 时间戳 + 速率限制 ✅
**修复漏洞**: V-05 (高危) / V-12 (中危)

| 检查项 | 结果 |
|--------|------|
| 是否解决了对应漏洞 | ✅ 全部时间检查使用 `sys_tick_get_ms()` |
| V-12 速率限制满 | ✅ 所有 32 槽满时返回 -1 拒绝 |
| 离线时长计算 | ✅ 正确使用条件表达式避免溢出 |
| 密钥权限时间窗 | ✅ `check_permission()` 使用系统时间校验 |
| 请求时间戳检查 | ✅ `request->timestamp == 0` 拒绝请求 |

**⚠️ 非对齐内存访问** (低危): `check_key_validity()` 中 `*((uint32_t*)&cache_key[4]) = key_id;` — 非对齐访问，在 ARM Cortex-M 上可能触发 HardFault。

---

### 4. iccoa_dk30.c — 栈溢出修复 + seq_num 防回放 ❌ (发现 Bug)
**修复漏洞**: V-02 (高危) / V-08 (中危) / V-09 (中危) / V-20 (低危)

| 漏洞 | 状态 |
|------|------|
| V-02 栈溢出 (send_response 检查) | ✅ 修复 |
| V-02 OOB 校验和 (移除 struct cast) | ✅ 修复 |
| V-08 payload_len 验证 | ✅ 修复 |
| V-09 枚举越界 | ✅ 修复 |
| V-20 seq_num 防回放 | ❌ **有 Bug** |

#### 🔴 CR-1: Seq 回绕 0xFFFF→0 被拒绝

```c
if (seq_num == 0 && g_last_seq_num == 0xFFFF) {
    // 合法回绕：这个case会被忽略！
    // 直接走到下面的 seq_num <= g_last_seq_num → 拒绝
}
```

当前条件判断无法正确处理 `0xFFFF → 0` 的合法回绕。框序从 `0xFFFF` 正常回绕到 `0` 时，会在第三个条件 `seq_num <= g_last_seq_num (0 <= 0xFFFF)` 被拒绝。

**建议修复**:
```c
if (seq_num == 0 && g_last_seq_num == 0) {
    /* First frame ever */
} else if (seq_num == 0 && g_last_seq_num == 0xFFFF) {
    /* Valid wrap from 0xFFFF to 0 — accept */
} else if (seq_num <= g_last_seq_num) {
    return ICCOA_ERR_SECURITY;
}
```

---

### 5. iccoa_auth.c — 权限时间检查 ✅
**修复漏洞**: V-11 (中危)

| 检查项 | 结果 |
|--------|------|
| 是否解决了对应漏洞 | ✅ 使用 `sys_tick_get_ms()` 检查 valid_from/valid_until |
| 上限使用 >= | ✅ `current_time >= g_users[idx].valid_until` 正确 |
| 使用次数限制 | ✅ `max_uses > 0 && used_count >= max_uses` |
| 无新增问题 | ✅ 代码简洁，边界清晰 |

**评价**: 干净的修复。`g_users[idx].used_count++` 在单线程上下文中没问题。

---

### 6. iccoa_dk_core.c — 降级安全策略 ✅
**修复漏洞**: V-16 (中危)

| 检查项 | 结果 |
|--------|------|
| 是否解决了对应漏洞 | ✅ `no_downgrade` 标志阻断 DK4.0→DK3.0 降级 |
| 默认启用 | ✅ `no_downgrade = 1` 在 `iccoa_dk_init()` 中 |
| 运行时检测 | ✅ BLE 数据分发器中检测 DK3.0 SOP 帧并丢弃 |
| 版本设置检查 | ✅ `iccoa_set_version()` 也检查降级 |
| 审计日志 | ✅ `DK_LOG_SEC_WARN()` 记录降级尝试 |
| Query API | ✅ 完整的查询/清除降级尝试标志 |

**评价**: 这是整个代码库中最完善的修复之一。覆盖了所有降级攻击面，无问题。

---

## 二、Android（4 个文件）

### 7. KeyManager.kt — AES→ECDSA + EncryptedSharedPreferences ✅
**修复漏洞**: V-04 (高危) / V-06 (高危)

| 漏洞 | 状态 |
|------|------|
| V-06 密钥类型错误 | ✅ `KeyPairGenerator(EC) + secp256r1 + PURPOSE_SIGN\|VERIFY` |
| V-04 密钥元数据明文 | ✅ `EncryptedSharedPreferences + MasterKey(AES256_GCM)` |
| build.gradle.kts 依赖 | ✅ `androidx.security:security-crypto:1.1.0-alpha06` |

**⚠️ 低危建议**: `keysCache = mutableMapOf()` 依然非线程安全。多个协程同时调用 `createKey/useKey/revokeKey` 可能导致数据丢失。建议改用 `ConcurrentHashMap`。

---

### 8. NfcManager.kt — FLAG_IMMUTABLE ✅
**修复漏洞**: V-18 (低危)

| 检查项 | 结果 |
|--------|------|
| 是否解决了对应漏洞 | ✅ `FLAG_MUTABLE→FLAG_IMMUTABLE` |
| 兼容性 | ✅ Android 12+ 使用 `FLAG_IMMUTABLE`，旧版本使用 `FLAG_UPDATE_CURRENT` |
| 无新增问题 | ✅ |

---

### 9. NfcSecureChannel.kt (新增) — NFC 加密通道 ❌ (发现 Bug)
**修复漏洞**: V-17 (中危)

**设计评估**:
- ✅ ECDH 密钥协商 (secp256r1) + Android KeyStore 保护私钥
- ✅ AES-256-GCM 数据载荷加密
- ✅ 独立发送/接收 Counter 作为 IV（防回放）
- ✅ AAD 绑定 CLA/INS/P1/P2（防命令伪造）
- ✅ 每个 Tag 独立会话，5 分钟 TTL，100 次传输上限
- ✅ `ConcurrentHashMap` 线程安全

#### 🔴 CR-2: secureReadData 使用 buildSecureWriteApdu

**NfcManager.kt:327-328**:
```kotlin
val encryptedReadCommand = secureChannel.buildSecureWriteApdu(
    tagId, p1, p2, byteArrayOf(length.toByte())
)
```

读取操作调用了 `buildSecureWriteApdu`，内部硬编码了 `INS_SECURE_WRITE (0xD6)`。但 `decryptSecureReadResponse` 期望 APDU CLA/INS/P1/P2 中 INS 为 `INS_SECURE_READ (0xB0)`。导致：
1. APDU 命令字节错误（发送 WRITE 命令但意图是 READ）
2. AAD 认证数据不匹配，车端解密时 GCM 认证失败

**建议修复**: 新建 `buildSecureReadApdu()` 方法：
```kotlin
fun buildSecureReadApdu(tagId: String, p1: Byte, p2: Byte, length: Int): ByteArray {
    val aad = byteArrayOf(SecureApduIns.CLA_SECURE, SecureApduIns.INS_SECURE_READ, p1, p2)
    val encryptedPayload = encryptForWrite(tagId, byteArrayOf(length.toByte()), aad)
    return buildApdu(SecureApduIns.CLA_SECURE, SecureApduIns.INS_SECURE_READ, p1, p2, encryptedPayload)
}
```

---

## 三、iOS（2 个文件）

### 10. DigitalKeySDK.swift — API Key 移入 Keychain ✅
**修复漏洞**: iOS API Key 明文内存存储

| 检查项 | 结果 |
|--------|------|
| API Key 不在内存存储 | ✅ `SdkConfig` 中不存储 `apiKey` 为属性 |
| Keychain 写入时机 | ✅ `init` 立即调用 `storeApiKeyToKeychain()` |
| 按需读取 | ✅ `retrieveApiKey()` 从 Keychain 读取 |
| 清理 | ✅ `reset()` 调用 `keychain.deleteApiKey()` |
| 进程 dump 保护 | ✅ Keychain 数据硬件加密 |

**⚠️ 低危**: `storeApiKeyToKeychain` 在 `print()` 中输出错误信息（第107行）。应使用正式日志系统或静默处理。

---

### 11. KeychainManager.swift (新增) — Keychain 封装 ✅
**修复漏洞**: V-17 基础设施

| 检查项 | 结果 |
|--------|------|
| 访问级别 | ✅ `kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly` |
| Data Protection | ✅ `kSecUseDataProtectionKeychain = true` |
| 错误处理 | ✅ OSStatus 错误码中文映射 |
| String/Data 双接口 | ✅ 完整 |
| SDK 扩展 | ✅ `sdkInstance` 单例 + 便捷方法 |

**评价**: 高质量封装，无问题。

---

## 四、Go 后端（6 个文件）

### 12. rest_gateway.go — JWT 认证中间件 ⚠️ 部分修复
**修复漏洞**: V-01 (高危) — 部分修复

| 检查项 | 结果 |
|--------|------|
| JWT 中间件挂载 | ✅ 已挂载到 `/api/v1` 组 |
| Bearer token 解析 | ✅ `strings.TrimPrefix(authHeader, "Bearer ")` |
| 鉴权失败 401 | ✅ |
| user_id/role 上下文注入 | ✅ gin context + gRPC metadata |

**⚠️ (CR-5 中危)**: **16 个 Handler 仍为 Stub** — bindKey, revokeKey 等所有 REST 端点仍是空实现（返回 `{"message": "..."}`）。V-01 的鉴权部分已修复，但业务逻辑 stub 部分未修复。`login` 端点和 `validateToken()` 也是 placeholder。

**建议**: 实现 handler 到 gRPC 的转发逻辑。

---

### 13. key_management.go — RevokeKey 实现 ✅
**修复漏洞**: V-07 (高危)

| 检查项 | 结果 |
|--------|------|
| 适配器查询 | ✅ `registry.GetByVendor(req.Vendor)` |
| TSP 适配器调用 | ✅ `a.RevokeKey(ctx, req)` |
| 适配器容错 | ✅ 失败时记录部分成功 |
| 手机推送通知 | ✅ `notifyPhoneRevocation()` 架构预留 |
| 审计日志 | ✅ `auditLog()` 全部路径覆盖 |

**⚠️ (CR-10 低危)**: UnbindKey, SuspendKey, ResumeKey, RenewKey 仍然是 stub（返回空响应）。建议列入后续迭代。

---

### 14. registry.go — RWMutex 修复 ✅
**修复漏洞**: V-14 (中危)

| 检查项 | 结果 |
|--------|------|
| 原问题 | 双重 RUnlock（显式 + defer） |
| 修复后 | 单一 `defer r.mu.RUnlock()` |
| 读锁迭代安全 | ✅ map 在 RLock 下迭代安全 |

**评价**: 干净修复，无问题。

---

### 15. encoder.go — writeInt 动态长度 ✅
**修复漏洞**: V-13 (低危)

| 检查项 | 结果 |
|--------|------|
| intSize() 正确性 | ✅ 正数用 uintSize，负数用补码最小字节数 |
| uintSize() 正确性 | ✅ 0→1字节，大值→多字节 |
| 边界值验证 | ✅ 正确 |

无问题。

---

### 16. key_service.go — Secret 哈希存储 ❌ (发现遗漏)
**修复漏洞**: V-03 (高危) — 大部分修复

| 检查项 | 结果 |
|--------|------|
| CreateKey 密钥哈希 | ✅ `hashSecret(secret) = salt:SHA256(salt+secret)` |
| 响应不返回 secret | ✅ `Secret: ""` |

#### 🔴 CR-3: ShareKey 方法存储明文 Secret (第 278 行)

```go
Secret: secret,  // ← 明文！未哈希！
```

ShareKey 的密钥 Secret 以明文存储在数据库中。CreateKey 已修复，但 ShareKey 遗漏了同样的问题。

#### 🟠 CR-4: generateShareCode() 弱随机性 (第 298-301 行)

```go
func generateShareCode() string {
    bytes := make([]byte, 4)
    rand.Read(bytes)
    return fmt.Sprintf("%06d", bytes[0]%1000000)  // 仅使用 4 个随机字节中的 1 个！
}
```

`bytes[0]%1000000` 最多产生 1,000,000 个可能值（< 20 位熵），攻击者可暴力枚举。

---

### 17. middleware.go — goroutine 泄露 + AuthInterceptor ✅
**修复漏洞**: V-15 (低危) / V-19 (低危)

| 漏洞 | 状态 |
|------|------|
| V-19 AuthInterceptor 绕过 | ✅ `HasSuffix → 精确匹配` `/grpc.health.v1.Health/Check` |
| V-15 goroutine 泄露 | ✅ `stopCh + Stop()` 方法 |
| Stop() 幂等 | ✅ `select` 带 `default` 防止重复关闭 |

**⚠️ (CR-6 中危)**: `LoggingInterceptor` 缺失 `fmt` 导入 — 第 72 行 `fmt.Sprintf("%+v", req)` 使用了 `fmt` 包，但 `import` 块中不存在 `fmt`，会导致编译失败。

---

## 完整问题汇总

| 编号 | 严重程度 | 文件 | 问题 | 建议 |
|------|---------|------|------|------|
| **CR-1** | 🔴 高危 | `iccoa_dk30.c` | Seq 回绕 0xFFFF→0 被拒绝 | 修复条件判断，添加合法回绕分支 |
| **CR-2** | 🔴 高危 | `NfcSecureChannel.kt` / `NfcManager.kt` | READ 使用 WRITE APDU，AAD/INS 不匹配 | 创建 `buildSecureReadApdu()` |
| **CR-3** | 🔴 高危 | `key_service.go:278` | ShareKey 明文存储 Secret | 改用 `hashSecret(secret)` |
| CR-4 | 🟠 中危 | `key_service.go:298-301` | generateShareCode() 弱随机 | 使用全部 4 字节 |
| CR-5 | 🟠 中危 | `rest_gateway.go` | 16 个 Handler 仍是 stub | 实现 gRPC 转发逻辑 |
| CR-6 | 🟠 中危 | `middleware.go` | LoggingInterceptor 缺失 fmt import | 添加 "fmt" 导入 |
| CR-7 | 🟢 低危 | `offline_decision.c` | 非对齐内存访问 | 使用 memcpy 替代指针类型转换 |
| CR-8 | 🟢 低危 | `DigitalKeySDK.swift:107` | Keychain 错误 print() | 使用正式日志 |
| CR-9 | 🟢 低危 | `KeyManager.kt` | keysCache 非线程安全 | 改用 ConcurrentHashMap |
| CR-10 | 🟢 低危 | `key_management.go` | UnbindKey 等 4 方法 stub | 列入后续迭代 |

---

## 各漏洞修复状态汇总

| 漏洞 | 原始等级 | 修复状态 |
|------|---------|---------|
| V-01 REST 网关无鉴权 | 🔴 高危 | ⚠️ 部分修复 (JWT 已挂载但 handler 仍为 stub) |
| V-02 ICCOA DK3.0 栈溢出 | 🔴 高危 | ✅ 修复 |
| V-03 CreateKey 明文返回 Secret | 🔴 高危 | ✅ 修复 (但 **CR-3** ShareKey 遗漏) |
| V-04 Android 密钥元数据明文 | 🔴 高危 | ✅ 修复 (EncryptedSharedPreferences) |
| V-05 嵌入式 C 时间戳为 0 | 🔴 高危 | ✅ 修复 (sys_time.h 抽象层) |
| V-06 Android 生成 AES 而非 ECDSA | 🔴 高危 | ✅ 修复 (KeyPairGenerator EC) |
| V-07 HUB 层 RevokeKey 空函数 | 🔴 高危 | ✅ 修复 |
| V-08 payload_len 不验证 | 🟠 中危 | ✅ 修复 |
| V-09 控制指令枚举越界 | 🟠 中危 | ✅ 修复 |
| V-10 ICCE 挑战过期失效 | 🟠 中危 | ✅ 修复 |
| V-11 ICCOA 权限时间未实现 | 🟠 中危 | ✅ 修复 |
| V-12 速率限制槽满放行 | 🟠 中危 | ✅ 修复 |
| V-13 writeInt 固定 8 字节 | 🟡 低危 | ✅ 修复 |
| V-14 Registry RWMutex 锁错误 | 🟠 中危 | ✅ 修复 |
| V-15 Token Bucket goroutine 泄露 | 🟡 低危 | ✅ 修复 |
| V-16 DK4.0→DK3.0 降级安全 | 🟠 中危 | ✅ 修复 |
| V-17 NFC APDU 明文传输 | 🟠 中危 | ✅ 修复 (但 **CR-2** READ 指令错误) |
| V-18 PendingIntent FLAG_MUTABLE | 🟡 低危 | ✅ 修复 |
| V-19 AuthInterceptor 匹配过宽 | 🟡 低危 | ✅ 修复 |
| V-20 seq_num 无防回放 | 🟡 低危 | ❌ **CR-1** 回绕逻辑 Bug |

---

## 按平台新发现的问题

### 嵌入式 C — 2 个新问题
1. 🔴 CR-1: `iccoa_dk30.c` seq_num 回绕逻辑缺陷
2. 🟢 CR-7: `offline_decision.c` 非对齐内存访问

### Android — 2 个新问题
1. 🔴 CR-2: NfcSecureChannel READ 使用 WRITE APDU
2. 🟢 CR-9: KeyManager keysCache 非线程安全

### iOS — 1 个新问题
1. 🟢 CR-8: Keychain 错误使用 print() 输出

### Go 后端 — 4 个新问题
1. 🔴 CR-3: ShareKey 明文存储 Secret
2. 🟠 CR-4: generateShareCode() 弱随机性
3. 🟠 CR-5: REST handler 仍为 stub
4. 🟠 CR-6: LoggingInterceptor 缺失 fmt import
5. 🟢 CR-10: UnbindKey 等方法 stub

---

## 需要立即修复的项 (P0)

1. **CR-1** 🔴 — `iccoa_dk30.c` seq_num 回绕逻辑 → 合法回绕被拒绝
2. **CR-2** 🔴 — `NfcSecureChannel.kt` / `NfcManager.kt` READ 使用 WRITE APDU → NFC 安全读取完全不可用
3. **CR-3** 🔴 — `key_service.go:278` ShareKey 明文 Secret → 与 V-03 相同的安全问题
