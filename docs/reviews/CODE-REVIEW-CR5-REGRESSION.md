# CR-5 Handler 修复 — 回归审查报告

审查工具：Hermes Agent
审查日期：2026-05-31 17:18
审查文件：`backend/cloud/hub/internal/gateway/rest_gateway.go`

---

## 审查结论：✅ 全部 PASS — 已自动 Commit

| 回归项 | 结果 | 说明 |
|--------|------|------|
| P0: RevokeKeyRequest.UserId | ✅ PASS | 已移除 `UserId` 字段，user_id 通过 auth middleware 注入的 gRPC metadata 传递 |
| P1: sendCommand Params | ✅ PASS | `Params` 声明为 `json.RawMessage`，直接 `[]byte(body.Params)` 类型转换，无二次 `json.Marshal` |
| P2: listAdapters/hubHealthCheck | ✅ PASS | 两处 handler 均改用 `g.replyProto(c, resp)`，旧 `c.JSON` 代码已清理 |
| P2: handleGRPCError 状态码 | ✅ PASS | 已增加 `DeadlineExceeded → 504` 和 `ResourceExhausted → 429` 映射 |

---

## 逐项详情

### P0: RevokeKeyRequest.UserId ✅

`revokeKey` handler (L426-458) 构造 `RevokeKeyRequest` 时仅设置 `KeyId`、`Reason`、`TraceId`，无 `UserId` 字段。`userID` 从 gin context 获取后仅用于日志记录，user_id 通过 auth middleware 自动注入的 gRPC metadata (`metadata.Pairs("user_id", userID)`) 传递到下游。

### P1: sendCommand Params ✅

`sendCommand` handler (L678-695) 中 `Params` 声明为 `json.RawMessage`（即 `[]byte`），赋值时使用 `Params: []byte(body.Params)` 直接类型转换，无 `json.Marshal` 调用。`json.RawMessage` 在 `ShouldBindJSON` 时已以原始 JSON 字节形式保留，没有二次序列化。

### P2: listAdapters/hubHealthCheck ✅

- `listAdapters`: `g.replyProto(c, resp)` — 使用统一的 proto 序列化响应
- `hubHealthCheck`: `g.replyProto(c, resp)` — 同上
- 两处 handler 的成功路径均已清除旧的 `c.JSON` 调用

### P2: handleGRPCError 状态码 ✅

`handleGRPCError` (L245-281) 已增加两种映射:
- `codes.DeadlineExceeded` → `http.StatusGatewayTimeout` (504)
- `codes.ResourceExhausted` → `http.StatusTooManyRequests` (429)

完整映射覆盖 9 种 gRPC 状态码，所有 handler 均通过 `handleGRPCError` 进行错误转换。

---

## Commit 信息

```
提交: fb427d7
信息: ✅ 回归审查通过: REST gateway handler 所有问题已修复
文件: backend/cloud/hub/internal/gateway/rest_gateway.go
变更: 562 insertions, 19 deletions
```
