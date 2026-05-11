# 三端通信协议规范 — 予乐 Yule Notion

> **版本：** v1.0  
> **日期：** 2026-05-10  
> **适用范围：** Web端 / Electron端 / 服务端 / 数据层  

---

## 一、架构总览

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              三端通信架构                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   ┌──────────────────┐     HTTPS/WSS     ┌──────────────────┐              │
│   │   【第一端】      │ ◄──────────────► │   【第二端】      │              │
│   │   客户端层        │    REST API      │   服务端层        │              │
│   │   (Web/Electron) │                  │   (Node.js)      │              │
│   │                  │                  │                  │              │
│   │  • Vue 3 SPA     │                  │  • Express       │              │
│   │  • IndexedDB     │                  │  • JWT Auth      │              │
│   │  • ServiceWorker │                  │  • Sync Engine   │              │
│   └────────┬─────────┘                  └────────┬─────────┘              │
│            │                                      │                        │
│            │  ① 请求/响应                          │  ② SQL                │
│            │  ② 同步变更                          │  ③ 事务               │
│            │  ③ 文件上传                          │  ④ 全文检索            │
│            │                                      │                        │
│            │                              ┌───────┴─────────┐              │
│            └──────────────────────────────►   【第三端】      │              │
│                                               数据存储层      │              │
│                                               (PostgreSQL)   │              │
│                                                             │              │
│                                               • users        │              │
│                                               • pages        │              │
│                                               • tags         │              │
│                                               • sync_log     │              │
│                                               └──────────────┘              │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 二、第一端 ↔ 第二端：客户端-服务端通信协议

### 2.1 基础约定

| 项目 | 规范 |
|------|------|
| Base URL | `/api/v1` |
| 协议 | HTTPS（生产环境强制） |
| Content-Type | `application/json`（默认）；`multipart/form-data`（文件上传） |
| 字符编码 | UTF-8 |
| 时间格式 | ISO 8601 UTC (`2026-03-20T10:00:00.000Z`) |
| ID 格式 | UUID v4 |
| 认证方式 | `Authorization: Bearer <token>` |
| 分页参数 | `?page=1&pageSize=20`（page 从 1 开始，pageSize 范围 1-100） |

### 2.2 统一响应格式

**成功响应（单资源）：**
```json
{
  "data": { ... }
}
```

**成功响应（列表/分页）：**
```json
{
  "data": [ ... ],
  "meta": {
    "page": 1,
    "pageSize": 20,
    "total": 100
  }
}
```

**错误响应：**
```json
{
  "error": {
    "code": "RESOURCE_NOT_FOUND",
    "message": "页面不存在",
    "details": {}
  }
}
```

### 2.3 HTTP 状态码规范

| 状态码 | 含义 | 使用场景 |
|--------|------|----------|
| 200 | OK | 查询/更新成功 |
| 201 | Created | 创建成功 |
| 204 | No Content | 删除成功（无响应体） |
| 400 | Bad Request | 参数验证失败 |
| 401 | Unauthorized | 未认证 / Token 过期 |
| 403 | Forbidden | 无权限访问该资源 |
| 404 | Not Found | 资源不存在 |
| 409 | Conflict | 资源冲突 / 同步冲突 |
| 413 | Payload Too Large | 文件超过大小限制 |
| 422 | Unprocessable Entity | 业务逻辑校验失败 |
| 429 | Too Many Requests | 请求频率超限 |
| 500 | Internal Server Error | 服务端异常 |

### 2.4 错误码枚举

