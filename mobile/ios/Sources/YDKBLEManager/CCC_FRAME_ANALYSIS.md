# CCC BLE 指令帧分析 (2b-E)

> 来源: 仓库内参考实现研读, 非凭空设计
> - `embedded/ccc_protocol/include/ccc_digital_key.h`
> - `embedded/ccc_protocol/src/ble/ble_kw47a.c`
> - `docs/certification/relay-server-spec.md` (CCC-TS-101 知识库)
> - `docs/sdk/PHASE2B-BLEPROTOCOL-PLAN.md`
> - `docs/certification/pics-ccc.md`

---

## 1. 帧头 (ble_frame_header_t) — 参考实现, 字节级确定

`ccc_digital_key.h:119-124`:

```c
typedef struct __attribute__((packed)) {
    uint8_t  msg_type;      // 消息类型
    uint8_t  msg_id;        // 消息ID (递增)
    uint16_t payload_len;   // 负载长度 (大端)
    uint8_t  reserved;      // 预留
} ble_frame_header_t;
```

线格式 (共 **5 字节帧头** + payload):

| 偏移 | 长度 | 字段 | 编码 |
|:----:|:----:|:-----|:-----|
| 0    | 1    | msg_type   | 枚举值 (见 §2) |
| 1    | 1    | msg_id     | 递增消息 ID (防重放辅助; 本实现取 session.counter 低 8 位) |
| 2-3  | 2    | payload_len| **大端** (uint16) |
| 4    | 1    | reserved   | 置 0 |
| 5... | N    | payload    | 负载 |

## 2. 消息类型 (ble_msg_type_e) — 参考实现, 字节级确定

`ccc_digital_key.h:106-117`:

| 值 | 名称 | 用途 |
|:--:|:-----|:-----|
| 0x01 | BLE_MSG_PAIR_REQUEST  | 配对请求 |
| 0x02 | BLE_MSG_PAIR_RESPONSE | 配对响应 |
| 0x10 | BLE_MSG_KEY_CREATE    | 密钥创建 |
| 0x11 | BLE_MSG_KEY_DELETE    | 密钥删除 |
| 0x12 | BLE_MSG_KEY_SHARE     | 密钥分享 |
| 0x20 | BLE_MSG_AUTH_REQUEST  | **认证请求 — 车辆控制指令载体** (PICS §7: 解锁/锁车/启动均走 0x20→0x21) |
| 0x21 | BLE_MSG_AUTH_RESPONSE | 认证响应 (状态返回) |
| 0x30 | BLE_MSG_UWB_CONFIG    | UWB 参数配置 |
| 0x40 | BLE_MSG_STATE_NOTIFY  | 车辆状态主动上报 |
| 0xFF | BLE_MSG_ERROR         | 错误 |

## 3. GATT 服务映射 (0xFFD1) — 参考实现, 字节级确定

`ble_kw47a.c:430-529` (注释引用 CCC DK R3.0 §4.3.1):

| UUID | Characteristic | 属性 | max_len | 用途 |
|:----:|:---------------|:-----|:-------:|:-----|
| FFD2 | Bond Management (配对) | 读/写/Notify | 64  | OOB/数字配对 |
| FFD3 | Key Delivery (密钥下发) | 写/Indicate | 512 | 密钥数据 |
| FFD4 | **Auth Control (认证控制)** | 写/Notify | 64  | **控制指令通道** |
| FFD5 | **Vehicle Status (车辆状态)** | 读/Notify | 32  | 状态读取/上报 |
| FFD6 | UWB Params | 写/Indicate | 128 | UWB 参数 |
| FFD7 | RSSI | 读/Notify | 4   | RSSI 测距 |

→ iOS 侧: 控制指令写 FFD4, 状态读 FFD5; 响应通过 FFD4 的 Notify 返回。

## 4. 广告包 (Advertisement) — 参考实现 (LP 模式), 字节级确定

`ble_kw47a.c:138-145` (`ble_enter_lp_mode`):

```
0x1A,        // AD 长度 = 26
0xFF,        // AD 类型: Manufacturer Specific
0x4C, 0x00,  // Company ID = Apple 0x004C (BLE 小端)
0x02,        // iBeacon subtype
0x15,        // iBeacon payload 长度 = 21
// 20 字节 UUID — 由 keymgmt 模块按车辆填充 → vehicleId
```

→ iOS 侧解析 (YDKAdvertisementParser.cccVehicleID):
1. 校验 service UUID 含 0xFFD1;
2. 解析 mfr data: companyID == 0x004C, vendor[0]==0x02, vendor[1]==0x15;
3. vendor[2...22] 20 字节 → UUID 字符串 = vehicleId。

**TODO-verify**: 上述为车端低功耗 (iBeacon 风格) 广播; 生产 connectable 广播
结构需对照 CCC-TS-101 §4.3 (Advertising) 确认。

## 5. 加密/签名 — 规范原文已裁决 (2026-07-31)

