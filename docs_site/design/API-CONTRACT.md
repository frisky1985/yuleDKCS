# 数字钥匙系统 — API 接口合同文档

> **版本**：v1.0  
> **日期**：2026-04-26  
> **作者**：code_designer  
> **状态**：正式版

---

## 目录

1. [BLE GATT 服务定义](#1-ble-gatt-服务定义)
2. [NFC APDU 命令集](#2-nfc-apdu-命令集)
3. [App 与云端 RESTful API](#3-app-与云端-restful-api)
4. [云端内部 gRPC 接口 Proto 定义](#4-云端内部-grpc-接口-proto-定义)
5. [消息队列 Topic 定义](#5-消息队列-topic-定义)

---

## 1. BLE GATT 服务定义

### 1.1 服务总览

| 服务名称 | UUID | 类型 | 说明 |
|---------|------|------|------|
| **Digital Key Service (CCC)** | `0xF5A3` | Primary Service | CCC Digital Key 3.0 主服务 |
| **Digital Key Service (ICCE)** | `A000000F5A3ICCE` | Primary Service | ICCE 数字钥匙服务 |
| **Battery Service** | `0x180F` | Standard BLE | 电池电量服务 |
| **Device Information** | `0x180A` | Standard BLE | 设备信息服务 |

### 1.2 CCC Digital Key GATT 服务详细定义

#### 1.2.1 主服务声明

```
Service UUID: 0xF5A3 (Primary Service)
Service Name: CCC Digital Key Service
```

#### 1.2.2 Characteristic 定义

| Characteristic Name | UUID | Properties | Format | Description |
|---------------------|------|------------|--------|-------------|
| **Key Pairing** | `0000F5A4-0000-1000-8000-00805F9B34FB` | WRITE, INDICATE |见下表| 钥匙配对握手 |
| **Auth Challenge** | `0000F5A5-0000-1000-8000-00805F9B34FB` | READ, INDICATE |见下表| 服务器挑战 |
| **Auth Response** | `0000F5A6-0000-1000-8000-00805F9B34FB` | WRITE |见下表| 认证响应 |
| **Vehicle Control** | `0000F5A7-0000-1000-8000-00805F9B34FB` | WRITE, INDICATE |见下表| 车辆控制指令 |
| **UWB Ranging** | `0000F5A8-0000-1000-8000-00805F9B34FB` | WRITE, NOTIFY |见下表| UWB 测距控制与结果 |
| **Key Status** | `0000F5A9-0000-1000-8000-00805F9B34FB` | READ, NOTIFY |见下表| 钥匙状态 |
| **Vehicle Status** | `0000F5AA-0000-1000-8000-00805F9B34FB` | READ, NOTIFY |见下表| 车辆状态 |
| **OTA Control** | `0000F5AB-0000-1000-8000-00805F9B34FB` | WRITE, INDICATE |见下表| OTA 升级控制 |

#### 1.2.3 Key Pairing Characteristic 详细格式

**Request (App → Vehicle):**

| 字段 | 类型 | 长度 | 说明 |
|-----|------|------|------|
| `command` | uint8 | 1 | 0x01=开始配对, 0x02=配对确认, 0x03=取消配对 |
| `protocol` | uint8 | 1 | 0x01=ICCE, 0x02=CCC, 0x03=Auto |
| `publicKey` | bytes | 64 | 公钥数据 (P-256 X9.62 格式 或 SM2) |
| `nonce` | bytes | 16 | 随机挑战值 |
| `keyType` | uint8 | 1 | 0x01=车主钥匙, 0x02=副钥匙, 0x03=临时钥匙 |
| `keyId` | bytes | 16 | 钥匙唯一标识 (可选) |

**Response (Vehicle → App) - Indications:**

| 字段 | 类型 | 长度 | 说明 |
|-----|------|------|------|
| `status` | uint8 | 1 | 0x00=成功, 0x01=协议不支持, 0x02=配对码错误, 0x03=SE错误 |
| `serverPublicKey` | bytes | 64 | 车端公钥 |
| `serverNonce` | bytes | 16 | 车端随机数 |
| `sessionId` | bytes | 16 | 配对会话ID |
| `extras` | bytes | 变长 | 扩展字段 |

#### 1.2.4 Auth Challenge Characteristic 详细格式

**Value (Vehicle → App):**

| 字段 | 类型 | 长度 | 说明 |
|-----|------|------|------|
| `challenge` | bytes | 32 | 服务器随机挑战 |
| `timestamp` | uint64 | 8 | UTC 时间戳 (毫秒) |
| `sequenceNumber` | uint32 | 4 | 递增序列号 |

#### 1.2.5 Auth Response Characteristic 详细格式

**Value (App → Vehicle):**

| 字段 | 类型 | 长度 | 说明 |
|-----|------|------|------|
| `signature` | bytes | 64 | 签名值 (ECDSA P-256 / SM2) |
| `certificate` | bytes | 变长 | 证书链 (DER 编码) |
| `counter` | uint32 | 4 | 防重放计数器 |
| `timestamp` | uint64 | 8 | 客户端时间戳 |

#### 1.2.6 Vehicle Control Characteristic 详细格式

**Request (App → Vehicle):**

| 字段 | 类型 | 长度 | 说明 |
|-----|------|------|------|
| `action` | uint8 | 1 | 见 Action 定义表 |
| `authToken` | bytes | 32 | 认证令牌 |
| `parameterLength` | uint16 | 2 | 参数长度 |
| `parameters` | bytes | 变长 | 参见各 Action 参数 |

**Action 定义:**

| Action | Value | 参数 | 说明 |
|--------|-------|------|------|
| `UNLOCK` | 0x01 | 无 | 解锁所有车门 |
| `UNLOCK_DRIVER` | 0x02 | 无 | 仅解锁主驾车门 |
| `LOCK` | 0x03 | 无 | 上锁所有车门 |
| `START_ENGINE` | 0x04 | `engineMode` | 启动发动机 |
| `STOP_ENGINE` | 0x05 | 无 | 停止发动机 |
| `TRUNK_OPEN` | 0x06 | 无 | 开启后备箱 |
| `TRUNK_CLOSE` | 0x07 | 无 | 关闭后备箱 |
| `HORN_LIGHTS` | 0x08 | `duration` | 寻车 (鸣笛闪灯) |
| `CLIMATE_ON` | 0x09 | `temperature`, `duration` | 开启空调 |
| `CLIMATE_OFF` | 0x0A | 无 | 关闭空调 |
| `WINDOWS_UP` | 0x0B | 无 | 升起车窗 |
| `WINDOWS_DOWN` | 0x0C | `position` | 降下车窗 |
| `SEAT_HEATING` | 0x0D | `seat`, `level` | 座椅加热 |
| `VALET_MODE` | 0x0E | `enable`, `maxSpeed` | 代客泊车模式 |

**Response (Vehicle → App) - Indications:**

| 字段 | 类型 | 长度 | 说明 |
|-----|------|------|------|
| `requestId` | bytes | 16 | 请求唯一标识 |
| `status` | uint8 | 1 | 0x00=成功, 0x01=认证失败, 0x02=权限不足, 0x03=车端忙碌, 0x04=车辆条件不满足 |
| `action` | uint8 | 1 | 对应的 Action |
| `executedAt` | uint64 | 8 | 执行时间戳 |
| `errorCode` | uint16 | 2 | 错误码 (失败时填充) |

#### 1.2.7 UWB Ranging Characteristic 详细格式

**Request (App → Vehicle):**

| 字段 | 类型 | 长度 | 说明 |
|-----|------|------|------|
| `action` | uint8 | 1 | 0x01=开始测距, 0x02=停止测距, 0x03=配置 |
| `sessionId` | uint32 | 4 | UWB 会话ID |
| `channel` | uint8 | 1 | UWB 通道 (0x01=CH5, 0x02=CH9) |
| `sessionType` | uint8 | 1 | 0x01=测距, 0x02=定位 |
| `slotDuration` | uint16 | 2 | 时隙时长 (ms) |

**Notification (Vehicle → App):**

| 字段 | 类型 | 长度 | 说明 |
|-----|------|------|------|
| `sessionId` | uint32 | 4 | 会话ID |
| `sequenceNumber` | uint32 | 4 | 序列号 |
| `status` | uint8 | 1 | 0x00=进行中, 0x01=完成, 0x02=超时, 0x03=失败 |
| `distance` | float | 4 | 距离 (米, 精确到厘米) |
| `angleAzimuth` | float | 4 | 方位角 (度) |
| `angleElevation` | float | 4 | 俯仰角 (度) |
| `confidence` | float | 4 | 置信度 (0-1) |
| `isSecure` | bool | 1 | 是否为安全测距 |
| `signature` | bytes | 64 | 测距结果签名 |

#### 1.2.8 Key Status Characteristic 详细格式

**Value:**

| 字段 | 类型 | 长度 | 说明 |
|-----|------|------|------|
| `keyId` | bytes | 16 | 钥匙ID |
| `status` | uint8 | 1 | 0x01=激活, 0x02=暂停, 0x03=已吊销, 0x04=已过期 |
| `keyType` | uint8 | 1 | 钥匙类型 |
| `permissions` | bytes | 8 | 权限位图 |
| `validFrom` | uint64 | 8 | 生效时间 |
| `validUntil` | uint64 | 8 | 失效时间 |
| `usageCount` | uint32 | 4 | 已使用次数 |
| `maxUsageCount` | uint32 | 4 | 最大使用次数 (-1=无限) |

#### 1.2.9 Vehicle Status Characteristic 详细格式

**Value:**

| 字段 | 类型 | 长度 | 说明 |
|-----|------|------|------|
| `doorStatus` | uint8 | 1 | 位标志: bit0=左前, bit1=右前, bit2=左后, bit3=右后, bit4=后备箱 |
| `windowStatus` | uint8 | 1 | 位标志: bit0-3=各车窗 |
| `engineStatus` | uint8 | 1 | 0x00=关闭, 0x01=运行, 0x02=远程启动中 |
| `batteryVoltage` | uint16 | 2 | 电池电压 (mV) |
| `fuelLevel` | uint8 | 1 | 油量/电量百分比 |
| `temperature` | int16 | 2 | 车内温度 (0.1°C) |
| `location` | bytes | 12 | 位置信息 (加密) |
| `timestamp` | uint64 | 8 | 状态时间戳 |

### 1.3 ICCE Digital Key GATT 服务定义

#### 1.3.1 主服务声明

```
Service UUID: A000000F5A3ICCE (Primary Service, 128-bit)
Service Name: ICCE Digital Key Service
```

#### 1.3.2 ICCE Characteristic 定义

| Characteristic Name | UUID (16-bit) | UUID (128-bit) | Properties | 说明 |
|---------------------|---------------|----------------|------------|------|
| **ICCE Pairing Request** | 0xF5B0 | - | WRITE, INDICATE | ICCE 配对请求 |
| **ICCE Pairing Response** | 0xF5B1 | - | READ, NOTIFY | ICCE 配对响应 |
| **ICCE Auth Request** | 0xF5B2 | - | WRITE, INDICATE | ICCE 认证请求 |
| **ICCE Auth Response** | 0xF5B3 | - | READ, NOTIFY | ICCE 认证响应 |
| **ICCE Control** | 0xF5B4 | - | WRITE, INDICATE | ICCE 车辆控制 |
| **ICCE Ranging** | 0xF5B5 | - | WRITE, NOTIFY | ICCE UWB 测距 |
| **ICCE Key Info** | 0xF5B6 | - | READ, NOTIFY | ICCE 钥匙信息 |
| **ICCE Vehicle Info** | 0xF5B7 | - | READ, NOTIFY | ICCE 车辆信息 |

#### 1.3.3 ICCE Pairing Request 格式

**TLV 编码结构:**

| Tag | 名称 | 长度 | 说明 |
|-----|------|------|------|
| 0x01 | `ProtocolVersion` | 1 | 协议版本 (0x10=T/CA 110-2020) |
| 0x02 | `DeviceType` | 1 | 0x01=手机, 0x02=手表, 0x03=手环 |
| 0x03 | `PublicKey` | 64 | SM2 公钥 |
| 0x04 | `Certificate` | 变长 | 设备证书 (GM/T 0015) |
| 0x05 | `Nonce` | 16 | 随机数 |
| 0x06 | `KeyType` | 1 | 0x01=车主钥匙, 0x02=副钥匙, 0x03=临时钥匙 |
| 0x07 | `Capabilities` | 2 | 能力标志位图 |
| 0x08 | `Timestamp` | 8 | 时间戳 |

### 1.4 BLE 连接参数

| 参数 | 值 | 说明 |
|-----|-----|------|
| 连接间隔 | 30ms ~ 50ms | 平衡响应速度与功耗 |
| 从机延迟 | 0 | 无延迟 |
| 超时监控 | 400ms | 连接超时 |
| MTU | 512 bytes | 最大传输单元 |
| 广播间隔 | 100ms ~ 1000ms | 可配置 |
| PHY | 1M / 2M | 物理层速率 |

---

## 2. NFC APDU 命令集

### 2.1 ISO/IEC 7816-4 APDU 基础格式

#### 2.1.1 APDU 命令结构

```
Header (4 bytes): CLA | INS | P1 | P2
Body:
  -Lc (1-3 bytes): 数据字段长度
  -Data (Lc bytes): 命令数据
  -Le (1-3 bytes): 期望响应长度
```

#### 2.1.2 APDU 响应结构

```
Response:
  -Data (可变): 响应数据
  -SW1 SW2 (2 bytes): 状态字
```

**常用状态码:**

| SW1 SW2 | 含义 |
|---------|------|
| `90 00` | 成功 |
| `63 00` | 签名校验失败 |
| `69 82` | 安全状态不满足 |
| `69 84` | 引用数据无效 |
| `6A 81` | 功能不支持 |
| `6A 82` | 文件/应用未找到 |
| `6A 86` | P1/P2 参数不正确 |
| `6A 88` | 引用数据未找到 |
| `6D 00` | INS 不支持 |
| `6E 00` | CLA 不支持 |

### 2.2 Digital Key AID

| AID | 版本 | 说明 |
|-----|------|------|
| `A000000F5A3` | CCC DK 3.0 | CCC Digital Key |
| `A000000F5A3ICCE` | ICCE | ICCE 数字钥匙 |

### 2.3 CCC Digital Key APDU 命令集

#### 2.3.1 应用选择 SELECT

**命令:**

```
CLA: 00
INS: A4
P1:  04        // 直接选择文件
P2:  00        // 首次读
Lc:  07 (或 0C)
Data: AID (7 bytes 或 12 bytes)
Le:  00        // 返回全部可用数据
```

**响应:**

```
Data:
  FCI (File Control Information):
    - 卡片序列号
    - 支持的能力
    - 安全状态
SW: 90 00
```

#### 2.3.2 获取挑战 GET CHALLENGE

**命令:**

```
CLA: 80
INS: 84
P1:  00
P2:  00
Lc:  00
Le:  20        // 32字节挑战
```

**响应:**

```
Data: 32字节随机挑战
SW: 90 00
```

#### 2.3.3 内部认证 INTERNAL AUTHENTICATE

**命令:**

```
CLA: 80
INS: 88
P1:  00
P2:  00
Lc:  01 00     // 认证模板长度
Data:
  80 01 01     // 认证模板: 纯数字挑战, 协议1
  81 20        // 挑战数据
    <32字节挑战>
  82 01 01     // 关键模板: 纯数字结果
Le:  40        // 期望64字节签名
```

**响应:**

```
Data:
  80 40        // 结果模板
    <64字节签名>
SW: 90 00
```

#### 2.3.4 读二进制数据 READ BINARY

**命令:**

```
CLA: 00
INS: B0
P1:  <高5位: SFI> <低3位: offset高3位>
P2:  <offset低8位>
Lc:  00
Le:  <要读取的字节数>
```

**常用 SFI:**

| SFI | 文件名 | 内容 |
|-----|-------|------|
| 0x01 | EF.KeyInfo | 钥匙基本信息 |
| 0x02 | EF.KeyPermissions | 钥匙权限列表 |
| 0x03 | EF.ValidityPeriod | 有效期信息 |
| 0x04 | EF.VehicleInfo | 车辆信息 |
| 0x05 | EF.Counter | 防重放计数器 |

#### 2.3.5 更新二进制 UPDATE BINARY

**命令:**

```
CLA: 00
INS: D6
P1:  <SFI>
P2:  <offset>
Lc:  <数据长度>
Data: <要写入的数据>
Le:  00
```

#### 2.3.6 车辆控制命令 MANAGE CHANNEL / VERIFY

**解锁命令示例:**

```
CLA: 80
INS: 2A            // VERIFY
P1:  90            // ICCE控制
P2:  01            // 解锁
Lc:  00
Le:  00
```

#### 2.3.7 获取钥匙状态 GET DATA

**命令:**

```
CLA: 80
INS: CA
P1:  00
P2:  00
Lc:  02
Data: <数据标签 (2 bytes)>
Le:  <期望长度>
```

**数据标签定义:**

| 标签 | 含义 |
|-----|------|
| `00 01` | 钥匙状态 |
| `00 02` | 钥匙类型 |
| `00 03` | 权限位图 |
| `00 04` | 有效期开始 |
| `00 05` | 有效期结束 |
| `00 06` | 剩余使用次数 |
| `00 07` | 车辆ID |
| `00 08` | 配对时间 |

### 2.4 ICCE APDU 命令集

#### 2.4.1 ICCE 应用选择

**命令:**

```
CLA: 00
INS: A4
P1:  04
P2:  00
Data: A000000F5A3ICCE
```

#### 2.4.2 ICCE 挑战获取

```
CLA: 80
INS: 84
P1:  00
P2:  00
Le:  20        // SM2 挑战长度
```

#### 2.4.3 ICCE 认证 (国密)

```
CLA: 80
INS: 88
P1:  00
P2:  00
Data: SM2挑战数据 + keyId
Le:  41        // SM2 签名长度 (65字节) + 1
```

#### 2.4.4 ICCE 车辆控制

**解锁:**

```
CLA: 80
INS: 20
P1:  01        // ICCE 控制类
P2:  01        // 解锁命令
```

**上锁:**

```
CLA: 80
INS: 20
P1:  01
P2:  02        // 上锁命令
```

**启动:**

```
CLA: 80
INS: 20
P1:  01
P2:  03        // 启动引擎
Data: <安全认证数据>
```

### 2.5 NFC 交互流程

#### 2.5.1 CCC NFC 完整交易流程

```
┌─────────────────────────────────────────────────────────────────┐
│                     CCC NFC 刷卡交易流程                          │
├─────────────────────────────────────────────────────────────────┤
│  1. SELECT                                                     │
│     App → Card: 00 A4 04 00 07 A000000F5A3 00                   │
│     Card → App: <FCI> 90 00                                     │
│                                                                  │
│  2. GET CHALLENGE                                              │
│     App → Card: 80 84 00 00 00 20                              │
│     Card → App: <32-byte challenge> 90 00                      │
│                                                                  │
│  3. INTERNAL AUTHENTICATE                                      │
│     App → Card: 80 88 00 00 <challenger_data> 40               │
│     Card → App: <64-byte signature> 90 00                      │
│                                                                  │
│  4. [可选] VERIFY PIN                                          │
│     App → Card: 00 20 00 81 08 <PIN>                           │
│     Card → App: 90 00                                          │
│                                                                  │
│  5. EXTERNAL AUTHENTICATE (车端验证手机)                        │
│     App → Card: 00 82 00 00 <auth_data>                         │
│     Card → App: 90 00                                          │
│                                                                  │
│  6. 控制命令 (UNLOCK)                                          │
│     App → Card: 80 2A 90 01 00                                 │
│     Card → App: 90 00                                          │
│                                                                  │
│  7. [可选] 状态查询 GET DATA                                   │
│     App → Card: 80 CA 00 00 02 00 01 00                        │
│     Card → App: <status> 90 00                                │
└─────────────────────────────────────────────────────────────────┘
```

---

## 3. App 与云端 RESTful API

### 3.1 API 基础信息

| 项目 | 值 |
|-----|---|
| 基础 URL | `https://api.digitalkey.example.com/api/v1` |
| 认证方式 | Bearer Token (JWT) |
| 内容类型 | `application/json` |
| 字符编码 | UTF-8 |
| 协议版本 | TLS 1.3 |

### 3.2 通用响应格式

**成功响应:**

```json
{
  "code": 0,
  "message": "success",
  "data": { ... },
  "requestId": "req_abc123xyz",
  "timestamp": 1714118400000
}
```

**错误响应:**

```json
{
  "code": 10001,
  "message": "Invalid parameter",
  "details": {
    "field": "vin",
    "reason": "VIN checksum validation failed"
  },
  "requestId": "req_abc123xyz",
  "timestamp": 1714118400000
}
```

**错误码定义:**

| Code | 说明 |
|------|------|
| 0 | 成功 |
| 10001 | 参数错误 |
| 10002 | 认证失败 |
| 10003 | Token 过期 |
| 10004 | 权限不足 |
| 20001 | 钥匙不存在 |
| 20002 | 钥匙已过期 |
| 20003 | 钥匙已吊销 |
| 30001 | 车辆不存在 |
| 30002 | 车辆未绑定 |
| 30003 | 车辆离线 |
| 40001 | 分享不存在 |
| 40002 | 分享已过期 |
| 50001 | 系统错误 |
| 50002 | 服务不可用 |

### 3.3 用户认证 API

#### 3.3.1 发送验证码

```
POST /auth/send-code
```

**Request:**

```json
{
  "phone": "+8613912345678",
  "type": "LOGIN"  // LOGIN | REGISTER | RESET_PASSWORD | BIND_PHONE
}
```

**Response:**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "codeId": "code_abc123",
    "expiresIn": 300
  }
}
```

#### 3.3.2 手机号登录

```
POST /auth/login
```

**Request:**

```json
{
  "phone": "+8613912345678",
  "code": "123456",
  "deviceInfo": {
    "deviceId": "device_xxx",
    "deviceType": "iOS",
    "osVersion": "17.0",
    "appVersion": "1.0.0",
    "deviceModel": "iPhone 15 Pro"
  }
}
```

**Response:**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "userId": "usr_abc123",
    "accessToken": "eyJhbGciOiJSUzI1NiIs...",
    "refreshToken": "eyJhbGciOiJSUzI1NiIs...",
    "expiresIn": 3600,
    "refreshExpiresIn": 604800
  }
}
```

#### 3.3.3 Token 刷新

```
POST /auth/token/refresh
```

**Request:**

```json
{
  "refreshToken": "eyJhbGciOiJSUzI1NiIs..."
}
```

**Response:**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "accessToken": "eyJhbGciOiJSUzI1NiIs...",
    "refreshToken": "eyJhbGciOiJSUzI1NiIs...",
    "expiresIn": 3600,
    "refreshExpiresIn": 604800
  }
}
```

#### 3.3.4 退出登录

```
POST /auth/logout
Headers:
  Authorization: Bearer <accessToken>
```

**Response:**

```json
{
  "code": 0,
  "message": "success"
}
```

#### 3.3.5 获取用户信息

```
GET /auth/profile
Headers:
  Authorization: Bearer <accessToken>
```

**Response:**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "userId": "usr_abc123",
    "phone": "+8613912345678",
    "email": "user@example.com",
    "nickname": "张三",
    "avatar": "https://cdn.example.com/avatar/xxx.jpg",
    "bindVehicles": 2,
    "createdAt": "2024-01-15T10:30:00Z"
  }
}
```

#### 3.3.6 第三方登录

```
POST /auth/oauth
```

**Request:**

```json
{
  "provider": "apple",  // apple | wechat | google
  "idToken": "eyJhbGciOiJFUzI1NiIs...",
  "authorizationCode": "...",
  "deviceInfo": { ... }
}
```

### 3.4 车辆管理 API

#### 3.4.1 获取车辆列表

```
GET /vehicles
Headers:
  Authorization: Bearer <accessToken>
```

**Response:**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "vehicles": [
      {
        "vehicleId": "veh_abc123",
        "vin": "LSVXXXXXXXXXXXXXXXX",
        "licensePlate": "京A12345",
        "brand": "比亚迪",
        "model": "汉 EV",
        "modelYear": 2024,
        "color": "深空灰",
        "protocolType": "ICCE",
        "bindStatus": "ACTIVE",
        "keys": [
          {
            "keyId": "key_xxx",
            "keyType": "PRIMARY",
            "status": "ACTIVE",
            "isDefault": true
          }
        ],
        "thumbnail": "https://cdn.example.com/vehicle/xxx.jpg"
      }
    ]
  }
}
```

#### 3.4.2 绑定车辆

```
POST /vehicles/bind
Headers:
  Authorization: Bearer <accessToken>
