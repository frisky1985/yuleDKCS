# Layer Structure Table (LST) — yuleDKCS ASPICE 层映射

**版本**: 1.0 | **日期**: 2026-07-07 | **标准**: ISO 26262 / ASPICE SWE.4–SWE.6

---

## 1. 层定义

| ASPICE 层 | 缩写 | 说明 | 对应 AUTOSAR 层 |
|-----------|------|------|----------------|
| **SWE.6** | SWC | 应用软件组件 — 业务逻辑 | Application Layer |
| **SWE.5** | RTE | 运行时环境 — 路由/鉴权/限流 | Runtime Environment |
| **SWE.4** | BSW | 基础软件 — 数据库/缓存/安全/基础设施 | Basic Software Layer |
| **SWE.4-MCAL** | MCAL | 微控制器抽象 — 协议适配器 | Microcontroller Abstraction |

---

## 2. 云端 Go 包层映射

### 2.1 DK Hub (backend/cloud/hub/)

| 包路径 | ASPICE 层 | 职责 | 对外接口 | 依赖 |
|--------|-----------|------|----------|------|
| `internal/gateway` | **RTE (SWE.5)** | REST/gRPC 路由、JWT 鉴权、限流、熔断 | REST API / gRPC | service, token |
| `internal/service` | **SWC (SWE.6)** | 钥匙编排、车控指令、设备管理、钥匙分享 | Service Interfaces | token, adapter |
| `internal/unified` | **SWC (SWE.6)** | CCC/ICCOA/ICCE 统一协议处理 | Unified Router | codec, service |
| `internal/adapter` | **MCAL (SWE.4)** | CCC/ICCOA/ICCE 协议消息适配 | Adapter Registry | codec, service |
| `internal/token` | **BSW (SWE.4)** | JWT Token 签发/验证、权限编码 | Token Interface | — |
| `internal/codec` | **BSW (SWE.4)** | BER-TLV 编解码 | Decoder/Encoder | — |
| `internal/telemetry` | **BSW (SWE.4)** | 指标采集、OpenTelemetry | Metrics Interface | — |
| `internal/logger` | **BSW (SWE.4)** | 结构化日志、日志级别管理 | Logger Interface | — |
| `internal/error` | **BSW (SWE.4)** | 错误类型定义、错误码映射 | Error Interface | — |
| `internal/security` | **BSW (SWE.4)** | 安全监控、威胁事件检测、告警 | Monitor Interface | store, logger |
| `internal/diagnostics` | **RTE (SWE.5)** | 全链路追踪、日志采集、健康检查 | Tracer/Collector | logger |

### 2.2 DKCS (backend/dkcs/)

| 包路径 | ASPICE 层 | 职责 | 对外接口 | 依赖 |
|--------|-----------|------|----------|------|
| `internal/service` | **SWC (SWE.6)** | 钥匙服务、事件服务、命令服务、状态机 | gRPC Service | keymgmt, device, mq |
| `internal/device` | **SWC (SWE.6)** | 设备注册/配对/状态管理 | Device Service | repository |
| `internal/tsp` | **SWC (SWE.6)** | TSP 第三方集成 (车厂对接) | TSP Interface | config |
| `internal/middleware` | **RTE (SWE.5)** | gRPC 拦截器(鉴权/日志/追踪) | Middleware Chain | config, logger |
| `internal/keymgmt` | **BSW (SWE.4)** | 密钥生成/派生/轮换/安全存储 | Key Management | cache, repository |
| `internal/mq` | **BSW (SWE.4)** | Kafka 消息队列集成 | MQ Producer/Consumer | config |
| `internal/repository` | **BSW (SWE.4)** | 数据库数据访问层 (PostgreSQL) | Repository Interface | db |
| `internal/cache` | **BSW (SWE.4)** | Redis 缓存层 | Cache Interface | redis |
| `internal/config` | **BSW (SWE.4)** | 配置管理 (环境变量/YAML) | Config Interface | — |
| `pkg/logger` | **BSW (SWE.4)** | 共享日志封装 | Logger | — |
| `pkg/telemetry` | **BSW (SWE.4)** | 共享遥测封装 | Telemetry | — |

### 2.3 Java 适配器 (backend/adapters/)

| 包路径 | ASPICE 层 | 职责 |
|--------|-----------|------|
| `adapter-ccc` | **MCAL (SWE.4)** | CCC 3.0 协议适配 |
| `adapter-iccoa` | **MCAL (SWE.4)** | ICCOA DK3.0/DK4.0 协议适配 |
| `adapter-icce` | **MCAL (SWE.4)** | ICCE 协议适配 |
| `adapter-core` | **BSW (SWE.4)** | 适配器核心框架 |
| `adapter-grpc-server` | **RTE (SWE.5)** | gRPC 服务器端适配 |

---

## 3. 嵌入式 C 模块层映射

### 3.1 ICCE 协议栈 (embedded/icce_protocol/)

