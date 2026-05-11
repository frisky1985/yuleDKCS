# 云端数据库定义说明

> **适用文档**: TRIPLE-END-COMMUNICATION-SPEC.md (第二端 ↔ 第三端)  
> **数据库**: PostgreSQL 15+  
> **连接驱动**: Knex.js Query Builder

---

## 一、数据库配置规范

| 配置项 | 规范值 | 说明 |
|--------|--------|------|
| 数据库 | PostgreSQL 15+ | 支持 JSONB、UUID、时区 |
| 连接池 | min: 2, max: 10 | 根据并发调整 |
| 超时 | acquire: 60s, create: 30s | 防止连接泄漏 |
| 编码 | UTF-8 | 支持 emoji、中文 |
| 时区 | UTC | 统一时区处理 |

---

## 二、核心数据表

### 2.1 用户表 (users)

用户账户管理，支持多用户隔离

| 字段 | 类型 | 约束 | 说明 |
|------|------|------|------|
| id | UUID | PK | 用户唯一标识 |
| email | VARCHAR(254) | UNIQUE, NOT NULL | 登录邮箱 |
| password_hash | VARCHAR(255) | NOT NULL | bcrypt 哈希 |
| name | VARCHAR(50) | NOT NULL | 用户昵称 |
| created_at | TIMESTAMPTZ | DEFAULT NOW() | 注册时间 |
| updated_at | TIMESTAMPTZ | DEFAULT NOW() | 最后更新 |

```sql
CREATE TABLE users (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    email VARCHAR(254) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    name VARCHAR(50) NOT NULL,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);
```

---

### 2.2 页面表 (pages) ⭐ 核心

支持树形结构、软删除、版本控制

| 字段 | 类型 | 约束 | 说明 |
|------|------|------|------|
| id | UUID | PK | 页面唯一标识 |
| user_id | UUID | FK → users | 所属用户 |
| title | VARCHAR(255) | DEFAULT '无标题' | 页面标题 |
| content | JSONB | DEFAULT '{}' | TipTap JSON 内容 |
| parent_id | UUID | FK → pages | 父页面ID (树形) |
| "order" | INTEGER | DEFAULT 0 | 同级排序 |
| icon | VARCHAR(10) | DEFAULT '📄' | 页面图标 |
| version | INTEGER | DEFAULT 1 | 乐观锁版本 |
| is_deleted | BOOLEAN | DEFAULT FALSE | 软删除标记 |
| deleted_at | TIMESTAMPTZ | NULL | 删除时间 |
| created_at | TIMESTAMPTZ | DEFAULT NOW() | 创建时间 |
| updated_at | TIMESTAMPTZ | DEFAULT NOW() | 更新时间 |

```sql
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
```

**设计要点:**
- 树形结构: parent_id 自引用实现嵌套
- 软删除: is_deleted + deleted_at 支持回收站
- 版本控制: version 字段用于乐观锁
- JSONB: content 存储富文本，支持全文搜索

---

### 2.3 标签表 (tags)

页面分类标签

| 字段 | 类型 | 约束 | 说明 |
|------|------|------|------|
| id | UUID | PK | 标签ID |
| user_id | UUID | FK → users | 所属用户 |
| name | VARCHAR(30) | NOT NULL | 标签名称 |
| color | VARCHAR(7) | DEFAULT '#6b7280' | 标签颜色 |
| created_at | TIMESTAMPTZ | DEFAULT NOW() | 创建时间 |
| UNIQUE | (user_id, name) | | 用户内唯一 |

```sql
CREATE TABLE tags (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    name VARCHAR(30) NOT NULL,
    color VARCHAR(7) DEFAULT '#6b7280',
    created_at TIMESTAMPTZ DEFAULT NOW(),
    UNIQUE(user_id, name)
);
```

---

### 2.4 页面标签关联表 (page_tags)

多对多关系

| 字段 | 类型 | 约束 | 说明 |
|------|------|------|------|
| page_id | UUID | FK → pages | 页面ID |
| tag_id | UUID | FK → tags | 标签ID |
| PK | (page_id, tag_id) | | 联合主键 |

```sql
CREATE TABLE page_tags (
    page_id UUID REFERENCES pages(id) ON DELETE CASCADE,
    tag_id UUID REFERENCES tags(id) ON DELETE CASCADE,
    PRIMARY KEY (page_id, tag_id)
);
```

---

### 2.5 文件上传表 (uploads)

文件元数据管理

| 字段 | 类型 | 约束 | 说明 |
|------|------|------|------|
| id | UUID | PK | 文件ID |
| user_id | UUID | FK → users | 上传用户 |
| filename | VARCHAR(255) | NOT NULL | 原始文件名 |
| path | VARCHAR(500) | NOT NULL | 存储路径 |
| mime_type | VARCHAR(100) | NOT NULL | 文件类型 |
| size | INTEGER | NOT NULL | 文件大小(字节) |
| created_at | TIMESTAMPTZ | DEFAULT NOW() | 上传时间 |

```sql
CREATE TABLE uploads (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    filename VARCHAR(255) NOT NULL,
    path VARCHAR(500) NOT NULL,
    mime_type VARCHAR(100) NOT NULL,
    size INTEGER NOT NULL,
    created_at TIMESTAMPTZ DEFAULT NOW()
);
```

---

### 2.6 同步日志表 (sync_log) ⭐ 关键

增量同步的核心，记录所有数据变更