```

**Request:**

```json
{
  "vin": "LSVXXXXXXXXXXXXXXXX",
  "proofType": "QR_CODE",  // QR_CODE | MANUAL | DEALER
  "proofData": "xxxxxx",   // 配对码或相关证明
  "nickname": "我的小汉"
}
```

**Response:**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "vehicleId": "veh_abc123",
    "bindId": "bind_xxx",
    "status": "PENDING",
    "message": "请在车辆中控屏确认配对"
  }
}
```

#### 3.4.3 解绑车辆

```
DELETE /vehicles/{vehicleId}/bind
Headers:
  Authorization: Bearer <accessToken>
```

**Request:**

```json
{
  "reason": "更换车辆"
}
```

**Response:**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "vehicleId": "veh_abc123",
    "keysRevoked": 3
  }
}
```

#### 3.4.4 获取车辆详情

```
GET /vehicles/{vehicleId}
Headers:
  Authorization: Bearer <accessToken>
```

**Response:**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "vehicleId": "veh_abc123",
    "vin": "LSVXXXXXXXXXXXXXXXX",
    "licensePlate": "京A12345",
    "brand": "比亚迪",
    "model": "汉 EV",
    "modelYear": 2024,
    "color": "深空灰",
    "protocolType": "ICCE",
    "bindStatus": "ACTIVE",
    "bindTime": "2024-01-20T14:30:00Z",
    "supportedFeatures": [
      "UNLOCK", "LOCK", "START_ENGINE", "TRUNK",
      "CLIMATE", "WINDOWS", "HORN_LIGHTS", "LOCATION"
    ],
    "otaInfo": {
      "currentVersion": "v2.1.0",
      "latestVersion": "v2.2.0",
      "hasUpdate": true
    }
  }
}
```

