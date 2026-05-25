# API 对齐报告: 移动端 SDK ↔ 后端

**日期**: 2026-05-26
**分析范围**: iOS SDK (APIService.swift), Android SDK (YuleDKCSApi.kt), 后端路由 (router.go + handlers)

---

## 1. 覆盖总览

| 分类 | 后端端点数 | iOS SDK | Android SDK |
|------|-----------|---------|------------|
| 用户认证 | 4 | 2/4 | 4/4 |
| 钥匙管理 | 9 | 5/9 | 8/9 |
| 钥匙分享 | 4 | 1/4 | 4/4 |
| 车辆管理 | 6 | 0/6 | 1/6 |
| OTA | 12 | 0/12 | 0/12 |
| WebSocket | 4 | 0/4 | 0/4 |
| **合计** | **39** | **8/39 (21%)** | **17/39 (44%)** |

---

## 2. 详细差异分析

### 2.1 用户认证 API

| 端点 | 方法 | 后端 | iOS | Android | 状态 |
|------|------|------|-----|---------|------|
| `/auth/login` | POST | ✅ | ✅ (ep: `/auth/login`) | ✅ (ep: `auth/login`) | ✅ 一致 |
| `/auth/register` | POST | ✅ | ✅ (ep: `/auth/register`) | ✅ (ep: `auth/register`) | ✅ 一致 |
| `/auth/refresh` | POST | ✅ | ❌ 缺失 | ✅ (ep: `auth/refresh`) | ⚠️ iOS 缺失 |
| `/user/profile` | GET | ✅ | ❌ 缺失 | ✅ (ep: `user/profile`) | ⚠️ iOS 缺失 |

### 2.2 钥匙管理 API

| 端点 | 方法 | 后端 | iOS | Android | 状态 |
|------|------|------|-----|---------|------|
| `/keys/issue` | POST | ✅ | ❌ 缺失 | ✅ (ep: `keys/issue`) | ⚠️ iOS 缺失 |
| `/keys` | GET | ✅ | ✅ (ep: `/keys`) | ✅ (ep: `keys`) | ✅ 一致 |
| `/keys/:id` | GET | ✅ | ✅ (ep: `/keys/{keyId}`) | ✅ (ep: `keys/{keyId}`) | ✅ 一致 |
| `/keys/:id/activate` | POST | ✅ | ✅ (ep: `/keys/{keyId}/activate`) | ✅ (ep: `keys/{keyId}/activate`) | ✅ 一致 |
| `/keys/:id/deactivate` | POST | ✅ | ✅ (ep: `/keys/{keyId}/deactivate`) | ✅ (ep: `keys/{keyId}/deactivate`) | ✅ 一致 |
| `/keys/:id` | DELETE | ✅ | ✅ (ep: `/keys/{keyId}`) | ✅ (ep: `keys/{keyId}`) | ✅ 一致 |
| `/keys/:id/permissions` | PUT | ✅ | ❌ 缺失 | ✅ (ep: `keys/{keyId}/permissions`) | ⚠️ iOS 缺失 |
| `/keys/:id/logs` | GET | ✅ | ✅ (ep: `/keys/{keyId}/logs`) | ✅ (ep: `keys/{keyId}/logs`) | ✅ 一致 |
| `/keys/shared/list` | GET | ✅ | ❌ 缺失 | ✅ (ep: `keys/shared/list`) | ⚠️ iOS 缺失 |

### 2.3 钥匙分享 API

| 端点 | 方法 | 后端 | iOS | Android | 状态 |
|------|------|------|-----|---------|------|
| `/keys/:id/share` | POST | ✅ | ✅ (ep: `/keys/{keyId}/share`) | ✅ (ep: `keys/{keyId}/share`) | ⚠️ 参数不一致 |
| `/keys/:id/shares` | GET | ✅ | ❌ 缺失 | ✅ (ep: `keys/{keyId}/shares`) | ⚠️ iOS 缺失 |
| `/keys/shares/:share_id` | DELETE | ✅ | ❌ 缺失 | ✅ (ep: `keys/shares/{shareId}`) | ⚠️ iOS 缺失 |

### 2.4 车辆管理 API

