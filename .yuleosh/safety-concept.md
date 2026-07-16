# yuleDKCS — 安全概念 (Safety Concept)

> 基于: project-context.md, ISO 26262 流程, 数字钥匙行业安全标准
> 目标 ASIL: ASIL-B (关键功能), QM/ASIL-A (非关键功能)
> 生成时间: 2026-07-07

---

## 1. Item Definition

### 1.1 系统范围
数字钥匙系统 (Digital Key System, DKS) 实现通过智能手机/可穿戴设备对车辆进行无钥匙进入和启动。系统覆盖三端：

- **车端 (Embedded)**: ICCE/CCC/ICCOA 三协议栈运行在 S32G2/G3 MCU + SE050 安全芯片
- **手机端 (App)**: Android/iOS SDK + App，通过 BLE/UWB/NFC 与车端通信
- **云端 (Cloud)**: Go Hub + DKCS 服务 + Java Adapter，负责密钥分发和管理

### 1.2 系统边界

| 边界 | 包含 | 不包含 |
|:-----|:-----|:------|
| 功能边界 | 无钥匙进入/启动、钥匙分享、远程控车、钥匙吊销 | 车辆行驶控制、ADAS、动力系统控制 |
| 通信边界 | BLE/UWB/NFC (近场)、MQTT/TLS (远场) | CAN 总线控制以外的车内网络 |
| 安全边界 | 密钥生成存储、加密通信、身份认证 | PKI 根证书管理、OEM PKI 基础设施 |

### 1.3 接口定义

| 接口 | 方向 | 协议 | 说明 |
|:-----|:-----|:-----|:-----|
| BLE | 车↔手机 | BLE GATT | 连接建立、测距、指令通道 |
| UWB | 车↔手机 | UWB IEEE 802.15.4z | 精准测距 (厘米级) |
| NFC | 车↔手机 | ISO 14443 | 被动解锁 (刷卡模式) |
| MQTT/TLS | 手机↔云 | MQTT over TLS 1.3 | 远程指令、状态同步 |
| gRPC | 云↔车 | gRPC (TLS) | 云到车的密钥下发 |
| REST | 云↔App | HTTPS | 业务管理接口 |
| CAN FD | 车内部 | CAN FD | 与 BCM/GW 通信 |

## 2. Hazard Identification (HARA)

### 2.1 功能场景定义

| 编号 | 场景 | 涉及功能 | 操作模式 |
|:-----|:-----|:---------|:---------|
| SC-01 | 正常解锁：用户靠近车辆，自动解锁 | BLE+UWB 测距 → 解锁指令 | 驾驶员正常使用 |
| SC-02 | 正常上锁：用户离开，自动上锁 | BLE 断连 → 上锁 | 驾驶员离开 |
| SC-03 | NFC 刷卡解锁：手机/卡片贴车门 NFC 区 | NFC 交互 → 解锁 | 手机没电/离线 |
| SC-04 | 远程解锁：App 点击解锁 (远场) | MQTT → 云 → 车 | 远程操作 |
| SC-05 | 发动机启动：进入车内后一键启动 | BLE 安全会话 → 启动授权 | 驾驶员在位 |
| SC-06 | 钥匙分享：车主分享钥匙给家人 | 云密钥管理 → 加密分享 | 多用户场景 |
| SC-07 | 钥匙吊销：车主吊销已分享钥匙 | 云密钥管理 → 远程失效 | 安全事件 |
| SC-08 | 中继攻击：攻击者中继 BLE/UWB 信号 | 攻击者伪造测距 | 恶意攻击 |
| SC-09 | 密钥泄露：手机丢失/云服务器被攻破 | 密钥安全存储 | 安全事件 |

### 2.2 危害识别与分类