#### 3.4.5 获取车辆状态

```
GET /vehicles/{vehicleId}/status
Headers:
  Authorization: Bearer <accessToken>
```

**Response:**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "vehicleId": "veh_abc123",
    "online": true,
    "status": {
      "doors": {
        "driver": "LOCKED",
        "passenger": "LOCKED",
        "rearLeft": "LOCKED",
        "rearRight": "LOCKED",
        "trunk": "CLOSED"
      },
      "windows": {
        "driver": "CLOSED",
        "passenger": "CLOSED",
        "sunroof": "CLOSED"
      },
      "engine": "OFF",
      "location": {
        "latitude": 39.9042,
        "longitude": 116.4074,
        "address": "北京市朝阳区xxx"
      },
      "battery": {
        "level": 85,
        "range": 520,
        "charging": false
      },
      "climate": {
        "on": false,
        "temperature": 25
      }
    },
    "lastUpdated": "2024-04-26T11:30:00Z"
  }
}
```

### 3.5 钥匙管理 API

#### 3.5.1 创建钥匙

```
POST /keys
Headers:
  Authorization: Bearer <accessToken>
```

**Request:**

```json
{
  "vehicleId": "veh_abc123",
  "keyType": "PRIMARY",
  "protocol": "ICCE"
}
```

**Response:**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "keyId": "key_abc123",
    "publicKey": "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgA...",
    "pairingToken": "pair_xxx",
    "expiresAt": 1714118400000
  }
}
```

