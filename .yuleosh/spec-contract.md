# yuleDKCS — OpenSpec 契约层 (Spec Contract)

> **基于**: PRD.md, SYSTEM_ARCHITECTURE.md, API-CONTRACT.md, TEST-PLAN.md, safety-concept.md
> **版本**: 1.0.0 | **日期**: 2026-07-07 | **作者**: quality-architect
> **ASIL 继承**: safety-concept.md (SG-01~SG-06)
> **约束**: SHALL/SHALL NOT 为不可变需求，变更须全员（PM+架构+QA）同意

---

## 序言

### 项目概述

yuleDKCS 是一套完整的数字钥匙系统 (Digital Key System)，采用三端架构：
- **嵌入式端 (C)**: ICCE/CCC/ICCOA 三协议栈运行于 NXP S32G2/G3 + SE050 安全芯片
- **移动端 (Android/iOS)**: Kotlin/Swift SDK + App，通过 BLE/UWB/NFC 与车端通信
- **云端 (Go + Java)**: HUB 网关 + DKCS 核心服务 + Java 协议适配器

系统覆盖无钥匙进入/启动、钥匙分享、远程控车、钥匙吊销等完整功能生命周期，目标 ASIL-B(D) 安全等级。

### 业务目标

| 目标 | 量化指标 |
|:-----|:---------|
| 协议兼容 | 同时支持 ICCE + CCC + ICCOA 三大标准 |
| 解锁响应 | 车门解锁响应 ≤ 1 秒（距车 ≤ 2m） |
| 安全等级 | 满足 EAL6+（SE050 认证等级），端到端加密，防中继攻击 |
| 扩展能力 | 支持 ≥ 1,000,000 活跃用户，单平台 ≥ 100 车型 |
| 合规要求 | T/CA 110-2020 (ICCE) + CCC Digital Key 3.0 + ICCOA DK3.0/DK4.0 |

### ASIL 等级映射

| 等级 | 来源 | 适用范围 |
|:-----|:-----|:---------|
| **ASIL-B** | SG-01, SG-02, SG-04 | 非预期解锁/启动防护、密钥保护 |
| **ASIL-B(D)** | SG-03 | 中继攻击防护（建议 ASIL-B，可协商到 ASIL-D） |
| **ASIL-A** | SG-05, SG-06 | 远程控车鉴权、钥匙吊销时效 |
| **QM** | — | 非安全关键（NFC 刷卡、钥匙分享 UI、审计日志） |

---

## 1. 契约层：SHALL / SHALL NOT 需求

### 1.1 密钥生命周期管理 (Key Lifecycle)

| ID | 描述 | ASIL | 端 |
|:---|:-----|:-----|:--|
| KL-SHALL-01 | 系统 SHALL 支持数字钥匙的完整生命周期：创建(Created) → 预配对(Pre-Paired) → 配对完成(Paired) → 激活(Active) → 更新(Updated) → 吊销(Revoked) → 删除(Deleted) | ASIL-B | 全部 |
| KL-SHALL-02 | 系统 SHALL 在钥匙创建时使用非对称密钥对，私钥在手机 SE/TEE 中生成，公钥上传云端 | ASIL-B | App + Cloud |
| KL-SHALL-03 | 系统 SHALL 在配对过程中完成双向身份认证：车端验证手机签名，手机验证车端签名 | ASIL-B | Embedded + App |
| KL-SHALL-04 | 系统 SHALL 限制同一车辆绑定的有效数字钥匙数量 ≤ 10 把 | QM | Cloud |
| KL-SHALL-05 | 系统 SHALL 在钥匙状态变更（激活/暂停/吊销/过期）时通过 MQTT 推送实时同步至车端 | ASIL-B | Cloud + Embedded |
| KL-SHALL-06 | 系统 SHALL 允许车主暂停、恢复和吊销其名下车辆的任意钥匙 | ASIL-A | App + Cloud |
| KL-SHALL-07 | 系统 SHALL 确保钥匙更新（权限变更/有效期延长/密钥轮换）经密码学签名验证后才生效 | ASIL-B | 全部 |
| KL-SHALL-08 | 系统 SHALL 在钥匙创建和配对过程中，通过安全通道传输所有密钥材料，不允许明文密钥暴露于网络 | ASIL-B | 全部 |
| KL-SHALL-NOT-01 | 系统 SHALL NOT 允许越权用户（非钥匙持有者/非车主）执行钥匙创建、更新或吊销操作 | ASIL-B | 全部 |
| KL-SHALL-NOT-02 | 系统 SHALL NOT 在未经配对确认的情况下激活任何数字钥匙 | ASIL-B | 全部 |

### 1.2 被动无感解锁/上锁 (Passive Entry/Exit)

