# SDK-Hub 接口映射验证

验证依据: `api/v1/hub.proto` → `api/sdk/v1/sdk.proto`

## Hub 17 个 RPC 映射结果

| # | Hub RPC | SDK HubService | 一致？ | 说明 |
|:-:|:--------|:--------------|:------:|:-----|
| 1 | `KeyManagement.BindKey` | `HubService.BindKey` | ✅ | App 只传 vehicle_id，SDK 自动填充 device_id/user_id/vendor/protocol/key_type/device_pubkey/access_level |
| 2 | `KeyManagement.UnbindKey` | `HubService.UnbindKey` | ✅ | 签名一致 (key_id + trace_id) |
| 3 | `KeyManagement.SuspendKey` | `HubService.SuspendKey` | ✅ | 新加。key_id + reason |
| 4 | `KeyManagement.ResumeKey` | `HubService.ResumeKey` | ✅ | 新加。key_id |
| 5 | `KeyManagement.RevokeKey` | `HubService.RevokeKey` | ✅ | 新加。key_id + reason |
| 6 | `KeyManagement.RenewKey` | `HubService.RenewKey` | ✅ | 新加。key_id + valid_until |
| 7 | `KeyManagement.GetKey` | `HubService.GetKey` | ✅ | SDK 返回 KeyItem（含 vehicle_name 本地缓存增强）|
| 8 | `KeyManagement.ListKeys` | `HubService.ListKeys` | ✅ | 签名一致 |
| 9 | `KeyShare.CreateShare` | `HubService.CreateShare` | ✅ | **App 必须传 to_vendor**（SDK 不知道好友的手机厂商） |
| 10 | `KeyShare.AcceptShare` | `HubService.AcceptShare` | ✅ | App 只传 share_code，SDK 自动填充 device_id/user_id/vendor/device_pubkey |
| 11 | `KeyShare.CancelShare` | `HubService.CancelShare` | ✅ | 签名一致 |
| 12 | `KeyShare.GetShare` | `HubService.GetShare` | ✅ | 新加 |
| 13 | `VehicleControl.SendCommand` | `HubService.SendCommand` | ✅ | App 传 vehicle_id + action，SDK 自动填充 key_id/source(4=Remote)/trace_id |
| 14 | `VehicleControl.StreamStatus` | `HubService.StreamStatus` | ✅ | 新加。Server-Stream 实时车辆状态 |
| 15 | `HubTransport.ForwardToVendor` | ❌ 跳过 | ✅ | Hub 内部：DKCS → Hub 转发到厂商云端 |
| 16 | `HubTransport.VendorCallback` | ❌ 跳过 | ✅ | Hub 内部：厂商回调 Hub |
| 17 | `HubTransport.HealthCheck` | ❌ 跳过 | ✅ | Hub 内部健康检查 |

## SDK 增强（非 Hub RPC，本地实现）

| SDK 模块 | 实现方式 | 说明 |
|:---------|:--------|:-----|
| `BLEManager` 7 RPC | Native (CoreBluetooth/Android BLE) | 本地 BLE/UWB，不走服务器 |
| `KeyManager` 3 RPC | Native (本地缓存) | SQLite 缓存 + 定时调 ListKeys 同步 |
| `MailboxClient` 3 RPC | HTTP/JSON → Relay Server REST gateway | CCC 协议分享用，Sharing URL 中的 REST 地址 |
| `Callbacks` 4 消息 | Native (Swift protocol / Kotlin interface) | SDK → App 的事件通知 |

## 验证结论

**完整。** 全部 Hub 的 14 个对外 RPC（排除 3 个 Hub 内部 RPC）都被 SDK HubService 覆盖。SDK 与 Hub 的接口协议完全一致。

**SDK 需要做的工作（非 Hub 感知）**:
- SDK `BindKey` 内部: 从手机 SE 读 `device_pubkey`，自动检测 `vendor`/`protocol`，从 token 解 `user_id`
- SDK `AcceptShare` 内部: 同上
- SDK `SendCommand` 内部: `source=4(Remote)`, `key_id` 从本地缓存查找
- SDK `ListKeys` 内部: 从 Hub 的 `DigitalKey` 转成 `KeyItem`（补 vehicle_name 缓存）
