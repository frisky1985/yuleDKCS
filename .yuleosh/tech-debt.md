# yuleDKCS — 技术债务跟踪 (Tech Debt)

> **生成**: 2026-07-07 (Pipeline v1) | **最后更新**: 2026-07-08 (Phase 2 完成后批量闭合)
> **来源**: architecture-assessment.md + startup-analysis.md + 实际验证结果 + Phase 2 修复记录

---

## P0 — 阻塞级

| ID | 描述 | 模块 | 影响 | 目标周期 | 状态 |
|:---|:-----|:-----|:-----|:---------|:----:|
| ~~TD-01~~ | 嵌入式交叉编译工具链未验证 (gcc-arm-none-eabi) | Embedded | 阻断了所有嵌入式 CI | Week 1 | ✅ **已闭合** (2026-07-07, embedded-toolchain-report.md) |
| ~~TD-02~~ | ICCE 国密算法 (SM2/SM3/SM4) 默认未启用，库依赖未确认 | Embedded ICCE | 国标合规缺口 | Week 2 | ✅ **已闭合** (2026-07-07, icce-sm-crypto-report.md) |
| ~~TD-03~~ | 三端 CI/CD 割裂 — 仅 Go 有 GitHub Actions | All | 无法统一质量门禁 | Week 1-5 | ✅ **已闭合** (2026-07-07/08, android/ios/java/misra CI) |
| TD-04 | yuleASR BSW 10 个模块零集成 | Embedded | 量产级 BSW 缺失 | Week 8+ | 🔴 持续 (属 Phase 4 范畴) |

## P1 — 重要级

| ID | 描述 | 模块 | 影响 | 目标周期 | 状态 |
|:---|:-----|:-----|:-----|:---------|:----:|
| ~~TD-05~~ | 嵌入式 MISRA C:2023 门禁未配置 | Embedded | 安全合规缺口 | Week 3 | ✅ **已闭合** (2026-07-08, misra-ci.yml + report-misra-cppcheck.md) |
| ~~TD-05a~~ | MISRA CI 门禁非阻塞修复 (--error-exitcode) | Embedded CI | P0 — 门禁形同虚设 | Done (2026-07-08) | ✅ **已闭合** |
| TD-06 | DKCS repository 包零测试覆盖 | Go DKCS | DB 层不可测 | Week 2 | ⚠️ 部分修复 (9.4%, 远低于50%目标) — 需增加 sqlmock 依赖后补测 |
| TD-07 | Hub api/v1 包仅 2.1% 覆盖 | Go Hub | API 层未测试 | Week 2 | ✅ **已闭合** (2026-07-08, 66.2% ≥60% 达成) |
| ~~TD-08~~ | Hub service/cmd 包零测试覆盖 | Go Hub | 业务逻辑缺口 | Week 3 | ⚠️ 部分修复 (17.0%, 远低于80%目标) — 依赖 gRPC mock server |
| ~~TD-09~~ | Android Kotlin 无 lint/test/coverage 门禁 | Android | 质量不可控 | Week 4 | ✅ **已闭合** (2026-07-07, android-ci.yml) |
| ~~TD-10~~ | iOS Swift 无 lint/test/coverage 门禁 | iOS | 质量不可控 | Week 4 | ✅ **已闭合** (2026-07-07, ios-ci.yml) |
| ~~TD-11~~ | Java Adapter 无 lint/test/coverage 门禁 | Java | 质量不可控 | Week 5 | ✅ **已闭合** (2026-07-08, ci-java.yml) |
| TD-12 | ASIL-B 安全机制零落地 (代码层) | Embedded+All | 功能安全缺口 | Week 6+ | 🔴 持续 (安全概念已定义, 代码层实现待 Phase 3/4) |
| ~~TD-13~~ | 缺少需求→架构→代码→测试追溯矩阵 | All | ASPICE 合规障碍 | Week 8 | ✅ **已闭合** (2026-07-07, traceability-matrix.md) |

## P2 — 建议级

| ID | 描述 | 模块 | 目标周期 | 状态 |
|:---|:-----|:-----|:---------|:----:|
| ~~TD-14~~ | 缺少 Docker 集成测试环境 | Cloud | Week 2 | ✅ **已闭合** (2026-07-07, docker-compose.yml) |
| TD-15 | 缺少压力测试和性能基线 | All | Week 10 | 🔴 持续 (属 Phase 4) |
| TD-16 | 缺少端到端 (Embedded↔App↔Cloud) 集成测试 | All | Week 6 | 🔴 持续 (属 Phase 3, P3.1-P3.4) |
| TD-17 | 文档: DEV-TASKS.md 中的任务状态未更新 | Docs | Week 8 | ⚠️ 未处理 |
| TD-18 | 缺少 ADR (Architecture Decision Records) | Docs | Week 8 | ⚠️ 未处理 |
| TD-19 | Hub gateway 76.7% 覆盖率可提升至 85%+ | Go Hub | Week 3 | ⚠️ 未处理 (gateway 76.7%, 未达 85% 优化目标) |

## TD-20 | Go repository 包依赖集成测试环境（当前被 mock 覆盖）
- **状态**: open (持续)
- **来源**: actual-validate (Go test passes with mocks, real PG needed)
- **建议**: 搭建 docker-compose PG 实例 + go test -tags=integration

## 新增 (Phase 2 审查发现)

| ID | 描述 | 模块 | 严重度 | 建议 |
|:---|:-----|:-----|:-------|:-----|
| TD-21 | Android detekt 配置文件 `config/detekt.yml` 不存在 | Android | P1 | 需在 `frontend/android/config/detekt.yml` 创建 |
| TD-22 | iOS 缺独立 `.swiftlint.yml` (SDK 和 App) | iOS | P1 | 需在 `frontend/ios/` 和 `frontend/ios-app/` 各自创建 |
| TD-23 | Java pom.xml 缺 Checkstyle/JaCoCo plugin 配置 | Java | P1 | 需在 `backend/adapters/pom.xml` 配置（CI 已做 fallback） |
| TD-24 | 6 份文档的 [待确认] 标记未解决 | Docs | P1 | 2026-07-08 已修复（除 INTEGRATION_GUIDE.md PKI/KMS 外）|
| TD-25 | `INTEGRATION_GUIDE.md`/`RUNBOOK.md` 旧版与新文件并存 | Docs | P1 | 旧文件未标记 deprecated，可能被误用 |

## 备注
- ✅ = 已闭合 (当前 Phase 已解决)
- ⚠️ = 部分修复 (仍有残留)
- 🔴 = 持续 (未计划在当前 Phase 解决)
- TD-06/08: 覆盖率未达目标 (`td-19` 同理)，纳入下一覆盖补测周期
