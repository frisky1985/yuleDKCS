# Sprint Contract: 并行批 — 2b-G UWB / 2b-H NFC / 后端 P1（PG 迁移 + Helm 同步 + JWKS 防放大）

> 老板指令: "并行开发"。三条独立工作流同时推进。
> 约束: 2b-G/H 真机依赖 → 完成标准 = 真实平台 API 代码就位 + 接口 + 模拟测, 真机联调单列。
> 裁决日期: 2026-08-01

---

## 工作流 1: 2b-G UWB 测距真实化（W1）

### Scope
- iOS: `YDKUWBManager.swift` — 新增真实实现 `YDKNIUWBManager`（NearbyInteraction NISession/NINearbyPeerConfiguration/NIDiscoveryToken 完整代码, 编译级）, 保留 `YDKMockUWBManager`
- Android: `UwbManager.kt` — 新增 `AndroidUwbManager`（API 34+ 原生 android.uwb UwbManager/UwbAdapter; 或 FiRa 兼容抽象 + 版本降级 Mock）, 保留 `MockUwbManager`
- 模拟测就位（无真机: 接口/状态机/参数校验可单测）
- 文档: UWB 平台差异 + 真机联调清单（token 交换流程、App 前后台限制）

### 完成标准（AC-U）
| ID | Criterion | Pass | Owner |
|:--:|:----------|:-----|:------|
| U-1 | iOS 真实实现编译级就位（NISession 配置 + token 交换 + 回调） | swiftc -parse/typecheck | W1 |
| U-2 | Android 真实实现编译级就位（原生 API 或 FiRa 抽象 + 降级） | 静态审查 + 测试 | W1 |
| U-3 | 接口不变, Mock 保留 | 现有调用零破坏 | W1 |
| U-4 | 模拟测覆盖接口契约（不依赖硬件） | 测试就位 | W1 |

## 工作流 2: 2b-H NFC 备用解锁真实化（W2）

### Scope
- iOS: `YDKNFCManager.swift` — 新增 `YDKCoreNFCManager`（NFCTagReaderSession/NFCMiFareTag 读取车辆标签 + 指令写入, 编译级）, 保留接口
- Android: `NfcManager.kt` — 新增 `AndroidNfcManager`（NfcAdapter + Tag 读取/ISO-DEP 指令, 编译级）
- 权限/配置文档（iOS: NFCReaderUsageDescription + entitlement; Android: NFCTechList 过滤 + 前台调度）
- 模拟测就位

### 完成标准（AC-N）
| ID | Criterion | Pass | Owner |
|:--:|:----------|:-----|:------|
| N-1 | iOS 真实实现编译级就位（CoreNFC session + tag 解析） | swiftc -parse/typecheck | W2 |
| N-2 | Android 真实实现编译级就位（NfcAdapter + tag dispatch） | 静态审查 + 测试 | W2 |
| N-3 | 接口不变 | 现有调用零破坏 | W2 |
| N-4 | 配置文档（权限/entitlement/tech-list） | 文档就位 | W2 |

## 工作流 3: 后端 P1 批量（W3）— TASK_STATUS.md P1

### Scope
1. **dkcs PG schema 迁移**: `db/schema.sql` 是 MySQL 方言 → 转 PG 方言（`db/schema.pg.sql` 或迁移文件）+ 纳入迁移机制（确认 dkcs 服务启动时自动执行或脚本化）
2. **Helm Chart 同步 postgres**: `helm/dkcs` 仍引用 mysql-statefulset + mysql-password → 改为 postgres StatefulSet + secret（对齐 kustomize 现行部署）; 或标记废弃指向 kustomize
3. **JWKS kid 未命中防放大**: 恶意令牌随机 kid 触发重复拉取 → kid miss 负缓存或 30s 冷却后再刷新

### 完成标准（AC-P）
| ID | Criterion | Pass | Owner |
|:--:|:----------|:-----|:------|
| P-1 | PG schema 方言正确 + 迁移可执行 | 脚本/迁移运行成功（本地 PG 或 SQL 校验） | W3 |
| P-2 | Helm chart 与 kustomize 一致或明确废弃 | helm template 通过 / 文档说明 | W3 |
| P-3 | JWKS kid miss 负缓存 + 单测 | go test 通过 | W3 |
| P-4 | 不破坏现有: 1365 测试全绿 | go test ./... 通过 | W3 |

---

## 文件边界

| Worker | 目录 |
|:-------|:-----|
| W1 (UWB) | `mobile/ios/Sources/YDKBLEManager/YDKUWBManager.swift` + `mobile/android/.../ble/UwbManager.kt` + 各自测试 |
| W2 (NFC) | `mobile/ios/Sources/YDKBLEManager/YDKNFCManager.swift` + `mobile/android/.../ble/NfcManager.kt` + 各自测试 |
| W3 (P1) | `db/` + `helm/` + `internal/auth/`（Go 后端, 全在项目根） |

## Negotiation Log

| Round | Party | Action | Notes |
|:------|:------|:-------|:------|
| R1 | Generator | PROPOSE | 三线并行, 文件边界零交叉; UWB/NFC 真机依赖按"代码就位+模拟测, 联调单列"验收 |
| R2 | architect-lead | APPROVE | UWB iOS 用 NearbyInteraction (U1/U2), Android 用 API 34+ 原生 (FiRa); NFC iOS 用 CoreNFC, Android 用 NfcAdapter; 均保留 Mock |
| R3 | Evaluator | APPROVE | 编译级 + 接口契约 + 模拟测可客观验证; P1 有 go test 可跑 |
| R4 | 老板 | 确认 | 并行开发 |
| R5 | Evaluator | APPROVE | 三线完成: W1 UWB (iOS NI typecheck 零错误 + Android 版本策略 8 用例); W2 NFC (iOS 12/12 运行时断言 + Android 33/33); W3 P1 (PG 迁移真实 docker 幂等验证 + helm 0 mysql 残留 + JWKS 24 测试含 -race; 全量回归 hub 1378 + dkcs 373 绿) |
| R6 | 主导 | CLOSE | TASK_STATUS P1 三项 + P2 JWKS 标 ✅; SDK-TASKS 2b-G/H 代码就位, 真机联调单列 |
