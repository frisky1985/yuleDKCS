# ICCOA OEM 对接文档 — yuleDKCS × 智慧车联开放联盟

> **文档版本**: 1.0.0  
> **创建日期**: 2026-07-16  
> **协议标准**: ICCOA Digital Key 3.0 / 4.0  
> **系统版本**: yuleDKCS v1.0.0  

---

## 1. ICCOA 协议概述

### 1.1 标准信息

| 项目 | 内容 |
|:-----|:------|
| 标准全称 | ICCOA 数字钥匙技术规范 |
| 标准组织 | 智慧车联开放联盟（Intelligent Car Connectivity Open Alliance） |
| 协议版本 | DK 3.0（基础版） / DK 4.0（UWB 增强版） |
| 加密体系 | ECDSA P-256 / ECDH P-256 / AES-CMAC (DK 3.0) / HMAC-SHA256 (DK 4.0) |
| BLE Service UUID | `0000FEF5-0000-1000-8000-00805F9B34FB` |
| 近场通信 | BLE（必选）+ UWB FiRa（DK 4.0 可选） |
| 手机厂商 | 小米（小米钱包）、OPPO（OPPO 钱包）、vivo（vivo 钱包） |

### 1.2 ICCOA 背景

ICCOA（智慧车联开放联盟）由中国汽车工业协会牵头，小米、OPPO、vivo 等手机厂商联合发起，旨在建立跨品牌的手机-汽车互联互通标准。与 ICCE（华为主导）和 CCC（全球标准）不同，ICCOA 立足中国手机生态，强调多厂商互认和用户体验一致性。

yuleDKCS 提供完整的 ICCOA 协议栈支持，覆盖小米钱包、OPPO 钱包、vivo 钱包三大主流手机厂商的数字钥匙服务。

### 1.3 DK 3.0 与 DK 4.0 差异

| 特性 | DK 3.0 | DK 4.0 |
|:-----|:------:|:------:|
| 协议版本 | 基础版 | UWB 增强版 |
| 帧结构 | SOP (0xAA) + Head + Payload + Checksum (XOR) + EOP (0x55) | Magic (0x1CC0) + Header + Session Token + Payload + HMAC |
| 消息完整性 | XOR 校验 | HMAC-SHA256（截断 16 字节） |
| 安全认证 | Challenge-Response + ECDSA 签名 | Session Token + HMAC + ECDSA 签名 |
| 会话管理 | 无状态 | 有状态 Session Token（4 字节） |
| 多设备并发 | 有限 | 最多 4 个并发会话 |
| UWB 测距 | 不支持 | 支持 FiRa 兼容 UWB 安全测距 |
| 测距区域 | 不支持 | 支持 5 级区域（INSIDE/UNLOCK/APPROACH/AWAY/FAR） |
| 密钥派生 | 简单密钥派生 | 主密钥 + 会话密钥 + HMAC 密钥三级派生 |
| 抗降级保护 | N/A | 支持 DK4.0→DK3.0 降级攻击检测 |
| 推荐场景 | BLE 基础钥匙 | 无钥匙进入 + 手机即钥匙 + UWB 精准定位 |

**yuleDKCS 默认版本**: DK 4.0，自动协商降级至 DK 3.0（支持 `no_downgrade` 安全策略标记）

### 1.4 ICCOA 协议栈架构

```
┌─────────────────────────────────────────────────┐
│              ICCOA Application Layer            │
│  钥匙管理 · 车辆控制 · 钥匙分享 · 安全认证      │
├─────────────────────────────────────────────────┤
│              ICCOA Security Layer               │
│  ECDSA P-256 · ECDH P-256 · AES-CMAC (DK3.0)   │
│  HMAC-SHA256 (DK4.0) · SE050 密钥保管           │
├─────────────────────────────────────────────────┤
│              ICCOA Transport Layer              │
│  BLE GATT 0xFEF5 · UWB FiRa (DK 4.0)           │
├─────────────────────────────────────────────────┤
│         ICCOA Session Management (DK4.0)        │
│   Session Token · 多设备并发 · 生命周期管理      │
└─────────────────────────────────────────────────┘
```

yuleDKCS 中 ICCOA 协议栈：
- **车端**: `embedded/iccoa_protocol/` — 完整的 DK 3.0 + DK 4.0 双协议栈实现
  - DK 3.0：SOP/EOP 帧结构、XOR 校验、基础配对/认证/控制
  - DK 4.0：Session Token、HMAC-SHA256、UWB 测距、多会话管理
  - 权限与认证模块（8 类权限位、4 种授权类型）
  - SE050 安全芯片集成（密钥存储、签名验证、HMAC 计算）
- **移动端**: Android SDK + iOS SDK 中的 ICCOA BLE 自适应层
- **云端**: Java Adapter（`adapter-iccoa`）+ Go Adapter（`hub/iccoa_adapter.go`）

---

## 2. 对接流程

### 2.1 密钥开通流程（DK 3.0）

#### 帧结构定义

```
| SOP(1) | CMD_ID(1) | SEQ_NUM(2) | PAYLOAD_LEN(2) | PAYLOAD(n) | CHECKSUM(1) | EOP(1) |
|  0xAA  |  0x01~0x40 |  uint16    |    uint16      |   n bytes   |    XOR      |  0x55  |
```