#### 3.5.2 获取钥匙列表

```
GET /keys
Headers:
  Authorization: Bearer <accessToken>
Query Parameters:
  vehicleId: string (可选)
  status: string (可选: ACTIVE | SUSPENDED | REVOKED)
  page: number (默认: 1)
  pageSize: number (默认: 20)
```

**Response:**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "keys": [
      {
        "keyId": "key_abc123",
        "vehicleId": "veh_abc123",
        "vehicleName": "比亚迪 汉 EV",
        "keyType": "PRIMARY",
        "status": "ACTIVE",
        "permissions": [
          "UNLOCK", "LOCK", "START_ENGINE", "TRUNK",
          "CLIMATE", "WINDOWS", "HORN_LIGHTS"
        ],
        "createdAt": "2024-01-20T14:30:00Z",
        "expiresAt": null,
        "lastUsedAt": "2024-04-25T18:30:00Z",
        "deviceInfo": {
          "deviceId": "device_xxx",
          "deviceModel": "iPhone 15 Pro"
        }
      }
    ],
    "pagination": {
      "total": 3,
      "page": 1,
      "pageSize": 20,
      "totalPages": 1
    }
  }
}
```

#### 3.5.3 获取钥匙详情

```
GET /keys/{keyId}
Headers:
  Authorization: Bearer <accessToken>
```

**Response:**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "keyId": "key_abc123",
    "vehicleId": "veh_abc123",
    "vehicleName": "比亚迪 汉 EV",
    "keyType": "PRIMARY",
    "status": "ACTIVE",
    "permissions": ["UNLOCK", "LOCK", "START_ENGINE", ...],
    "validity": {
      "startAt": "2024-01-20T14:30:00Z",
      "endAt": null,
      "maxUsageCount": -1,
      "usageCount": 156
    },
    "security": {
      "publicKey": "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgA...",
      "certificateSerial": "CERT_xxx",
      "lastAuthAt": "2024-04-25T18:30:00Z"
    },
    "deviceInfo": { ... },
    "createdAt": "2024-01-20T14:30:00Z",
    "updatedAt": "2024-04-25T18:30:00Z"
  }
}
```

#### 3.5.4 暂停钥匙

```
POST /keys/{keyId}/suspend
Headers:
  Authorization: Bearer <accessToken>
```

**Request:**

```json
{
  "reason": "手机丢失"
}
```

**Response:**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "keyId": "key_abc123",
    "status": "SUSPENDED",
    "suspendedAt": "2024-04-26T11:00:00Z"
  }
}
```

#### 3.5.5 恢复钥匙

```
POST /keys/{keyId}/resume
Headers:
  Authorization: Bearer <accessToken>
```

**Response:**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "keyId": "key_abc123",
    "status": "ACTIVE"
  }
}
```

#### 3.5.6 吊销钥匙

```
DELETE /keys/{keyId}
Headers:
  Authorization: Bearer <accessToken>
```

**Request:**

```json
{
  "reason": "车辆已出售",
  "notifyUser": true
}
```

**Response:**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "keyId": "key_abc123",
    "status": "REVOKED",
    "revokedAt": "2024-04-26T11:00:00Z"
  }
}
```

### 3.6 钥匙分享 API

#### 3.6.1 创建分享

```
POST /keys/{keyId}/shares
Headers:
  Authorization: Bearer <accessToken>
```

**Request:**

```json
{
  "shareType": "LINK",  // LINK | USER | PHONE
  "targetUserId": null,  // 特定用户ID
  "targetPhone": null,    // 目标手机号
  "permissions": ["UNLOCK", "LOCK"],
  "validity": {
    "type": "TIME_RANGE",  // TIME_RANGE | USAGE_COUNT | PERMANENT
    "startAt": "2024-04-26T12:00:00Z",
    "endAt": "2024-04-26T14:00:00Z",
    "maxUsageCount": null
  },
  "restrictions": {
    "geoFence": {
      "enabled": true,
      "type": "CIRCLE",
      "center": { "latitude": 39.9042, "longitude": 116.4074 },
      "radius": 5000  // 米
    },
    "prohibitedActions": []
  },
  "message": "临时借车2小时，请注意安全"
}
```

**Response:**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "shareId": "share_abc123",
    "shareLink": "https://digitalkey.app/s/abc123xyz",
    "shareCode": "DK789012",
    "expiresAt": "2024-04-26T12:00:00Z",
    "permissions": ["UNLOCK", "LOCK"],
    "qrCode": "https://api.digitalkey.example.com/qr/abc123"
  }
}
```

#### 3.6.2 获取分享列表

```
GET /keys/{keyId}/shares
Headers:
  Authorization: Bearer <accessToken>
```

**Response:**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "shares": [
      {
        "shareId": "share_abc123",
        "shareType": "LINK",
        "recipient": null,
        "recipientPhone": null,
        "status": "ACTIVE",
        "permissions": ["UNLOCK", "LOCK"],
        "validity": {
          "type": "TIME_RANGE",
          "startAt": "2024-04-26T12:00:00Z",
          "endAt": "2024-04-26T14:00:00Z"
        },
        "usageCount": 3,
        "createdAt": "2024-04-26T11:30:00Z"
      }
    ]
  }
}
```

#### 3.6.3 接受分享

```
POST /shares/{shareId}/accept
Headers:
  Authorization: Bearer <accessToken>
```

**Request:**

```json
{
  "deviceInfo": { ... }
}
```

**Response:**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "shareId": "share_abc123",
    "keyId": "key_new_xxx",
    "vehicleId": "veh_abc123",
    "vehicleName": "比亚迪 汉 EV",
    "permissions": ["UNLOCK", "LOCK"],
    "validity": {
      "endAt": "2024-04-26T14:00:00Z"
    }
  }
}
```

#### 3.6.4 撤销分享

```
DELETE /keys/{keyId}/shares/{shareId}
Headers:
  Authorization: Bearer <accessToken>
```

**Response:**

```json
{
  "code": 0,
  "message": "success"
}
```

### 3.7 远程控车 API

#### 3.7.1 发送控制指令

```
POST /vehicles/{vehicleId}/controls
Headers:
  Authorization: Bearer <accessToken>
```

**Request:**

```json
{
  "action": "CLIMATE_ON",
  "parameters": {
    "temperature": 22,
    "fanSpeed": 2,
    "defrost": true
  },
  "priority": "NORMAL",  // LOW | NORMAL | HIGH
  "timeout": 30  // 秒
}
```

**Response:**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "requestId": "ctrl_abc123",
    "action": "CLIMATE_ON",
    "status": "PENDING",
    "vehicleOnline": true,
    "estimatedExecutionTime": 5
  }
}
```

#### 3.7.2 查询控制指令状态

```
GET /vehicles/{vehicleId}/controls/{requestId}
Headers:
  Authorization: Bearer <accessToken>
```

**Response:**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "requestId": "ctrl_abc123",
    "action": "CLIMATE_ON",
    "status": "EXECUTED",  // PENDING | EXECUTING | EXECUTED | FAILED | TIMEOUT
    "executedAt": "2024-04-26T11:30:05Z",
    "result": {
      "previousState": { "on": false },
      "newState": { "on": true, "temperature": 22 }
    },
    "error": null
  }
}
```

#### 3.7.3 控制历史

```
GET /vehicles/{vehicleId}/controls/history
Headers:
  Authorization: Bearer <accessToken>
Query Parameters:
  startTime: ISO8601
  endTime: ISO8601
  action: string (可选)
  page: number
  pageSize: number
```

**Response:**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "records": [
      {
        "requestId": "ctrl_abc123",
        "action": "UNLOCK",
        "initiatedBy": "APP",
        "initiatorId": "usr_xxx",
        "status": "EXECUTED",
        "executedAt": "2024-04-25T18:30:00Z",
        "location": {
          "latitude": 39.9042,
          "longitude": 116.4074
        }
      }
    ],
    "pagination": { ... }
  }
}
```

### 3.8 OTA 升级 API

#### 3.8.1 检查更新

```
GET /vehicles/{vehicleId}/ota/check
Headers:
  Authorization: Bearer <accessToken>
```

**Response:**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "hasUpdate": true,
    "update": {
      "version": "v2.2.0",
      "previousVersion": "v2.1.0",
      "size": 52428800,  // 50MB
      "checksum": "sha256:abc123...",
      "description": "1. 修复蓝牙连接问题\n2. 优化解锁响应速度",
      "mandatory": false,
      "deadline": null,
      "downloadUrl": "https://ota.digitalkey.example.com/v2.2.0/firmware.bin",
      "signature": "..."
    }
  }
}
```