| 端点 | 方法 | 后端 | iOS | Android | 状态 |
|------|------|------|-----|---------|------|
| `/vehicles` | POST | ✅ | ❌ 缺失 | ❌ 缺失 | 🔴 双平台缺失 |
| `/vehicles` | GET | ✅ | ❌ 缺失 | ❌ 缺失 | 🔴 双平台缺失 |
| `/vehicles/:id` | GET | ✅ | ❌ 缺失 | ❌ 缺失 | 🔴 双平台缺失 |
| `/vehicles/:id/status` | GET | ✅ | ❌ 缺失 | ✅ (ep: `vehicles/{vehicleId}/status`) | ⚠️ iOS 缺失 |
| `/vehicles/:id/commands` | POST | ✅ | ❌ 缺失 | ❌ 缺失 | 🔴 双平台缺失 |
| `/vehicles/:id/commands/:command_id` | GET | ✅ | ❌ 缺失 | ❌ 缺失 | 🔴 双平台缺失 |

### 2.5 OTA API

| 端点 | 方法 | 后端 | iOS | Android | 状态 |
|------|------|------|-----|---------|------|
| 所有 OTA 端点 (12个) | 各种 | ✅ | ❌ 全部缺失 | ❌ 全部缺失 | 🔴 双平台缺失 |

**注意**: OTA 处理器已实现但路由器中未注册 (`router.go` 中缺少 `otaHandler.RegisterRoutes(authorized)`)。

### 2.6 WebSocket

| 端点 | 后端 | iOS | Android | 状态 |
|------|------|-----|---------|------|
| `/ws` | ✅ (501占位) | ❌ 缺失 | ❌ 缺失 | 🔴 双平台缺失 |
| 车辆/用户/管理员 WS | ✅ (已实现) | ❌ 缺失 | ❌ 缺失 | 🔴 双平台缺失 |

---

## 3. 请求/响应字段不匹配

### 🔴 KEY-01: 分享钥匙请求参数不一致

**后端期望:**
```go
type ShareKeyRequest struct {
    UserID      uint           `json:"user_id"`       // 目标用户 ID
    Permissions KeyPermissions `json:"permissions"`
    ExpiresAt   *time.Time     `json:"expires_at"`
}
```

**iOS 发送:**
```swift
struct ShareKeyRequest: Codable {
    let shared_to_username: String   // ← 用户名，非 ID
    let expires_at: String?
    let permissions: [String: Bool]
}
```

**Android 发送:**
```kotlin
data class ShareKeyRequest(
    val shared_to_username: String,  // ← 用户名，非 ID
    val expires_at: String?,
    val permissions: Map<String, Boolean>
)
```

**影响**: 后端不识别 `shared_to_username`，需要的是 `user_id`。分享钥匙 API 调用必然失败。

**修复建议**: 将移动端的 `shared_to_username` 改为 `user_id` (uint)。由移动端先通过 `/user/profile` 或搜索接口将用户名解析为用户 ID。

### 🟡 KEY-02: iOS 使用 `page_size` vs Android 使用 `page_size`

| SDK | 参数名 | 后端期望 |
|-----|--------|---------|
| iOS | `page_size` | `page_size` |
| Android | `page_size` | `page_size` |

两者一致，无问题。

### 🟡 KEY-03: 后端 Context Key 不一致

**用户处理器** (`user_handler.go`) 使用:
```go
userID, exists := c.Get("userID")   // 正确
```

**钥匙处理器** (`key_handler.go`) 使用:
```go
userID, exists := c.Get("user_id")   // 错误!
```

但 JWT 中间件注册的是:
```go
c.Set("userID", claims.UserID)
```

**影响**: 钥匙处理器无法从 context 中获取用户 ID，所有钥匙相关接口认证将返回 401。

**修复建议**: 
- 方案A: 将 `key_handler.go` 中 `c.Get("user_id")` 统一改为 `c.Get("userID")`
- 方案B: 在 JWT 中间件中同时设置 `c.Set("user_id", ...)`

---

## 4. 功能缺口汇总

### 4.1 iOS SDK 缺失功能

| # | 功能 | 优先级 | 说明 |
|---|------|--------|------|
| 1 | Token 刷新 | 高 | `/auth/refresh` 用于无感续期 |
| 2 | 用户信息查询 | 高 | `/user/profile` 获取用户资料 |
| 3 | 发行钥匙 | 高 | `/keys/issue` 移动端注册钥匙的核心功能 |
| 4 | 更新权限 | 高 | `/keys/:id/permissions` 修改钥匙权限 |
| 5 | 分享列表 | 中 | `/keys/shared/list` 查看收到的分享 |
| 6 | 分享详情 | 中 | `/keys/:id/shares` 查看钥匙的分享记录 |
| 7 | 撤销分享 | 中 | `/keys/shares/:share_id` 撤销分享 |
| 8 | 车辆管理全部 | 高 | 车辆注册/列表/详情/命令/状态 |
| 9 | OTA全部 | 低 | 固件管理/OTA状态/更新检查 |
| 10 | WebSocket | 中 | 实时通知接收 |