- **SOP**: `0xAA` — 帧起始标记
- **CMD_ID**: 命令标识（见命令枚举表）
- **SEQ_NUM**: 序列号（防重放，单调递增，允许 0xFFFF→0 回绕）
- **PAYLOAD_LEN**: 载荷长度
- **PAYLOAD**: 命令载荷数据
- **CHECKSUM**: XOR 校验和，覆盖范围 `CMD_ID + SEQ_NUM + PAYLOAD_LEN + PAYLOAD`
- **EOP**: `0x55` — 帧结束标记

#### DK 3.0 命令枚举

| 命令 | 值 | 方向 | 说明 |
|:-----|:--:|:-----|:-----|
| `ICCOA_CMD_BIND_REQ` | 0x01 | 手机→车 | 绑定请求 |
| `ICCOA_CMD_BIND_RSP` | 0x02 | 车→手机 | 绑定响应 |
| `ICCOA_CMD_UNBIND_REQ` | 0x03 | 手机→车 | 解绑请求 |
| `ICCOA_CMD_UNBIND_RSP` | 0x04 | 车→手机 | 解绑响应 |
| `ICCOA_CMD_AUTH_REQ` | 0x10 | 手机→车 | 认证请求 |
| `ICCOA_CMD_AUTH_RSP` | 0x11 | 车→手机 | 认证响应 |
| `ICCOA_CMD_CTRL_REQ` | 0x20 | 手机→车 | 控制请求 |
| `ICCOA_CMD_CTRL_RSP` | 0x21 | 车→手机 | 控制响应 |
| `ICCOA_CMD_STATUS_NTF` | 0x30 | 车→手机 | 状态通知 |
| `ICCOA_CMD_KEY_SHARE` | 0x40 | 手机→车 | 钥匙分享 |
| `ICCOA_CMD_KEY_SHARE_ACK` | 0x41 | 车→手机 | 分享确认 |

#### DK 3.0 开通流程

```
手机端                                     车端 (BLE Slave)
  │                                           │
  │ ① BLE Advertisement Scan                  │
  │ ←── Service UUID 0xFEF5 ──────────────    │
  │                                           │
  │ ② BLE GATT Connect                        │
  │ ──────────── GATT Connect ───────────────→ │
  │                                           │
  │ ③ 绑定请求 (CMD_ID=0x01)                  │
  │ ── SOP|BIND_REQ|SEQ|LEN|PUB_KEY(64)|CS|EOP │
  │                                           │
  │ ④ 车端生成密钥对 (SE050), 创建会话        │
  │                                           │
  │ ⑤ 绑定响应 (CMD_ID=0x02)                  │
  │ ←─ SOP|BIND_RSP|SEQ|LEN|VEH_PUB_KEY(64)|CS│
  │                                           │
  │ ⑥ 认证请求 (CMD_ID=0x10)                  │
  │ ── SOP|AUTH_REQ|SEQ|LEN|SIGNATURE|CS|EOP → │
  │                                           │
  │ ⑦ 车端验证签名                             │
  │                                           │
  │ ⑧ 认证响应 (CMD_ID=0x11)                  │
  │ ←── SOP|AUTH_RSP|SEQ|LEN|RESULT(1)|CS|EOP  │
  │                                           │
```

### 2.2 密钥开通流程（DK 4.0）

#### 帧结构定义

```
| MAGIC(2) | VER(1) | MSG_TYPE(1) | MSG_ID(2) | FLAGS(2) | PAYLOAD_LEN(2) |
| SESSION_TOKEN(4) | PAYLOAD(n) | HMAC(16) |
```

- **MAGIC**: `0x1CC0`（[P0-5] 已修正原 `0xICC0` 的非法十六进制问题）
- **VER**: `0x01` — 协议版本
- **MSG_TYPE**: 消息类型（见消息类型枚举表）
- **MSG_ID**: 消息 ID（单调递增防重放）
- **FLAGS**: 标志位
  - Bit 0: `ENCRYPT` — 负载加密
  - Bit 1: `UWB` — UWB 测距启用
  - Bit 2: `RESPONSE` — 响应帧标记
  - Bit 3: `ERROR` — 错误帧标记
- **PAYLOAD_LEN**: 载荷长度
- **SESSION_TOKEN**: 4 字节会话令牌（服务器分配）
- **PAYLOAD**: 载荷数据（最大 244 字节）
- **HMAC**: 16 字节消息认证码（HMAC-SHA256 截断）

#### DK 4.0 消息类型

| 消息 | 值 | 说明 |
|:-----|:--:|:------|
| `ICCOA_V4_SESSION_OPEN` | 0x01 | 打开会话 |
| `ICCOA_V4_SESSION_CLOSE` | 0x02 | 关闭会话 |
| `ICCOA_V4_BIND` | 0x10 | 钥匙绑定 |
| `ICCOA_V4_AUTH` | 0x20 | 认证 |
| `ICCOA_V4_CTRL` | 0x30 | 车辆控制 |
| `ICCOA_V4_UWB_CONFIG` | 0x40 | UWB 配置 |
| `ICCOA_V4_SHARE` | 0x50 | 钥匙分享 |
| `ICCOA_V4_NOTIFY` | 0x60 | 状态通知 |
| `ICCOA_V4_ERROR` | 0xFF | 错误 |

#### DK 4.0 开通流程