#### 3.8.2 确认升级

```
POST /vehicles/{vehicleId}/ota/confirm
Headers:
  Authorization: Bearer <accessToken>
```

**Request:**

```json
{
  "version": "v2.2.0",
  "installNow": false,
  "scheduledTime": "2024-04-27T02:00:00Z"
}
```

**Response:**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "taskId": "ota_abc123",
    "status": "SCHEDULED",
    "scheduledTime": "2024-04-27T02:00:00Z"
  }
}
```

#### 3.8.3 查询升级状态

```
GET /vehicles/{vehicleId}/ota/status
Headers:
  Authorization: Bearer <accessToken>
```

**Response:**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "currentTask": {
      "taskId": "ota_abc123",
      "version": "v2.2.0",
      "status": "DOWNLOADING",  // DOWNLOAD_PENDING | DOWNLOADING | VERIFYING | INSTALLING | REBOOTING | COMPLETED | FAILED
      "progress": 45,
      "downloadedSize": 23592960,
      "totalSize": 52428800,
      "error": null
    },
    "currentVersion": "v2.1.0",
    "lastUpdateAt": null
  }
}
```

### 3.9 审计日志 API

#### 3.9.1 获取审计日志

```
GET /audit/logs
Headers:
  Authorization: Bearer <accessToken>
Query Parameters:
  keyId: string (可选)
  vehicleId: string (可选)
  action: string (可选)
  startTime: ISO8601
  endTime: ISO8601
  page: number
  pageSize: number
```

**Response:**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "logs": [
      {
        "logId": "log_abc123",
        "timestamp": "2024-04-26T11:30:00Z",
        "action": "KEY_UNLOCK",
        "keyId": "key_abc123",
        "vehicleId": "veh_abc123",
        "userId": "usr_xxx",
        "result": "SUCCESS",
        "location": {
          "latitude": 39.9042,
          "longitude": 116.4074,
          "address": "北京市朝阳区xxx"
        },
        "deviceInfo": {
          "deviceId": "device_xxx",
          "deviceModel": "iPhone 15 Pro"
        },
        "metadata": {
          "authMethod": "UWB",
          "distance": 1.5
        }
      }
    ],
    "pagination": { ... }
  }
}
```

---

## 4. 云端内部 gRPC 接口 Proto 定义

### 4.1 Proto 文件结构

```
digitalkey/
├── key/
│   └── v1/
│       ├── key_service.proto
│       └── key_types.proto
├── vehicle/
│   └── v1/
│       ├── vehicle_service.proto
│       └── vehicle_types.proto
├── auth/
│   └── v1/
│       ├── auth_service.proto
│       └── auth_types.proto
├── kms/
│   └── v1/
│       ├── kms_service.proto
│       └── kms_types.proto
├── event/
│   └── v1/
│       └── event_service.proto
└── common/
    └── v1/
        ├── common.proto
        └── pagination.proto
```

### 4.2 Key Service Proto

```protobuf
// digitalkey/key/v1/key_service.proto
syntax = "proto3";

package digitalkey.key.v1;

import "digitalkey/key/v1/key_types.proto";
import "digitalkey/common/v1/pagination.proto";
import "google/protobuf/timestamp.proto";
import "google/api/field_behavior.proto";
import "google/api/annotations.proto";

option go_package = "github.com/digitalkey/key-service/gen/go/key/v1";

// 钥匙管理服务
service KeyService {
  // 创建钥匙
  rpc CreateKey(CreateKeyRequest) returns (CreateKeyResponse) {
    option (google.api.http) = {
      post: "/api/v1/keys"
      body: "*"
    };
  }
  
  // 获取钥匙
  rpc GetKey(GetKeyRequest) returns (Key) {
    option (google.api.http) = {
      get: "/api/v1/keys/{key_id}"
    };
  }
  
  // 列表钥匙
  rpc ListKeys(ListKeysRequest) returns (ListKeysResponse) {
    option (google.api.http) = {
      get: "/api/v1/keys"
    };
  }
  
  // 更新钥匙
  rpc UpdateKey(UpdateKeyRequest) returns (Key) {
    option (google.api.http) = {
      patch: "/api/v1/keys/{key_id}"
      body: "*"
    };
  }
  
  // 暂停钥匙
  rpc SuspendKey(SuspendKeyRequest) returns (SuspendKeyResponse);
  
  // 恢复钥匙
  rpc ResumeKey(ResumeKeyRequest) returns (ResumeKeyResponse);
  
  // 吊销钥匙
  rpc RevokeKey(RevokeKeyRequest) returns (RevokeKeyResponse);
  
  // 创建分享
  rpc CreateShare(CreateShareRequest) returns (CreateShareResponse);
  
  // 撤销分享
  rpc RevokeShare(RevokeShareRequest) returns (RevokeShareResponse);
  
  // 接受分享
  rpc AcceptShare(AcceptShareRequest) returns (AcceptShareResponse);
  
  // 获取分享列表
  rpc ListShares(ListSharesRequest) returns (ListSharesResponse);
  
  // 钥匙配对初始化
  rpc InitiatePairing(InitiatePairingRequest) returns (InitiatePairingResponse);
  
  // 确认配对
  rpc ConfirmPairing(ConfirmPairingRequest) returns (ConfirmPairingResponse);
  
  // 同步钥匙状态到车端
  rpc SyncKeyToVehicle(SyncKeyToVehicleRequest) returns (SyncKeyToVehicleResponse);
}

// 创建钥匙请求
message CreateKeyRequest {
  string user_id = 1 [(google.api.field_behavior) = REQUIRED];
  string vehicle_id = 2 [(google.api.field_behavior) = REQUIRED];
  KeyType key_type = 3 [(google.api.field_behavior) = REQUIRED];
  Protocol protocol = 4 [(google.api.field_behavior) = REQUIRED];
  bytes public_key = 5 [(google.api.field_behavior) = REQUIRED];
  string certificate = 6;
  google.protobuf.Timestamp valid_from = 7;
  google.protobuf.Timestamp valid_until = 8;
  int32 max_usage_count = 9;
  KeyPermissions permissions = 10;
  map<string, string> metadata = 11;
}

// 创建钥匙响应
message CreateKeyResponse {
  string key_id = 1;
  bytes server_public_key = 2;
  bytes pairing_token = 3;
  google.protobuf.Timestamp expires_at = 4;
}

// 获取钥匙请求
message GetKeyRequest {
  string key_id = 1 [(google.api.field_behavior) = REQUIRED];
}

// 列表钥匙请求
message ListKeysRequest {
  string user_id = 1;
  string vehicle_id = 2;
  KeyStatus status = 3;
  digitalkey.common.v1.PaginationRequest pagination = 4;
}

// 列表钥匙响应
message ListKeysResponse {
  repeated Key keys = 1;
  digitalkey.common.v1.PaginationResponse pagination = 2;
}

// 更新钥匙请求
message UpdateKeyRequest {
  string key_id = 1 [(google.api.field_behavior) = REQUIRED];
  KeyPermissions permissions = 2;
  google.protobuf.Timestamp valid_until = 3;
  int32 max_usage_count = 4;
  map<string, string> metadata = 5;
}

// 暂停钥匙请求
message SuspendKeyRequest {
  string key_id = 1 [(google.api.field_behavior) = REQUIRED];
  string reason = 2;
}

// 暂停钥匙响应
message SuspendKeyResponse {
  string key_id = 1;
  KeyStatus status = 2;
  google.protobuf.Timestamp suspended_at = 3;
}

// 恢复钥匙请求
message ResumeKeyRequest {
  string key_id = 1 [(google.api.field_behavior) = REQUIRED];
}

// 恢复钥匙响应
message ResumeKeyResponse {
  string key_id = 1;
  KeyStatus status = 2;
}

// 吊销钥匙请求
message RevokeKeyRequest {
  string key_id = 1 [(google.api.field_behavior) = REQUIRED];
  string reason = 2;
  bool notify_user = 3;
}

// 吊销钥匙响应
message RevokeKeyResponse {
  string key_id = 1;
  KeyStatus status = 2;
  google.protobuf.Timestamp revoked_at = 3;
}

// 创建分享请求
message CreateShareRequest {
  string key_id = 1 [(google.api.field_behavior) = REQUIRED];
  ShareType share_type = 2;
  string target_user_id = 3;
  string target_phone = 4;
  repeated Permission permissions = 5 [(google.api.field_behavior) = REQUIRED];
  ValidityPeriod validity = 6 [(google.api.field_behavior) = REQUIRED];
  GeoFenceRestriction geo_fence = 7;
  string message = 8;
}

// 创建分享响应
message CreateShareResponse {
  string share_id = 1;
  string share_link = 2;
  string share_code = 3;
  google.protobuf.Timestamp expires_at = 4;
}

// 撤销分享请求
message RevokeShareRequest {
  string share_id = 1 [(google.api.field_behavior) = REQUIRED];
}

// 撤销分享响应
message RevokeShareResponse {
  string share_id = 1;
  ShareStatus status = 2;
}

