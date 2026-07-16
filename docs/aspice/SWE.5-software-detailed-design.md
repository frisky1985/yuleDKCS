# SWE.5 — 软件详细设计文档

> **项目**: yuleDKCS 数字钥匙系统
> **版本**: 1.0.0 | **日期**: 2026-07-07 | **状态**: 初版
> **关联**: SWE.4 (架构设计), spec-contract.md, TEST-PLAN.md
> **模块范围**: 车端嵌入式 (C) + 云端 Go/Java + 移动端

---

## 1. 车端嵌入式模块详细设计

### 1.1 防中继模块（`embedded/src/security/anti_relay.c`）

**职责**: UWB 测距 + BLE RSSI 多因子交叉验证，判定中继攻击。

**接口规范**:
| 函数 | 签名 | 说明 |
|:-----|:-----|:-----|
| `anti_relay_evaluate()` | `(distance_cm, rssi_dbm, counter, nonce) → decision_t` | 综合多因子输出判定决策 |
| `anti_relay_challenge()` | `(challenge_buf, len) → err_t` | 生成一次性随机挑战 |
| `anti_relay_verify_response()` | `(response_buf, challenge_buf) → bool` | 验证响应签名 |

**数据流**: UWB ToF 测距 → 距离转换 → BLE RSSI 采样 → 交叉校验 → 决策输出。

**错误处理**: 测距超时（>100ms）→ `DECISION_DENY_TIMEOUT`；Nonce 重复 → `DECISION_DENY_REPLAY`；RSSI 与 UWB 显著不一致 → `DECISION_DENY`。

### 1.2 ICCE 协议栈（`embedded/icce_protocol/`）

**状态机**: `Created → Pre-Paired → Paired → Active`（配对）；`Active → Updated/Revoked/Deleted`（生命周期）。

**接口规范**:
| 函数 | 签名 | ASIL |
|:-----|:-----|:-----|
| `icce_security_auth()` | `(peer_cert, challenge) → bool` | ASIL-B |
| `icce_unlock_vehicle()` | `(nonce, signature) → err_t` | ASIL-B |
| `icce_engine_start_auth()` | `(challenge, response) → bool` | ASIL-B |

**数据流**: BLE 连接建立 → UWB 测距开始 → 双向认证 → 解锁/启动指令 → CAN FD 输出。

**错误处理**: 签名验证失败 → 返回 `ERR_AUTH_FAILED`；安全通道超时 → 返回 `ERR_TIMEOUT`；阈值距离 > 2m → 返回 `ERR_DISTANCE_EXCEEDED`。

### 1.3 CCC 协议栈（`embedded/ccc_protocol/`）

**接口规范**:
| 函数 | 签名 | ASIL |
|:-----|:-----|:-----|
| `ccc_sec_verify()` | `(signature, pubkey, digest) → bool` | ASIL-B |
| `ccc_dk_core_process()` | `(tlv_msg) → err_t` | ASIL-B |

**算法**: ECDSA P-256 签名验证、AES-256-GCM 加密通信、ECDH P-256 密钥协商。

**数据流**: BLE GATT 服务发现 → CCC 应用选择 → 密钥协商 → 安全通道加密通信。

**错误处理**: `[待确认: CCC 错误码定义当前有 Default/Unlock mode 错误状态，但缺少完整错误码枚举]`。

### 1.4 HAL 层（`embedded/unified_hal/`）

| HAL 模块 | 关键 API | 说明 |
|:---------|:---------|:-----|
| hal_ble | `hal_ble_connect()`, `hal_ble_send()`, `hal_ble_recv()` | BLE 驱动抽象，MTU ≥ 512 bytes |
| hal_uwb | `hal_uwb_start_ranging()`, `hal_uwb_get_distance()` | UWB ToF 测距 |
| hal_nfc | `hal_nfc_select_aid()`, `hal_nfc_exchange_apdu()` | ISO 14443 APDU 交换 |
| hal_sec | `hal_sec_sign()`, `hal_sec_verify()`, `hal_sec_key_derive()` | SE050 密码学操作抽象 |

## 2. 云端模块详细设计

### 2.1 Hub 网关（`backend/cloud/hub/`）

**职责**: API 网关 + JWT 鉴权 + 请求路由 + 限流熔断。

**接口规范**:
| 端点 | 方法 | 职责 |
|:-----|:-----|:-----|
| `/api/v1/auth/*` | POST/GET | 用户认证与会话 |
| `/api/v1/keys/*` | POST/GET/DELETE | 钥匙生命周期管理 |
| `/api/v1/vehicles/*` | POST/GET | 车辆绑定与控制 |

**数据流**: App → HTTPS/TLS 1.3 → Hub → gRPC → DKCS → MQTT → 车端。

**错误处理**: JWT 过期 → 401；无权限 → 403；速率超限 → 429；HSM 不可用 → 503。

