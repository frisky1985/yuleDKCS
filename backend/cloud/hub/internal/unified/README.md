# 统一协议接口层 (unified)

整合 CCC Digital Key 3.0 / ICCOA DK 3.0+4.0 / ICCE 三套数字钥匙协议，提供统一的 API 抽象。

## 架构

```
┌──────────────────────────────────────────────────────────┐
│                     Application Layer                      │
│            (hub service / gateway / SDK)                  │
└─────────────────────────┬────────────────────────────────┘
                           │ BindKey / ShareKey / RemoteControl
┌──────────────────────────▼────────────────────────────────┐
│                    Manager (管理器)                        │
│  · 协议协商   · 消息路由   · 编解码抽象   · 会话管理        │
└────────┬───────────┬───────────┬──────────────────────────┘
         │           │           │
    ┌────▼────┐ ┌────▼────┐ ┌────▼────┐
    │  ICCOA  │ │   ICCE  │ │   CCC   │
    │  Codec  │ │  Codec  │ │  Codec  │
    └─────────┘ └─────────┘ └─────────┘
```

## 核心组件

| 文件 | 职责 |
|------|------|
| `protocol.go` | 统一数据模型：Device、CapabilitySet、UnifiedMessage、MessageType |
| `router.go` | 协议协商器：根据设备/车辆能力自动选择最优协议 |
| `codec.go` | 统一编解码器：ICCOA / ICCE / CCC 三套编码器 + 自动协议检测 |
| `state.go` | 会话状态机：Init→协商→验证→绑定→激活 完整生命周期管理 |
| `device.go` | 设备抽象：厂商映射、能力匹配、功能特性 |
| `manager.go` | 顶层管理器：整合以上所有组件，提供统一 API |
| `unified_test.go` | 单元测试 |

## 协议协商

```go
req := &NegotiateRequest{
    DeviceID:   "device-001",
    Vendor:     "xiaomi",
    OS:         "android",
    DeviceCaps: &NegotiateCapabilities{BLE: true, UWB: true, NFC: true, SE: true, FiRa: true},
    VehicleCaps: &NegotiateCapabilities{BLE: true, UWB: true, NFC: true, SE: true},
}
resp, err := mgr.NegotiateProtocol(ctx, req)
// resp.Protocol = ProtocolICCOA40 (评分最高)
// resp.SessionID = "sess-device-001-..."
```

### 协议选择规则

| 厂商 | 协议优先级 | 说明 |
|------|-----------|------|
| 小米/OPPO/vivo | ICCOA 4.0 > ICCOA 3.0 | 中国移动联盟标准 |
| 华为 | ICCE > ICCOA 4.0 | 华为生态 |
| 苹果 | CCC 3.0 | 苹果独占 |
| 三星 | CCC 3.0 > ICCOA 4.0 | 优先 CCC |

## 消息类型

```go
UnifiedMessage{
    Type: MsgTypeKeyBind,      // 密钥绑定
    Type: MsgTypeKeyShare,     // 密钥分享
    Type: MsgTypeRemoteControl, // 远程控制
    Type: MsgTypeVehicleStatus, // 车辆状态上报
    Type: MsgTypeDeviceInfo,   // 设备信息
    ...
}
```

每条消息统一用 `ProtocolCodec.Encode(msg)` → `[]byte`，自动适配对应协议帧格式。

## 会话状态机

```
Init ──[协商]──> Negotiating ──[完成]──> DeviceVerified
    │                                        │
    │                                   [绑定开始]
    │                                        ▼
    └──[超时/撤销]──> Revoked ◄──[撤销]◄── KeyBinding
       (终态)                                  │
                                              ▼
                                         Active
                                    /    |    \    \
                              [分享]  [挂起]  [撤销]  [过期]
                                 │       │       │
                                 ▼       ▼       ▼
                              Sharing Suspended Revoked
                                 │
                            [分享完成]
                                 │
                                 ▼
                              Active
```

## 使用示例

```go
// 1. 创建 Manager
mgr := unified.NewManager(&unified.Config{
    Logger: zap.NewExample(),
    SessionTimeout: 30 * time.Minute,
})

// 2. 协议协商
negResp, err := mgr.NegotiateProtocol(ctx, &unified.NegotiateRequest{
    DeviceID:   deviceID,
    Vendor:     "xiaomi",
    OS:         "android",
    DeviceCaps: &unified.NegotiateCapabilities{BLE: true, UWB: true},
    VehicleCaps: &unified.NegotiateCapabilities{BLE: true, UWB: true},
})

// 3. 密钥绑定 (自动路由到对应协议)
bindResp, err := mgr.BindKey(ctx, negResp.SessionID, &pb.BindKeyRequest{
    VehicleId: vehicleID,
    UserId:    userID,
    ...
})

// 4. 密钥分享
shareResp, err := mgr.ShareKey(ctx, negResp.SessionID, &pb.CreateShareRequest{
    KeyId: bindResp.Key.KeyId,
    RecipientId: recipientID,
})

// 5. 处理车辆上报
status, err := mgr.HandleVehicleStatus(ctx, negResp.SessionID, rawData)

// 6. 处理远程控制
ok, err := mgr.HandleRemoteControl(ctx, negResp.SessionID, rawData)
```

## 编解码器

### ICCOA (DK3.0 / DK4.0)
- 帧格式: `[0xA0][msg_type][TLV...]`
- TLV: `[tag][length][value]`
- 消息类型: 0x11=绑定, 0x21=分享, 0x31=控制

### ICCE (华为)
- 帧格式: BER-TLV
- Tag 范围: 0x9F01-0x9FFF (消息), 0x9F20-0x9F2F (状态)
- Length: 短格式(0-127) / 长格式(1-3字节长度字段)

### CCC (Digital Key 3.0)
- 帧格式: FiRa HCP
- 帧头: `0x5C`
- 消息类型: 0x10=绑定, 0x30=控制, 0x40=状态

## TODO

- [ ] 与 adapter registry 集成
- [ ] 与 key service 集成
- [ ] gRPC 服务层
- [ ] WebSocket 实时通知
- [ ] 协议版本协商
- [ ] 完整的 TLV 编解码 (ICCOA)
- [ ] ICCE BER-TLV 编解码 (复用 hub/codec/bertlv)
- [ ] 集成测试
