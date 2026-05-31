# yuleDKCS 协议标准符合性审计 — 独立审查验证报告

**审查人**: Hermes 独立验证  
**基准文档**: STANDARDS-AUDIT-CLAUDE.md  
**审查日期**: 2026-05-31  
**抽样深度**: 12 个源文件 + 6 个配置/文档 + 3 层交叉比对  

---

## 一、抽样验证结果

### ✅ Claim 1: BLE UUID 跨层断裂 — 基本正确，但需修正

| 层 | Claude 声称 | 我验证的结果 | 裁决 |
|---|---|---|---|
| **CCC 嵌入式 SPEC** | 0xEFFF vs 0xFFD1 | SPEC.md L164 确认 0xEFFF。但代码 (ble_kw47a.c) 从不引用此常量 — 它通过 SPI 驱动 KW47A 芯片，UUID 烧录在芯片固件里，不在代码层定义 | ✅ 结论正确，但位置有偏差 |
| **ICCE 嵌入式** | 0xFEFA / 0x18F0 | icce_digital_key.h L41 定义 0xFEFA；module_design.md L237 定义 0x18F0。代码实际引用 GATT_UUID_DIGITAL_KEY_SERVICE，但该常量在任意头文件中均未定义（搜索 0 匹配） | ✅ 存在不一致，且更严重 |
| **ICCOA 嵌入式** | 0xFEF5 (正确) | iccoa_digital_key.h L38 确认 ICCOA_SERVICE_UUID 0xFEF5 | ✅ |
| **Android SDK** | 0xFEFF | BleManager.kt L20 确认 0000FEFF-... | ✅ |
| **iOS SDK** | 0xFDE2-FDE7 | BleManager.swift L78-89 确认 | ✅ |

### ✅ Claim 2: ICCE 国密完全缺失 — 确认

**security_auth.c 逐行稽查:**
- `KEY_TYPE_ECC_P256_PUBLIC` (L153) — P-256 非 SM2
- `crypto_aes_gcm_encrypt` (L265) — AES-GCM 非 SM4
- `crypto_sha256` (L340) — SHA-256 非 SM3
- `hsm_ecdsa_sign` (L345) — ECDSA 非 SM2
- `hsm_ecdh_compute_shared` (L211) — ECDH 非 SM2

**icce_security.c 同样:** `device_pubkey[64]` (L11, P-256 格式)、注释 "64 bytes (r \|\| s) P-256" (L54)、`/* TODO: SE050 ECDSA verify */` (L59)

**全项目搜索:** `SM2|SM3|SM4|gmssl|GMSSL` — 0 个实际代码匹配（仅 README 和 CLOUD-DEV-GUIDE 文档提及）

### ✅ Claim 3: iOS 缺少 UWB/NFC — 确认

**iOS Sources 全部文件清单:**

| 文件 | import |
|---|---|
| BleManager.swift | CoreBluetooth ✅ |
| DigitalKeySDK.swift | Foundation, Combine, os.log |
| KeychainManager.swift | Foundation, Security |
| BertlvEncoder/Decoder | Foundation |
| DkError.swift | Foundation |
| DkLogger.swift | Foundation, os.log |
| DkTelemetry.swift | Foundation, UIKit |

**缺失:** CoreNFC ❌、NearbyInteraction ❌、任何 UWB/NFC 类型 ❌

### ✅ Claim 4: ICCE TSP Adapter 为 stub — 确认

**IcceAdapter.java** 确实为 Template Method 模式的骨架实现:
- `doGetVehicles()`: 返回 `List.of()` (空列表) + 注释 `// Add actual ICCE vehicle parsing`
- `doRequestKeys()`: 返回 `"icce-key-" + System.currentTimeMillis()` 模拟值
- 无 `CccClient` 或 `IccoaClient` 等效的 HTTP 客户端

相比之下 `CccAdapter.java` 有真实 `CccClient`，`IccoaAdapter.java` 有真实 `IccoaClient`。

### ✅ Claim 5: CCC Security 为 placeholder — 确认