| ID | 描述 | ASIL | 端 |
|:---|:-----|:-----|:--|
| PE-SHALL-01 | 系统 SHALL 在手机靠近车辆 ≤ 2m 时自动完成 BLE 连接 + UWB 测距 + 双向认证，总延迟 ≤ 1 秒 | ASIL-B | Embedded + App |
| PE-SHALL-02 | 系统 SHALL 在车主离开车辆 ≥ 5m 且超过 30 秒后自动上锁所有车门 | ASIL-B | Embedded |
| PE-SHALL-03 | 系统 SHALL 在上锁前确认车内无有效数字钥匙（防止钥匙锁在车内） | ASIL-B | Embedded |
| PE-SHALL-04 | 系统 SHALL 在解锁指令执行前，完成 UWB 距离验证（距离 < 2m 阈值）和 BLE RSSI 交叉校验 | ASIL-B(D) | Embedded + App |
| PE-SHALL-05 | 系统 SHALL 每次解锁/上锁操作使用新的一次性随机数 (Nonce)，防止重放攻击 | ASIL-B | Embedded + App |
| PE-SHALL-06 | 系统 SHALL 在解锁/上锁成功后通过 CAN FD 向 BCM/GW 发送对应指令 | QM | Embedded |
| PE-SHALL-07 | 系统 SHALL 在解锁/上锁时提供视觉/声音反馈（车灯闪烁/鸣笛） | QM | Embedded |
| PE-SHALL-08 | 系统 SHALL 支持 BLE Central (Peripheral) 角色，保持与最多 8 台设备的并发 BLE 连接 | QM | Embedded |
| PE-SHALL-NOT-01 | 系统 SHALL NOT 在 UWB 测距距离 > 2m 时执行解锁指令 | ASIL-B(D) | Embedded + App |
| PE-SHALL-NOT-02 | 系统 SHALL NOT 在无有效经过认证的数字钥匙时执行任何车辆解锁/车门打开操作 | ASIL-B | 全部 |

### 1.3 NFC 刷卡解锁 (NFC Tap)

| ID | 描述 | ASIL | 端 |
|:---|:-----|:-----|:--|
| NF-SHALL-01 | 系统 SHALL 支持通过 NFC 被动刷卡方式解锁车门（手机电量耗尽/离线时仍可用） | QM | Embedded + App |
| NF-SHALL-02 | 系统 SHALL 在 NFC 交互中使用 ISO/IEC 7816-4 APDU 协议完成 SELECT → GET CHALLENGE → INTERNAL AUTHENTICATE → CONTROL 完整交易序列 | QM | Embedded + App |
| NF-SHALL-03 | 系统 SHALL 支持 CCC Digital Key AID (`A000000F5A3`) 和 ICCE AID (`A000000F5A3ICCE`) 两种应用选择 | QM | Embedded |
| NF-SHALL-04 | 系统 SHALL 在 NFC 刷卡解锁时完成芯片级安全认证（签名验证），认证失败则拒绝解锁 | ASIL-B | Embedded |
| NF-SHALL-05 | NFC 刷卡解锁响应时间 SHALL ≤ 500ms（从手机触碰 NFC 读卡器到解锁成功反馈） | QM | Embedded + App |
| NF-SHALL-NOT-01 | 系统 SHALL NOT 在 NFC 交互超时或卡片会话异常中断后，残留任何未提交的解锁状态 | QM | Embedded |

### 1.4 远程控车 (Remote Vehicle Control)

| ID | 描述 | ASIL | 端 |
|:---|:-----|:-----|:--|
| RC-SHALL-01 | 系统 SHALL 要求所有远程控车指令携带 JWT 签名和密钥签名双重认证 | ASIL-A | App + Cloud |
| RC-SHALL-02 | 系统 SHALL 支持以下远程控车动作：解锁/上锁、启动/停止发动机、闪灯鸣笛、空调控制、车窗控制 | ASIL-A | Cloud + Embedded |
| RC-SHALL-03 | 远程控车指令从 App 发出到车端执行的端到端响应时间 SHALL ≤ 3s | QM | 全部 |
| RC-SHALL-04 | 系统 SHALL 在远程控车指令经过云端时记录完整审计日志（用户ID、钥匙ID、操作类型、时间戳、结果） | ASIL-A | Cloud |
| RC-SHALL-05 | 远程控车指令 SHALL 通过 MQTT over TLS 1.3 通道下发至车端 | ASIL-A | Cloud + Embedded |
| RC-SHALL-06 | 系统 SHALL 在车端离线时返回明确的"车辆离线"状态给 App 端 | QM | Cloud |
| RC-SHALL-07 | 系统 SHALL 支持远程控车指令的状态查询（PENDING/EXECUTING/EXECUTED/FAILED/TIMEOUT） | QM | Cloud |
| RC-SHALL-NOT-01 | 系统 SHALL NOT 执行缺少有效密钥签名的远程控车指令 | ASIL-A | Cloud + Embedded |
| RC-SHALL-NOT-02 | 系统 SHALL NOT 允许临时钥匙或权限不足的钥匙执行 START_ENGINE 等受限操作 | ASIL-B | Cloud + Embedded |

### 1.5 发动机启动 (Engine Start)

| ID | 描述 | ASIL | 端 |
|:---|:-----|:-----|:--|
| ES-SHALL-01 | 系统 SHALL 仅在车内检测到有效经认证的数字钥匙时授权发动机启动 | ASIL-B | Embedded + App |
| ES-SHALL-02 | 系统 SHALL 在启动授权前建立 BLE 安全会话并完成双向签名验证 | ASIL-B | Embedded + App |
| ES-SHALL-03 | 系统 SHALL 确认车内测距结果（UWB 检出手机位于驾驶舱内）后才发送启动授权 | ASIL-B | Embedded |
| ES-SHALL-04 | 发动机启动授权响应时间 SHALL ≤ 500ms | ASIL-B | Embedded + App |
| ES-SHALL-NOT-01 | 系统 SHALL NOT 在没有有效经认证数字钥匙的情况下授权发动机启动 | ASIL-B | Embedded + App |
| ES-SHALL-NOT-02 | 系统 SHALL NOT 允许已吊销/暂停/过期的钥匙授权发动机启动 | ASIL-B | Embedded |

### 1.6 钥匙分享 (Key Sharing)