| 字段 | 类型 | 约束 | 说明 |
|------|------|------|------|
| id | UUID | PK | 日志ID |
| user_id | UUID | FK → users | 所属用户 |
| page_id | UUID | FK → pages | 变更页面 |
| operation | VARCHAR(10) | NOT NULL | CREATE/UPDATE/DELETE |
| server_version | INTEGER | NOT NULL | 服务端版本号 |
| synced_at | TIMESTAMPTZ | DEFAULT NOW() | 同步时间 |

```sql
CREATE TABLE sync_log (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    page_id UUID REFERENCES pages(id) ON DELETE CASCADE,
    operation VARCHAR(10) NOT NULL,
    server_version INTEGER NOT NULL,
    synced_at TIMESTAMPTZ DEFAULT NOW()
);
```

**作用:**
- 记录所有数据变更操作
- 支持增量同步 (客户端获取 `synced_at > last_sync` 的记录)
- 服务端版本号用于冲突检测

---

## 三、索引设计

### 3.1 全文搜索索引

```sql
-- 标题+内容全文搜索
CREATE INDEX idx_pages_fulltext ON pages 
USING GIN (to_tsvector('simple', title || ' ' || COALESCE(content->>'text', '')));
```

### 3.2 同步查询优化

```sql
-- 用户更新时间索引 (增量同步)
CREATE INDEX idx_pages_updated_at ON pages(user_id, updated_at);

-- 树形查询索引
CREATE INDEX idx_pages_parent ON pages(user_id, parent_id) WHERE is_deleted = FALSE;
```

---

## 四、事务级别规范

| 场景 | 事务级别 | 原因 |
|------|----------|------|
| 单条查询 | 无 | 普通 SELECT |
| 同步变更处理 | READ COMMITTED | 批量写入 + 冲突检测 |
| 页面移动(含子页面) | SERIALIZABLE | 防止树形结构循环引用 |
| 删除页面(级联软删除) | READ COMMITTED | 递归更新 is_deleted |

---

## 五、典型查询模式

### 5.1 树形页面查询

```sql
-- 获取根页面列表
SELECT * FROM pages 
WHERE user_id = ? AND parent_id IS NULL AND is_deleted = FALSE 
ORDER BY "order" ASC;

-- 获取子页面
SELECT * FROM pages 
WHERE parent_id = ? AND is_deleted = FALSE 
ORDER BY "order" ASC;
```

### 5.2 全文搜索

```sql
-- 高亮搜索结果
SELECT 
    id, title, icon, updated_at,
    ts_rank(to_tsvector('simple', title || ' ' || COALESCE(content->>'text', '')), 
            plainto_tsquery('simple', ?)) AS rank,
    ts_headline('simple', title, plainto_tsquery('simple', ?)) AS title_highlight,
    ts_headline('simple', COALESCE(content->>'text', ''), plainto_tsquery('simple', ?)) AS snippet
FROM pages
WHERE user_id = ? AND is_deleted = FALSE
    AND to_tsvector('simple', title || ' ' || COALESCE(content->>'text', '')) 
        @@ plainto_tsquery('simple', ?)
ORDER BY rank DESC, updated_at DESC
LIMIT ? OFFSET ?;
```

### 5.3 增量同步查询

```sql
-- 获取指定时间后的变更
SELECT * FROM pages
WHERE user_id = ? AND updated_at > ? AND is_deleted = FALSE
ORDER BY updated_at ASC
LIMIT ?;

-- 获取同步日志 (用于确认变更)
SELECT * FROM sync_log
WHERE user_id = ? AND synced_at > ?
ORDER BY server_version ASC;
```

---

## 六、数据关系图

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│     users       │     │     pages       │     │      tags       │
├─────────────────┤     ├─────────────────┤     ├─────────────────┤
│ id (PK)         │◄────┤ user_id (FK)    │     │ id (PK)         │
│ email           │     │ parent_id (FK)  │──┐  │ user_id (FK)    │
│ password_hash   │     │ id (PK)         │  │  │ name            │
│ name            │     │ title           │  │  │ color           │
│ created_at      │     │ content (JSONB) │  │  └─────────────────┘
│ updated_at      │     │ version         │  │            │
└─────────────────┘     │ is_deleted      │  │            │
                        │ created_at      │  │            ▼
                        │ updated_at      │  │  ┌─────────────────┐
                        └─────────────────┘  │  │   page_tags     │
                               │              │  ├─────────────────┤
                               │              └──┤ page_id (FK)    │
                               │                 │ tag_id (FK)     │
                               ▼                 │ PK(page,tag)    │
                        ┌─────────────────┐     └─────────────────┘
                        │   sync_log      │
                        ├─────────────────┤
                        │ user_id (FK)    │
                        │ page_id (FK)    │
                        │ operation       │
                        │ server_version  │
                        │ synced_at       │
                        └─────────────────┘

┌─────────────────┐
│    uploads      │
├─────────────────┤
│ id (PK)         │
│ user_id (FK)    │
│ filename        │
│ path            │
│ mime_type       │
│ size            │
└─────────────────┘
```

---

## 七、与三端通信的关联

| 数据表 | 第一端 (Frontend) | 第二端 (Backend) | 第三端 (Embedded) |
|--------|-------------------|------------------|-------------------|
| users | 登录认证 | JWT 验证 | 无直接访问 |
| pages | IndexedDB 同步 | REST/gRPC 提供 | 遥测数据存储 |
| tags | 本地标签管理 | API 增删改查 | 无 |
| sync_log | 同步状态查询 | 变更推送 | 无 |
| uploads | 文件上传/预览 | S3 代理 | OTA 包存储 |

---

*文档版本: v1.0.0*  
*关联文档: TRIPLE-END-COMMUNICATION-SPEC.md (第3章)*
