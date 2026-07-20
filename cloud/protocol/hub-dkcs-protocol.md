# HUB ↔ DKCS 私有协议规范 (BERTLV)

**版本**: 1.0.0
**日期**: 2026-05-06
**传输**: gRPC + BERTLV
**安全**: mTLS双向认证 + 数据签名

---

## 1. 协议概述

### 1.1 消息结构

```
┌─────────────────────────────────────────────────────┐
│                    Message Envelope                   │
├──────────────┬──────────────────────────────────────┤
│   Header     │                Body                  │
│  (BERTLV)    │              (BERTLV)                 │
├──────────────┴──────────────────────────────────────┤
│                    Trailer                           │
│                  (BERTLV - 签名)                    │
└─────────────────────────────────────────────────────┘
```

### 1.2 BERTLV 编码规则

| 字段 | 长度 | 说明 |
|------|------|------|
| Tag | 1-2字节 | 标签号，见数据字典 |
| Length | 1-3字节 | 值长度，TLV编码 |
| Value | 变长 | 实际数据 |

**Length编码**:
- `00-7F`: 1字节表示长度
- `80-FF`: 首字节表示后续字节数，后续字节表示长度

---

## 2. 消息头部 (E1 01 - Header)

### 2.1 完整头部结构

| 标签 | 名称 | 格式 | 必填 | 说明 |
|------|------|------|------|------|
| E1 01 | Version | BCD | 是 | 版本号，如"0100"=v1.0 |
| E1 02 | Timestamp | N14 | 是 | YYYYMMDDhhmmss |
| E1 03 | MessageType | N4 | 是 | 消息类型 |
| E1 04 | SequenceNo | N8 | 是 | 序列号 |
| E1 05 | DeviceId | AN16 | 是 | 设备ID |
| E1 06 | SessionId | AN32 | 否 | 会话ID |
| E1 07 | Priority | N1 | 否 | 1=低 2=中 3=高 4=紧急 |
| E1 08 | Flags | B1 | 否 | 标志位 |
| E1 09 | CorrelationId | AN32 | 否 | 关联消息ID |

### 2.2 消息类型定义 (E1 03)

| 类型码 | 名称 | 方向 | 说明 |
|--------|------|------|------|
| 1000 | KeyBind | HUB→DKCS | 密钥绑定请求 |
| 1001 | KeyBindResp | DKCS→HUB | 密钥绑定响应 |
| 1002 | KeyUnbind | HUB→DKCS | 密钥解绑请求 |
| 1003 | KeyUnbindResp | DKCS→HUB | 密钥解绑响应 |
| 1004 | KeyRevoke | HUB→DKCS | 密钥撤销 |
| 1005 | KeyRevokeResp | DKCS→HUB | 密钥撤销响应 |
| 1010 | KeyList | HUB→DKCS | 密钥列表查询 |
| 1011 | KeyListResp | DKCS→HUB | 密钥列表响应 |
| 2000 | KeyShareCreate | HUB→DKCS | 分享创建 |
| 2001 | KeyShareCreateResp | DKCS→HUB | 分享创建响应 |
| 2002 | KeyShareAccept | HUB→DKCS | 分享接受 |
| 2003 | KeyShareAcceptResp | DKCS→HUB | 分享接受响应 |
| 3000 | VehicleCommand | HUB→DKCS | 车辆控制指令 |
| 3001 | VehicleCommandResp | DKCS→HUB | 车辆控制响应 |
| 3002 | VehicleStatusReport | DKCS→HUB | 车辆状态上报 |
| 4000 | VendorCallback | HUB→DKCS | 厂商回调 |
| 4001 | VendorCallbackResp | DKCS→HUB | 厂商回调响应 |
| 9000 | Heartbeat | 双向 | 心跳 |
| 9001 | HeartbeatResp | 双向 | 心跳响应 |
| 9100 | Error | 双向 | 错误响应 |

---

## 3. 消息体定义

### 3.1 密钥绑定 (KeyBind) - 1000/1001

**请求 Body (1000)**:

| 标签 | 名称 | 格式 | 说明 |
|------|------|------|------|
| A0 01 | VehicleId | AN32 | 车辆ID |
| A0 02 | Vin | AN17 | VIN码 |
| A0 03 | DeviceId | AN32 | 设备ID |
| A0 04 | UserId | AN32 | 用户ID |
| A0 05 | Vendor | AN16 | 手机厂商 |
| A0 06 | Protocol | AN16 | 协议类型 |
| A0 07 | KeyType | N2 | 密钥类型 |
| A0 08 | AccessLevel | B4 | 权限位掩码 |
| A0 09 | DevicePubkey | B64 | 设备公钥 |
| A0 0A | DeviceCert | B512 | 设备证书链 |
| A0 0B | ValidFrom | N14 | 生效时间 |
| A0 0C | ValidUntil | N14 | 失效时间 |
| A0 0D | MaxUses | N6 | 最大使用次数 |
| A0 10 | DistanceLimit | N8 | 距离限制(mm) |

**响应 Body (1001)**:

| 标签 | 名称 | 格式 | 说明 |
|------|------|------|------|
| A0 01 | KeyId | AN32 | 密钥ID |
| A0 02 | Status | N2 | 状态 |
| A0 03 | VehiclePubkey | B64 | 车端公钥 |
| A0 04 | SharedSecret | B32 | 共享密钥 |
| A0 05 | VehicleCert | B512 | 车端证书 |
| A0 06 | ResultCode | N4 | 结果码 |

**AccessLevel位掩码定义**:

| 位 | 功能 |
|----|------|
| 0 | 解锁 (Unlock) |
| 1 | 闭锁 (Lock) |
| 2 | 启动引擎 (EngineStart) |
| 3 | 熄火 (EngineStop) |
| 4 | 开启后备箱 (TrunkOpen) |
| 5 | 车窗控制 (Window) |
| 6 | 空调控制 (Climate) |
| 7 | 寻车 (FindVehicle) |
| 8 | 座椅控制 (SeatAdjust) |
| 9 | 远程启动 (RemoteStart) |
| 10-15 | 保留 |

**KeyType定义**:

| 值 | 名称 | 说明 |
|----|------|------|
| 01 | Owner | 车主密钥 |
| 02 | Friend | 朋友密钥 |
| 03 | Service | 服务密钥 |
| 04 | Temporary | 临时密钥 |

### 3.2 密钥解绑 (KeyUnbind) - 1002/1003

**请求 Body**:

| 标签 | 名称 | 格式 |
|------|------|------|
| A0 01 | KeyId | AN32 |
| A0 02 | Reason | AN64 |

**响应 Body**:

| 标签 | 名称 | 格式 |
|------|------|------|
| A0 01 | Status | N2 |
| A0 02 | UnboundAt | N14 |

### 3.3 密钥撤销 (KeyRevoke) - 1004/1005

**请求 Body**:

| 标签 | 名称 | 格式 |
|------|------|------|
| A0 01 | KeyId | AN32 |
| A0 02 | Reason | AN64 |
| A0 03 | Emergency | N1 | 1=紧急撤销

### 3.4 密钥列表 (KeyList) - 1010/1011

**请求 Body**:

| 标签 | 名称 | 格式 |
|------|------|------|
| A0 01 | VehicleId | AN32 |
| A0 02 | UserId | AN32 |
| A0 03 | Status | AN16 |
| A0 04 | Page | N4 |
| A0 05 | PageSize | N4 |

**响应 Body**:

| 标签 | 名称 | 格式 |
|------|------|------|
| A0 01 | Keys | 结构数组 |
| A0 02 | Total | N6 |
| A0 03 | Page | N4 |
| A0 04 | HasMore | N1 |

### 3.5 分享创建 (KeyShareCreate) - 2000/2001

**请求 Body**:

| 标签 | 名称 | 格式 |
|------|------|------|
| A0 01 | KeyId | AN32 |
| A0 02 | ToUserId | AN32 |
| A0 03 | ToVendor | AN16 |
| A0 04 | AccessLevel | B4 |
| A0 05 | ValidFrom | N14 |
| A0 06 | ValidUntil | N14 |
| A0 07 | MaxUses | N6 |

**响应 Body**:

| 标签 | 名称 | 格式 |
|------|------|------|
| A0 01 | ShareId | AN32 |
| A0 02 | ShareCode | N6 |
| A0 03 | ShareUrl | AN128 |

### 3.6 车辆控制指令 (VehicleCommand) - 3000/3001

**请求 Body**:

| 标签 | 名称 | 格式 | 说明 |
|------|------|------|------|
| A0 01 | CmdId | AN32 | 指令ID |
| A0 02 | KeyId | AN32 | 密钥ID |
| A0 03 | VehicleId | AN32 | 车辆ID |
| A0 04 | Action | N2 | 操作类型 |
| A0 05 | Params | 结构 | 参数 |
| A0 06 | Source | N1 | 来源 |
| A0 07 | WaitResponse | N1 | 1=等待响应 |
| A0 08 | TimeoutMs | N5 | 超时ms |

**Action定义**:

| 值 | 名称 | 说明 |
|----|------|------|
| 01 | Unlock | 解锁 |
| 02 | Lock | 闭锁 |
| 03 | EngineStart | 启动引擎 |
| 04 | EngineStop | 熄火 |
| 05 | TrunkOpen | 开启后备箱 |
| 06 | WindowUp | 车窗升起 |
| 07 | WindowDown | 车窗降下 |
| 08 | ClimateOn | 开启空调 |
| 09 | ClimateOff | 关闭空调 |
| 10 | FindVehicle | 寻车 |
| 11 | Horn | 鸣笛 |

**Source定义**:

| 值 | 来源 |
|----|------|
| 1 | NFC |
| 2 | BLE |
| 3 | UWB |
| 4 | Remote (云端) |
| 5 | Edge (边缘触发) |

**响应 Body**:

| 标签 | 名称 | 格式 |
|------|------|------|
| A0 01 | CmdId | AN32 |
| A0 02 | Status | N2 |
| A0 03 | ResultCode | N4 |
| A0 04 | ExecutedAt | N14 |