| ID | 描述 | ASIL | 端 |
|:---|:-----|:-----|:--|
| KS-SHALL-01 | 系统 SHALL 支持车主创建三种级别钥匙分享：主钥匙(Full Access)、副钥匙(Limited Admin)、临时钥匙(Time/Location Restricted) | QM | App + Cloud |
| KS-SHALL-02 | 系统 SHALL 支持四种分享方式：二维码分享、链接分享、NFC 碰一碰分享、手机号直接推送 | QM | App + Cloud |
| KS-SHALL-03 | 系统 SHALL 支持以下分享约束设置：时间窗口（开始/结束时间）、使用次数上限、地理围栏 | QM | App + Cloud |
| KS-SHALL-04 | 系统 SHALL 在车主撤销分享后，被分享方的钥匙在 < 10s 内失效 | ASIL-A | Cloud + Embedded + App |
| KS-SHALL-05 | 分享创建请求 SHALL 在 30 秒内完成云端处理和分享链接/码生成 | QM | Cloud |
| KS-SHALL-06 | 系统 SHALL 完整记录每次钥匙分享的创建、接受、使用和撤销事件 | QM | Cloud |
| KS-SHALL-07 | 系统 SHALL 在受邀者首次接受分享钥匙时，要求受邀者注册/登录并通过身份认证 | QM | App + Cloud |
| KS-SHALL-NOT-01 | 系统 SHALL NOT 允许非车主用户创建或撤销钥匙分享 | QM | Cloud |
| KS-SHALL-NOT-02 | 系统 SHALL NOT 允许被分享钥匙超出其权限约束（时间/次数/地理范围）执行操作 | ASIL-A | 全部 |

### 1.7 钥匙吊销 (Key Revocation)

| ID | 描述 | ASIL | 端 |
|:---|:-----|:-----|:--|
| KR-SHALL-01 | 系统 SHALL 支持云端即时吊销数字钥匙，云端吊销列表 TTL ≤ 10s | ASIL-A | Cloud |
| KR-SHALL-02 | 系统 SHALL 在车端维护本地吊销缓存，车辆下次联网时同步更新 | ASIL-A | Cloud + Embedded |
| KR-SHALL-03 | 系统 SHALL 在被吊销钥匙尝试操作时，车端本地缓存能独立判定吊销状态并拒绝 | ASIL-A | Embedded |
| KR-SHALL-04 | 系统 SHALL 在钥匙吊销后通过推送通知告知钥匙持有者 | QM | Cloud + App |
| KR-SHALL-05 | 吊销操作 SHALL 记录完整的审计日志（操作人、时间、原因、关联钥匙） | QM | Cloud |
| KR-SHALL-NOT-01 | 系统 SHALL NOT 允许被吊销的钥匙在吊销操作完成后继续执行任何车辆操作 | ASIL-A | Embedded + Cloud |

### 1.8 防中继攻击 (Relay Attack Protection)

| ID | 描述 | ASIL | 端 |
|:---|:-----|:-----|:--|
| RA-SHALL-01 | 系统 SHALL 使用 UWB 物理层 ToF (Time of Flight) 测距测量手机与车辆的真实距离 | ASIL-B(D) | Embedded + App |
| RA-SHALL-02 | 系统 SHALL 对每次测距使用一次性随机数 (Nonce)，防止测距结果重放 | ASIL-B(D) | Embedded + App |
| RA-SHALL-03 | 系统 SHALL 验证测距结果的签名，确保测距数据未被中间人篡改 | ASIL-B(D) | Embedded + App |
| RA-SHALL-04 | 系统 SHALL 在解锁指令的响应时间超出协议规定阈值（~3μs）时拒绝执行 | ASIL-B(D) | Embedded + App |
| RA-SHALL-05 | 系统 SHALL 实现 BLE RSSI + UWB 距离多因子交叉验证：若 BLE RSSI 估算距离与 UWB 测距结果显著不一致，拒绝解锁 | ASIL-B(D) | Embedded + App |
| RA-SHALL-06 | 系统 SHALL 实现防重放计数器，拒绝计数器值小于或等于上次已接收值的消息 | ASIL-B | Embedded + App |
| RA-SHALL-07 | 系统 SHALL 在检测到疑似中继攻击时记录安全事件并推送告警至车主 App | ASIL-B(D) | Embedded + Cloud |
| RA-SHALL-NOT-01 | 系统 SHALL NOT 在未完成 UWB 安全测距的情况下仅凭 BLE 连接执行解锁 | ASIL-B(D) | Embedded + App |
| RA-SHALL-NOT-02 | 系统 SHALL NOT 允许同一 Nonce 值被使用两次（重放攻击） | ASIL-B | Embedded + App |

### 1.9 密钥存储与安全 (Key Storage & Security)

