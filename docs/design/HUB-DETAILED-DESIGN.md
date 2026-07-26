# Digital Key Hub 模块级详细设计

> **版本**: v1.0 | **日期**: 2026-07-27
> **基于**: DK-HUB-ARCHITECTURE.md, SYSTEM_ARCHITECTURE.md §3.1.1, CLOUD-DEV-GUIDE.md
> **技术栈**: Go 1.22+ / gRPC / BERTLV / Redis / Kafka

---

## 1. Hub 模块架构总览

### 1.1 架构分层

```
┌─────────────────────────────────────────────────────────────────┐
│  API Gateway — 统一入口层                                       │
│  ┌─────────────────────┐  ┌────────────────────────────────┐   │
│  │  REST Gateway       │  │  gRPC Gateway                  │   │
│  │  (HTTPS/TLS 1.3)    │  │  (内部微服务通信)               │   │
│  │  JWT鉴权/限流/熔断   │  │  mTLS双向认证                   │   │
│  └─────────┬───────────┘  └──────────────┬────────────────┘   │
├────────────┼──────────────────────────────┼────────────────────┤
│            ▼                              ▼                     │
│  Token Manager（认证授权层）                                    │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  Access Token签发/验证/刷新  │  Refresh Token轮换       │   │
│  │  Token吊销/黑名单            │  JWT Claims管理          │   │
│  └─────────────────────────────────────────────────────────┘   │
├────────────────────────────────────────────────────────────────┤
│  Service Layer（业务服务层）                                     │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌────────────────┐   │
│  │KeyService│ │DeviceSvc │ │Vehicle  │ │KeyShareSvc    │   │
│  │编排管理   │ │设备管理   │ │控制服务  │ │分享服务        │   │
│  └──────────┘ └──────────┘ └──────────┘ └────────────────┘   │
│  ┌──────────────────────────────────────────────────────┐    │
│  │  HubTransport — DKCS 通信通道层                       │    │
│  │  gRPC双向流 + BERTLV 编解码 + 消息签名/验签           │    │
│  └──────────────────────────────────────────────────────┘    │
├────────────────────────────────────────────────────────────────┤
│  Protocol Adapter（协议适配层）                                 │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌────────────────┐   │
│  │CCC适配器  │ │ICCOA适配 │ │ICCE适配  │ │  Unified       │   │
│  │(Apple/   │ │(小米/    │ │(其他厂商) │ │  协议转换引擎   │   │
│  │Samsung)  │ │OPPO/vivo)│ │          │ │  (BERTLV核心)   │   │
│  └──────────┘ └──────────┘ └──────────┘ └────────────────┘   │
├────────────────────────────────────────────────────────────────┤
│  Logger（审计日志层）                                           │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  结构化审计日志  │  异步Kafka写入  │  查询检索API       │   │
│  └─────────────────────────────────────────────────────────┘   │
├────────────────────────────────────────────────────────────────┤
│  Infrastructure（基础设施层）                                   │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌────────────────┐   │
│  │Redis     │ │Kafka     │ │TiDB      │ │  Telemetry/    │   │
│  │缓存/锁   │ │事件队列   │ │持久存储   │ │  Metrics       │   │
│  └──────────┘ └──────────┘ └──────────┘ └────────────────┘   │
└────────────────────────────────────────────────────────────────┘
```

### 1.2 模块依赖关系

| 模块 | 依赖 | 被依赖 |
|------|------|--------|
| Token Manager | Redis(缓存), KMS(签名密钥) | REST Gateway, gRPC Gateway |
| REST Gateway | Token Manager, Service Layer | 手机App/SDK |
| Service Layer | Protocol Adapter, Logger, TiDB | REST Gateway |
| Protocol Adapter | Unified Protocol(BERTLV) | Service Layer |
| Logger | Kafka | Service Layer, Gateway |
| Unified Protocol | — | Protocol Adapter, HubTransport |

### 1.3 目录结构（现有）

