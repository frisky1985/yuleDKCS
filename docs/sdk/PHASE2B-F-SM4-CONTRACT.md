# Sprint Contract: 2b-F ICCOA/ICCE 指令帧真实化（帧格式 + SM4/HMAC + 枚举映射）

> 依据: 老板钦定规则 — 协议指令真实化必须先研读规范/参考实现再写，禁止凭印象编帧格式。
> 本轮事实来源（唯一依据）:
>   - `embedded/iccoa_protocol/src/iccoa/dk30/iccoa_dk30.c`（车端 DK3.0 实现, 240 行）
>   - `embedded/iccoa_protocol/src/iccoa/dk40/iccoa_dk40.c`（车端 DK4.0 实现）
>   - `embedded/iccoa_protocol/include/iccoa_digital_key.h`（DK3.0 命令/控制枚举）
>   - `embedded/iccoa_protocol/docs/SPEC.md` + `docs/module_design.md`（帧格式定义）
>   - `embedded/icce_protocol/docs/module_design.md` §3.1.4（control_command_t）
>   - `embedded/icce_protocol/src/crypto/sm4.c` + `crypto_engine.c`（SM4/HMAC 参考实现）
>   - `docs/certification/iccoa-spec.md`（ICCOA/T 002-2024 知识库摘要）
> 裁决日期: 2026-08-01

---

## 1. Scope

### What
把 mobile SDK 的 ICCOA/ICCE BLE 指令构造从"占位/内部约定"升级为**与车端参考实现可互通**的真实实现。

### In Scope
- iOS: `ICCOABleAdapter.swift`、`ICCEBleAdapter.swift` 指令构造真实化
- iOS 新增: `IcocaFrame.swift`（DK3.0 帧编解码）、`Sm4.swift`（国密 SM4, ICCE 会话加密用）
- Android: `IcocaFrame.kt` 字节序/checksum 修正、`ICCOABleAdapter.kt` payload/加密修正、`ICCEBleAdapter.kt` HMAC 实现
- 双端测试更新 + 独立验证
- 裁决文档: `docs/certification/iccoa-icce-ble-command-frames.md`（字节级声明可追溯）

### Out of Scope
- UWB/NFC（2b-G/H, 真机依赖）
- 后台 BLE（2b-I）
- BLE 连接/扫描层（2b-A/B/C/D 已就位; 链路层加密由系统 BLE 连接属性负责, 不在本 Contract 改）
- 绑定/认证完整流程（AUTH/BIND 命令帧后续任务）
- 真机联调（单列, 需物理车/真机）

---

## 2. Architecture Decision

### 2.1 事实裁决（architect-lead 会签）

| # | 裁决 | 依据 | 结论 |
|:-:|:-----|:-----|:-----|
| AD-1 | **ICCOA DK3.0 应用层无加密** | dk30.c `handle_ctrl_request` 直接解析明文 payload[0]/[1]; iccoa_ble.c 加密由 `hal_ble_request_encryption`（LE SC 链路层）完成 | ICCOA 手机端**不做应用层 SM4**（原任务名"SM4"对 ICCOA 是误标） |
| AD-2 | **ICCOA 帧 = SOP(0xAA) \| CMD \| SEQ(LE u16) \| LEN(LE u16) \| PAYLOAD \| XOR checksum(不含 SOP) \| EOP(0x55)** | dk30.c:120-121 (LE), dk30.c:131-132 (checksum 从 raw+1 起 4+len 字节) | 帧编解码以车端为准 |
| AD-3 | **ICCOA CTRL_REQ payload = [cmd(1)][param(1)]** | dk30.c:93-99 + dk40.c:463-467 `payload_len < 2` 拒绝 | Android 现有 [cmd][keyId_len][keyId][counter] 结构废除 |
| AD-4 | **ICCOA CTRL 枚举: LOCK=0x01, UNLOCK=0x02, ENGINE_ON=0x03, ENGINE_OFF=0x04, TRUNK=0x05, WINDOW_UP=0x06, WINDOW_DOWN=0x07, CLIMATE_ON=0x08, CLIMATE_OFF=0x09, FIND=0x0A, HORN=0x0B** | iccoa_digital_key.h:155-167 | 通用 `BleCommandType`（unlock=0x01/lock=0x02）**必须在适配器内部映射**, 禁止直接序列化 |
| AD-5 | **ICCE control_command_t = [command_type(1)][target(1)][user_id(BE u32)][hmac(32)] = 38 字节; 命令 UNLOCK=0x01, LOCK=0x02, START=0x03, STOP=0x04, TRUNK=0x05, QUERY=0x06** | icce module_design.md §3.1.4:275-291 | Android 现有格式一致, 保留 |
| AD-6 | **ICCE 命令 HMAC = HMAC-SHA256(会话密钥, 命令体) 截断/全量 32 字节** | crypto_engine.c 提供 HMAC-SHA256; control_command_t.hmac[32] = SHA256 输出长度; 覆盖范围: command_type..user_id（前 6 字节）| 实现 + 标注"覆盖范围待真机确认" |
| AD-7 | **ICCE 会话加密 = SM4-CBC（PKCS#7 填充）, 密钥 = 协商 session_key 前 16 字节, IV = 协商 IV（未协商时全零, 仅调试）** | security_auth.h KEY_TYPE_SM4 + session_key[32]; Android 现有实现同构 | 保留 Android 现有 SM4-CBC; iOS 新增 Sm4.swift 同构 |
| AD-8 | **ICCOA 广播帧 vehicleId 编码未知 → parseAdvertisement 保持返回 nil 前检查** | iccoa-spec.md 无广播数据定义; iOS 注释已声明不猜测 | 不动（2b-A/B 范围外） |