**依据**: `docs/certification/ccc-ts101-ble-secure-channel.md` (CCC-TS-101 v4.0.0 APPROVED PDF §18.4.9/12/13, §19.3)

### 5.1 算法裁决 — 既不是 AES-CCM 也不是 AES-256-GCM

| 来源 | 算法 | 裁决 |
|:-----|:-----|:-----|
| PHASE2B-BLEPROTOCOL-PLAN.md 安全通道表 | ECDH + AES-CCM | ❌ 错误 |
| embedded security.c `sec_encrypt` 注释 | AES-256-GCM | ❌ 错误 |
| **CCC-TS-101 v4.0.0 §18.4.12/13** | **GPC_SPE_014 (SCP03): AES-128 + CMAC-AES-128 (RFC4493)** | ✅ **规范为准** |

### 5.2 密钥派生 (§18.4.9, Listing 18-9)

```
HKDF-SHA256 (RFC5869):
  IKM=SK (SPAKE2+ 共享密钥), Salt=NULL, Info="SystemKeys", L=64 (或 96)
  OKM: [0:128]=Kenc, [128:128]=Kmac, [256:128]=Krmac, [384:128]=LONG_TERM_SHARED_SECRET
```

### 5.3 命令加密 (§18.4.12, Listing 18-10, GPC_SPE_014 §6.2.6)

- S-ENC=Kenc (AES-128), Padded Counter Block = `0000...00h || 1-byte counter` (01h 起)
- MAC Chaining Value 16B (首命令全零), S-MAC=Kmac, C-MAC=8B (CMAC-AES-128 截断)

### 5.4 响应加密 (§18.4.13, Listing 18-11, GPC_SPE_014 §6.2.7)

- Padded Counter Block = `8000...00h || counter_value used in command`
- MAC Chaining Value 取命令的 16B, S-RMAC=Krmac, R-MAC=8B

### 5.5 帧头 — 参考实现 5 字节有误, 按规范改 4 字节

| 字段 | 位置 | 说明 |
|:-----|:-----|:-----|
| Message Header | Byte 0 | Bit[5:0]=Message Type, Bit[7:6]=RFU |
| Payload Header | Byte 1 | Message ID (DK_APDU_RQ=0x0B / RS=0x0C) |
| Length | Byte [3:2] | 2 字节大端 |
| Data | N | — |

**规范示例**: SELECT APDU → `0x010B0013 00A404000DA000000809434343444B41763100`
(Message Type=0x01 SE, ID=0x0B DK_APDU_RQ, Len=0x0013=19, Data=19B APDU)

### 5.6 实现计划 (2b-E)

1. `CCCSystemKeyDerivation`: HKDF-SHA256 Info="SystemKeys" → Kenc/Kmac/Krmac/LTSS
2. `CCCCommandEncryptor`: AES-128-CBC(ICV=counter block) + CMAC-AES-128 8B, MAC chaining
3. `CCCAuthResponseVerifier`: R-MAC 验证 (Krmac)
4. 帧头 4 字节改造 (Message Header + Payload Header + Length BE)
5. 控制指令: Message Type=0x01 (SE), DK_APDU_RQ=0x0B, class byte=0x84 (secure messaging)
6. Wire 级测试用规范 §19.3 示例

## 6. 控制指令载荷 (session 层) — SDK 自定义, 需 TODO-verify

参考实现只定义了帧头与消息类型, **未定义控制指令 payload 布局**。
`CCCControlPayload` 当前为 SDK 数据模型 (SessionContext) 的最小编码:

| 偏移 | 长度 | 字段 |
|:----:|:----:|:-----|
| 0    | 1    | subcommand (0x01 unlock / 0x02 lock / 0x03 engine on) |
| 1-2  | 2    | session handle (大端) |
| 3-6  | 4    | message counter (大端, 抗重放) |
| 7    | 1    | keyId 长度 |
| 8... | N    | keyId (UTF-8) |

**TODO-verify**: 需对照 CCC-TS-101 Reader Protocol Vehicle Access 命令 APDU 结构
确认/替换后, 该布局才能作为量产协议。

## 7. 响应解析

- `AUTH_RESPONSE (0x21)` / `PAIR_RESPONSE (0x02)`: payload[0] == 0x00 成功, 否则错误码;
- `ERROR (0xFF)`: payload[0] = 错误码;
- `STATE_NOTIFY (0x40)`: 状态载荷字段布局 **TODO-verify** (当前占位:
  [locked(1)][engineOn(1)][batteryPct(1)]);
- 非帧格式 (<5 字节裸字节): 回退 [0]=status 旧语义 (兼容联调)。

## 8. 实现文件

| 文件 | 内容 |
|:-----|:-----|
| `CCCCommandFrame.swift` | 帧头编解码、消息类型、控制载荷、安全提供者接口 |
| `CCCBleAdapter.swift` | 广告解析 + 指令构建 + 响应解析 |
| `YDKAdvertisementParser.swift` | 广告 AD 结构解析 (纯逻辑) |