```
backend/cloud/hub/
├── internal/
│   ├── gateway/          # REST API 网关 + Token 处理
│   │   ├── rest_gateway.go          # HTTP Handler
│   │   ├── token_handler.go         # Token 签发/验证
│   │   ├── device_handlers.go       # 设备相关端点
│   │   ├── rest_gateway_test.go
│   │   ├── token_handler_test.go
│   │   └── device_handlers_test.go
│   ├── token/            # Token Manager 核心
│   │   ├── token.go       # JWT 签发/验证/刷新
│   │   └── token_test.go
│   ├── service/          # 业务服务层
│   │   ├── key_management.go        # 密钥编排服务
│   │   ├── key_share.go             # 钥匙分享服务
│   │   ├── vehicle_control.go       # 车辆控制服务
│   │   ├── device_service.go        # 设备管理服务
│   │   ├── unified_key_service.go   # 统一钥匙编排
│   │   ├── hub_transport.go         # Hub↔DKCS 通信
│   │   └── dk_server.go             # DK Server 代理
│   ├── unified/         # Unified Protocol 核心
│   │   ├── protocol.go   # 协议定义
│   │   ├── router.go     # 协议路由
│   │   ├── manager.go    # 统一协议管理器
│   │   ├── device.go     # 设备协议能力管理
│   │   ├── state.go      # 会话状态
│   │   ├── codec.go      # BERTLV 编解码
│   │   └── *_test.go     # 单元测试
│   ├── codec/bertlv/     # BERTLV 编码器
│   │   └── tags.go       # Tag 定义
│   ├── logger/           # 审计日志
│   │   └── logger.go
│   ├── telemetry/        # 监控指标
│   │   └── telemetry.go
│   └── error/            # 错误码
│       └── error.go
├── tests/
│   ├── integration/      # 集成测试
│   ├── compliance/       # 协议合规测试
│   └── stress/           # 压力测试
└── docs/                 # Hub 设计文档
```

---

## 2. Token Manager

### 2.1 职责与边界

Token Manager 是 Hub 的认证授权核心，负责：
- **Access Token 签发**：用户认证成功后签发改写 JWT
- **Token 验证**：拦截所有 API 请求，验证 JWT 签名和有效期
- **Token 刷新**：基于 Refresh Token 安全轮换
- **Token 吊销**：支持主动吊销（黑名单机制）
- **不涉及**：用户密码/验证码验证（由 IAM 服务处理）

### 2.2 核心接口

```go
// internal/token/token.go

type TokenManager interface {
    // IssueAccessToken 签发访问令牌
    IssueAccessToken(ctx context.Context, user *UserClaims) (*Token, error)

    // IssueRefreshToken 签发刷新令牌（长期，7天）
    IssueRefreshToken(ctx context.Context, user *UserClaims) (*Token, error)

    // ValidateAccessToken 验证访问令牌
    // 返回解析后的 Claims；若无效返回 error
    ValidateAccessToken(ctx context.Context, tokenString string) (*UserClaims, error)

    // RefreshToken 刷新令牌对
    // 接收 Refresh Token，返回新的 Access Token + Refresh Token
    RefreshToken(ctx context.Context, refreshToken string) (*TokenPair, error)

    // RevokeToken 吊销令牌（加入黑名单）
    RevokeToken(ctx context.Context, tokenID string) error

    // IsTokenRevoked 检查令牌是否已被吊销
    IsTokenRevoked(ctx context.Context, tokenID string) (bool, error)
}

type Token struct {
    TokenString string    `json:"token"`
    TokenID     string    `json:"token_id"`
    ExpiresAt   time.Time `json:"expires_at"`
    TokenType   string    `json:"token_type"` // "access" | "refresh"
}

type TokenPair struct {
    AccessToken  *Token `json:"access_token"`
    RefreshToken *Token `json:"refresh_token"`
}

type UserClaims struct {
    UserID    string   `json:"user_id"`
    Phone     string   `json:"phone"`
    Roles     []string `json:"roles"`
    DeviceID  string   `json:"device_id"`
    Vendor    string   `json:"vendor"`
    TokenID   string   `json:"jti"`
    TokenType string   `json:"token_type"`
    IssuedAt  int64    `json:"iat"`
    ExpiresAt int64    `json:"exp"`
}
```

### 2.3 数据结构

```go
// Redis 存储结构
type TokenBlacklist struct {
    TokenID    string    `redis:"token_id"`
    RevokedAt  time.Time `redis:"revoked_at"`
    Reason     string    `redis:"reason"`
    TTL        int64     `redis:"ttl"` // 自动过期（与原 Token 过期时间一致）
}

// Redis Key 设计
// 黑名单:   "blacklist:token:{token_id}" → TTL = token剩余有效期
// 用户Token: "user:tokens:{user_id}"     → Set of token_ids
```

### 2.4 Token 生命周期

```
签发流程:
User认证 → IssueAccessToken → 签名(JWT RS256) → 返回Token → 存入Redis(用户映射)

验证流程:
API请求 → Bearer Token → ValidateAccessToken → 解码 → 验签 → 查黑名单 → 通过

刷新流程:
Refresh Token → 验证 → 作废旧Token → 签发新Token Pair → 返回

吊销流程:
管理员/用户操作 → RevokeToken → 写入Redis黑名单 → 通知客户端(N/A)
```

### 2.5 错误处理策略

| 错误场景 | 错误码 | 行为 |
|---------|--------|------|
| Token 过期 | 401 Unauthorized | 返回 `token_expired`，客户端应刷新 |
| Token 签名无效 | 401 Unauthorized | 返回 `invalid_signature`，需重新登录 |
| Token 已吊销 | 401 Unauthorized | 返回 `token_revoked`，需重新认证 |
| Refresh Token 过期 | 401 Unauthorized | 返回 `refresh_expired`，需重新登录 |
| 黑名单写入失败 | 500 Internal | 记录告警，不影响主流程 |

