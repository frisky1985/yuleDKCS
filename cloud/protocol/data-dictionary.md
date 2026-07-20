# 统一数据字典

**版本**: 1.0.0
**日期**: 2026-05-06

---

## 1. BERTLV标签分配

### 1.1 标签区间分配

| 区间 | 用途 | 说明 |
|------|------|------|
| E0 01 - E0 FF | 协议控制 | 协议级字段 |
| E1 01 - E1 FF | 消息头 | 消息头部字段 |
| E1 FF | Trailer | 消息尾部 |
| A0 01 - A0 FF | 业务数据 | 业务请求/响应 |
| B0 01 - B0 FF | TCU数据 | TCU特定数据 |
| C0 01 - C0 FF | 保留 | 未来扩展 |
| D0 01 - D0 FF | 用户数据 | 用户相关 |
| E2 01 - E2 FF | 保留 | 未来扩展 |

---

## 2. 协议控制标签 (E0)

| 标签 | 名称 | 格式 | 说明 |
|------|------|------|------|
| E0 01 | ProtocolVersion | BCD | 协议版本 |
| E0 02 | Encoding | N1 | 编码方式 |
| E0 03 | Compression | N1 | 压缩方式 |
| E0 04 | Encryption | N1 | 加密方式 |
| E0 05 | Padding | N1 | 填充方式 |

---

## 3. 消息头标签 (E1)

| 标签 | 名称 | 格式 | 最大长度 | 说明 |
|------|------|------|----------|------|
| E1 01 | Version | BCD | 4 | 版本号 |
| E1 02 | Timestamp | N14 | 14 | YYYYMMDDhhmmss |
| E1 03 | MessageType | N4 | 4 | 消息类型 |
| E1 04 | SequenceNo | N8 | 8 | 序列号 |
| E1 05 | DeviceId | AN32 | 32 | 设备ID |
| E1 06 | SessionId | AN32 | 32 | 会话ID |
| E1 07 | Priority | N1 | 1 | 优先级 |
| E1 08 | Flags | B1 | 1 | 标志位 |
| E1 09 | CorrelationId | AN32 | 32 | 关联ID |
| E1 0A | UserId | AN32 | 32 | 用户ID |
| E1 0B | AppId | AN16 | 16 | 应用ID |
| E1 0C | AppVersion | AN16 | 16 | App版本 |
| E1 0D | TspId | AN16 | 16 | TSP ID |
| E1 0E | VehicleId | AN32 | 32 | 车辆ID |
| E1 0F | TcuId | AN32 | 32 | TCU ID |

---

## 4.  Trailer标签 (E1 FF)

| 标签 | 名称 | 格式 | 说明 |
|------|------|------|------|
| E1 FF 01 | Signature | B | HMAC签名 |
| E1 FF 02 | MacKeyId | AN16 | MAC密钥ID |
| E1 FF 03 | Crc32 | B4 | CRC32校验 |
| E1 FF 04 | EncryptedData | B | 加密数据 |

---

## 5. 业务数据标签 (A0)

### 5.1 用户相关 (D0)

| 标签 | 名称 | 格式 | 说明 |
|------|------|------|------|
| D0 01 | UserId | AN32 | 用户ID |
| D0 02 | UserName | AN64 | 用户名 |
| D0 03 | Phone | AN16 | 手机号 |
| D0 04 | Email | AN64 | 邮箱 |
| D0 05 | Avatar | AN256 | 头像URL |
| D0 06 | Nickname | AN32 | 昵称 |
| D0 07 | Gender | N1 | 性别 |
| D0 08 | BirthDate | N8 | 生日 |
| D0 09 | CreatedAt | N14 | 创建时间 |
| D0 0A | UpdatedAt | N14 | 更新时间 |
| D0 0B | Status | N1 | 状态 |

### 5.2 设备相关 (D1)

