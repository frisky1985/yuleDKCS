# TSP 适配器集成指南

## 概述

本文档描述如何对接第三方 TSP（Trusted Service Provider）的 API 接口，实现数字钥匙的绑定、签发、吊销和状态查询功能。

本项目包含三个协议联盟的 TSP 适配器：

| 协议 | 联盟 | 适配器 | Java 类 |
|------|------|--------|---------|
| CCC | Car Connectivity Consortium | `adapter-ccc` | `CccAdapter` / `CccClient` |
| ICCOA | Intelligent Car Connectivity Over Air | `adapter-iccoa` | `IccoaAdapter` / `IccoaClient` |
| ICCE | Car Connectivity Experience | `adapter-icce` | `IcceAdapter` / `IcceClient` |

---

## 架构概览

```
┌──────────────┐     gRPC      ┌─────────────────────┐     HTTP/HTTPS     ┌──────────┐
│  Go Backend   │ ───────────→  │  Adapter gRPC Server │ ────────────────→ │  OEM TSP  │
│  (Hub)        │ ←───────────  │  (Java Spring Boot) │ ←──────────────── │  API      │
└──────────────┘               └─────────────────────┘                   └──────────┘
                                       │
                                       │ Spring DI
                                       ▼
                              ┌─────────────────────┐
                              │  AdapterRegistry     │
                              │  (CCC/ICCOA/ICCE)    │
                              └─────────────────────┘
```

Go 后端通过 gRPC 调用 Java 适配器服务。适配器通过 HTTP 调用 OEM 提供的 TSP API。

---

## 对接前置条件

OEM（车辆制造商）必须提供以下信息：

### 1. TSP API 端点

每个协议需要独立的 API 基础 URL（HTTPS 必须）：

| 协议 | 配置项 | 示例 |
|------|--------|------|
| CCC | `adapter.ccc.api-url` | `https://ccc-tsp.oe m.com` |
| ICCOA | `adapter.iccoa.api-url` | `https://iccoa-tsp.oe m.com` |
| ICCE | `adapter.icce.api-url` | `https://icce-tsp.oe m.com` |

### 2. 认证凭据

| 协议 | 认证方式 | 配置项 | 说明 |
|------|----------|--------|------|
| CCC | OAuth2 Client Credentials | `client-id`, `client-secret` | 通过 OAuth2 获取 Bearer Token |
| ICCOA | OAuth2 App Credentials | `app-id`, `app-secret` | 同 OAuth2，可能包含 region 参数 |
| ICCE | API Key | `api-key`, `tenant-id` | 静态 API Key + 租户标识 |

### 3. 网络要求

- 适配器服务器需能直连 TSP API 端点（HTTPS 443）
- TSP API 端点需具有公网或专线可达 IP
- 建议 TSP 响应时间 ≤ 5 秒（超时默认 30 秒）
- 支持 TLS 1.2+ 加密通信

### 4. API 端点清单

每个协议适配器需要 TSP 暴露以下 REST API：

#### 通用接口（所有协议）

| 操作 | 方法 | 描述 | 重试策略 |
|------|------|------|----------|
| `getVehicles` | GET | 获取用户车辆列表 | 最多 3 次，指数退避 |
| `requestKeys` | POST | 请求签发数字钥匙 | 最多 3 次，指数退避 |
| `revokeKeys` | POST | 吊销已有钥匙 | 最多 3 次，指数退避 |
| `bindKey` | POST | 绑定钥匙到设备 | 最多 3 次，指数退避 |
| `unbindKey` | POST | 解除钥匙绑定 | 最多 3 次，指数退避 |
| `getKeyStatus` | GET | 查询钥匙状态 | 最多 3 次，指数退避 |

#### CCC 特有端点（`adapter.ccc.api-url`）

| 操作 | HTTP | Path |
|------|------|------|
| getVehicles | GET | `/api/v1/users/{userId}/vehicles` |
| requestKeys | POST | `/api/v1/keys/request` |
| revokeKeys | POST | `/api/v1/keys/revoke` |
| bindKey | POST | `/api/v1/keys/bind` |
| unbindKey | POST | `/api/v1/keys/unbind` |
| getKeyStatus | GET | `/api/v1/keys/{keyId}/status` |

#### ICCOA 特有端点（`adapter.iccoa.api-url`）

| 操作 | HTTP | Path |
|------|------|------|
| getVehicles | GET | `/v1/vehicles?user_id={userId}` |
| requestKeys | POST | `/v1/keys/issue` |
| revokeKeys | POST | `/v1/keys/revoke` |
| bindKey | POST | `/v1/keys/bind` |
| unbindKey | POST | `/v1/keys/unbind` |
| getKeyStatus | GET | `/v1/keys/{keyId}` |

#### ICCE 特有端点（`adapter.icce.api-url`）

| 操作 | HTTP | Path |
|------|------|------|
| getVehicles | GET | `/api/v1/vehicles?userId={userId}` |
| requestKeys | POST | `/api/v1/keys` |
| revokeKeys | POST | `/api/v1/keys/revoke` |
| bindKey | POST | `/api/v1/keys/bind` |
| unbindKey | POST | `/api/v1/keys/unbind` |
| getKeyStatus | GET | `/api/v1/keys/{keyId}` |

---

## BindKey 流程详解

BindKey 是 TSP 适配器最关键的接口。它执行设备与车辆的钥匙绑定，包含以下步骤：

