# Sprint Contract: Loop Batch C — 量产就绪收尾（SDK 集成示例 / 认证文档 / 杂项）

> 老板指令: "直接 loop"。来源: Phase 3 准备 + TASK_STATUS 剩余。
> 裁决日期: 2026-08-01

---

## 工作流 1: SDK 集成示例 + Phase 3 文档（W1）

### 任务
1. `docs/sdk/SDK-INTEGRATION-GUIDE.md`: 车厂 App 集成 yuleDKCS SDK 完整指南
   （iOS SPM/CocoaPods 接入 + Android Gradle 接入 + 初始化/BLE/远程控车/分享/离线
   授权最小代码示例 + 权限声明汇总）
2. 示例工程骨架: `examples/` 下 iOS Swift Package demo + Android module demo
   （最小可编译骨架, 展示 API 调用面）
3. 集成 checklist（车厂侧配置项: Info.plist/Manifest/entitlements/证书）

### 完成标准（AC-INT）
| ID | Criterion | Pass | Owner |
|:--:|:----------|:-----|:------|
| INT-1 | 集成指南覆盖全 API 面 | 文档含示例代码 | W1 |
| INT-2 | 示例骨架可编译/语法检查 | iOS parse + Android 静态 | W1 |
| INT-3 | checklist 完整 | 配置项清单 | W1 |

## 工作流 2: 认证文档补齐（W2）

### 任务
1. 审计 docs/certification/ 现有文档（ccc/iccoa/icce/relay/pixit/pics）
2. 补齐缺口: SDK 侧认证 checklist（每协议 BLE/NFC/UWB 需要的测试项 + PICS 引用）、
   厂商适配矩阵（CCC/ICCOA/ICCE × 功能面）
3. 更新 docs/compliance/ 如有缺口

### 完成标准（AC-CERT）
| ID | Criterion | Pass | Owner |
|:--:|:----------|:-----|:------|
| CERT-1 | SDK 认证 checklist 文档 | 就位 | W2 |
| CERT-2 | 适配矩阵完整 | 全功能面 × 三协议 | W2 |

## 工作流 3: 杂项收尾（W3）

### 任务
1. TEST-COVERAGE-AUDIT.md 第 74 行过时更新（iOS ListKeys/GetKey/unbind/cancel 已补）
2. kustomize 全量渲染 smoke: kubectl kustomize 所有 overlay（如有）
3. helm 静态校验（模板平衡 + values 覆盖）
4. TASK_STATUS 最终整理（剩余项状态确认）

### 完成标准（AC-MISC）
| ID | Criterion | Pass | Owner |
|:--:|:----------|:-----|:------|
| MISC-1 | 审计文档更新 | 无过时标注 | W3 |
| MISC-2 | 部署渲染全绿 | 命令 exit 0 | W3 |
| MISC-3 | TASK_STATUS 整洁 | 状态准确 | W3 |

---

## Negotiation Log

| Round | Party | Action | Notes |
|:------|:------|:-------|:------|
| R1 | Generator | PROPOSE | 三线独立; 全部无真机依赖 |
| R2 | architect-lead | APPROVE | 示例骨架最小化, 不复制 SDK 逻辑 |
| R3 | Evaluator | APPROVE | 可验证 |
| R4 | 老板 | 确认 | 直接 loop |
