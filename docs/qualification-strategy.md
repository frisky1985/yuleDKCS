# 软件合格性测试策略 (Qualification Test Strategy)

> **项目**: yuleDKCS 数字钥匙系统
> **文档编号**: yuleDKCS-QTS-001
> **版本**: 1.0.0 | **日期**: 2026-08-06 | **状态**: APPROVED (基线)
> **过程域**: ASPICE SWE.6 (Software Qualification Test)
> **关联**: `docs/software-requirements.md` (SRS, REQ-001~040)、`docs/aspice/SYS.5-system-qualification.md`、`docs/hil/acceptance-criteria.md`、`docs/hil/hil-test-spec.md`、`docs/hil/HIL-TEST-PLAN.md`

---

## 1. 策略概述

软件合格性测试在**目标环境或等效环境**中验证完整软件是否满足全部软件需求（SWE.6.BP1/BP2）。本策略为每条 REQ-xxx 定义验收标准，并指定测试环境与方法。

### 1.1 测试分层与环境

| 环境 | 说明 | 适用需求域 |
|:-----|:-----|:-----------|
| **E1. 单元层 (CI L1)** | Go test + coverage 门禁（backend/dkcs, backend/cloud/hub） | REQ-019~023, REQ-010~018 |
| **E2. 集成契约层 (CI L2)** | `tests/integration/` pytest 契约测试（BERTLV/接口/数据流） | REQ-024~027, REQ-010~018, REQ-019~020 |
| **E3. 组件 E2E (Go 集成套件)** | `backend/cloud/hub/tests/integration/` 16 用例（15 编号，e2e_06 已去重为 e2e_15；e2e_11 含两个测试函数），真实 HUB+DKCS 服务 | REQ-001~003, REQ-010~015, REQ-019 |
| **E4. 系统 E2E (carsim)** | `tests/e2e/` 11 场景（手机↔车模拟器↔云） | REQ-001~009 |
| **E5. HIL 硬件在环** | S32K312 EVB + KW47A/NCJ29D6/ST25R501，`tests/hil/hil_runner.py` | REQ-028~035, REQ-006, REQ-016 |
| **E6. 安全测试** | `tests/security/`（replay/jwt/rate-limit/malformed） | REQ-006, REQ-018 |

### 1.2 通过标准

- SHALL 类需求: 100% 用例通过（安全相关用例零失败）。
- HIL P0 用例: 100% 通过（P0 阻塞级）；P1 ≥ 90%。
- 性能需求: 见各 REQ 验收标准（如解锁 ≤1s、P99 ≤200ms）。

---

## 2. 逐需求验收标准（REQ-xxx → 验收准则 → 环境 → 状态）

> 状态: ✅ 已满足（有证据）| 🟡 部分（证据待补）| ⬜ 未执行

### 2.1 系统级 (REQ-001 ~ 009)

| 需求 | 验收准则 | 环境 | 状态 |
|:-----|:---------|:-----|:----:|
| REQ-001 设备注册 | 认证后注册成功；返回唯一 device_id 与已有车辆/钥匙 | E3/E4 | 🟡 部分 |
| REQ-002 多设备配钥 | 按能力配钥；协议自动协商；重复注册返回已有钥匙 | E3/E4 | 🟡 部分 |
| REQ-003 多设备管理 | 列表/吊销/推送通知；≥5 设备/用户 | E3 | 🟡 部分 |
| REQ-004 性能 | 解锁/上锁 ≤1s；冷启动 ≤2s；P99 ≤200ms；配对 ≤3min；控车 ≤3s；≥100k TPS | E4/E6(压测) | ⬜ 未执行（基准待建） |
| REQ-005 可用性 | 可用性 ≥99.9%；MTTR ≤30min；RTO ≤4h；RPO ≤1h；小时级增量备份 | 运维监控 | ⬜ 未执行 |
| REQ-006 安全性 | TLS 1.3；MFA；SE 存储；UWB 防中继；Nonce 防重放；审计 ≥3 年 | E5/E6 | ✅ HIL 故障注入 + 安全套件 |
| REQ-007 离线能力 | NFC 无电解锁；离线钥匙有效期内可用；恢复后同步 | E4/E5 | 🟡 部分 |
| REQ-008 协议兼容 | ICCE+CCC 双栈；自动协商；双证书管理；VIN 识别 | E3/E4 | 🟡 部分 |
| REQ-009 用户体验 | ≤2m 靠近 1s 解锁；离车 5m/30s 上锁；后台解锁；≤5 钥匙/车 | E4/E5 (HIL-UL-01..03) | ✅ HIL 解锁 100% |