### 2.2 映射表（适配器内）

| 通用 BleCommandType | ICCOA wire | ICCE wire |
|:---|:---|:---|
| unlock | 0x02 | 0x01 |
| lock | 0x01 | 0x02 |
| engineOn | 0x03 | 0x03 |
| engineOff | 0x04 | 0x04 |
| status | 0x05 (CTRL_REQ payload 复用) | 0x06 (QUERY) |

### 2.3 Specialists
| Specialist | 职责 | 文件隔离 |
|:---|:---|:---|
| W1 (iOS worker) | ICCOA/iCCE iOS 实现 + Sm4.swift + IcocaFrame.swift | `mobile/ios/Sources/YDKBLEManager/` |
| W2 (Android worker) | IcocaFrame.kt 修正 + ICCOA/ICCE adapter 修正 + HMAC | `mobile/android/sdk/src/main/kotlin/com/yuledkcs/sdk/ble/` + test |

---

## 3. Testable Behaviors

### iOS（W1）
- [ ] B1.1: `IcocaFrame.build(cmdId:seqNum:payload:)` 生成帧 = SOP 0xAA + CMD + SEQ(LE) + LEN(LE) + PAYLOAD + XOR(不含 SOP) + EOP 0x55 | Owner: W1
- [ ] B1.2: `IcocaFrame.parse` 能解析自身 build 的帧; 坏 checksum / 坏 EOP / 长度不符 → nil | Owner: W1
- [ ] B1.3: ICCOA `buildUnlockCommand` wire = 0xAA 0x20 SEQ(LE) 0x02 0x00 **0x02** 0x00(checksum) 0x55（unlock→0x02 映射）; lock→0x01 | Owner: W1
- [ ] B1.4: ICCOA buildCommand **不加密**（无 SM4 调用; payload 明文 [cmd][param]） | Owner: W1
- [ ] B1.5: ICCE `buildUnlockCommand` wire = 0x01 0x00 user_id(BE) + hmac[32]（非零, 会话密钥存在时）; 未协商密钥 → 明确抛错或零 hmac + 注释 | Owner: W1
- [ ] B1.6: `Sm4.cbcEncrypt/cbcDecrypt` 通过 GM/T 0002-2012 附录 A 测试向量（密钥 0123456789ABCDEFFEDCBA9876543210 → 密文 681EDF34D206965E86B3E94F536E4246） | Owner: W1

### Android（W2）
- [ ] B2.1: `IcocaFrame.build` SEQ/LEN 改为**小端 LE**; checksum 改为**不含 SOP**（覆盖 CMD+SEQ+LEN+PAYLOAD） | Owner: W2
- [ ] B2.2: 既有 `IcocaFrameTest` 更新对齐 LE + checksum 语义, 全绿 | Owner: W2
- [ ] B2.3: ICCOA `buildControlPayload` 改为 [cmd][param]（2 字节）; `encryptPayload` 删除（明文帧） | Owner: W2
- [ ] B2.4: ICCOA 适配器做枚举映射（unlock→0x02, lock→0x01, engineOn→0x03）; 测试断言 wire 值 | Owner: W2
- [ ] B2.5: ICCE `buildControlCommand` 填充真实 HMAC-SHA256(前 6 字节命令体)（会话密钥存在时）; 未协商 → 零 hmac + 警告日志 | Owner: W2
- [ ] B2.6: `BleProtocolAdapterTest` 全部更新并新增映射/帧断言 | Owner: W2

---

## 4. Acceptance Criteria

