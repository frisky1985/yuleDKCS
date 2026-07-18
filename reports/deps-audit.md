# 依赖审计报告

**日期**: 2026-07-18
**扫描工具**: `govulncheck` (Go vuln management)
**扫描模块**: `backend/dkcs`, `backend/cloud/hub`

---

## 1. 模块概览

### backend/dkcs (Digital Key Control Service)

| 直接依赖 | 版本 | 说明 |
|----------|------|------|
| `github.com/golang-jwt/jwt/v5` | v5.3.1 | JWT 签发与验证 |
| `github.com/jmoiron/sqlx` | v1.4.0 | SQL 数据库操作 |
| `github.com/lib/pq` | v1.10.9 | PostgreSQL 驱动 |
| `github.com/redis/go-redis/v9` | v9.5.1 | Redis 缓存客户端 |
| `google.golang.org/grpc` | v1.63.2 | gRPC 服务框架 |

### backend/cloud/hub (Cloud Hub Service)

| 直接依赖 | 版本 | 说明 |
|----------|------|------|
| `github.com/gin-gonic/gin` | v1.10.0 | HTTP REST 框架 |
| `go.uber.org/zap` | v1.27.0 | 结构化日志 |
| `google.golang.org/grpc` | v1.81.1 | gRPC 服务框架 |
| `google.golang.org/protobuf` | v1.36.11 | Protobuf 编解码 |
| `github.com/golang-jwt/jwt/v5` | v5.3.1 | JWT 令牌处理 |

---

## 2. 已知漏洞 (CVEs)

### backend/dkcs — 4 个漏洞（影响代码）

| # | 漏洞 ID | 影响模块 | 当前版本 | 修复版本 | 严重程度 | 说明 |
|---|---------|----------|---------|---------|---------|------|
| 1 | GO-2026-5856 | `crypto/tls` (stdlib) | go1.26.3 | go1.26.5 | **高危** | TLS 握手拒绝服务 |
| 2 | GO-2026-5039 | `net/textproto` (stdlib) | go1.26.3 | go1.26.4 | **中危** | 未转义输入导致信息泄露 |
| 3 | GO-2026-5037 | `crypto/x509` (stdlib) | go1.26.3 | go1.26.4 | **低危** | 证书主机名校验效率低 |
| 4 | GO-2025-3540 | `github.com/redis/go-redis/v9` | v9.5.1 | v9.6.3 | **中危** | CLIENT SETINFO 超时可能乱序 |

**影响路径示例**:
- Vuln #1: `key_service.go:402` → `rand.Read` → `tls.Conn.HandshakeContext`
- Vuln #4: `redis.go:221` → `ZRemRangeByScore` → `client.initConn`

### backend/cloud/hub — 3 个漏洞（影响代码）

| # | 漏洞 ID | 影响模块 | 当前版本 | 修复版本 | 严重程度 | 说明 |
|---|---------|----------|---------|---------|---------|------|
| 1 | GO-2026-5856 | `crypto/tls` (stdlib) | go1.26.3 | go1.26.5 | **高危** | TLS 握手拒绝服务 |
| 2 | GO-2026-5039 | `net/textproto` (stdlib) | go1.26.3 | go1.26.4 | **中危** | 未转义输入导致信息泄露 |
| 3 | GO-2026-5037 | `crypto/x509` (stdlib) | go1.26.3 | go1.26.4 | **低危** | 证书主机名校验效率低 |

**影响路径示例**:
- Vuln #1: `rest_gateway.go:280` → `http.Server.ListenAndServe` → `tls.Conn.HandshakeContext`
- Vuln #3: `rest_gateway.go:280` → `http.Server.ListenAndServe` → `x509.Certificate.Verify`

---

## 3. 扫描发现的其他问题

以下依赖存在已知漏洞，但当前代码**未直接调用**受影响路径（低风险，建议升级但不紧急）：

### backend/dkcs
- `golang.org/x/net` v0.55.0 — 已知漏洞，未在调用栈中发现
- `google.golang.org/protobuf` v1.33.0 — 已知漏洞，未在调用栈中发现

### backend/cloud/hub
- `golang.org/x/net` v0.51.0 — 已知漏洞，未在调用栈中发现

---

## 4. 依赖版本更新建议

| 依赖 | 当前版本 | 建议版本 | 优先级 | 理由 |
|------|---------|---------|--------|------|
| Go 运行版本 | 1.26.3 | 1.26.5 | **P0** | 修复 TLS 拒绝服务高危 CVE |
| `github.com/redis/go-redis/v9` | v9.5.1 | v9.6.3+ | **P1** | 修复连接初始化乱序问题 |
| `golang.org/x/net` (dkcs) | v0.55.0 | v0.60.0+ | **P2** | 间接依赖，定期升级 |
| `golang.org/x/net` (hub) | v0.51.0 | v0.60.0+ | **P2** | 间接依赖，定期升级 |
| `google.golang.org/grpc` (dkcs) | v1.63.2 | v1.81.x | **P3** | 功能未降级可不升级 |
| `golang.org/x/crypto` (dkcs) | v0.52.0 | v0.60.0+ | **P2** | 间接依赖 |

---

## 5. 操作建议

1. **立即**: 升级 Go 发行版至 1.26.5（`go install golang.org/dl/go1.26.5@latest && go1.26.5 download`）
2. **本周**: 更新 `redis/go-redis/v9` 至 v9.6.3+
3. **本月**: 同步升级 `golang.org/x/net` 和 `golang.org/x/crypto` 间接依赖版本

---

*扫描命令: `govulncheck ./...` | 扫描日期: 2026-07-18*
