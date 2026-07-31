# CCC-TS-101 v4.0.0 — BLE Secure Channel 知识库

> 来源: `CCC-TS-101-Digital-Key-v4.0.0_APPROVED 3-19-25.pdf` (647 页, 官方 APPROVED 版)
> 提取: 2026-07-31, 原文摘录见 `/tmp/ccc_ts101_extract.md` (5445 行, §18.4 + §19.2 + §19.3)
> 本文档用途: 2b-E 加密实现的唯一依据, 所有字节级声明必须可追溯到下述页码

---

## 1. 系统密钥派生 (§18.4.9 Derivation of System Keys, PDF p.429, Listing 18-9)

```
input: SK, Flag_Kble_intro_Kble_oob_master_support
output: Kenc, Kmac, Krmac, LONG_TERM_SHARED_SECRET
        (if flag true: Kble_intro, Kble_oob_master)

HKDF-SHA256 per RFC5869 [13]:
  IKM  : SK                       (SPAKE2+ 共享密钥, 32 字节)
  L    : 64 或 96 (flag true 时 96, 否则 64)
  Salt : NULL
  Info : "SystemKeys"
  OKM  : [0:128]   = Kenc                    (16 字节)
         [128:128] = Kmac                    (16 字节)
         [256:128] = Krmac                   (16 字节)
         [384:128] = LONG_TERM_SHARED_SECRET (16 字节)
         若 flag true: [512:128]=Kble_intro, [640:128]=Kble_oob_master
  注: [start_index: number_of_bits]
```

## 2. Secure Channel 命令加密与认证 (§18.4.12, PDF p.429-430, Listing 18-10)

**算法: GPC_SPE_014 [7] §6.2.6 映射 = SCP03 风格 (AES-128 + CMAC-AES-128 per RFC4493)**

| 参数 | 值 |
|:-----|:---|
| Command Data Field - Plain Text | payload |
| Padded Counter Block | `000000000000000000000000000000h \|\| 1-byte counter` (首命令 01h, 最大 FFh) |
| S-ENC | Kenc (AES-128) |
| Ciphered Command Data Field | encrypted_payload |
| MAC Chaining Value | 全零(首命令) 或 上一条命令的 MAC Chaining Value (16 字节) |
| S-MAC | Kmac |
| C-MAC | mac (8 字节, CMAC-AES-128 截断) |

## 3. Secure Channel 响应加密与认证 (§18.4.13, PDF p.430, Listing 18-11)

**算法: GPC_SPE_014 [7] §6.2.7 映射**

| 参数 | 值 |
|:-----|:---|
| Response Data Field - Plain Text | payload |
| Padded Counter Block | `800000000000000000000000000000h \|\| 1-byte counter_value used in command` |
| S-ENC | Kenc (AES-128) |
| Ciphered Response Data Field | encrypted_payload |
| MAC Chaining Value | 16 字节, 取自命令的 MAC Chaining Value |
| S-RMAC | Krmac |
| R-MAC | mac (8 字节) |

## 4. DK Message Format (Table 19-19, PDF p.449) — 4 字节帧头

| 字段 | 位置 | 说明 |
|:-----|:-----|:-----|
| Message Header | Byte 0 | Bit[5:0]=Message Type, Bit[7:6]=RFU(置0) |
| Payload Header | Byte 1 | Bit[7:0]=Message ID |
| Length | Byte [3:2] | 2 字节, **大端**, Data 长度 |
| Data | N | 见 §19.3.1-19.3.9 各消息 |

**Message Type (Table 19-21)**: 0=Framework, 1=SE, 2=UWB Ranging Service, 3=DK Event Notification, 4=Vehicle OEM App, 5=Supplementary Service, 6=Head Unit Pairing, 7-63=Reserved

**示例 (PDF p.451)**: SELECT APDU (AID `A000000809434343444B417631`) over BLE:
```
Message Type = 0x01 (SE)
Message ID   = 0x0B (DK_APDU_RQ)
Data         = 0x00A404000DA000000809434343444B41763100
DK BLE Payload = 0x010B0013 00A404000DA000000809434343444B41763100
               (帧头4字节)  (19 字节 SELECT APDU)
```

## 5. APDU 封装 (§19.3.2, PDF p.459-460)

| 消息 | Message ID | 说明 |
|:-----|:----------:|:-----|
| DK_APDU_RQ | 0x0B | APDU 命令封装 (SE/Framework) |
| DK_APDU_RS | 0x0C | APDU 响应封装 |