| 错误码 | HTTP 状态码 | 说明 |
|--------|------------|------|
| `VALIDATION_ERROR` | 400 | 请求参数校验失败 |
| `UNAUTHORIZED` | 401 | 未提供或 Token 无效 |
| `TOKEN_EXPIRED` | 401 | Token 已过期 |
| `FORBIDDEN` | 403 | 无权访问此资源 |
| `RESOURCE_NOT_FOUND` | 404 | 资源不存在 |
| `EMAIL_ALREADY_EXISTS` | 409 | 邮箱已被注册 |
| `INVALID_CREDENTIALS` | 401 | 邮箱或密码错误 |
| `FILE_TOO_LARGE` | 413 | 文件超过 5MB |
| `UNSUPPORTED_FILE_TYPE` | 422 | 不支持的文件类型 |
| `SYNC_CONFLICT` | 409 | 同步冲突 |
| `RATE_LIMIT_EXCEEDED` | 429 | 请求频率超限 |
| `INTERNAL_ERROR` | 500 | 服务端内部错误 |
| `VERSION_NOT_SUPPORTED` | 406 | 请求的 API 版本不被支持 |
| `VERSION_DEPRECATED` | 410 | 该版本已被弃用 |

### 2.5 API 端点总览

| 方法 | 路径 | 认证 | 说明 |
|------|------|------|------|
| **认证模块** ||||
| POST | `/api/v1/auth/register` | ❌ | 用户注册 |
| POST | `/api/v1/auth/login` | ❌ | 用户登录 |
| GET | `/api/v1/auth/me` | ✅ | 获取当前用户 |
| **页面模块** ||||
| GET | `/api/v1/pages` | ✅ | 获取页面列表/树 |
| POST | `/api/v1/pages` | ✅ | 创建页面 |
| GET | `/api/v1/pages/:id` | ✅ | 获取单个页面 |
| PUT | `/api/v1/pages/:id` | ✅ | 更新页面（乐观锁：If-Match） |
| DELETE | `/api/v1/pages/:id` | ✅ | 删除页面（软删除） |
| PUT | `/api/v1/pages/:id/move` | ✅ | 移动页面 |
| **搜索模块** ||||
| GET | `/api/v1/search?q=` | ✅ | 全文搜索（高亮返回） |
| **标签模块** ||||
| GET | `/api/v1/tags` | ✅ | 获取所有标签 |
| POST | `/api/v1/tags` | ✅ | 创建标签 |
| DELETE | `/api/v1/tags/:id` | ✅ | 删除标签 |
| PUT | `/api/v1/pages/:id/tags` | ✅ | 更新页面标签 |
| GET | `/api/v1/tags/:id/pages` | ✅ | 获取标签下页面 |
| **文件模块** ||||
| POST | `/api/v1/upload/image` | ✅ | 上传图片（multipart） |
| **导出模块** ||||
| GET | `/api/v1/pages/:id/export?format=markdown` | ✅ | 导出 Markdown |
| GET | `/api/v1/pages/:id/export?format=pdf` | ✅ | 导出 PDF |
| **同步模块** ||||
| POST | `/api/v1/sync` | ✅ | 推送本地变更 |
| GET | `/api/v1/sync/changes?since=` | ✅ | 获取服务端增量变更 |

### 2.6 同步协议详解

#### 2.6.1 推送本地变更

**请求：**
```json
{
  "changes": [
    {
      "type": "create" | "update" | "delete",
      "pageId": "uuid",
      "title": "string",
      "content": { "type": "doc", "content": [] },
      "version": 3,
      "localUpdatedAt": "2026-03-20T15:00:00.000Z",
      "parentId": "uuid | null",
      "order": 0,
      "icon": "📄"
    }
  ],
  "lastSyncAt": "2026-03-20T10:00:00.000Z"
}
```

**响应：**
```json
{
  "data": {
    "synced": ["page-uuid-1", "page-uuid-2"],
    "conflicts": [
      {
        "pageId": "uuid",
        "localVersion": 3,
        "serverVersion": 5,
        "resolution": "server_wins",
        "serverPage": { ... }
      }
    ],
    "serverChanges": [ ... ],
    "syncedAt": "2026-03-20T15:10:00.000Z"
  }
}
```