### 2.6 现有实现映射

| 接口方法 | 实现文件 | 测试文件 |
|---------|---------|---------|
| IssueAccessToken | `internal/token/token.go` | `internal/token/token_test.go` |
| ValidateAccessToken | `internal/token/token.go` | `internal/token/token_test.go` |
| RefreshToken | `internal/token/token.go` | `internal/token/token_test.go` |
| RevokeToken | `internal/token/token.go` | `internal/token/token_test.go` |
| Token HTTP Handler | `internal/gateway/token_handler.go` | `internal/gateway/token_handler_test.go` |

---

## 3. Protocol Adapter

### 3.1 职责与边界

Protocol Adapter 是 Hub 的多协议转换核心，负责：
- **协议转换**：将 CCC/ICCOA/ICCE 三种手机厂商协议统一为内部 BERTLV 协议
- **厂商路由**：根据 vendor+protocol 找到对应适配器
- **能力协商**：设备注册时协商支持的协议
- **适配器注册**：动态注册/注销适配器实例
- **不涉及**：密钥材料生成、车端通信（由 DKCS/TCU 处理）

### 3.2 核心接口

```go
// internal/service/dk_server.go — TspAdapter 定义

type TspAdapter interface {
    // GetVendor 返回适配器对应的厂商名
    GetVendor() string

    // GetProtocols 返回适配器支持的协议列表
    GetProtocols() []string

    // ForwardBind 转发绑定请求到厂商
    ForwardBind(ctx context.Context, req *BindRequest) (*BindResponse, error)

    // ForwardUnbind 转发解绑请求
    ForwardUnbind(ctx context.Context, req *UnbindRequest) (*UnbindResponse, error)

    // ForwardCommand 转发车控指令
    ForwardCommand(ctx context.Context, req *CommandRequest) (*CommandResponse, error)

    // ForwardShare 转发分享请求
    ForwardShare(ctx context.Context, req *ShareRequest) (*ShareResponse, error)
}

// 适配器注册中心
type AdapterRegistry interface {
    // Register 注册适配器（key 自动 lowercase）
    Register(ctx context.Context, vendor string, protocol string, adapter TspAdapter) error

    // Get 获取适配器（查询自动 lowercase）
    Get(ctx context.Context, vendor string, protocol string) (TspAdapter, error)

    // List 列出所有注册的适配器
    List(ctx context.Context) ([]*AdapterEntry, error)

    // Remove 移除适配器
    Remove(ctx context.Context, vendor string, protocol string) error
}

type AdapterEntry struct {
    Vendor   string `json:"vendor"`
    Protocol string `json:"protocol"`
    Adapter  TspAdapter `json:"-"`
}
```

### 3.3 数据结构

```go
// 适配器注册表（内存 + Redis 备份）
type RegistryStore struct {
    // vendor:protocol → TspAdapter
    adapters sync.Map

    // vendor → []protocol （用于快速枚举）
    vendorProtocols map[string][]string
}

// BindRequest 绑定请求（统一内部格式）
type BindRequest struct {
    UserID      string   `bertlv:"A0 04"`
    DeviceID    string   `bertlv:"A0 03"`
    VehicleID   string   `bertlv:"A0 01"`
    VIN         string   `bertlv:"A0 02"`
    Vendor      string   `bertlv:"A0 05"`
    Protocol    string   `bertlv:"A0 06"`
    KeyType     int      `bertlv:"A0 07"`     // 1=Owner, 2=Friend, 3=Service, 4=Temporary
    AccessLevel uint16   `bertlv:"A0 08"`     // 位掩码
    DevicePubKey []byte  `bertlv:"A0 09"`     // 设备公钥
    DeviceCert   []byte  `bertlv:"A0 0A"`     // 设备证书链
    ValidFrom   string   `bertlv:"A0 0B"`     // N14
    ValidUntil  string   `bertlv:"A0 0C"`     // N14
    MaxUses     int      `bertlv:"A0 0D"`     // 最大使用次数
}
```

### 3.4 适配器路由流程

```
手机请求 → REST Gateway → 协议识别 → AdapterRegistry.Get(vendor, protocol)
                                                      │
                                                      ▼
                                              ┌─────────────────┐
                                              │  CCC Adapter    │
                                              │  (Apple/Samsung)│
                                              │                 │
                                              │  ICCOA Adapter  │
                                              │  (小米/OPPO/vivo)│
                                              │                 │
                                              │  ICCE Adapter   │
                                              │  (其他厂商)      │
                                              └────────┬────────┘
                                                       │
                                                       ▼
                                              HubTransport → DKCS (gRPC+BERTLV)
```

### 3.5 错误处理策略

