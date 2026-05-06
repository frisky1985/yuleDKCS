# DKCS ↔ TCU MQTT私有协议规范 (BERTLV)

**版本**: 1.0.0
**日期**: 2026-05-06
**传输**: MQTT over TLS
**安全**: 设备证书双向认证 + 数据签名

---

## 1. MQTT Topic规范

### 1.1 Topic结构

```
digitalkey/{tsp_id}/{vehicle_id}/{resource}/{action}
```

### 1.2 Topic定义

| Topic | 方向 | QoS | 说明 |
|-------|------|-----|------|
| `{root}/status` | TCU→DKCS | 1 | 车辆状态上报 |
| `{root}/command` | DKCS→TCU | 2 | 下发控制指令 |
| `{root}/command/ack` | TCU→DKCS | 1 | 指令响应 |
| `{root}/key/bind` | DKCS→TCU | 2 | 下发绑定密钥 |
| `{root}/key/bind/ack` | TCU→DKCS | 1 | 绑定响应 |
| `{root}/key/unbind` | DKCS→TCU | 2 | 解绑密钥 |
| `{root}/key/unbind/ack` | TCU→DKCS | 1 | 解绑响应 |
| `{root}/key/revoke` | DKCS→TCU | 2 | 撤销密钥 |
| `{root}/key/revoke/ack` | TCU→DKCS | 1 | 撤销响应 |
| `{root}/ota/campaign` | DKCS→TCU | 2 | OTA推送 |
| `{root}/ota/progress` | TCU→DKCS | 1 | OTA进度 |
| `{root}/config` | DKCS→TCU | 1 | 远程配置 |
| `{root}/heartbeat` | TCU→DKCS | 0 | 心跳 |
| `{root}/response` | DKCS→TCU | 1 | 通用响应 |

### 1.3 QoS级别

| 级别 | 名称 | 使用场景 |
|------|------|----------|
| 0 | At-most-once | 心跳、状态上报 |
| 1 | At-least-once | 密钥同步、配置 |
| 2 | Exactly-once | 控制指令、OTA |

---

## 2. 消息格式

### 2.1 Payload结构 (BERTLV)

```
┌────────────────────────────────────────────────┐
│                   Payload                       │
├──────────────┬─────────────────┬───────────────┤
│   Header     │      Body       │    Trailer    │
│  (E1 01)    │    (BERTLV)      │   (E1 FF)     │
│   20-50字节  │   变长           │   32-64字节   │
└──────────────┴─────────────────┴───────────────┘
```

### 2.2 消息头部 (E1 01)

| 标签 | 名称 | 格式 | 说明 |
|------|------|------|------|
| E1 01 01 | Version | BCD | 版本号 |
| E1 01 02 | Timestamp | N14 | YYYYMMDDhhmmss |
| E1 01 03 | MessageType | N4 | 消息类型 |
| E1 01 04 | SequenceNo | N6 | 序列号 |
| E1 01 05 | TcuId | AN32 | TCU设备ID |
| E1 01 06 | VehicleId | AN32 | 车辆ID |
| E1 01 07 | KeyId | AN32 | 密钥ID(可选) |

### 2.3 消息类型 (E1 01 03)

| 类型码 | 名称 | 方向 | QoS |
|--------|------|------|-----|
| 5001 | Command | DKCS→TCU | 2 |
| 5002 | CommandAck | TCU→DKCS | 1 |
| 5011 | KeyBind | DKCS→TCU | 2 |
| 5012 | KeyBindAck | TCU→DKCS | 1 |
| 5021 | KeyUnbind | DKCS→TCU | 2 |
| 5022 | KeyUnbindAck | TCU→DKCS | 1 |
| 5031 | KeyRevoke | DKCS→TCU | 2 |
| 5032 | KeyRevokeAck | TCU→DKCS | 1 |
| 5040 | StatusReport | TCU→DKCS | 1 |
| 5050 | Heartbeat | TCU→DKCS | 0 |
| 5060 | OtaCampaign | DKCS→TCU | 2 |
| 5061 | OtaProgress | TCU→DKCS | 1 |
| 5070 | Config | DKCS→TCU | 1 |
| 9100 | Error | 双向 | 1 |

---

## 3. 消息体定义

### 3.1 控制指令 (Command) - 5001/5002

**下发 Body (5001)**:

| 标签 | 名称 | 格式 | 说明 |
|------|------|------|------|
| B0 01 | CmdId | AN32 | 指令ID |
| B0 02 | KeyId | AN32 | 密钥ID |
| B0 03 | Action | N2 | 操作类型 |
| B0 04 | Params | 结构 | 参数 |
| B0 05 | Priority | N1 | 优先级 |
| B0 06 | TimeoutMs | N5 | 超时ms |
| B0 07 | ExpiresAt | N14 | 过期时间 |