| 模块 | 路径 | ASPICE 层 | 职责 |
|------|------|-----------|------|
| ICCE 主协议 (SWC) | `include/icce_digital_key.h` | **SWC (SWE.6)** | 应用层协议接口 |
| BLE 适配器 (BSW) | `src/ble/` | **BSW (SWE.4)** | BLE 5.0 通信抽象 |
| 密码引擎 (BSW) | `src/crypto/` | **BSW (SWE.4)** | 国密 SM2/SM3/SM4 + 标准算法 |
| 缓存管理 (BSW) | `src/cache/` | **BSW (SWE.4)** | 会话缓存与持久化 |
| 离线决策 (SWC) | `src/decision/` | **SWC (SWE.6)** | 边缘计算决策引擎 |
| 安全认证 (BSW) | `include/security_auth.h` | **BSW (SWE.4)** | 安全认证/密钥管理 |
| HSM接口 (MCAL) | `src/security/hsm_interface.h` | **MCAL (SWE.4)** | SE 硬件安全模块接口 |
| 车辆集成 (MCAL) | `src/vehicle/` | **MCAL (SWE.4)** | TCU/CAN 总线集成 |

### 3.2 CCC 协议栈 (embedded/ccc_protocol/)

| 模块 | 路径 | ASPICE 层 | 职责 |
|------|------|-----------|------|
| CCC 主协议 (SWC) | `include/ccc_digital_key.h` | **SWC (SWE.6)** | CCC 3.0 应用层协议 |
| 硬件抽象 (MCAL) | `include/hardware_abstraction.h` | **MCAL (SWE.4)** | HW 引脚/驱动抽象 |
| 安全模块 (BSW) | `src/security/security.c` | **BSW (SWE.4)** | SCP03/加密/认证 |
| BLE 驱动 (MCAL) | `src/ble/` | **MCAL (SWE.4)** | KW47A BLE 驱动 |
| NFC 驱动 (MCAL) | `src/nfc/` | **MCAL (SWE.4)** | ST25R501 NFC 驱动 |
| UWB 驱动 (MCAL) | `src/uwb/` | **MCAL (SWE.4)** | NCJ29D6 UWB 驱动 |
| 密钥管理 (BSW) | `src/keymgmt/` | **BSW (SWE.4)** | 密钥存储与管理 |

### 3.3 ICCOA 协议栈 (embedded/iccoa_protocol/)

| 模块 | 路径 | ASPICE 层 | 职责 |
|------|------|-----------|------|
| ICCOA 主协议 (SWC) | `include/iccoa_digital_key.h` | **SWC (SWE.6)** | ICCOA 应用层协议 |

### 3.4 统一协议层 (embedded/unified_protocol/)

| 模块 | 路径 | ASPICE 层 | 职责 |
|------|------|-----------|------|
| 统一协议接口 (SWC) | `include/dk_unified.h` | **SWC (SWE.6)** | 三协议统一 API |

---

## 4. 层间依赖规则

```
SWC (SWE.6)  ──────→  RTE (SWE.5)  ──────→  BSW (SWE.4)  ──────→  MCAL
  │                       │                       │
  │   业务逻辑            │   路由/鉴权           │   基础设施
  │   service/           │   gateway/            │   keymgmt/
  │   unified/           │   middleware/         │   repository/
  │   tsp/               │   diagnostics/        │   cache/
  │   device/            │                       │   mq/
  │                       │                       │   security/
  └───────────────────────┴───────────────────────┘
  └────── 上层可依赖下层，下层不可反向依赖上层 ─────┘
```

**核心规则:**
1. SWC 可依赖 RTE + BSW + MCAL
2. RTE 可依赖 BSW + MCAL，不可依赖 SWC
3. BSW 可依赖 MCAL，不可依赖 SWC/RTE
4. MCAL 不可依赖任何上层
5. 同层之间允许相互引用（但应通过接口而非实现）

---

## 5. 接口清单

### 云端接口

| 接口名 | 提供方 | 消费方 | 协议 |
|--------|--------|--------|------|
| REST API | gateway | 手机端/三方 | HTTP/1.1 + TLS |
| gRPC API | middleware | Hub ↔ DKCS | HTTP/2 + TLS |
| Token 签发 | token | gateway | 内存调用 |
| 钥匙服务 | service | gateway | 内存调用 |
| 协议适配 | adapter | service | 内存调用 |
| 统一路由 | unified | gateway | 内存调用 |
| 安全监控 | security | gateway, service | 内存调用 |
| 全链路追踪 | diagnostics | 全局 | 内存/Kafka |

### 嵌入式接口

| 接口名 | 提供方 | 消费方 | 协议 |
|--------|--------|--------|------|
| dk_init | 各协议栈 | system | API 调用 |
| BLE 通信 | BLE 模块 | 协议栈 | GATT |
| UWB 测距 | UWB 模块 | 协议栈 | FiRa |
| NFC 读写 | NFC 模块 | 协议栈 | ISO 14443 |
| HSM 操作 | HSM 模块 | 安全模块 | I²C/SPI |

---

*LST 版本 1.0 — 与 ASPICE SWE.4–SWE.6 对齐*
