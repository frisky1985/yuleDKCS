# GO-P0-01: Hub 日志系统死代码修复报告

## 原始问题详情

### 诊断发现

`internal/logger/` 包定义了一套完整的自定义日志系统：
- `Logger` 接口（Trace/Debug/Info/Warn/Error/Fatal）
- `DefaultLogger` 实现类
- 全局函数：`logger.Debug()`, `logger.Info()` 等
- 模块日志器（`ModuleLogger`）：`logger.Init()`, `logger.KeyMgr()`, `logger.Adapter()` 等

**核心发现：该包在所有代码中均未被导入使用。**

| 检查项 | 结果 |
|--------|------|
| `internal/logger` 被任何 `.go` 文件导入 | ❌ 零处 |
| 全局函数 `logger.Debug()/Info()` 被调用 | ❌ 零处 |
| `internal/logger` 被 main.go 初始化 | ❌ 完全死代码 |

### 实际日志链路

实际线上日志走的是 `go.uber.org/zap` 直接初始化：
- `cmd/hub/main.go` — `zap.NewProduction()` → 传入各 service/adapter 的 `*zap.Logger`
- `cmd/yuledkcs/main.go` — 同上
- 约 **141 处** `logger.Info/Debug/Warn/Error` 调用全部使用 `*zap.Logger`（zap 风格），非自定义 logger

### 根因

自定义 `internal/logger` 包与 `zap` 并行存在但无任何连接。`internal/logger` 是一个 "备胎代码"——当初设计作为统一日志接口但没有完成集成。

---

## 修复内容

### 1. `internal/logger/logger.go` — 新增 `ParseLevel` 辅助函数

```go
func ParseLevel(s string) (Level, error) { ... }
```
支持将 `"trace"/"debug"/"info"/"warn"/"error"/"fatal"` 字符串解析为 `Level` 枚举，方便从命令行参数初始化。

### 2. `cmd/hub/main.go` — 集成 `internal/logger` 初始化

**新增内容：**
- 导入 `flag` 包和 `hub_logger`（别名 `internal/logger`）
- 新增两个命令行参数：
  - `--log-level` — 日志级别（默认 `info`）
  - `--log-file` — 日志文件路径（默认空 = stderr）
- 在 `zap.NewProduction()` 之前调用 `hub_logger.Init(cfg)`

**原有 `zap.Logger` 初始化完全保留**，服务/适配器等 141 处 zap 调用不受影响。

### 3. `cmd/yuledkcs/main.go` — 同上

**新增内容：**
- 导入 `hub_logger`
- 新增 `--log-level` 和 `--log-file` 参数
- 在 `zap.NewProduction()` 之前初始化 `hub_logger.Init(cfg)`

**所有原有 flag (mode, http-addr, grpc-addr, jwt-secret, db-dsn) 和 zap 初始化保持不变。**

### 4. 向后兼容性

- 不删除任何现有代码
- zap 初始化和所有 zap 风格调用完全不变
- `--help` 输出包含新参数，不影响无参数默认运行
- 旧启动脚本无需修改

---

## 验证结果

### 编译检查 ✅
```bash
$ cd ~/yuleDKCS/backend/cloud/hub && go build ./...
# 静默通过，无错误
```

### --help 输出 ✅
```bash
$ cd cmd/hub && go run . --help
  -log-file string    日志输出文件 (默认 stderr)
  -log-level string   日志级别: trace/debug/info/warn/error/fatal (default "info")

$ cd cmd/yuledkcs && go run . --help
  -log-file string    日志输出文件 (默认 stderr)
  -log-level string   日志级别: trace/debug/info/warn/error/fatal (default "info")
  -mode string        启动模式: all-in-one | hub-only | server-only (default "all-in-one")
  -http-addr string   ...
  -grpc-addr string   ...
  -jwt-secret string  ...
  -db-dsn string      ...
```

### 测试全部通过 ✅
```bash
$ cd ~/yuleDKCS/backend/cloud/hub && go test -count=1 -timeout 60s ./...
ok  	github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1	0.346s
ok  	github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/adapter	0.883s
ok  	github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/codec/bertlv	0.562s
ok  	github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/gateway	1.647s
ok  	github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/token	1.992s
ok  	github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/unified	1.761s
ok  	github.com/frisky1985/yuleDKCS/backend/cloud/hub/tests/compliance/ccc	1.470s
ok  	github.com/frisky1985/yuleDKCS/backend/cloud/hub/tests/compliance/icce	2.240s
ok  	github.com/frisky1985/yuleDKCS/backend/cloud/hub/tests/compliance/iccoa	2.525s
```

---

## 其他端是否需要同样修复

| 端 | 状态 | 说明 |
|----|------|------|
| `backend/cloud/hub/` | ✅ 已修复 | hub + yuledkcs 两个入口点 |
| `backend/dkcs/` | ❓ 需检查 | 如果也有 `internal/logger` 引用但未初始化，需同样接入 |
| `backend/phone/` | ❓ 需检查 | 同上，但物理上分散在不同目录，应当单独评估 |

### 建议

1. **长期：** 如果团队决定统一使用 `internal/logger` 接口，需要逐步将各 service/adapter 的 `*zap.Logger` 替换为 `logger.Logger` 接口。这是架构重构，不建议在 P0 任务中完成。
2. **短期：** 当前修复已经使 `internal/logger` 全局函数可用。新代码可以开始使用 `logger.Info("TAG", "msg")` 风格，逐步迁移。
3. **DKCS 端：** 如果 `backend/dkcs/` 有自己的日志包或也引用了 hub 的 `internal/logger`，应单独立项修复。

---

## 修改文件清单

| 文件 | 操作 |
|------|------|
| `internal/logger/logger.go` | + 新增 `ParseLevel()` 函数，+ 新增 `strings` import |
| `cmd/hub/main.go` | + `flag`/`hub_logger` import，+ 参数解析，+ 初始化代码块 |
| `cmd/yuledkcs/main.go` | + `hub_logger` import，+ `--log-level`/`--log-file` 参数，+ 初始化代码块 |
