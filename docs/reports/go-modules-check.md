# Go 多模块依赖一致性检查

**日期**: 2026-07-16  
**命令**: `go mod verify` (所有模块)

---

## 模块清单 (共 6 个 go.mod)

| # | go.mod 路径 | Go 版本 | Verify 结果 |
|---|---|---|---|
| 1 | `backend/dkcs/go.mod` | 1.25.0 | ✅ 通过 |
| 2 | `backend/dkcs/proto/go.mod` | 1.22 | ✅ 通过 |
| 3 | `backend/hub/go.mod` | 1.25.0 | ✅ 通过 |
| 4 | `backend/cloud/hub/go.mod` | 1.25.0 | ✅ 通过 |
| 5 | `backend/cloud/hub/tests/integration/go.mod` | 1.25.0 | ✅ 通过 |
| 6 | `backend/cloud/hub/tests/stress/go.mod` | 1.25.0 | ✅ 通过 |

**结果**: 所有模块 `go mod verify` 均通过，依赖树完整性良好。

---

## 依赖版本一致性分析

### 1. ⚠️ Go 版本不一致

| 模块 | go directive |
|---|---|
| `backend/dkcs/proto/go.mod` | **1.22** |
| 其余 5 个模块 | 1.25.0 |

- **风险**: `proto/go.mod` 使用 `go 1.22`，而宿主模块 `dkcs` 使用 `go 1.25.0`。低版本 proto 模块可能缺少 Go 1.25 的语言特性，但不会被使用到。
- **建议**: 将 `proto/go.mod` 中的 `go 1.22` 升级为 `go 1.25.0` 以保持统一。

### 2. ❌ 共享依赖版本分歧 (grpc / protobuf)

| 依赖 | dkcs | hub | cloud/hub | dkcs/proto |
|---|---|---|---|---|
| `google.golang.org/grpc` | v1.82.0 | v1.82.0 | v1.82.0 | ⚠️ **v1.63.2** |
| `google.golang.org/protobuf` | v1.36.11 | v1.36.11 | v1.36.11 | ⚠️ **v1.33.0** |

- **风险**: `dkcs/proto` 依赖的 `grpc v1.63.2` 和 `protobuf v1.33.0` 远落后于主模块的 `v1.82.0` / `v1.36.11`。虽然通过 `replace` 指令桥接，但生成的 proto stub 可能使用了较旧的 API，存在潜在的运行时兼容问题。
- **建议**: 重新生成 proto 代码，使 `proto/go.mod` 的 grpc/protobuf 版本与主模块保持一致。

### 3. ❌ `replace` 指令中的模块名不匹配

```go
// dkcs/go.mod 中的 replace 指令
replace github.com/digitalkey/proto/dkcs => ./proto
```

- **问题**: `replace` 左侧的模块路径为 `github.com/digitalkey/proto/dkcs`（引用了一个不存在的 `digitalkey`），而实际模块名声明为 `github.com/frisky1985/yuleDKCS/backend/dkcs/proto/dkcs`。
- **风险**: 该 replace 指令虽然在当前仓库中能工作（因为在 go.work 或本地路径），但模块名与路径不一致，如果未来移除 replace 或发布为独立模块会导致引用失败。
- **建议**: 将 replace 左侧统一修正为 `github.com/frisky1985/yuleDKCS/backend/dkcs/proto/dkcs`。

### 4. ℹ️ hub vs cloud/hub — 依赖关系说明

| 模块 | 说明 |
|---|---|
| `backend/hub` | 原始 hub 模块（含 gin 和 grpc 依赖） |
| `backend/cloud/hub` | 云版本 hub 模块（几乎相同的依赖树） |
| `backend/cloud/hub/tests/integration` | 集成测试（仅依赖 testify） |
| `backend/cloud/hub/tests/stress` | 压力测试（含 replace hub 指向 `../../`） |

- hub 和 cloud/hub 的依赖树高度相似（都有 gin v1.12.0、grpc v1.82.0、protobuf v1.36.11、zap v1.28.0 等），建议评估是否可以合并或通过 go.work 共享。

### 5. ✅ 一致通过的共享依赖

所有主模块（dkcs、hub、cloud/hub）对以下依赖版本一致：
- `google.golang.org/grpc` — v1.82.0 ✅
- `google.golang.org/protobuf` — v1.36.11 ✅
- `go.uber.org/zap` — v1.28.0 ✅
- `google.golang.org/genproto/googleapis/rpc` — v0.0.0-20260706201446-f0a921348800 ✅
- `golang.org/x/net` — v0.56.0 ✅
- `golang.org/x/sys` — v0.46.0 ✅
- `golang.org/x/text` — v0.39.0 ✅
- `golang.org/x/crypto` — v0.53.0 ✅

---

## 总结

| 状态 | 项 | 严重度 |
|---|---|---|
| ✅ | go mod verify 全部通过 | — |
| ⚠️ | proto/go.mod Go 版本 (1.22) 落后于主模块 (1.25) | 低 |
| ❌ | proto/go.mod grpc v1.63.2 vs 主模块 v1.82.0 | **中** |
| ❌ | proto/go.mod protobuf v1.33.0 vs 主模块 v1.36.11 | **中** |
| ❌ | dkcs/go.mod replace 指令模块名不匹配 (digitalkey) | **中** |
| ℹ️ | hub 与 cloud/hub 依赖高度重复 | 低 (架构决策) |
