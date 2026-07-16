# P0: Android 钥匙元数据存储迁移 — EncryptedSharedPreferences → Android KeyStore

**日期**: 2026-07-16
**组件**: Android SDK (frontend/android/)
**负责人**: 小克 (编码)
**问题编号**: yuleDKCS P0-3

---

## 问题概述

Android 端钥匙元数据（DigitalKey 的 JSON 序列化）原使用 `EncryptedSharedPreferences` 存储。

专家指出：
- 「Android KeyStore 是更安全的硬件级存储」
- 「OEM 审计的硬性要求」

`EncryptedSharedPreferences` 虽是加密存储，但其主密钥（MasterKey）不强制硬件 backing，无法满足部分 OEM 对「密钥材料永不离开 TEE/StrongBox」的审计要求。

---

## 解决方案

### 架构变更

```
Before:
DigitalKey JSON → EncryptedSharedPreferences (MasterKey AES256_GCM)

After:
DigitalKey JSON → AES-256/GCM (Android KeyStore 回话密钥) → Base64 → SharedPreferences
```

引入 **`KeyStoreMetadataStore`** 作为元数据存储层：

| 层 | 职责 |
|---|---|
| **Android KeyStore** | 硬件级持久化 AES-256/GCM 密钥 |
| **`KeyStoreMetadataStore`** | 加密/解密 + Base64 编解码 + SharedPreferences 读写 |
| **`KeyManager`** | 调用 `KeyStoreMetadataStore.readMetadata/writeMetadata` |

### 新增文件

| 文件 | 大小 | 说明 |
|---|---|---|
| `src/main/kotlin/.../key/KeyStoreMetadataStore.kt` | ~8.2 KB | 核心加密存储层 |
| `src/test/kotlin/.../key/KeyStoreMetadataStoreTest.kt` | ~7.1 KB | 单元测试（JSON 序列化 + 业务逻辑） |
| `src/androidTest/.../key/KeyStoreMetadataStoreInstrumentedTest.kt` | ~6.9 KB | 仪器化测试（KeyStore 回话） |

### 修改文件

| 文件 | 变更 |
|---|---|
| `src/main/kotlin/.../key/KeyManager.kt` | 移除 `EncryptedSharedPreferences` / `MasterKey` 导入，改用 `KeyStoreMetadataStore` |
| `build.gradle.kts` | 添加 `androidx.test:runner` 依赖；注释说明 `security-crypto` 降级为迁移用 |

---

## 关键 API — KeyStoreMetadataStore

```kotlin
class KeyStoreMetadataStore(
    context: Context,
    prefsName: String = "yuledkcs_keys",
    keyAlias: String = "yuledkcs_metadata_key"
)

fun readMetadata(): String?       // 读取解密后 JSON，失败返回 null
fun writeMetadata(plaintextJson: String)  // 加密并写入
fun clearMetadata()                     // 清除所有存储
fun migrateFromLegacyIfNeeded()         // 从 EncryptedSharedPreferences 迁移
```

### 加密格式

```
SharedPreferences("yuledkcs_keys") {
    "keys_encrypted" => Base64([IV (12B)][AES-GCM ciphertext])
}
```

- 算法: `AES/GCM/NoPadding`
- 密钥大小: 256 bits
- IV: 12 bytes (每次加密随机生成)
- GCM Tag: 128 bits
- Base64: `NO_WRAP` 模式

### 数据迁移

`KeyManager.init` 调用 `metadataStore.migrateFromLegacyIfNeeded()`：

1. 检查 `migration_complete` 标志是否已设
2. 用 `EncryptedSharedPreferences` 读取旧 `"digital_keys_encrypted"` 文件
3. 用 `KeyStoreMetadataStore.writeMetadata()` 重加密到新存储
4. 清理旧 SP 文件
5. 设置 `migration_complete = true`

迁移仅执行一次，失败则下次重试，不阻塞正常初始化。

---

## 安全分析

### 优势（对比 EncryptedSharedPreferences）

| 维度 | EncryptedSharedPreferences | KeyStoreMetadataStore |
|---|---|---|
| 密钥材料位置 | 软件级 Tink 派生密钥 | Android KeyStore（TEE/StrongBox） |
| 密钥导出 | 低熵 PIN 派生 | 硬件 TRNG 生成 |
| OEM 审计 | ❌ 不满足硬件级要求 | ✅ 满足 |
| 加密算法 | AES256-GCM + SIV（双层） | AES-256/GCM（单层） |
| 抗篡改 | ✅ (GCM 认证加密) | ✅ (GCM 认证加密) |
| 恢复性 | 密钥可备份 | 密钥不可导出（硬件级） |

### 风险

- **密钥不可导出**: 设备重置后密钥丢失，元数据不可恢复。需要从云端重新同步钥匙数据。
- **StrongBox 兼容性**: Android 9+ 设备基本支持 TEE，部分低端设备可能退化到软件实现。
- **迁移失败**: 极少数情况下旧加密数据损坏，迁移静默跳过，用户需重新绑定钥匙。

---

## 测试覆盖

### 单元测试（test/）

| 测试名称 | 验证内容 |
|---|---|
| `serialize and deserialize roundtrip preserves all fields` | DigitalKey 所有字段 JSON 回话 |
| `serialize with null maxUses and shareCode` | 空值处理 |
| `all key types roundtrip correctly` | 4 种 KeyType 回话 |
| `all key statuses roundtrip correctly` | 5 种 KeyStatus 回话 |
| `isValid returns true/false` | 有效期、状态、使用次数判断 |
| `remainingUses` | 剩余次数计算 |

### 仪器化测试（androidTest/）

| 测试名称 | 验证内容 |
|---|---|
| `writeAndReadRoundtrip` | 基本加密/解密回话 |
| `readReturnsNullWhenEmpty` | 空存储返回 null |
| `writeMultipleTimes_roundtripPreservesLatest` | 多次写入最后写入有效 |
| `clearMetadata_removesAllData` | 清除后返回 null |
| `eachEncryptionProducesDifferentCiphertext` | IV 随机性 |
| `writeAndReadLargeJson` | 200 把钥匙数据回话 |
| `keyIsReusedAcrossStoreInstances` | 跨实例密钥复用 |
| `tamperedCiphertextReturnsNull` | 抗篡改 |
| `writeAndReadUnicodeText` | 中英文+Emoji |
| `migrationFlagIsSetAfterRead` | 迁移标志逻辑 |

---

## 运行测试

```bash
# 单元测试（JVM，无需设备）
cd frontend/android
./gradlew test

# 仪器化测试（需 Android 模拟器或设备）
./gradlew connectedAndroidTest
```

---

## 技术债务

- 运行期仍保留 `security-crypto` 依赖（仅用于迁移），可在所有用户完成迁移后的版本中移除。
- 建议增加 CloudKit 旁路（如果设备 KeyStore 损坏，通过云端验证恢复钥匙）。