| 编号 | 危害事件 | 触发场景 | S | E | C | ASIL |
|:-----|:---------|:---------|:-:|:-:|:-:|:----:|
| H-01 | 非预期解锁 (无有效钥匙车辆被解锁) | SC-01/03/04 被绕过 | S2 | E3 | C2 | **ASIL-B** |
| H-02 | 非预期启动 (非授权用户启动车辆) | SC-05 被绕过 | S2 | E3 | C3 | **ASIL-B** |
| H-03 | 解锁失败 (有效钥匙无法解锁) | SC-01/03 故障 | S1 | E2 | C1 | QM |
| H-04 | 启动失败 (有效钥匙无法启动) | SC-05 故障 | S1 | E2 | C1 | QM |
| H-05 | 中继攻击成功 (攻击者放大信号) | SC-08 | S2 | E2 | C2 | **ASIL-B(D)** |
| H-06 | 钥匙隐私泄露 (钥匙被克隆) | SC-09 | S2 | E1 | C3 | **ASIL-B** |
| H-07 | 远程非预期操作 (远程解锁/关窗等) | SC-04 认证被绕过 | S1 | E2 | C2 | **ASIL-A** |
| H-08 | 钥匙时效错误 (已吊销钥匙仍可用) | SC-07 失效 | S1 | E2 | C2 | **ASIL-A** |

**S (Severity):** S0=无伤害, S1=轻伤, S2=重伤可能, S3=致命可能
**E (Exposure):** E1=极低, E2=低, E3=中等, E4=高
**C (Controllability):** C1=易控制, C2=可控制, C3=难控制

## 3. Safety Goals

| ID | Safety Goal | ASIL | 关联危害 | FTTI |
|:---|:------------|:----:|:---------|:----:|
| SG-01 | The system SHALL prevent vehicle unlock without a valid authenticated digital key | **ASIL-B** | H-01 | <500ms |
| SG-02 | The system SHALL prevent engine start without a valid authenticated digital key present inside the vehicle | **ASIL-B** | H-02 | <500ms |
| SG-03 | The system SHALL detect and reject relay attacks on BLE/UWB ranging | **ASIL-B(D)** | H-05 | <100ms |
| SG-04 | The system SHALL protect digital key cryptographic material against extraction or cloning | **ASIL-B** | H-06 | N/A |
| SG-05 | The system SHALL authenticate all remote commands with cryptographic proof of possession | **ASIL-A** | H-07 | <1s |
| SG-06 | The system SHALL enforce key revocation within defined time bound | **ASIL-A** | H-08 | <10s |

## 4. FSC (Functional Safety Concept) 初步映射

| Safety Goal | 安全机制 | 实现层 | 验证方法 |
|:------------|:---------|:-------|:---------|
| SG-01 | BLE/UWB 双向认证 + 测距验证 | Embedded + App | 故障注入测试 |
| SG-01 | SE050 安全元件存储私钥 | Embedded SE | EAL6+ 认证 |
| SG-02 | 车内 BLE 连接验证 + 测距门限 | Embedded | 测距精度测试 |
| SG-03 | UWB 双向测距 + 随机挑战响应 | Embedded + App | 中继攻击测试 |
| SG-03 | BLE RSSI 多路径特征校验 | Embedded | 异常检测 |
| SG-04 | SE050 安全存储 + 反调试保护 | Embedded SE | 渗透测试 |
| SG-04 | 密钥从不在明文态离开 SE | Embedded SE | 密钥管理审计 |
| SG-05 | MQTT over TLS 1.3 + JWT 签名 | Cloud + App | 安全通信测试 |
| SG-06 | 云吊销列表 TTL ≤10s + 本地吊销缓存 | Cloud + Embedded | 吊销时效测试 |

## 5. 残留风险与假设

### 假设
1. SE050 安全元件硬件安全等级满足 EAL6+ (信赖芯片厂商)
2. 手机端 TEE 用于保护 App 内密钥 (信赖手机厂商)
3. OEM PKI 基础设施正常运行

### 残留风险
1. 物理接触攻击 (攻击者拆解车辆直接访问 CAN 总线) — 超出 Item 范围
2. 手机 OS 被 root/jailbreak 后 TEE 保护失效 — 需 App 端运行环境检测
3. 车载 UWB/BLE 模块的物理侧信道 — 需量产时确认模块抗侧信道能力