| ID | 描述 | ASIL | 端 |
|:---|:-----|:-----|:--|
| KSS-SHALL-01 | 车端私钥 SHALL 存储于 SE050 安全芯片内，任何软件层无法以明文形式读取私钥 | ASIL-B | Embedded |
| KSS-SHALL-02 | 手机端私钥 SHALL 存储于 SE/TEE 安全区域（iOS Keychain / Android KeyStore）内 | ASIL-B | App |
| KSS-SHALL-03 | 所有密码学运算（签名/解密/密钥派生）SHALL 在 SE/TEE 安全环境中执行，而非在通用 CPU/内存中 | ASIL-B | Embedded + App |
| KSS-SHALL-04 | SE050 SHALL 满足 EAL6+（安全芯片认证等级）及以上 | ASIL-B | Embedded |
| KSS-SHALL-05 | 系统 SHALL 支持多重密钥层级：Root Key → Master Key → Device Key → Session Key，各级密钥通过 HKDF 派生 | ASIL-B | Embedded + App |
| KSS-SHALL-06 | 会话密钥 (Session Key) SHALL 每次连接通过 ECDH (P-256/SM2) 密钥协商生成，用完即销毁 | ASIL-B | Embedded + App |
| KSS-SHALL-07 | 系统 SHALL 支持 ICCE 模式下的国密算法 (SM2/SM3/SM4) 和 CCC 模式下的国际算法 (ECDSA/AES-256-GCM) | ASIL-B | 全部 |
| KSS-SHALL-08 | 系统 SHALL 支持安全启动链：Boot ROM → BootLoader(SE050验签) → TFM → Application 逐级校验 | ASIL-B | Embedded |
| KSS-SHALL-NOT-01 | 密钥材料 SHALL NOT 以明文形式离开安全环境 (SE/TEE/HSM) | ASIL-B | 全部 |
| KSS-SHALL-NOT-02 | 系统 SHALL NOT 在生产环境中使用 Mock HSM 或模拟安全元件执行密码学操作 | ASIL-B | Cloud + Embedded |

### 1.10 通信安全 (Communication Security)

| ID | 描述 | ASIL | 端 |
|:---|:-----|:-----|:--|
| CM-SHALL-01 | 手机与云端之间所有通信 SHALL 使用 TLS 1.3 加密 | ASIL-A | App + Cloud |
| CM-SHALL-02 | 云端与车端之间所有通信 SHALL 使用 MQTT over TLS 1.3 或 gRPC over TLS | ASIL-B | Cloud + Embedded |
| CM-SHALL-03 | 手机与车端 BLE 通信 SHALL 使用 LE Secure Connections (LE SC) 建立安全认证链路 | ASIL-B | Embedded + App |
| CM-SHALL-04 | BLE GATT 连接参数 SHALL 满足：连接间隔 30ms~50ms、MTU ≥ 512 bytes | QM | Embedded + App |
| CM-SHALL-05 | 内部协议消息 SHALL 使用 BER-TLV 编码（紧凑、标准化） | QM | 全部 |
| CM-SHALL-06 | 所有远程控车接口（REST API）SHALL 使用 Bearer Token (JWT) 鉴权 | ASIL-A | App + Cloud |
| CM-SHALL-07 | JWT Access Token 有效期 SHALL ≤ 1 小时，Refresh Token SHALL ≤ 7 天 | ASIL-A | Cloud |
| CM-SHALL-08 | 系统 SHALL 在云端 REST API 对所有关键操作（钥匙创建/吊销/分享、远程控车）进行细粒度权限校验 | ASIL-A | Cloud |
| CM-SHALL-NOT-01 | 系统 SHALL NOT 使用未加密的明文信道传输任何密钥材料、认证令牌或车辆控制指令 | ASIL-B | 全部 |
| CM-SHALL-NOT-02 | 系统 SHALL NOT 允许缺少有效 JWT Token 或 Token 已过期的 API 请求通过 | ASIL-A | Cloud |

### 1.11 OTA 升级 (OTA Update)

| ID | 描述 | ASIL | 端 |
|:---|:-----|:-----|:--|
| OT-SHALL-01 | 系统 SHALL 支持通过 OTA 方式升级车端固件 | QM | Cloud + Embedded |
| OT-SHALL-02 | OTA 升级包 SHALL 经过数字签名，车端在安装前 SHALL 验证签名完整性 | ASIL-B | Embedded |
| OT-SHALL-03 | 系统 SHALL 支持 OTA 升级状态追踪（DOWNLOAD_PENDING → DOWNLOADING → VERIFYING → INSTALLING → REBOOTING → COMPLETED/FAILED） | QM | Cloud + Embedded |
| OT-SHALL-NOT-01 | 系统 SHALL NOT 安装签名校验失败的 OTA 升级包 | ASIL-B | Embedded |

### 1.12 用户认证与会话管理 (User Auth & Session)

| ID | 描述 | ASIL | 端 |
|:---|:-----|:-----|:--|
| UA-SHALL-01 | 系统 SHALL 支持手机号+验证码、第三方 OAuth（微信/Apple ID/Google）等多种登录方式 | QM | App + Cloud |
| UA-SHALL-02 | 系统 SHALL 基于 OAuth 2.0 / OpenID Connect 协议实现用户认证 | ASIL-A | Cloud |
| UA-SHALL-03 | 系统 SHALL 在车主证明环节要求用户通过 VIN 码 + 购车证明或车厂 API 验证车辆所有权 | QM | App + Cloud |
| UA-SHALL-04 | 系统 SHALL 支持多因子认证 (MFA)：短信验证码、生物识别等 | QM | App + Cloud |
| UA-SHALL-05 | 系统 SHALL 基于 RBAC + ABAC 实现细粒度权限控制 | ASIL-A | Cloud |
| UA-SHALL-NOT-01 | 系统 SHALL NOT 允许未通过车辆所有权验证的用户创建该车辆的主钥匙 | QM | Cloud |

### 1.13 审计日志 (Audit Logging)