| 标签 | 名称 | 格式 | 说明 |
|------|------|------|------|
| D1 01 | DeviceId | AN32 | 设备ID |
| D1 02 | DeviceType | AN16 | 设备类型 |
| D1 03 | DeviceModel | AN32 | 设备型号 |
| D1 04 | OsVersion | AN16 | OS版本 |
| D1 05 | Vendor | AN16 | 厂商 |
| D1 06 | AppVersion | AN16 | App版本 |
| D1 07 | BleMac | AN12 | BLE MAC |
| D1 08 | Uuid | AN36 | 设备UUID |
| D1 09 | SeId | AN64 | SE序列号 |
| D1 0A | PublicKey | B64 | 公钥 |

### 5.3 车辆相关 (A1)

| 标签 | 名称 | 格式 | 说明 |
|------|------|------|------|
| A1 01 | VehicleId | AN32 | 车辆ID |
| A1 02 | Vin | AN17 | VIN码 |
| A1 03 | Brand | AN32 | 品牌 |
| A1 04 | Model | AN32 | 车型 |
| A1 05 | Year | N4 | 年份 |
| A1 06 | Color | AN16 | 颜色 |
| A1 07 | PlateNo | AN16 | 车牌号 |
| A1 08 | TspId | AN16 | TSP ID |
| A1 09 | TcuId | AN32 | TCU ID |
| A1 0A | BleMac | AN12 | BLE MAC |
| A1 0B | CreatedAt | N14 | 添加时间 |

### 5.4 密钥相关 (A2)

| 标签 | 名称 | 格式 | 说明 |
|------|------|------|------|
| A2 01 | KeyId | AN32 | 密钥ID |
| A2 02 | VehicleId | AN32 | 车辆ID |
| A2 03 | UserId | AN32 | 用户ID |
| A2 04 | DeviceId | AN32 | 设备ID |
| A2 05 | Vendor | AN16 | 厂商 |
| A2 06 | Protocol | AN16 | 协议 |
| A2 07 | KeyType | N2 | 密钥类型 |
| A2 08 | Status | N2 | 状态 |
| A2 09 | AccessLevel | B4 | 权限 |
| A2 0A | ValidFrom | N14 | 生效时间 |
| A2 0B | ValidUntil | N14 | 失效时间 |
| A2 0C | MaxUses | N6 | 最大次数 |
| A2 0D | UsedCount | N6 | 已用次数 |
| A2 0E | DistanceLimit | N8 | 距离限制 |
| A2 0F | CreatedAt | N14 | 创建时间 |
| A2 10 | UpdatedAt | N14 | 更新时间 |
| A2 11 | RevokedAt | N14 | 撤销时间 |
| A2 12 | RevokeReason | AN64 | 撤销原因 |

### 5.5 分享相关 (A3)

| 标签 | 名称 | 格式 | 说明 |
|------|------|------|------|
| A3 01 | ShareId | AN32 | 分享ID |
| A3 02 | KeyId | AN32 | 密钥ID |
| A3 03 | FromUserId | AN32 | 分享人 |
| A3 04 | ToUserId | AN32 | 接收人 |
| A3 05 | ToVendor | AN16 | 接收人厂商 |
| A3 06 | Status | N2 | 状态 |
| A3 07 | ShareCode | N6 | 分享码 |
| A3 08 | AccessLevel | B4 | 权限 |
| A3 09 | ValidFrom | N14 | 生效时间 |
| A3 0A | ValidUntil | N14 | 失效时间 |
| A3 0B | MaxUses | N6 | 最大次数 |
| A3 0C | UsedCount | N6 | 已用次数 |
| A3 0D | CreatedAt | N14 | 创建时间 |
| A3 0E | AcceptedAt | N14 | 接受时间 |

### 5.6 指令相关 (A4)

| 标签 | 名称 | 格式 | 说明 |
|------|------|------|------|
| A4 01 | CmdId | AN32 | 指令ID |
| A4 02 | VehicleId | AN32 | 车辆ID |
| A4 03 | KeyId | AN32 | 密钥ID |
| A4 04 | Action | N2 | 操作 |
| A4 05 | Status | N2 | 状态 |
| A4 06 | Source | N1 | 来源 |
| A4 07 | ResultCode | N4 | 结果码 |
| A4 08 | CreatedAt | N14 | 创建时间 |
| A4 09 | ExecutedAt | N14 | 执行时间 |
| A4 0A | CompletedAt | N14 | 完成时间 |
| A4 0B | LatencyMs | N5 | 耗时 |