#### 2.6.2 冲突解决策略（LWW）

```
比较规则：
┌─────────────────────┬─────────────────────┬─────────────────────┐
│   服务端版本        │   客户端版本        │   结果              │
├─────────────────────┼─────────────────────┼─────────────────────┤
│ version = 3         │ version = 3         │ 无冲突，正常同步    │
│ updatedAt = 10:00   │ localUpdatedAt = 12:00 │                  │
├─────────────────────┼─────────────────────┼─────────────────────┤
│ version = 5         │ version = 3         │ 冲突！              │
│ updatedAt = 14:00   │ localUpdatedAt = 12:00 │ server_wins       │
├─────────────────────┼─────────────────────┼─────────────────────┤
│ version = 3         │ version = 3         │ 无冲突              │
│ updatedAt = 10:00   │ localUpdatedAt = 08:00 │ 服务端版本更新   │
└─────────────────────┴─────────────────────┴─────────────────────┘
```

---

## 三、第二端 ↔ 第三端：服务端-数据层通信协议

### 3.1 数据库连接规范

| 项目 | 规范 |
|------|------|
| 驱动 | Knex.js (Query Builder) |
| 数据库 | PostgreSQL 15+ |
| 连接池 | 默认 min: 2, max: 10 |
| 超时 | acquire: 60000ms, create: 30000ms |
| 编码 | UTF-8 |
| 时区 | UTC |

### 3.2 数据库 Schema

```sql
-- 用户表
CREATE TABLE users (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    email VARCHAR(254) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    name VARCHAR(50) NOT NULL,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);

-- 页面表（支持树形结构 + 软删除 + 版本控制）
CREATE TABLE pages (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    title VARCHAR(255) DEFAULT '无标题',
    content JSONB DEFAULT '{"type":"doc","content":[]}'::jsonb,
    parent_id UUID REFERENCES pages(id) ON DELETE CASCADE,
    "order" INTEGER DEFAULT 0,
    icon VARCHAR(10) DEFAULT '📄',
    version INTEGER DEFAULT 1,
    is_deleted BOOLEAN DEFAULT FALSE,
    deleted_at TIMESTAMPTZ,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);

-- 标签表
CREATE TABLE tags (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    name VARCHAR(30) NOT NULL,
    color VARCHAR(7) DEFAULT '#6b7280',
    created_at TIMESTAMPTZ DEFAULT NOW(),
    UNIQUE(user_id, name)
);

-- 页面标签关联表
CREATE TABLE page_tags (
    page_id UUID REFERENCES pages(id) ON DELETE CASCADE,
    tag_id UUID REFERENCES tags(id) ON DELETE CASCADE,
    PRIMARY KEY (page_id, tag_id)
);

-- 文件上传表
CREATE TABLE uploads (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    filename VARCHAR(255) NOT NULL,
    path VARCHAR(500) NOT NULL,
    mime_type VARCHAR(100) NOT NULL,
    size INTEGER NOT NULL,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

-- 同步日志表（用于增量同步）
CREATE TABLE sync_log (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    page_id UUID REFERENCES pages(id) ON DELETE CASCADE,
    operation VARCHAR(10) NOT NULL, -- CREATE/UPDATE/DELETE
    server_version INTEGER NOT NULL,
    synced_at TIMESTAMPTZ DEFAULT NOW()
);
```

### 3.3 全文搜索索引

```sql
-- 为标题和内容创建全文搜索索引
CREATE INDEX idx_pages_fulltext ON pages 
USING GIN (to_tsvector('simple', title || ' ' || COALESCE(content->>'text', '')));

-- 为更新时间创建索引（同步查询优化）
CREATE INDEX idx_pages_updated_at ON pages(user_id, updated_at);
CREATE INDEX idx_pages_parent ON pages(user_id, parent_id) WHERE is_deleted = FALSE;
```

### 3.4 事务规范