// 接受分享请求
message AcceptShareRequest {
  string share_id = 1 [(google.api.field_behavior) = REQUIRED];
  string user_id = 2 [(google.api.field_behavior) = REQUIRED];
  string device_info_json = 3;
}

// 接受分享响应
message AcceptShareResponse {
  string share_id = 1;
  string key_id = 2;
  string vehicle_id = 3;
  string vehicle_name = 4;
}

// 列表分享请求
message ListSharesRequest {
  string key_id = 1 [(google.api.field_behavior) = REQUIRED];
}

// 列表分享响应
message ListSharesResponse {
  repeated Share shares = 1;
}

// 配对初始化请求
message InitiatePairingRequest {
  string vehicle_id = 1 [(google.api.field_behavior) = REQUIRED];
  string key_id = 2 [(google.api.field_behavior) = REQUIRED];
  Protocol protocol = 3 [(google.api.field_behavior) = REQUIRED];
}

// 配对初始化响应
message InitiatePairingResponse {
  string session_id = 1;
  bytes server_public_key = 2;
  bytes nonce = 3;
  google.protobuf.Timestamp expires_at = 4;
}

// 确认配对请求
message ConfirmPairingRequest {
  string session_id = 1 [(google.api.field_behavior) = REQUIRED];
  bytes signature = 2 [(google.api.field_behavior) = REQUIRED];
  bytes certificate = 3;
}

// 确认配对响应
message ConfirmPairingResponse {
  string key_id = 1;
  KeyStatus status = 2;
}

// 同步钥匙到车端请求
message SyncKeyToVehicleRequest {
  string vehicle_id = 1 [(google.api.field_behavior) = REQUIRED];
  repeated string key_ids = 2 [(google.api.field_behavior) = REQUIRED];
}

// 同步钥匙到车端响应
message SyncKeyToVehicleResponse {
  bool success = 1;
  string error = 2;
}
```

### 4.3 Key Types Proto

```protobuf
// digitalkey/key/v1/key_types.proto
syntax = "proto3";

package digitalkey.key.v1;

import "google/protobuf/timestamp.proto";

// 钥匙类型
enum KeyType {
  KEY_TYPE_UNSPECIFIED = 0;
  PRIMARY = 1;        // 车主钥匙
  SECONDARY = 2;      // 副钥匙
  TEMPORARY = 3;     // 临时钥匙
  SERVICE = 4;        // 服务钥匙
}

// 钥匙状态
enum KeyStatus {
  KEY_STATUS_UNSPECIFIED = 0;
  CREATED = 1;        // 已创建
  PAIRED = 2;         // 已配对
  ACTIVE = 3;         // 激活
  SUSPENDED = 4;      // 暂停
  REVOKED = 5;        // 已吊销
  EXPIRED = 6;        // 已过期
}

// 协议类型
enum Protocol {
  PROTOCOL_UNSPECIFIED = 0;
  ICCE = 1;           // 国内标准
  CCC = 2;            // 国际标准
}

// 分享类型
enum ShareType {
  SHARE_TYPE_UNSPECIFIED = 0;
  LINK = 1;           // 链接分享
  USER = 2;           // 指定用户
  PHONE = 3;          // 手机号分享
}

// 分享状态
enum ShareStatus {
  SHARE_STATUS_UNSPECIFIED = 0;
  PENDING = 1;        // 待接受
  ACCEPTED = 2;        // 已接受
  REVOKED = 3;         // 已撤销
  EXPIRED = 4;         // 已过期
}

// 权限
enum Permission {
  PERMISSION_UNSPECIFIED = 0;
  UNLOCK = 1;
  LOCK = 2;
  START_ENGINE = 3;
  STOP_ENGINE = 4;
  TRUNK_OPEN = 5;
  TRUNK_CLOSE = 6;
  CLIMATE_ON = 7;
  CLIMATE_OFF = 8;
  HORN_LIGHTS = 9;
  WINDOWS_UP = 10;
  WINDOWS_DOWN = 11;
  SEAT_HEATING = 12;
  VALET_MODE = 13;
  LOCATION = 14;
}

// 钥匙
message Key {
  string key_id = 1;
  string user_id = 2;
  string vehicle_id = 3;
  KeyType key_type = 4;
  KeyStatus status = 5;
  Protocol protocol = 6;
  KeyPermissions permissions = 7;
  ValidityPeriod validity = 8;
  SecurityInfo security = 9;
  DeviceInfo device_info = 10;
  map<string, string> metadata = 11;
  google.protobuf.Timestamp created_at = 12;
  google.protobuf.Timestamp updated_at = 13;
  google.protobuf.Timestamp last_used_at = 14;
}

// 钥匙权限
message KeyPermissions {
  repeated Permission permissions = 1;
}

// 有效期
message ValidityPeriod {
  google.protobuf.Timestamp valid_from = 1;
  google.protobuf.Timestamp valid_until = 2;
  int32 max_usage_count = 3;
  int32 usage_count = 4;
}

// 安全信息
message SecurityInfo {
  string public_key = 1;
  string certificate_serial = 2;
  string algorithm = 3;
  google.protobuf.Timestamp last_auth_at = 4;
  int32 auth_failure_count = 5;
}

// 设备信息
message DeviceInfo {
  string device_id = 1;
  string device_type = 2;
  string device_model = 3;
  string os_version = 4;
  string app_version = 5;
}

// 分享
message Share {
  string share_id = 1;
  string key_id = 2;
  string from_user_id = 3;
  string to_user_id = 4;
  string recipient_phone = 5;
  ShareType share_type = 6;
  ShareStatus status = 7;
  repeated Permission permissions = 8;
  ValidityPeriod validity = 9;
  GeoFenceRestriction geo_fence = 10;
  string message = 11;
  int32 usage_count = 12;
  google.protobuf.Timestamp created_at = 13;
  google.protobuf.Timestamp accepted_at = 14;
}

// 地理围栏限制
message GeoFenceRestriction {
  bool enabled = 1;
  GeoFenceType type = 2;
  double center_latitude = 3;
  double center_longitude = 4;
  int64 radius_meters = 5;
  repeated string allowed_regions = 6;
}

// 地理围栏类型
enum GeoFenceType {
  GEO_FENCE_TYPE_UNSPECIFIED = 0;
  CIRCLE = 1;
  POLYGON = 2;
  REGION = 3;
}
```

### 4.4 Vehicle Service Proto

```protobuf
// digitalkey/vehicle/v1/vehicle_service.proto
syntax = "proto3";

package digitalkey.vehicle.v1;

import "digitalkey/vehicle/v1/vehicle_types.proto";
import "digitalkey/common/v1/pagination.proto";
import "google/protobuf/timestamp.proto";
import "google/api/field_behavior.proto";
import "google/api/annotations.proto";

option go_package = "github.com/digitalkey/vehicle-service/gen/go/vehicle/v1";

// 车辆管理服务
service VehicleService {
  // 获取车辆列表
  rpc ListVehicles(ListVehiclesRequest) returns (ListVehiclesResponse) {
    option (google.api.http) = {
      get: "/internal/v1/vehicles"
    };
  }
  
  // 获取车辆详情
  rpc GetVehicle(GetVehicleRequest) returns (Vehicle) {
    option (google.api.http) = {
      get: "/internal/v1/vehicles/{vehicle_id}"
    };
  }
  
  // 绑定车辆
  rpc BindVehicle(BindVehicleRequest) returns (BindVehicleResponse);
  
  // 解绑车辆
  rpc UnbindVehicle(UnbindVehicleRequest) returns (UnbindVehicleResponse);
  
  // 获取车辆状态
  rpc GetVehicleStatus(GetVehicleStatusRequest) returns (VehicleStatus);
  
  // 获取车辆实时状态 (从车端)
  rpc GetRealTimeStatus(GetRealTimeStatusRequest) returns (GetRealTimeStatusResponse);
  
  // 发送车辆控制指令
  rpc SendControlCommand(SendControlCommandRequest) returns (SendControlCommandResponse);
  
  // 获取控制命令状态
  rpc GetControlCommandStatus(GetControlCommandStatusRequest) returns (ControlCommandStatus);
  
  // OTA 检查
  rpc CheckOtaUpdate(CheckOtaUpdateRequest) returns (CheckOtaUpdateResponse);
  
  // OTA 状态上报
  rpc ReportOtaStatus(ReportOtaStatusRequest) returns (ReportOtaStatusResponse);
  
  // 同步钥匙到车辆
  rpc SyncKeysToVehicle(SyncKeysToVehicleRequest) returns (SyncKeysToVehicleResponse);
}

// 获取车辆列表请求
message ListVehiclesRequest {
  string user_id = 1 [(google.api.field_behavior) = REQUIRED];
}

// 获取车辆列表响应
message ListVehiclesResponse {
  repeated Vehicle vehicles = 1;
}

// 获取车辆详情请求
message GetVehicleRequest {
  string vehicle_id = 1 [(google.api.field_behavior) = REQUIRED];
}

// 绑定车辆请求
message BindVehicleRequest {
  string user_id = 1 [(google.api.field_behavior) = REQUIRED];
  string vin = 2 [(google.api.field_behavior) = REQUIRED];
  string proof_type = 3;
  string proof_data = 4;
  string nickname = 5;
}