### 2.2 DKCS 核心服务（`backend/cloud/dkcs/`）

**职责**: 密钥生命周期状态机、车控指令下发、事件流处理。

**接口规范**:
| gRPC Service | 方法 | 说明 |
|:-------------|:-----|:-----|
| KeyService | `CreateKey`, `ActivateKey`, `RevokeKey`, `ShareKey` | 密钥 CRUD |
| CommandService | `Unlock`, `Lock`, `StartEngine`, `FindCar` | 车控指令 |
| EventService | `RecordEvent`, `StreamPush` | 事件日志 |

**数据流**: Hub → gRPC → DKCS 业务层 → MQTT/DB → 反馈。

**错误处理**: `[待确认: Kafka 消息队列未使用 (GO-P0-02)，事件驱动架构不可用]`。

### 2.3 Java 适配器（`backend/adapters/`）

**职责**: 各手机厂商协议适配，统一 gRPC 接口。

**设计**:
```java
abstract class AbstractTspAdapter {
    // 所有适配器继承此基类，实现各自厂商协议
    void onKeyProvisioning(KeyRequest req);
    void onKeyRevocation(KeyId keyId);
    void onCommandDispatch(Command cmd);
}
```

## 3. 移动端模块详细设计

### 3.1 Android SDK（`frontend/android/`）

| 层 | 关键类 | 职责 |
|:---|:-------|:-----|
| API | `DigitalKeyClient`, `KeyManager`, `VehicleManager` | SDK 统一入口 |
| Communication | `BleManager`, `NfcManager`, `UwbManager` | BLE/UWB/NFC 通信封装 |
| Security | `SecureStorage(KeyStore)`, `CryptoEngine` | 密钥安全存储与密码学运算 |
| Infrastructure | `DkError`, `DkLogger`, `DkTelemetry` | 基础设施 |

### 3.2 iOS SDK（`frontend/ios/`）

| 层 | 关键类 | 职责 |
|:---|:-------|:-----|
| API | `DigitalKeyClient`, `KeyManager`, `VehicleManager` | SDK 统一入口 |
| Communication | `BleManager(CoreBluetooth)`, `NfcManager(CoreNFC)`, `UwbManager(NearbyInteraction)` | 通信封装 |
| Security | `SecureStorage(Keychain)`, `CryptoEngine(CryptoKit)` | 安全存储与密码学 |
| Infrastructure | `DkError`, `DkLogger`, `DkTelemetry` | 基础设施 |

## 4. 错误处理策略

### 4.1 通用原则

| 等级 | 处理方式 | 示例 |
|:-----|:---------|:-----|
| 致命错误 | 系统锁定 + 告警日志 | SE050 断开、安全启动失败 |
| 业务错误 | 返回错误码 + 重试 | 签名验证失败、超时 |
| 可恢复错误 | 自动重试 + 退避 | BLE 断连重连、MQTT 离线 |
| 日志错误 | 记录告警，不影响业务 | 审计日志写入失败 |

### 4.2 错误码框架 `[待确认: 三端错误码体系一致性]`

| 范围 | 端 | 说明 |
|:-----|:----|:-----|
| 0x00–0x0F | 通用错误 | `SUCCESS`, `ERR_GENERAL`, `ERR_TIMEOUT` |
| 0x10–0x1F | ICCE 协议 | `ERR_ICCE_AUTH`, `ERR_ICCE_BIND` |
| 0x20–0x2F | CCC 协议 | `ERR_CCC_VERIFY`, `ERR_CCC_KEY` |
| 0x30–0x3F | ICCOA 协议 | `[待确认: ICCOA 错误码]` |
| 0x40–0x4F | HAL 层 | `ERR_HAL_BLE`, `ERR_HAL_UWB`, `ERR_HAL_SE` |
| 0x50–0x5F | 云端 | `ERR_JWT`, `ERR_PERMISSION`, `ERR_HSM` |

## 5. 数据流描述

### 5.1 无感解锁数据流

```
App (BLE广告侦听) → 靠近 ≤ 2m → BLE连接建立 → UWB测距开始
→ App发送认证挑战 → 车端SE050验证签名
→ 多因子交叉校验 → 决策ALLOW → CAN FD解锁指令 → BCM执行
→ App收到成功通知 → 状态更新
```

### 5.2 钥匙分享数据流

```
车主App → 选择分享类型/约束 → HTTPS → Hub → DKCS创建分享记录
→ 生成分享链接/二维码 → 发送给受邀者
→ 受邀者注册 → 接受分享 → DKCS生成副钥匙 → 推送至车端
→ 车端SE050存储副钥匙公钥 → 邀请完成
```

### 5.3 远程控车数据流

```
App → JWT认证 → 签名远程指令 → MQTT over TLS 1.3 → 云端
→ 权限校验 → 协议转换 → MQTT下发至车端
→ 车端验证签名 → 执行CAN指令 → 反馈结果 → 云端记录审计日志
```