| 错误场景 | 错误码 | 行为 |
|---------|--------|------|
| 适配器未注册 | 4001 (VendorAdapterNotFound) | 返回错误给客户端 |
| 厂商 API 超时 | 4003 (VendorTimeout) | 重试1次后返回超时 |
| 厂商 API 错误 | 4002 (VendorApiError) | 透传厂商错误信息 |
| 协议不匹配 | 1001 (InvalidRequest) | 提示支持的协议列表 |
| Registry 大小写不匹配 | — | 自动 lowercase 后重试 |

### 3.6 已知修复

- **KNI-001**: Registry.Get/Register 自动 lowercase 输入
- **KNI-002**: 确保 strings.ToLower() 被实际调用
- **KNI-003**: 大小写规范化不破坏现有小写匹配行为

---

## 4. Service Layer

### 4.1 职责与边界

Service Layer 是 Hub 的业务编排层，负责：
- **钥匙编排**：创建/绑定/分享/吊销/挂起钥匙的流程编排
- **车辆控制**：远程车控指令的权限校验与转发
- **设备管理**：设备注册/能力上报/多设备管理
- **DK Server 代理**：与车厂 DK Server 的 gRPC 通信
- **不涉及**：密钥材料存储（全部委托给 DKCS/KMS）

### 4.2 核心服务接口

#### KeyManagementService

```go
// internal/service/key_management.go

type KeyManagementService interface {
    // CreateKey 创建钥匙（编排 DKM 指令 + 通知适配器）
    CreateKey(ctx context.Context, req *CreateKeyRequest) (*CreateKeyResponse, error)

    // ActivateKey 激活钥匙（配对完成后）
    ActivateKey(ctx context.Context, req *ActivateKeyRequest) error

    // RevokeKey 吊销钥匙
    RevokeKey(ctx context.Context, req *RevokeKeyRequest) error

    // SuspendKey 挂起钥匙
    SuspendKey(ctx context.Context, req *SuspendKeyRequest) error

    // ResumeKey 恢复钥匙
    ResumeKey(ctx context.Context, req *ResumeKeyRequest) error

    // ListKeys 查询钥匙列表（按用户/车辆/状态）
    ListKeys(ctx context.Context, req *ListKeysRequest) (*ListKeysResponse, error)

    // GetKeyDetail 获取钥匙详情
    GetKeyDetail(ctx context.Context, keyID string) (*KeyDetail, error)
}

type CreateKeyRequest struct {
    UserID      string
    VehicleID   string
    KeyType     KeyType       // Owner / Friend / Service / Temporary
    Protocol    Protocol      // CCC / ICCOA / ICCE
    Vendor      string
    AccessLevel uint16        // 权限位掩码
    ValidFrom   time.Time
    ValidUntil  *time.Time
    MaxUses     int
    DeviceID    string
}
```

#### DeviceService

```go
// internal/service/device_service.go

type DeviceService interface {
    // RegisterDevice 注册设备（含能力上报）
    RegisterDevice(ctx context.Context, req *RegisterDeviceRequest) (*Device, error)

    // GetDevice 获取设备信息
    GetDevice(ctx context.Context, deviceID string) (*Device, error)

    // ListDevices 列出用户所有设备
    ListDevices(ctx context.Context, userID string) ([]*Device, error)

    // RemoveDevice 移除设备（吊销该设备所有钥匙）
    RemoveDevice(ctx context.Context, deviceID string, userID string) error

    // UpdateDeviceCapabilities 更新设备能力
    UpdateDeviceCapabilities(ctx context.Context, deviceID string, caps *DeviceCapabilities) error
}
```

#### VehicleControlService

```go
// internal/service/vehicle_control.go

type VehicleControlService interface {
    // SendCommand 发送车控指令
    SendCommand(ctx context.Context, req *CommandRequest) (*CommandResponse, error)

    // GetVehicleStatus 获取车辆实时状态
    GetVehicleStatus(ctx context.Context, vehicleID string) (*VehicleStatus, error)

    // GetCommandHistory 查询指令执行历史
    GetCommandHistory(ctx context.Context, vehicleID string, page int, size int) ([]*CommandRecord, error)
}

// CommandRequest
type CommandRequest struct {
    UserID    string
    KeyID     string
    VehicleID string
    Action    CommandAction    // Unlock/Lock/EngineStart/...
    Source    CommandSource    // NFC/BLE/UWB/Remote/Edge
    Params    map[string]interface{}
    TimeoutMs int
}
```

#### KeyShareService

