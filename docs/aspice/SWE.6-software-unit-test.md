# SWE.6 — 软件单元测试文档

> **项目**: yuleDKCS 数字钥匙系统
> **版本**: 1.0.0 | **日期**: 2026-07-07 | **状态**: 初版
> **关联**: SWE.5 (详细设计), TEST-PLAN.md, production-readiness-audit.md
> **方法**: 当前仅 Go 后端有完整 CI 测试，其他四端无自动化流水线

---

## 1. 测试策略

### 1.1 分层测试架构

| 测试层 | 范围 | 工具/框架 | 覆盖率目标 | 当前状态 |
|:-------|:-----|:----------|:-----------|:---------|
| 单元测试 | 函数/方法级 | Go Test, CTest, Flutter Test, XCTest, JUnit | ≥ 75% | ⚠️ 仅 Go 后端通过 |
| 集成测试 | 模块间交互 | 协议模拟器 + 模拟硬件 | `[待确认]` | ❌ 无自动化 |
| 安全测试 | 加密/认证/防中继 | 故障注入 + 攻击模拟 | 100% 安全用例通过 | ⚠️ 计划已定义未执行 |
| 端到端测试 | 三端联合 | 嵌入式模拟器 + App 测试 + 云端测试 | 关键流程 100% | ❌ 环境未搭建 |

### 1.2 通过标准

| 门禁 | 条件 | 责任人 |
|:-----|:-----|:-------|
| Go build + test | `go build ./...` 全过 + `go test ./...` 全过 | CI |
| Go race | 零竞态条件 | CI |
| Go vet | 零 vet 错误 | CI |
| 嵌入式编译 | 交叉编译零错误 | CI `[待确认]` |
| MISRA 合规 | `[待确认: MISRA C:2023 门禁未配置]` | —— |
| 覆盖率门禁 | Go ≥ 75%, App ≥ 70%, Embedded ≥ 80% | CI |

---

## 2. 现有测试覆盖清单

### 2.1 Go 后端（`backend/cloud/`）

**CI 状态**: ✅ 全部通过（build + test + vet + race）

| 测试文件 | 模块 | 用例数 | 覆盖代码 | 状态 |
|:---------|:-----|:------|:---------|:-----|
| `pkg/crypto/*_test.go` | SM2/SM4/ECDSA/AES/JWT | `[待确认: 精确计数]` | `cloud/pkg/crypto/` | ✅ |
| `pkg/crypto/hsm_mock_test.go` | HSM 隔离验证 | 5 | MockHSM 生产环境拒绝 | ⚠️ 已修复 |

**已知缺口**:
| 模块 | 覆盖率 | 说明 |
|:-----|:-------|:-----|
| Hub API v1 | 2.1% | P1 缺陷 (GO-P1-06)，需要补充 |
| Repository 包 | 0% | DB 层不可测 (GO-P1-07) |
| service/ | `[待确认]` | 业务逻辑层，应 ≥ 75% 目标 |

### 2.2 嵌入式端（`embedded/`）

**CI 状态**: ❌ 无自动化 pipeline（所有 P0 已修复，但无持续测试）

| 测试文件 | 用例数 | 覆盖范围 | 状态 |
|:---------|:-------|:---------|:-----|
| `embedded/tests/test_anti_relay.c` | `[待确认: ≥7]` | 防中继决策逻辑 | ⚠️ 测试文件存在，未集成 CI |
| `embedded/tests/test_icce_sm.c` | `[待确认]` | ICCE 状态机 | ⚠️ 测试文件存在 |
| `embedded/tests/test_ccc_sm.c` | `[待确认]` | CCC 状态机 | ⚠️ 测试文件存在 |

**编译验证**:
| 目标 | 状态 |
|:-----|:-----|
| ICCE 交叉编译 (arm-none-eabi) | ✅ 通过 |
| CCC 交叉编译 | ✅ 通过 |
| ICCOA 交叉编译 | ✅ 通过 |
| SM2/SM3/SM4 编译 | ✅ 通过 |

**CTest 命令**:
```bash
cd embedded && cmake -B build -DENABLE_TESTS=ON && cmake --build build && ctest --test-dir build
```
`[待确认: 以上命令执行结果，当前环境可能缺少 ARM 交叉编译工具链]`

### 2.3 App 端