### 5.7 状态相关 (A5)

| 标签 | 名称 | 格式 | 说明 |
|------|------|------|------|
| A5 01 | LockStatus | N1 | 锁状态 |
| A5 02 | EngineStatus | N1 | 引擎状态 |
| A5 03 | DoorStatus | B2 | 门状态 |
| A5 04 | WindowStatus | B2 | 车窗状态 |
| A5 05 | TrunkStatus | N1 | 后备箱 |
| A5 06 | BatteryVoltage | N4 | 电压mV |
| A5 07 | BatteryPct | N3 | 电量% |
| A5 08 | InteriorTemp | NS5 | 车内温度 |
| A5 09 | ExteriorTemp | NS5 | 车外温度 |
| A5 0A | OdometerKm | N8 | 里程 |
| A5 0B | FuelPct | N3 | 燃油% |
| A5 0C | AlarmStatus | N2 | 报警 |
| A5 0D | Latitude | N10+1 | 纬度 |
| A5 0E | Longitude | N11+1 | 经度 |
| A5 0F | GpsAccuracy | N3 | GPS精度 |
| A5 10 | Timestamp | N14 | 时间戳 |

---

## 6. 枚举值定义

### 6.1 状态 (Status)

| 值 | 名称 | 说明 |
|----|------|------|
| 0 | Unknown | 未知 |
| 1 | Active | 激活 |
| 2 | Suspended | 挂起 |
| 3 | Revoked | 撤销 |
| 4 | Expired | 过期 |
| 5 | Pending | 待激活 |

### 6.2 密钥类型 (KeyType)

| 值 | 名称 | 说明 |
|----|------|------|
| 1 | Owner | 车主 |
| 2 | Friend | 朋友 |
| 3 | Service | 服务 |
| 4 | Temporary | 临时 |

### 6.3 协议 (Protocol)

| 值 | 名称 | 说明 |
|----|------|------|
| ccc_dk3 | CCC DK3.0 | Car Connectivity Consortium |
| iccoa_dk30 | ICCOA DK3.0 | 智慧车联开放联盟 |
| iccoa_dk40 | ICCOA DK4.0 | 智慧车联开放联盟 |
| icce | ICCE | Automotive Edge Computing |

### 6.4 操作 (Action)

| 值 | 名称 | 说明 |
|----|------|------|
| 1 | Unlock | 解锁 |
| 2 | Lock | 闭锁 |
| 3 | EngineStart | 启动引擎 |
| 4 | EngineStop | 熄火 |
| 5 | TrunkOpen | 开启后备箱 |
| 6 | WindowUp | 车窗升起 |
| 7 | WindowDown | 车窗降下 |
| 8 | ClimateOn | 开启空调 |
| 9 | ClimateOff | 关闭空调 |
| 10 | FindVehicle | 寻车 |
| 11 | Horn | 鸣笛 |

### 6.5 来源 (Source)

| 值 | 名称 | 说明 |
|----|------|------|
| 1 | NFC | NFC触碰 |
| 2 | BLE | BLE通信 |
| 3 | UWB | UWB测距 |
| 4 | Remote | 云端远程 |
| 5 | Edge | 边缘触发 |

### 6.6 设备类型 (DeviceType)

| 值 | 名称 |
|----|------|
| phone | 手机 |
| watch | 手表 |
| band | 手环 |
| card | 卡片 |

### 6.7 锁状态 (LockStatus)

| 值 | 名称 |
|----|------|
| 0 | Unlocked |
| 1 | Locked |
| 2 | Unknown |

### 6.8 引擎状态 (EngineStatus)

| 值 | 名称 |
|----|------|
| 0 | Off |
| 1 | Running |
| 2 | Starting |

---

## 7. 数据格式说明

| 格式代码 | 说明 | 示例 |
|----------|------|------|
| AN | 字母数字 | "ABC123" |
| N | 数字 | "12345" |
| B | 二进制(十六进制) | "0F1A2B" |
| BCD | BCD编码数字 | "0123"=0x01 0x23 |
| NS | 带符号数字 | "-123" |
| AN16 | 最多16字节 | |
| AN32 | 最多32字节 | |
| B64 | Base64编码 | |