```go
// internal/service/key_share.go

type KeyShareService interface {
    // CreateShare 创建钥匙分享
    CreateShare(ctx context.Context, req *CreateShareRequest) (*Share, error)

    // AcceptShare 接受分享
    AcceptShare(ctx context.Context, shareCode string, userID string) (*AcceptShareResponse, error)

    // RevokeShare 撤销分享
    RevokeShare(ctx context.Context, shareID string, ownerID string) error

    // ListShares 列出分享记录
    ListShares(ctx context.Context, userID string, role ShareRole) ([]*Share, error)

    // GetShareDetail 获取分享详情
    GetShareDetail(ctx context.Context, shareID string) (*Share, error)
}

type CreateShareRequest struct {
    KeyID       string
    ToUserID    string
    ToVendor    string
    AccessLevel uint16
    ValidFrom   time.Time
    ValidUntil  time.Time
    MaxUses     int
    Message     string  // 分享附言
}
```

### 4.3 关键业务流

#### 钥匙绑定流程（Service 视角）

```
手机App → REST Gateway → Token验证 → KeyManagementService.CreateKey
  │
  ├─ 1. 校验用户权限（Owner 或管理员）
  ├─ 2. 校验车辆是否存在
  ├─ 3. 校验钥匙数量限制（≤5/车辆）
  ├─ 4. 构建 BindRequest (BERTLV)
  ├─ 5. HubTransport.SendToDKCS(ctx, bindReq) → gRPC
  ├─ 6. 接收 DKCS 响应
  ├─ 7. Logger.Log(audit entry)
  └─ 8. 返回结果给客户端
```

#### 钥匙分享流程（Service 视角）

```
车主App → REST Gateway → KeyShareService.CreateShare
  │
  ├─ 1. CreateShareRequest 权限校验
  ├─ 2. 校验分享目标是否存在
  ├─ 3. 检查分享限制（时间/次数/同级限制）
  ├─ 4. 写入分享记录 (TiDB)
  ├─ 5. HubTransport.SendToDKCS(ShareRequest)
  ├─ 6. 生成分享码/URL
  ├─ 7. Logger.Log(audit entry)
  └─ 8. 返回分享信息
```

### 4.4 错误处理策略

| 错误场景 | HTTP 状态 | 业务码 | 说明 |
|---------|----------|--------|------|
| 钥匙已达上限 | 409 Conflict | 2006 | 单车辆最多5把钥匙 |
| 设备不存在 | 404 Not Found | — | 注册时未上报能力 |
| 车辆离线 | 503 Service Unavailable | 3002 | 控制指令无法下发 |
| 权限不足 | 403 Forbidden | 2007 | 非车主尝试分享/吊销 |
| 分享超限 | 409 Conflict | — | 分享数量/时间超限 |

---

## 5. Logger（审计日志）

### 5.1 职责与边界

审计日志模块负责全生命周期操作记录，满足合规要求（保留 ≥ 3 年）。

### 5.2 核心接口

```go
// internal/logger/logger.go

type AuditLogger interface {
    // Log 记录审计日志（异步，写入 Kafka）
    Log(ctx context.Context, entry *AuditEntry)

    // LogSync 同步记录审计日志（关键操作）
    LogSync(ctx context.Context, entry *AuditEntry) error

    // Query 查询审计日志
    Query(ctx context.Context, req *AuditQuery) (*AuditResult, error)

    // Export 导出审计日志
    Export(ctx context.Context, req *AuditExportRequest) (string, error)
}

type AuditEntry struct {
    EventID     string                 `json:"event_id"`
    Timestamp   int64                  `json:"timestamp"`
    UserID      string                 `json:"user_id"`
    DeviceID    string                 `json:"device_id"`
    Action      string                 `json:"action"`       // key.bind / key.revoke / share.create / ...
    Resource    string                 `json:"resource"`     // key_id / vehicle_id
    Result      string                 `json:"result"`       // success / failure
    Reason      string                 `json:"reason,omitempty"`
    RequestIP   string                 `json:"request_ip"`
    UserAgent   string                 `json:"user_agent"`
    Extra       map[string]interface{} `json:"extra,omitempty"`
}

type AuditQuery struct {
    UserID    string   `json:"user_id,omitempty"`
    Action    string   `json:"action,omitempty"`
    Resource  string   `json:"resource,omitempty"`
    StartTime int64    `json:"start_time"`
    EndTime   int64    `json:"end_time"`
    Page      int      `json:"page"`
    PageSize  int      `json:"page_size"`
    SortBy    string   `json:"sort_by"`     // timestamp / user_id / action
    SortOrder string   `json:"sort_order"`  // asc / desc
}
```

### 5.3 日志分类

| 类别 | Action 前缀 | 记录内容 | 同步/异步 |
|------|------------|---------|----------|
| 密钥操作 | `key.*` | 绑定/解绑/吊销/挂起/恢复 | 同步 |
| 分享操作 | `share.*` | 创建/接受/撤销分享 | 同步 |
| 车辆控制 | `vehicle.*` | 解锁/闭锁/启动/熄火/远程控车 | 异步 |
| 用户管理 | `user.*` | 注册/登录/注销/设备操作 | 异步 |
| 系统事件 | `system.*` | 配置变更/告警/服务启动停止 | 异步 |