- 允许的 class byte: `00h` (SELECT), `80h` (非安全消息), `84h` (安全消息/secure messaging)
- Framework 消息 (owner pairing): SELECT, SPAKE2+ REQUEST/VERIFIER, WRITE DATA, GET DATA, GET RESPONSE, OP CONTROL FLOW (Table 5-1)
- SE 消息 (standard transaction): SELECT, AUTH0, AUTH1, EXCHANGE, CONTROL FLOW, CREATE RANGING KEY (Table 15-1)

## 6. 其他 BLE 关键定义 (PDF p.437-449)

- **Owner Pairing Advertising (§19.2.1.3, p.437-438)**: Legacy LE 1M PHY, ADV_IND connectable+scannable undirected
  - AD1: Length=0x03, AD Type=0x03 (16-bit Service UUID), Data=`0xFFF5` (CCC_DK_UUID)
  - AD2: Length=0x14, AD Type=0x21 (Service Data - 128bit UUID), Data=16B `0x5810bbc0-b499-11e9-a2a3-2a2ae2dbcce4` + 1B IntentConfiguration (0x1 default) + 2B Vehicle Brand Identifier
- **Pairing Request (§19.2.1.4)**: IO Capability 0x00-0xFF, OOB data flag=1, AuthReq=1 (Bonding)
- **Pairing Response (§19.2.1.5)**: OOB flag=0x1, AuthReq=0x1, Max Enc Key Size=0x10, Initiator/Responder Key Dist=0xF0
- **DK Service (§19.2.1.6)**: primary service, 仅一个实例, UUID=0xFFF5
- **SPSM (§19.2.1.7)**: UUID_SPSM=`D3B5A130-9E23-4B3A-8BE4-6B1EE5F980A3`, uint16 大端, Read only, no auth; 动态 SPSM 范围 0x0080-0x00FF
- **DK Version (§19.2.1.8)**: UUID_SPSM_DK_VERSION=`AE285B91-6D23-23F1-CA12-6B1EE5B780A3` (Read, encrypted for passive entry); UUID_DEVICE_DK_VERSION=`BD4B9502-3F54-11EC-B919-0242AC120005` (Write, encrypted)
- **蓝牙加密 (§19.2.2, p.445)**: device (central) 须在 L2CAP 建立前完成 LE 加密; 未加密 L2CAP 5 秒后 vehicle 断开 (First_Approach_RQ 或 pairing 除外)
- **DK 消息经 L2CAP**: 用 DK Service SPSM 建 LE credit-based connection (§19.3)

## 7. 与参考实现的差异裁决 (防幻觉结论)

| 项 | 参考实现 (ble_kw47a.c / PLAN) | 规范 v4.0.0 | 裁决 |
|:---|:---|:---|:---|
| 帧头 | 5 字节 (msg_type+msg_id+payload_len+reserved) | **4 字节** (MsgHeader+PayloadHeader+Length) | ❌ 参考实现有误, 按规范改 |
| 加密 | PLAN: AES-CCM; security.c: AES-256-GCM | **GPC_SPE_014 (SCP03): AES-128 + CMAC-AES-128** | ❌ 两者都错, 按规范改 |
| 密钥派生 | 未定义 | HKDF-SHA256, Info="SystemKeys" | ✅ 按规范实现 |
| 消息类型 | 0x01 pair req / 0x20 auth req 等自定义 | 0=Framework, 1=SE, 2=UWB... | ❌ 按规范重定义 |
| DK_APDU_RQ | 无 | 0x0B / 0x0C | ✅ 按规范 |
| 广告 | iBeacon (0x004C) | CCC_DK_UUID=0xFFF5 + 0x21 Service Data | ⚠️ LP 模式 vs 规范; 生产用规范 |
| GATT | FFD1 服务 + FFD2-FFD7 特征 | 0xFFF5 + SPSM/DK Version 特征 | ⚠️ 参考实现为 R3.0 遗留, 生产按 v4.0.0 |

## 8. 实现要求 (2b-E 完成标准)

1. `CCCSystemKeyDerivation`: HKDF-SHA256, Info="SystemKeys", 输出 Kenc/Kmac/Krmac/LTSS
2. `CCCCommandEncryptor`: AES-128-CBC(ICV=counter block) + CMAC-AES-128 8B, MAC chaining
3. `CCCAuthResponseVerifier`: R-MAC 验证 (Krmac)
4. 帧头改 4 字节 (Message Header + Payload Header + Length BE)
5. 控制指令走 SE 消息: Message Type=0x01, DK_APDU_RQ=0x0B, class byte=0x84 (secure)
6. 测试向量: 用规范 §19.3 示例 SELECT APDU 做 wire 级测试
