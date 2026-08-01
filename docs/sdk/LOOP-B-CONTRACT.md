# Sprint Contract: Loop Batch B — 离线授权回退 / postgres-exporter / 移动端补尾

> 老板指令: "直接 loop" — 自主连续推进。
> 来源: TASK_STATUS.md P2 待办 + 4.1 审计遗留。
> 裁决日期: 2026-08-01

---

## 工作流 1: 离线授权回退机制（W1, P2）

### 现状
- TASK_STATUS P2: "离线授权回退机制 📋" — 语义未定义, 需先调研 yuleDKCS 语境
- 可能范围: (a) 移动端 KeyManager 离线解锁授权策略 (2.18 已有离线缓存+状态推断, 可能指其授权安全策略) (b) Hub 离线授权 (c) 其他

### 任务
1. 调研: 搜代码/设计文档 (mobile KeyManager, backend) 确定"离线授权回退"在 yuleDKCS 的真实含义
2. 给出方案 (A/B/C 对比, 按老板偏好) → 实现最小可行方案
3. 单测覆盖

### 完成标准（AC-OFF）
| ID | Criterion | Pass | Owner |
|:--:|:----------|:-----|:------|
| OFF-1 | 调研结论明确 (是什么/为什么/范围) | 文档记录决策 | W1 |
| OFF-2 | 方案落地 (按调研结论) | 代码 + 单测 | W1 |
| OFF-3 | 不破坏现有测试 | go test / 双端验证 | W1 |

## 工作流 2: postgres-exporter 部署（W2, P2）

### 现状
- TASK_STATUS P2: "Prometheus 采集已指向 postgres-exporter:9187, 需部署 exporter（可用 postgres_exporter sidecar 或独立 Deployment）"

### 任务
1. 研读现有 kustomize 部署 (deploy/k8s/ + base/overlay) 的 Prometheus 配置
2. 实现: postgres-exporter 独立 Deployment (或 sidecar) + Service :9187 + 对齐 helm
3. 文档: 部署说明

### 完成标准（AC-EXP）
| ID | Criterion | Pass | Owner |
|:--:|:----------|:-----|:------|
| EXP-1 | kustomize overlay 含 exporter Deployment + Service | YAML 就位 + 静态校验 | W2 |
| EXP-2 | helm 同步 (若 helm 仍维护) | 无 mysql 残留 | W2 |
| EXP-3 | 文档说明 scrape 目标 | 文档就位 | W2 |

## 工作流 3: 移动端补尾（W3）

### 现状
- W1 (Batch A) 遗留: YDKBLEManagerTests 最终 typecheck 未跑 (已给命令)
- 4.1 审计记录待补: iOS 侧 ListKeys/GetKey/unbind/cancel wire 断言 (需先给 YDKHubClient 加 transport 注入缝)
- TASK_STATUS: iOS 编译错误项需确认标 ✅

### 任务
1. 补跑 YDKBLEManagerTests typecheck (命令已在 LOOP-A Contract R6)
2. 若 YDKHubClient 有 transport 注入缝: 补 iOS ListKeys/GetKey/unbindKey/cancelShare wire 断言测试
3. TASK_STATUS: iOS 编译错误 → ✅ (已确认 swift build 全绿)

### 完成标准（AC-TAIL）
| ID | Criterion | Pass | Owner |
|:--:|:----------|:-----|:------|
| TAIL-1 | YDKBLEManagerTests typecheck 通过 | 命令 exit 0 | W3 |
| TAIL-2 | iOS wire 断言补齐 (若缝存在) 或记录阻塞 | 测试就位/说明 | W3 |
| TAIL-3 | TASK_STATUS 更新 | 文档更新 | W3 |

---

## 文件边界

| Worker | 目录 |
|:-------|:-----|
| W1 | 调研全仓 + 实现 (范围依结论) |
| W2 | `backend/cloud/deploy/k8s/` + `backend/cloud/deploy/helm/` |
| W3 | `mobile/ios/Tests/` + `mobile/ios/Sources/YDKHubClient/` + `TASK_STATUS.md` |

## Negotiation Log

| Round | Party | Action | Notes |
|:------|:------|:-------|:------|
| R1 | Generator | PROPOSE | 三线并行; W1 先调研后实现 (A/B/C); W2 K8s; W3 补尾 |
| R2 | architect-lead | APPROVE | 边界零交叉; W1 若结论为移动端则只改 mobile |
| R3 | Evaluator | APPROVE | 各 AC 可验证 |
| R4 | 老板 | 确认 | 直接 loop |
| R5 | Evaluator | APPROVE | W1 离线授权: 调研确定语义 (移动端 KeyManager 离线裁决, PRD 模块五) + 方案 A 双端 OfflineAuthorizer (fail-closed + 7 天宽限), iOS 13/Android 17 用例实跑 OK (修复嵌套插值语法错误后 swift build 全绿); W2 exporter: kustomize + helm 同步 + 文档, kubectl kustomize 33 文档验证; W3 补尾: YDKBLEManagerTests typecheck exit 0 (规避 Swift 6.3.3 编译器 bug), iOS wire 断言补齐 (MockURLProtocol + transport 注入缝, 5 形状), TASK_STATUS iOS 编译错误 ✅ |
| R6 | 主导 | CLOSE | TASK_STATUS 离线授权/exporter ✅; 插件 SDK/性能测试标注疑 yuleASR 错放; TEST-COVERAGE-AUDIT 第 74 行过时待下轮更新 |