| ID | 描述 | ASIL | 端 |
|:---|:-----|:-----|:--|
| AL-SHALL-01 | 系统 SHALL 记录所有钥匙生命周期变更（创建、激活、暂停、恢复、吊销、过期）的审计日志 | QM | Cloud |
| AL-SHALL-02 | 系统 SHALL 记录所有车辆控制和车门操作（解锁、上锁、启动、寻车等）的审计日志 | QM | 全部 |
| AL-SHALL-03 | 审计日志 SHALL 包含：操作人、关联钥匙、操作类型、时间戳、设备信息、地理位置、操作结果 | QM | Cloud |
| AL-SHALL-04 | 审计日志保留期限 SHALL ≥ 3 年 | QM | Cloud |
| AL-SHALL-05 | 系统 SHALL 记录安全事件（认证失败、中继攻击检测、异常权限使用）到独立的安全事件日志 | QM | 全部 |
| AL-SHALL-06 | 审计日志 SHALL 通过消息队列 (Kafka Topic: `digitalkey.audit`) 异步写入 | QM | Cloud |
| AL-SHALL-NOT-01 | 系统 SHALL NOT 允许非授权用户删除或篡改审计日志 | QM | Cloud |

### 1.14 双协议支持 (Dual Protocol Support)

| ID | 描述 | ASIL | 端 |
|:---|:-----|:-----|:--|
| DP-SHALL-01 | 系统 SHALL 同时支持 ICCE、(T/CA 110-2020) 和 CCC (Digital Key 3.0) 两种数字钥匙协议 | QM | 全部 |
| DP-SHALL-02 | 车端固件 SHALL 同时包含 ICCE 和 CCC 协议栈，配对时自动协商协议类型 | QM | Embedded |
| DP-SHALL-03 | 云端 SHALL 同时管理 ICCE 国密证书和 CCC X.509 证书 | QM | Cloud |
| DP-SHALL-04 | App SHALL 根据车辆 VIN 自动识别并选用对应协议，用户无感知 | QM | App |
| DP-SHALL-NOT-01 | ICCE 模式 SHALL NOT 使用国际算法（ECDSA/AES）替代国密算法（SM2/SM3/SM4） | QM | 全部 |
| DP-SHALL-NOT-02 | CCC 模式 SHALL NOT 使用国密算法替代国际算法（ECDSA/P-256/AES-256-GCM） | QM | 全部 |

### 1.15 离线模式 (Offline Mode)

| ID | 描述 | ASIL | 端 |
|:---|:-----|:-----|:--|
| OM-SHALL-01 | 系统 SHALL 在手机无网络连接时，通过 NFC 刷卡方式仍然可执行车辆解锁 | QM | Embedded + App |
| OM-SHALL-02 | 预下载的离线钥匙授权数据在有效期内 SHALL 可在无网络环境下使用 | QM | App |
| OM-SHALL-03 | 离线期间的操作记录（解锁/上锁等）在网络恢复后 SHALL 自动同步至云端 | QM | App + Cloud |
| OM-SHALL-NOT-01 | 离线钥匙 SHALL NOT 在过期后仍可用于车辆解锁或启动 | ASIL-B | Embedded + App |

---

## 2. SHOULD / MAY 需求

### 2.1 SHOULD（建议实现）

| ID | 描述 | 依据 | 端 |
|:---|:-----|:-----|:--|
| SHOULD-01 | 车门解锁响应时间 SHOULD ≤ 800ms（目标优于 1 秒阈值） | PRD 性能 | Embedded + App |
| SHOULD-02 | App 冷启动时间 SHOULD ≤ 2s | PRD 6.1 | App |
| SHOULD-03 | 云端 API P99 响应时间 SHOULD ≤ 200ms | PRD 6.1 | Cloud |
| SHOULD-04 | 服务可用性 SHOULD ≥ 99.9% | PRD 6.2 | Cloud |
| SHOULD-05 | App SHOULD 在检测到设备被 root/jailbreak 后拒绝使用敏感功能（解锁、启动） | TEST-PLAN AS-SEC-02 | App |
| SHOULD-06 | BLE SHOULD 采用 2M PHY 提升数据传输速率 | 架构最佳实践 | Embedded + App |
| SHOULD-07 | 系统 SHOULD 支持 ICCOA DK3.0/DK4.0 协议作为第三协议 | 架构规划 | 全部 |
| SHOULD-08 | UWB 测距结果 SHOULD 包含方位角和俯仰角数据，支持三维定位 | PRD 3.1.2 | Embedded |
| SHOULD-09 | 系统 SHOULD 在车端离线时在本地存储吊销缓存，确保下次联网时立即同步 | safety-concept | Embedded |
| SHOULD-10 | 钥匙分享的地理围栏限制 SHOULD 支持多边形 (Polygon) 和区域列表 (Region) 两种类型 | PRD US2.3 | Cloud + App |
| SHOULD-11 | 系统 SHOULD 在钥匙过期前 7 天/3 天/1 天分别推送提醒通知 | 用户体验 | Cloud + App |
| SHOULD-12 | UWB 测距 SHOULD 支持 SE050 签名的安全测距，测距结果附加密码学签名 | IEEE 802.15.4z | Embedded |

### 2.2 MAY（可选）