| 场景 | 事务级别 | 说明 |
|------|----------|------|
| 单条查询 | 无 | 普通 SELECT |
| 同步变更处理 | READ COMMITTED | 批量写入 + 冲突检测 |
| 页面移动（含子页面） | SERIALIZABLE | 防止树形结构循环引用 |
| 删除页面（级联软删除） | READ COMMITTED | 递归更新 is_deleted |

### 3.5 查询模式示例

**树形页面查询：**
```sql
-- 获取用户的根页面（扁平列表）
SELECT * FROM pages 
WHERE user_id = ? AND parent_id IS NULL AND is_deleted = FALSE 
ORDER BY "order" ASC;

-- 获取子页面
SELECT * FROM pages 
WHERE parent_id = ? AND is_deleted = FALSE 
ORDER BY "order" ASC;
```

**全文搜索：**
```sql
-- 高亮搜索结果
SELECT 
    id, title, icon, updated_at,
    ts_rank(to_tsvector('simple', title || ' ' || COALESCE(content->>'text', '')), 
            plainto_tsquery('simple', ?)) AS rank,
    ts_headline('simple', title, plainto_tsquery('simple', ?), 
                'MaxWords=50, MinWords=10, HighlightAll=true') AS title_highlight,
    ts_headline('simple', COALESCE(content->>'text', ''), plainto_tsquery('simple', ?),
                'MaxWords=100, MinWords=20') AS snippet
FROM pages
WHERE user_id = ? AND is_deleted = FALSE
    AND to_tsvector('simple', title || ' ' || COALESCE(content->>'text', '')) 
        @@ plainto_tsquery('simple', ?)
ORDER BY rank DESC, updated_at DESC
LIMIT ? OFFSET ?;
```

**增量同步查询：**
```sql
-- 获取指定时间后的变更
SELECT * FROM pages
WHERE user_id = ? AND updated_at > ? AND is_deleted = FALSE
ORDER BY updated_at ASC
LIMIT ?;
```

---

## 四、第一端内部：客户端离线存储协议

### 4.1 IndexedDB Schema

```typescript
// Dexie 数据库定义
interface LocalPage {
  id: string                    // UUID
  title: string
  content: object               // TipTap JSON
  parentId: string | null
  order: number
  icon: string
  tags: string[]
  createdAt: string             // ISO 8601
  updatedAt: string
  version: number
  isDeleted: boolean
  // 本地扩展字段
  _syncStatus: 'synced' | 'pending' | 'conflict'
  _dirty: boolean
}

interface LocalTag {
  id: string
  name: string
  color: string
  pageCount: number
  createdAt: string
}

interface SyncQueueItem {
  id?: number                   // 自增ID
  pageId: string
  type: 'create' | 'update' | 'delete'
  payload?: object              // 变更内容
  createdAt: string
  retryCount?: number           // 重试次数
}

interface SyncMeta {
  key: string
  value: any
}

// 索引定义
const dbSchema = {
  pages: 'id, parentId, updatedAt, title, [_syncStatus+updatedAt]',
  tags: 'id, name',
  syncQueue: '++id, pageId, type, createdAt',
  meta: 'key'
};
```

### 4.2 自动保存流程

```
用户编辑
    │
    ▼
[1] Debounce 1000ms ──────────────┐
    │                              │
    ▼                              │
[2] 写入 IndexedDB               │
    • pages.content               │
    • pages.updatedAt = now()     │
    • pages._syncStatus = 'pending'
    • pages._dirty = true         │
    │                              │
    ▼                              │
[3] 加入 syncQueue               │
    • type: 'update'              │
    • pageId, createdAt           │
    │                              │
    ▼                              │
[4] 触发同步引擎 ──────────────────┘
    • 如果在线：立即同步
    • 如果离线：等待网络恢复
```

### 4.3 Service Worker 缓存策略

