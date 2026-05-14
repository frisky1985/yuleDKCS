# API 字段对齐检查报告

## 检查日期: 2024-05-12

---

## 1. Key (钥匙) 模型对比

### 后端 (backend/internal/models/key.go)
```go
type Key struct {
    ID             uint
    UserID         uint
    VehicleID      uint
    Type           KeyType     // "CCC" | "ICCE" | "ICCOA"
    Status         KeyStatus   // "active" | "inactive" | "revoked" | "expired"
    Permissions    KeyPermissions
    KeyIdentifier  string
    Name           string
    Description    string
    ExpiresAt      *time.Time
    LastUsedAt     *time.Time
    UsageCount     int
    CreatedAt      time.Time
    UpdatedAt      time.Time
}

type KeyPermissions struct {
    Unlock      bool
    Lock        bool
    StartEngine bool
    Trunk       bool
    Windows     bool
}
```

### 前端 (frontend/src/api/keys.ts)
```typescript
interface DigitalKey {
    id: string;           // ❌ 后端是 uint
    vehicle: Vehicle;     // ❌ 后端直接是VehicleID
    protocol: 'CCC' | 'ICCOA' | 'ICCE';  // ❌ 后端是 Type
    type: 'owner' | 'shared';  // ❌ 后端没有这个枚举
    status: 'active' | 'inactive' | 'expired' | 'revoked';
    permissions: KeyPermission[];  // ❌ 类型不匹配
    issued_at: string;    // ❌ 后端是 CreatedAt
    expires_at?: string;
    shared_by?: string;
    shared_to?: string;
    share_count: number;  // ❌ 后端没有这个字段
    last_used_at?: string;
    use_count: number;    // ❌ 后端是 UsageCount
}

interface KeyPermission {
    type: 'unlock' | 'lock' | 'start_engine' | ...;
    enabled: boolean;
    constraints?: {...}
}
```

### 问题汇总
| 字段 | 后端 | 前端 | 状态 |
|------|------|------|------|
| id | uint | string | ❌ 类型不匹配 |
| vehicle | VehicleID (uint) | Vehicle (object) | ❌ 结构不匹配 |
| protocol/type | Type (CCC/ICCE/ICCOA) | protocol + type | ❌ 字段定义不一致 |
| permissions | KeyPermissions (object) | KeyPermission[] | ❌ 结构不同 |
| created_at | CreatedAt (time) | issued_at (string) | ⚠️ 命名不同 |
| usage_count | UsageCount (int) | use_count (number) | ⚠️ 命名不同 |

---

## 2. User (用户) 模型对比

### 后端 (backend/internal/models/user.go)
```go
type User struct {
    ID        uint
    Username  string
    Email     string
    Phone     string
    Password  string
    Role      string   // "user" | "admin"
    CreatedAt time.Time
    UpdatedAt time.Time
}
```

### 前端 (frontend/src/store/auth.ts)
```typescript
interface User {
    id: string      // ❌ 后端是 uint
    username: string
    email: string
    role: 'admin' | 'user'
}
```

---

## 3. 建议修复方案

### 方案A: 统一转换为JSON字符串ID (推荐)
前后端都使用 string 类型的 ID，避免 JavaScript number 精度丢失问题。

### 方案B: 创建转换层
在前端和后端都创建数据转换层，统一数据格式。

---

## 4. 需要同步的修改

### 前端修改 (frontend/src/api/keys.ts)
1. `id: string` → `id: number`
2. `protocol` + `type` → 合并为 `type: 'CCC' \| 'ICCE' \| 'ICCOA'`
3. `permissions: KeyPermission[]` → `permissions: KeyPermissions` (object)
4. `issued_at` → `created_at`
5. `use_count` → `usage_count`

### 后端修改
1. KeyResponse 中的 ID 可以转为 string 输出
2. 增加 issued_at 别名映射到 created_at

---

## 5. Mobile SDK 检查

### Android (YuleDKCSApi.kt)
```kotlin
// DigitalKey 模型需要与后端对齐
data class DigitalKey(
    val id: String,           // 需要确认是 String 还是 Long
    val type: String,
    val status: String,
    // ...
)
```

### iOS (APIService.swift)
```swift
struct DigitalKey: Codable {
    let id: String  // 需要确认类型
    // ...
}
```

---

## 结论

**优先级: 高**

前后端数据模型存在多处不匹配，建议：
1. 统一 ID 类型（推荐字符串）
2. 统一权限结构（对象 vs 数组）
3. 统一字段命名（snake_case）
4. 添加数据转换层处理差异

**下一步行动**: 创建数据转换工具函数，统一三端数据格式