### 4.2 Android SDK 缺失功能

| # | 功能 | 优先级 | 说明 |
|---|------|--------|------|
| 1 | 车辆注册/列表/详情 | 高 | 车辆管理核心功能 |
| 2 | 发送车辆命令 | 高 | 远程控制车辆 |
| 3 | 命令状态查询 | 中 | 查询命令执行结果 |
| 4 | OTA全部 | 低 | 固件管理和OTA更新 |
| 5 | WebSocket | 中 | 实时通知接收 |

### 4.3 后端问题

| # | 问题 | 严重度 | 说明 |
|---|------|--------|------|
| 1 | OTA 路由未注册 | 高 | `otaHandler.RegisterRoutes(authorized)` 在 `router.go` 中缺失 |
| 2 | Context key 不一致 | 高 | `key_handler.go` 使用 `user_id`，JWT中间件设置 `userID` |
| 3 | 车辆 `UpdateLocation`/`Heartbeat` 未注册 | 中 | 处理器中有方法但路由未注册 |
| 4 | 错误格式不一致 | 中 | 钥匙处理器用 `{code, message, data}`，车辆处理器用 `{error}` |
| 5 | `/ws` 占位符 | 中 | 返回 501，未接入 WebSocket Hub |
| 6 | `user_handler.go` 获取 `userID` 用字符串转换 | 低 | `GetProfile` 中手动转换字符串→uint，应使用 `strconv` |

---

## 5. 修复建议 (优先级排序)

### 紧急 (P0 - 导致功能不可用)

1. **修复 Context Key 不一致**: 修改 `key_handler.go` 中所有 `c.Get("user_id")` 为 `c.Get("userID")`
2. **注册 OTA 路由**: 在 `router.go` 的 authorized 组中添加 `otaHandler.RegisterRoutes(authorized)`
3. **修复分享参数**: 后端或移动端统一 `shared_to_username` → `user_id`

### 高优先级 (P1 - 核心功能缺失)

1. **iOS SDK**: 添加 `issueKey`, `refreshToken`, `getProfile`, `getSharedKeys` 等基础接口
2. **双平台**: 添加车辆管理 API (`registerVehicle`, `getVehicles`, `getVehicle`, `sendCommand`, `getCommandStatus`)
3. **iOS SDK**: 修复 `page` 参数名（当前 iOS 使用 `page` 但端点 `/keys` 后端也使用 `page`，这没问题）

### 中优先级 (P2 - 体验增强)

1. **双平台**: 实现 WebSocket 连接管理
2. **错误格式统一**: 后端车辆处理器改用统一响应格式 `{code, message, data}`
3. **`GetProfile` 优化**: 使用 `strconv.Atoi` 替代手动转换

### 低优先级 (P3 - 扩展功能)

1. **OTA 客户端**: 在移动端添加 OTA API 调用
2. **WS 路由**: 接入真正的 WebSocket Hub

---

## 6. 对齐检查清单

### iOS SDK 需要添加

- [ ] `POST /auth/refresh` — refreshToken
- [ ] `GET /user/profile` — getProfile
- [ ] `POST /keys/issue` — issueKey(keyRequest)
- [ ] `PUT /keys/:id/permissions` — updatePermissions(keyId, permissions)
- [ ] `GET /keys/shared/list` — getSharedKeys()
- [ ] `GET /keys/:id/shares` — getKeyShares(keyId)
- [ ] `DELETE /keys/shares/:share_id` — revokeShare(shareId)
- [ ] `POST /vehicles` — registerVehicle(request)
- [ ] `GET /vehicles` — getVehicles(page, pageSize)
- [ ] `GET /vehicles/:id` — getVehicle(vehicleId)
- [ ] `GET /vehicles/:id/status` — getVehicleStatus(vehicleId)
- [ ] `POST /vehicles/:id/commands` — sendCommand(vehicleId, command)
- [ ] `GET /vehicles/:id/commands/:command_id` — getCommandStatus(vehicleId, commandId)

### Android SDK 需要添加

- [ ] `POST /vehicles` — registerVehicle
- [ ] `GET /vehicles` — listVehicles
- [ ] `GET /vehicles/:id` — getVehicleDetail
- [ ] `POST /vehicles/:id/commands` — sendCommand
- [ ] `GET /vehicles/:id/commands/:command_id` — getCommandStatus

### 字段对齐修复

- [ ] `ShareKeyRequest.shared_to_username` → `ShareKeyRequest.user_id` (uint)
- [ ] 后端 `key_handler.go` context key: `user_id` → `userID`
- [ ] 后端 OTA handler: 注册到 router.go