// 绑定车辆响应
message BindVehicleResponse {
  string vehicle_id = 1;
  string bind_id = 2;
  BindStatus status = 3;
}

// 解绑车辆请求
message UnbindVehicleRequest {
  string vehicle_id = 1 [(google.api.field_behavior) = REQUIRED];
  string user_id = 2 [(google.api.field_behavior) = REQUIRED];
  string reason = 3;
}

// 解绑车辆响应
message UnbindVehicleResponse {
  string vehicle_id = 1;
  int32 keys_revoked = 2;
}

// 获取车辆状态请求
message GetVehicleStatusRequest {
  string vehicle_id = 1 [(google.api.field_behavior) = REQUIRED];
}

// 获取实时状态请求
message GetRealTimeStatusRequest {
  string vehicle_id = 1 [(google.api.field_behavior) = REQUIRED];
  int32 timeout_seconds = 2;
}

// 获取实时状态响应
message GetRealTimeStatusResponse {
  VehicleStatus status = 1;
}

// 发送控制命令请求
message SendControlCommandRequest {
  string vehicle_id = 1 [(google.api.field_behavior) = REQUIRED];
  string user_id = 2 [(google.api.field_behavior) = REQUIRED];
  string key_id = 3 [(google.api.field_behavior) = REQUIRED];
  ControlAction action = 4 [(google.api.field_behavior) = REQUIRED];
  string parameters_json = 5;
  int32 timeout_seconds = 6;
}

// 发送控制命令响应
message SendControlCommandResponse {
  string request_id = 1;
  CommandStatus status = 2;
  int32 estimated_execution_time = 3;
}

// 获取控制命令状态请求
message GetControlCommandStatusRequest {
  string vehicle_id = 1 [(google.api.field_behavior) = REQUIRED];
  string request_id = 2 [(google.api.field_behavior) = REQUIRED];
}

// 检查 OTA 更新请求
message CheckOtaUpdateRequest {
  string vehicle_id = 1 [(google.api.field_behavior) = REQUIRED];
  string current_version = 2;
}

// 检查 OTA 更新响应
message CheckOtaUpdateResponse {
  bool has_update = 1;
  OtaUpdate update = 2;
}

// 上报 OTA 状态请求
message ReportOtaStatusRequest {
  string vehicle_id = 1 [(google.api.field_behavior) = REQUIRED];
  string task_id = 2;
  OtaStatus status = 3;
  int32 progress = 4;
  string error = 5;
}

// 上报 OTA 状态响应
message ReportOtaStatusResponse {
  bool acknowledged = 1;
}

// 同步钥匙到车辆请求
message SyncKeysToVehicleRequest {
  string vehicle_id = 1 [(google.api.field_behavior) = REQUIRED];
  repeated VehicleKey keys = 2 [(google.api.field_behavior) = REQUIRED];
}

// 同步钥匙到车辆响应
message SyncKeysToVehicleResponse {
  bool success = 1;
  string error = 2;
  int32 synced_count = 3;
}
```

### 4.5 KMS Service Proto

```protobuf
// digitalkey/kms/v1/kms_service.proto
syntax = "proto3";

package digitalkey.kms.v1;

import "digitalkey/kms/v1/kms_types.proto";
import "google/protobuf/timestamp.proto";
import "google/api/field_behavior.proto";

option go_package = "github.com/digitalkey/kms-service/gen/go/kms/v1";

// 密钥管理服务
service KMSService {
  // 生成密钥对
  rpc GenerateKeyPair(GenerateKeyPairRequest) returns (GenerateKeyPairResponse);
  
  // 签名
  rpc Sign(SignRequest) returns (SignResponse);
  
  // 验证签名
  rpc VerifySignature(VerifySignatureRequest) returns (VerifySignatureResponse);
  
  // 生成会话密钥
  rpc GenerateSessionKey(GenerateSessionKeyRequest) returns (GenerateSessionKeyResponse);
  
  // 加密数据
  rpc Encrypt(EncryptRequest) returns (EncryptResponse);
  
  // 解密数据
  rpc Decrypt(DecryptRequest) returns (DecryptResponse);
  
  // 生成随机数
  rpc GenerateRandom(GenerateRandomRequest) returns (GenerateRandomResponse);
  
  // 导入密钥
  rpc ImportKey(ImportKeyRequest) returns (ImportKeyResponse);
  
  // 销毁密钥
  rpc DestroyKey(DestroyKeyRequest) returns (DestroyKeyResponse);
  
  // 获取证书
  rpc GetCertificate(GetCertificateRequest) returns (GetCertificateResponse);
}

// 生成密钥对请求
message GenerateKeyPairRequest {
  KeyAlgorithm algorithm = 1 [(google.api.field_behavior) = REQUIRED];
  string key_id = 2;
  KeyUsage usage = 3 [(google.api.field_behavior) = REQUIRED];
  google.protobuf.Timestamp valid_until = 4;
}

// 生成密钥对响应
message GenerateKeyPairResponse {
  string key_id = 1;
  bytes public_key = 2;
  string algorithm = 3;
  google.protobuf.Timestamp created_at = 4;
}

// 签名请求
message SignRequest {
  string key_id = 1 [(google.api.field_behavior) = REQUIRED];
  bytes data = 2 [(google.api.field_behavior) = REQUIRED];
  SignAlgorithm algorithm = 3 [(google.api.field_behavior) = REQUIRED];
}

// 签名响应
message SignResponse {
  bytes signature = 1;
}

// 验证签名请求
message VerifySignatureRequest {
  string key_id = 1 [(google.api.field_behavior) = REQUIRED];
  bytes data = 2 [(google.api.field_behavior) = REQUIRED];
  bytes signature = 3 [(google.api.field_behavior) = REQUIRED];
  SignAlgorithm algorithm = 4 [(google.api.field_behavior) = REQUIRED];
}

// 验证签名响应
message VerifySignatureResponse {
  bool valid = 1;
}

// 生成会话密钥请求
message GenerateSessionKeyRequest {
  string algorithm = 1 [(google.api.field_behavior) = REQUIRED];
  int32 key_length = 2;
}

// 生成会话密钥响应
message GenerateSessionKeyResponse {
  bytes session_key = 1;
  bytes encrypted_session_key = 2;
  google.protobuf.Timestamp expires_at = 3;
}

// 加密请求
message EncryptRequest {
  string key_id = 1 [(google.api.field_behavior) = REQUIRED];
  bytes plaintext = 2 [(google.api.field_behavior) = REQUIRED];
  EncryptAlgorithm algorithm = 3 [(google.api.field_behavior) = REQUIRED];
  bytes additional_data = 4;
}

// 加密响应
message EncryptResponse {
  bytes ciphertext = 1;
  bytes iv = 2;
  bytes tag = 3;
}

// 解密请求
message DecryptRequest {
  string key_id = 1 [(google.api.field_behavior) = REQUIRED];
  bytes ciphertext = 2 [(google.api.field_behavior) = REQUIRED];
  bytes iv = 3 [(google.api.field_behavior) = REQUIRED];
  bytes tag = 4;
  bytes additional_data = 5;
}

// 解密响应
message DecryptResponse {
  bytes plaintext = 1;
}

// 生成随机数请求
message GenerateRandomRequest {
  int32 length = 1 [(google.api.field_behavior) = REQUIRED];
}

// 生成随机数响应
message GenerateRandomResponse {
  bytes random = 1;
}

// 导入密钥请求
message ImportKeyRequest {
  string key_id = 1 [(google.api.field_behavior) = REQUIRED];
  KeyAlgorithm algorithm = 2 [(google.api.field_behavior) = REQUIRED];
  bytes key_material = 3 [(google.api.field_behavior) = REQUIRED];
  KeyUsage usage = 4 [(google.api.field_behavior) = REQUIRED];
}

// 导入密钥响应
message ImportKeyResponse {
  string key_id = 1;
  google.protobuf.Timestamp imported_at = 2;
}

// 销毁密钥请求
message DestroyKeyRequest {
  string key_id = 1 [(google.api.field_behavior) = REQUIRED];
}

// 销毁密钥响应
message DestroyKeyResponse {
  bool success = 1;
}

// 获取证书请求
message GetCertificateRequest {
  string certificate_id = 1 [(google.api.field_behavior) = REQUIRED];
}

// 获取证书响应
message GetCertificateResponse {
  string certificate_id = 1;
  string certificate = 2;  // PEM 格式
  google.protobuf.Timestamp valid_from = 3;
  google.protobuf.Timestamp valid_until = 4;
}
```

### 4.6 Common Proto

```protobuf
// digitalkey/common/v1/common.proto
syntax = "proto3";

package digitalkey.common.v1;

import "google/protobuf/any.proto";
import "google/rpc/status.proto";

// 分页请求
message PaginationRequest {
  int32 page = 1;
  int32 page_size = 2;
  string sort_by = 3;
  bool descending = 4;
}

// 分页响应
message PaginationResponse {
  int32 total = 1;
  int32 page = 2;
  int32 page_size = 3;
  int32 total_pages = 4;
}

