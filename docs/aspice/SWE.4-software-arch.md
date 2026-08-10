# SWE.4 — 软件架构设计文档

> **项目**: yuleDKCS 数字钥匙系统
> **版本**: 1.0.0 | **日期**: 2026-07-07 | **状态**: 初版
> **关联**: SYS.3 (系统架构), SWE.5 (详细设计), spec-contract.md
> **ASIL 继承**: 见 safety-concept.md (SG-01~SG-06)

---

## 1. 架构概览

yuleDKCS 采用四层架构：SWC / RTE / BSW / MCAL。当前实现尚未集成 yuleASR OSEK OS，`[待确认: BSW 层处于待集成状态]`。

```
SWC  ┌────────────────────────────────────┐
     │ Application: VehicleAccess,        │
     │   KeyManagement, SecurityService   │
     └──────────────┬─────────────────────┘
────────────────────┼──────────────────────── RTE 层
BSW  ┌──────────────┴─────────────────────┐
     │ 三协议栈: ICCE / CCC / ICCOA       │
     │ HAL: hal_ble, hal_uwb, hal_nfc,    │
     │      hal_sec                        │
     │ SE050 驱动 / TFM 可信固件           │
     └──────────────┬─────────────────────┘
────────────────────┼──────────────────────── MCAL 层
MCAL ┌──────────────┴─────────────────────┐
     │ CAN / SPI / I2C / UART 驱动        │
     │ 硬件: KW47A / NCJ29D6 / ST25R501   │
     └────────────────────────────────────┘
```

| 层 | 职责 | ASIL 目标 |
|:---|:-----|:----------|
| SWC | 应用服务（车辆进出控制、密钥管理、安全服务） | ASIL-B |
| RTE | 运行时环境（当前为裸机/RTOS，`[待确认: yuleASR OSEK OS 集成时间]`） | ASIL-B |
| BSW | 基础软件（协议栈 + HAL + 安全芯片驱动 + BSW 服务） | ASIL-B / QM |
| MCAL | 微控制器抽象层（硬件驱动，CAN/SPI/I2C） | QM |

---

## 2. 各层模块列表

### 2.1 SWC 层（`embedded/`）

| 模块 | 路径 | 职责 | ASIL |
|:-----|:-----|:-----|:-----|
| VehicleAccess | `embedded/icce_protocol/`, `embedded/ccc_protocol/`, `embedded/iccoa_protocol/` | 车辆进出控制（解锁/上锁/启动授权） | ASIL-B |
| KeyManagement | 同上各协议栈核心逻辑 | 密钥配对、存储、更新、吊销处理 | ASIL-B |
| SecurityService | `embedded/*/security_*.c` | 安全挑战/响应、签名验证、中继攻击检测 | ASIL-B(D) |

### 2.2 BSW 层 — 协议栈层（`embedded/`）

| 模块 | 路径 | 职责 | ASIL |
|:-----|:-----|:-----|:-----|
| ICCE 协议栈 | `embedded/icce_protocol/` | ICCE T/CA 110-2020 协议实现 | ASIL-B |
| CCC 协议栈 | `embedded/ccc_protocol/` | CCC Digital Key 3.0 协议实现 | ASIL-B |
| ICCOA 协议栈 | `embedded/iccoa_protocol/` | ICCOA DK3.0/DK4.0 协议实现 | QM |

### 2.3 BSW 层 — HAL（`embedded/unified_hal/`）

| 模块 | 路径 | 职责 | ASIL |
|:-----|:-----|:-----|:-----|
| hal_ble | `embedded/unified_hal/hal_ble.c` | BLE 驱动抽象，适配 KW47A | QM |
| hal_uwb | `embedded/unified_hal/hal_uwb.c` | UWB 驱动抽象，适配 NCJ29D6 | ASIL-B(D) |
| hal_nfc | `embedded/unified_hal/hal_nfc.c` | NFC 驱动抽象，适配 ST25R501 | QM |
| hal_sec | `embedded/unified_hal/hal_sec.c` | 安全元件驱动抽象，适配 SE050 | ASIL-B |

### 2.4 BSW 层 — 安全（`embedded/`）

| 模块 | 路径 | 职责 | ASIL |
|:-----|:-----|:-----|:-----|
| SE050 驱动 | `embedded/*/se050_*` | 安全芯片密钥存储、密码学运算 | ASIL-B |
| TFM 可信固件 | `embedded/tfm/` | 安全启动链、隔离执行环境 | ASIL-B |

### 2.5 BSW 服务（`[待确认: yuleASR BSW 组件集成状态]`）

| 组件 | 职责 | 优先级 | 状态 |
|:-----|:-----|:-------|:-----|
| CSM | Crypto Service Manager，标准化密码接口 | P1 | ❌ 待集成 |
| NvM | NVRAM 管理，钥匙数据持久化 | P1 | ❌ 待集成 |
| DCM | UDS 诊断服务 | P1 | ❌ 待集成 |
| DEM | DTC 故障诊断 | P1 | ❌ 待集成 |
| COM | Signal→I-PDU 映射 | P1 | ❌ 待集成 |
| WdgM | 看门狗管理，ASIL-B 监控 | P1 | ❌ 待集成 |
| EcuM | ECU 状态管理 | P0 | ❌ 待集成 |
| OS | OSEK AUTOSAR OS | P0 | ❌ 待集成 |