```
手机端                                         车端 (BLE Slave)
  │                                               │
  │ ① BLE Scan + GATT Connect                     │
  │ ────────────────── Connect ─────────────────→ │
  │                                               │
  │ ② Session Open (MSG_TYPE=0x01)               │
  │ ── MAGIC|VER|0x01|ID|FLAGS|LEN|[PEER_ID(16)  │
  │     +DEV_TYPE(1)+CAP(4)]|SESSION(4)|HMAC  →  │
  │                                               │
  │ ③ 车端分配会话，生成 4-byte Session Token     │
  │                                               │
  │ ④ Session Open Response                       │
  │ ←─ MAGIC|VER|0x81|ID|RSP_FLAG|LEN|[TOKEN(4)  │
  │     +NONCE(12)]|SESSION(4)|HMAC              │
  │                                               │
  │ ⑤ 绑定请求 (MSG_TYPE=0x10)                   │
  │ ── MAGIC|VER|0x10|ID|FLAGS|LEN|[PUB_KEY(64)  │
  │     +SIG(72)+PERM(1)]|TOKEN(4)|HMAC         → │
  │                                               │
  │ ⑥ 车端验证签名，存储公钥和权限               │
  │                                               │
  │ ⑦ 绑定响应                                   │
  │ ←─ MAGIC|VER|0x90|ID|RSP|LEN|[RESULT(1)      │
  │     +VEH_PUB_KEY(64)]|TOKEN(4)|HMAC          │
  │                                               │
  │ ⑧ 认证请求 (MSG_TYPE=0x20)                   │
  │ ── MAGIC|VER|0x20|ID|FLAGS|LEN|[CHALLENGE(32)│
  │     +SIGNATURE(72)]|TOKEN(4)|HMAC           → │
  │                                               │
  │ ⑨ 车端验证挑战响应，状态→AUTHENTICATED       │
  │                                               │
  │ ⑩ 认证响应                                   │
  │ ←─ MAGIC|VER|0xA0|ID|RSP|LEN|[0x00]|TOKEN    │
  │                                               │
  │ [DK 4.0 可选] UWB 配置 (MSG_TYPE=0x40)        │
  │ ── UWB_CONFIG|ID|FLAGS|LEN|[UWB_PARAMS(12)]  │
  │     |TOKEN|HMAC                              → │
  │                                               │
  │ [DK 4.0 可选] 车端启动 UWB 测距               │
  │ ←─ UWB_CONFIG_RSP|...|TOKEN|HMAC             │
  │                                               │
  │ ⑪ 车辆控制 (MSG_TYPE=0x30)                   │
  │ ── CTRL|ID|FLAGS|LEN|[CMD(1)+PARAM(1)]|TOKEN │
  │     |HMAC                                    → │
  │                                               │
```

#### HMAC 算法说明

DK 4.0 的 HMAC 计算覆盖帧头到 payload 结束部分（magic 到 payload 结束，共 `12 + payload_len` 字节）：

```c
// HMAC-SHA256 截断到 16 字节
// 当前实现: se050_hmac_sha256(hmac_key, 32, data, data_len, mac_out);
// 截断: 取前 16 字节
void dk40_compute_hmac(const uint8_t *data, uint16_t len, uint8_t *hmac_out) {
    se050_hmac_sha256(g_key_ctx.hmac_key, 32, data, len, hmac_out);
    // HMAC 输出截断到 16 字节
}
```

> ⚠️ **注意**: 当前实现使用 HMAC-SHA256 截断 16 字节。需与 ICCOA DS v4.0 规范确认实际要求的 HMAC 算法（部分 ICCOA 规范文档使用 AES-CMAC 而非 HMAC-SHA256）。如有出入，在 `iccoa_dk40.c` 中切换 HMAC 计算引擎即可。

### 2.3 车辆控制指令

yuleDKCS 支持以下车辆控制指令：

| 指令 | 值 | 说明 | 所需权限 | 远程支持 |
|:-----|:--:|:------|:---------|:--------:|
| `CTRL_LOCK` | 0x01 | 上锁 | `ICCOA_PERM_LOCK` | ✅ |
| `CTRL_UNLOCK` | 0x02 | 解锁 | `ICCOA_PERM_UNLOCK` | ✅ |
| `CTRL_ENGINE_ON` | 0x03 | 启动引擎 | `ICCOA_PERM_ENGINE` | ✅ |
| `CTRL_ENGINE_OFF` | 0x04 | 停止引擎 | `ICCOA_PERM_ENGINE` | ✅ |
| `CTRL_TRUNK_OPEN` | 0x05 | 打开后备箱 | `ICCOA_PERM_TRUNK` | ✅ |
| `CTRL_WINDOW_UP` | 0x06 | 升窗 | `ICCOA_PERM_WINDOW` | ✅ |
| `CTRL_WINDOW_DOWN` | 0x07 | 降窗 | `ICCOA_PERM_WINDOW` | ✅ |
| `CTRL_CLIMATE_ON` | 0x08 | 开空调 | `ICCOA_PERM_CLIMATE` | ✅ |
| `CTRL_CLIMATE_OFF` | 0x09 | 关空调 | `ICCOA_PERM_CLIMATE` | ✅ |
| `CTRL_FIND` | 0x0A | 寻车（双闪+鸣笛） | `ICCOA_PERM_FIND` | ✅ |
| `CTRL_HORN` | 0x0B | 鸣笛 | `ICCOA_PERM_FIND` | ✅ |