**Params结构**:

| 标签 | 名称 | 格式 |
|------|------|------|
| B0 04 01 | Window | AN16 |
| B0 04 02 | ClimateTemp | NS4 |
| B0 04 03 | SeatPosition | N2 |

**响应 Body (5002)**:

| 标签 | 名称 | 格式 |
|------|------|------|
| B0 01 | CmdId | AN32 |
| B0 02 | Status | N2 |
| B0 03 | ResultCode | N4 |
| B0 04 | ExecutedAt | N14 |
| B0 05 | LatencyMs | N5 |

**Status定义**:

| 值 | 名称 |
|----|------|
| 00 | 成功 |
| 01 | 失败 |
| 02 | 超时 |
| 03 | 密钥无效 |
| 04 | 权限不足 |
| 05 | 车辆离线 |

### 3.2 密钥绑定 (KeyBind) - 5011/5012

**下发 Body (5011)**:

| 标签 | 名称 | 格式 | 说明 |
|------|------|------|------|
| B0 01 | KeyId | AN32 | |
| B0 02 | Protocol | N2 | 1=CCC 2=ICCOA 3=ICCE |
| B0 03 | KeyType | N2 | 1=Owner |
| B0 04 | AccessLevel | B4 | 权限 |
| B0 05 | SeKeyRef | AN64 | SE密钥引用 |
| B0 06 | VehicleKeyRef | AN64 | 车辆密钥引用 |
| B0 07 | BleChannel | N2 | BLE通道 |
| B0 08 | BleMac | AN12 | 车端BLE MAC |
| B0 09 | UwbChannel | N2 | UWB通道 |
| B0 0A | UwbPreamble | N2 | UWB前导码 |
| B0 0B | ValidFrom | N14 | |
| B0 0C | ValidUntil | N14 | |
| B0 0D | MaxUses | N6 | |

**响应 Body (5012)**:

| 标签 | 名称 | 格式 |
|------|------|------|
| B0 01 | Status | N2 |
| B0 02 | ErrorMsg | AN64 |

### 3.3 密钥解绑 (KeyUnbind) - 5021/5022

**下发 Body (5021)**:

| 标签 | 名称 | 格式 |
|------|------|------|
| B0 01 | KeyId | AN32 |
| B0 02 | Reason | AN64 |

**响应 Body (5022)**:

| 标签 | 名称 | 格式 |
|------|------|------|
| B0 01 | Status | N2 |

### 3.4 密钥撤销 (KeyRevoke) - 5031/5032

**下发 Body (5031)**:

| 标签 | 名称 | 格式 |
|------|------|------|
| B0 01 | KeyId | AN32 |
| B0 02 | Reason | AN64 |
| B0 03 | Emergency | N1 |

**响应 Body (5032)**:

| 标签 | 名称 | 格式 |
|------|------|------|
| B0 01 | Status | N2 |

### 3.5 车辆状态上报 (StatusReport) - 5040

**Body**:

| 标签 | 名称 | 格式 | 说明 |
|------|------|------|------|
| B0 01 | TcuFirmware | AN32 | TCU固件版本 |
| B0 02 | LockStatus | N1 | 0=解锁 1=闭锁 |
| B0 03 | EngineStatus | N1 | 0=熄火 1=运行 |
| B0 04 | DoorStatus | B2 | 门位掩码 |
| B0 05 | WindowStatus | B2 | 车窗位掩码 |
| B0 06 | BatteryVoltage | N4 | mV |
| B0 07 | BatteryPct | N3 | % |
| B0 08 | InteriorTemp | NS5 | 0.1°C |
| B0 09 | OdometerKm | N8 | |
| B0 0A | AlarmStatus | N2 | |
| B0 0B | Latitude | N10+1 | |
| B0 0C | Longitude | N11+1 | |
| B0 0D | SignalStrength | N3 | dBm |
| B0 0E | BleConnected | N2 | BLE连接数 |
| B0 0F | UwbConnected | N2 | UWB连接数 |
| B0 10 | ErrorCodes | 数组 | 错误码列表 |
| B0 11 | Timestamp | N14 | |

**DoorStatus位掩码**:

| 位 | 含义 |
|----|------|
| 0 | 左前门 |
| 1 | 右前门 |
| 2 | 左后门 |
| 3 | 右后门 |
| 4 | 后备箱 |
| 5 | 天窗 |

### 3.6 心跳 (Heartbeat) - 5050

**Body**:

| 标签 | 名称 | 格式 |
|------|------|------|
| B0 01 | Status | N1 |
| B0 02 | BleAvailable | N1 |
| B0 03 | NfcAvailable | N1 |
| B0 04 | UwbAvailable | N1 |
| B0 05 | SeAvailable | N1 |
| B0 06 | FreeStorage | N3 |
| B0 07 | CpuUsage | N3 |
| B0 08 | MemUsage | N3 |
| B0 09 | Timestamp | N14 |

