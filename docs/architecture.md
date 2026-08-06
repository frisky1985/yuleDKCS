# 软件架构设计文档 (Software Architecture)

> **项目**: yuleDKCS 数字钥匙系统
> **文档编号**: yuleDKCS-ARCH-001
> **版本**: 1.0.0 | **日期**: 2026-08-06 | **状态**: APPROVED (基线)
> **过程域**: ASPICE SWE.2 (Software Architectural Design)
> **来源提炼**: `docs/design/DK-HUB-ARCHITECTURE.md`、`docs/design/HUB-DETAILED-DESIGN.md`、`docs/aspice/SWE.4-software-arch.md`、`docs/SYSTEM_ARCHITECTURE.md`
> **关联**: `docs/software-requirements.md` (SRS), `docs/impact-analysis.md` (IA-001)

---

## 1. 引言

### 1.1 目的与范围

本文档定义 yuleDKCS 数字钥匙系统的软件架构：组件边界、组件间接口与数据流，并映射至软件需求（REQ-001~040）。架构遵循两条主线：

1. **云端**：App/手机钱包 → **Digital Key Hub（授权决策层）** → **OEM DK Server（密钥材料层）** → TSP/TCU/车端。Hub 只做授权决策，**不触碰密钥材料**（密钥生成/存储归车厂 PKI/KMS）。
2. **车端**：SWC / RTE / BSW / MCAL 四层架构，三协议栈（ICCE/CCC/ICCOA）+ 统一 HAL + SE050 安全芯片。

### 1.2 参考

- `docs/design/DK-HUB-ARCHITECTURE.md`（Hub 架构方案，303 行）
- `docs/design/HUB-DETAILED-DESIGN.md`（Hub 模块级详细设计，1012 行）
- `docs/aspice/SWE.4-software-arch.md`（车端四层架构）
- `docs/SYSTEM_ARCHITECTURE.md`、`docs/safety-concept.md`

---

## 2. 架构概览

### 2.1 系统上下文

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           手机厂商端 (App/钱包)                          │
│   Apple Wallet (CCC) · Samsung Pass (CCC) · 小米/OPPO (ICCOA) · ICCE App │
└───────────────┬──────────────────┬──────────────────┬───────────────────┘
                │ HTTPS/REST+JWT   │ HTTPS/REST+JWT   │ HTTPS/REST+JWT
                ▼                  ▼                  ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                    Digital Key Hub（授权决策层 / 云端）                   │
│  ┌────────────┐ ┌────────────┐ ┌────────────┐ ┌──────────────────────┐  │
│  │ REST/GRPC  │ │ Token      │ │ Service    │ │ Protocol Adapter     │  │
│  │ Gateway    │ │ Manager    │ │ Layer      │ │ (CCC/ICCOA/ICCE +     │  │
│  │ (JWT/限流) │ │ (签发/吊销) │ │ (编排业务) │ │  Unified BERTLV)     │  │
│  └─────┬──────┘ └─────┬──────┘ └─────┬──────┘ └──────────┬───────────┘  │
│        │              │              │                   │              │
│  ┌─────▼──────────────▼──────────────▼───────────────────▼───────────┐  │
│  │ HubTransport: gRPC 双向流 + BERTLV 编解码 + HMAC 签名/验签          │  │
│  └───────────────────────────────────────────────────────────────────┘  │
│   Logger(审计, Kafka, ≥3年) · Redis(缓存/黑名单) · Kafka(事件) · TiDB    │
└──────────────────────────┬──────────────────────────────────────────────┘
                           │ gRPC 双向 TLS (mTLS)
                           ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                        OEM DK Server（密钥材料层）                        │
