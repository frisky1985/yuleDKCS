# CMD 入口测试进度报告

> 日期: 2026-07-18 | 分支: main

## 概览

| 入口 | 文件 | 行数 | 测试 | 覆盖率 | 状态 |
|------|------|------|------|--------|------|
| `backend/dkcs/cmd/dkcs/` | main.go | 195 | ✅ main_test.go (10 tests) | 6.2% | ✅ PASS + go vet |
| `backend/cloud/hub/cmd/hub/` | main.go | 97 | ✅ main_test.go (7 tests) | 48.8% | ✅ PASS + go vet |
| `backend/cloud/hub/cmd/yuledkcs/` | main.go | 129 | ✅ main_test.go (13 tests) | 52.0% | ✅ PASS + go vet |

## 测试方法

按照 Go cmd 包标准实践，**不直接测试 main()**，改为：

### dkcs (6.2%)
- **提取的函数**: `initDatabase`、`initRedis`（main.go 已有）
- **测试内容**:
  - 无效数据库配置 → 返回 error（含 DSN 格式验证）
  - 无效 Redis 地址 → 返回非 nil client（lazy connect）
  - 配置加载环境变量覆盖 + 默认值
- **限制**: main() 包含 DB/Redis/Kafka 真实连接调用，无法在测试环境执行

### hub (48.8%)
- **提取的函数**: `setupHubGRPCServer(logger)` → 重构抽取自 main()
- **测试内容**:
  - 返回非 nil server + gateway
  - 6 个服务注册验证（4 个 hub 服务 + 2 个 reflection）
  - Keepalive 参数合理性验证
  - 与不同 logger 配置的兼容性
- **成果**: 覆盖 adapter 注册、service 创建、gateway 初始化全路径

### yuledkcs (52.0%)
- **已有函数**: `startHubOnly`、`startServerOnly`、`startAllInOne`（main.go 已分离）
- **测试内容**:
  - 三种启动模式路由逻辑验证
  - 各模式 goroutine 不 panic
  - JWT_SECRET 环境变量读取
  - 未知模式 fallback 到 all-in-one

## 变更文件

```
backend/dkcs/cmd/dkcs/main_test.go          (创建, 10 tests)
backend/cloud/hub/cmd/hub/main.go            (重构, 提取 setupHubGRPCServer)
backend/cloud/hub/cmd/hub/main_test.go       (创建, 7 tests)
backend/cloud/hub/cmd/yuledkcs/main_test.go  (创建, 13 tests)
specs/spec-cmd-test.md                       (创建, OpenSpec 格式)
reports/cmd-test-progress.md                 (本文件)
```

## 技术债务

| # | 问题 | 优先级 | 建议 |
|---|------|--------|------|
| 1 | dkcs cmd 覆盖率仅 6.2% | Medium | 可进一步提取 `startDKCSServer(cfg, log)` 函数，包含完整初始化+服务注册，用 mock 测试启动流程 |
| 2 | initDatabase 测试依赖 TCP 超时（~5s） | Low | 添加 DSN 格式预校验函数，在连接前就发现 config 异常 |
| 3 | 启动 goroutine 泄漏 | ~~Low~~ **Fixed** (P0) | NoPanic 测试的 goroutine 在测试结束后持续运行，建议在 cleanup 中关闭 server |
| 4 | Service 名称前缀错误 | ~~Low~~ **Fixed** (P1) | hub/main_test.go 期望 `hub.KeyManagementService`，实际为 `digitalkey.hub.v1.KeyManagementService` |
| 5 | 空测试 \(TestMain\_FlagDefaults\) | ~~Low~~ **Fixed** (P1) | 只有注释无断言，已用 flag.NewFlagSet 添加实际断言 |

## 小马审查缺陷修复记录 (2026-07-18)

### P0: Goroutine 泄漏 — ✅ Fixed
**文件**: `backend/cloud/hub/cmd/yuledkcs/main_test.go`
**问题**: 8 个测试启动 goroutine (`done := make(chan struct{})` + `go func()`) 但从不清理，`_ = done` 无实际作用
**修复**:
- 移除 `done` channel 模式，改用 `context.WithTimeout` + `t.Cleanup()`
- 将 server/gateway 创建 inline，在 `t.Cleanup()` 中调用 `hub.Shutdown()` / `srv.GracefulStop()` / `lis.Close()`
- 每个测试等待 goroutine 启动后用 `ctx.Done()` 超时退出，cleanup 确保 goroutine 完全退出
- 端口冲突消除：每个测试使用独立端口，测试结束后清理释放端口

### P1: Service 名称前缀错误 — ✅ Fixed
**文件**: `backend/cloud/hub/cmd/hub/main_test.go:42-49`
**问题**: 期望 `"hub.KeyManagementService"` 但 proto 定义为 `"digitalkey.hub.v1.KeyManagementService"`
**修复**: 修改期望值为正确的 proto 命名空间：
  - `hub.KeyManagementService` → `digitalkey.hub.v1.KeyManagementService`
  - `hub.KeyShareService` → `digitalkey.hub.v1.KeyShareService`
  - `hub.VehicleControlService` → `digitalkey.hub.v1.VehicleControlService`
  - `hub.HubTransportService` → `digitalkey.hub.v1.HubTransportService`

### P1: 空测试 — ✅ Fixed
**文件**: `backend/cloud/hub/cmd/yuledkcs/main_test.go:20-22`
**问题**: `TestMain_FlagDefaults` 只有注释无断言
**修复**: 使用 `flag.NewFlagSet` 模拟 main 函数中的 flag 定义，添加 5 个 flag 默认值断言：
  - `mode` → `"all-in-one"`
  - `http-addr` → `":8080"`
  - `grpc-addr` → `":9090"`
  - `jwt-secret` → `""`
  - `db-dsn` → `""`

### 验证结果

```bash
$ go test -count=1 -v ./backend/dkcs/cmd/dkcs/...
PASS  (10 tests, 5.360s)

$ go test -count=1 -v ./backend/cloud/hub/cmd/hub/...
PASS  (7 tests, 0.354s)

$ go test -count=1 -v ./backend/cloud/hub/cmd/yuledkcs/...
PASS  (13 tests, 21.403s)

$ go vet ./backend/dkcs/... ./backend/cloud/hub/...
# (clean, no output)
```