控制命令协议（DK 3.0 示例）:

```
发送: SOP | 0x20 | SEQ | 0x0002 | [CMD(1) + PARAM(1)] | CS | EOP
响应: SOP | 0x21 | SEQ | 0x0002 | [RESULT(1) + STATUS(1)] | CS | EOP
```

- `RESULT`: `0x00`=成功, `0x01`=失败, `0x02`=无权限

### 2.4 钥匙分享流程

ICCOA 支持同厂商和跨厂商钥匙分享。

```
发起方                                    接收方                 ICCOA 云端
  │                                         │                       │
  │ ① 发起分享请求                           │                       │
  │ ── POST /v1/keys/share (DK3.0 BLE)  ───→ │                       │
  │   或 Cloud API share request             │                       │
  │ ──────────────────────────────────────────|──── POST share ────→ │
  │                                         │                       │
  │ ② ICCOA 云生成分享码                     │                       │
  │                                         │ ←── share_code ────── │
  │                                         │                       │
  │ ③ 分享码传递给接收方                     │                       │
  │ (微信/短信/二维码)                       │                       │
  │ ────────── share_code ─────────────────→ │                       │
  │                                         │                       │
  │ ④ 接收方通过钱包 App 使用分享码          │                       │
  │                                         │ ── accept share ────→ │
  │                                         │                       │
  │ ⑤ 接收方手机与车 BLE 连接完成绑定        │                       │
  │                                         │ ── BLE bind ────────→ │
  │                                         │  (车端)               │
  │                                         │                       │
  │ ⑥ 接收方获得受限权限的数字钥匙           │                       │
```

**分享钥匙权限限制**：
- 可选有效期限（小时/天）
- 可选使用次数限制
- 可选权限位子集
- 可设置地理围栏（如需）

### 2.5 钥匙吊销流程

```
管理员/TSP                                   ICCOA 云端              车端
  │                                            │                      │
  │ ① 发起吊销                                 │                      │
  │ ── POST /v1/keys/revoke ─────────────────→ │                      │
  │   {keyId, reason, userId}                  │                      │
  │                                            │                      │
  │ ② 标记钥匙状态为 REVOKED                    │                      │
  │                                            │                      │
  │ ③ 推送吊销通知到绑定的手机厂商              │                      │
  │                                            │                      │
  │ ④ 厂商钱包 Push 通知                       │                      │
  │ ────────────────── Push (REVOKED) ───────→ │                      │
  │                                            │                      │
  │ ⑤ [可选] 云→车端同步吊销列表               │                      │
  │                                            │ ── revoke cmd ─────→ │
  │                                            │                      │
  │ ⑥ 车端从本地白名单移除钥匙                  │                      │
```

---

## 3. TSP API 接口要求

### 3.1 接口总览

yuleDKCS ICCOA Adapter 需要 OEM TSP 暴露以下 API 端点：

| 序号 | 接口 | 方法 | 路径 | 用途 | 优先级 |
|:----:|:-----|:----:|:-----|:-----|:------:|
| 1 | 钥匙签发 | POST | `/v1/keys/issue` | 申请签发新数字钥匙 | P0 |
| 2 | 钥匙绑定 | POST | `/v1/keys/bind` | 绑定钥匙到设备（含 ECDH） | P0 |
| 3 | 钥匙状态查询 | GET | `/v1/keys/{keyId}` | 查询钥匙当前状态 | P0 |
| 4 | 钥匙吊销 | POST | `/v1/keys/revoke` | 吊销指定的数字钥匙 | P0 |
| 5 | 钥匙解绑 | POST | `/v1/keys/unbind` | 解绑钥匙与设备 | P0 |
| 6 | 车辆信息查询 | GET | `/v1/vehicles` | 获取用户车辆列表 | P0 |
| 7 | 钥匙分享 | POST | `/v1/keys/share` | 发起钥匙分享 | P1 |
| 8 | 事件通知 | POST | (callback) | OEM TSP 推送事件到 Hub | P1 |
| 9 | 钥匙状态变更通知 | POST | (callback) | 厂商推送钥匙状态变更 | P1 |

> **认证方式**: ICCOA 使用 App ID + App Secret 鉴权，请求头携带 `X-App-Id` 和 `X-Region`。

### 3.2 接口一：钥匙签发

```
POST /v1/keys/issue
```

**请求体：**

```json
{
  "user_id": "string",
  "vehicle_id": "string",
  "vin": "string",
  "key_type": "enum(OWNER | SUB_KEY | TEMPORARY | SHARED)",
  "valid_from": 1710000000,
  "valid_until": 1799999999,
  "max_uses": 100,
  "permissions": 9,
  "signature": "base64(ecdsa_sign(user_id||vehicle_id||valid_until))"
}
```

**`permissions` 位掩码说明**：

| 位 | 值 | 权限 | 说明 |
|:--:|:--:|:-----|:------|
| Bit 0 | 0x01 | `PERM_LOCK` | 上锁 |
| Bit 1 | 0x02 | `PERM_UNLOCK` | 解锁 |
| Bit 2 | 0x04 | `PERM_ENGINE` | 引擎控制 |
| Bit 3 | 0x08 | `PERM_TRUNK` | 后备箱 |
| Bit 4 | 0x10 | `PERM_WINDOW` | 车窗 |
| Bit 5 | 0x20 | `PERM_CLIMATE` | 空调 |
| Bit 6 | 0x40 | `PERM_FIND` | 寻车 |
| Bit 7 | 0x80 | `PERM_SEAT` | 座椅 |

