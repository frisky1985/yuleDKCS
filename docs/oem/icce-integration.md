# ICCE OEM 对接文档 — yuleDKCS × 华为 ICCE

> **文档版本**: 1.0.0  
> **创建日期**: 2026-07-16  
> **协议标准**: ICCE T/CA 110-2020  
> **系统版本**: yuleDKCS v1.0.0  

---

## 1. ICCE 协议概述

### 1.1 标准信息

| 项目 | 内容 |
|:-----|:------|
| 标准全称 | T/CA 110-2020《基于智能移动终端的车辆数字钥匙技术规范》 |
| 标准组织 | 中国汽车工程学会 / 华为 ICCE 联盟 |
| 协议版本 | ICCE Digital Key 2.0（接口层） |
| 加密体系 | 国密 SM2/SM3/SM4（可选支持国际算法） |
| BLE UUID | 0xFEFA 系列（ICCE 专用服务 UUID） |
| 近场通信 | BLE（必选）+ NFC（可选）+ UWB（可选） |

### 1.2 ICCE 协议栈架构

```
┌─────────────────────────────────────────────┐
│            ICCE Application Layer           │
│   钥匙管理 · 车辆控制 · 安全认证 · 生态     │
├─────────────────────────────────────────────┤
│            ICCE Security Layer              │
│   SM2 签名/验签 · SM3 哈希 · SM4 加密       │
├─────────────────────────────────────────────┤
│            ICCE Transport Layer             │
│   BLE GATT (0xFEFA) / NFC ISO 7816-4       │
├─────────────────────────────────────────────┤
│            ICCE Edge Computing Layer        │
│   边缘触发 · 条件评估 · 智能解锁决策         │
└─────────────────────────────────────────────┘
```

yuleDKCS 中 ICCE 协议栈覆盖全栈架构：
- **车端**: `embedded/icce_protocol/` — 完整的 ICCE 协议栈实现（5 状态 FSM 边缘引擎已于 P0-2 修复为真实 944 行实现）
- **移动端**: Android SDK / iOS SDK 中的 ICCE BLE 自适应层
- **云端**: `adapter-icce` Java Adapter 对接 OEM ICCE TSP API

---

## 2. ICCE TSP API 对接要求

### 2.1 接口总览

yuleDKCS ICCE Adapter 需要 OEM TSP 暴露以下 API 端点（基于行业通用 ICCE 对接实践）：

| 序号 | 接口 | 方法 | 路径 | 用途 | 优先级 |
|:----:|:-----|:----:|:-----|:-----|:------:|
| 1 | 密钥分发 | POST | `/api/v1/keys` | 申请签发新数字钥匙 | P0 |
| 2 | 密钥派生 | POST | `/api/v1/keys/bind` | 将钥匙绑定到设备（含 ECDH 密钥协商） | P0 |
| 3 | 钥匙状态同步 | GET | `/api/v1/keys/{keyId}` | 查询钥匙当前状态 | P0 |
| 4 | 钥匙吊销 | POST | `/api/v1/keys/revoke` | 吊销指定的数字钥匙 | P0 |
| 5 | 钥匙解绑 | POST | `/api/v1/keys/unbind` | 解除钥匙与设备的绑定关系 | P0 |
| 6 | 车辆信息查询 | GET | `/api/v1/vehicles` | 获取用户车辆列表及信息 | P0 |
| 7 | 车辆控制指令 | POST | `/api/v1/vehicles/{vin}/command` | 远程车辆控制（解锁/闭锁/开窗等） | P1 |
| 8 | 事件通知 | POST | (callback) | 车辆事件推送（钥匙使用记录、OTA 状态） | P1 |
| 9 | 密钥状态变更通知 | POST | (callback) | TSP 主动推送钥匙状态变更事件 | P1 |

### 2.2 接口一：密钥分发（Key Issuance）

```
POST /api/v1/keys
```

**请求体：**

```json
{
  "userId": "string",
  "vehicleId": "string",
  "vin": "string",
  "keyType": "enum(OWNER | SUB_KEY | TEMPORARY | VALET)",
  "validFrom": 1710000000,
  "validTo": 1799999999,
  "maxUses": 100,
  "permissions": [
    "UNLOCK", "LOCK", "START_ENGINE", "OPEN_TRUNK"
  ],
  "geoFence": {
    "lat": 31.2304,
    "lng": 121.4737,
    "radius": 5000
  },
  "signature": "base64(sm2_sign(userId||vehicleId||validTo))"
}
```

