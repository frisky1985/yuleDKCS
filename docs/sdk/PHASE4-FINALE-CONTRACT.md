# Sprint Contract: Phase 4 收尾批 — 4.1 单测审计 / 4.2 契约 E2E 扩展 / 4.5 S2S E2E 验证

> 老板指令: "继续下一轮"。Phase 4 剩余三项, 并行推进。
> 依据: SDK-TASKS.md Phase 4（4.1/4.2/4.5）+ TASK_STATUS.md。
> 裁决日期: 2026-08-01

---

## 工作流 1: 4.1 SDK 单元测试覆盖审计 + 补齐（W1）

### 现状
- iOS Tests: 1994 行（ShareFlow 464/BleStub 386/CCC SC 253/Advertise 186/UWB 135/Background 135/RequestShape 125/NFC 100/...）
- Android tests: 2603 行（ShareFlow 442/BleProtocol 434/BleStub 317/RequestShape 225/CCC SC 204/SM4 149/...）
- 覆盖已较全，需**系统性审计**对照 SDK 功能面找缺口

### Scope
- 审计清单: HubClient 全方法（BindKey/UnbindKey/ListKeys/GetKey/CreateShare/AcceptShare/CancelShare/RemoteLock/RemoteUnlock/RemoteStart/Push 回调）、KeyManager（2.16 缓存/2.17 同步/2.18 离线/2.19 增量）、MailboxClient（2.20/2.21 secret 处理）
- 补缺口: 双端缺什么补什么（测试文件, CI 执行）
- 交付: `docs/sdk/TEST-COVERAGE-AUDIT.md`（审计表: 功能面 × 双端 × 覆盖状态）+ 新测试

### 完成标准（AC-41）
| ID | Criterion | Pass | Owner |
|:--:|:----------|:-----|:------|
| 41-1 | 审计表覆盖 SDK 全部公开 API 面 | 文档列出全方法 × 双端覆盖 | W1 |
| 41-2 | 明显缺口补齐（如 HubClient 远程控制/Push/KeyManager 状态同步） | 新测试就位, 断言真实 | W1 |
| 41-3 | 现有测试零破坏 | 不删不改现有断言 | W1 |

## 工作流 2: 4.2 SDK × Hub 契约 E2E 扩展 + 跑通（W2）

### 现状
- `scripts/sdk-hub-contract-e2e.sh` 已覆盖: login/bindKey/getKey/listKeys/mailbox（5 端点）
- 本机 docker postgres 可用（yuledkcs-postgres healthy）, hub 可本地 build 跑

### Scope
- 扩展脚本: UnbindKey、RemoteLock/RemoteUnlock（SendCommand）、CreateShare/AcceptShare/CancelShare 契约断言
- 实际跑通: 完整运行脚本, 新端点真实返回成功
- 注意: 遵循记忆契约要点 — 枚举名契约（protojson 只收枚举名）、camelCase、base64 pubkey

### 完成标准（AC-42）
| ID | Criterion | Pass | Owner |
|:--:|:----------|:-----|:------|
| 42-1 | 脚本扩展 3+ 新端点断言 | 脚本含 unbind/remote/share 流程 | W2 |
| 42-2 | 完整跑通 | 脚本 exit 0, 新端点实测成功 | W2 |
| 42-3 | 失败有明确诊断输出 | 断言失败打印响应体 | W2 |

## 工作流 3: 4.5 S2S E2E 执行验证（W3）

### 现状
- `backend/cloud/hub/tests/integration/scenarios/e2e_01..e2e_14` 已存在
- e2e_12 (ICCOA full share via S2S) / e2e_13 (ICCE full share) 目标场景
- TASK_STATUS P2: e2e_14 有预存 relay API 签名漂移 vet 错误

### Scope
- 跑 `go test ./tests/integration/...` 确认 e2e_12/13 通过
- 修复失败场景（含 e2e_14 预存 vet 错误, 若属 relay API 签名漂移则对齐修复）
- 交付: 运行报告（哪些场景通过/修复了什么）

### 完成标准（AC-45）
| ID | Criterion | Pass | Owner |
|:--:|:----------|:-----|:------|
| 45-1 | e2e_12/13 通过 | go test 输出 PASS | W3 |
| 45-2 | e2e_14 vet 错误修复 | go vet 无新错 | W3 |
| 45-3 | 全量 integration 不回归 | 相关包测试绿 | W3 |

---

## 文件边界

| Worker | 目录 |
|:-------|:-----|
| W1 (4.1) | `mobile/ios/Tests/` + `mobile/android/sdk/src/test/` + `docs/sdk/TEST-COVERAGE-AUDIT.md` |
| W2 (4.2) | `scripts/sdk-hub-contract-e2e.sh`（+ 临时 hub 二进制/日志） |
| W3 (4.5) | `backend/cloud/hub/tests/integration/` + 可能修 `internal/relay` 或测试 |

## Negotiation Log

| Round | Party | Action | Notes |
|:------|:------|:-------|:------|
| R1 | Generator | PROPOSE | 三项收尾: 审计补齐 / 契约扩展跑通 / E2E 执行验证; 边界零交叉 |
| R2 | architect-lead | APPROVE | W2 跑真 hub (docker PG 就绪); W3 修 e2e_14 预存错误属本批范围; W1 只增不删 |
| R3 | Evaluator | APPROVE | 每个 AC 客观可验证 (go test / 脚本 exit code / 审计表) |
| R4 | 老板 | 确认 | 继续下一轮 |
| R5 | Evaluator | APPROVE | 三线完成: W1 4.1 (审计表 + 远程控车/Push/KeyManager 测试双端, iOS typecheck + Android 33/33); W2 4.2 (脚本 11 段 21 断言全跑通 exit 0, 发现 sendCommand snake_case 契约事实 + hub stub 缺口); W3 4.5 (e2e_12/13 全 PASS, 修 e2e_14 编译错误 + e2e_11 隐性回归, integration 87/0) |
| R6 | 主导 | CLOSE | SDK-TASKS 4.1/4.2/4.5 标 ✅; TASK_STATUS e2e_14 ✅ + 新增 P1: iOS 模块预存编译错误 4 处 |