| ID | 描述 | 依据 | 端 |
|:---|:-----|:-----|:--|
| MAY-01 | 系统 MAY 支持可穿戴设备（手表/手环）作为数字钥匙载体 | PRD 1.1 | Embedded + App |
| MAY-02 | 系统 MAY 支持通过 App 查看车辆 360° 影像/哨兵模式视频 | 扩展 | Cloud + App |
| MAY-03 | 系统 MAY 支持无感车控（靠近自动解锁 + 离开自动上锁）的灵敏度用户可调 | 体验 | Embedded |
| MAY-04 | 系统 MAY 支持第三方出行平台/分时租赁平台的 API 接入 | PRD 1.3 | Cloud |
| MAY-05 | 系统 MAY 支持基于 UWB 的精确车内定位（区分主驾/副驾/后排） | PRD 3.1.2 | Embedded |
| MAY-06 | 系统 MAY 在钥匙分享时支持代驾模式（限制最高速度/行李箱/手套箱） | PRD 2.2.3 | Cloud + Embedded |
| MAY-07 | 系统 MAY 支持云端自动检测异常行为（如异常时间段解锁、异常地理位置）并触发告警 | 安全 | Cloud |
| MAY-08 | 系统 MAY 支持车主通过 App 远程授权临时钥匙给未安装 App 的用户（短信分享） | 体验 | App + Cloud |

---

## 3. GIVEN / WHEN / THEN 场景

### 场景 S-01：车主首次创建数字钥匙

```
GIVEN 车主已登录数字钥匙 App 并拥有已验证的车辆
  AND 车辆已激活数字钥匙服务（车厂开通）
  AND 手机支持 BLE/NFC/UWB
WHEN 车主在 App 中选择"添加车辆"并输入 VIN
  AND 车主扫描车端显示的配对二维码
  AND App 通过 BLE 连接车端
  AND 车主完成身份认证（短信/人脸）
THEN 手机在 SE/TEE 中生成非对称密钥对
  AND 公钥通过安全通道上传至云端
  AND 云端生成钥匙模板并下发至车端
  AND 车端在 SE050 中存储钥匙公钥信息
  AND 钥匙状态变为"已激活"
  AND App 显示正确车辆信息
  AND 整个配对流程在 3 分钟内完成
  AND 同一车辆最多绑定 5 把有效钥匙
```

### 场景 S-02：日常无感解锁与上锁

```
GIVEN 车主已成功配对并持有激活的数字钥匙
  AND 车端 BLE 正在广播/UWB 待命
  AND 手机后台运行数字钥匙 App
WHEN 车主携带手机走近车辆 ≤ 2m
  AND BLE 连接建立
  AND UWB 测距开始（Nonce 挑战/响应）
  AND UWB 测距距离 < 2m
  AND 手机完成密码学签名挑战验证
THEN 车端执行 CAN FD 解锁指令
  AND 车灯闪烁/蜂鸣确认
  AND 从靠近到解锁成功 ≤ 1 秒
  AND 整个解锁过程无需用户解锁手机屏幕

GIVEN 如前条件
WHEN 车主离开车辆 ≥ 5m
  AND BLE 连接断开
  AND 30 秒超时计时开始
  AND 车内无有效数字钥匙检出
THEN 车端自动上锁所有车门
  AND 车灯闪烁确认
```

### 场景 S-03：NFC 刷卡解锁

```
GIVEN 车主持有已配对且激活的数字钥匙
  AND 手机可能处于电量耗尽/离线状态
WHEN 车主将手机 NFC 区域贴近车门 NFC 读卡器
THEN 车端 NFC 模块通过 ISO 14443 协议与手机通信
  AND 手机通过 SELECT AID 选择 CCC/ICCE 数字钥匙应用
  AND 车端发送随机挑战 (GET CHALLENGE)
  AND 手机返回签名认证 (INTERNAL AUTHENTICATE)
  AND 车端使用 SE050 验证签名
  AND 签名验证通过后执行解锁
  AND NFC 刷卡解锁总时间 ≤ 500ms
```

### 场景 S-04：远程解锁

```
GIVEN 车主已持有激活的数字钥匙
  AND 车端已联网（MQTT 在线）
  AND 车主 App 已通过 JWT 鉴权
WHEN 车主在 App 中点击"解锁"按钮
THEN App 发送带密钥签名的远程解锁请求至 HUB
  AND HUB 验证 JWT Token
  AND HUB 将指令转发至 DKCS
  AND DKCS 验证钥匙权限和有效期
  AND DKCS 通过 MQTT 下发解锁指令至车端
  AND 车端验证指令签名后执行 CAN FD 解锁
  AND 端到端响应 ≤ 3s
  AND 操作记录写入审计日志
```

### 场景 S-05：发动机启动

```
GIVEN 车主已持有有效数字钥匙
  AND 车主位于驾驶舱内
  AND BLE 安全会话已建立
WHEN 车主按下车内一键启动按钮
  AND 车端发起发动机启动授权挑战
  AND 手机完成签名响应
  AND UWB 确认手机位于驾驶舱内
THEN 车端 SE050 验证手机签名
  AND 车端发送 CAN FD 启动授权
  AND 发动机启动
  AND 启动响应时间 ≤ 500ms
```

### 场景 S-06：钥匙分享与使用

```
GIVEN 车主持有主钥匙，已登录 App
WHEN 车主在 App 中选择"分享钥匙"
  AND 选择"临时钥匙"类型
  AND 设置时间限制（2 小时）
  AND 设置地理围栏
  AND 生成分享链接
THEN 云端创建分享记录
  AND 分享链接在 30 秒内生成
  AND 受邀者收到分享链接或通知

GIVEN 受邀者收到分享链接
WHEN 受邀者点击链接并注册/登录 App
  AND 接受钥匙分享
THEN 云端为受邀者生成独立的数字钥匙
  AND 钥匙有效期 = 2 小时
  AND 钥匙受地理围栏约束
  AND 受邀者可在授权范围内解锁车辆

GIVEN 授权时间已到期
WHEN 受邀者尝试解锁车辆
THEN 车端本地缓存判定钥匙已过期
  AND 云端拒绝授权请求
  AND 解锁不执行
```