**响应体：**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "keyId": "icce-key-uuid",
    "keyData": ["base64-encoded-key-material-packets"],
    "keySlot": 1
  }
}
```

**验证要点：**
- [ ] 返回 `keyData` 数组非空
- [ ] `keySlot` 在 1–5 范围内
- [ ] `keyData` 包体使用 BER-TLV 编码
- [ ] 请求中的 `signature` 使用 SM2 算法

### 2.3 接口二：钥匙绑定（Key Binding）

```
POST /api/v1/keys/bind
```

这是 ICCE 对接中最关键的接口。它执行设备与钥匙的安全绑定，包含 ECDH 密钥协商。

**请求体：**

```json
{
  "userId": "string",
  "vehicleId": "string",
  "vin": "string",
  "deviceId": "string",
  "keyId": "string",
  "devicePublicKey": "base64(sm2_device_ephemeral_pubkey)",
  "attestationToken": "base64(device_attestation)",
  "signature": "base64(sm2_sign(userId||keyId||deviceId))"
}
```

**响应体：**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "keyId": "icce-key-uuid",
    "sharedSecret": "base64(ecdh_shared_secret)",
    "tspPublicKey": "base64(tsp_ephemeral_pubkey)",
    "sessionId": "session-uuid",
    "keySlot": 1,
    "keyData": ["base64-encoded-key-material"]
  }
}
```

**⚠️ 关键要求：**
- `sharedSecret` 必须由 TSP 使用 SM2 ECDH 派生，**不允许为空**
- `tspPublicKey` 必须为有效的 SM2 公钥
- `signature` 必须使用 SM2 算法

**验证要点：**
- [ ] `sharedSecret` 非空且为有效 base64 编码
- [ ] `tspPublicKey` 返回设备方可以验证的公钥
- [ ] `sessionId` 唯一，后续状态查询可用

### 2.4 接口三：钥匙状态同步

```
GET /api/v1/keys/{keyId}
```

**响应体：**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "keyId": "icce-key-uuid",
    "status": "enum(ACTIVE | PENDING | REVOKED | EXPIRED)",
    "boundDeviceId": "device-uuid",
    "createdAt": 1710000000,
    "expiresAt": 1799999999,
    "lastUsedAt": 1715000000,
    "metadata": {}
  }
}
```

**状态定义：**

| 状态 | 含义 | 可操作 |
|:-----|:------|:------:|
| `ACTIVE` | 钥匙有效可正常使用 | ✅ 可解锁、可分享 |
| `PENDING` | 等待设备绑定完成 | ✅ 仅可绑定 |
| `REVOKED` | 已被吊销 | ❌ 不可使用 |
| `EXPIRED` | 已过期 | ❌ 不可使用 |

### 2.5 接口四：钥匙吊销

```
POST /api/v1/keys/revoke
```

**请求体：**

```json
{
  "keyId": "icce-key-uuid",
  "reason": "enum(LOST | STOLEN | OWNER_REQUEST | ADMIN)",
  "userId": "string"
}
```

### 2.6 接口五：钥匙解绑

```
POST /api/v1/keys/unbind
```

**请求体：**

```json
{
  "keyId": "icce-key-uuid",
  "deviceId": "device-uuid",
  "reason": "string"
}
```

### 2.7 接口六：车辆信息查询

```
GET /api/v1/vehicles?userId={userId}
```

**响应体：**

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "vehicles": [
      {
        "vehicleId": "string",
        "vin": "WBA3A5C5XDF123456",
        "brand": "OEM",
        "modelName": "Model X",
        "year": 2026,
        "icceSupported": true
      }
    ]
  }
}
```

### 2.8 接口七：车辆控制指令（P1）

```
POST /api/v1/vehicles/{vin}/command
```

**请求体：**

```json
{
  "command": "enum(UNLOCK | LOCK | OPEN_TRUNK | START | STOP)",
  "keyId": "icce-key-uuid",
  "signature": "base64(sm2_sign(command||keyId||timestamp))",
  "timestamp": 1715000000,
  "nonce": "random-nonce"
}
```

---

## 3. ICCE 国密合规要求