### 5.4 数据流

```
Service Layer → AuditLogger.Log(entry)
  │
  ├─ 1. 构建 AuditEntry（含时间戳、请求ID）
  ├─ 2. 序列化为 JSON
  ├─ 3. 写入 Kafka Topic: "digitalkey.audit.log"
  ├─ 4. [同步路径] 等待确认（关键操作）
  │
  ▼
Kafka Consumer → ElasticSearch
  │
  ├─ 索引: audit-{YYYY.MM.dd}
  ├─ 生命周期: 3 年
  └─ 用于: 合规查询、安全审计、操作追踪
```

### 5.5 错误处理

| 场景 | 行为 |
|------|------|
| Kafka 不可用 | 同步路径降级为写入本地文件/Redis（buffer 模式） |
| ES 不可用 | Kafka 消息保留，ES 恢复后重放 |
| 序列化失败 | 记录 structured log 到 stdout，不影响业务 |
| 日志超量 | 采样策略：非关键操作 1:10 采样 |

---

## 6. Unified Protocol (BERTLV)

### 6.1 职责与边界

统一协议引擎是整个 Hub 与 DKCS 通信的协议基石，负责：
- **BERTLV 编解码**：所有内部消息的 Tag-Length-Value 编码
- **消息完整性**：Header + Body + Trailer 签名
- **消息路由**：根据 MessageType 分发到对应处理函数
- **会话管理**：维护配对/分享会话状态
- **不涉及**：协议的业务语义（由 Service Layer 处理）

### 6.2 核心数据结构

```go
// internal/unified/protocol.go

// 消息信封
type Message struct {
    Header  *Header  `bertlv:"E1 01"`
    Body    *Body    `bertlv:"variable"`
    Trailer *Trailer `bertlv:"E1 FF"`
}

// 消息头 (E1 01)
type Header struct {
    Version     string `bertlv:"E1 01"`     // BCD, "0100" = v1.0
    Timestamp   string `bertlv:"E1 02"`     // N14, YYYYMMDDhhmmss
    MessageType int    `bertlv:"E1 03"`     // N4, 消息类型码
    SequenceNo  int    `bertlv:"E1 04"`     // N8, 序列号
    DeviceID    string `bertlv:"E1 05"`     // AN16, 设备ID
    SessionID   string `bertlv:"E1 06"`     // AN32, 会话ID(可选)
    Priority    int    `bertlv:"E1 07"`     // N1, 1=低 2=中 3=高 4=紧急
    Flags       byte   `bertlv:"E1 08"`     // B1, 标志位
    CorrelationID string `bertlv:"E1 09"`   // AN32, 关联消息ID
}

// 消息尾 (E1 FF)
type Trailer struct {
    Signature    []byte `bertlv:"E1 FF 01"`  // HMAC-SHA256
    MacKeyID     string `bertlv:"E1 FF 02"`  // MAC密钥ID
}
```

### 6.3 消息编解码

```go
// internal/unified/codec.go

type Codec interface {
    // Encode 将 Message 编码为 BERTLV 字节流
    Encode(msg *Message) ([]byte, error)

    // Decode 将 BERTLV 字节流解码为 Message
    Decode(data []byte) (*Message, error)

    // EncodeBody 编码消息体（按指定 Tag 字典）
    EncodeBody(tagDict map[int]TagDef, body interface{}) ([]byte, error)

    // DecodeBody 解码消息体
    DecodeBody(data []byte, tagDict map[int]TagDef) (interface{}, error)
}

// Tag 定义
type TagDef struct {
    Tag      int    // 标签号
    Name     string // 字段名
    Format   string // 格式: N, AN, B, BCD, 结构
    Required bool   // 是否必填
    MaxLen   int    // 最大长度
}
```

### 6.4 消息类型注册

```go
// internal/unified/router.go

type MessageRouter struct {
    handlers map[int]MessageHandler  // MessageType → handler
}

type MessageHandler func(ctx context.Context, msg *Message) (*Message, error)

// 已注册的消息类型：
// 1000 KeyBind        → KeyManagementService
// 1002 KeyUnbind      → KeyManagementService
// 1004 KeyRevoke      → KeyManagementService
// 1010 KeyList        → KeyManagementService
// 2000 KeyShareCreate → KeyShareService
// 2002 KeyShareAccept → KeyShareService
// 3000 VehicleCommand → VehicleControlService
// 3002 VehicleStatus  → 事件处理
// 9000 Heartbeat      → HubTransport
```

### 6.5 BERTLV Tag 定义（hub-dkcs-protocol.md §1.2 映射）