│   密钥对生成/证书签发 · SE050 写入编排 · KMS 集成 (AWS KMS/Azure KV/HSM)   │
│                     │                                                  │
│                     ▼ MQTT (TLS 1.3, BERTLV payload)                   │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │  车端 TCU / 嵌入式系统 (SWC/RTE/BSW/MCAL 四层)                    │  │
│  │  ICCE 栈 · CCC 栈 · ICCOA 栈 · unified_hal · SE050 · 电源/启动链   │  │
│  │  BLE (KW47A) · UWB (NCJ29D6) · NFC (ST25R501)                    │  │
│  └──────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────┘
```

### 2.2 组件边界总览

| 组件 | 归属 | 职责 | 不做 |
|:-----|:-----|:-----|:-----|
| Digital Key Hub | 云端 | 授权决策、协议转换、钥匙编排、审计 | ❌ 不生成/存储密钥材料 |
| OEM DK Server | 车厂云 | 密钥对生成、证书签发、SE050 写入 | — |
| DKCS Core | 云端 | 密钥编排（KeyBind/Unbind/Revoke/List/Share/Command） | 密钥材料下沉 KMS |
| 车端三协议栈 | 车端 | ICCE/CCC/ICCOA 协议实现 | — |
| 统一 HAL | 车端 | hal_ble/hal_uwb/hal_nfc/hal_sec 抽象 | — |
| SE050 | 硬件 | 密钥存储与密码学运算 | — |
| App SDK | 移动端 | 通道选择、密钥管理、控车 API | — |

---

## 3. 组件分解与边界

### 3.1 云端 Hub 内部组件（`backend/cloud/hub/internal/`）

```
┌─ API Gateway 层 ────────────────────────────────────────────────┐
│  gateway/rest_gateway.go    — REST (HTTPS/TLS 1.3, JWT, 限流/熔断)│
│  gateway/token_handler.go   — Token 签发/验证                    │
│  gateway/device_handlers.go — 设备相关端点                       │
├─ 认证授权层 ─────────────────────────────────────────────────────┤
│  token/token.go — Access/Refresh Token 签发·验证·刷新·吊销(黑名单)│
├─ 业务服务层 ─────────────────────────────────────────────────────┤
│  service/key_management.go  — 钥匙编排 (Create/Activate/Revoke/…) │
│  service/key_share.go       — 钥匙分享 (Create/Accept/Revoke/List)│
│  service/vehicle_control.go — 车控指令 + 状态查询                 │
│  service/device_service.go  — 设备注册/能力/列表/移除             │
│  service/unified_key_service.go — 统一钥匙编排                    │
│  service/hub_transport.go   — Hub↔DKCS 通信 (gRPC + BERTLV)      │
│  service/dk_server.go       — DK Server 代理 + TspAdapter 注册中心│
├─ 协议层 ─────────────────────────────────────────────────────────┤
│  unified/ (protocol/router/manager/device/state/codec) — BERTLV  │
│  codec/bertlv/tags.go       — Tag 定义 (E1 01…E1 FF, A0 01…)     │
├─ 横切层 ─────────────────────────────────────────────────────────┤
│  logger/ (审计日志, Kafka) · telemetry/ · error/ (错误码)         │
└──────────────────────────────────────────────────────────────────┘
```

**组件依赖**（设计文档 §1.2）: Gateway → Token/Service；Service → Protocol Adapter/Logger/TiDB；Protocol Adapter → Unified Protocol；Logger → Kafka。

### 3.2 车端嵌入式（`embedded/`）四层架构

| 层 | 模块 | 路径 | ASIL |
|:---|:-----|:-----|:-----|
| SWC | VehicleAccess / KeyManagement / SecurityService | `embedded/icce_protocol/` `embedded/ccc_protocol/` `embedded/iccoa_protocol/` | ASIL-B |
| BSW | ICCE/CCC/ICCOA 协议栈 | 同上各栈核心 | ASIL-B |
| BSW | 统一 HAL: hal_ble/hal_uwb/hal_nfc/hal_sec | `embedded/unified_hal/` | QM / ASIL-B(D) |
| BSW | SE050 驱动 / TFM | `embedded/*/se050_*`, `embedded/tfm/` | ASIL-B |
| MCAL | CAN/SPI/I2C/UART 驱动 | `embedded/mcal_stubs/` | QM |

> ⚠️ BSW 服务（CSM/NvM/DCM/DEM/COM/WdgM/EcuM/OS）为 `[待集成]` 状态，当前 RTE 为裸机/RTOS — 见 ADR-04。

### 3.3 Frontend（`frontend/`）

- Android SDK（min SDK 26）: KeyManager/VehicleController/ShareManager/ChannelManager/SecurityModule。
- iOS SDK（min iOS 14.0）: KeyManaging/VehicleControlling/ShareManaging/ChannelManaging/SecurityManaging。
- REST API / gRPC 4 组微服务 / 9 个 MQ Topic（REQ-036~040）。

---

## 4. 接口定义（Interface Specification）

### 4.1 外部接口

| 接口 | 协议 | 方向 | 数据类型/范围 | 需求 |
|:-----|:-----|:-----|:--------------|:-----|
| App↔Hub | HTTPS REST + JWT Bearer + HMAC-SHA256 | 双向 | JSON; 统一响应 {code,message,data,requestId,timestamp}; TLS 1.3 | REQ-026, REQ-038 |
| Hub↔DKCS | gRPC 双向流 + BERTLV + HMAC-SHA256 Trailer | 双向 | 消息信封 E1 01/Body/Trailer E1 FF; Header: Version(BCD) Timestamp(N14) MessageType(N4) SequenceNo(N8) DeviceId(AN16) | REQ-010~018, REQ-024 |
| DKCS↔TCU | MQTT over TLS 1.3, mTLS | 双向 | Topic `digitalkey/{tsp_id}/{vehicle_id}/{resource}/{action}`; QoS 2/1/0; BERTLV payload | REQ-025 |
| 车端↔手机 | BLE GATT (0xFFD1) / UWB 802.15.4z / NFC 13.56MHz | 近场 | CCC GATT Profile; UWB STS; ISO 7816-4 APDU | REQ-028~032 |
| Hub↔OEM DK Server | gRPC 双向 TLS | 双向 | TspAdapter ForwardBind/Unbind/Command/Share | REQ-010~015 |
| 内部微服务 | gRPC (KeyService/VehicleService/KMSService/EventService) | 内部 | Protobuf `digitalkey/` 包 | REQ-039 |
| 消息队列 | Kafka + Avro Schema Registry | 内部 | 9 topics; 隔离 consumer group | REQ-040 |

### 4.2 内部接口（Go 接口契约，详见 HUB-DETAILED-DESIGN.md §2~§6）

| 接口 | 方法集 | 位置 |
|:-----|:-------|:-----|
| TokenManager | IssueAccessToken / IssueRefreshToken / ValidateAccessToken / RefreshToken / RevokeToken / IsTokenRevoked | `internal/token/token.go` |
| TspAdapter | GetVendor / GetProtocols / ForwardBind / ForwardUnbind / ForwardCommand / ForwardShare | `internal/service/dk_server.go` |
| AdapterRegistry | Register / Get / List / Remove（key 自动 lowercase — REQ-019） | 同上 |
| KeyManagementService | CreateKey / ActivateKey / RevokeKey / SuspendKey / ResumeKey / ListKeys / GetKeyDetail | `internal/service/key_management.go` |
| DeviceService | RegisterDevice / GetDevice / ListDevices / RemoveDevice / UpdateDeviceCapabilities | `internal/service/device_service.go` |
| VehicleControlService | SendCommand / GetVehicleStatus / GetCommandHistory | `internal/service/vehicle_control.go` |
| KeyShareService | CreateShare / AcceptShare / RevokeShare / ListShares / GetShareDetail | `internal/service/key_share.go` |
| AuditLogger | Log / LogSync / Query / Export（审计保留 ≥3 年 — REQ-006-S8） | `internal/logger/logger.go` |
| Codec | Encode / Decode / EncodeBody / DecodeBody（BERTLV） | `internal/unified/codec.go` |
| MessageRouter | MessageType → MessageHandler 注册表（1000/1002/1004/1010/2000/3000/3002/9000） | `internal/unified/router.go` |

### 4.3 车端接口头文件清单（8 处 include/ 目录 + freestanding 头，41 个头文件）

| # | 头文件目录 | 关键头文件 | 接口内容 | 数据类型/范围 |
|:-:|:-----------|:-----------|:---------|:--------------|
| 1 | `firmware/include/` | `fw_version.h` | 固件版本接口 | FW_VERSION "2.1.0" (MAJOR 2/MINOR 1/PATCH 0) |
| 2 | `embedded/unified_protocol/include/` | `dk_unified.h` | 统一协议接口（EMB-SWC-UNIFIED） | 消息类型码 1000~9000; 权限位 16-bit |
| 3 | `embedded/ccc_protocol/include/` | `ccc_digital_key.h` | CCC 协议栈主接口 | CCC DK 3.0 GATT Profile; UUID 0xFFD1 |
| 4 | `embedded/ccc_protocol/include/` | `se050_scp03.h` | SCP03 安全通道 | 会话密钥 AES-128/256; 通道计数 3 |
| 5 | `embedded/ccc_protocol/include/` | `hardware_abstraction.h` | 硬件抽象层 (MCAL) | BLE/UWB/NFC 外设句柄; 错误码 int |
| 6 | `embedded/ccc_protocol/include/` | `crypto_random.h` | 安全随机数 (TRNG) | 熵源 ≥ 256-bit; 输出 buffer |
| 7 | `embedded/icce_protocol/include/` | `icce_digital_key.h` | ICCE 类型/枚举/公共 API | SM2/SM3/SM4; 帧格式 SOP+CMD+SEQ+LEN+PAYLOAD+CHK+EOP |
| 8 | `embedded/icce_protocol/include/` | `security_auth.h` | 安全认证模块 | 挑战值 16B; 签名 ECDSA/SM2 |
| 9 | `embedded/iccoa_protocol/include/` | `iccoa_digital_key.h` | ICCOA 协议栈主接口 | GATT 0xFEF5; DK3.0/4.0 帧; 8 权限位 |
| — | `embedded/bsw_integration/include/` | `Com_Cfg_Dk.h` 等 10 个 | BSW 集成配置头（OS/COM/DCM/DEM/WdgM/SchM） | 配置宏; [待集成] |
| — | `embedded/freertos_port/include/` | `FreeRTOSConfig.h` 等 9 个 | FreeRTOS 移植层 | 内核对象类型; 时钟节拍 |
| — | `embedded/mcal_stubs/include/` | `Mcu.h` `Dio.h` 等 11 个 | MCAL 驱动桩接口 | 寄存器地址/位宽; 通道枚举 |
| — | `embedded/freestanding_includes/` | `stdlib.h` `string.h` | 交叉编译 freestanding 头 | — |

> **汇总层**: 根目录 `include/` 提供跨域接口契约头（`dk_interfaces.h` / `dk_protocol.h` / `dk_hal.h`），将上表接口规范化为文档级契约（含数据类型与范围），见 §4.4。

### 4.4 接口汇总层（根目录 `include/`）

根目录 `include/` 为 ASPICE SWE.2.BP2 证据的汇总层，提供云端-车端统一接口契约头文件，覆盖全部外部接口的数据类型与范围定义：

| 头文件 | 内容 |
|:-------|:-----|
| `include/dk_interfaces.h` | 总契约：消息信封 (Header E1 01 / Trailer E1 FF)、消息类型码表 (1000~9000)、KeyType/AccessLevel 枚举与范围 |
| `include/dk_protocol.h` | 协议契约：BERTLV 编码规则、MQTT topic/QoS 映射、REST 统一响应结构、错误码段位 (1xxx/2xxx/3xxx/4xxx) |
| `include/dk_hal.h` | 车端 HAL 契约：hal_ble/hal_uwb/hal_nfc/hal_sec 函数签名、参数范围（距离分区 LOCKED~INSIDE、电源状态 5 级） |

---

## 5. 关键数据流

### 5.1 密钥绑定（三层数据流）

```
手机App ─1:登录+注册设备─▶ Hub ─2:发起绑定请求─▶ Service
    ◀─6:绑定成功+车端公钥─ Hub ◀─5:仅记录编排元数据─ DKCS ─3:转发绑定指令─▶ OEM DK Server
                                                              └─4:生成密钥对(SE050/HSM), 返回公钥+证书─┘
```

### 5.2 远程控车（App → Hub → DKCS → MQTT → 车端）

```
App POST /command(JWT+签名) → Gateway → Token 验证 → VehicleControlService.SendCommand
  → 权限校验(AccessLevel) → HubTransport.SendToDKCS(BERTLV 3000) → DKCS → MQTT QoS2 → TCU → 车端执行
  → 状态回传: 车端 → MQTT → DKCS → Kafka → Hub → App 推送 (REQ-015/016)
```

### 5.3 钥匙分享

```
车主创建分享(权限+有效期) → Hub 记录分享元数据(不存密钥材料) → 通知 DK Server 签发新证书
  → 被分享者接受 → 分享码/推送 → 临时钥匙就绪 (REQ-014, REQ-002-S4)
```

### 5.4 消息完整性（BERTLV Trailer 签名）

```
发送: signature = HMAC-SHA256(Header + Body, sessionKey) → Trailer(E1 FF)
接收: verify == Trailer.Signature 否则拒绝 (REQ-018)
```

---

## 6. 需求覆盖映射

| 功能域 | 需求 | 架构组件 |
|:-------|:-----|:---------|
| System | REQ-001~009 | 全局（Hub + DKCS + Embedded + Frontend 协同） |
| DKCS Core | REQ-010~018 | Service Layer + Unified Protocol + HubTransport |
| Hub | REQ-019~023 | AdapterRegistry + service/ + CI 配置 |
| Protocol | REQ-024~027 | unified/codec + backend/cloud/protocol + gateway |
| Embedded | REQ-028~035 | 三协议栈 + unified_hal + SE050 + power/secure-boot |
| Frontend | REQ-036~040 | frontend SDK + REST/gRPC/MQ 契约 |

> 逐需求验收准则见 `docs/qualification-strategy.md`；需求↔测试追溯见 `.osh/evidence/traceability-matrix.md`。

---

## 7. 部署模型

| 模式 | Hub | OEM DK Server | TSP | 适用 |
|:-----|:----|:--------------|:----|:-----|
| A. Hub SaaS | 我方云 | 车厂云(参考实现) | 车厂原有 | 中小车厂快速接入 |
| B. On-Prem | 车厂内网容器 | 车厂自建 | 车厂原有 | 数据不出境 |
| C. 混合（推荐） | 我方 SaaS | 车厂云 | 车厂原有 | 主流车企；密钥材料不出厂 |

---

## 8. 安全架构关联

| Safety Goal | 架构层覆盖 | 验证方法 |
|:------------|:-----------|:---------|
| SG-01 防非预期解锁 | SWC + 协议栈 + hal_uwb + hal_sec | 故障注入 |
| SG-02 防非预期启动 | SWC + 协议栈 + hal_uwb + hal_sec | 测距精度测试 |
| SG-03 防中继攻击 | 协议栈 + hal_uwb + hal_sec | 中继攻击模拟 |
| SG-04 密钥保护 | hal_sec + SE050 | EAL6+ 渗透测试 |
| SG-05 远程控车鉴权 | Cloud (JWT) + MQTT 通道 | 安全通信测试 |
| SG-06 钥匙吊销分离 | Cloud CRL + 车端本地缓存 | 吊销时效测试 |

---

## 9. 架构决策记录 (ADR)

| ADR | 决策 | 理由 | 影响 |
|:----|:-----|:-----|:-----|
| ADR-01 | Hub 为 Token 签发机，不碰密钥材料 | 车厂 KMS 不可触碰；三协议均不要求云端持有密钥 | DKCS 剥离密钥存储职责 |
| ADR-02 | 三协议栈独立目录 + 统一 HAL | 协议互不干扰，硬件变更只改 HAL | 需维护三份类似逻辑 |
| ADR-03 | SE050 唯一安全元件 | EAL6+，密钥不离开芯片 | 性能受 SE050 吞吐限制 |
| ADR-04 | 当前裸机/RTOS 非 yuleASR | 快速原型先行 | ASIL-B 时间/内存隔离不可用 |
| ADR-05 | UWB+BLE+NFC 三模互补 | NFC 保底离线，UWB 防中继，BLE 主通道 | 三模测试复杂度高 |
| ADR-06 | BERTLV 统一内部协议 | 跨层消息信封 + HMAC 签名 | 编解码为性能关键路径 |

---

## 10. 已知架构缺口

| 缺口 | 影响 | 状态 |
|:-----|:-----|:-----|
| BSW 服务（OS/COM/DCM/…）待集成 | 无 ASIL-B 运行时框架 | 🔴 高风险，BSW 集成后解决 |
| 无统一错误处理规范 | 三协议栈错误码风格不统一 | 🟡 中风险（REQ-027 已定义云端统一错误码） |
| VFB 接口未定义 | 与 AUTOSAR 标准 SWC 不兼容 | `[待确认]` |
| 无 BSW 层时序约束文档 | 无法验证 FTTI 合规 | `[待确认]` |

---

*— 本文档为 yuleDKCS ASPICE CL2 证据链 SWE.2 主产出物；架构变更评审见 `docs/architecture-review.md`。—*