### 3.1 国密算法要求

| 算法 | 标准 | 密钥长度 | 用途 | yuleDKCS 状态 |
|:-----|:-----|:--------:|:-----|:-------------:|
| **SM2** | GB/T 32918.1-2016 | 256 bit | 数字签名、密钥协商 | ⚠️ 软件回退路径存在（`#ifdef USE_SM_CRYPTO`），P1 待完成 |
| **SM3** | GB/T 32905-2016 | 256 bit | 哈希 | ⚠️ 同上 |
| **SM4** | GB/T 32907-2016 | 128 bit | 对称加密 | ⚠️ 同上 |

### 3.2 GM/T 0028 安全模块要求

GM/T 0028《密码模块安全技术要求》是 ICCE 对接时 OEM 经常审计的标准：

| 要求项 | 要求描述 | yuleDKCS 满足情况 |
|:-------|:---------|:-----------------:|
| 密钥生命周期管理 | 密钥生成、存储、使用、销毁全程安全 | ✅ 5 级密钥层级 |
| 物理安全 | 密钥材料不得以明文形式离开安全硬件 | ✅ SE050（CC EAL 6+） |
| 算法库认证 | SM2/SM3/SM4 算法库需通过国家密码管理局认证 | ⚠️ 待纳入商业 GM 库 |
| 随机数质量 | 需使用硬件 TRNG | ⚠️ DEV ONLY 回退，生产需接入 TRNG |
| 日志审计 | 关键密码操作需记录审计日志 | ✅ Hub 事件系统 |

### 3.3 ICCE 密钥派生流程

```
 手机端                               TSP/云端
   │                                   │
   │  SM2 生成临时密钥对 (eph_sk, eph_pk)│
   │  eph_pk ──────────────────────────→│  接收设备临时公钥
   │                                   │  SM2 ECDH: shared_secret = SM2_KA(eph_pk, tsp_sk)
   │  shared_secret = SM2_KA(tsp_pk, eph_sk)
   │  派生: MK = SM3_KDF(shared_secret, "ICCE-Key-Master-Key")
   │  派生: SK = SM3_KDF(MK, "ICCE-Key-Session-Key")
   │                                   │
   │  ←── TSP 返回 shared_secret(已加密)│
   │                                   │
```

---

## 4. ICCE BLE 协议对接

### 4.1 ICCE BLE Service UUID

| 服务 | UUID | 角色 |
|:-----|:-----|:-----|
| ICCE Digital Key Service | `0000FEFA-0000-1000-8000-00805F9B34FB` | Server (车端) |
| ICCE Control Point | `0000FEFB-0000-1000-8000-00805F9B34FB` | 指令通道 |
| ICCE Data Stream | `0000FEFC-0000-1000-8000-00805F9B34FB` | 数据通道 |
| ICCE Battery Service | `0000180F-0000-1000-8000-00805F9B34FB` | 电池状态 |

### 4.2 BLE GATT 操作映射

| ICCE 操作 | GATT 操作 | 编码格式 | 优先级 |
|:----------|:---------|:---------|:------:|
| 车辆搜索 | Advertisement + Scan Response | BLE AD Structure | P0 |
| 配对握手 | Read/Write on Control Point | BER-TLV | P0 |
| 钥匙绑定 | Write on Data Stream | BER-TLV + SM2 签名 | P0 |
| 车辆解锁 | Write on Control Point | BER-TLV + SM4 加密 | P0 |
| 车辆锁定 | Write on Control Point | BER-TLV + SM4 加密 | P0 |
| 状态同步 | Notification on Data Stream | BER-TLV | P1 |
| OTA 传输 | Long Write on Data Stream | BER-TLV (分片) | P1 |

---

## 5. 对接 CheckList（25 项）

### 5.1 环境与凭据（5 项）

- [ ] **I-P1** OEM 提供 ICCE TSP API 端点（HTTPS URL）
- [ ] **I-P2** OEM 提供 API Key 与 Tenant ID 凭据
- [ ] **I-P3** 网络连通性验证：`curl -I https://icce-tsp.oem.com` 返回 200
- [ ] **I-P4** TLS 版本验证：≥ TLS 1.2，推荐 TLS 1.3
- [ ] **I-P5** 确认 API 速率限制策略（提前申请 QPS 配额）