### 3.7 车辆状态上报 (VehicleStatusReport) - 3002

**Body结构**:

| 标签 | 名称 | 格式 | 说明 |
|------|------|------|------|
| A0 01 | VehicleId | AN32 | |
| A0 02 | TcuId | AN32 | TCU ID |
| A0 03 | LockStatus | N1 | 0=解锁 1=闭锁 |
| A0 04 | EngineStatus | N1 | 0=熄火 1=运行 |
| A0 05 | DoorStatus | B2 | 门状态位掩码 |
| A0 06 | WindowStatus | B2 | 车窗状态 |
| A0 07 | BatteryPct | N3 | 电量% |
| A0 08 | InteriorTemp | NS5 | 温度(0.1°C) |
| A0 09 | OdometerKm | N8 | 里程(km) |
| A0 0A | AlarmStatus | N2 | 报警状态 |
| A0 0B | Latitude | N10+1 | 纬度 |
| A0 0C | Longitude | N11+1 | 经度 |
| A0 0D | Timestamp | N14 | 上报时间 |

### 3.8 心跳 (Heartbeat) - 9000/9001

**请求 Body (9000)**:

| 标签 | 名称 | 格式 |
|------|------|------|
| A0 01 | Status | N1 |
| A0 02 | CpuUsage | N3 |
| A0 03 | MemUsage | N3 |
| A0 04 | ConnCount | N4 |

**响应 Body (9001)**: 空

---

## 4. 消息尾 Trailer (E1 FF)

| 标签 | 名称 | 格式 | 说明 |
|------|------|------|------|
| E1 FF 01 | Signature | B | HMAC-SHA256签名 |
| E1 FF 02 | MacKeyId | AN16 | MAC密钥ID |

**签名范围**: Header + Body

---

## 5. 错误码定义

| 错误码 | 名称 | 说明 |
|--------|------|------|
| 0000 | Success | 成功 |
| 1001 | InvalidRequest | 无效请求 |
| 1002 | InvalidSignature | 签名无效 |
| 1003 | Expired | 消息过期 |
| 1004 | RateLimit | 限流 |
| 1999 | InternalError | 内部错误 |
| 2001 | KeyNotFound | 密钥不存在 |
| 2002 | KeyExists | 密钥已存在 |
| 2003 | KeyExpired | 密钥已过期 |
| 2004 | KeyRevoked | 密钥已撤销 |
| 2005 | KeySuspended | 密钥已挂起 |
| 2006 | MaxKeysReached | 密钥数量达上限 |
| 2007 | NoPermission | 权限不足 |
| 3001 | VehicleNotFound | 车辆不存在 |
| 3002 | VehicleOffline | 车辆离线 |
| 3003 | TcuTimeout | TCU超时 |
| 3004 | CommandFailed | 指令执行失败 |
| 4001 | VendorAdapterNotFound | 厂商适配器不存在 |
| 4002 | VendorApiError | 厂商API错误 |
| 4003 | VendorTimeout | 厂商超时 |

---

## 6. 编码示例

### 6.1 KeyBind请求编码示例

```
// 原始数据
{
  VehicleId: "VH123456789012345",
  DeviceId: "DEV987654321",
  UserId: "USR123456",
  Vendor: "xiaomi",
  Protocol: "iccoa_dk40",
  KeyType: 01,
  AccessLevel: 0x000F (解锁+闭锁+启动+熄火),
  ValidFrom: "20260506000000",
  ValidUntil: "20270506000000"
}

// BERTLV编码 (十六进制)
E1 01 04 01 30 00           // Version = 1.0
E1 02 0E 32 30 32 36 30 35 30 36 30 38 35 30 30 30 // Timestamp = 20260506085000
E1 03 02 31 30 30 30         // MessageType = 1000
E1 04 04 30 30 30 30 31       // SequenceNo = 00001
E1 05 10 44 45 56 39 38 37 36 35 34 33 32 31 // DeviceId = DEV987654321

A0 01 11 56 48 31 32 33 34 35 36 37 38 39 30 31 32 33 34 35 // VehicleId
A0 03 08 55 53 52 31 32 33 34 35 36 // UserId = USR123456
A0 05 06 78 69 61 6F 6D 65 69 // Vendor = xiaomi
A0 06 08 69 63 63 6F 61 5F 64 6B 34 30 // Protocol = iccoa_dk40
A0 07 01 30 31               // KeyType = 01 (Owner)
A0 08 02 30 30 46           // AccessLevel = 0046 (Unlock+Lock+EngineStart+EngineStop)
A0 0B 0E 32 30 32 36 30 35 30 36 30 30 30 30 30 30 // ValidFrom
A0 0C 0E 32 30 32 37 30 35 30 36 30 30 30 30 30 30 // ValidUntil

E1 FF 01 20 [HMAC-SHA256签名]  // Trailer
```

---

## 7. 版本演进

| 版本 | 日期 | 变更说明 |
|------|------|----------|
| 1.0.0 | 2026-05-06 | 初始版本 |
