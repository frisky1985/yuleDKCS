# yuleDKCS P3.5 审查 P0 修复综合报告

> **日期**: 2026-07-08 | **执行人**: Claude (Subagent)
> **来源**: 小马全量审查 (评分 73/100) — 4 个 P0

---

## 总览

| # | 问题 | 文件 | 状态 |
|:-:|:----|:-----|:----:|
| P0-01 | cover-check.yml 未接入 CI | `~/.yuleDKCS/cover-check.yml` → `~/.github/workflows/cover-check.yml` | ✅ 已修复 |
| P0-02 | OTA 测试完全缺失 | `e2e_06_ota_update_test.go` + `ota_service_test.go` | ✅ 已修复 |
| P0-03 | 安全启动链 KSS-SHALL-08 无独立测试 | `secure_boot_test.go` | ✅ 已修复 |
| P0-04 | SwiftLint 禁用 force_cast/force_try | `.swiftlint.yml` | ✅ 已修复 |

---

## 修复详情

### P0-01: cover-check.yml 接入 CI

**操作:**
1. 文件 `~/.yuleDKCS/cover-check.yml` → `~/yuleDKCS/.github/workflows/cover-check.yml`
2. cover-check.yml 支持 `workflow_call` + `pull_request` 两种触发器
3. ci.yml 中新增 `coverage-gate` job，在 PR 时调用 cover-check.yml

**文件:**
- `~/.github/workflows/cover-check.yml` (新建，源自 `~/.yuleDKCS/cover-check.yml`)
- `~/.github/workflows/ci.yml` (修改)

### P0-02: OTA 测试

**新建文件:**
- `backend/cloud/hub/tests/integration/scenarios/e2e_06_ota_update_test.go` — 6 个 E2E 子测试
- `backend/dkcs/internal/service/ota_service_test.go` — 4 个单元测试

**Mock 扩展:**
- `suite/OTAPackage` 结构体
- `MockTCUAgent.VerifyOTAPackage()`, `StartOTADownload()`, `InstallOTAPackage()`, `GetOTAPackageStatus()`

**需求覆盖:**

| 需求 | E2E | 单元 | 场景 |
|:----|:---:|:----:|:-----|
| OT-SHALL-01 (OTA升级支持) | E2E-06-01 | TestOTAShall01 | 下载→校验→安装 |
| OT-SHALL-02 (签名验证) | E2E-06-03/05 | TestOTAShall02 | ECDSA P-256 签名校验 |
| OT-SHALL-03 (状态追踪) | E2E-06-04 | TestOTAShall03 | DOWNLOAD_PENDING→...→COMPLETED/FAILED |
| OT-SHALL-NOT-01 (拒绝) | E2E-06-02/06 | TestOTAShallNot01 | 篡改/无签名/错误密钥 |

### P0-03: 安全启动链测试

**新建文件:**
- `backend/dkcs/internal/service/secure_boot_test.go` — 6 个测试函数

**模型:** Boot ROM(OEM Root) → BootLoader(SE050验签) → TFM → Application 逐级校验

**测试覆盖:**
- 正常完整链验证 ✓
- BootLoader 签名篡改 ✓
- TFM 签名篡改 ✓
- Application 无签名 ✓
- 表驱动完整性测试（4 种场景） ✓
- SE050 验签边界条件（nil image/nil key/short signature） ✓

### P0-04: SwiftLint force_cast/force_try

**操作:**
- 从 `.swiftlint.yml` 的 `disabled_rules` 中移除 `force_cast` 和 `force_try`
- 添加注释说明 ASIL-B(D) 安全关键代码规则

**效果:** 所有 `as!` / `try!` 将被 SwiftLint 标记为 error

---

## 验证结果

| 检查项 | 结果 |
|:------|:----:|
| `go build ./...` (dkcs) | ✅ PASS |
| `go build ./...` (cloud/hub) | ✅ PASS |
| `go build ./...` (cloud/hub/tests/integration) | ✅ PASS |
| `go build ./...` (hub) | ✅ PASS |
| `go test ./...` (dkcs) | ✅ PASS |
| `go test ./...` (cloud/hub) | ✅ PASS |
| `go test -run TestE2E06` (integration) | ✅ PASS |
| `go vet ./...` (all packages) | ✅ PASS |
| 新增测试文件数量 | 4 |
| 新增测试函数数量 | 14 |