示例：`permissions = 0x0F` = LOCK | UNLOCK | ENGINE | TRUNK

**响应体：**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "key_id": "iccoa-key-uuid",
    "key_content": ["base64-key-material-packet-1", "base64-key-material-packet-2"],
    "key_slot": 1,
    "status": "PENDING"
  }
}
```

### 3.3 接口二：钥匙绑定

```
POST /v1/keys/bind
```

**请求体：**

```json
{
  "user_id": "string",
  "vehicle_id": "string",
  "vin": "string",
  "device_id": "string",
  "key_id": "string",
  "device_public_key": "base64(ecdsa_p256_device_ephemeral_pubkey)",
  "attestation_token": "base64(device_attestation)",
  "signature": "base64(ecdsa_sign(user_id||key_id||device_id))"
}
```

**响应体：**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "key_id": "iccoa-key-uuid",
    "shared_secret": "base64(ecdh_shared_secret)",
    "tsp_public_key": "base64(tsp_ephemeral_pubkey)",
    "session_id": "session-uuid",
    "key_slot": 1,
    "key_content": ["base64-key-material"]
  }
}
```

**⚠️ 关键要求：**
- `shared_secret` 由 TSP 使用 ECDH P-256 派生，**不允许为空**
- `tsp_public_key` 必须是有效的 ECDSA P-256 公钥
- `device_public_key` 为手机侧临时公钥

### 3.4 接口三：钥匙状态查询

```
GET /v1/keys/{keyId}
```

**响应体：**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "key_id": "iccoa-key-uuid",
    "status": "enum(ACTIVE | PENDING | REVOKED | EXPIRED | SUSPENDED)",
    "bound_device_id": "device-uuid",
    "created_at": 1710000000,
    "expires_at": 1799999999,
    "last_used_at": 1715000000,
    "metadata": {}
  }
}
```

### 3.5 接口四：钥匙吊销

```
POST /v1/keys/revoke
```

**请求体：**

```json
{
  "key_id": "iccoa-key-uuid",
  "reason": "enum(LOST | STOLEN | OWNER_REQUEST | ADMIN)",
  "user_id": "string"
}
```

### 3.6 接口五：钥匙解绑

```
POST /v1/keys/unbind
```

**请求体：**

```json
{
  "key_id": "iccoa-key-uuid",
  "device_id": "device-uuid",
  "reason": "string"
}
```

### 3.7 接口六：车辆信息查询

```
GET /v1/vehicles?user_id={userId}
```

**响应体：**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "vehicle_list": [
      {
        "vehicle_id": "string",
        "vin": "WBA3A5C5XDF123456",
        "brand": "OEM",
        "model": "Model X",
        "year": 2026,
        "iccoa_supported": true,
        "capabilities": ["BLE", "UWB"]
      }
    ]
  }
}
```

### 3.8 接口七：钥匙分享

```
POST /v1/keys/share
```

**请求体：**

```json
{
  "key_id": "string",
  "friend_user_id": "string",
  "permissions": 7,
  "valid_hours": 72,
  "max_uses": 50,
  "share_type": "enum(SHARE_CODE | DIRECT_TRANSFER)"
}
```

