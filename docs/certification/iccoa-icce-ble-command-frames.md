# ICCOA/ICCE BLE 指令帧 — 真实化裁决知识库（2b-F）

> 来源（唯一依据, 2026-08-01 研读）:
>   - `embedded/iccoa_protocol/src/iccoa/dk30/iccoa_dk30.c`（车端 DK3.0, 240 行）
>   - `embedded/iccoa_protocol/src/iccoa/dk40/iccoa_dk40.c`（车端 DK4.0）
>   - `embedded/iccoa_protocol/include/iccoa_digital_key.h`（命令/控制枚举）
>   - `embedded/iccoa_protocol/docs/SPEC.md`、`docs/module_design.md`（帧格式）
>   - `embedded/icce_protocol/docs/module_design.md` §3.1.4（control_command_t）
>   - `embedded/icce_protocol/src/crypto/sm4.c`、`crypto_engine.c`（SM4/HMAC 参考）
>   - `docs/certification/iccoa-spec.md`（ICCOA/T 002-2024 知识库摘要）
> 用途: 2b-F 移动端实现的唯一依据, 所有字节级声明可追溯到上述文件。

---

## 1. ICCOA DK 3.0 帧格式（车端 iccoa_dk30.c 为准）

```
[SOP(1)=0xAA][CMD_ID(1)][SEQ(LE u16)][LEN(LE u16)][PAYLOAD][XOR checksum(1)][EOP(1)=0x55]
```

| 字段 | 字节 | 规则 | 出处 |
|:-----|:-----|:-----|:-----|
| SOP | 0 | `0xAA` | iccoa_digital_key.h:53 (`DK30_SOP`) |
| CMD_ID | 1 | 见 §2 | iccoa_digital_key.h:56-69 |
| SEQ | 2-3 | **小端 LE** `raw[2] \| raw[3]<<8` | dk30.c:120 |
| LEN | 4-5 | **小端 LE** `raw[4] \| raw[5]<<8` | dk30.c:121 |
| PAYLOAD | 6..6+LEN | 明文（应用层无加密） | dk30.c:88-104 |
| CHECKSUM | 6+LEN | **XOR, 不含 SOP** — 覆盖 CMD+SEQ+LEN+PAYLOAD（从 raw+1 起 5+len 字节） | dk30.c:131-132 |
| EOP | 7+LEN | `0x55` | iccoa_digital_key.h:54 (`DK30_EOP`) |

> **勘误记录**: 契约初稿引用 dk30.c:236 `send_response` 的 `4+len`（该处与校验路径自相矛盾）。
> 手机→车端帧必须按**校验路径** `cs_len = 1+2+2+payload_len`（dk30.c:131-132）计算，
> 否则车端 `iccoa_dk30_process` 以 `ICCOA_ERR_SECURITY` 拒收。双端实现均按 5+len。

### 最小帧: header(6) + checksum(1) + EOP(1) = 8 字节（payload 可为空）
- 防重放: 车端要求 seq_num 单调递增（dk30.c:141-151, 0xFFFF→0 回绕合法）

## 2. ICCOA DK 3.0 命令 ID（iccoa_digital_key.h:56-69）

| 命令 | 值 | 方向 |
|:-----|:--:|:-----|
| BIND_REQ | 0x01 | 手机→车 |
| BIND_RSP | 0x02 | 车→手机 |
| UNBIND_REQ | 0x03 | 手机→车 |
| UNBIND_RSP | 0x04 | 车→手机 |
| AUTH_REQ | 0x10 | 手机→车 |
| AUTH_RSP | 0x11 | 车→手机 |
| **CTRL_REQ** | **0x20** | 手机→车（解锁/闭锁/启动） |
| CTRL_RSP | 0x21 | 车→手机 |
| STATUS_NOTIFY | 0x30 | 车→手机 |
| KEY_SHARE | 0x40 | — |
| KEY_SHARE_ACK | 0x41 | — |
| ERROR | 0xFF | 车→手机 |

## 3. ICCOA CTRL_REQ 负载与枚举（dk30.c:88-104 + iccoa_digital_key.h:155-167）

```
CTRL_REQ payload = [cmd(1)][param(1)]   // 共 2 字节; dk30.c:90 要求 payload_len >= 2
```

车端处理: `cmd = payload[0]; param = payload[1]; 校验 cmd ∈ [CTRL_LOCK, CTRL_HORN]`
响应: CTRL_RSP payload = [result(1)][0x00]（0x00=成功 0x01=失败）— dk30.c:102-103

| 枚举 | 值 |
|:-----|:--:|
| CTRL_LOCK | 0x01 |
| CTRL_UNLOCK | 0x02 |
| CTRL_ENGINE_ON | 0x03 |
| CTRL_ENGINE_OFF | 0x04 |
| CTRL_TRUNK_OPEN | 0x05 |
| CTRL_WINDOW_UP | 0x06 |
| CTRL_WINDOW_DOWN | 0x07 |
| CTRL_CLIMATE_ON | 0x08 |
| CTRL_CLIMATE_OFF | 0x09 |
| CTRL_FIND | 0x0A |
| CTRL_HORN | 0x0B |

