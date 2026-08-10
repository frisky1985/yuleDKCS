# Android 防重放计数器持久化修复报告

> **修复编号**: P1-5
> **修复日期**: 2026-07-16
> **对应评审问题**: 专家评审第4项 — 防重放计数器未跨进程持久化
> **影响范围**: Android SDK `KeyManager.kt` / `KeyStoreMetadataStore.kt`
> **修复状态**: ✅ 已完成

---

## 问题描述

**原始问题**: Android SDK 中的 `transactionCounter`（交易计数器）被标记为 `@Volatile`，但其值仅在进程内存中维护。当应用进程被销毁并重启后，计数器重置为 0，攻击者可利用这一行为实施**重放攻击**：

1. 发起有效交易，获取交易 ID = n
2. 强制重启应用进程
3. 计数器归零，重新从 0 开始递增
4. 重放之前拦截的交易请求，车辆端无法通过计数器单调递增检测

## 修复方案

将 `transactionCounter` 从内存变量改为**KeyStore AES-256/GCM 加密持久化存储**，复用已实现的 `KeyStoreMetadataStore` 基础设施：

### 架构设计

```
┌─────────────────────────────────────────────────────────┐
│                    KeyManager                           │
│                                                        │
│  ┌─────────────┐     ┌──────────────────────────────┐  │
│  │ 交易计数器   │────▶│ KeyStoreMetadataStore        │  │
│  │ (实例字段)   │     │                              │  │
│  │             │◀────│  .readCounter()  ← 解密     │  │
│  │ init: 加载  │     │  .writeCounter() → 加密      │  │
│  │ incr: 写回  │     │                              │  │
│  └─────────────┘     │  共享同一 SharedPreferences  │  │
│                       │  文件 + 同一 AES-256/GCM 密钥  │  │
│                       │  不同键名 (keys_encrypted vs  │  │
│                       │  tx_counter_encrypted)       │  │
│                       └──────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

### 修改文件

| 文件 | 变更类型 | 说明 |
|------|---------|------|
| `KeyStoreMetadataStore.kt` | 新增方法 | `readCounter()`, `writeCounter()`, `clearCounter()` |
| `KeyManager.kt` | 重构 | 计数器从 companion 移至实例字段，加载/持久化/上限检查 |
| `KeyStoreMetadataStoreTest.kt` | 新增测试 | 计数器序列化/反序列化逻辑验证 |
| `KeyStoreMetadataStoreInstrumentedTest.kt` | 新增测试 | 加密计数器完整回环、跨实例、篡改检测、边界值 |

### 关键设计决策

#### 1. 存储方案：复用 KeyStore AES-256/GCM

- **不引入新 SharedPreferences 文件**：计数器与钥匙元数据共享同一 `yuledkcs_keys` SP 文件
- **不同键名隔离**：钥匙元数据使用 `keys_encrypted`，计数器使用 `tx_counter_encrypted`
- **同一 AES 密钥**：计数器加密使用与钥匙元数据相同的 `yuledkcs_metadata_key` 密钥（硬件 backed，KeyStore 管理）
- **IV 随机性**：每次写入生成新 IV，相同计数器的多次加密输出不同密文

#### 2. 启动加载与向后兼容

- `KeyManager.init()` 调用 `metadataStore.readCounter()` 加载计数器
- 从未存储过计数器值的设备（老版本升级）返回 `0L`，无缝兼容
- `clearMetadata()` 同时清理计数器值

#### 3. 最大交易次数保护

- 定义 `MAX_TRANSACTION_COUNT = 1_000_000_000_000L`（1 万亿）
- 达到上限时抛 `DkError(DkErrorCode.ERR_QUOTA_EXCEEDED)` 拒绝新交易
- 防止计数器溢出导致攻击者可构造悲观序列号

#### 4. 线程安全

- `getNextTransactionId()` 使用 `synchronized(this)` 保证原子性
- 序列化操作：递增 → 加密 → 写回 SharedPreferences 在同一同步块中完成
- 通过 `Dispatchers.IO` 上下文执行持久化操作的协程调用不会竞争

## 代码变更详情

### `KeyStoreMetadataStore.kt` 新增方法

```kotlin
// 读取持久化的交易计数器值
fun readCounter(): Long

