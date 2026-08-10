# KNI 生产 Bug 修复报告

**日期**: 2026-07-18
**状态**: ✅ 已完成

## 修复内容

### KNI-001 (P0 🔴) — Registry 大小写不敏感匹配
**位置**: `backend/cloud/hub/internal/adapter/registry.go`
**修复**:
- `Registry.Register()`: 内部 key 改为 `strings.ToLower(vendor) + ":" + strings.ToLower(protocol)`
- `Registry.Get()`: 查询 key 同样做 `strings.ToLower` 标准化
- `Registry.GetByVendor()`: 比较时双方都做 `strings.ToLower()`

### KNI-002 (P1 🟡) — strings.ToLower 定义了但没调用
**位置**: `backend/cloud/hub/internal/adapter/registry.go` 的 `GetByVendor`
**修复**: 将 `strings.ToLower(vendor)` 存入 `vendorLower` 变量并在比较时使用，同时对 `a.Vendor()` 也做 `strings.ToLower`

### KNI-003 (P1 🟡) — Registry 注册/查找不一致
**位置**: `backend/cloud/hub/internal/adapter/registry.go`
**修复**: Register 和 Get 统一做 lowercase 标准化，消除了大小写不匹配的根源

**未改**:
- `hub_transport.go` 无需改动 — Registry 内部处理 lowercase
- 现有测试中的大写注册（如 `Register("XIAOMI", "ICCOA_DK40", ...)`) 依然正常工作

## 验证结果

| 验证项 | 结果 |
|--------|------|
| `go test -count=1 -v ./backend/cloud/hub/internal/adapter/...` | ✅ PASS (32/32) |
| `go test -count=1 ./backend/cloud/hub/internal/service/...` | ✅ PASS |
| `go vet ./backend/cloud/hub/...` | ✅ 零错误 |

## 改动的文件

| 文件 | 改动 |
|------|------|
| `backend/cloud/hub/internal/adapter/registry.go` | 添加 `strings` 导入；Register/Get/GetByVendor 全部 lowercase 标准化 |