### 场景 S-07：钥匙吊销

```
GIVEN 车主已分享钥匙给家庭成员
WHEN 车主在 App 中选择"撤销分享"
  AND 确认撤销操作
THEN 云端将钥匙状态标记为 REVOKED
  AND 云端吊销列表 TTL 设置为 ≤ 10s
  AND 推送通知告知被分享者
  AND 车端下次联网时同步吊销列表
  AND 审计日志记录完整吊销事件

GIVEN 钥匙已被云端吊销
  AND 车端尚未联网（离线状态无吊销缓存）
WHEN 被分享者尝试使用已吊销的钥匙通过近距离解锁
THEN 车端本地安全会话认证失败（签名验证失败或 SE050 拒绝）
  AND 解锁不执行
```

### 场景 S-08：中继攻击防护

```
GIVEN 攻击者使用信号放大器/延迟器尝试中继 BLE+UWB 信号
WHEN 攻击者尝试中继手机与车辆之间的测距信号
  AND UWB 飞行时间测量值大于物理距离对应的时延
  OR 测距响应时间超出协议阈值（~3μs）
  OR BLE RSSI 估算距离与 UWB 距离显著不一致
  OR Nonce 值重复
THEN 系统判定为中继攻击
  AND 拒绝执行解锁指令
  AND 记录安全事件日志
  AND 可选：推送告警至车主 App
```

### 场景 S-09：多设备同时连接

```
GIVEN 车辆已激活数字钥匙服务
  AND 多个家庭成员的手机均在车辆附近
WHEN 车辆 BLE 模块同时与最多 8 台手机保持连接
THEN 每台手机独立完成 UWB 测距
  AND 每台手机独立通过认证
  AND 车辆可正确识别距离最近且有对应权限的钥匙持有者
  AND 执行对应权限允许的操作
```

### 场景 S-10：ICC/CCC 双协议兼容

```
GIVEN 车辆同时支持 ICCE 和 CCC 协议
WHEN 手机 App 通过 BLE 广播发现车辆
  AND App 根据车辆 VIN/能力识别协议类型
THEN App 自动选用对应协议栈
  AND ICCE 模式下全程使用 SM2/SM3/SM4 国密算法
  AND CCC 模式下全程使用 ECDSA P-256/AES-256-GCM 国际算法
  AND 用户完全无感知协议切换
```

### 场景 S-11：车端安全启动失败

```
GIVEN 车端 TCU 系统启动
WHEN Boot ROM 加载 BootLoader
  AND SE050 校验 BootLoader 签名
  AND 签名验证失败
THEN 系统终止启动流程
  AND 系统进入安全状态（锁定 + 告警日志）
  AND 不加载任何后续固件
```

### 场景 S-12：钥匙分享越权操作拒绝

```
GIVEN 被分享者持有临时钥匙（仅允许解锁/上锁，不允许启动）
WHEN 被分享者尝试通过远程控车或近场通信启动发动机
THEN 云端权限校验拒绝（403 Forbidden）
  AND 车端本地权限检查拒绝
  AND 发动机不启动
  AND 操作被记录为越权尝试
```

---

## 4. 验收判定矩阵

