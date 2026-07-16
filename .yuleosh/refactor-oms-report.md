# 搭建报告：yuleHUB OMS (生命周期管理系统) MVP

## 概述

完成 yuleDKCS 钥匙全生命周期管理系统（OMS）MVP 的搭建，对标银基 OMS 的售前→售中→售后覆盖能力。

## 文件清单

```
├── backend/hub/oms/
│   ├── doc.go            — ASPICE 注释 / 架构说明 / 数据流图
│   ├── types.go          — 核心类型定义 (7 种状态 / 6 种记录 / 5 种筛选器)
│   ├── lifecycle.go      — 生命周期管理器接口 + 状态转换安全校验
│   ├── provisioning.go   — 预置管理器接口 (create / get / cancel / list)
│   ├── deployment.go     — 部署管理器接口 (create / rollback / progress / list)
│   ├── monitoring.go     — 监控管理器接口 (record / stats / history / aggregate)
│   └── store.go          — 持久化存储接口 (5 个子存储契约)
```

## 核心能力

### 1. 生命周期状态机 (lifecycle.go)

| 当前状态 | 允许转换至 |
|---------|-----------|
| created | pre_paired |
| pre_paired | paired, created (回退) |
| paired | active, revoked |
| active | suspended, revoked |
| suspended | active (恢复), revoked |
| revoked | (终态) |
| deleted | (终态) |

- `EnsureTransition()` — 核心安全检查，包含乐观锁（期望状态匹配）
- `IsValidNextState() / IsTerminal()` — 校验辅助函数

### 2. 预置管理 (provisioning.go)

- `ProvisioningManager` 接口：任务创建、状态查询、取消、列表
- 5 种预置状态：pending → in_progress → completed/failed/expired

### 3. 部署管理 (deployment.go)

- `DeploymentManager` 接口：创建、回滚、状态查询、历史列表、灰度推进
- 6 种部署状态：planning → in_progress → completed/rolled_back/failed/cancelled
- 灰度比例字段 `RolloutPct` (0-100)

### 4. 监控管理 (monitoring.go)

- `MonitoringManager` 接口：使用记录写入、单钥统计、使用历史、全局聚合
- `AggregatedStats` 全局统计：活跃/暂停/吊销数量、按 OEM 聚合

### 5. 持久化层 (store.go)

- 5 个存储接口：`KeyStore` / `ProvisioningStore` / `DeploymentStore` / `UsageStore`
- 接口契约明确，支持 PostgreSQL/MongoDB/InMemory 等多种实现

## 验证结果

```bash
$ cd ~/yuleDKCS/backend/hub && go build ./...
# 编译通过，零错误
```

## 架构决策

| 决策 | 说明 |
|------|------|
| 接口优先 | 所有 Manager 都是 interface，实现可以后续按需替换 |
| context-aware | 所有 public 方法接收 `context.Context` 参数，支持追踪和超时 |
| 显式转换 | `TransitionState(ctx, keyID, from, to)` 要求传入期望的当前状态，防止并发冲突 |
| 与 dkcs/security 解耦 | OMS 不直接依赖 device/security 模块，通过接口契约交互 |
| 终态不可逆 | revoked / deleted 不允许任何状态转换 |

## 下一步

- [ ] 实现 InMemory 存储层（用于集成测试）
- [ ] 实现 PostgreSQL 存储层（生产环境）
- [ ] 添加 middleware 层（API route handler）
- [ ] 集成 lifecycle hook（状态变更时通知 security 模块）
- [ ] 添加单元测试覆盖状态机