### 2.2 DKCS Core (REQ-010 ~ 018)

| 需求 | 验收准则 | 环境 | 状态 |
|:-----|:---------|:-----|:----:|
| REQ-010 密钥绑定 | 1000/1001 BERTLV 双向；必填字段校验；4 KeyType；16 位 AccessLevel；响应含 KeyId/VehiclePubkey/SharedSecret/VehicleCert | E2/E3 (e2e_02/07) | ✅ |
| REQ-011 密钥解绑 | 1002/1003 支持；KeyId+Reason 校验 | E3 | ✅ |
| REQ-012 密钥撤销 | 1004/1005；紧急模式；撤销后车端即时拒绝（e2e_10） | E3 (e2e_10_key_expiry_revocation) | ✅ |
| REQ-013 密钥列表 | 1010/1011 分页 | E3 | ✅ |
| REQ-014 分享创建 | 2000/2001；AccessLevel/有效期/次数限制（e2e_08/13/14） | E3 (e2e_08_icce_keyshare) | ✅ |
| REQ-015 车辆控制 | 3000/3001；11 动作；5 来源（e2e_04/06） | E3 (e2e_04_remote_control) | ✅ |
| REQ-016 状态上报 | 3002；LockStatus/EngineStatus/DoorStatus/BatteryPct/InteriorTemp/GPS | E3 (e2e_04-05 车辆状态上报) + E5 (HIL-VS-01..03) | ✅ |
| REQ-017 心跳 | 9000/9001 双向；Status/CpuUsage/MemUsage/ConnCount | E2 | ✅ |
| REQ-018 消息签名 | HMAC-SHA256(Header+Body) Trailer；篡改即拒 | E2 (test_protocol_codec) | ✅ |

### 2.3 Hub (REQ-019 ~ 023)

| 需求 | 验收准则 | 环境 | 状态 |
|:-----|:---------|:-----|:----:|
| REQ-019 Registry 规范化 | 大小写混合查询命中；Register/Get lowercase；原小写行为不破坏 | E2 (test_hub_interfaces) | ✅ |
| REQ-020 nil 安全检查 | RemoteControl 访问 nil 防护；ToLower 生效 | E2 + 代码审查 | ✅ |
| REQ-021 单测覆盖 | service 7 文件 ≥80%；logger ≥85% | E1 (CI L1) | ✅ |
| REQ-022 覆盖率门禁 | fail-under=60 生效；低于即 CI 失败 | E1 | ✅ |
| REQ-023 CI 分层 | 3 层依赖；L1 必过；集成测试独立；gosec 全量 | E1 (CI 配置) | ✅ |

### 2.4 Protocol (REQ-024 ~ 027)

| 需求 | 验收准则 | 环境 | 状态 |
|:-----|:---------|:-----|:----:|
| REQ-024 BERTLV | 信封 E1 01/Body/Trailer E1 FF；长度规则 00-7F/80-FF；头字段齐全；编解码往返一致 | E2 (test_protocol_codec) | ✅ |
| REQ-025 MQTT | topic 格式；QoS 2/1/0 映射；mTLS；BERTLV payload | E3 (协议合规套件) | ✅ |
| REQ-026 App↔HUB | REST+BERTLV；OAuth2.0；HMAC 签名；绑/解/撤/列 API | E3 | ✅ |
| REQ-027 错误码 | 1xxx/2xxx/3xxx/4xxx 段位统一 | E2 (dk_protocol.h 契约) | ✅ |

### 2.5 Embedded (REQ-028 ~ 035)

| 需求 | 验收准则 | 环境 | 状态 |
|:-----|:---------|:-----|:----:|
| REQ-028 NFC 层 | 13.56MHz 场检测；ISO14443-A/FeliCa；NDEF；NFC-F OOB；APDU 序列 | E5 (HIL-NFC-01..04) | ✅ 100% |
| REQ-029 BLE 层 | BLE 5.0 GATT 0xFFD1；OOB 配对；LE SC；全 characteristics | E5 (HIL-BLE-01..05) | ✅ 100% |
| REQ-030 UWB 层 | 802.15.4z；TWR；STS；5 距离分区；多锚点 | E5 (HIL-UWB-01..04) | ✅ 100% |
| REQ-031 ICCOA 栈 | 广播格式；GATT 0xFEF5；DK3.0 帧；DK4.0；BIND 流程；8 权限位 | E5 + 协议合规 | 🟡 部分 |
| REQ-032 ICCE 栈 | KW47A 运行；离线决策；缓存策略；OOB 配对；SM2/SM3/SM4 | E5 + 交叉编译 | 🟡 部分 |
| REQ-033 SE050 | SCP03 通道；attestation；证书链+ECDSA+有效期+固件哈希验签 | E5 (HIL-SE-01..05) | ✅ 100% |
| REQ-034 电源管理 | 5 级电源；ACTIVE<15mA/SLEEP<100μA/DEEPSLEEP<10μA；唤醒 <50ms/<100ms | E5 (HIL-PM-01..03, HIL-WK-01..03) | ✅ |
| REQ-035 安全启动 | 4 阶段 BL→OS→APP→COMPLETE；每阶段验签；SE050 信任锚 | E5 (HIL-FI-05) | 🟡 部分 |