| 功能域 | 验收项 | 判定方法 | 判定标准 | ASIL | 端 |
|:-------|:-------|:---------|:---------|:-----|:--|
| 密钥生命周期 | 完整生命周期覆盖 | 状态机测试 | Created→Pre-Paired→Paired→Active→Updated→Revoked→Deleted 状态转换全部实现 | ASIL-B | 全部 |
| 密钥生命周期 | 配对完成时间 | 计时测试 | ≤ 3 分钟 | QM | App+Cloud+Embedded |
| 密钥生命周期 | 双向认证完整性 | 协议分析+注入测试 | 缺省签名/证书验证即拒绝配对 | ASIL-B | Embedded+App |
| 被动解锁 | 解锁响应时间（FTTI 对齐 SG-01） | 计时测试 | <500ms（靠近→解锁） | ASIL-B | Embedded+App |
| 被动解锁 | 自动上锁 | 场景测试 | 离车 ≥ 5m + 30s 后自动上锁 | ASIL-B | Embedded |
| 被动解锁 | 解锁前 UWB 距离验证 | 距离注入测试 | 距离 > 2m 时拒绝解锁 | ASIL-B(D) | Embedded+App |
| NFC 解锁 | NFC 刷卡解锁 | NFC 场景测试 | 手机没电/离线时锁定 NFC 刷卡可解锁 | QM | Embedded+App |
| NFC 解锁 | NFC 交易完整序列 | APDU 追踪测试 | SELECT→CHALLENGE→AUTH→CONTROL 完整执行 | QM | Embedded+App |
| 远程控车 | 远程解锁端到端 | E2E 计时测试 | App 点击到车门解锁 ≤ 3s | QM | 全部 |
| 远程控车 | JWT+密钥双重认证 | 安全测试 | 缺省任一认证即拒 | ASIL-A | App+Cloud |
| 远程控车 | 权限校验 | RBAC 测试 | 临时钥匙启动→403 | ASIL-B | Cloud+Embedded |
| 发动机启动 | 车内认证+启动 | 场景测试 | 仅车内 BLE+UWB 检出后启动 | ASIL-B | Embedded+App |
| 钥匙分享 | 分享创建时间 | 计时测试 | ≤ 30s | QM | Cloud |
| 钥匙分享 | 时间限制 | T+延时测试 | 超出有效期后钥匙失效 | QM | 全部 |
| 钥匙分享 | 即时撤销 | 撤销后尝试解锁 | 撤销后 < 10s 内钥匙失效 | ASIL-A | 全部 |
| 钥匙分享 | 地理围栏 | 模拟定位测试 | 超出围栏区域解锁申请→403 | QM | Cloud+App |
| 钥匙吊销 | 吊销列表 TTL | T+延时测试 | 云端 CRL TTL ≤ 10s | ASIL-A | Cloud |
| 钥匙吊销 | 车端本地吊销缓存 | 离线场景测试 | 车端离线时吊销本地缓存生效 | ASIL-A | Embedded |
| 防中继攻击 | UWB ToF 测距 | 中继模拟测试 | 信号延迟 ≥ 3μs → 拒绝 | ASIL-B(D) | Embedded+App |
| 防中继攻击 | Nonce 不重复 | 协议分析测试 | 同一 Nonce 被重放 → 拒绝 | ASIL-B | Embedded+App |
| 防中继攻击 | BLE+RSSI+UWB 交叉验证 | 注入测试 | RSSI 与 UWB 不一致 → 拒绝 | ASIL-B(D) | Embedded+App |
| 防中继攻击 | 纯 BLE 无 UWB 解锁 | 禁用 UWB 测试 | BLE 单独解锁请求 → 拒绝 | ASIL-B(D) | Embedded+App |
| 密钥安全 | SE050 密钥隔离 | 渗透测试 | 任何软件路径无法提取私钥明文 | ASIL-B | Embedded |
| 密钥安全 | 手机 Keychain/KeyStore 存储 | 安全审查 | 私钥仅在 SE/TEE 中运算 | ASIL-B | App |
| 密钥安全 | 生产环境 HSM 隔离 | ENV 切换测试 | `ENV=production` 时 Mock HSM 拒绝所有操作 | ASIL-B | Cloud |
| 密钥安全 | 安全启动链 | 签名校验测试 | 任意级签名失败 → 启动终止 | ASIL-B | Embedded |
| 通信安全 | TLS 1.3 强制 | 抓包分析 | 所有外网通信必须为 TLS 1.3 | ASIL-A | 全部 |
| 通信安全 | BLE LE SC | 协议分析 | 非 LE SC 模式连接被拒绝 | ASIL-B | Embedded+App |
| 通信安全 | JWT 有效期 | 时效测试 | Access Token > 1h → 401，Refresh > 7d → 401 | ASIL-A | Cloud |
| 通信安全 | 无 Token 请求 | 认证穿测 | 缺 Authorization header → 401 | ASIL-A | Cloud |
| OTA | 升级包签名验证 | 签名篡改测试 | 签名错误的升级包被拒绝安装 | ASIL-B | Embedded |
| 审计日志 | 关键操作日志完整性 | 操作→日志验证 | 所有关键操作（创建/吊销/解锁/分享/远程控车）均产生对应日志 | QM | Cloud |
| 审计日志 | 日志保留期限 | 配置检查 | 日志保留 ≥ 3 年 | QM | Cloud |
| 双协议 | ICCE 国密合规 | 算法合规测试 | SM2/SM3/SM4 实现符合 T/CA 110-2020 | QM | 全部 |
| 双协议 | CCC 国际算法合规 | 算法合规测试 | ECDSA/P-256/AES-256-GCM 实现符合 CCC DK 3.0 | QM | 全部 |
| 双协议 | 协议自动识别 | 场景测试 | 手机根据 VIN 自动选择协议，用户无感知 | QM | App |
| 离线模式 | NFC 无电解锁 | 场景测试 | 手机物理关机状态下 NFC 仍可刷卡解锁 | QM | Embedded+App |
| 离线模式 | 离线操作同步 | 网络恢复测试 | 离线期间操作记录在网恢复后 60s 内同步完成 | QM | App+Cloud |
| 用户认证 | 多种登录方式 | 集成测试 | 手机号+验证码、第三方 OAuth 均可用 | QM | App+Cloud |
| 用户认证 | 车辆所有权验证 | 场景测试 | 未验证 VIN+购车证明的用户无法创建主钥匙 | QM | Cloud |
| 性能 | 云端 API P99 | 压力测试 (k6) | P99 ≤ 200ms，吞吐 ≥ 100,000 TPS | QM | Cloud |
| 性能 | 服务可用性 | 监控测试 | ≥ 99.9% (年度) | QM | Cloud |
| 性能 | 并发 BLE 连接 | 极限测试 | 8 台设备同时连接正常 | QM | Embedded |

---

## 附录 A：变更控制规则

| 变更类型 | 审批流程 | 说明 |
|:---------|:---------|:-----|
| SHALL/SHALL NOT 新增或修改 | 全员同意（PM+架构+QA+开发代表） | 不可逆变更，需正式变更请求 |
| SHOULD/MAY 新增或修改 | 团队讨论 + 架构师确认 | 建议性变更 |
| ASIL 等级变更 | 安全工程师 + 架构师确认 | 涉及 safety-concept.md 同步更新 |
| 验收判定标准修改 | PM + QA 确认 | 不影响契约内容时 |

## 附录 B：规范引用

- T/CA 110-2020《智慧车联 数字钥匙系统技术规范》(ICCE)
- T/CA 109-2020《智慧车联 数字钥匙系统安全规范》
- CCC Digital Key 3.0 Specification
- ICCOA DK3.0 / DK4.0 Specification
- ISO 26262 (ASIL 等级定义)
- ISO 21434 (网络安全)
- IEEE 802.15.4z HRP UWB (安全测距)