```javascript
// workbox 配置
const cacheStrategy = {
  // 静态资源：Cache First
  staticAssets: {
    pattern: /\.(js|css|woff2?|png|jpg|svg)$/,
    strategy: 'CacheFirst',
    expiration: { maxAge: 30 * 24 * 60 * 60 } // 30天
  },
  
  // API 请求：Network First（支持离线）
  apiCalls: {
    pattern: /\/api\/v1\//,
    strategy: 'NetworkFirst',
    backgroundSync: {
      name: 'api-sync-queue',
      options: {
        maxRetentionTime: 24 * 60 // 24小时
      }
    }
  }
};
```

---

## 五、通信时序图

### 5.1 正常同步流程

```
客户端                                    服务端                         数据库
  │                                         │                              │
  │  [用户编辑]                             │                              │
  │     │                                   │                              │
  │     ▼                                   │                              │
  │  ┌─────────────────┐                    │                              │
  │  │ 1. 写入IndexedDB │                   │                              │
  │  │ 2. 加入syncQueue │                   │                              │
  │  └────────┬────────┘                    │                              │
  │           │                             │                              │
  │           │  POST /api/v1/sync          │                              │
  │           ├────────────────────────────►│                              │
  │           │ {changes, lastSyncAt}       │                              │
  │           │                             │  BEGIN TRANSACTION           │
  │           │                             ├─────────────────────────────►│
  │           │                             │  SELECT version, updated_at  │
  │           │                             │  FROM pages WHERE id IN (...)│
  │           │                             ├─────────────────────────────►│
  │           │                             │◄─────────────────────────────│
  │           │                             │                              │
  │           │                             │  冲突检测 & LWW 解决         │
  │           │                             │                              │
  │           │                             │  INSERT/UPDATE pages         │
  │           │                             ├─────────────────────────────►│
  │           │                             │  INSERT sync_log             │
  │           │                             ├─────────────────────────────►│
  │           │                             │  COMMIT                      │
  │           │                             ├─────────────────────────────►│
  │           │                             │◄─────────────────────────────│
  │           │  200 OK                     │                              │
  │           │  {synced, conflicts,        │                              │
  │           │   serverChanges}            │                              │
  │           │◄────────────────────────────│                              │
  │           │                             │                              │
  │  ┌────────┴────────┐                    │                              │
  │  │ 处理响应：       │                    │                              │
  │  │ • 清空syncQueue │                    │                              │
  │  │ • 应用serverChanges                │                              │
  │  │ • 处理conflicts  │                   │                              │
  │  │ • 更新lastSyncAt │                   │                              │
  │  └─────────────────┘                    │                              │
  │                                         │                              │
```

### 5.2 离线 → 在线切换

```
客户端
  │
  │  [离线状态]
  │     │
  │     ▼
  │  ┌─────────────────────────┐
  │  │ 编辑操作进入syncQueue   │
  │  │ 显示"离线 - 等待同步"   │
  │  └─────────────────────────┘
  │     │
  │     │  [网络恢复]
  │     ▼
  │  ┌─────────────────────────┐
  │  │ navigator.onLine事件    │
  │  │ 触发SyncEngine.sync()   │
  │  └─────────────────────────┘
  │     │
  │     ▼
  │  执行正常同步流程 ────────► 服务端
  │     │
  │     ▼
  │  ┌─────────────────────────┐
  │  │ 同步完成                │
  │  │ 显示"已同步"            │
  │  └─────────────────────────┘
```

---

## 六、安全规范

### 6.1 认证流程

```
登录/注册
    │
    ▼
服务端验证凭证
    │
    ├─ 失败 ──► 401 Unauthorized
    │
    ▼
生成 JWT Token
  • Payload: { userId, email, iat, exp }
  • 过期时间: 7天
  • 签名: HS256
    │
    ▼
返回 { token, user }
    │
    ▼
客户端存储 token (localStorage)
    │
    ▼
后续请求 Header: Authorization: Bearer <token>
    │
    ▼
服务端中间件验证 JWT
    ├─ 无效/过期 ──► 401
    └─ 有效 ───────► 继续处理
```