**响应体：**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "share_id": "share-uuid",
    "share_code": "123456",
    "expires_at": 1715100000
  }
}
```

### 3.9 事件通知（回调）

**OEM TSP → yuleDKCS Hub（由 OEM 配置回调 URL）**

```json
{
  "event_type": "enum(KEY_STATUS_CHANGE | VEHICLE_EVENT | KEY_USAGE)",
  "timestamp": 1715000000,
  "data": {
    "key_id": "string",
    "vehicle_id": "string",
    "new_status": "REVOKED",
    "reason": "string"
  }
}
```

---

## 4. 权限模型

### 4.1 8 类权限位

ICCOA 定义 8 类权限位，编码在 32 位权限字段中：

| 索引 | 常量 | 位掩码 | 说明 |
|:----:|:-----|:------:|:------|
| 0 | `ICCOA_PERM_LOCK` | `1 << 0` = 0x01 | 上锁 |
| 1 | `ICCOA_PERM_UNLOCK` | `1 << 1` = 0x02 | 解锁 |
| 2 | `ICCOA_PERM_ENGINE` | `1 << 2` = 0x04 | 引擎启动/停止 |
| 3 | `ICCOA_PERM_TRUNK` | `1 << 3` = 0x08 | 后备箱 |
| 4 | `ICCOA_PERM_WINDOW` | `1 << 4` = 0x10 | 车窗控制 |
| 5 | `ICCOA_PERM_CLIMATE` | `1 << 5` = 0x20 | 空调控制 |
| 6 | `ICCOA_PERM_FIND` | `1 << 6` = 0x40 | 寻车（闪灯/鸣笛） |
| 7 | `ICCOA_PERM_SEAT` | `1 << 7` = 0x80 | 座椅控制 |

车端权限检查实现：

```c
// iccoa_dk40.c — handle_ctrl 中的权限检查
if (cmd == CTRL_ENGINE_ON || cmd == CTRL_ENGINE_OFF) {
    if (!(g_sessions[idx].permissions & ICCOA_PERM_ENGINE)) {
        rsp_payload[0] = 0x02;  // 权限不足
        *rsp_len = 1;
        return ICCOA_OK;
    }
    // [P0-5] 引擎启动前执行完整权限安全检查
    int32_t perm_ret = iccoa_dk40_check_engine_start_permission(idx);
    if (perm_ret != ICCOA_OK) return perm_ret;
}
```

### 4.2 4 种授权类型

| 类型 | 枚举值 | 说明 | 典型场景 |
|:-----|:------:|:------|:---------|
| **绑定授权** | `ICCOA_AUTH_BIND = 0x01` | 初次绑定时的设备认证 | 钥匙开通、设备更换 |
| **日常授权** | `ICCOA_AUTH_DAILY = 0x02` | 日常使用的快速认证 | 无钥匙进入、一键启动 |
| **远程授权** | `ICCOA_AUTH_REMOTE = 0x03` | 远程指令的强认证 | 远程解锁、远程开空调 |
| **分享授权** | `ICCOA_AUTH_SHARE = 0x04` | 钥匙分享时的授权 | 分享钥匙给家人/朋友 |

### 4.3 授权验证流程

```
                  ┌──────────────┐
                  │ 发起请求     │
                  └──────┬───────┘
                         │
                  ┌──────▼───────┐
                  │ 授权类型判断  │
                  └──────┬───────┘
                         │
          ┌──────────────┼──────────────┐
          ▼              ▼              ▼
    ┌─────────┐   ┌──────────┐   ┌──────────┐
    │ 绑定授权 │   │ 日常授权  │   │ 远程授权  │
    │ ECDSA签  │   │ 快速验签  │   │ 强认证+  │
    │ 名+SE   │   │ SE缓存   │   │ 挑战值   │
    └─────────┘   └──────────┘   └──────────┘
          │              │              │
          └──────────────┼──────────────┘
                         ▼
                  ┌──────────────┐
                  │ 权限位校验    │
                  │ valid_from ~ │
                  │ valid_until  │
                  │ max_uses     │
                  └──────┬───────┘
                         │
                  ┌──────▼───────┐
                  │ 执行操作/拒绝  │
                  └──────────────┘
```

---

## 5. 对接 CheckList（22 项）

### 5.1 环境与凭据（4 项）

- [ ] **I-P1** OEM 提供 ICCOA TSP API 端点（HTTPS URL）及 Region 信息（cn/us）
- [ ] **I-P2** OEM 提供 App ID 与 App Secret 凭据
- [ ] **I-P3** 网络连通性验证：`curl -I https://iccoa-tsp.oem.com` 返回 200，TLS ≥ 1.2
- [ ] **I-P4** 确认 API 限流策略和 QPS 配额

### 5.2 BLE UUID 与帧结构对齐（4 项）

- [ ] **I-B1** BLE Service UUID 对齐为 `0000FEF5-0000-1000-8000-00805F9B34FB`，确认广播数据包中包含该 UUID
- [ ] **I-B2** DK 3.0 帧结构验证：SOP (0xAA) / EOP (0x55) / XOR Checksum 与规范一致
- [ ] **I-B3** DK 4.0 帧结构验证：Magic (0x1CC0) / Session Token (4 字节) / HMAC (16 字节) 与规范一致
- [ ] **I-B4** 帧序列号（SEQ_NUM / MSG_ID）单调递增验证，防重放机制确认

### 5.3 核心接口验证（7 项）

- [ ] **I-C1** `getVehicles` 返回正确的车辆列表（至少 1 台支持 ICCOA 的车辆）
- [ ] **I-C2** `requestKeys`（`POST /v1/keys/issue`）成功签发钥匙，`key_content` 非空
- [ ] **I-C3** `bindKey`（`POST /v1/keys/bind`）调用成功，`shared_secret` 非空且为 ECDH P-256 派生
- [ ] **I-C4** `bindKey` 返回的 `tsp_public_key` 为有效 ECDSA P-256 公钥
- [ ] **I-C5** `getKeyStatus` 对已绑定钥匙返回 `ACTIVE`
- [ ] **I-C6** `revokeKeys` 成功吊销钥匙，状态变更为 `REVOKED`；吊销后解锁指令被拒绝
- [ ] **I-C7** `unbindKey` 成功解绑钥匙

### 5.4 算法与安全（4 项）

- [ ] **I-S1** HMAC 算法确认：HMAC-SHA256 截断 16 字节 vs AES-CMAC 对齐 ICCOA DS v4.0 规范（当前实现使用 HMAC-SHA256，需确认 ICCOA 联盟最新版本要求）
- [ ] **I-S2** ECDSA P-256 签名生成与验证算法对齐（交换测试向量验证签名一致性）
- [ ] **I-S3** ECDH P-256 密钥协商验证：车端与 TSP 端派生同一 `shared_secret`
- [ ] **I-S4** DK 4.0 Session Token 生成随机性验证，确认使用 SE050 硬件 TRNG

### 5.5 DK 4.0 Session 管理（3 项）