### 2.6 MCAL 层

| 模块 | 硬件 | ASIL |
|:-----|:-----|:-----|
| CAN 驱动 | S32K312 CAN FD | QM |
| SPI 驱动 | SE050 SPI | QM |
| I2C 驱动 | NFC 控制器 I2C | QM |
| UART 驱动 | 调试串口 | QM |

---

## 3. 模块间接口摘要

| 接口方向 | 协议/机制 | 说明 |
|:---------|:----------|:-----|
| SWC ↔ 协议栈 | 函数调用 | 应用层调用协议栈核心逻辑 |
| 协议栈 ↔ HAL | 函数调用（hal_* API） | 统一 HAL 接口隔离硬件差异 |
| HAL ↔ MCAL | 寄存器/外设驱动 | BLE/UWB/NFC/SE 硬件控制 |
| SWC ↔ SE050 | hal_sec → SE050 | 密码学运算在安全芯片内执行 |
| 车端 ↔ 手机 | BLE GATT / UWB / NFC | 近场通信通道 |
| 车端 ↔ 云端 | MQTT over TLS 1.3 / gRPC | 远程指令、状态同步 |

---

## 4. 层间依赖关系

```
SWC ────────────▶ 协议栈 ──────────▶ HAL ──────────▶ MCAL
                      │                  │
                      │                  ▼
                      │              SE050 安全芯片
                      ▼
                   `[待确认: yuleASR BSW]`───▶ MCAL
```

| 依赖 | 方向 | 类型 | 说明 |
|:-----|:-----|:-----|:-----|
| SWC → 协议栈 | 调用 | 编译期 | SWC 调用协议栈 API |
| 协议栈 → HAL | 调用 | 链接期 | 协议栈通过 HAL 接口访问硬件 |
| HAL → MCAL | 调用 | 链接期 | HAL 封装 MCAL 驱动 |
| SWC → SE050 | 间接 | 运行时 | 经 hal_sec 访问 SE050 |
| `[待确认]` BSW → MCAL | 调用 | 链接期 | 待 yuleASR 集成后建立 |

---

## 5. 架构决策记录

| ADR ID | 决策 | 理由 | 影响 |
|:-------|:-----|:-----|:-----|
| ADR-01 | 四层架构 (SWC/RTE/BSW/MCAL) | 与 AUTOSAR 分层对齐，便于 BSW 逐步替换 | 当前 RTE 层为裸机，BSW 集成后需调整接口 |
| ADR-02 | 三协议栈独立目录、统一 HAL | 协议互不干扰，硬件变更只需更新 HAL | 协议间复用代码少，需维护三份类似逻辑 |
| ADR-03 | SE050 作为唯一安全元件 | EAL6+ 认证，密钥从不离开安全芯片 | 密码学运算性能受限于 SE050 吞吐 |
| ADR-04 | 当前裸机/RTOS 运行，非 yuleASR | 快速原型先行，ASIL-B 分区后续迁移 | ASIL-B 时间/内存隔离当前不可用 |
| ADR-05 | UWB + BLE + NFC 三模互补 | NFC 保底离线，UWB 防中继，BLE 主通道 | 三模调试和测试复杂度高 |
| ADR-06 | BSW 集成三阶段推进 | 先 MCAL+OS，再诊断，后 ASIL-B 分区 | 短期无法获取 ASIL-B 认证 |

---

## 6. 安全架构关联

| Safety Goal | 架构层覆盖 | 验证方法 |
|:------------|:-----------|:---------|
| SG-01 防非预期解锁 | SWC + 协议栈 + hal_uwb + hal_sec | 故障注入 |
| SG-02 防非预期启动 | SWC + 协议栈 + hal_uwb + hal_sec | 测距精度测试 |
| SG-03 防中继攻击 | 协议栈 + hal_uwb + hal_sec | 中继攻击模拟 |
| SG-04 密钥保护 | hal_sec + SE050 | EAL6+ 渗透测试 |
| SG-05 远程控车鉴权 | Cloud (JWT) + MQTT 通道 | 安全通信测试 |
| SG-06 钥匙吊销分离 | Cloud CRL + 车端本地缓存 | 吊销时效测试 |

---

## 7. 已知缺口

| 缺口 | 影响 | 状态 |
|:-----|:-----|:-----|
| 无 ASIL-B 运行时框架 | 无时间隔离、内存隔离、看门狗管理 | 🔴 高风险，BSW 集成后解决 |
| 无统一错误处理规范 | 三协议栈错误码风格不统一 | 🟡 中风险 |
| VFB 接口未定义 | 与 AUTOSAR 标准 SWC 接口不兼容 | `[待确认: 是否需要 AUTOSAR VFB]` |
| 无 BSW 层时序约束文档 | 无法验证 FTTI 合规 | `[待确认]` |