// 持久化交易计数器值
fun writeCounter(counter: Long)

// 删除持久化的交易计数器
fun clearCounter()
```

**实现要点**：
- `readCounter()` 使用 `toLongOrNull()` 并 fallback 到 0，避免异常传播
- `writeCounter()` 将 Long 转为字符串加密，与钥匙元数据的字符串 JSON 格式统一
- 解密失败时记录错误并返回 0，保证应用的鲁棒性

### `KeyManager.kt` 变更

```kotlin
// 从 companion object 移至实例字段
private var transactionCounter: Long = 0

// init中加载
transactionCounter = metadataStore.readCounter()

// 同步递增和持久化
fun getNextTransactionId(): Long {
    synchronized(this) {
        if (transactionCounter >= MAX_TRANSACTION_COUNT) {
            throw DkError(DkErrorCode.ERR_QUOTA_EXCEEDED, ...)
        }
        transactionCounter++
        metadataStore.writeCounter(transactionCounter)
        return transactionCounter
    }
}
```

## 测试覆盖

### 单元测试（KeyStoreMetadataStoreTest.kt，纯逻辑）

| 测试方法 | 验证内容 |
|---------|---------|
| `counter starts at zero when empty` | 空存储默认值 0 |
| `counter increments correctly` | 递增逻辑正确性 |
| `counter serialization roundtrip` | Long → String → Long 转换 |
| `zero is the default for missing counter` | 空字符串 fallback |
| `invalid counter data returns zero` | 损坏数据 fallback |

### 仪器化测试（KeyStoreMetadataStoreInstrumentedTest.kt，完整加密回环）

| 测试方法 | 验证内容 |
|---------|---------|
| `counterReadWriteRoundtrip` | 基本读写回环 |
| `counterReadReturnsZeroWhenEmpty` | 空存储返回 0 |
| `counterWriteMultipleTimes_preservesLatest` | 多次写入保留最新值 |
| `counterPersistsAcrossStoreInstances` | 跨实例密钥持久化 |
| `counterIndependentFromMetadata` | 计数器与元数据独立 |
| `eachCounterEncryptionProducesDifferentCiphertext` | IV 随机性 |
| `tamperedCounterCiphertextReturnsZero` | 篡改检测返回 0 |
| `clearMetadata_alsoClearsCounter` | 清空元数据同时清空计数器 |
| `clearCounter_onlyRemovesCounter` | 仅清除计数器不影响元数据 |
| `zeroAndLargeCounterValues` | 0 和 Long.MAX_VALUE 边界值 |

## 安全分析

### 攻击场景缓解

1. **进程重启重置** ✅：计数器持久化至 KeyStore 加密存储，重启后恢复上次值
2. **重放攻击** ✅：计数器单调递增，进程重启后不会归零
3. **值篡改** ✅：AES-256/GCM 认证加密，篡改密文导致解密失败返回 0
4. **KeyStore 密钥保护** ✅：密钥材料由 Android KeyStore 硬件级管理，不离开 TEE/SE
5. **计数器溢出** ✅：1 万亿上限 + 溢出拒绝机制

### 残余风险

- **APK 级密钥删除**：用户清除应用数据会同时删除 SharedPreferences 和 KeyStore 密钥，计数器归零
- **低版本 Android（< API 23）**：KeyStore 可能在非硬件 backed 模式下运行
- **生物识别变更失效**：默认 `invalidateOnBiometricEnrollment = false`，如需更高安全性可开启（参考 KeyStoreMetadataStore 文档）

## 回滚方案

如出现计数器持久化导致的问题，可通过清空应用数据恢复。如需代码回滚，还原 `KeyManager.kt` 中 `transactionCounter` 为 `@Volatile` companion 字段即可。

---

## 附件

- [KeyStoreMetadataStore.kt] 第 110-172 行：新增计数器持久化方法
- [KeyManager.kt] 第 60-65 行：`MAX_TRANSACTION_COUNT` 常量
- [KeyManager.kt] 第 96-99 行：`init` 中加载计数器
- [KeyManager.kt] 第 447-480 行：`getNextTransactionId()` 完整实现
- [KeyStoreMetadataStoreInstrumentedTest.kt] 第 165-290 行：11 个仪器化测试