- [ ] **I-D1** DK 4.0 Session Token 持久化验证：车端断电重启后，Token 状态是否恢复（当前实现为内存态，生产需持久化）
- [ ] **I-D2** 多设备并发处理验证：最多 4 个并发 Session，Session 限制后新连接响应 `ICCOA_ERR_NO_MEM`
- [ ] **I-D3** Session 超时管理：长时间无活动的 Session 自动过期释放（`SESSION_STATE_EXPIRED`）

### 5.6 离线模式与多场景（3 项）

- [ ] **I-O1** 离线模式验证：无网络时仍可通过 BLE 近场解锁/启动（依赖车端本地白名单）| □ 车端本地
- [ ] **I-O2** 降级安全保护：DK 4.0 模式收到 DK 3.0 帧时触发降级攻击告警（`no_downgrade` 标志默认启用）
- [ ] **I-O3** 多厂商钱包兼容性验证：同一台车分别接入小米钱包 / OPPO 钱包 / vivo 钱包

### 5.7 验证测试（3 项）

- [ ] **I-T1** 指数退避重试验证：TSP 返回 503 时，Adapter 自动重试 3 次（延迟 500ms / 1s / 2s）
- [ ] **I-T2** 超时测试：TSP 无响应 35s，Adapter 正确超时并返回错误
- [ ] **I-T3** 防重放测试：重复使用序列号/Nonce 的安全帧被车端拒绝

---

## 6. 与 ICCE / CCC 的差异

### 6.1 ICCOA vs ICCE

| 维度 | ICCOA | ICCE |
|:-----|:------|:------|
| 标准组织 | 智慧车联开放联盟 | 中国汽车工程学会 / 华为 ICCE 联盟 |
| 主导厂商 | 小米/OPPO/vivo | 华为 |
| BLE UUID | `0xFEF5` | `0xFEFA` 系列 |
| 加密体系 | ECDSA P-256 / ECDH P-256 | 国密 SM2/SM3/SM4（强制） |
| **国密要求** | **非强制国密**，可用国际算法 | **强制国密** SM2/SM3/SM4 |
| DK 版本 | DK 3.0 + DK 4.0 | 2.0（接口层） |
| 帧格式 | DK3.0: SOP/EOP/XOR；DK4.0: Magic/Token/HMAC | BER-TLV 编码 |
| 权限模型 | 8 类权限位 + 4 种授权类型 | 基础权限集 |
| 待避免的差异 | 国密非强制，算法选型更灵活 | ICCE 需要 GM/T 0028 安全模块认证 |
| yuleDKCS 实现 | 双协议栈，默认 DK 4.0 | 单协议栈，边缘引擎 |

**核心差异**: ICCOA 不强制使用国密算法，OEM 可以选用国际算法（ECDSA P-256 / ECDH P-256），降低了国密合规门槛。但部分中国车企可能仍要求国密，yuleDKCS 的 SE050 同时支持国际和国密算法。

### 6.2 ICCOA vs CCC

| 维度 | ICCOA | CCC |
|:-----|:------|:------|
| 标准组织 | 智慧车联开放联盟 | Car Connectivity Consortium |
| 市场范围 | 中国为主 | 全球 |
| BLE UUID | `0xFEF5` | `0xFFD1` |
| 证书体系 | 无强制证书链要求 | CCC Root CA → OEM 子 CA → 设备证书（X.509 v3） |
| 安全通道 | Session Token + HMAC | SCP03（GlobalPlatform v2.3.1） |
| 近场通信 | BLE（必选）+ UWB（可选） | NFC OOB（必须）+ BLE（必须）+ UWB（可选） |
| **权限模型** | **8 类权限位 + 4 种授权类型** | 基础权限集 |
| 钥匙分享 | 跨厂商分享（ICCOA 联盟内互认） | 需厂商自定义 |
| 会话管理 | DK 4.0 Session Token（有状态） | 无状态 |
| 抗降级保护 | 内置 no_downgrade 策略 | 无标准定义 |
| yuleDKCS 覆盖度 | 85% | 90% |

**核心差异**: ICCOA 的权限模型明显比 CCC 更丰富（8 类权限位 × 4 种授权类型），这得益于 ICCOA 联盟中手机厂商对用户体验的深度参与。此外，ICCOA 不需要 CCC 那样复杂的 X.509 证书链体系，部署门槛较低。

### 6.3 ICCOA DK 4.0 Session 管理独有特性

DK 4.0 的 Session 管理是 ICCOA 独有的核心机制，在 ICCE 和 CCC 中均不存在：

| 特性 | 说明 | yuleDKCS 实现状态 |
|:-----|:------|:-----------------:|
| Session Token | 4 字节随机数，每次会话唯一 | ✅ SE050 TRNG 生成 |
| 多设备并发 | 支持最多 4 个独立会话 | ✅ `DK40_MAX_SESSIONS = 4` |
| 状态生命线 | IDLE → BIND_PENDING → AUTHENTICATED → RANGING_ACTIVE → EXPIRED | ✅ 5 状态实现 |
| 超时管理 | 无活动自动过期释放 | ⚠️ 基础实现，生产需补充心跳 |
| UWB 测距集成 | UWB 测距会话与 Session 绑定 | ✅ 回调驱动 |
| **持久化** | **车端重启后 Session 恢复** | **⚠️ 当前为内存态，生产需持久化到 SE050** |

**持久化建议**：生产部署时应将 Session Token 和状态持久化到 SE050 的透明对象存储中，实现车端断电重启后的 Session 恢复，避免用户每次上车都需要重新绑定。