### 3.7 OTA推送 (OtaCampaign) - 5060

**Body**:

| 标签 | 名称 | 格式 |
|------|------|------|
| B0 01 | CampaignId | AN32 |
| B0 02 | TargetVersion | AN32 |
| B0 03 | DownloadUrl | AN256 |
| B0 04 | FileSize | N10 |
| B0 05 | FileHash | AN64 |
| B0 06 | Priority | N1 |
| B0 07 | MinBatteryPct | N3 |
| B0 08 | ChargingRequired | N1 |
| B0 09 | MinFreeSpaceMb | N5 |
| B0 0A | ReleaseNotes | AN512 |

### 3.8 OTA进度 (OtaProgress) - 5061

**Body**:

| 标签 | 名称 | 格式 |
|------|------|------|
| B0 01 | CampaignId | AN32 |
| B0 02 | Phase | N1 |
| B0 03 | ProgressPct | N3 |
| B0 04 | DownloadedBytes | N10 |
| B0 05 | TotalBytes | N10 |
| B0 06 | Result | N1 |
| B0 07 | ErrorCode | AN16 |
| B0 08 | StartedAt | N14 |
| B0 09 | CompletedAt | N14 |

**Phase定义**: 1=下载 2=校验 3=安装 4=重启

### 3.9 远程配置 (Config) - 5070

**Body**:

| 标签 | 名称 | 格式 | 说明 |
|------|------|------|------|
| B0 01 | BleTxPower | N3 | dBm |
| B0 02 | BleAdvInterval | N4 | ms |
| B0 03 | UwbTxPower | N3 | dBm |
| B0 04 | UwbChannel | N2 | |
| B0 05 | UwbPreamble | N2 | |
| B0 06 | RangingInterval | N4 | ms |
| B0 07 | ZoneUnlockMm | N5 | 解锁距离 |
| B0 08 | ZoneLockMm | N5 | 闭锁距离 |
| B0 09 | SeEnabled | N1 | |
| B0 0A | BleEncryptionRequired | N1 | |
| B0 0B | UwbRangingRequired | N1 | |
| B0 0C | CommandTimeoutMs | N5 | |
| B0 0D | PairingTimeoutS | N3 | |

---

## 4. Trailer (E1 FF)

| 标签 | 名称 | 格式 |
|------|------|------|
| E1 FF 01 | Signature | B32 |
| E1 FF 02 | MacKeyId | AN16 |

---

## 5. TCU状态机

```
                    ┌─────────────┐
                    │   OFFLINE   │
                    └──────┬──────┘
                           │ Connect+Auth
                           ▼
┌──────────┐    ┌─────────────┐    ┌──────────────┐
│  ERROR   │◄──►│   ONLINE    │◄──►│   ACTIVE     │
└──────────┘    └──────┬──────┘    └──────┬───────┘
                       │                  │
                       │ Heartbeat Timeout│ Received Cmd/Key
                       │                  │
                       ▼                  ▼
                ┌─────────────┐    ┌──────────────┐
                │  STANDBY   │    │  COMMAND     │
                └─────────────┘    │  EXECUTING   │
                                   └──────┬───────┘
                                          │
                                          │ Complete/Timeout
                                          ▼
```

---

## 6. 编码示例

### 6.1 Command消息编码

```
// Topic: digitalkey/TSP001/VH123456789012345/command
// QoS: 2

// Header
E1 01 01 04 01 30 00           // Version = 1.0
E1 01 02 0E 32 30 32 36 30 35 30 36 30 38 35 30 30 30 // Timestamp
E1 01 03 04 35 30 30 31         // MessageType = 5001
E1 01 04 04 30 30 30 30 31       // SequenceNo = 00001
E1 01 05 10 54 43 55 31 32 33 34 35 36 37 38 // TcuId = TCU12345678
E1 01 06 11 56 48 31 32 33 34 35 36 37 38 39 30 31 32 33 34 35 // VehicleId

// Body
B0 01 20 43 4D 44 31 32 33 34 35 36 37 38 39 30 31 32 33 34 35 36 37 38 39 30 31 32 33 34 // CmdId
B0 02 10 4B 45 59 31 32 33 34 35 36 37 38 39 30 31 32 33 34 // KeyId
B0 03 01 30 31                 // Action = 01 (Unlock)
B0 06 04 30 31 30 30 30         // TimeoutMs = 01000

// Trailer
E1 FF 01 20 [HMAC签名]          // Signature
```

---

## 7. 版本演进

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0.0 | 2026-05-06 | 初始版本 |