// 空请求
message Empty {}

// 空响应
message EmptyResponse {}

// 地理位置
message GeoLocation {
  double latitude = 1;
  double longitude = 2;
  string address = 3;
}

// 设备信息
message DeviceInfo {
  string device_id = 1;
  string device_type = 2;
  string device_model = 3;
  string os_version = 4;
  string app_version = 5;
  string ip_address = 6;
  string user_agent = 7;
}

// 错误信息
message Error {
  int32 code = 1;
  string message = 2;
  map<string, string> details = 3;
}

// 请求元数据
message RequestMetadata {
  string request_id = 1;
  string trace_id = 2;
  string span_id = 3;
  google.protobuf.Timestamp timestamp = 4;
  DeviceInfo device_info = 5;
}

// 事件
message Event {
  string event_id = 1;
  string event_type = 2;
  google.protobuf.Any payload = 3;
  RequestMetadata metadata = 4;
}
```

---

## 5. 消息队列 Topic 定义

### 5.1 Topic 概览

| Topic 名称 | 用途 | Partition | Replication | 保留时间 |
|-----------|------|----------|-------------|---------|
| `digitalkey.key.lifecycle` | 钥匙生命周期事件 | 12 | 3 | 7 天 |
| `digitalkey.key.share` | 钥匙分享事件 | 12 | 3 | 7 天 |
| `digitalkey.vehicle.status` | 车辆状态事件 | 12 | 3 | 3 天 |
| `digitalkey.vehicle.control` | 车辆控制指令 | 12 | 3 | 1 天 |
| `digitalkey.vehicle.ota` | OTA 升级事件 | 6 | 3 | 30 天 |
| `digitalkey.auth` | 认证事件 | 6 | 3 | 7 天 |
| `digitalkey.audit` | 审计日志 | 6 | 3 | 365 天 |
| `digitalkey.notification` | 推送通知任务 | 6 | 3 | 1 天 |
| `digitalkey.vehicle.telemetry` | 车辆遥测数据 | 24 | 3 | 3 天 |

### 5.2 钥匙生命周期事件 Topic

**Topic:** `digitalkey.key.lifecycle`

**消息格式:**

```json
{
  "eventId": "evt_abc123",
  "eventType": "key.created | key.activated | key.suspended | key.revoked | key.expired | key.updated",
  "keyId": "key_abc123",
  "vehicleId": "veh_abc123",
  "userId": "usr_abc123",
  "keyType": "PRIMARY | SECONDARY | TEMPORARY",
  "protocol": "ICCE | CCC",
  "timestamp": "2024-04-26T11:30:00Z",
  "metadata": {
    "reason": "...",
    "initiatedBy": "USER | SYSTEM | ADMIN",
    "deviceInfo": { ... }
  }
}
```

**Event Type 详细:**

| Event Type | 触发条件 | 消费者 |
|-----------|---------|-------|
| `key.created` | 钥匙创建成功 | KeySvc, VehicleSvc, NotificationSvc |
| `key.activated` | 钥匙配对激活 | KeySvc, VehicleSvc, AuditSvc |
| `key.suspended` | 钥匙被暂停 | KeySvc, VehicleSvc, NotificationSvc |
| `key.resumed` | 钥匙恢复激活 | KeySvc, VehicleSvc |
| `key.revoked` | 钥匙被吊销 | KeySvc, VehicleSvc, NotificationSvc |
| `key.expired` | 钥匙过期 | KeySvc, NotificationSvc |
| `key.updated` | 钥匙信息更新 | KeySvc, CacheSvc |
| `key.usage` | 钥匙使用 | KeySvc, AuditSvc, AnalyticsSvc |

### 5.3 车辆状态事件 Topic

**Topic:** `digitalkey.vehicle.status`

**消息格式:**

```json
{
  "eventId": "evt_abc123",
  "eventType": "vehicle.online | vehicle.offline | vehicle.bound | vehicle.unbound | vehicle.status_changed",
  "vehicleId": "veh_abc123",
  "vin": "LSVXXXXXXXXXXXXXXXX",
  "timestamp": "2024-04-26T11:30:00Z",
  "payload": {
    "online": true,
    "gps": { "latitude": 39.9042, "longitude": 116.4074 },
    "doors": { "driver": "LOCKED", ... },
    "engine": "OFF",
    "battery": { "level": 85 }
  },
  "metadata": {
    "source": "VEHICLE | CLOUD",
    "sequence": 12345
  }
}
```

### 5.4 车辆控制指令 Topic

**Topic:** `digitalkey.vehicle.control`

**消息格式:**

```json
{
  "eventId": "evt_abc123",
  "requestId": "ctrl_abc123",
  "vehicleId": "veh_abc123",
  "userId": "usr_abc123",
  "keyId": "key_abc123",
  "action": "UNLOCK | LOCK | START_ENGINE | ...",
  "parameters": { ... },
  "timestamp": "2024-04-26T11:30:00Z",
  "priority": "LOW | NORMAL | HIGH",
  "timeout": 30,
  "status": "PENDING | PROCESSING | COMPLETED | FAILED",
  "result": {
    "success": true,
    "executedAt": "2024-04-26T11:30:05Z",
    "vehicleResponse": { ... }
  }
}
```

### 5.5 认证事件 Topic

**Topic:** `digitalkey.auth`

**消息格式:**

```json
{
  "eventId": "evt_abc123",
  "eventType": "auth.success | auth.failure | auth.mfa_required | token.issued | token.revoked",
  "userId": "usr_abc123",
  "deviceInfo": { ... },
  "timestamp": "2024-04-26T11:30:00Z",
  "metadata": {
    "authMethod": "PASSWORD | SMS | BIOMETRIC | OAUTH",
    "provider": "...",
    "failureReason": "..."
  }
}
```

### 5.6 审计日志 Topic

**Topic:** `digitalkey.audit`

**消息格式:**

```json
{
  "eventId": "evt_abc123",
  "eventType": "audit.key_operation | audit.control_executed | audit.permission_changed | audit.security_event",
  "timestamp": "2024-04-26T11:30:00Z",
  "userId": "usr_abc123",
  "keyId": "key_abc123",
  "vehicleId": "veh_abc123",
  "action": "UNLOCK | LOCK | KEY_CREATE | KEY_REVOKE | ...",
  "result": "SUCCESS | FAILURE",
  "location": {
    "latitude": 39.9042,
    "longitude": 116.4074,
    "address": "..."
  },
  "deviceInfo": { ... },
  "metadata": {
    "authMethod": "UWB | NFC | BLE | CLOUD",
    "distance": 1.5,
    "processingTime": 150
  },
  "security": {
    "isSecureRanging": true,
    "antiRelayPassed": true
  }
}
```

### 5.7 推送通知 Topic

**Topic:** `digitalkey.notification`

**消息格式:**

```json
{
  "taskId": "notif_abc123",
  "userId": "usr_abc123",
  "channel": "PUSH | SMS | EMAIL",
  "template": "key_revoked | control_success | share_received | ...",
  "recipient": {
    "deviceTokens": ["token1", "token2"],
    "phone": "+8613912345678",
    "email": "user@example.com"
  },
  "payload": {
    "title": "钥匙已吊销",
    "body": "您的钥匙已被吊销",
    "data": { ... }
  },
  "priority": "HIGH | NORMAL | LOW",
  "createdAt": "2024-04-26T11:30:00Z",
  "scheduledAt": null,
  "status": "PENDING | SENT | DELIVERED | FAILED",
  "result": {
    "delivered": 1,
    "failed": 0
  }
}
```

### 5.8 Consumer Group 配置

| Consumer Group | 消费 Topic | 用途 |
|---------------|-----------|------|
| `key-service-lifecycle` | `digitalkey.key.lifecycle` | 钥匙生命周期处理 |
| `vehicle-service-status` | `digitalkey.vehicle.status` | 车辆状态同步 |
| `vehicle-service-control` | `digitalkey.vehicle.control` | 控制指令处理 |
| `notification-service` | `digitalkey.notification` | 推送发送 |
| `audit-service` | `digitalkey.audit` | 审计日志写入 |
| `analytics-service` | `digitalkey.*` | 数据分析 |
| `cache-invalidation` | `digitalkey.key.lifecycle`, `digitalkey.vehicle.status` | 缓存失效 |
| `oem-adapter-service` | `digitalkey.key.lifecycle`, `digitalkey.vehicle.control` | 车企系统同步 |

### 5.9 消息 Schema Registry

所有消息使用 **Apache Avro** 进行序列化，并注册到 **Confluent Schema Registry**。

**Schema 注册表:**

| Subject | Schema | 兼容性 |
|---------|--------|-------|
| `digitalkey-key-lifecycle-value` | KeyLifecycleEvent | `BACKWARD` |
| `digitalkey-vehicle-status-value` | VehicleStatusEvent | `BACKWARD` |
| `digitalkey-vehicle-control-value` | VehicleControlEvent | `BACKWARD` |
| `digitalkey-audit-value` | AuditEvent | `BACKWARD` |
| `digitalkey-notification-value` | NotificationTask | `BACKWARD` |

---

*文档版本：v1.0 | 最后更新：2026-04-26 | 作者：code_designer*