### 6.2 数据隔离原则

- **数据库层**：所有查询必须包含 `WHERE user_id = ?`
- **服务端层**：Controller 从 JWT 解析 userId，不信赖客户端传入
- **客户端层**：IndexedDB 按用户隔离（不同用户 = 不同数据库名或清空）

### 6.3 输入校验

| 层级 | 校验方式 | 说明 |
|------|----------|------|
| 客户端 | Zod Schema | 即时反馈，减少无效请求 |
| 服务端 | Zod Validation | 强制校验，拒绝非法数据 |
| 数据库 | Constraints | 最终防线，类型/唯一性约束 |

---

## 七、性能规范

### 7.1 响应时间目标

| 操作 | 目标时间 | 说明 |
|------|----------|------|
| API 响应（正常） | < 200ms | 95th percentile |
| 搜索响应 | < 500ms | 1000篇文档内 |
| 页面加载（有网） | < 3s | 首次加载 |
| 页面加载（离线） | < 1.5s | 从 IndexedDB |
| 同步完成 | < 2s | 单次同步 < 100条变更 |
| 编辑器输入延迟 | < 50ms | 本地保存 |

### 7.2 分页与限流

- **列表查询**：默认 pageSize=20，最大 pageSize=100
- **同步变更**：单次最大 500 条
- **文件上传**：最大 5MB
- **API 限流**：每分钟 100 次（认证接口更严格）

---

## 八、版本控制

### 8.1 协议版本策略

| 版本 | URL | 状态 | 发布日期 | 计划下线 |
|------|-----|------|----------|----------|
| v1 | `/api/v1/*` | 当前稳定版 | 2026-03-20 | 暂定 |
| v2 | `/api/v2/*` | 未来扩展 | TBD | - |

### 8.2 版本协商机制

#### 8.2.1 请求头部

客户端可通过以下头部指定期望的 API 版本：

| 头部 | 格式 | 说明 |
|------|------|------|
| `Accept-Version` | `v1`, `v2`, `v1.1` | 客户端期望的协议版本 |
| `X-API-Version` (已弃用) | - | 保留向后兼容，建议迁移到 Accept-Version |

**示例：**
```http
GET /api/v1/pages HTTP/1.1
Host: api.yule-notion.com
Authorization: Bearer <token>
Accept-Version: v1
```

#### 8.2.2 版本匹配规则

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        版本匹配规则                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   1. URL 版本为主，Accept-Version 为辅：                             │
│      - 先匹配 URL 中的版本标识 (/api/v1/ → v1)                   │
│      - 若 Accept-Version 与 URL 版本不一致，返回 406                │
│                                                                     │
│   2. 版本兼容性：                                                      │
│      - 同一大版本内保持向后兼容 (v1.x 兼容 v1.0)                   │
│      - 不同大版本不兼容 (v1 与 v2 无法互通)                        │
│                                                                     │
│   3. 缺省行为：                                                       │
│      - 未指定 Accept-Version 时，使用 URL 版本                      │
│      - 未指定 URL 版本时，默认跳转到最新稳定版                        │
│                                                                     │
└─────────────────────────────────────────────────────────────────────────────┘
```

#### 8.2.3 版本不匹配响应

当请求的版本不被支持时，返回 **406 Not Acceptable**：

```http
HTTP/1.1 406 Not Acceptable
Content-Type: application/json