---

## 7. 厂商 API 差异（小米/OPPO/vivo）

yuleDKCS ICCOA Adapter 需要针对不同手机厂商的 API 差异做适配：

| 操作 | 小米 API | OPPO API | vivo API |
|:-----|:---------|:---------|:---------|
| 绑定 | `POST /api/v1/carkey/bind` | `POST /ocar/v2/key/bind` | `POST /vivotsp/v1/digitalkey/bind` |
| 解绑 | `DELETE /api/v1/carkey/{key_id}` | `DELETE /ocar/v2/key/{key_id}` | `DELETE /vivotsp/v1/digitalkey/{key_id}` |
| 分享 | `POST /api/v1/carkey/{key_id}/share` | `POST /ocar/v2/key/{key_id}/share` | 厂商自定义 |
| 推送 | 小米 Push | OPPO Push | vivo Push |

Adapter 设计参考：

```go
// backend/cloud/hub/internal/adapter/iccoa_adapter.go
// 厂商适配: 根据 vendor 参数路由到不同 API 端点
func (a *ICCOAAdapter) BindKey(ctx context.Context, req *pb.BindKeyRequest) (*pb.BindKeyResponse, error) {
    // 根据 a.vendor 选择 API 端点
    //   "xiaomi"  → POST /api/v1/carkey/bind
    //   "oppo"    → POST /ocar/v2/key/bind
    //   "vivo"    → POST /vivotsp/v1/digitalkey/bind
    ...
}
```

---

## 8. ICCOA Adapter 配置参考

### 8.1 application-iccoa.yml

```yaml
adapter:
  enabled: true
  iccoa:
    enabled: true
    api-url: ${ICCOA_API_URL:https://api.iccoa.example.com}
    app-id: ${ICCOA_APP_ID}
    app-secret: ${ICCOA_APP_SECRET}
    region: ${ICCOA_REGION:cn}  # cn or us

logging:
  level:
    com.digitalkey.adapter.iccoa: DEBUG
```

### 8.2 启动与健康检查

```bash
# 启动 ICCOA Adapter（含 gRPC Server）
java -jar adapter-grpc-server.jar \
  --spring.profiles.active=iccoa,prod

# 健康检查
curl http://localhost:8080/actuator/health

# 预期返回
# {"status":"UP","adapters":{"total":1,"enabled":1,"iccoa":"UP"}}
```

### 8.3 多厂商配置

```yaml
adapter:
  iccoa:
    vendors:
      xiaomi:
        api-url: https://api.xiaomi.com/iccoa
        app-id: ${XIAOMI_APP_ID}
        app-secret: ${XIAOMI_APP_SECRET}
        push-enabled: true
      oppo:
        api-url: https://api.oppo.com/ocar
        app-id: ${OPPO_APP_ID}
        app-secret: ${OPPO_APP_SECRET}
        push-enabled: true
      vivo:
        api-url: https://api.vivo.com/vivotsp
        app-id: ${VIVO_APP_ID}
        app-secret: ${VIVO_APP_SECRET}
        push-enabled: false
```

---

## 9. 常见问题

### Q1: ICCOA 国密是否必须？

A: **不必须。** ICCOA 协议规范不强制要求国密算法，OEM 可以使用国际算法（ECDSA P-256 / ECDH P-256 / HMAC-SHA256）。这与 ICCE（强制 SM2/SM3/SM4）形成鲜明对比。但如果 OEM 同时需要适配 ICCE，建议统一使用 SE050 的混合密码学支持。

### Q2: DK 4.0 的 Session 持久化为什么重要？

A: 当前 `iccoa_dk40.c` 的 Session 管理在内存中操作，车端断电后所有 Session 丢失。如果用户绑定过的手机需要重新走绑定流程，体验差。建议将 Session Token 和密钥状态持久化到 SE050 透明对象存储中，实现热启动恢复。

### Q3: HMAC-SHA256 vs AES-CMAC 如何选择？

A: 当前 yuleDKCS 实现使用 HMAC-SHA256（截断 16 字节）。建议与 ICCOA 联盟最新规范文档确认。如需切换为 AES-CMAC，修改 `iccoa_dk40.c` 中的 `dk40_compute_hmac()` 实现即可，SE050 同时支持两种算法。

### Q4: 如何测试多厂商兼容性？

A: 使用同一台车连接多台不同品牌手机（小米/OPPO/vivo），分别执行完整的钥匙开通→解锁→分享→吊销流程。验证车端支持最多 4 个并发 Session 的正确性。

### Q5: 钥匙分享在 ICCOA 联盟内跨厂商如何工作？

A: ICCOA 联盟定义了跨厂商钥匙分享的标准。发出方在自家钱包 App 中生成分享码（share_code），接收方在自家钱包 App 中输入分享码完成钥匙接收和绑定。yuleDKCS 的 `ShareKey` 和 `AcceptShare` 方法在 `iccoa_adapter.go` 中实现。

### Q6: 如果 OEM TSP 的 API 路径与 ICCOA 规范不同？

A: Adapter 层负责路径映射。`IccoaClient.java` 中的路径常量可以覆盖配置。在 `application-iccoa.yml` 中可以为每个厂商配置不同的 API 路径模板。

---

*本文档应与 [POC 联调指南](poc-guide.md)、[Demo 脚本](demo-script.md) 配套使用。*
