# 车云一体化三端通信协议规范 (BER-TLV版)

> **文档版本**: v2.0.0  
> **协议版本**: v1.2.0  
> **编码格式**: BER-TLV (Basic Encoding Rules - Tag Length Value)  
> **适用场景**: 车载嵌入式(车端) ↔ 云端(Backend) ↔ 手机 SDK(Frontend)  
> **制定日期**: 2026-05-10  
> **TLV TAG规范**: ISO/IEC 8825-1 / ISO 7816

---

## 目录

1. [BER-TLV 基础规范](#1-ber-tlv-基础规范)
2. [协议版本管理](#2-协议版本管理)
3. [Backend-云端协议](#3-backend-云端协议)
4. [Frontend-手机SDK协议](#4-frontend-手机sdk协议)
5. [Embedded-车端协议](#5-embedded-车端协议)
6. [三端数据一致性](#6-三端数据一致性)
7. [安全与认证](#7-安全与认证)
8. [版本协商与升级](#8-版本协商与升级)
9. [附录: 完整TAG定义](#9-附录-完整tag定义)

---

## 1. BER-TLV 基础规范

### 1.1 BER-TLV 格式定义

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         BER-TLV 数据帧结构                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌──────────────┬──────────────┬──────────────────────────────┐              │
│  │     TAG      │   LENGTH     │            VALUE             │              │
│  │   (1-4B)     │   (1-4B)     │          (N Bytes)           │              │
│  └──────────────┴──────────────┴──────────────────────────────┘              │
│                                                                             │
│  TAG 格式:                                                                  │
│  ┌─────┬─────┬──────────────────────────────────────────────┐               │
│  │ b8  │ b7  │ b6-b1 或 b6-b5 + 后续字节                      │               │
│  ├─────┼─────┼──────────────────────────────────────────────┤               │
│  │ 0/1 │ 0/1 │ 类别位(2bit) + Tag编号(5bit或扩展)              │               │
│  └─────┴─────┴──────────────────────────────────────────────┘               │
│                                                                             │
│  类别 (Class):                                                              │
│    - 00 (Universal): 通用数据类型                                            │
│    - 01 (Application): 应用特定                                              │
│    - 10 (Context-Specific): 上下文特定                                       │
│    - 11 (Private): 私有定义                                                  │
│                                                                             │
│  类型 (P/C):                                                                │
│    - 0: Primitive (原始类型)                                                 │
│    - 1: Constructed (构造类型,包含子TLV)                                      │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 1.2 车云协议TAG类别定义

| 类别 | 范围 | 用途 |
|-----|------|------|
| **Universal (00)** | 0x00-0x1F | 标准数据类型 (Integer, UTF8String, Boolean等) |
| **Application (01)** | 0x40-0x5F | 应用层业务数据 (车辆信息、控制指令) |
| **Context-Specific (10)** | 0x80-0x9F | 协议控制信息 (版本、会话、安全) |
| **Private (11)** | 0xC0-0xDF | OEM私有扩展 |

### 1.3 基础数据类型TAG (Universal Class)

| Tag | 名称 | 编码 | 说明 |
|-----|------|------|------|
| 0x01 | BOOLEAN | Primitive | 布尔值 (0x00=false, 0xFF=true) |
| 0x02 | INTEGER | Primitive | 有符号整数，大端序 |
| 0x03 | BIT STRING | Primitive | 位串 |
| 0x04 | OCTET STRING | Primitive | 字节串 (二进制数据) |
| 0x05 | NULL | Primitive | 空值 |
| 0x0C | UTF8String | Primitive | UTF-8编码字符串 |
| 0x13 | PrintableString | Primitive | 可打印ASCII字符串 |
| 0x30 | SEQUENCE | Constructed | 有序TLV序列 |
| 0x31 | SET | Constructed | 无序TLV集合 |

### 1.4 协议控制TAG (Context-Specific Class)

| Tag | 名称 | 类型 | 必需 | 说明 |
|-----|------|------|------|------|
| **0x80** | **PROTOCOL_VERSION** | Primitive | ✅ | 协议版本 (如 0x01 0x02 0x00 = v1.2.0) |
| **0x81** | **MESSAGE_TYPE** | Primitive | ✅ | 消息类型 (见消息类型表) |
| **0x82** | **MESSAGE_ID** | Primitive | ✅ | 消息唯一标识 (UUID, 16字节) |
| **0x83** | **TIMESTAMP** | Primitive | ✅ | 时间戳 (Unix时间, 8字节大端) |
| **0x84** | **SESSION_ID** | Primitive | 条件 | 会话标识 (认证后必需) |
| **0x85** | **SEQUENCE_NUMBER** | Primitive | 条件 | 序列号 (可靠传输用) |
| **0x86** | **CORRELATION_ID** | Primitive | 条件 | 关联ID (请求-响应对) |
| **0x87** | **ERROR_CODE** | Primitive | 条件 | 错误码 (错误响应时必需) |
| **0x88** | **AUTH_TOKEN** | Primitive | 条件 | 认证令牌 (JWT或类似) |
| **0x89** | **SIGNATURE** | Primitive | 条件 | 消息签名 (HMAC-SHA256) |
| **0x8A** | **ENCRYPTION_INFO** | Constructed | 条件 | 加密信息 (算法+IV) |
| **0x8B** | **COMPRESSION_FLAG** | Primitive | ❌ | 压缩标志 (0x00=未压缩, 0x01=ZLIB) |
| **0x8C** | **SOURCE_ENDPOINT** | Primitive | ❌ | 源端点标识 |
| **0x8D** | **DEST_ENDPOINT** | Primitive | ❌ | 目标端点标识 |
| **0x8E** | **PRIORITY** | Primitive | ❌ | 优先级 (0-255, 0=最高) |
| **0x8F** | **TTL** | Primitive | ❌ | 生存时间 (秒) |

### 1.5 消息类型定义 (0x81 TAG的值)

| 值 | 名称 | 方向 | 说明 |
|---|------|------|------|
| 0x01 | **REQUEST** | C→S | 请求消息 |
| 0x02 | **RESPONSE** | S→C | 成功响应 |
| 0x03 | **ERROR** | S→C | 错误响应 |
| 0x04 | **NOTIFICATION** | S→C | 服务器推送通知 |
| 0x05 | **HEARTBEAT** | 双向 | 心跳保活 |
| 0x06 | **HEARTBEAT_ACK** | 双向 | 心跳确认 |
| 0x07 | **COMMAND** | C→S | 控制指令 |
| 0x08 | **COMMAND_ACK** | S→C | 指令接收确认 |
| 0x09 | **COMMAND_RESULT** | S→C | 指令执行结果 |
| 0x0A | **OTA_REQUEST** | 双向 | OTA相关请求 |
| 0x0B | **OTA_RESPONSE** | 双向 | OTA响应 |
| 0x0C | **SYNC_REQUEST** | C→S | 数据同步请求 |
| 0x0D | **SYNC_RESPONSE** | S→C | 数据同步响应 |
| 0x0E | **VERSION_NEGOTIATION** | 双向 | 版本协商 |
| 0x0F | **VERSION_NEGOTIATION_ACK** | 双向 | 版本协商确认 |

### 1.6 标准消息头格式

```
┌──────────────────────────────────────────────────────────────────────────┐
│                      标准消息头 (Standard Header)                         │
│                         长度: 可变 (最少 32 字节)                          │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │ 0x80 PROTOCOL_VERSION 0x03 [Major Minor Patch]                  │    │
│  │ 例如: [0x01 0x02 0x00] = v1.2.0                                  │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │ 0x81 MESSAGE_TYPE 0x01 [Type]                                   │    │
│  │ 例如: [0x01] = REQUEST                                           │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │ 0x82 MESSAGE_ID 0x10 [UUID-16-bytes]                            │    │
│  │ 例如: [0x12 0x34 0x56...0xEF] (128-bit UUID)                    │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │ 0x83 TIMESTAMP 0x08 [64-bit Unix timestamp, big-endian]         │    │
│  │ 例如: [0x00 0x00 0x01 0x91 0xD4 0x8A 0xE0 0x00] = 1715410200000 │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │ [可选] 0x84 SESSION_ID 0x10 [Session UUID]                      │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │ [可选] 0x86 CORRELATION_ID 0x10 [关联ID，响应对应请求]            │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │ [Payload] 业务数据 TLV SEQUENCE                                 │    │
│  │ 0x30 LENGTH { [业务TAGs...] }                                    │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 1.7 BER-TLV 编码示例

```
【示例1: 心跳消息】
JSON表示:
{
  "protocolVersion": "1.2.0",
  "messageType": "HEARTBEAT",
  "messageId": "550e8400-e29b-41d4-a716-446655440000",
  "timestamp": 1715410200000,
  "payload": {
    "deviceStatus": "online",
    "batteryLevel": 85
  }
}

BER-TLV编码 (Hex):
80 03 01 02 00                    ; PROTOCOL_VERSION v1.2.0
81 01 05                          ; MESSAGE_TYPE = HEARTBEAT
82 10 55 0E 84 00 E2 9B 41 D4 A7 16 44 66 55 44 00 00  ; MESSAGE_ID
83 08 00 00 01 91 D4 8A E0 00    ; TIMESTAMP
30 14                             ; SEQUENCE (Payload)
  ├─ A0 06                        ;   deviceStatus (Application Tag 0)
  │   0C 04 6F 6E 6C 69 6E 65     ;     UTF8String "online"
  └─ A1 02                        ;   batteryLevel (Application Tag 1)
      02 01 55                    ;     INTEGER 85

总长度: 56字节

【示例2: 车辆状态上报】
80 03 01 02 00                    ; PROTOCOL_VERSION
81 01 04                          ; MESSAGE_TYPE = NOTIFICATION
82 10 [16-byte UUID]              ; MESSAGE_ID
83 08 [8-byte timestamp]          ; TIMESTAMP
84 10 [16-byte session]           ; SESSION_ID
30 4A                             ; SEQUENCE (Payload)
  ├─ A2 11                        ;   vehicleInfo (App Tag 2)
  │   30 0F                       ;     SEQUENCE
  │     ├─ A0 05                  ;       vin
  │     │   0C 03 4C 53 56 41 47  ;         "LSVAG"
  │     └─ A1 06                  ;       vehicleModel
  │         0C 04 4D 6F 64 65 6C 58 ;       "ModelX"
  ├─ A3 25                        ;   status (App Tag 3)
  │   30 23                       ;     SEQUENCE
  │     ├─ A0 01                  ;       ignition
  │     │   01 01 FF              ;         BOOLEAN true
  │     ├─ A1 01                  ;       locked
  │     │   01 01 00              ;         BOOLEAN false
  │     ├─ A2 03                  ;       fuelLevel
  │     │   02 01 65              ;         INTEGER 101 (actually 65=101? wait)
  │     │   ; Correction: 02 01 55 = INTEGER 85
  │     └─ A3 0A                  ;       mileage
  │         02 08 00 00 00 00 00 BC 61 4E ; INTEGER 12345678
  └─ A4 0C                        ;   location (App Tag 4)
      30 0A                       ;     SEQUENCE
        ├─ A0 04                  ;       latitude
        │   02 02 1C 15 70 00     ;         INTEGER 471000000 (scale 1e7)
        └─ A1 04                  ;       longitude
            02 02 73 9C A0 00     ;         INTEGER 1936992000 (scale 1e7)
```

---

## 2. 协议版本管理

### 2.1 版本号格式 (SemVer)

```
版本号: MAJOR.MINOR.PATCH
         │      │     └─ 向后兼容的问题修复
         │      └──────── 向后兼容的功能添加
         └─────────────── 不兼容的API修改

TLV编码: 0x80 0x03 [MAJOR] [MINOR] [PATCH]

示例:
  v1.0.0   →  80 03 01 00 00
  v1.2.0   →  80 03 01 02 00
  v2.0.0   →  80 03 02 00 00
  v1.2.3   →  80 03 01 02 03
```

### 2.2 版本兼容性规则

| 版本变化 | 兼容性 | 说明 |
|---------|--------|------|
| MAJOR ↑ | ❌ 不兼容 | 必须升级客户端/服务端 |
| MINOR ↑ | ✅ 兼容 | 新功能可选使用，旧功能保持不变 |
| PATCH ↑ | ✅ 兼容 | Bug修复，完全透明 |

### 2.3 版本声明要求

**每个协议消息必须包含:**
1. `0x80 PROTOCOL_VERSION` - 消息发送方的协议版本
2. `0x81 MESSAGE_TYPE` - 消息类型
3. `0x83 TIMESTAMP` - 发送时间戳

**协商流程中的版本信息:**
```
Client (v1.2.0)                    Server (v1.3.0)
     │                                  │
     │  1. VERSION_NEGOTIATION          │
     │  ─────────────────────────────►  │
     │  80 03 01 02 00  (Client ver)    │
     │                                  │
     │  2. VERSION_NEGOTIATION_ACK      │
     │  ◄─────────────────────────────  │
     │  80 03 01 03 00  (Server ver)    │
     │  89 03 01 02 00  (Negotiated)    │
     │                                  │
     │  3. 后续消息使用 v1.2.0           │
     │                                  │
```

---

## 3. Backend-云端协议

### 3.1 应用层TAG定义 (Application Class: 0x40-0x5F)

#### 车辆相关TAG (0x40-0x4F)

| Tag | 名称 | 类型 | 说明 |
|-----|------|------|------|
| 0x40 | VEHICLE_INFO | Constructed | 车辆信息容器 |
| 0x41 | VIN | Primitive | 车辆识别号 (17字符ASCII) |
| 0x42 | VEHICLE_MODEL | Primitive | 车型 |
| 0x43 | VEHICLE_BRAND | Primitive | 品牌 |
| 0x44 | LICENSE_PLATE | Primitive | 车牌号 |
| 0x45 | VEHICLE_STATUS | Constructed | 车辆状态容器 |
| 0x46 | IGNITION | Primitive | BOOLEAN 点火状态 |
| 0x47 | LOCK_STATUS | Primitive | BOOLEAN 锁车状态 |
| 0x48 | MILEAGE | Primitive | INTEGER 总里程(米) |
| 0x49 | FUEL_LEVEL | Primitive | INTEGER 油量百分比 |
| 0x4A | BATTERY_VOLTAGE | Primitive | INTEGER 电压(mV) |
| 0x4B | ENGINE_TEMP | Primitive | INTEGER 发动机温度(℃) |

#### 位置相关TAG (0x50-0x57)

| Tag | 名称 | 类型 | 说明 |
|-----|------|------|------|
| 0x50 | LOCATION | Constructed | 位置信息容器 |
| 0x51 | LATITUDE | Primitive | INTEGER (度 × 10^7) |
| 0x52 | LONGITUDE | Primitive | INTEGER (度 × 10^7) |
| 0x53 | ALTITUDE | Primitive | INTEGER (厘米) |
| 0x54 | SPEED | Primitive | INTEGER (厘米/秒) |
| 0x55 | HEADING | Primitive | INTEGER (度) |
| 0x56 | GPS_ACCURACY | Primitive | INTEGER (厘米) |
| 0x57 | LOCATION_TIMESTAMP | Primitive | INTEGER (Unix时间戳) |

#### 设备相关TAG (0x58-0x5F)

| Tag | 名称 | 类型 | 说明 |
|-----|------|------|------|
| 0x58 | DEVICE_INFO | Constructed | 设备信息容器 |
| 0x59 | DEVICE_ID | Primitive | UUID (16字节) |
| 0x5A | DEVICE_TYPE | Primitive | ENUM (0x01=TBox, 0x02=IVI, 0x03=ADAS) |
| 0x5B | FIRMWARE_VERSION | Primitive | TLV: MAJOR MINOR PATCH |
| 0x5C | HARDWARE_VERSION | Primitive | UTF8String |
| 0x5D | DEVICE_STATUS | Primitive | ENUM (0x01=online, 0x02=offline, 0x03=sleep) |
| 0x5E | LAST_HEARTBEAT | Primitive | INTEGER (Unix时间戳) |
| 0x5F | SIGNAL_STRENGTH | Primitive | INTEGER (dBm) |

### 3.2 云端API请求格式

```
【获取车辆状态请求】
POST /api/v2/vehicles/{vin}/status

BER-TLV Body:
80 03 01 02 00                    ; PROTOCOL_VERSION v1.2.0
81 01 01                          ; MESSAGE_TYPE = REQUEST
82 10 [16-byte UUID]              ; MESSAGE_ID
83 08 [timestamp]                 ; TIMESTAMP
84 10 [session-id]                ; SESSION_ID
88 40 [JWT-token]                 ; AUTH_TOKEN (可选, Header已带时可省)
30 12                             ; SEQUENCE (Payload)
  ├─ 41 11                        ;   VIN
  │   0C 0F 4C 53 56 41 47 32 31 38 30 45 32 31 30 30 30 30 31
  │   ; "LSVAG2180E2100001"
  └─ 90 01                        ;   REQUEST_TYPE (Context-Specific)
      02                          ;     0x02 = GET_STATUS

【获取车辆状态响应 - 成功】
HTTP 200 OK

80 03 01 02 00                    ; PROTOCOL_VERSION
81 01 02                          ; MESSAGE_TYPE = RESPONSE
82 10 [response-UUID]             ; MESSAGE_ID
83 08 [timestamp]                 ; TIMESTAMP
86 10 [request-UUID]              ; CORRELATION_ID
30 4A                             ; SEQUENCE (Payload)
  ├─ 40 20                        ;   VEHICLE_INFO
  │   41 11 4C 53 56 41...        ;     VIN
  │   42 08 4D 6F 64 65 6C 20 58  ;     VEHICLE_MODEL "Model X"
  │   43 06 54 65 73 6C 61        ;     VEHICLE_BRAND "Tesla"
  └─ 45 2A                        ;   VEHICLE_STATUS
      46 01 01 01 FF              ;     IGNITION = true
      47 01 01 01 FF              ;     LOCK_STATUS = true
      48 05 02 03 0C 49 2E        ;     MILEAGE = 12345678
      49 01 02 01 55              ;     FUEL_LEVEL = 85
      4A 02 02 30 39              ;     BATTERY_VOLTAGE = 12345 (12.345V)
```

### 3.3 数据库存储的BER-TLV格式

```sql
-- 遥测数据表 (TimescaleDB hypertable)
CREATE TABLE telemetry_tlv (
    time TIMESTAMPTZ NOT NULL,
    device_id UUID NOT NULL,
    
    -- BER-TLV 原始数据存储
    tlv_data BYTEA NOT NULL,           -- 完整的TLV消息
    
    -- 索引字段 (从TLV解析)
    message_type SMALLINT,              -- 0x81的值
    protocol_version INTEGER,           -- 0x80的值
    vin VARCHAR(17),                    -- 0x41的值
    latitude BIGINT,                    -- 0x51的值
    longitude BIGINT,                   -- 0x52的值
    speed INTEGER,                      -- 0x54的值
    
    -- 分区键
    PRIMARY KEY (time, device_id)
);

-- 创建 hypertable
SELECT create_hypertable('telemetry_tlv', 'time', chunk_time_interval => INTERVAL '1 day');

-- 创建解码函数
CREATE OR REPLACE FUNCTION decode_tlv_vin(tlv_data BYTEA)
RETURNS VARCHAR AS $$
DECLARE
    vin_pos INTEGER;
BEGIN
    -- 在TLV数据中搜索 VIN tag (0x41)
    vin_pos := position('\x41' in tlv_data);
    IF vin_pos > 0 THEN
        -- 解析长度并提取值
        RETURN convert_from(
            substring(tlv_data from vin_pos + 2 for get_tlv_length(tlv_data, vin_pos)),
            'UTF8'
        );
    END IF;
    RETURN NULL;
END;
$$ LANGUAGE plpgsql IMMUTABLE;
```

---

## 4. Frontend-手机SDK协议

### 4.1 手机SDK专用TAG (Private Class: 0xC0-0xCF)

| Tag | 名称 | 类型 | 说明 |
|-----|------|------|------|
| 0xC0 | PUSH_TOKEN | Primitive | FCM/APNs 推送令牌 |
| 0xC1 | OS_TYPE | Primitive | ENUM (0x01=iOS, 0x02=Android) |
| 0xC2 | OS_VERSION | Primitive | UTF8String |
| 0xC3 | APP_VERSION | Primitive | TLV: MAJOR MINOR PATCH |
| 0xC4 | DEVICE_MODEL | Primitive | UTF8String |
| 0xC5 | SCREEN_SIZE | Constructed | SEQUENCE(宽,高) |
| 0xC6 | BUNDLE_ID | Primitive | UTF8String (包名) |

### 4.2 WebSocket连接建立

```
【WebSocket握手请求】
GET /ws/v2/vehicles
Upgrade: websocket
Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
X-Protocol-Version: 1.2.0

【WebSocket握手响应】
HTTP 101 Switching Protocols
Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
X-Server-Version: 1.3.0
X-Supported-Versions: 1.2,1.3

【WebSocket首消息 - 客户端注册】(BER-TLV over binary frame)
80 03 01 02 00                    ; PROTOCOL_VERSION v1.2.0
81 01 0E                          ; MESSAGE_TYPE = VERSION_NEGOTIATION
82 10 [UUID]                      ; MESSAGE_ID
83 08 [timestamp]                 ; TIMESTAMP
84 10 [session-id]                ; SESSION_ID
30 2A                             ; SEQUENCE (Device Info)
  ├─ C1 01 02                     ;   OS_TYPE = Android
  ├─ C2 04 31 34 2E 30            ;   OS_VERSION "14.0"
  ├─ C3 03 02 01 05               ;   APP_VERSION v2.1.5
  ├─ C4 0A 53 4D 2D 47 39 39 38 55 ;  DEVICE_MODEL "SM-G998U"
  └─ C0 8C [...]                  ;   PUSH_TOKEN (FCM token)

【WebSocket首消息 - 服务器响应】
80 03 01 03 00                    ; PROTOCOL_VERSION v1.3.0 (Server)
81 01 0F                          ; MESSAGE_TYPE = VERSION_NEGOTIATION_ACK
82 10 [UUID]                      ; MESSAGE_ID
83 08 [timestamp]                 ; TIMESTAMP
86 10 [correlation-id]            ; CORRELATION_ID
89 03 01 02 00                    ; NEGOTIATED_VERSION = v1.2.0
30 08                             ; SEQUENCE
  ├─ 91 01 00                     ;   NEGOTIATION_RESULT = 0x00 (Success)
  └─ 92 03 01 03 00               ;   EFFECTIVE_VERSION = v1.3.0 (Server降级)
```

---

## 5. Embedded-车端协议

### 5.1 MQTT over BER-TLV

```
【MQTT连接包 - CONNECT】(将BER-TLV作为Payload)
固定头: 0x10 (CONNECT)
可变头: Protocol Name "MQTT"
       Protocol Level 5
       Connect Flags
       Keep Alive 30
       
Payload (BER-TLV格式):
80 03 01 02 00                    ; PROTOCOL_VERSION v1.2.0
41 11 4C 53 56 41...              ; VIN (Client Identifier)
5B 03 02 05 02                    ; FIRMWARE_VERSION v2.5.2
5C 03 01 2E 30                    ; HARDWARE_VERSION "1.0"
8A 20 [...]                       ; AUTH_TOKEN (TLS Client Cert指纹)
30 10                             ; Will Message (BER-TLV)
  81 01 03                        ;   MESSAGE_TYPE = ERROR (离线)
  40 0A                           ;   VEHICLE_INFO
    46 01 01 01 00                ;     IGNITION = false

【CONNACK - 带版本协商】
固定头: 0x20 (CONNACK)
原因码: 0x00 (Success)

User Properties (MQTT v5):
  protocol_version: "1.2.0"
  minimum_version: "1.1.0"
  server_time: "2026-05-10T10:00:00Z"
  
响应Payload (BER-TLV):
80 03 01 02 00                    ; PROTOCOL_VERSION
89 03 01 02 00                    ; NEGOTIATED_VERSION
30 06                             ; SEQUENCE
  91 01 00                        ;   RESULT = Success
  92 01 00                        ;   SESSION_FLAGS
```

### 5.2 遥测数据上报 (MQTT Publish)

```
Topic: vehicle/{vin}/telemetry/status
QoS: 1

Payload (BER-TLV, 压缩后):
8B 01 01                          ; COMPRESSION_FLAG = ZLIB
30 [压缩后的TLV]                  ; 原始TLV压缩

解压后:
80 03 01 02 00                    ; PROTOCOL_VERSION
81 01 04                          ; MESSAGE_TYPE = NOTIFICATION
82 10 [UUID]                      ; MESSAGE_ID
83 08 [timestamp]                 ; TIMESTAMP
84 10 [session]                   ; SESSION_ID
30 38                             ; SEQUENCE (Payload)
  ├─ 45 2A                        ;   VEHICLE_STATUS
  │   46 01 01 01 FF              ;     IGNITION = true
  │   47 01 01 01 FF              ;     LOCKED = true
  │   48 05 02 03 0C 49 2E        ;     MILEAGE = 12345678
  │   49 01 02 01 55              ;     FUEL_LEVEL = 85
  │   4A 02 02 30 39              ;     BATTERY_VOLTAGE = 12345
  │   4B 01 02 01 55              ;     ENGINE_TEMP = 85
  └─ 50 12                        ;   LOCATION
      51 04 01 2A 05 F0           ;     LATITUDE = 31.2304°
      52 04 07 3A D7 01           ;     LONGITUDE = 121.4737°
      54 02 00 0F A0              ;     SPEED = 4000 (40.00 m/s = 144km/h)
      55 02 00 5A                 ;     HEADING = 90°

【大小对比】
JSON格式: ~450 字节
TLV格式:   ~120 字节 (无压缩)
TLV+ZLIB:  ~65 字节  (节省85%)
```

### 5.3 车端离线缓存格式

```c
// 车端Flash存储结构
typedef struct {
    uint32_t magic;              // 0x42544C56 "BTLV"
    uint16_t version;            // 存储格式版本
    uint32_t sequence_number;    // 全局序列号
    uint32_t timestamp;          // 写入时间
    uint16_t tlv_length;         // TLV数据长度
    uint8_t  tlv_data[2040];     // BER-TLV数据
    uint32_t crc32;              // CRC校验
} CachedMessage;

// 离线队列文件
#define CACHE_MAGIC 0x42544C56   // "BTLV"
#define CACHE_VERSION 0x0102     // v1.2

// 批量上传时打包多个TLV
30 [总长度]                       ; SEQUENCE
  30 [长度1]                      ;   CachedMessage 1
    [TLV数据1]
  30 [长度2]                      ;   CachedMessage 2
    [TLV数据2]
  ...
```

---

## 6. 三端数据一致性

### 6.1 同步协议TAG (Context-Specific: 0x90-0x9F)

| Tag | 名称 | 类型 | 说明 |
|-----|------|------|------|
| 0x90 | REQUEST_TYPE | Primitive | 请求类型 (GET/PUT/POST/DELETE) |
| 0x91 | RESULT_CODE | Primitive | 结果码 (0=Success) |
| 0x92 | SESSION_FLAGS | Primitive | 会话标志 |
| 0x93 | SYNC_VERSION | Primitive | INTEGER 数据版本号 |
| 0x94 | SYNC_TIMESTAMP | Primitive | INTEGER 最后同步时间 |
| 0x95 | CONFLICT_STATUS | Primitive | ENUM (0=无冲突, 1=服务器胜, 2=客户端胜) |
| 0x96 | SERVER_VERSION | Primitive | INTEGER 服务器数据版本 |
| 0x97 | CLIENT_VERSION | Primitive | INTEGER 客户端数据版本 |
| 0x98 | CHANGE_SET | Constructed | 变更集容器 |
| 0x99 | CHANGE_OPERATION | Primitive | ENUM (0=CREATE, 1=UPDATE, 2=DELETE) |
| 0x9A | CHANGE_ENTITY | Primitive | ENUM (0=Vehicle, 1=Device, 2=User) |
| 0x9B | CHANGE_DATA | Constructed | 变更数据TLV |

### 6.2 增量同步请求/响应

```
【同步请求】
80 03 01 02 00                    ; PROTOCOL_VERSION
81 01 0C                          ; MESSAGE_TYPE = SYNC_REQUEST
82 10 [UUID]                      ; MESSAGE_ID
83 08 [timestamp]                 ; TIMESTAMP
84 10 [session]                   ; SESSION_ID
30 0C                             ; SEQUENCE
  ├─ 94 08 [timestamp]            ;   SYNC_TIMESTAMP (上次同步时间)
  └─ 90 01 01                     ;   REQUEST_TYPE = 0x01 (增量)

【同步响应】
80 03 01 02 00                    ; PROTOCOL_VERSION
81 01 0D                          ; MESSAGE_TYPE = SYNC_RESPONSE
82 10 [UUID]                      ; MESSAGE_ID
83 08 [timestamp]                 ; TIMESTAMP
86 10 [correlation]               ; CORRELATION_ID
30 8A                             ; SEQUENCE
  ├─ 91 01 00                     ;   RESULT_CODE = 0 (Success)
  ├─ 93 04 00 00 00 0A            ;   SYNC_VERSION = 10 (新版本号)
  └─ 98 80                        ;   CHANGE_SET
      30 25                       ;     SEQUENCE (Change 1)
        99 01 01                  ;       CHANGE_OPERATION = UPDATE
        9A 01 00                  ;       CHANGE_ENTITY = Vehicle
        93 04 00 00 00 09         ;       VERSION = 9
        9B 18                     ;       CHANGE_DATA
          45 16                   ;         VEHICLE_STATUS
            [状态数据]
      30 20                       ;     SEQUENCE (Change 2)
        99 01 02                  ;       CHANGE_OPERATION = DELETE
        9A 01 01                  ;       CHANGE_ENTITY = Device
        9B 15                     ;       CHANGE_DATA
          59 13 [device-uuid]     ;         DEVICE_ID
```

---

## 7. 安全与认证

### 7.1 安全相关TAG (Private: 0xD0-0xDF)

| Tag | 名称 | 类型 | 说明 |
|-----|------|------|------|
| 0xD0 | SIGNATURE_ALGORITHM | Primitive | ENUM (0x01=HMAC-SHA256) |
| 0xD1 | SIGNATURE_VALUE | Primitive | OCTET STRING |
| 0xD2 | ENCRYPTION_ALGORITHM | Primitive | ENUM (0x01=AES-256-GCM) |
| 0xD3 | ENCRYPTION_KEY_ID | Primitive | UTF8String |
| 0xD4 | ENCRYPTION_IV | Primitive | OCTET STRING (12 bytes for GCM) |
| 0xD5 | ENCRYPTION_TAG | Primitive | OCTET STRING (16 bytes auth tag) |
| 0xD6 | CERTIFICATE_CHAIN | Constructed | SEQUENCE of certificates |
| 0xD7 | CHALLENGE | Primitive | OCTET STRING (随机数) |
| 0xD8 | CHALLENGE_RESPONSE | Primitive | OCTET STRING (签名后的挑战) |

### 7.2 签名计算

```
签名范围: 从第一个TLV到签名TLV之前所有数据
算法: HMAC-SHA256
密钥: 设备私钥或会话密钥

签名前数据示例:
80 03 01 02 00                    ; PROTOCOL_VERSION
81 01 01                          ; MESSAGE_TYPE
82 10 [UUID]                      ; MESSAGE_ID
83 08 [timestamp]                 ; TIMESTAMP
30 12                             ; SEQUENCE (Payload)
  [Payload data...]

签名计算:
Signature = HMAC-SHA256(Key, Data-to-sign)

签名后消息追加:
89 20 [32-byte signature]         ; SIGNATURE
或分开传输:
D0 01 01                          ; SIGNATURE_ALGORITHM = HMAC-SHA256
D1 20 [32-byte signature]         ; SIGNATURE_VALUE
```

### 7.3 加密封装

```
【加密TLV消息结构】
80 03 01 02 00                    ; PROTOCOL_VERSION (明文)
81 01 01                          ; MESSAGE_TYPE (明文)
82 10 [UUID]                      ; MESSAGE_ID (明文)
8A 3A                             ; ENCRYPTION_INFO (明文容器)
  D2 01 01                        ;   ENCRYPTION_ALGORITHM = AES-256-GCM
  D3 08 [key-id]                  ;   ENCRYPTION_KEY_ID
  D4 0C [12-byte IV]              ;   ENCRYPTION_IV
04 [加密数据长度]                 ; OCTET STRING (加密内容)
  [AES-256-GCM密文]
D5 10 [16-byte tag]               ; ENCRYPTION_TAG (认证标签)
```

---

## 8. 版本协商与升级

### 8.1 版本协商TAG (Private: 0xE0-0xEF)

| Tag | 名称 | 类型 | 说明 |
|-----|------|------|------|
| 0xE0 | CLIENT_VERSION | Primitive | TLV: MAJOR MINOR PATCH |
| 0xE1 | SERVER_VERSION | Primitive | TLV: MAJOR MINOR PATCH |
| 0xE2 | MINIMUM_VERSION | Primitive | TLV: MAJOR MINOR PATCH |
| 0xE3 | NEGOTIATED_VERSION | Primitive | TLV: MAJOR MINOR PATCH |
| 0xE4 | SUPPORTED_VERSIONS | Constructed | SET of version TLVs |
| 0xE5 | DEPRECATED_VERSIONS | Constructed | SET of version TLVs |
| 0xE6 | SUNSET_DATE | Primitive | GeneralizedTime |
| 0xE7 | UPGRADE_URL | Primitive | UTF8String |
| 0xE8 | FORCE_UPGRADE | Primitive | BOOLEAN |
| 0xE9 | NEGOTIATION_RESULT | Primitive | ENUM (0=Success, 1=Downgrade, 2=Reject) |
| 0xEA | GRACE_PERIOD | Primitive | INTEGER (小时) |

### 8.2 版本协商流程

```
【版本协商请求】
80 03 01 02 00                    ; PROTOCOL_VERSION v1.2.0 (Client)
81 01 0E                          ; MESSAGE_TYPE = VERSION_NEGOTIATION
82 10 [UUID]                      ; MESSAGE_ID
83 08 [timestamp]                 ; TIMESTAMP
E0 03 01 02 00                    ; CLIENT_VERSION v1.2.0
E4 0C                             ; SUPPORTED_VERSIONS
  03 01 02 00                     ;   v1.2.0
  03 01 01 05                     ;   v1.1.5
  03 01 00 03                     ;   v1.0.3

【版本协商响应 - 成功】
80 03 01 03 00                    ; PROTOCOL_VERSION v1.3.0 (Server)
81 01 0F                          ; MESSAGE_TYPE = VERSION_NEGOTIATION_ACK
82 10 [UUID]                      ; MESSAGE_ID
83 08 [timestamp]                 ; TIMESTAMP
86 10 [correlation]               ; CORRELATION_ID
E1 03 01 03 00                    ; SERVER_VERSION v1.3.0
E3 03 01 02 00                    ; NEGOTIATED_VERSION v1.2.0 (Client)
E9 01 00                          ; NEGOTIATION_RESULT = Success (0x00)
EA 02 01 68                       ; GRACE_PERIOD = 360 hours (15天)
E6 11                             ; SUNSET_DATE
  18 0F 32 30 32 36 31 31 30 31   ;   "20261101" (v1.2.0退役日期)

【版本协商响应 - 拒绝 (版本过低)】
80 03 01 03 00                    ; PROTOCOL_VERSION v1.3.0
81 01 0F                          ; MESSAGE_TYPE = VERSION_NEGOTIATION_ACK
82 10 [UUID]                      ; MESSAGE_ID
86 10 [correlation]               ; CORRELATION_ID
E1 03 01 03 00                    ; SERVER_VERSION v1.3.0
E2 03 01 02 00                    ; MINIMUM_VERSION v1.2.0
E9 01 02                          ; NEGOTIATION_RESULT = Reject (0x02)
87 02 20 01                       ; ERROR_CODE = VERSION_NOT_SUPPORTED
E8 01 FF                          ; FORCE_UPGRADE = true
E7 20 68 74 74 70 73 3A 2F 2F...  ; UPGRADE_URL
```

### 8.3 OTA升级TLV格式

```
【OTA升级通知】
80 03 01 02 00                    ; PROTOCOL_VERSION
81 01 0A                          ; MESSAGE_TYPE = OTA_REQUEST
82 10 [UUID]                      ; MESSAGE_ID
83 08 [timestamp]                 ; TIMESTAMP
30 4A                             ; SEQUENCE
  ├─ 41 11 [vin]                  ;   VIN
  ├─ 5B 03 02 05 03               ;   TARGET_VERSION v2.5.3
  ├─ 04 04 00 08 00 00            ;   PACKAGE_SIZE = 524288 (512KB)
  ├─ 04 20 [sha256-hash]          ;   PACKAGE_HASH
  ├─ 0C 30 [url]                  ;   PACKAGE_URL
  ├─ 01 01 FF                     ;   FORCE_UPGRADE = true
  └─ 02 02 00 3C                  ;   TIMEOUT = 60 minutes

【OTA进度上报】
80 03 01 02 00                    ; PROTOCOL_VERSION
81 01 0B                          ; MESSAGE_TYPE = OTA_RESPONSE
82 10 [UUID]                      ; MESSAGE_ID
83 08 [timestamp]                 ; TIMESTAMP
86 10 [correlation]               ; CORRELATION_ID
30 1A                             ; SEQUENCE
  ├─ 91 01 00                     ;   RESULT_CODE = InProgress
  ├─ 02 01 50                     ;   PROGRESS = 80%
  ├─ 99 01 01                     ;   CHANGE_OPERATION = UPDATE (stage)
  └─ 9B 08                        ;   CHANGE_DATA
      0C 06 64 6F 77 6E 6C 6F 61 64  ;   "download"
```

---

## 9. 附录: 完整TAG定义

### 9.1 TAG范围总览

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          BER-TLV TAG 范围图                               │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  0x00 ┌─────────────────────────────────────────────────────────────┐   │
│       │ Universal Class (标准数据类型)                               │   │
│  0x1F │ 0x01=BOOLEAN 0x02=INTEGER 0x04=OCTET 0x0C=UTF8 0x30=SEQUENCE │   │
│       └─────────────────────────────────────────────────────────────┘   │
│  0x20 ┌─────────────────────────────────────────────────────────────┐   │
│       │ Universal Class (更多标准类型)                               │   │
│  0x3F │ 0x31=SET                                                     │   │
│       └─────────────────────────────────────────────────────────────┘   │
│  0x40 ┌─────────────────────────────────────────────────────────────┐   │
│       │ Application Class (应用层业务数据)                           │   │
│  0x5F │ 0x40-0x4F=车辆 0x50-0x57=位置 0x58-0x5F=设备                 │   │
│       └─────────────────────────────────────────────────────────────┘   │
│  0x60 ┌─────────────────────────────────────────────────────────────┐   │
│       │ (保留)                                                       │   │
│  0x7F │                                                              │   │
│       └─────────────────────────────────────────────────────────────┘   │
│  0x80 ┌─────────────────────────────────────────────────────────────┐   │
│       │ Context-Specific Class (协议控制信息)                        │   │
│  0x9F │ 0x80-0x8F=消息头 0x90-0x9F=同步                              │   │
│       └─────────────────────────────────────────────────────────────┘   │
│  0xA0 ┌─────────────────────────────────────────────────────────────┐   │
│       │ Context-Specific Class (应用上下文)                          │   │
│  0xBF │ 0xA0-0xAF=车辆信息容器 0xB0-0xBF=扩展                        │   │
│       └─────────────────────────────────────────────────────────────┘   │
│  0xC0 ┌─────────────────────────────────────────────────────────────┐   │
│       │ Private Class - OEM Mobile SDK                               │   │
│  0xCF │ 0xC0-0xCF=手机端专用                                         │   │
│       └─────────────────────────────────────────────────────────────┘   │
│  0xD0 ┌─────────────────────────────────────────────────────────────┐   │
│       │ Private Class - Security                                     │   │
│  0xDF │ 0xD0-0xDF=签名/加密                                          │   │
│       └─────────────────────────────────────────────────────────────┘   │
│  0xE0 ┌─────────────────────────────────────────────────────────────┐   │
│       │ Private Class - Version Management                           │   │
│  0xEF │ 0xE0-0xEF=版本协商                                           │   │
│       └─────────────────────────────────────────────────────────────┘   │
│  0xF0 ┌─────────────────────────────────────────────────────────────┐   │
│       │ (保留/未来扩展)                                              │   │
│  0xFF │                                                              │   │
│       └─────────────────────────────────────────────────────────────┘   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 9.2 完整TAG表

| Tag | Class | Name | Type | Description |
|-----|-------|------|------|-------------|
| 0x01 | U | BOOLEAN | P | 布尔值 |
| 0x02 | U | INTEGER | P | 有符号整数 |
| 0x04 | U | OCTET STRING | P | 字节串 |
| 0x05 | U | NULL | P | 空值 |
| 0x0C | U | UTF8String | P | UTF-8字符串 |
| 0x30 | U | SEQUENCE | C | 有序序列 |
| 0x31 | U | SET | C | 无序集合 |
| 0x40 | A | VEHICLE_INFO | C | 车辆信息 |
| 0x41 | A | VIN | P | VIN码 |
| 0x42 | A | VEHICLE_MODEL | P | 车型 |
| 0x43 | A | VEHICLE_BRAND | P | 品牌 |
| 0x44 | A | LICENSE_PLATE | P | 车牌 |
| 0x45 | A | VEHICLE_STATUS | C | 车辆状态 |
| 0x46 | A | IGNITION | P | 点火状态 |
| 0x47 | A | LOCK_STATUS | P | 锁车状态 |
| 0x48 | A | MILEAGE | P | 里程 |
| 0x49 | A | FUEL_LEVEL | P | 油量 |
| 0x4A | A | BATTERY_VOLTAGE | P | 电压 |
| 0x4B | A | ENGINE_TEMP | P | 发动机温度 |
| 0x50 | A | LOCATION | C | 位置信息 |
| 0x51 | A | LATITUDE | P | 纬度 |
| 0x52 | A | LONGITUDE | P | 经度 |
| 0x53 | A | ALTITUDE | P | 海拔 |
| 0x54 | A | SPEED | P | 速度 |
| 0x55 | A | HEADING | P | 方向 |
| 0x56 | A | GPS_ACCURACY | P | GPS精度 |
| 0x57 | A | LOCATION_TIMESTAMP | P | 位置时间戳 |
| 0x58 | A | DEVICE_INFO | C | 设备信息 |
| 0x59 | A | DEVICE_ID | P | 设备ID |
| 0x5A | A | DEVICE_TYPE | P | 设备类型 |
| 0x5B | A | FIRMWARE_VERSION | P | 固件版本 |
| 0x5C | A | HARDWARE_VERSION | P | 硬件版本 |
| 0x5D | A | DEVICE_STATUS | P | 设备状态 |
| 0x5E | A | LAST_HEARTBEAT | P | 最后心跳 |
| 0x5F | A | SIGNAL_STRENGTH | P | 信号强度 |
| 0x80 | C | PROTOCOL_VERSION | P | 协议版本 **必需** |
| 0x81 | C | MESSAGE_TYPE | P | 消息类型 **必需** |
| 0x82 | C | MESSAGE_ID | P | 消息ID **必需** |
| 0x83 | C | TIMESTAMP | P | 时间戳 **必需** |
| 0x84 | C | SESSION_ID | P | 会话ID |
| 0x85 | C | SEQUENCE_NUMBER | P | 序列号 |
| 0x86 | C | CORRELATION_ID | P | 关联ID |
| 0x87 | C | ERROR_CODE | P | 错误码 |
| 0x88 | C | AUTH_TOKEN | P | 认证令牌 |
| 0x89 | C | SIGNATURE | P | 签名 |
| 0x8A | C | ENCRYPTION_INFO | C | 加密信息 |
| 0x8B | C | COMPRESSION_FLAG | P | 压缩标志 |
| 0x8C | C | SOURCE_ENDPOINT | P | 源端点 |
| 0x8D | C | DEST_ENDPOINT | P | 目标端点 |
| 0x8E | C | PRIORITY | P | 优先级 |
| 0x8F | C | TTL | P | 生存时间 |
| 0x90 | C | REQUEST_TYPE | P | 请求类型 |
| 0x91 | C | RESULT_CODE | P | 结果码 |
| 0x92 | C | SESSION_FLAGS | P | 会话标志 |
| 0x93 | C | SYNC_VERSION | P | 同步版本 |
| 0x94 | C | SYNC_TIMESTAMP | P | 同步时间戳 |
| 0x95 | C | CONFLICT_STATUS | P | 冲突状态 |
| 0x96 | C | SERVER_VERSION | P | 服务器数据版本 |
| 0x97 | C | CLIENT_VERSION | P | 客户端数据版本 |
| 0x98 | C | CHANGE_SET | C | 变更集 |
| 0x99 | C | CHANGE_OPERATION | P | 变更操作 |
| 0x9A | C | CHANGE_ENTITY | P | 变更实体 |
| 0x9B | C | CHANGE_DATA | C | 变更数据 |
| 0xC0 | P | PUSH_TOKEN | P | 推送令牌 |
| 0xC1 | P | OS_TYPE | P | 操作系统类型 |
| 0xC2 | P | OS_VERSION | P | OS版本 |
| 0xC3 | P | APP_VERSION | P | App版本 |
| 0xC4 | P | DEVICE_MODEL | P | 设备型号 |
| 0xC5 | P | SCREEN_SIZE | C | 屏幕尺寸 |
| 0xC6 | P | BUNDLE_ID | P | 包名 |
| 0xD0 | P | SIGNATURE_ALGORITHM | P | 签名算法 |
| 0xD1 | P | SIGNATURE_VALUE | P | 签名值 |
| 0xD2 | P | ENCRYPTION_ALGORITHM | P | 加密算法 |
| 0xD3 | P | ENCRYPTION_KEY_ID | P | 密钥ID |
| 0xD4 | P | ENCRYPTION_IV | P | 加密IV |
| 0xD5 | P | ENCRYPTION_TAG | P | 认证标签 |
| 0xD6 | P | CERTIFICATE_CHAIN | C | 证书链 |
| 0xD7 | P | CHALLENGE | P | 挑战值 |
| 0xD8 | P | CHALLENGE_RESPONSE | P | 挑战响应 |
| 0xE0 | P | CLIENT_VERSION | P | 客户端版本 |
| 0xE1 | P | SERVER_VERSION | P | 服务器版本 |
| 0xE2 | P | MINIMUM_VERSION | P | 最低版本 |
| 0xE3 | P | NEGOTIATED_VERSION | P | 协商版本 |
| 0xE4 | P | SUPPORTED_VERSIONS | C | 支持版本列表 |
| 0xE5 | P | DEPRECATED_VERSIONS | C | 废弃版本列表 |
| 0xE6 | P | SUNSET_DATE | P | 退役日期 |
| 0xE7 | P | UPGRADE_URL | P | 升级URL |
| 0xE8 | P | FORCE_UPGRADE | P | 强制升级 |
| 0xE9 | P | NEGOTIATION_RESULT | P | 协商结果 |
| 0xEA | P | GRACE_PERIOD | P | 宽限期 |

**图例:**
- **U** = Universal Class
- **A** = Application Class  
- **C** = Context-Specific Class
- **P** = Private Class
- **P** = Primitive
- **C** = Constructed

---

**文档维护**: 车云架构组  
**最后更新**: 2026-05-10  
**文档版本**: v2.0.0 (BER-TLV Edition)