| Tag | 名称 | 位置 | 格式 |
|-----|------|------|------|
| E1 01 | Version | Header | BCD |
| E1 02 | Timestamp | Header | N14 |
| E1 03 | MessageType | Header | N4 |
| E1 04 | SequenceNo | Header | N8 |
| E1 05 | DeviceId | Header | AN16 |
| E1 06 | SessionId | Header | AN32 |
| E1 07 | Priority | Header | N1 |
| E1 08 | Flags | Header | B1 |
| E1 09 | CorrelationId | Header | AN32 |
| A0 01 | VehicleId | Body | AN32 |
| A0 02 | Vin | Body | AN17 |
| A0 04 | UserId | Body | AN32 |
| A0 05 | Vendor | Body | AN16 |
| A0 07 | KeyType | Body | N2 |
| A0 08 | AccessLevel | Body | B4 |
| A0 09 | DevicePubkey | Body | B64 |
| A0 0A | DeviceCert | Body | B512 |
| A0 0B | ValidFrom | Body | N14 |
| A0 0C | ValidUntil | Body | N14 |
| A0 0D | MaxUses | Body | N6 |
| E1 FF 01 | Signature | Trailer | B |
| E1 FF 02 | MacKeyId | Trailer | AN16 |

### 6.6 消息完整性保障

```
发送方:
  message = Header + Body
  signature = HMAC-SHA256(message, sessionKey)
  Trailer.Signature = signature
  wire = Header + Body + Trailer

接收方:
  verify = HMAC-SHA256(Header + Body, sessionKey)
  assert verify == Trailer.Signature
```

---

## 7. 关键数据流（时序图）

### 7.1 密钥绑定

```
┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐
│ 手机App   │    │ Hub Gate │    │ TokenMgr│    │ Service  │    │ DKCS     │
└────┬─────┘    └────┬─────┘    └────┬─────┘    └────┬─────┘    └────┬─────┘
     │               │               │               │               │
     │ 1. POST /bind │               │               │               │
     │   {token,     │               │               │               │
     │    vehicleId, │               │               │               │
     │    devicePubKey}              │               │               │
     │──────────────▶│               │               │               │
     │               │ 2. Validate   │               │               │
     │               │   AccessToken│               │               │
     │               │──────────────▶               │               │
     │               ◀──────────────│               │               │
     │               │   claims     │               │               │
     │               │               │               │               │
     │               │ 3. CreateKey  │               │               │
     │               │   (Service)   │               │               │
     │               │───────────────────────────────────────────────▶│
     │               │   BindRequest │               │               │
     │               │   {BERTLV}    │               │               │
     │               │               │               │               │
     │               │               │               │ 4. 校验+编排  │
     │               │               │               │  调用KMS生成  │
     │               │               │               │  密钥对(SE050)│
     │               │               │               │  返回公钥+证书│
     │               │               │               ◀───────────────│
     │               │   BindResp    │               │               │
     │               │◀───────────────────────────────────────────────│
     │               │               │               │               │
     │               │ 5. Logger.Log │               │               │
     │               │   (audit)     │               │               │
     │               │               │               │               │
     │ 6. 返回绑定   │               │               │               │
     │   结果+车端公钥│               │               │               │
     │◀──────────────│               │               │               │
     │               │               │               │               │
```

### 7.2 钥匙分享

```
┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐
│ 车主App   │    │ Hub Gate │    │ Service  │    │ DKCS     │    │被分享者  │
└────┬─────┘    └────┬─────┘    └────┬─────┘    └────┬─────┘    └────┬─────┘
     │               │               │               │               │
     │ 1. POST /share│               │               │               │
     │   {keyId,     │               │               │               │
     │    toUserId,  │               │               │               │
     │    accessLevel,               │               │               │
     │    validUntil}│               │               │               │
     │──────────────▶│               │               │               │
     │               │ 2. Validate   │               │               │
     │               │    权限校验   │               │               │
     │               │──────────────▶               │               │
     │               │               │               │               │
     │               │               │ 3. 保存分享    │               │
     │               │               │    记录(TiDB) │               │
     │               │               │               │               │
     │               │               │ 4. SendToDKCS │               │
     │               │               │   ShareCreate │               │
     │               │               │──────────────▶               │
     │               │               │               │ 5. 通知车端   │
     │               │               │               │    PKI签发    │
     │               │               ◀───────────────               │
     │               │               │               │               │
     │               │               │ 6. 生成分享码  │               │
     │               │               │    记录分享事件│               │
     │               │               │               │               │
     │ 7. 分享码     │               │               │               │
     │◀──────────────│               │               │               │
     │               │               │               │               │
     │               │               │ 8. 通知被分享者(推送)          │
     │               │               │─────────────────────────────────▶
     │               │               │               │               │
     │               │               │ 9. 被分享者    │               │
     │               │               │    接受分享    │               │
     │               │               │◀─────────────────────────────────
     │               │               │               │               │
```

### 7.3 生态授权

