# yuleHUB 集成测试报告

> 生成时间: 2026-07-08 01:19 CST
> 测试范围: yuleHUB gRPC 服务端到端启动验证

---

## 1. 测试基础设施

### 1.1 本地启动验证脚本
**文件**: `scripts/verify-hub.sh`

构建并启动 yuleHUB，验证：
- [x] 编译通过（`go build`）
- [x] HTTP `/health` 端点返回 `{"status":"ok"}`
- [x] HTTP `/healthz` 端点返回详细健康状态
- [x] JWT Login 流程（获取 token）
- [x] 认证保护端点正确拒绝未认证请求
- [x] gRPC reflection（若 grpcurl 已安装）

### 1.2 Go 集成测试
**文件**: `backend/hub/tests/integration/hub_integration_test.go`

构建标签 `integration`，不干扰单元测试。

| 测试函数 | 描述 | 独立端口 |
|---|---|---|
| `TestHealthEndpoint` | HTTP /health + /healthz 正确响应 | 8081/9091 |
| `TestGrpcConnectivity` | gRPC HealthCheck RPC 返回适配器列表 | 8082/9092 |
| `TestHubStartStop` | 编译→启动→健康检查→停止 | 8083/9093 |
| `TestLoginEndpoint` | 有效凭据→JWT token；无效凭据→401 | 8084/9094 |
| `TestAuthProtectedEndpoint` | 无认证→401；无效token→401；有效token→503（无gRPC后端） | 8085/9095 |

### 1.3 Docker Compose 测试
**文件**: `tests/test-docker-compose.yml`

- yuleHUB（从项目 Dockerfile 构建）
- PostgreSQL 16（tmpfs，数据不持久化）
- 端口映射: REST 8080, gRPC 9090, PG 15432

---

## 2. 测试执行命令

### 本地验证脚本（无 Go 编译环境要求）
```bash
bash scripts/verify-hub.sh
```

### Go 集成测试（编译 + 自动启动 hub）
```bash
cd backend/hub
go test -tags=integration -count=1 -timeout 120s ./tests/integration/...
```

### Docker Compose 集成测试（在 Mac 端运行）
```bash
# 启动
cd tests
docker compose -f test-docker-compose.yml up -d

# 等待就绪
sleep 10
curl http://localhost:8080/healthz

# 清理
docker compose -f test-docker-compose.yml down -v
```

---

## 3. 环境变量

| 变量 | 默认值 | 说明 |
|---|---|---|
| `JWT_SECRET` | (必填) | JWT 签名密钥，启动 hub 前必须设置 |
| `HUB_REST` | `http://localhost:8080` | 集成测试中指定 hub REST 地址 |
| `HUB_GRPC` | `localhost:9090` | 集成测试中指定 hub gRPC 地址 |
| `GRPC_PORT` | `9090` | verify-hub.sh 中 gRPC 端口 |
| `REST_PORT` | `8080` | verify-hub.sh 中 REST 端口 |

---

## 4. 已知限制

1. **gRPC 后端缺失**: REST API 内调用 gRPC 服务需 `grpcConn`，集成测试中通过 503 确认认证成功。
2. **端口冲突**: 多测试并行运行时需指定不同端口。
3. **grpcurl 可选**: gRPC reflection 验证需要本地安装 grpcurl。
4. **Dockerfile 兼容**: 测试用 Docker Compose 使用项目根目录 Dockerfile，确保 hub 二进制位于 `/usr/local/bin/hub-server`。

---

## 5. 验证文件清单

```
scripts/verify-hub.sh                    — 本地启动验证 shell 脚本
backend/hub/tests/integration/           — Go 集成测试目录
  hub_integration_test.go                — 集成测试文件（build tag: integration）
tests/test-docker-compose.yml            — Docker compose 集成测试
.yuleosh/integration-test-report.md      — 本报告
```

---

## 6. 测试通过判断

- [ ] `scripts/verify-hub.sh` 退出码 0
- [ ] `go test -tags=integration -count=1 ./tests/integration/...` 全部 PASS
- [ ] `docker compose -f test-docker-compose.yml up -d` 后 `/healthz` 返回 200