**security.c** 每条函数:
| 函数 | 行为 | 状态 |
|---|---|---|
| `sec_init` | `g_sec_initialized = true; return CCC_OK;` | 无真实 SE050 初始化 |
| `sec_scp03_open` | `(void)ch; return CCC_OK;` | 完全空实现 |
| `sec_encrypt` | `*out_len = 12 + len + 16; return CCC_OK;` | 不加密 |
| `sec_decrypt` | `*out_len = len - 28; return CCC_OK;` | 不解密 |
| `sec_sign` | `sig_len = 64; return CCC_OK;` | 不签名 |
| `sec_verify` | `return VERIFY_OK;` | 永远通过 |

所有真实 SE050 调用在注释中: `/* Platform-specific: Se05x_...() */`

### ✅ Claim 6: ICCE CAN 时间处理有 bug — 确认，且比 Claude 说的严重

**vehicle_integration.c L140-143:**
```c
uint32_t start_time = 0;  // TODO: 获取实际时间
// ...
while ((0 - start_time) < timeout) {  // TODO: 时间比较
```
- `start_time` 被设为 0，且从未更新
- `0 - start_time = 0` (无符号 32 位)，所以 `0 < 500` 为 true
- **结果:** 这是一个**无限循环**，而非简单的时间比较 bug
- **影响:** `vehicle_execute_command()` 挂死所有 CAN 命令

**Claude 将其评为 P3(低优先级) ⚠️ 我不同意 — 这是一个触发即崩溃的 bug，至少 P1。**

---

## 二、Claude 遗漏的重要发现 ⚠️

### 发现 #1: ICCE GATT UUID 常量未定义（编译失败）

`ble_manager.c` 引用了 `GATT_UUID_DIGITAL_KEY_SERVICE`、`GATT_UUID_KEY_STATUS` 等 **6 个常量**，但在整个 `embedded/icce_protocol/` 目录中没有任何头文件定义了它们。`#define GATT_UUID_` 在 ICCE 目录内 0 匹配。

唯一定义在 `docs/module_design.md L237` — 设计文档中，不是可编译的代码。

**影响:** ICCE BLE 模块无法通过编译。

### 发现 #2: README.md 声称 ICCE SM2/SM3/SM4 "✅ 完成" — 严重文档失真

**README.md L93:**
```
| ICCE | T/CA 110-2020（国密 SM2/SM3/SM4） | ✅ 完成 |
```

Claude 未交叉比对这个声称。实际上代码中完全没有国密实现。这是一个**文档–代码一致性**问题，修复时需同时更新 README 和实现。

### 发现 #3: 全项目 0 个可执行测试用例

所有测试文档 (TEST_CASES_CCC.md, TEST_CASES_ICCE.md, TEST_CASES_ICCOA.md, TEST_PLAN.md) 都是自然语言的测试计划/用例描述，没有任何可执行的测试代码。

Claude 提到了 "仅 4 个模型初始化测试"，但未明确指出: **整个代码库没有任何单元测试、集成测试或协议合规测试的可执行实现。测试计划是纯文档产品。**

`frontend/ios-tests/DigitalKeyAppTests/DigitalKeyAppTests.swift` 存在但内容未验证。

### 发现 #4: CCC SPEC.md 中 0xEFFF 未在 C 代码中使用

`ble_kw47a.c` 完全不引用 0xEFFF 常量。CCC 的 BLE UUID 定义仅存在于 SPEC.md 的文档示例中，不是一个可被任何代码包含的 `#define` 或引用。这意味着:
- 车端 CCC BLE 的 UUID 实际上由 KW47A 芯片固件决定（SPI 写入）
- SPEC.md 中的 0xEFFF 定义是一个假象 — 代码实际不受该常量控制

### 发现 #5: Android NFC NfcSecureChannel 存在但未在 iOS 对应

Claude 已指出 iOS 缺少 NFC/UWB，但未强调: Android 的 `NfcSecureChannel.kt`（ISO 7816-4 APDU + 安全通道）是协议规范的核心要求之一。iOS 不仅缺少 NFC 硬件调用，连对应的 APDU 层也完全缺失。差距比 "仅有错误码定义" 更大。

---

## 三、优先级修正建议