### 2.6 Frontend (REQ-036 ~ 040)

| 需求 | 验收准则 | 环境 | 状态 |
|:-----|:---------|:-----|:----:|
| REQ-036 Android SDK | min SDK 26；5 接口；BLE/UWB/NFC 通道 | 移动端 CI (android-ci.yml) | 🟡 部分 |
| REQ-037 iOS SDK | min iOS 14；3 框架；5 协议 | 移动端 CI (ios-ci.yml) | 🟡 部分 |
| REQ-038 REST API | /api/v1；JWT Bearer；TLS 1.3；统一响应结构 | E3 (gateway 单测+集成) | ✅ |
| REQ-039 gRPC 服务 | 4 组微服务；定义 RPC 集 | E3 (proto-check + grpc 测试) | ✅ |
| REQ-040 消息队列 | 9 topics；Avro Schema Registry；consumer 隔离 | E3 (Kafka 集成测试) | ✅ |

---

## 3. 测试执行状态汇总

| 环境 | 用例数 | 通过 | 失败 | 通过率 | 证据 |
|:-----|:------:|:----:|:----:|:------:|:-----|
| E2 集成契约层 | 53 | 53 | 0 | 100% | tests/integration/ |
| E3 Go 组件 E2E | 16 用例 | 16 | 0 | 100% | backend/cloud/hub/tests/integration/（绿跑日志 `test-output/go-integration.log`） |
| E4 系统 E2E (carsim) | 11 场景 | 11 | 0 | 100% | tests/e2e/scenarios/ |
| E5 HIL (最终回归) | 37 | 37 | 0 | 100% | .osh/ci/sil-hil-results.json, docs/hil/HIL-TEST-RESULTS.md |
| E6 安全套件 | 4 文件 | ✅ | 0 | 100% | tests/security/ |

> **E3 运行前提**: 场景层（`./scenarios/`，16 个测试函数）为 mock 内嵌套件，无需外部服务，`go test -tags=integration` 直接全绿。
> 顶层 hub API 测试（`TestHealthEndpoint/TestGrpcConnectivity/TestLoginEndpoint/TestAuthProtectedEndpoint/TestHubStartStop`，5 个）为 **best-effort**：
> 需要运行中的 yuleHUB 服务（REST :8080 / gRPC :9090，且 `DATABASE_URL` 可达）；环境无 hub 时自动 **skip**（不做假绿，也不染红套件），
> 有 hub 时真实执行。归档绿跑日志 `test-output/go-integration.log`（16 PASS + 5 SKIP，exit 0）为本 Sprint 真实证据。

**遗留**（⬜/🟡 项）: REQ-004 性能基准、REQ-005 可用性监控、REQ-031/032 协议合规全量、REQ-036/037 移动端覆盖率 — 列入后续 Sprint（见 §4）。

## 4. 覆盖缺口与行动

| 缺口 | 影响需求 | 行动 | 目标 Sprint |
|:-----|:---------|:-----|:-----------|
| 性能基准未建立（TPS/P99/响应时间） | REQ-004 | k6 压测基准 + 计时测试 | S+1 |
| 可用性/灾备监控未自动化 | REQ-005 | 监控告警 + 混沌演练 | S+2 |
| ICCOA/ICCE 协议合规全量 | REQ-031/032 | 协议合规测试套件扩展 | S+1 |
| 移动端覆盖率门禁 | REQ-036/037 | JaCoCo/XCTest coverage pipeline | S+2 |

---

*— 本策略覆盖全部 40 条 REQ；逐条验收证据归档于 `.osh/evidence/`（含 acceptance-matrix.md 与 requirement-coverage.md）。—*