**CI 状态**: ❌ 无自动化 pipeline

| 端 | 框架 | 测试文件 | 状态 |
|:---|:-----|:---------|:-----|
| Android | Flutter Test | `frontend/android-app/test/` | ⚠️ 测试文件存在 |
| iOS | XCTest | `frontend/ios-tests/` | ⚠️ 测试文件存在 |

**已知缺口**:
- Android: 无 lint (detekt)、无覆盖率 (JaCoCo)、无自动化测试 pipeline
- iOS: 无 SwiftLint、无 XCTest coverage pipeline

### 2.4 Java 适配器

**CI 状态**: ❌ 无自动化 pipeline

| 测试 | 状态 |
|:-----|:-----|
| Checkstyle | ❌ |
| JUnit 测试 | ❌ |
| JaCoCo 覆盖率 | ❌ |

---

## 3. 缺口分析

### 3.1 按端的覆盖矩阵

```
端              单元测试  集成测试  安全测试  E2E  CI pipeline
─────────────────────────────────────────────────────────────
Go 后端          ⚠️ 部分    ❌      ⚠️ 计划   ❌   ✅ 已配置
嵌入式 C         ⚠️ 文件    ❌      ⚠️ 计划   ❌   ❌
Android          ⚠️ 文件    ❌      ❌      ❌   ❌
iOS              ⚠️ 文件    ❌      ❌      ❌   ❌
Java 适配器      ❌        ❌      ❌      ❌   ❌
```

### 3.2 关键缺口

| 缺口 ID | 描述 | 严重度 | 影响 | 建议修复时间 |
|:--------|:-----|:-------|:-----|:------------|
| UT-GAP-01 | 嵌入式 C 测试未集成 CI，无自动化回归 | 🔴 高 | 安全修复无法自动化回归验证 | Week 1-3 |
| UT-GAP-02 | Android/iOS 零 CI/CD (GO-P1-12) | 🔴 高 | App 质量问题无门禁 | Week 4-5 |
| UT-GAP-03 | Java 适配器无 CI (GO-P1-11) | 🟡 中 | Java 端质量问题 | Week 5 |
| UT-GAP-04 | Go API 覆盖率仅 2.1% (GO-P1-06) | 🟡 中 | API 回归风险 | Week 3-4 |
| UT-GAP-05 | Go Repository 包零测试 (GO-P1-07) | 🟡 中 | DB 层不可测 | Week 4-5 |
| UT-GAP-06 | MISRA C:2023 门禁未配置 | 🟡 中 | 代码质量无基线 | Week 3 |
| UT-GAP-07 | E2E 集成测试环境未搭建 | 🔴 高 | 跨端协议兼容性风险 | Week 3-5 |
| UT-GAP-08 | 无覆盖率聚合看板 | 🟢 低 | 无法全局监控 | Week 8+ |

### 3.3 建议修复优先级

| 优先级 | 行动项 | 端 |
|:-------|:-------|:---|
| P0 | 配置嵌入式 CI pipeline (模板已有，见体系结构评估 §5) | Embedded |
| P0 | 搭建 E2E 集成测试环境 | All |
| P1 | 配置 Android CI (detekt + JaCoCo) | Android |
| P1 | 配置 iOS CI (SwiftLint + XCTest) | iOS |
| P1 | 配置 Java CI (Checkstyle + JaCoCo) | Java |
| P1 | 补充 Go API 覆盖率 | Go |
| P2 | 配置覆盖率聚合看板 | All |
| P2 | 运行嵌入式 MISRA C:2023 扫描 | Embedded |

---

## 4. 安全关键测试清单

| 测试用例 | 关联缺陷修复 | 验证方法 | 优先级 |
|:---------|:------------|:---------|:-------|
| EU-AR-01~07 | EMB-P0-07, EMB-P1-04 | 防中继决策逻辑 | P0 |
| EU-SE-01~05 | EMB-P0-04 | SE050 驱动 | P0 |
| ES-RA-01~04 | EMB-P0-07 | 中继攻击模拟 | P1 |
| CU-HSM-01~05 | GO-P0-02 (降级) | HSM 隔离验证 | P0 |
| CS-JWT-01~04 | — | JWT 安全 | P1 |
| AS-SEC-01~04 | — | App 安全 | P1 |
