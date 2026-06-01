# DK Hub / DK Server 包重组迁移计划

## 目标

当前代码一套 binary 同时承担编排层和密钥材料层职责。通过包重组实现"逻辑分离 + 部署灵活"。

## 目录结构变化

```
当前:                          迁移后:
backend/                      backend/
├── cloud/hub/     编排+路由    ├── dk-hub/      编排层 (原 cloud/hub/)
├── dkcs/          密钥材料     ├── dk-server/    密钥材料层 (原 dkcs/)
├── adapters/      TSP适配     └── adapters/     TSP适配 (不变)
└── db/                       └── db/

cmd/                          cmd/
                               └── yuledkcs/      统一入口 (新增)
```

## 迁移步骤

### Step 1: 统一入口 (已完成)
- `cmd/yuledkcs/main.go` — 支持 `--mode=all-in-one | hub-only | server-only`
- `--mode=all-in-one` = 当前默认行为，零变化

### Step 2: 包重命名 (待执行)
```
backend/cloud/hub/  →  backend/dk-hub/
backend/dkcs/       →  backend/dk-server/
```

### Step 3: 内部接口抽象 (待执行)
- Hub → Server 的调用改为 `DKServer` 接口，同进程直接调用，跨进程走 gRPC
- `backend/dk-server/` 实现该接口

```go
// backend/dk-hub/internal/service/dk_server.go (新增)
type DKServer interface {
    BindKey(ctx, req) → 创建密钥对，写入SE050
    RevokeKey(ctx, req) → 吊销车端密钥
    SuspendKey(ctx, req) → 挂起车端密钥
    // ... 不暴露私钥，只编排
}
```

### Step 4: 模式切换 (待执行)
- `all-in-one`: Hub → 内部直接调 DKServer 实现
- `hub-only`: Hub → gRPC client → 车厂 DK Server
- `server-only`: gRPC server → 本地 DKServer 实现

## 安全模型

```
部署模式          | 密钥材料位置          | 适用场景
──────────────────┼───────────────────────┼──────────────────
all-in-one       | 车厂云 (同一个进程)    | DK Server 也由我们开发
hub-only +       | 车厂云 (独立进程)      | 车厂已有 DK Server
server-only      |                        |
```

## 不做的事

- ❌ 不拆微服务（不引入 gRPC 网络开销）
- ❌ 不改数据库 schema
- ❌ 不改前端/嵌入式代码
- ❌ 不改 Java 适配器