### ⚠️ 通用枚举映射（关键, 防锁车/解锁颠倒）
SDK 通用 `BleCommandType` = UNLOCK 0x01 / LOCK 0x02（CCC 风格）,
**与 ICCOA 车端枚举相反**。适配器内部必须映射:

| 通用 | ICCOA wire |
|:-----|:----------:|
| UNLOCK | **0x02** |
| LOCK | **0x01** |
| ENGINE_ON | 0x03 |
| ENGINE_OFF | 0x04 |
| STATUS | 0x05 |

## 4. ICCOA 安全模型（裁决 AD-1/AD-2）

- **应用层无加密**。车端 `handle_ctrl_request` 直接解析明文 payload（dk30.c:88-104）。
- 加密由 **BLE 链路层 LE Secure Connections** 负责: 配对/绑定完成 →
  `hal_ble_request_encryption` → `BLE_STATE_READY`（iccoa_ble.c:224-231, 236-244）。
- 结论: **手机端 ICCOA 指令禁止做应用层 SM4**（原任务名"SM4"对 ICCOA 系误标）。
  手机端 BLE 连接层需确保链路加密（iOS CBPeripheral 连接加密属性 / Android GATT 加密）。

## 5. ICCE control_command_t（module_design.md §3.1.4:275-291）

```
[command_type(1)][target(1)][user_id(BE u32)][hmac(32)] = 38 字节
```

| 字段 | 字节 | 规则 |
|:-----|:-----|:-----|
| command_type | 0 | 见下表 |
| target | 1 | 目标设备（0x00 = 车辆主体） |
| user_id | 2-5 | 大端 BE u32 |
| hmac | 6-37 | HMAC-SHA256(会话密钥, 命令体前 6 字节) — **覆盖范围待真机确认** |

| 枚举 | 值 |
|:-----|:--:|
| CMD_UNLOCK_DOOR | 0x01 |
| CMD_LOCK_DOOR | 0x02 |
| CMD_START_ENGINE | 0x03 |
| CMD_STOP_ENGINE | 0x04 |
| CMD_OPEN_TRUNK | 0x05 |
| CMD_QUERY_STATUS | 0x06 |

通用映射: UNLOCK→0x01, LOCK→0x02, ENGINE_ON→0x03, ENGINE_OFF→0x04, STATUS→0x06（与 ICCOA 方向相反, 注意区分）。

## 6. ICCE 安全模型（裁决 AD-6/AD-7）

- 配对: OOB（NFC/QR 传公钥）+ LE SC 加密（technical_specification.md §3.3.1）
- 认证: 挑战-响应 → ECDH 派生 session_key[32]（security_auth.c:177-179）
- 命令完整性: hmac[32] = HMAC-SHA256（crypto_engine.c 提供, RFC 2104）
- 会话加密: **SM4-CBC + PKCS#7**（KEY_TYPE_SM4, security_auth.h:54）;
  密钥 = session_key 前 16 字节, IV = 协商 IV（未协商全零仅调试）

## 7. SM4 参考（GM/T 0002-2012 / GB/T 32907-2016）

- 参考实现: `embedded/icce_protocol/src/crypto/sm4.c`（493 行, NXP KW47A+SE050 平台）
- 移动端移植: Android `Sm4.kt`（已验证）/ iOS `Sm4.swift`（新增, 同构）
- 标准测试向量（附录 A）:
  - 密钥: `0123456789ABCDEFFEDCBA9876543210`
  - 明文: `0123456789ABCDEFFEDCBA9876543210`
  - 密文: `681EDF34D206965E86B3E94F536E4246`

---

## 8. 双端落地状态（2026-08-01）

| 项 | iOS | Android |
|:---|:---:|:---:|
| IcocaFrame 帧编解码（LE + checksum 不含 SOP） | ✅ `IcocaFrame.swift` | ✅ `IcocaFrame.kt` 修正 |
| ICCOA payload [cmd][param] | ✅ | ✅ |
| ICCOA 去应用层 SM4 | ✅ | ✅ |
| 枚举映射（unlock→0x02 等） | ✅ | ✅ |
| ICCE control_command 38B | ✅ | ✅ |
| ICCE HMAC-SHA256 | ✅ CryptoKit | ✅ javax.crypto |
| SM4-CBC 会话加密 | ✅ `Sm4.swift` | ✅ `Sm4.kt` 预存 |
| 独立验证 | ✅ 42/42 断言 | ✅ 测试更新, CI 执行 |

## 9. 待确认项（真机/规范原文）

1. ICCE hmac 覆盖范围（当前: 命令体前 6 字节, 待真机对照）
2. ICCE SM4 IV 协商机制（当前: 未协商全零, 仅调试）
3. ICCE GATT 特征 UUID（Android 注释 0xFEFE vs 参考 0x2A04, 属连接层 2b-C/D 范围）
4. ICCOA 广播 vehicleId 编码（无参考定义, 保持不猜测）