| 原始项 | Claude 优先级 | 建议修正 | 原因 |
|---|---|---|---|
| BLE UUID 统一 | P0 | ✅ **保持 P0** | |
| ICCE 算法策略决策 | P0 | ⬆️ **更高优先级** | README 声称 "✅ 完成"，实际完全缺失 — 需立即决策用国密还是通用算法，同时修正 README |
| iOS UWB | P0 | ✅ **保持 P0** | |
| iOS NFC | P0 | ✅ **保持 P0** | |
| CCC Security | P1 | ✅ **保持 P1** | |
| CCC Key Mgmt | P1 | ✅ **保持 P1** | |
| ICCE SEC | P1 | ✅ **保持 P1** | |
| BLE UUID App 端 | P1 | ✅ **保持 P1** | |
| CCC NFC NDEF | P2 | ⬆️ **应升至 P1** | 规范要求，也是 UWB OOB 配对的前置条件 |
| 后端 ICCE Adapter | P2 | ✅ **保持 P2** | |
| 证书链验证 | P2 | ⬆️ **应升至 P1** | 安全隐患 — sec_verify 直接返回成功 |
| 协议测试 | P2 | ✅ **保持 P2** | Claude 说 "编写单元测试"，但当前实际是 0 个可执行测试 |
| App mock → 真实 | P3 | ✅ **保持 P3** | |
| CAN 时间处理 | P3 | ⬆️ **应升至 P1** | 无限循环 bug，非 "时间比较 bug" |
| 国密算法 | P3 | ⬆️ **应升至 P0-P1** | 取决于 ICCE 策略决策 |

### 新增修复项（Claude 遗漏）

| 优先级 | 修复项 |
|---|---|
| **P0** | 定义 ICCE GATT UUID 头文件（当前编译失败） |
| **P0** | 修正 README ICCE 完成度标记（文档–代码不一致） |
| **P1** | CAN vehicle_execute_command 超时循环修复（无限循环） |

---

## 四、最终结论

### ✅ 我认同的 Claude 结论

1. **BLE UUID 跨层断裂** — 4 层各有不同值，ICCOA 除外 ✅
2. **ICCE 国密完全缺失** — 代码100%使用 ECDSA/SHA256/AES，无 SM2/SM3/SM4 ✅
3. **iOS UWB/NFC 缺失** — 仅 BLE 实现 ✅
4. **ICCOA 实现最完整** — 头文件、帧协议、安全模块结构一致 ✅
5. **CCC Security 为 placeholder** — 所有加密函数返回空值 ✅
6. **CCC Key Mgmt 内存存储** — 有 SE050 TODO 但未实现 ✅
7. **ICCE TSP Adapter 为 stub** — 正确 ✅
8. **Android NFC/UWB 完整** — NfcManager (569行) + UwbManager (302行) ✅
9. **App 层 mock 数据** — 两端均真实 ✅
10. **测试覆盖极低** — 0 个可执行测试用例 ✅

### ❌ 我不同意或需修正的结论

1. **CAN 时间处理评为 P3** — 实际是 `while ((0 - 0) < timeout)` **无限循环**，应升为 **P1**
2. **CCC BLE UUID "使用" 0xEFFF** — SPEC.md 中定义了 0xEFFF，但 `ble_kw47a.c` 从未引用，UUID 实际在 KW47A 芯片固件中。说法不精确
3. **国密算法评为 P3 "如果 ICCE 是必须的"** — README 明确声明 ICCE 需要 SM2/SM3/SM4，应由你确认后决定 P0/P1，而不是 P3。文档失真必须先修

### ⚠️ Claude 遗漏的重要项

1. **ICCE GATT UUID 常量未在任何头文件定义** — 编译错误级别，**P0**
2. **README.md 声称 ICCE SM2/SM3/SM4 "✅ 完成"** — 严重文档–代码不一致
3. **0 个可执行测试** — 不仅 "覆盖低"，而是**没有一行可运行的测试代码**
4. **Android NfcSecureChannel 存在 → iOS 差距比 "仅有错误码" 更大** — iOS 连 APDU 层都没有
5. **CCC security 的 sec_verify 直接返回 VERIFY_OK** — 任何伪造的签名都能通过验证
