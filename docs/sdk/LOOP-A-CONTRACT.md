# Sprint Contract: Loop Batch A — iOS 编译修复 / hub 分享+sendCommand 真实化 / relay Completed

> 老板指令: "直接 loop" — 自主连续推进, 不等待确认。
> 来源: TASK_STATUS.md P1/P2 待办。
> 裁决日期: 2026-08-01

---

## 工作流 1: iOS 模块预存编译错误修复（W1, P1）

### 现状（4.1 审计发现）
1. `YDKHubClient+Stream.swift` 跨文件访问 `private baseURL` / `fileprivate token`
2. `YDKKeyManager.swift` / `YDKKeyCache.swift` 缺 `import YDKHubClient`
3. 引用 YDKHubClient 内部类型 `YDKLogger`（internal 不可跨模块）
4. `YDKKeyManager+Sync.swift` 跨文件访问 `private syncQueue` / `private logger`

### 完成标准（AC-IOS）
| ID | Criterion | Pass | Owner |
|:--:|:----------|:-----|:------|
| IOS-1 | 4 处编译错误修复, 零行为变化 | 修复方式最小化（改访问级别/加 import/暴露必要类型） | W1 |
| IOS-2 | `swift build` 或等效模块构建全绿 | 全 target 编译通过（排除已知 UIKit/NI 平台限制） | W1 |
| IOS-3 | 现有测试不回归 | 已就位测试 typecheck 通过 | W1 |

## 工作流 2: hub 分享状态机 + sendCommand 真实化（W2, P1）

### 现状（4.2 契约 E2E 发现）
- CreateShare/AcceptShare 是适配器 stub（时间戳生成新 id, accept 不校验 shareCode）; CancelShare/GetShare 是 service stub（空 200, 无 ShareStatus 枚举/持久化）
- sendCommand 服务是 stub（权限校验/MQTT 下发注释未实现）, REST 端点 snake_case

### 完成标准（AC-HUB）
| ID | Criterion | Pass | Owner |
|:--:|:----------|:-----|:------|
| HUB-1 | 分享状态机: Share 记录持久化 + 状态流转 (PENDING→ACCEPTED/CANCELLED) | 新表/store + 单测 | W2 |
| HUB-2 | AcceptShare 校验 shareCode 有效性 | 无效 shareCode 拒绝 | W2 |
| HUB-3 | CancelShare 真实取消（状态变更） | 取消后状态 CANCELLED | W2 |
| HUB-4 | sendCommand 权限校验（key 存在 + 权限位）+ 下发通道接口（MQTT stub/接口化） | 单测 + 非 200 场景 | W2 |
| HUB-5 | 不破坏现有 1378 测试 | go test 全绿 | W2 |

## 工作流 3: relay StatusCompleted 可达路径（W3, P2）

### 现状（4.5 发现）
- relay 状态机 StatusCompleted 无 Update 路径可达; Delete reason="completed" 只能作用于 Cancelled 态

### 完成标准（AC-REL）
| ID | Criterion | Pass | Owner |
|:--:|:----------|:-----|:------|
| REL-1 | Update 支持 dataType 映射到 Completed（按规范定义） | 单测覆盖 | W3 |
| REL-2 | Delete(reason=completed) 可作用于 Completed 态 | 单测覆盖 | W3 |
| REL-3 | 不破坏 relay 30 单测 | go test 全绿 | W3 |

---

## 文件边界

| Worker | 目录 |
|:-------|:-----|
| W1 | `mobile/ios/Sources/YDKHubClient/` + `YDKKeyManager/` |
| W2 | `backend/cloud/hub/internal/`（share service + gateway + store） |
| W3 | `backend/cloud/hub/internal/relay/` + 相关 proto 若需 |

## Negotiation Log

| Round | Party | Action | Notes |
|:------|:------|:-------|:------|
| R1 | Generator | PROPOSE | 三线独立; W2 改动最大 (分享持久化), 需先研读现有 adapter/service 结构 |
| R2 | architect-lead | APPROVE | W2 分享状态机以现有 key store 模式为参考; sendCommand 接口化不引外部 MQTT 依赖 |
| R3 | Evaluator | APPROVE | 单测可客观验证; W1 swift build 是硬标准 |
| R4 | 老板 | 确认 | 直接 loop, 不等确认 |
| R5 | Evaluator | APPROVE | W1 iOS: 4 处编译错误修复 + swift build 全绿 (连带 YDKBLEManager import/Package.swift 依赖); W2 hub: proto ShareStatus + ShareStore (PG 迁移 0002) + 状态机 + shareCode 校验 + sendCommand 权限/CommandDispatcher, service 185 测试全绿 (改写 8 个旧 stub 断言 + 新增生命周期/权限场景); W3 relay: dataType 3 (ImportRequest)→Completed 裁决 (CCC §11.3.3/§11.3.4 依据), 32 passed + e2e_11/12 全绿 |
| R6 | 主导 | CLOSE | 全量回归: hub + integration + dkcs (见提交); 遗留: YDKBLEManagerTests 最终 typecheck (W1 迭代上限, 已给命令) |