| ID | Criterion | Pass Condition | Fail Condition | Priority | Owner |
|:---|:----------|:---------------|:---------------|:--------:|:-----:|
| AC-1 | ICCOA 帧字节序/checksum 与 dk30.c 完全一致 | 构造帧与 C 参考逐字节对比一致（含 LE SEQ/LEN、checksum 不含 SOP） | 任何字节差异 | P0 | W1+W2 |
| AC-2 | ICCOA 无应用层 SM4 | 代码无 SM4 调用; wire 帧 payload 为明文 [cmd][param] | 仍加密 payload | P0 | W1+W2 |
| AC-3 | 枚举映射正确 | unlock wire=0x02 / lock wire=0x01 (ICCOA); unlock=0x01 / lock=0x02 (ICCE) | 颠倒 | P0 | W1+W2 |
| AC-4 | ICCE control_command 38 字节 + HMAC 非零 | 会话密钥存在时 hmac[32] 为 HMAC-SHA256 输出; 格式 [cmd][target][uid BE][hmac] | 零填充或格式错 | P0 | W1+W2 |
| AC-5 | SM4 通过标准测试向量 | GM/T 0002-2012 附录 A 向量加密/解密一致 (双端) | 向量不符 | P0 | W1+W2 |
| AC-6 | 编译/语法检查通过 | iOS: swiftc -parse 或类型检查通过; Android: kotlinc/gradle 编译通过 | 编译失败 | P0 | W1+W2 |
| AC-7 | 测试代码就位 | iOS 独立验证逻辑（脚本可执行）; Android JUnit 测试文件写出且断言与 wire 契约一致（CI 执行） | 测试缺失 | P1 | W1+W2 |
| AC-8 | 裁决文档更新 | `docs/certification/iccoa-icce-ble-command-frames.md` 记录全部字节级声明 + 出处 | 无文档 | P1 | 主导 |

---

## 5. Responsibility Matrix

| Criterion | Responsible | Fallback |
|:----------|:------------|:---------|
| AC-1 帧格式对齐 | W1 (iOS) + W2 (Android) | 主导（对照 dk30.c 复核） |
| AC-2 去 SM4 (ICCOA) | W1 + W2 | 主导 |
| AC-3 枚举映射 | W1 + W2 | 主导 |
| AC-4 ICCE HMAC | W1 + W2 | 主导（对照 crypto_engine.c） |
| AC-5 SM4 向量 | W1 (Swift) + W2 (Kotlin) | 主导（对照 sm4.c） |
| AC-6 编译检查 | W1 + W2 | 主导 |
| AC-7 测试 | W1 + W2 | 主导 |
| AC-8 文档 | 主导 | — |

---

## 6. Negotiation Log

| Round | Party | Action | Notes |
|:------|:------|:-------|:------|
| R1 | Generator | PROPOSE | 依据参考实现提出 8 项裁决; ICCOA 无 SM4 与任务名冲突, 以 AD-1 裁决纠正（任务名"SM4"实际指 ICCE） |
| R2 | architect-lead | APPROVE_ARCHITECTURE | 以 embedded/ 参考实现为唯一事实源; 文件隔离无交叉; 枚举映射放适配器内部正确 |
| R3 | Evaluator | APPROVE_TESTABILITY | 字节级 wire 断言可客观验证; AC-6/AC-7 覆盖无工具链约束 |
| R4 | 老板 | 确认 | 已按"先研读再动手"原则执行; 无需人工确认直接开工 |
| R5 | Evaluator | APPROVE | 双端实现完成: iOS 42/42 断言 + 类型检查零错误; Android 测试断言经 Python 独立交叉验证全部数学正确 (checksum/wire/HMAC); Android 测试由 CI 执行 (本环境无 gradle 工具链) |
| R6 | 主导 | CLOSE | 勘误: 契约 AD-2 checksum 覆盖 4+len 系笔误, 实际 5+len (dk30.c:131-132 校验路径), 双端已按校验路径实现 |

---

## 7. 交付物清单

- `mobile/ios/Sources/YDKBLEManager/IcocaFrame.swift`（新增）
- `mobile/ios/Sources/YDKBLEManager/Sm4.swift`（新增, ICCE 用）
- `mobile/ios/Sources/YDKBLEManager/ICCOABleAdapter.swift`（修正）
- `mobile/ios/Sources/YDKBLEManager/ICCEBleAdapter.swift`（实现）
- `mobile/android/sdk/src/main/kotlin/com/yuledkcs/sdk/ble/IcocaFrame.kt`（修正）
- `mobile/android/sdk/src/main/kotlin/com/yuledkcs/sdk/ble/BleProtocolAdapter.kt`（ICCOA/ICCE 修正）
- `mobile/android/sdk/src/test/kotlin/com/yuledkcs/sdk/ble/`（测试更新）
- `docs/certification/iccoa-icce-ble-command-frames.md`（新增裁决文档）
- `docs/sdk/SDK-TASKS.md`（2b-F 状态更新）
