# yuleDKCS HIL (Hardware-in-the-Loop) 测试计划

> **版本**：v1.0
> **日期**：2026-07-29
> **状态**：初版
> **适用范围**：硬件测试团队、质量工程、嵌入式开发团队

---

## 目录

1. [概述](#1-概述)
2. [测试环境拓扑](#2-测试环境拓扑)
3. [硬件需求清单](#3-硬件需求清单)
4. [软件需求清单](#4-软件需求清单)
5. [测试用例覆盖矩阵](#5-测试用例覆盖矩阵)
6. [自动化测试框架设计](#6-自动化测试框架设计)
7. [测试执行计划](#7-测试执行计划)
8. [测试通过标准](#8-测试通过标准)
9. [附录](#9-附录)

---

## 1. 概述

### 1.1 测试目标

yuleDKCS HIL 测试通过在硬件在环环境中对整个数字钥匙系统进行闭环测试，验证：

| 目标 | 说明 |
|------|------|
| **协议合规性** | CCC DK 3.0 / ICCOA DK 2.0 / ICCE T/CA 110-2020 协议实现正确性 |
| **功能完整性** | 配对、解锁、锁车、启动、钥匙分享等端到端流程 |
| **安全有效性** | 防中继、防重放、安全通道、签名验证等安全机制 |
| **通信可靠性** | BLE / UWB / NFC 三模通信在真实硬件上的稳定性和互操作性 |
| **压力稳定性** | 长时间运行、高并发、极限温度下的系统可靠性 |

### 1.2 测试范围

| 测试域 | 覆盖范围 | 不覆盖 |
|--------|----------|--------|
| 嵌入式固件 | CCC/ICCOA/ICCE 协议栈、SE050 交互、CAN 控制 | 单板硬件信号完整性 |
| 硬件模块 | BLE 射频、NFC 场强、UWB 测距精度 | EMI/EMC 合规测试 |
| 端到端流程 | App ↔ 车端 ↔ 云端完整链路 | 云端大规模压力测试 |
| 安全机制 | 中继攻击、重放攻击、签名伪造 | 物理攻击 (SE050 拆除) |
| 边界场景 | 信号干扰、低电量、极端温度 | 电磁兼容性 (EMC) |

---

## 2. 测试环境拓扑

### 2.1 总体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                       HIL 测试环境拓扑                               │
│                                                                      │
│   ┌──────────────────────────────────────────────────────────┐     │
│   │                     PC 上位机 (Test Controller)           │     │
│   │                                                           │     │
│   │   ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │     │
│   │   │  Test Runner  │  │  Data Logger  │  │  Report Gen  │  │     │
│   │   │  (Python)     │  │  (InfluxDB)   │  │  (Allure)    │  │     │
│   │   └──────┬───────┘  └──────┬───────┘  └──────┬───────┘  │     │
│   │          │                 │                  │           │     │
│   │   ┌──────▼─────────────────▼──────────────────▼───────┐  │     │
│   │   │              Test Orchestrator                     │  │     │
│   │   │  (Scenario Manager / Signal Generator / Monitor)   │  │     │
│   │   └──────┬──────────┬──────────┬──────────┬──────────┘  │     │
│   └──────────┼──────────┼──────────┼──────────┼─────────────┘     │
│              │          │          │          │                     │
│   ┌──────────▼──┐ ┌─────▼────┐ ┌──▼────┐ ┌──▼────────────┐       │
│   │  BLE Sniffer│ │NFC读写器 │ │UWB锚点│ │ 数字钥匙App   │       │
│   │  (nRF52840) │ │(PN5180) │ │(SR250)│ │ (iOS/Android) │       │
│   └─────────────┘ └──────────┘ └───────┘ └───────┬────────┘       │
│                                                    │               │
│   ┌───────────────────────────────────────────────┼───────────┐   │
│   │                  Shield Box (RF隔离)          │           │   │
│   │                                               ▼           │   │
│   │   ┌───────────────────────────────────────────────────┐   │   │
│   │   │              目标板 (DUT)                         │   │   │
│   │   │  ┌────────────┐  ┌────────────┐  ┌────────────┐  │   │   │
│   │   │  │ S32G2 EVB  │  │ KW47 BLE   │  │ ST25R501   │  │   │   │
│   │   │  │ (MCU)      │  │ (BLE模块)   │  │ (NFC模块)  │  │   │   │
│   │   │  ├────────────┤  ├────────────┤  ├────────────┤  │   │   │
│   │   │  │ NCJ29D6    │  │ SE050      │  │ CAN总线    │  │   │   │
│   │   │  │ (UWB)      │  │ (SE芯片)   │  │ (CAN FD)   │  │   │   │
│   │   │  └────────────┘  └────────────┘  └────────────┘  │   │   │
│   │   └───────────────────────────────────────────────────┘   │   │
│   └───────────────────────────────────────────────────────────┘   │
│                                                                      │
│   ┌──────────────────────────────────────────────────────────┐     │
│   │                    Cloud Mock / Simulator                 │     │
│   │   ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌────────┐ │     │
│   │   │ DKCS     │  │ Hub      │  │ KMS/HSM  │  │ OTA    │ │     │
│   │   │ Mock     │  │ Mock     │  │ Mock     │  │ Server │ │     │
│   │   └──────────┘  └──────────┘  └──────────┘  └────────┘ │     │
│   └──────────────────────────────────────────────────────────┘     │
└─────────────────────────────────────────────────────────────────────┘
```

### 2.2 通信连接图

```
PC 上位机
  │
  ├── USB/UART ────── S32G2 EVB (调试串口, 日志输出)
  ├── USB ─────────── nRF52840 DK (BLE Sniffer)
  ├── USB ─────────── PN5180 开发板 (NFC 读写器)
  ├── USB ─────────── UWB 锚点 (SR250)
  ├── Ethernet ────── Cloud Mock (Docker 容器)
  └── USB/WiFi ────── 测试手机 (iOS/Android)

目标板 (DUT)
  ├── SWD/JTAG ────── J-Link (调试/烧录)
  ├── UART ────────── PC 上位机 (日志)
  ├── CAN-FD ──────── CAN 分析仪 (CAN 报文监控)
  ├── BLE ─────────── nRF52840 Sniffer + 手机 App
  ├── NFC ─────────── PN5180 读写器 + 测试手机
  ├── UWB ─────────── UWB 锚点 (测距校准)
  └── Ethernet ────── Cloud Mock API 模拟
```

### 2.3 测试角色与设备映射

| 测试角色 | 物理设备 | 作用 |
|----------|----------|------|
| DUT (Device Under Test) | S32G2 EVB + 所有模块 | 被测嵌入式系统 |
| BLE 模拟器 | nRF52840 DK | 模拟手机 BLE 通信 |
| NFC 模拟器 | PN5180 开发板 | 模拟手机 NFC 交互 |
| UWB 参考点 | SR250 开发板 | UWB 测距标准参考 |
| 手机模拟器 | iOS/Android 测试机 + 自动化脚本 | App 端行为模拟 |
| Cloud Mock | Docker 容器 (DKCS + Hub + HSM) | 云端 API 模拟 |
| 攻击模拟器 | PC + 特殊工具 | 中继/重放/注入攻击模拟 |
| 信号发生器 | 任意波形发生器 | 信号干扰测试 |
| 温度箱 | 环境试验箱 | 极限温度测试 |

---

## 3. 硬件需求清单

### 3.1 必须硬件 (Minimum Viable HIL)

| 序号 | 设备 | 型号/规格 | 数量 | 用途 | 预估成本 |
|------|------|----------|------|------|---------|
| 1 | 开发板 | NXP S32G2 EVB | 2 | 主 DUT + 备用 | $800 |
| 2 | BLE 模块 | NXP KW47A DK | 2 | BLE 通信模块 | $100 |
| 3 | NFC 读写器板 | NXP PN5180 DK | 2 | NFC 交互 | $80 |
| 4 | UWB 模块 | NXP SR250 DK | 3 | UWB 测距 (1 DUT + 2 锚点) | $300 |
| 5 | 安全芯片 | NXP SE050 (已集成在 EVB) | 2 | 安全元件 | (含在 EVB) |
| 6 | 调试器 | SEGGER J-Link PLUS | 2 | 固件烧录/调试 | $600 |
| 7 | BLE Sniffer | Nordic nRF52840 DK | 1 | BLE 协议分析 | $50 |
| 8 | 测试手机 | iPhone 14+ / Pixel 7 | 2 | App 端测试 | $1000 |
| 9 | PC 上位机 | x86-64, 16GB RAM, Ubuntu 22.04 | 1 | 测试控制器 | $1500 |
| 10 | USB-UART 线 | CP2102 / FT232 | 5 | 串口通信 | $50 |

### 3.2 推荐硬件 (Full HIL Suite)

| 序号 | 设备 | 型号/规格 | 数量 | 用途 | 预估成本 |
|------|------|----------|------|------|---------|
| 1 | 射频屏蔽箱 | 30cm x 30cm x 20cm | 1 | RF 隔离测试环境 | $2000 |
| 2 | 频谱分析仪 | Rigol DSA815-TG | 1 | BLE/NFC/UWB 频谱分析 | $800 |
| 3 | 环境试验箱 | -40°C ~ +125°C | 1 | 温度循环测试 | $5000 |
| 4 | CAN 分析仪 | PCAN-USB FD | 1 | CAN FD 报文监控 | $500 |
| 5 | 示波器 | Rigol DS1104Z | 1 | 信号时序分析 | $500 |
| 6 | 任意波形发生器 | Siglent SDG1032X | 1 | 信号干扰生成 | $400 |
| 7 | NFC 分析仪 | Proxmark3 RDV4 | 1 | NFC 协议深度分析 | $300 |
| 8 | Lauterbach TRACE32 | PowerDebug X50 | 1 | 高级调试/代码追踪 | $8000 |
| 9 | UWB 分析仪 | FiRa PHY 测试设备 | 1 | UWB 物理层测试 | $3000 |

### 3.3 硬件连接清单

```
┌─────────────────────────────────────────────────────────────┐
│                   HIL 硬件布线图                             │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  PC (上位机)                                                  │
│   ├── USB ── J-Link ── SWD ── S32G2 EVB                     │
│   ├── USB ── UART  ── S32G2 Debug Console                    │
│   ├── USB ── nRF52840 DK (BLE Sniffer)                      │
│   ├── USB ── PN5180 DK (NFC 读写器)                         │
│   ├── USB ── SR250 DK (UWB 锚点 1)                          │
│   ├── USB ── SR250 DK (UWB 锚点 2)                          │
│   ├── USB ── PCAN-USB FD ── CAN bus                          │
│   ├── USB ── 示波器 / 频谱分析仪                              │
│   ├── USB ── 测试手机 (通过 adb / libimobiledevice)          │
│   └── ETH ── Docker 容器 (Cloud Mock)                       │
│                                                              │
│  DUT 框内 (Shield Box)                                       │
│   S32G2 EVB                                                  │
│    ├── UART ── PC                                            │
│    ├── SWD  ── J-Link                                        │
│    ├── CAN  ── PCAN-USB                                      │
│    ├── BLE  ── Sniffer + 手机 (天线引出至屏蔽箱外)            │
│    ├── NFC  ── PN5180 (天线在屏蔽箱内)                       │
│    └── UWB  ── SR250 锚点 (天线在屏蔽箱内)                  │
└─────────────────────────────────────────────────────────────┘
```

---

## 4. 软件需求清单

### 4.1 上位机软件栈

| 软件 | 版本 | 用途 |
|------|------|------|
| Ubuntu | 22.04 LTS | 测试控制主机 OS |
| Python | 3.11+ | 测试脚本语言 |
| pytest | 8.x | 测试框架 |
| Allure Framework | 2.x | 测试报告生成 |
| InfluxDB | 2.7+ | 测试数据时序存储 |
| Grafana | 10.x | 实时测试监控仪表板 |
| Robot Framework | 6.x | 关键字驱动测试 (可选) |
| Docker | 24.x | Cloud Mock 容器化部署 |

### 4.2 嵌入式工具链

| 工具 | 版本 | 用途 |
|------|------|------|
| ARM GCC Toolchain | 10.3.1 | 交叉编译 |
| CMake | 3.20+ | 构建系统 |
| Unity Test Framework | 2.5.x | 嵌入式单元测试 |
| J-Link Commander | 7.94+ | 固件烧录/调试 |
| nRF Connect | 5.x (PC) | BLE 协议监控 |
| Wireshark + BLE ext | 4.x | BLE 包分析 |
| Proxmark3 GUI | latest | NFC 协议分析 |
| PCAN-View | latest | CAN 报文监控 |

### 4.3 Cloud Mock 组件

| 组件 | 技术栈 | 说明 |
|------|--------|------|
| DKCS Mock | Go | 模拟数字钥匙云服务 |
| Hub Mock | Go | 模拟 Hub 路由器 |
| KMS/HSM Mock | Go + SoftHSM | 模拟密钥管理服务 |
| OTA Server Mock | Go | 模拟 OTA 更新服务 |
| TSP Adapter Mock | Java | 模拟 TSP 适配器 |

### 4.4 测试自动化库

```python
# Python 测试库依赖 (requirements-hil.txt)
pytest==8.1.0
pytest-asyncio==0.23.0
pytest-allure-adaptor==1.7.10
pytest-xdist==3.5.0        # 并行测试
pySerial==3.5              # 串口通信
pynrfjprog==10.16.0        # Nordic BLE 控制
nfcpy==1.0.4               # NFC 通信 (PN532/PN5180)
pyuwb==0.2.0               # UWB 控制 (预留接口)
influxdb-client==1.44.0    # 数据写入 InfluxDB
gql==3.5.0                 # GraphQL 客户端 (Cloud Mock)
httpx==0.27.0              # HTTP 客户端
paramiko==3.4.0            # SSH 远程控制
```

---

## 5. 测试用例覆盖矩阵

### 5.1 E2E 场景映射 (5 个主要场景)

以下 5 个 E2E 场景对应 `tests/integration/scenarios/` 中的集成测试用例，每个场景在 HIL 环境中实施完整验证。

#### 场景 1: NFC+BLE 双模式解锁 (INT_NFC_*)

| HIL 用例 ID | 对应 E2E 用例 | 测试描述 | 步骤 | 验证点 |
|------------|-------------|----------|------|--------|
| HIL_NFC_001 | INT_NFC_001 | NFC 触发 → BLE 建立 → 解锁 | 1. 手机靠近 NFC 天线 2. NFC 读取车端信息 3. BLE 自动连接 4. 安全通道建立 5. 执行解锁 | 3s 内完成解锁 |
| HIL_NFC_002 | INT_NFC_002 | NFC 优先启动 (车辆未连接) | 1. 车端 BLE 未连接 2. 手机靠近 NFC 3. NFC 读取钥匙信息 4. 自动建立 BLE 连接 | BLE 连接建立 < 5s |
| HIL_NFC_003 | INT_NFC_003 | BLE 降级 NFC (BLE 连接失败) | 1. 模拟 BLE 连接失败 2. 手机靠近 NFC 3. NFC 触发应急解锁 | NFC 解锁成功 |
| HIL_NFC_004 | INT_NFC_004 | NFC 应急解锁 (手机没电) | 1. 手机模拟低电量关机 2. 靠近 NFC 应急区 3. 触发 NFC 被动解锁 | 解锁成功 (NFC 被动模式) |

#### 场景 2: UWB 无感解锁 (INT_UWB_*)

| HIL 用例 ID | 对应 E2E 用例 | 测试描述 | 验证点 |
|------------|-------------|----------|--------|
| HIL_UWB_001 | INT_UWB_001 | 车主靠近 → UWB 测距 < 1m → 自动解锁 | 解锁延迟 ≤ 1s |
| HIL_UWB_002 | INT_UWB_002 | 4+ 锚点部署 → UWB 精确定位 | 定位精度 < 10cm |
| HIL_UWB_003 | INT_UWB_003 | UWB 测距 10m 范围内距离判断 | 距离误差 < 10cm |
| HIL_UWB_004 | INT_UWB_004 | 脚踢感应 → UWB 检测位置变化 → 开后备箱 | 后备箱开启成功 |
| HIL_UWB_005 | INT_UWB_005 | 多车主位置区分 → 仅主驾解锁主驾门 | 正确解�锁对应车门 |

#### 场景 3: 钥匙全生命周期 (INT_KEY_*)

| HIL 用例 ID | 对应 E2E 用例 | 测试描述 | 验证点 |
|------------|-------------|----------|--------|
| HIL_KEY_001 | INT_KEY_001 | 首次钥匙配发流程 | 钥匙成功写入 SE050 |
| HIL_KEY_002 | INT_KEY_002 | 钥匙 OTA 更新 | 更新后验证签名 |
| HIL_KEY_003 | INT_KEY_003 | 钥匙导入 (换手机) | 导入后可用 |
| HIL_KEY_004 | INT_KEY_010 | 解锁认证 (BLE 通信) | 签名验证通过 |
| HIL_KEY_005 | INT_KEY_011 | 启动认证 (P档+刹车) | 引擎启动成功 |
| HIL_KEY_006 | INT_KEY_012 | 远程解锁认证 (云端→车辆) | 远程解锁成功 |
| HIL_KEY_007 | INT_KEY_020 | 钥匙分享流程 (车主→受邀者) | 受邀者成功使用 |
| HIL_KEY_008 | INT_KEY_022 | 撤销分享钥匙 | 撤销后钥匙失效 |

#### 场景 4: 安全通道与加密通信 (INT_SEC_*)

| HIL 用例 ID | 对应 E2E 用例 | 测试描述 | 验证点 |
|------------|-------------|----------|--------|
| HIL_SEC_001 | INT_SEC_001 | SE050 安全通道建立 | SCP03 通道加密建立 |
| HIL_SEC_002 | INT_SEC_002 | 双向 ECDSA 认证 | 认证成功 |
| HIL_SEC_003 | INT_SEC_003 | ECDH 密钥协商 → 会话密钥派生 | 双方会话密钥一致 |
| HIL_SEC_004 | INT_SEC_010 | 数据加密传输 + 解密 | 明文一致 |
| HIL_SEC_005 | INT_SEC_011 | 命令完整性验证 (HMAC) | HMAC 验证通过 |
| HIL_SEC_006 | INT_SEC_012 | 防重放攻击 (重放旧数据包) | 拒绝并告警 |

#### 场景 5: 异常恢复与边界条件 (INT_REC_* + INT_BND_*)

| HIL 用例 ID | 对应 E2E 用例 | 测试描述 | 验证点 |
|------------|-------------|----------|--------|
| HIL_REC_001 | INT_REC_001 | 应用崩溃后恢复 | 钥匙状态恢复 |
| HIL_REC_002 | INT_REC_002 | BLE 断开后自动重连 | 重连成功 ≤ 3 次 |
| HIL_REC_003 | INT_REC_003 | 网络恢复后数据同步 | 同步完成 |
| HIL_REC_004 | INT_BND_001 | 信号干扰降级 | 降级/重试恢复 |
| HIL_REC_005 | INT_BND_002 | 多车同时场景识别 | 正确识别目标车 |
| HIL_REC_006 | INT_BND_003 | 低电量模式测试 | 基本解锁功能正常 |
| HIL_REC_007 | INT_BND_004 | 高速移动拒绝解锁 | 拒绝正确 |
| HIL_REC_008 | INT_BND_005 | 极端温度运行 | 进入保护/恢复正常 |

### 5.2 安全攻击测试矩阵

| HIL 用例 ID | 攻击类型 | 测试方法 | 预期结果 |
|------------|----------|----------|----------|
| HIL_ATK_001 | BLE 中继攻击 | 信号放大器延长 BLE 范围 + 延迟注入 | UWB 测距不受影响 → 拒绝 |
| HIL_ATK_002 | BLE+UWB 同步中继 | 延迟注入 50ms | 挑战超时 → 拒绝 |
| HIL_ATK_003 | 纯 BLE 中继 (无 UWB) | 禁用手机 UWB | 拒绝 (要求 UWB 验证) |
| HIL_ATK_004 | 位置跳跃攻击 | 模拟 5m→1m 瞬移 | 移动模式检测 → 拒绝 |
| HIL_ATK_005 | NFC 重放攻击 | 录制 APDU → 重放 | 计数器不匹配 → 拒绝 |
| HIL_ATK_006 | BLE 注入攻击 | 注入伪造 GATT 写请求 | 签名验证失败 → 拒绝 |
| HIL_ATK_007 | 会话密钥劫持 | 尝试重用旧会话密钥 | SE050 拒绝 |
| HIL_ATK_008 | JWT 伪造 | 修改 Token 内容重放 | 云端验证拒绝 |

### 5.3 压力测试矩阵

| HIL 用例 ID | 压力类型 | 条件 | 验收标准 |
|------------|----------|------|----------|
| HIL_STR_001 | 72h 连续运行 | 每小时 10 次解锁/锁车循环 | 无死机、无泄漏 |
| HIL_STR_002 | 10000 次解锁循环 | 连续解锁/锁车 | 成功率 ≥ 99.9% |
| HIL_STR_003 | 10 设备并发连接 | 同时连接 | 全部成功 |
| HIL_STR_004 | 50 设备并发扫描 | 密集广播环境 | 无崩溃 |
| HIL_STR_005 | 1000 次 BLE 连接/断开 | 循环 | 成功率 ≥ 99% |
| HIL_STR_006 | 10000 次密钥验证 | 循环验证 | 成功率 100% |

---

## 6. 自动化测试框架设计

### 6.1 框架架构

```
┌────────────────────────────────────────────────────────────────────┐
│                    HIL 自动化测试框架                               │
├────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌──────────────────────────────────────────────────────────┐     │
│  │                    Test Runner Layer                      │     │
│  │   ┌─────────────┐ ┌─────────────┐ ┌──────────────────┐  │     │
│  │   │ pytest       │ │ Robot       │ │ Allure Reporter  │  │     │
│  │   │ (Test Cases) │ │ (Keywords)  │ │ (Reports)       │  │     │
│  │   └──────┬──────┘ └──────┬──────┘ └────────┬─────────┘  │     │
│  └──────────┼───────────────┼─────────────────┼────────────┘     │
│             │               │                  │                  │
│  ┌──────────▼───────────────▼──────────────────▼────────────┐    │
│  │                   Orchestrator Layer                      │    │
│  │   ┌────────────┐  ┌──────────┐  ┌───────┐  ┌──────────┐ │    │
│  │   │ Scenario   │  │ Device   │  │ Data  │  │ Callback │ │    │
│  │   │ Manager    │  │ Manager  │  │ Logger│  │ Handler  │ │    │
│  │   └────────────┘  └──────────┘  └───────┘  └──────────┘ │    │
│  └──────────────────────────────────────────────────────────┘    │
│                                                                     │
│  ┌──────────────────────────────────────────────────────────┐    │
│  │                   Hardware Abstraction Layer (HAL)        │    │
│  │   ┌───────────┐ ┌──────────┐ ┌────────┐ ┌───────────┐  │    │
│  │   │ BLE HAL   │ │ NFC HAL  │ │ UWB HAL│ │ CAN HAL   │  │    │
│  │   │ (nRF BLE) │ │ (pn5180) │ │ (SR250)│ │ (PCAN)    │  │    │
│  │   └───────────┘ └──────────┘ └────────┘ └───────────┘  │    │
│  │   ┌───────────┐ ┌──────────┐ ┌────────┐ ┌───────────┐  │    │
│  │   │ SE050 HAL │ │ JLink    │ │ Cloud  │ │ Attacker  │  │    │
│  │   │ (i2c)     │ │ HAL      │ │ Mock   │ │ Tools HAL │  │    │
│  │   └───────────┘ └──────────┘ └────────┘ └───────────┘  │    │
│  └──────────────────────────────────────────────────────────┘    │
│                                                                     │
│  ┌──────────────────────────────────────────────────────────┐    │
│  │                Infrastructure Layer                       │    │
│  │   ┌───────────┐ ┌──────────┐ ┌────────┐ ┌────────────┐ │    │
│  │   │ Docker    │ │ InfluxDB │ │Grafana │ │ InfluxDB   │ │    │
│  │   │ (Mock)    │ │ (Data)   │ │(Dash)  │ │ (Metrics)  │ │    │
│  │   └───────────┘ └──────────┘ └────────┘ └────────────┘ │    │
│  └──────────────────────────────────────────────────────────┘    │
└────────────────────────────────────────────────────────────────────┘
```

### 6.2 核心接口定义

```python
# === 抽象硬件接口 ===

class BLEHal(ABC):
    """BLE 硬件抽象层"""
    @abstractmethod
    def connect(self, address: str) -> bool: ...
    @abstractmethod
    def gatt_write(self, char_uuid: str, data: bytes) -> bool: ...
    @abstractmethod
    def gatt_read(self, char_uuid: str) -> bytes: ...
    @abstractmethod
    def gatt_notify(self, char_uuid: str, callback: Callable) -> None: ...
    @abstractmethod
    def disconnect(self) -> None: ...

class NFCHAL(ABC):
    """NFC 硬件抽象层"""
    @abstractmethod
    def detect_field(self) -> bool: ...
    @abstractmethod
    def send_apdu(self, apdu: bytes) -> bytes: ...
    @abstractmethod
    def select_aid(self, aid: bytes) -> bool: ...

class UWbHal(ABC):
    """UWB 硬件抽象层"""
    @abstractmethod
    def start_ranging(self, session_id: int, role: str) -> bool: ...
    @abstractmethod
    def get_distance(self) -> float: ...
    @abstractmethod
    def stop_ranging(self) -> None: ...

class CanHal(ABC):
    """CAN 总线硬件抽象层"""
    @abstractmethod
    def send_message(self, msg_id: int, data: bytes) -> bool: ...
    @abstractmethod
    def read_message(self, timeout: float) -> Optional[CANFrame]: ...

class Se050Hal(ABC):
    """SE050 安全芯片硬件抽象层"""
    @abstractmethod
    def read_status(self) -> dict: ...
    @abstractmethod
    def sign(self, key_id: int, data: bytes) -> bytes: ...
    @abstractmethod
    def verify(self, key_id: int, data: bytes, sig: bytes) -> bool: ...


# === 测试编排 ===

class HILTestOrchestrator:
    """HIL 测试编排器"""

    def __init__(self, config: dict):
        self.ble = create_ble_hal(config['ble'])
        self.nfc = create_nfc_hal(config['nfc'])
        self.uwb = create_uwb_hal(config['uwb'])
        self.can = create_can_hal(config['can'])
        self.se = create_se050_hal(config['se'])
        self.cloud = CloudMockClient(config['cloud_mock'])
        self.logger = DataLogger(config['influxdb'])

    def setup(self):
        """测试环境初始化"""
        self.logger.start_recording()
        self.ble.init()
        self.nfc.init()
        self.uwb.init()
        self.can.init()
        self.cloud.start()

    def teardown(self):
        """测试环境清理"""
        self.ble.disconnect()
        self.nfc.deinit()
        self.uwb.stop_ranging()
        self.cloud.stop()
        self.logger.stop_recording()

    def run_scenario(self, scenario: TestScenario) -> TestResult:
        """执行单个测试场景"""
        self.setup()
        try:
            scenario.execute(self)
        finally:
            self.teardown()
        return self.logger.get_result()
```

### 6.3 测试用例示例

```python
# === HIL 测试用例示例: NFC 解锁 ===

@pytest.mark.asyncio
@pytest.mark.hil
@pytest.mark.parametrize("protocol", ["CCC", "ICCOA", "ICCE"])
async def test_nfc_unlock(hil: HILTestOrchestrator, protocol: str):
    """HIL_NFC_001: NFC触发 → BLE建立 → 解锁"""

    # 1. 准备测试数据
    test_key = await hil.cloud.create_test_key(
        protocol=protocol,
        permissions=["unlock"]
    )

    # 2. NFC 触发
    assert hil.nfc.detect_field(), "NFC 场检测失败"

    # 3. NFC 读取车端信息
    aid = hil.nfc.select_aid(get_protocol_aid(protocol))
    assert aid, f"AID {get_protocol_aid(protocol)} 选择失败"

    challenge = hil.nfc.send_apdu(build_get_challenge_apdu(protocol))
    assert len(challenge) == 32, "Challenge 长度错误"

    # 4. BLE 自动建立连接
    ble_connected = await hil.ble.wait_for_connection(timeout=5.0)
    assert ble_connected, "BLE 自动连接超时"

    # 5. 安全通道建立
    session_key = await establish_secure_channel(hil, protocol)
    assert session_key, "安全通道建立失败"

    # 6. 发送解锁命令
    unlock_cmd = build_unlock_command(protocol, session_key)
    response = hil.ble.gatt_write(get_control_char_uuid(protocol), unlock_cmd)
    assert response, "解锁命令发送失败"

    # 7. 验证 CAN 报文
    can_msg = hil.can.read_message(timeout=2.0)
    assert can_msg and can_msg.msg_id == 0x101, "CAN 解锁报文未收到"
    assert can_msg.data[0] == 0x01, "解锁命令码错误 (应为 0x01)"

    # 8. 验证解锁状态
    status = hil.se.read_status()
    assert status.get('last_action') == 'UNLOCK', "SE050 未记录解锁操作"

    # 9. 记录测试数据
    hil.logger.record_metric("unlock_latency_ms", get_latency())
    hil.logger.record_metric("nfc_field_strength", hil.nfc.get_field_strength())
    hil.logger.record_metric("ble_rssi", hil.ble.get_rssi())

    # 10. 日志断言
    log = await hil.dut.get_serial_log()
    assert "[UNLOCK] SUCCESS" in log, "DUT 日志未记录解锁成功"
```

### 6.4 测试数据采集与仪表板

```python
# InfluxDB 数据点格式
hil_metrics = {
    "measurement": "hil_test_result",
    "tags": {
        "test_id": "HIL_NFC_001",
        "protocol": "CCC",
        "dut_version": "2.1.0-CCC.20260729",
        "result": "PASS"
    },
    "fields": {
        "unlock_latency_ms": 1250,
        "nfc_field_strength": 2.1,
        "ble_rssi": -65,
        "uwb_distance_cm": 45,
        "total_steps": 8,
        "failed_steps": 0
    },
    "time": "2026-07-29T10:00:00Z"
}
```

**Grafana 仪表板指标**：

- 测试通过率 (按协议栈 × 场景)
- 平均解锁延迟时间趋势
- BLE RSSI / NFC 场强分布
- 失败用例 TOP-N 排行
- 硬件故障频率统计

---

## 7. 测试执行计划

### 7.1 阶段划分

| 阶段 | 内容 | 周期 | 硬件准备 |
|------|------|------|----------|
| Phase 0 | HIL 环境搭建与校准 | Week 1-2 | 全部硬件到货并连接 |
| Phase 1 | 基础通信测试 (BLE/UWB/NFC) | Week 3-4 | 基础硬件 |
| Phase 2 | 协议合规测试 (CCC/ICCOA/ICCE) | Week 5-8 | 全部硬件 |
| Phase 3 | 端到端功能测试 (5 场景) | Week 9-12 | 全部硬件 + 手机 |
| Phase 4 | 安全攻击测试 | Week 13-14 | 全部硬件 + 攻击工具 |
| Phase 5 | 压力与稳定性测试 | Week 15-16 | 全部硬件 + 温度箱 |
| Phase 6 | 回归测试 & 报告输出 | Week 17-18 | 全部硬件 |

### 7.2 每日执行脚本

```bash
#!/bin/bash
# HIL 每日回归测试脚本
# 在 PC 上位机 (Ubuntu 22.04) 上执行

set -e

echo "=== yuleDKCS HIL 每日回归测试 ==="
date

# 1. 启动 Cloud Mock
docker-compose -f hil/docker-compose.yml up -d

# 2. 烧录 DUT 固件
JLinkExe -CommanderScript hil/scripts/flash_dut.jlink

# 3. 启动 DUT
hil/scripts/power_on_dut.sh

# 4. 等待 DUT 启动
sleep 10

# 5. 运行 HIL 测试套件
cd hil/tests

# 5.1 NFC 场景
pytest test_nfc.py -v --alluredir=../reports/nfc

# 5.2 UWB 场景
pytest test_uwb.py -v --alluredir=../reports/uwb

# 5.3 钥匙生命周期
pytest test_key_lifecycle.py -v --alluredir=../reports/keys

# 5.4 安全攻击
pytest test_security.py -v --alluredir=../reports/security

# 5.5 压力测试 (简短版 10min)
pytest test_stress.py -v -k "short" --alluredir=../reports/stress

# 6. 生成报告
allure generate ../reports -o ../reports/html --clean

# 7. 生成摘要
python ../scripts/generate_summary.py

echo "=== HIL 测试完成 ==="
```

### 7.3 测试数据保留策略

| 数据类型 | 保留期 | 存储方式 |
|----------|--------|----------|
| 测试结果 (PASS/FAIL) | 永久 | InfluxDB + Grafana |
| 详细测试日志 | 90 天 | 文件系统 (按日期归档) |
| DUT 串口日志 | 30 天 | 压缩存储 |
| 原始 CAN 数据 | 7 天 | 循环覆盖 |
| BLE Sniffer 包 | 7 天 | 循环覆盖 |
| 测试报告 (Allure) | 永久 | HTML 静态文件 |

---

## 8. 测试通过标准

### 8.1 总体标准

| 指标 | 通过条件 |
|------|----------|
| P0 用例通过率 | 100% |
| P1 用例通过率 | ≥ 95% |
| 无 Critical / Blocker 级别缺陷 | 必须满足 |
| 安全攻击测试 | 100% 正确阻断 |
| 压力测试 | 0 次崩溃 / 0 次内存泄漏 |

### 8.2 分项标准

#### 8.2.1 功能测试通过标准

| 测试项 | 通过条件 | 测量方法 |
|--------|----------|----------|
| NFC 交易成功率 | ≥ 99% (100次) | 计数器 + 日志 |
| BLE 连接成功率 | ≥ 98% (100次) | 自动连接统计 |
| UWB 测距精度 | 误差 < 10cm @ 1-10m | 与激光测距对比 |
| 安全通道建立 | 100% (100次) | 会话密钥一致性校验 |
| 车控指令响应 | ≤ 500ms (P95) | 端到端时间戳 |
| 钥匙分享流程 | 100% (20次) | 邀请→接收→使用 |

#### 8.2.2 安全测试通过标准

| 测试项 | 通过条件 |
|--------|----------|
| 中继攻击检测 | 100% 正确拒绝 |
| 重放攻击检测 | 100% 正确拒绝 |
| SCP03 认证 | 错误密钥 100% 拒绝 |
| 签名伪造检测 | 100% 拒绝 |
| 防回滚机制 | 旧版本固件拒绝启动 |
| JWT 重放 | 100% 拒绝重复 jti |

#### 8.2.3 压力测试通过标准

| 测试项 | 通过条件 |
|--------|----------|
| 72h 连续运行 | 无死机 / 无异常重启 |
| 10000 次解锁循环 | 成功率 ≥ 99.9% |
| 10 设备并发 | 全部正常连接 |
| 内存泄漏检查 | 72h 内内存增长 ≤ 5% |
| -20°C ~ +70°C 循环 | 功能正常 |

### 8.3 缺陷分类

| 严重级别 | 定义 | 测试阶段处理 |
|----------|------|-------------|
| Blocker | 阻塞后续测试执行 | 立即修复，重新测试 |
| Critical | 核心功能失效、安全漏洞 | 24h 内修复 |
| Major | 功能异常但可绕过 | 本迭代修复 |
| Minor | 非功能性问题 | 后续迭代修复 |

---

## 9. 附录

### A. HIL 环境安装检查清单

- [ ] S32G2 EVB 供电正常 (12V DC)
- [ ] J-Link 连接正常 (SWD 通信)
- [ ] 串口连接正常 (115200 8N1)
- [ ] nRF52840 DK 固件已刷为 BLE Sniffer
- [ ] PN5180 DK USB 驱动正常
- [ ] SR250 DK 已接入 UWB 天线
- [ ] PCAN-USB FD 驱动正常
- [ ] 频谱分析仪已校准
- [ ] 屏蔽箱射频接口连接正确
- [ ] Docker 容器 (Cloud Mock) 启动正常
- [ ] 测试手机已安装 yuleDKCS App 测试版
- [ ] InfluxDB + Grafana 已配置

### B. 已知限制

| 限制 | 说明 | 缓解措施 |
|------|------|----------|
| 屏蔽箱尺寸 | 无法容纳全尺寸车辆 | 仅测试电子模块，不涉及整车 CAN 总线 |
| 温度箱范围 | -40°C ~ +125°C | 车载场景仅需 -20°C ~ +70°C |
| UWB 锚点数量 | 最多 3 个锚点 | 实际车辆 4-6 锚点，多锚点协同测试另做 |
| 手机数量 | 最多 2 台 | 多用户场景用模拟器补充 |
| BLE 连接稳定性 | 屏蔽箱内天线衰减 | 使用外接天线 + 低损耗馈线 |

### C. 文档参考

| 文档 | 说明 |
|------|------|
| [EMBEDDED-DEV-GUIDE.md](design/EMBEDDED-DEV-GUIDE.md) | 嵌入式开发指南 (硬件平台) |
| [TEST-PLAN.md](design/TEST-PLAN.md) | 三端测试计划 |
| [SYSTEM_ARCHITECTURE.md](SYSTEM_ARCHITECTURE.md) | 系统架构文档 |
| [TEST_CASES_INTEGRATION.md](../embedded/test_suite/TEST_CASES_INTEGRATION.md) | 集成测试用例 |
| [TEST_CASES_CCC.md](../embedded/test_suite/TEST_CASES_CCC.md) | CCC 测试用例 |
| [TEST_CASES_STRESS.md](../embedded/test_suite/TEST_CASES_STRESS.md) | 压力测试用例 |
| [SECURITY_WHITEPAPER.md](SECURITY_WHITEPAPER.md) | 安全白皮书 |

---

*文档版本: v1.0 | 最后更新: 2026-07-29 | 密级: 内部*