{
  "error": {
    "code": "VERSION_NOT_SUPPORTED",
    "message": "API version v1.5 is not supported",
    "details": {
      "requestedVersion": "v1.5",
      "supportedVersions": ["v1", "v2"],
      "currentVersion": "v1",
      "upgradeGuide": "https://docs.yule-notion.com/api/version-migration"
    }
  }
}
```

**错误码添加：**

| 错误码 | HTTP 状态码 | 说明 |
|--------|------------|------|
| `VERSION_NOT_SUPPORTED` | 406 | 请求的 API 版本不被支持 |
| `VERSION_DEPRECATED` | 410 | 该版本已被弃用（见 Sunset 头部） |

### 8.3 API 弃用机制 (Sunset)

当 API 或版本即将弃用时，服务端应通过 `Sunset` 头部通知客户端。

#### 8.3.1 Sunset 头部

基于 [RFC 8594](https://tools.ietf.org/html/rfc8594)

| 头部 | 格式 | 示例 |
|------|------|------|
| `Sunset` | HTTP-date | `Sunset: Sun, 01 Nov 2026 00:00:00 GMT` |
| `Deprecation` | 真/日期 | `Deprecation: true` 或 `Deprecation: Sun, 01 Jun 2026 00:00:00 GMT` |

**响应示例：**
```http
HTTP/1.1 200 OK
Sunset: Sun, 01 Nov 2026 00:00:00 GMT
Deprecation: true
Link: </api/v2/pages>; rel="successor-version"

{
  "data": { ... },
  "meta": {
    "version": "v1",
    "sunset": "2026-11-01T00:00:00.000Z",
    "migrationGuide": "https://docs.yule-notion.com/api/v1-to-v2"
  }
}
```

#### 8.3.2 弃用时间表范例

| 版本 | 状态 | 弃用通知日 | 最终日期 | 备注 |
|------|------|------------|----------|------|
| v0.9 (Beta) | 已下线 | 2026-02-01 | 2026-03-15 | MVP 内测版 |
| v1.0 | 稳定 | - | 暂定 | 当前版本 |
| v1.x | 计划中 | - | - | 小版本更新，保持兼容 |
| v2.0 | 规划中 | - | - | 大版本，将有破坏性变更 |

### 8.4 向后兼容性保证

#### 8.4.1 约定

- **主版本号（Major）变更**：可能导致破坏性变更
- **次版本号（Minor）变更**：向后兼容的新功能
- **修订版本号（Patch）变更**：向后兼容的 bug 修复

#### 8.4.2 不会变更的部分

以下内容在同一主版本内保持稳定：

| 项目 | 约定 |
|------|------|
| URL 路径结构 | 不会删除或更改现有端点路径 |
| 请求/响应字段名 | 不会重命名现有字段 |
| 错误码 | 不会移除已定义的错误码 |
| 认证机制 | JWT 格式保持兼容 |
| 核心数据结构 | Page/User/Tag 字段保持向后兼容 |

#### 8.4.3 可能的变更

以下变更在次版本中可能发生（保持向后兼容）：

| 变更类型 | 示例 | 影响 |
|----------|------|------|
| 新增字段 | Page 新增 `coverImage` | 旧客户端忽略未知字段即可 |
| 新增接口 | 新增 `/api/v1/templates` | 无影响 |
| 新增错误码 | 新增 `TEMPLATE_NOT_FOUND` | 客户端需处理未知错误码 |
| 可选参数 | 新增 `?includeArchived=true` | 默认行为不变 |

### 8.5 版本过渡流程

#### 8.5.1 新版本发布流程

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      新版本发布流程 (Timeline)                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   T-90天                                                          │
│     │  ┌─────────────────────────────────────────────┐        │
│     ▼  │ 发布新版本预告（Deprecation Notice）    │        │
│        │ • 公告新版本计划和时间表              │        │
│        │ • 发布迁移指南草案                  │        │
│        └─────────────────────────────────────────────┘        │
│                                                                     │
│   T-60天                                                          │
│     │  ┌─────────────────────────────────────────────┐        │
│     ▼  │ 新版本上线 + 旧版本标记弃用       │        │
│        │ • /api/v2/* 正式上线              │        │
│        │ • 对 /api/v1/* 添加 Sunset 头部    │        │
│        │ • 文档更新，客户端 SDK 发布       │        │
│        └─────────────────────────────────────────────┘        │
│                                                                     │
│   T-30天                                                          │
│     │  ┌─────────────────────────────────────────────┐        │
│     ▼  │ 升级提醒 + 客户端支持               │        │
│        │ • 向仍使用旧版本的客户端发送通知      │        │
│        │ • 发布自动迁移工具                  │        │
│        └─────────────────────────────────────────────┘        │
│                                                                     │
│   T-0                                                              │
│     │  ┌─────────────────────────────────────────────┐        │
│     ▼  │ 旧版本下线                        │        │
│        │ • /api/v1/* 返回 410 Gone          │        │
│        │ • 提供错误页面指引用户迁移        │        │
│        └─────────────────────────────────────────────┘        │
│                                                                     │
└─────────────────────────────────────────────────────────────────────────────┘
```