### 5.2 核心接口（12 项）

- [ ] **I-C1** `getVehicles` 接口返回正确的车辆列表（至少 1 台支持 ICCE）
- [ ] **I-C2** `requestKeys` 成功签发钥匙，`keyData` 非空
- [ ] **I-C3** `requestKeys` 中的 `keySlot` 在有效范围内（1–5）
- [ ] **I-C4** `bindKey` 调用成功，`sharedSecret` 非空且可用
- [ ] **I-C5** `bindKey` 返回的 `tspPublicKey` 为有效 SM2 公钥
- [ ] **I-C6** `bindKey` 返回的 `sessionId` 唯一
- [ ] **I-C7** `getKeyStatus` 对已绑定钥匙返回 `ACTIVE`
- [ ] **I-C8** `getKeyStatus` 对吊销钥匙返回 `REVOKED`
- [ ] **I-C9** `revokeKeys` 成功吊销钥匙，状态变更为 `REVOKED`
- [ ] **I-C10** `unbindKey` 成功解绑钥匙
- [ ] **I-C11** 对已吊销钥匙的解锁指令返回错误
- [ ] **I-C12** 所有接口使用 `signature` 字段且 SM2 验签通过

### 5.3 国密合规（4 项）

- [ ] **I-G1** SM2 签名生成与验证算法对齐（交换测试向量验证）
- [ ] **I-G2** SM3 哈希结果正确（对齐国标测试数据）
- [ ] **I-G3** SM4 加密/解密算法参数一致（ECB/CBC/GCM 模式确认）
- [ ] **I-G4** ECDH 密钥协商（SM2 椭圆曲线）派生 shared_secret 一致

### 5.4 验证测试（4 项）

- [ ] **I-V1** 指数退避重试验证：TSP 返回 503，Adapter 自动重试 3 次
- [ ] **I-V2** 错误码映射验证：TSP 错误码正确转换为 yuleDKCS 内部错误码
- [ ] **I-V3** 超时测试：TSP 无响应 35s，Adapter 正确超时并返回错误
- [ ] **I-V4** 防重放测试：重复使用 Nonce 拒绝解锁

---

## 6. ICCE Adapter 配置参考

### 6.1 application-icce.yml

```yaml
adapter:
  icce:
    enabled: true
    api-url: ${ICCE_API_URL:https://icce-tsp.oem.com}
    api-key: ${ICCE_API_KEY}
    tenant-id: ${ICCE_TENANT_ID}
    connection-timeout: 10000
    read-timeout: 30000
```

### 6.2 启动与健康检查

```bash
# 启动 ICCE Adapter（含 gRPC Server）
java -jar adapter-grpc-server.jar \
  --spring.profiles.active=icce,prod

# 健康检查
curl http://localhost:8080/actuator/health

# 预期返回（适配器已启用且连接正常）
# {"status":"UP","adapters":{"total":1,"enabled":1,"icce":"UP"}}
```

### 6.3 日志分类

```yaml
logging:
  level:
    com.digitalkey.adapter.icce: DEBUG      # ICCE 适配器
    com.digitalkey.adapter.core: INFO       # 核心框架
```

---

## 7. 常见问题

### Q1: ICCE SM2 尚未完整实现，POC 期间如何处理？

A: 使用 P-256 + SHA-256 模拟方案。测试通过后，升级为国密算法库（`github.com/tjfoc/gmsm` / 商业 GM 库）。yuleDKCS 车端已预留 `#ifdef USE_SM_CRYPTO` 编译开关。

### Q2: ICCE 边缘引擎需要什么外部依赖？

A: 需要主循环提供 `sys_tick_get_ms()` 系统时钟，以及可选的 RTC 时间同步。车端 FreeRTOS 系统 tick 即可满足基本需求。

### Q3: ICCE TSP API 必须完全符合规范吗？

A: 建议尽量对齐。实际联调中可接受 80% 对齐，适配器层负责做字段映射和转换。

### Q4: ICCE 车端模拟器如何获取？

A: yuleDKCS 提供 Docker 版车端模拟器（`docker-compose-poc.yml` 中包含 `tcu-simulator` 服务），支持 ICCE BLE 协议栈仿真。

---

*本文档应与 [POC 联调指南](poc-guide.md)、[Demo 脚本](demo-script.md) 配套使用。*
