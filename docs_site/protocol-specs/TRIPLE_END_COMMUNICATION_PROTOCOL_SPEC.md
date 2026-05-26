# yuleDKCS 三端通信协议规范

## 文档信息
- 版本: v1.0.0
- 日期: 2026-05-15
- 作者: YuleTech
- 状态: 定稿

## 目录
1. [概述](#1-概述)
2. [三端架构](#2-三端架构)
3. [协议栈对比](#3-协议栈对比)
4. [证书格式规范](#4-证书格式规范)
5. [通信协议格式](#5-通信协议格式)
6. [加密算法规范](#6-加密算法规范)
7. [命令集定义](#7-命令集定义)
8. [错误码定义](#8-错误码定义)
9. [实现状态](#9-实现状态)

---

## 1. 概述

### 1.1 范围
本规范定义了 yuleDKCS 数字钥匙系统三端（Backend/Frontend/Mobile）之间的通信协议，包括：
- CCC Digital Key R3 协议栈
- ICCOA Digital Key 协议栈
- **ICCE Digital Key 协议栈 (国密版)**
- RESTful API 接口规范
- WebSocket 实时通信协议

### 1.2 术语定义
| 术语 | 定义 |
|------|------|
| CCC | Car Connectivity Consortium |
| ICCOA | 智慧车联开放联盟 |
| **ICCE** | **智慧车联产业生态联盟 (国密版)** |
| DK | Digital Key (数字钥匙) |
| SE | Secure Element (安全芯片) |
| APDU | Application Protocol Data Unit |
| TLV | Type-Length-Value |

---

## 2. 三端架构

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                           云端 Backend (Go)                              │
│  ┌────────────────────────────────────────────────────────────────────────┐  │
│  │  API Gateway → Key Service → Crypto Service → SE050 Adapter        │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│          HTTPS/WSS           HTTP/2          PKCS#11                     │
├──────────────────────────────────────────────────────────────────────────────┤
│                              Web Frontend (React/TS)                       │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │  Keys Page → API Client → WebSocket Client → QR Code Scanner       │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
├──────────────────────────────────────────────────────────────────────────────┤
│                              Mobile App                                    │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │  Android(Kotlin) / iOS(Swift) → SDK → BLE/NFC Stack → SE           │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
├──────────────────────────────────────────────────────────────────────────────┤
│                              Vehicle (Embedded)                            │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │  CCC Stack / ICCOA Stack → BLE/NFC/UWL Driver → Crypto HAL         │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. 协议栈对比

| 维度 | CCC | ICCOA | **ICCE** |
|-----|-----|-------|----------|
| 规范版本 | CCC Digital Key R3 | ICCOA-DK-001 (4.0) | **ICCE-DK-001 (2.0)** |
| 协议模式 | BLE/NFC/UWB | BLE/NFC | **BLE/NFC** |
| 证书链 | 最多4层 | CA/非CA双模式 | **国密证书链** |
| 钥匙类型 | Owner/Friend/Guest | Owner/Friend | **Owner/Friend/Temp** |
| 分享深度 | 最多3级 | 最多1级(中间) | **最多2级** |
| 地理栏杆 | 有 | 有 | **有** |
| SE要求 | 必须 | 必须 | **必须 (国密SE)** |
| 算法栈 | ECDSA/AES/SHA-256 | ECDSA/AES/SHA-256 | **SM2/SM4/SM3** |

---

## 4. 证书格式规范

### 4.1 证书类型对比

| 类型代码 | CCC | ICCOA | ICCE | 大小限制 | OID/编码 |
|----------|-----|-------|------|----------|--------|
| 0x01 | Device | VehicleOemCA (A) | VehicleCA | 无/≤1024字节 | 1.3.6.1.4.1.59129.1.1 |
| 0x02 | Vehicle | Vehicle (B) | Vehicle | 无 | 1.3.6.1.4.1.59129.1.2 |
| 0x03 | Server | OwnerDK (C) | OwnerDK | ≤700字节 | 1.3.6.1.4.1.59129.1.3 |
| 0x04 | Intermediate | MidShare (D) | SharedDK | ≤700字节 | 1.3.6.1.4.1.59129.1.4 |
| 0x05 | Root | SharedDK (E) | TempAccess | ≤700字节 | 1.3.6.1.4.1.59129.1.5 |
| 0x0B | - | SharedDK_V2 (K) | - | ≤800字节 | 1.3.6.1.4.1.59129.1.5 |

### 4.2 证书模式对比

```
CCC 证书链 (固定4层):
  Root CA → Intermediate → Vehicle → Device

ICCOA CA模式 (5层):
  VehicleOemCA (A) → Vehicle (B) → Owner (C) → MidShare (D) → Friend (E)

ICCOA 非CA模式 (简化, ICCOA 4.0):
  VehicleOemCA (A) → Vehicle (B) → Owner (C)
                        |
                        → Friend (K) [直接由A签发]
```

### 4.3 证书格式详细对比

| 字段 | CCC | ICCOA | 说明 |
|-----|-----|-------|-----|
| 版本 | X.509 V3 | X.509 V3 | 相同 |
| 签名算法 | ECDSA-SHA256 | ECDSA-SHA256 | 相同 |
| 曲线 | P-256 (secp256r1) | P-256 (secp256r1) | 相同 |
| 编码 | DER/PEM | DER/PEM | 相同 |
| OID命名空间 | 1.3.6.1.4.1.xxx | 1.3.6.1.4.1.59129 | ICCOA固定 |
| 证书模式 | CA模式 | CA(0x00)/非CA(0x01) | ICCOA支持双模式 |

### 4.4 ICCE 证书格式 (国密版)

ICCE 使用自定义二进制格式（非X.509），基于SM2/SM3/SM4国密算法栈。

#### 4.4.1 ICCE 证书结构

```
魔法头 (4字节): 0x49 0x43 0x43 0x45 ("ICCE")

字段格式: [Field ID:1] [Length:2] [Value:N]
    - Field ID:   字段标识
    - Length:     大端序值长度
    - Value:      字段数据
```

#### 4.4.2 ICCE 字段定义

| Field ID | 名称 | 长度 | 描述 |
|----------|------|------|------|
| 0x01 | Version | 1 | 证书版本 (当前=0x01) |
| 0x02 | CertType | 1 | 证书类型 (0x01-0x05) |
| 0x03 | CertLen | 2 | 证书总长度 |
| 0x04 | Issuer | N | 颁发者名称 (UTF-8) |
| 0x05 | Subject | N | 主体名称 (UTF-8) |
| 0x06 | DeviceID | 16 | 设备唯一标识 |
| 0x07 | VehicleID | 17 | 车辆唯一标识 |
| 0x08 | KeyID | 16 | 钥匙唯一标识 |
| 0x09 | ValidFrom | 4 | 生效时间 (Unix时间戳) |
| 0x0A | ValidUntil | 4 | 过期时间 (Unix时间戳) |
| 0x0B | PublicKey | 65 | SM2公钥 (未压缩) |
| 0x0C | Signature | 64 | SM2签名 (r||s) |
| 0x0D | Permissions | 4 | 数字钥匙权限位图 |
| 0x0E | KeyUsage | 2 | 密钥用途 |
| 0x0F | IsCA | 1 | CA标志 (0/1) |
| 0x10 | MaxPathLen | 1 | 证书链最大深度 |
| 0xFF | EndMarker | 0 | 结束标记 |

#### 4.4.3 ICCE 与 ICCOA/CCC 证书对比

| 特性 | ICCE | ICCOA/CCC |
|------|------|-----------|
| 格式 | 自定义二进制 | X.509 DER |
| 签名算法 | SM2-SM3 | ECDSA-SHA256 |
| 加密算法 | SM4 | AES-128 |
| 公钥长度 | 65字节 | 65字节 |
| 签名长度 | 64字节 | 64字节 |
| 证书大小 | ≤1024字节 | ≤700-800字节 |
| 时间格式 | Unix时间戳 | UTC时间 |
| OID支持 | 无 (简化设计) | 完整OID树 |

#### 4.4.4 ICCE 证书链结构

```
VehicleCA (0x01) → Vehicle (0x02) → OwnerDK (0x03) → SharedDK (0x04)
                                       ↓
                                  TempAccess (0x05)

最大链长度: 4层
最大分享深度: 2级 (Owner → Friend)
```

## 5. 通信协议格式

### 5.1 ICCOA 消息格式 (TLV)

```
┌─────────────────────────────────────────────────────────────────────────┐
│                       ICCOA 消息头 (8字节)                                │
├─────────────────────────────────────────────────────────────────────────┤
│  位置  │ 字节  │            描述                                         │
├─────────┼───────┼───────────────────────────────────────────────┤
│  0     │  1   │  Magic: 0xA5                                          │
│  1     │  1   │  Version: 0x12 (1.2)                                  │
│  2-3   │  2   │  Length (大端)                                       │
│  4     │  1   │  Command                                              │
│  5     │  1   │  Flags                                                │
│  6-7   │  2   │  Sequence (大端)                                     │
└─────────┴───────┴───────────────────────────────────────────────┘

Flags 定义:
  bit 0: Encrypted (1=加密)
  bit 1: Fragmented (1=分片)
  bit 2: Last Fragment (1=最后片)
  bit 3: Response (1=响应)
  bit 4: Error (1=错误)
```

### 5.2 TLV 格式

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         TLV 格式 (N x 可变长)                          │
├─────────────────────────────────────────────────────────────────────────┤
│  字段    │ 长度 (字节) │               描述                          │
├──────────┼──────────┼───────────────────────────────────────────────┤
│  Type    │     1       │  TLV类型代码                                    │
│  Length  │     2       │  Value长度 (大端)                               │
│  Value   │    N       │  数据内容                                        │
└──────────┴──────────┴───────────────────────────────────────────────┘
```

### 5.3 ICCOA TLV 类型定义

| 类型代码 | 名称 | 长度 | 描述 |
|---------|------|------|------|
| 0x01 | VERSION | 2 | 协议版本 |
| 0x02 | DEVICE_ID | 16 | 设备标识符 |
| 0x03 | KEY_ID | 16 | 钥匙标识符 |
| 0x04 | TOKEN | 32 | 认证令牌 |
| 0x05 | PUBLIC_KEY | 65 | EC P-256 公钥 |
| 0x06 | SIGNATURE | 64 | ECDSA 签名 |
| 0x07 | CERTIFICATE | 变长 | X.509 证书 |
| 0x08 | CHALLENGE | 16 | 挑战随机数 |
| 0x09 | RESPONSE | 32 | 响应值 |
| 0x0A | SESSION_ID | 4 | 会话标识符 |
| 0x0B | MAC | 16 | AES-CMAC 值 |
| 0x0C | TIMESTAMP | 4 | Unix时间戳 |
| 0x0D | NONCE | 8 | 随机数 |
| 0x0E | PAYLOAD | 变长 | 负载数据 |
| 0x0F | ERROR_CODE | 1 | 错误码 |
| 0x10 | KEY_ROLE | 1 | 钥匙角色 |
| 0x11 | PERMISSIONS | 4 | 权限位图 |
| 0x12 | VALID_FROM | 4 | 有效起始时间 |
| 0x13 | VALID_UNTIL | 4 | 有效截止时间 |
| 0x14 | BT_ADDR | 6 | 蓝牙地址 |
| 0x15 | BLE_ADV_DATA | 变长 | 广播数据 |
| 0x16 | OTA_INFO | 变长 | OTA信息 |
| 0x17 | VEHICLE_STATUS | 16 | 车辆状态 |
| 0x18 | CONTROL_RESULT | 1 | 控制结果 |

---

## 6. 加密算法规范

### 6.1 密码学算法对照

| 算法 | CCC | ICCOA | **ICCE** | 说明 |
|------|-----|-------|----------|------|
| 对称加密 | AES-128-CCM | AES-128-CCM | **SM4-CCM** | ICCE用国密 |
| 非对称加密 | ECDH P-256 | ECDH P-256 | **SM2密钥交换** | ICCE用国密 |
| 消息认证 | AES-CMAC | AES-CMAC | **SM3-HMAC** | ICCE用国密 |
| 散列函数 | SHA-256 | SHA-256 | **SM3** | ICCE用国密 |
| 签名 | ECDSA P-256 | ECDSA P-256 | **SM2** | ICCE用国密 |
| 随机数生成 | TRNG | TRNG | **TRNG** | 相同 |
| 密钥派生 | HKDF-SHA256 | HKDF-SHA256 | **SM3-KDF** | ICCE用国密 |

### 6.2 国密算法详解

#### 6.2.1 SM2 椭圆曲线公钥密码算法

```
曲线参数: 国家密码局推荐的椭圆曲线
安全级别: 等同于 3072位 RSA
公钥长度: 65 字节 (未压缩格式: 0x04 || X || Y)
私钥长度: 32 字节
签名长度: 64 字节 (r || s)

应用场景:
- 证书签名/验签
- 密钥交换 (ECDH)
- 身份认证
```

#### 6.2.2 SM3 杂凑算法

```
输出长度: 256 位 (32 字节)
块大小: 512 位 (64 字节)
轮数: 64 轮

应用场景:
- 数据完整性校验
- 证书哈希
- 签名消息抽象
```

#### 6.2.3 SM4 分组加密算法

```分组大小: 128 位
密钥长度: 128 位 (16 字节)
轮数: 32 轮
结构: 改进的Feistel结构

应用场景:
- 会话加密
- 数据传输保护
- 安全通道建立
```

### 6.3 密钥派生流程

#### 6.3.1 CCC/ICCOA (HKDF)

```
Master Secret = HKDF-Extract(salt, ECDH_Shared_Secret)
Session Key   = HKDF-Expand(Master Secret, "ICCOA-Session-Key", 16)
MAC Key       = HKDF-Expand(Master Secret, "ICCOA-MAC-Key", 16)
```

#### 6.3.2 ICCE (SM3-KDF)

```
Shared Secret = SM2_Compute_Shared_Secret(Private_Key, Peer_Public_Key)
Master Secret = SM3_Hash(Shared_Secret || Salt || Counter)
Session Key   = SM3_Hash(Master_Secret || "ICCE-Session" || Counter)[0:16]
MAC Key       = SM3_Hash(Master_Secret || "ICCE-MAC" || Counter)[0:16]
```

### 6.4 加密消息格式

#### 6.4.1 AES-128-CCM (CCC/ICCOA)

```
├─────────────────────────────────────────────────────────────────────────────────────├────────────────────────────────────────────────────────────────────────────────────├────────────────────────────────────────────────────────────────────────────────────┤
  Nonce (8 bytes)           Ciphertext (variable)                Tag (8 bytes)
```

#### 6.4.2 SM4-CCM (ICCE)

```
├────────────────────────────────────────────────────────────────────────────────────├────────────────────────────────────────────────────────────────────────────────────├────────────────────────────────────────────────────────────────────────────────────┤
  Nonce (8 bytes)           Ciphertext (variable)                Tag (8 bytes)
```

注: SM4-CCM 与 AES-CCM 结构相同，仅替换底层分组加密算法
```

---

## 7. 命令集定义

### 7.1 ICCOA 命令码

#### 系统命令 (0x00-0x0F)
| 命令码 | 命令名称 | 描述 |
|--------|----------|------|
| 0x00 | GET_VERSION | 获取版本信息 |
| 0x01 | GET_DEVICE_INFO | 获取设备信息 |
| 0x02 | PING | 心跳检测 |
| 0x03 | RESET | 重置连接 |
| 0x04 | GET_STATUS | 获取状态 |

#### 配对命令 (0x10-0x1F)
| 命令码 | 命令名称 | 描述 |
|--------|----------|------|
| 0x10 | PAIRING_START | 开始配对 |
| 0x11 | PAIRING_EXCHANGE_KEY | 交换密钥 |
| 0x12 | PAIRING_AUTH | 配对认证 |
| 0x13 | PAIRING_CONFIRM | 确认配对 |
| 0x14 | PAIRING_COMPLETE | 完成配对 |
| 0x15 | PAIRING_CANCEL | 取消配对 |

#### 会话命令 (0x20-0x2F)
| 命令码 | 命令名称 | 描述 |
|--------|----------|------|
| 0x20 | SESSION_CREATE | 创建会话 |
| 0x21 | SESSION_AUTH | 会话认证 |
| 0x22 | SESSION_RENEW | 更新会话 |
| 0x23 | SESSION_CLOSE | 关闭会话 |

#### 钥匙管理命令 (0x30-0x3F)
| 命令码 | 命令名称 | 描述 |
|--------|----------|------|
| 0x30 | KEY_REGISTER | 注册钥匙 |
| 0x31 | KEY_ACTIVATE | 激活钥匙 |
| 0x32 | KEY_SUSPEND | 暂停钥匙 |
| 0x33 | KEY_RESUME | 恢复钥匙 |
| 0x34 | KEY_DELETE | 删除钥匙 |
| 0x35 | KEY_LIST | 列表钥匙 |
| 0x36 | KEY_INFO | 获取钥匙信息 |
| 0x37 | KEY_SHARE | 分享钥匙 |
| 0x38 | KEY_TRANSFER | 转让钥匙 |

#### 车辆控制命令 (0x40-0x4F)
| 命令码 | 命令名称 | 描述 |
|--------|----------|------|
| 0x40 | VEHICLE_UNLOCK | 解锁车辆 |
| 0x41 | VEHICLE_LOCK | 上锁车辆 |
| 0x42 | ENGINE_START | 启动发动机 |
| 0x43 | ENGINE_STOP | 关闭发动机 |
| 0x44 | TRUNK_OPEN | 开启后备箱 |
| 0x45 | TRUNK_CLOSE | 关闭后备箱 |
| 0x46 | WINDOWS_UP | 升起车窗 |
| 0x47 | WINDOWS_DOWN | 降下车窗 |
| 0x48 | CLIMATE_ON | 开启空调 |
| 0x49 | CLIMATE_OFF | 关闭空调 |
| 0x4A | HORN | 按下喇叭 |
| 0x4B | LIGHTS | 闪灯 |
| 0x4C | CHARGE_PORT_OPEN | 开启充电口 |
| 0x4D | CHARGE_PORT_CLOSE | 关闭充电口 |

### 7.2 RESTful API 接口

#### 钥匙管理接口
| 方法 | 路径 | 描述 | 状态 |
|------|------|------|------|
| GET | /api/v1/keys | 获取钥匙列表 | ✓ 已实现 |
| POST | /api/v1/keys | 创建钥匙 | ✓ 已实现 |
| GET | /api/v1/keys/:id | 获取钥匙详情 | ✓ 已实现 |
| PUT | /api/v1/keys/:id | 更新钥匙 | ✓ 已实现 |
| DELETE | /api/v1/keys/:id | 删除钥匙 | ✓ 已实现 |
| POST | /api/v1/keys/:id/share | 分享钥匙 | ✓ 已实现 |
| POST | /api/v1/keys/:id/suspend | 暂停钥匙 | ✓ 已实现 |
| POST | /api/v1/keys/:id/resume | 恢复钥匙 | ✓ 已实现 |
| GET | /api/v1/keys/:id/logs | 获取使用记录 | ✓ 已实现 |

#### 车辆控制接口
| 方法 | 路径 | 描述 | 状态 |
|------|------|------|------|
| POST | /api/v1/vehicles/:id/unlock | 解锁车辆 | ✓ 已实现 |
| POST | /api/v1/vehicles/:id/lock | 上锁车辆 | ✓ 已实现 |
| POST | /api/v1/vehicles/:id/engine/start | 启动发动机 | ✓ 已实现 |
| POST | /api/v1/vehicles/:id/engine/stop | 关闭发动机 | ✓ 已实现 |
| GET | /api/v1/vehicles/:id/status | 获取车辆状态 | ✓ 已实现 |

---

## 8. 错误码定义

### 8.1 ICCOA 错误码 (0x01-0x1F)

| 错误码 | 名称 | 描述 |
|--------|------|------|
| 0x00 | OK | 成功 |
| 0x01 | ERR_UNSUPPORTED_CMD | 不支持的命令 |
| 0x02 | ERR_INVALID_PARAM | 无效参数 |
| 0x03 | ERR_INVALID_LENGTH | 长度错误 |
| 0x04 | ERR_AUTH_FAILED | 认证失败 |
| 0x05 | ERR_KEY_NOT_FOUND | 钥匙不存在 |
| 0x06 | ERR_KEY_REVOKED | 钥匙已撤销 |
| 0x07 | ERR_KEY_EXPIRED | 钥匙已过期 |
| 0x08 | ERR_SESSION_INVALID | 会话无效 |
| 0x09 | ERR_BUSY | 设备忙 |
| 0x0A | ERR_TIMEOUT | 超时 |
| 0x0B | ERR_MEMORY | 内存不足 |
| 0x0C | ERR_CRYPTO | 加密错误 |
| 0x0D | ERR_TRANSPORT | 传输错误 |
| 0x0F | ERR_INTERNAL | 内部错误 |
| 0x10 | ERR_PAIRING_CANCELLED | 配对已取消 |
| 0x11 | ERR_PAIRING_TIMEOUT | 配对超时 |
| 0x12 | ERR_VERSION_MISMATCH | 版本不匹配 |
| 0x13 | ERR_SE_UNAVAILABLE | SE不可用 |
| 0x14 | ERR_PERMISSION_DENIED | 权限不足 |
| 0x15 | ERR_GEO_FENCE | 地理栏杆限制 |
| 0x16 | ERR_SPEED_LIMIT | 速度限制 |

### 8.2 ICCOA 证书错误码 (-200至-211)

| 错误码 | 名称 | 描述 |
|--------|------|------|
| -200 | CERT_ERROR_INVALID_PARAM | 无效参数 |
| -201 | CERT_ERROR_INVALID_FORMAT | 格式无效 |
| -202 | CERT_ERROR_EXPIRED | 证书已过期 |
| -203 | CERT_ERROR_NOT_YET_VALID | 证书尚未生效 |
| -204 | CERT_ERROR_INVALID_SIGNATURE | 签名无效 |
| -205 | CERT_ERROR_UNSUPPORTED_ALGORITHM | 不支持的算法 |
| -206 | CERT_ERROR_SIZE_EXCEEDED | 大小超限 |
| -207 | CERT_ERROR_INVALID_CHAIN | 证书链无效 |
| -208 | CERT_ERROR_TRUST_ANCHOR_NOT_FOUND | 找不到信任锚 |
| -209 | CERT_ERROR_REVOKED | 证书已撤销 |
| -210 | CERT_ERROR_INVALID_MODE | 无效模式 |
| -211 | CERT_ERROR_INVALID_TYPE | 无效类型 |

---

## 9. 实现状态

### 9.1 后端 (Backend) 实现状态

| 模块 | 文件路径 | 状态 | 统计 |
|------|---------|------|------|
| ICCOA证书模块 | `backend/internal/iccoa/cert/` | ✓ 完成 | 4,165行 |
| 证书生成器 | `generator.go` | ✓ 完成 | 757行 |
| 证书验证器 | `validator.go` | ✓ 完成 | 384行 |
| 证书存储层 | `store.go` | ✓ 完成 | 796行 |
| 证书服务层 | `service.go` | ✓ 完成 | 432行 |
| 数据库迁移 | `migration.sql` | ✓ 完成 | - |
| 单元测试 | `*_test.go` | ✓ 通过 | 1,192行 |
| API接口 | `backend/cmd/api/` | ✓ 完成 | - |
| JWT中间件 | `middleware/jwt.go` | ✓ 完成 | - |

### 9.2 前端 (Frontend) 实现状态

| 模块 | 文件路径 | 状态 |
|------|---------|------|
| 钥匙管理页面 | `src/pages/KeysPage.tsx` | ✓ 完成 |
| 钥匙详情页面 | `src/pages/KeyDetailPage.tsx` | ✓ 完成 |
| 使用记录页面 | `src/pages/KeyUsageLogsPage.tsx` | ✓ 完成 |
| 分享弹窗 | `src/components/ShareKeyDialog.tsx` | ✓ 完成 |
| API客户端 | `src/api/keys.ts` | ✓ 完成 |
| 路由配置 | `src/router/index.tsx` | ✓ 已更新 |

### 9.3 移动端 (Mobile) 实现状态

| 平台 | 模块 | 文件路径 | 状态 |
|-----|------|---------|------|
| Android | KeyManager | `KeyManager.kt` | ✓ 完成 |
| Android | BLEManager | `BLEManager.kt` | ✓ 完成 |
| Android | Models | `model/Models.kt` | ✓ 完成 |
| iOS | KeyManager | `KeyManager.swift` | ✓ 完成 |
| iOS | BLEManager | `BLEManager.swift` | ✓ 完成 |
| iOS | Models | `Models.swift` | ✓ 完成 |
| SDK | 使用文档 | `MOBILE_SDK_USAGE_GUIDE.md` | ✓ 完成 |

### 9.4 车端 (Embedded) 实现状态

| 模块 | 文件路径 | 状态 | 统计 |
|------|---------|------|------|
| CCC证书模块 | `embedded/sdk/src/protocol/ccc_certificate.c/h` | ✅ 完成 | 1,315行 |
| ICCOA协议头 | `embedded/include/iccoa.h` | ✅ 完成 | 658行 |
| ICCOA核心 | `embedded/src/iccoa/protocol/iccoa_core.c` | ✅ 完成 | ~600行 |
| ICCOA DK 4.0 | `embedded/src/iccoa/dk40/iccoa_dk40.c` | ✅ 完成 | ~380行 |
| **ICCOA证书模块** | `embedded/src/iccoa/security/iccoa_certificate.c/h` | **✅ 完成** | **1,361行** |
| **ICCE证书模块** | `embedded/src/icce/security/icce_certificate.c/h` | **✅ 完成** | **1,368行** |
| BLE驱动 | `embedded/src/iccoa/ble/iccoa_ble.c` | ✅ 完成 | ~270行 |
| 认证模块 | `embedded/src/iccoa/auth/iccoa_auth.c` | ✅ 完成 | ~50行 |
| 服务模块 | `embedded/src/iccoa/service/iccoa_service.c` | ✅ 完成 | ~170行 |

### 9.5 测试覆盖率

| 模块 | 测试类型 | 覆盖率 |
|------|----------|--------|
| 后端 ICCOA证书 | 单元测试 | 87% |
| 后端 ICCOA证书 | 集成测试 | 待完成 |
| 车端 CCC证书 | 单元测试 | 85% |
| 车端 ICCOA证书 | 单元测试 | 待完成 |
| **车端 ICCE证书** | **单元测试** | **待完成** |
| API接口 | 集成测试 | 待完成 |
| E2E测试 | 手动测试 | 待完成 |

---

## 附录

### A. 三端通信时序图

```
Mobile          Backend          Vehicle
  |                |                |
  |--1. Pair Req-->|                |
  |                |                |
  |<--2. Pair Ack--|                |
  |                |                |
  |--3. Key Exchg->|                |
  |                |--4. Forward--->|
  |                |                |
  |                |<--5. Resp------|
  |                |                |
  |<--6. Complete--|                |
  |                |                |
  |==== Session Established =======|
  |                |                |
  |--7. Unlock---->|--8. Cmd------->|
  |                |                |
  |                |<--9. Status----|
  |<--10. Result---|                |
  |                |                |
```

### B. 文档版本历史

| 版本 | 日期 | 描述 |
|------|------|------|
| 1.0.0 | 2026-05-15 | 首次发布，完整三端协议规范 |

### C. 参考文档

1. CCC Digital Key Release 3 Specification
2. ICCOA/T 002-2024 数字钥匙技术规范
3. **ICCE Digital Key 2.0 规范 (国密版)**
4. GM/T 0003.1-2012 SM2椭圆曲线公钥密码算法
5. GM/T 0004-2012 SM3杂凑算法
6. GM/T 0002-2012 SM4分组加密算法
7. ISO/IEC 18013-5 移动驾驶凭证
8. NIST SP 800-56A 密钥协议
9. RFC 7748/8032 ECDH/EdDSA
10. RFC 5869 HKDF

---

*本文档由 YuleDKCS 项目自动生成系统创建*
*最后更新: 2026-05-15*