#### 8.5.2 版本支持状态图

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        版本支持状态时间线                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   2026        2026        2026        2027        2027              │
│   Q1          Q2          Q3          Q1          Q2                 │
│    |           |           |           |           |               │
│    ▼           ▼           ▼           ▼           ▼               │
│                                                                     │
│ v0.9 ───────────────────────────────────────────────────────────────────│
│ [Beta]      → 已下线                                                       │
│                                                                     │
│ v1.0 ───────────────────────────────────────────────────────────────────│
│ [稳定]      [稳定]     [稳定]     [稳定]     [稳定]           │
│                                                                     │
│ v1.1 ───────────────────────────────────────────────────────────────────│
│              → [计划]    [计划]     [计划]                        │
│                                                                     │
│ v2.0 ───────────────────────────────────────────────────────────────────│
│                          [规划]    [开发]     [预发布]          │
│                                                                     │
└─────────────────────────────────────────────────────────────────────────────┘

图例: 
  ━━━━  当前支持    ────  计划    →→→  已下线
```

### 8.6 客户端版本检测建议

#### 8.6.1 版本检查流程

```typescript
// 客户端启动时检查 API 版本
async function checkApiVersion() {
  try {
    const response = await fetch('/api/v1/version', {
      headers: { 'Accept-Version': 'v1' }
    });
    
    const data = await response.json();
    
    // 检查 Sunset 头部
    const sunset = response.headers.get('Sunset');
    const deprecation = response.headers.get('Deprecation');
    
    if (deprecation) {
      console.warn('警告: 当前 API 版本即将弃用');
      showUpgradeNotice(sunset);
    }
    
    // 检查版本匹配
    if (data.meta.version !== SUPPORTED_VERSION) {
      handleVersionMismatch(data.meta.version);
    }
  } catch (error) {
    // 处理 406 错误
    if (error.code === 'VERSION_NOT_SUPPORTED') {
      redirectToUpgradePage(error.details.upgradeGuide);
    }
  }
}
```

#### 8.6.2 版本兼容性标识

```typescript
// package.json
{
  "name": "yule-notion-client",
  "version": "1.2.3",
  "engines": {
    "node": ">=18.0.0"
  },
  "apiCompatibility": {
    "minVersion": "v1.0",
    "maxVersion": "v1.9",
    "recommendedVersion": "v1.1"
  }
}
```

---

## 九、文档索引

| 文档 | 路径 | 说明 |
|------|------|------|
| API 完整规范 | `api-spec/API-SPEC.md` | 详细接口定义 |
| 架构设计 | `arch/ARCHITECTURE.md` | 系统架构说明 |
| 产品需求 | `prd/PRD-v1.0.md` | 功能需求 |
| 数据库设计 | `db/ER-DIAGRAM.md` | ER 图 |
| 部署文档 | `docker/` | Docker 配置 |

---

_三端通信协议规范 v1.0 | 2026-05-10_