```
┌──────┐         ┌──────────┐          ┌──────────┐
│ Phone │         │ HUB      │          │ TSP API  │
│       │         │ (Go)     │          │ (OEM)    │
└──┬───┘         └────┬─────┘          └────┬─────┘
   │                  │                      │
   │  1. BindKeyReq   │                      │
   │ ───────────────→ │                      │
   │                  │  2. bindKey(req)      │
   │                  │ ──────────────────→  │
   │                  │                      │
   │                  │  3. ECDH Key Exchange│
   │                  │ ←── ──────────────── │
   │                  │                      │
   │                  │  4. BindKeyResponse  │
   │                  │ ←──────────────────  │
   │  5. Response     │                      │
   │ ←─────────────── │                      │
```

### 请求体（BindKeyRequest）

```json
{
  "userId": "user-xxx",
  "vehicleId": "vehicle-xxx",
  "vin": "WBA3A5C5XDF123456",
  "deviceId": "device-xxx",
  "devicePublicKey": "base64-encoded-device-ephemeral-public-key",
  "attestationToken": "base64-encoded-device-attestation",
  "options": {
    "keyType": "owner",
    "accessLevel": "1"
  }
}
```

### 响应体（BindKeyResponse）

```json
{
  "success": true,
  "message": "Success",
  "keyId": "tsp-key-uuid",
  "sharedSecret": "base64-ecdh-shared-secret",
  "tspPublicKey": "base64-tsp-ephemeral-public-key",
  "sessionId": "session-uuid",
  "keySlot": 1,
  "keyData": ["base64-encoded-key-material"]
}
```

> **⚠️ 关键点**：`sharedSecret` 必须由 TSP 使用 ECDH 协议基于设备公钥派生。不能为空。如果 TSP 返回空 sharedSecret，`ResponseValidator` 会记录 CRITICAL 级别的告警。

---

## 重试策略

所有 HTTP 调用默认启用指数退避重试：

| 参数 | 默认值 | 说明 |
|------|--------|------|
| 最大重试次数 | 3 | 首次失败后再重试最多 3 次 |
| 初始等待 | 500ms | 首次重试前等待 |
| 退避因子 | 2 | 每次重试等待时间翻倍 |
| 最大总超时 | 30s | 从首次请求到最终放弃的时间上限 |
| 抖动 | ±25% | 均匀分布，防止惊群效应 |

### 触发重试的条件

- 网络错误：timeout, connection refused, connection reset, EOF
- 5xx 服务端错误：500, 502, 503, 504

### 不重试的条件

- 4xx 客户端错误：400, 401, 403, 404

---

## 配置文件参考

### application-ccc.yml

```yaml
adapter:
  ccc:
    enabled: true
    api-url: ${CCC_API_URL}
    client-id: ${CCC_CLIENT_ID}
    client-secret: ${CCC_CLIENT_SECRET}
    connection-timeout: 10000
    read-timeout: 30000
```

### application-iccoa.yml

```yaml
adapter:
  iccoa:
    enabled: true
    api-url: ${ICCOA_API_URL}
    app-id: ${ICCOA_APP_ID}
    app-secret: ${ICCOA_APP_SECRET}
    region: ${ICCOA_REGION:cn}
```

### application-icce.yml

```yaml
adapter:
  icce:
    enabled: true
    api-url: ${ICCE_API_URL}
    api-key: ${ICCE_API_KEY}
    tenant-id: ${ICCE_TENANT_ID}
```

### 全局配置

```yaml
adapter:
  retry-enabled: true
  max-retries: 3
  timeout-ms: 30000
```

---

## 调试与排障

### 日志排查

各适配器使用独立的日志类别：

```yaml
logging:
  level:
    com.digitalkey.adapter.ccc: DEBUG     # CCC 适配器
    com.digitalkey.adapter.iccoa: DEBUG   # ICCOA 适配器
    com.digitalkey.adapter.icce: DEBUG    # ICCE 适配器
```

### 健康检查

查看适配器状态：

```bash
curl http://localhost:8080/actuator/health
```

返回内容包含 `adapters` 和 `enabled` 数量。

### 常见问题

| 问题 | 可能原因 | 解决方案 |
|------|----------|----------|
| `sharedSecret is empty` | TSP 未执行 ECDH 或返回格式不对 | 检查 TSP 的 bindKey 实现是否派生 sharedSecret |
| `Connection timeout` | TSP 端点不可达或防火墙阻挡 | 确认网络连通性，检查 TLS 证书 |
| `No adapter available` | 适配器未初始化或未启用 | 检查配置中 `enabled: true` 并查看启动日志 |
| `500/503 from TSP` | TSP 服务端暂时不可用 | 自动重试，检查 TSP 服务状态 |
| `Exhausted retries` | 持续不可恢复的错误 | 检查网络和 TSP API 可用性 |

---

## 对接验收 checklist

- [ ] OEM 提供 TSP API 端点 URL（每个协议独立）
- [ ] OEM 提供认证凭据（client-id/secret, app-id/secret, api-key 等）
- [ ] 网络连通性验证（telnet / curl 到 TSP 端点）
- [ ] 配置文件中填写正确的凭据
- [ ] 适配器启动日志无错误
- [ ] `GET /actuator/health` 返回 UP
- [ ] `getVehicles` 返回正确的车辆列表
- [ ] `bindKey` 返回非空的 `sharedSecret`
- [ ] `requestKeys` 成功签发钥匙
- [ ] `getKeyStatus` 返回正确的钥匙状态
- [ ] `revokeKeys` / `unbindKey` 成功吊销/解绑
- [ ] 重试机制生效（模拟 TSP 502 确认重试日志）