```
┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐
│ 车主App   │    │ Hub      │    │ 服务商    │    │ DKCS     │    │ KMS/OEM  │
└────┬─────┘    └────┬─────┘    └────┬─────┘    └────┬─────┘    └────┬─────┘
     │               │               │               │               │
     │ 1. 授权代驾   │               │               │               │
     │   (30分钟,    │               │               │               │
     │    仅解锁+启动)│               │               │               │
     │──────────────▶│               │               │               │
     │               │ 2. 记录授权   │               │               │
     │               │   {车主,代驾  │               │               │
     │               │    权限,有效期}│               │               │
     │               │               │               │               │
     │ 3. 授权成功   │               │               │               │
     │◀──────────────│               │               │               │
     │               │               │               │               │
     │               │ 4. 代驾请求钥匙              │               │
     │               │◀──────────────│               │               │
     │               │               │               │               │
     │               │ 5. 验证授权   │               │               │
     │               │    有效性     │               │               │
     │               │               │               │               │
     │               │ 6. 通知DKCS   │               │               │
     │               │   → 临时钥匙  │               │               │
     │               │──────────────────────────────────────────────▶│
     │               │               │               │ 7. 生成临时   │
     │               │               │               │    密钥对    │
     │               │               │               │    签发证书  │
     │               │               │               ◀───────────────│
     │               │ 8. 返回结果   │               │               │
     │               │◀───────────────────────────────────────────────│
     │               │               │               │               │
     │               │ 9. 临时钥匙就绪              │               │
     │               │─────────────────────────────▶│               │
     │               │               │               │               │
```

---

## 8. 配置管理

### 8.1 配置文件结构

```yaml
# configs/hub.yaml

server:
  http:
    port: 8080
    read_timeout: 30s
    write_timeout: 30s
    max_header_bytes: 1MB
  grpc:
    port: 9090
    max_recv_msg_size: 4MB
    max_send_msg_size: 4MB

auth:
  token:
    access_token_ttl: 1h
    refresh_token_ttl: 168h  # 7天
    signing_algorithm: RS256
    key_rotation_interval: 720h  # 30天
  rate_limit:
    enabled: true
    requests_per_second: 100
    burst: 200

database:
  tidb:
    dsn: "root:@tcp(localhost:4000)/digitalkey?charset=utf8mb4"
    max_open_conns: 100
    max_idle_conns: 20
    conn_max_lifetime: 30m

redis:
  addrs: ["localhost:6379"]
  password: ""
  db: 0
  pool_size: 50
  min_idle_conns: 10

kafka:
  brokers: ["localhost:9092"]
  topics:
    audit_log: "digitalkey.audit.log"
    key_event: "digitalkey.key.lifecycle"
  consumer_group: "hub-audit-consumer"

logger:
  level: info
  format: json
  audit:
    kafka_topic: "digitalkey.audit.log"
    retention_days: 1095  # 3年
    sync_actions:         # 同步日志的action列表
      - key.bind
      - key.revoke
      - share.create
      - user.device.remove

adapter:
  registry:
    type: memory  # memory / redis
    redis_prefix: "adapter:registry:"
  timeout:
    vendor_api: 10s
    grpc_call: 30s
```

### 8.2 配置来源优先级

1. 环境变量（最高优先级，用于敏感信息）
2. 配置文件（`configs/hub.yaml`）
3. Nacos 配置中心（分布式配置，动态刷新）
4. 默认值（代码中硬编码）

### 8.3 敏感配置

| 配置项 | 来源 | 处理方式 |
|--------|------|---------|
| JWT 签名私钥 | 环境变量 / Vault | 运行时加载，不落盘 |
| DB 密码 | 环境变量 / K8s Secret | Base64 环境变量 |
| Kafka SASL 凭证 | 环境变量 / Vault | 运行时注入 |
| Redis 密码 | 环境变量 / K8s Secret | Base64 环境变量 |

---

## 9. 性能指标

| 指标 | 目标 | 测量方式 |
|------|------|---------|
| Token 验证 P99 | ≤ 5ms | Prometheus histograms |
| 协议转换 P99 | ≤ 10ms | OpenTelemetry span |
| gRPC 转发 P99 | ≤ 50ms | gRPC metrics |
| 审计日志异步延迟 | ≤ 1s (P99) | Kafka lag monitor |
| 并发连接数 | ≥ 10,000 | Goroutine count |
| 可用性 | ≥ 99.9% | Uptime checks |

---

## 10. 安全设计

| 层面 | 措施 |
|------|------|
| 传输安全 | TLS 1.3 + mTLS 双向认证（gRPC） |
| 认证安全 | JWT RS256 + Refresh Token 轮换 |
| 消息安全 | HMAC-SHA256 签名（BERTLV Trailer） |
| 权限安全 | 16 位 AccessLevel 位掩码 |
| 审计安全 | 全操作审计，防篡改日志 |
| 限流熔断 | Token Bucket + 熔断器（gRPC） |
