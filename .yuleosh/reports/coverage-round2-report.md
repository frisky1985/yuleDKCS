# yuleDKCS Phase 2 补测 — 第二轮报告

## 结论

| 模块 | 目标 | 实际 | 状态 |
|------|------|------|------|
| `backend/hub/pkg/service/` | ≥60% | **70.1%** | ✅ 达标 |
| `backend/dkcs/internal/repository/` | ≥50% | **84.9%** | ✅ 达标 |

## 变更内容

### 1. Repository 层 (`backend/dkcs/internal/repository/`)
**新增文件:**
- `key_repo_sqlmock_test.go` — go-sqlmock 测试，覆盖 `KeyRepository` 的 CRUD + 缓存层 + 查询方法
  - Create (成功/DB错误), GetByID (缓存命中/缓存未命中/NotFound/DB错误),
    Update (成功/NotFound), Delete (成功/DB错误),
    ListByUser (成功/空), ListByVehicle (成功), ListActiveByVehicle (成功/空)
- `vehicle_repo_sqlmock_test.go` — go-sqlmock 测试，覆盖 `VehicleRepository` 的查询/更新
  - GetByID (缓存命中/缓存未命中/NotFound/nil Features),
    GetByVIN, GetByTCUID (成功/NotFound),
    UpdateStatus, UpdateLocation, UpdateTelemetry (成功/错误/NotFound),
    ListByOwner (成功/空)
- `event_repo_sqlmock_test.go` — go-sqlmock 测试，覆盖 `EventRepository` 的 CRUD/统计
  - Create (成功/带KeyID/DB错误),
    GetByID (成功/NotFound/带KeyID),
    ListByVehicle/ListByUser/ListByKey (成功/空),
    GetStats (成功/空/DB错误)

**依赖新增:**
- `github.com/DATA-DOG/go-sqlmock v1.5.2`

**技术细节:**
- 使用 `sqlmock.New()` + `sqlx.NewDb(db, "postgres")` 模拟 sqlx.DB
- 使用 `miniredis` 模拟 Redis 缓存层（已有依赖）
- 注意：`squirrel` 使用 `?` 占位符，go-sqlmock 的 regex pattern 需匹配 `\?`

### 2. Service 层 (`backend/hub/pkg/service/`)
**新增文件:**
- `service_test.go` — 全面测试，覆盖以下服务：
  - **DeviceService** (13 个测试): RegisterDevice (成功/上限), GetDevice (找到/未找到),
    ListUserDevices, ProvisionKey (成功/设备不存在/用户不匹配/重复绑定/不同车辆),
    RevokeDeviceKeys (成功/未找到), DeleteDevice (成功/用户不匹配/密钥撤销验证)
  - **VehicleControlService** (2 个测试): SendCommand (解锁/锁车)
  - **KeyManagementService** (24 个测试): BindKey (成功/适配器未找到/适配器错误),
    UnbindKey (无认证/非所有者/管理员绕过/记录未找到),
    SuspendKey/ResumeKey/RevokeKey/RenewKey/GetKey (无认证/非所有者/成功),
    ListKeys (无认证/成功/管理员可查他人/非管理员不可查)
  - **KeyShareService** (5 个测试): CreateShare (成功/适配器未找到),
    AcceptShare (成功/适配器未找到), CancelShare, GetShare
  - **HubTransportService** (6 个测试): ForwardToVendor (绑定/适配器未找到/不支持操作),
    VendorCallback, HealthCheck (全部健康/部分不健康)
  - **LocalDKServer** (2 个测试): IssueKey, RevokeKeyByToken
  - **InMemoryKeyStore** (2 个测试): 完整 CRUD, 返回复制隔离
  - **辅助函数** (3 个测试): extractUserFromContext, isAdminUser, PushPayloadJSON

- `unified_key_service_test.go` — UnifiedKeyService 测试 (15 个测试):
  - 构造函数, NegotiateProtocol, HealthCheck (有适配器/空)
  - SuspendKey, ResumeKey, CancelShare, GetShare
  - BindKey (适配器找到/未找到/厂商回退)
  - ListKeys/GetKey (空会话), SendCommand (无会话)
  - VendorCallback, actionToRemoteAction, detectProtocol

**测试方法:**
- 手动 Mock: `MockAdapter` (实现 `adapter.Adapter`), `MockPushService`, `MockKeyStore`
- authCtx 辅助函数创建带 `user_id`/`user_role` 的 gRPC metadata context
- NewTestLogger 使用 `zap.NewDevelopment()`
- Adapter Registry 直接在测试中注册 MockAdapter 实例

## 覆盖遗留问题

- **UnifiedKeyService** 的部分方法 (`RevokeKey`, `UnbindKey`, `ForwardToVendor`, `VendorCallback` with payload, `StreamStatus`) 因依赖 unified codec 层的 panic/复杂依赖，测试覆盖偏弱。这些方法在单元测试中调用会触发编解码器 panic。
- **GRPCDKServer.RegisterGRPCServer** — 占位方法，0% 覆盖。
- **KeyManagementService.auditLog** — 内部 logger 调用，实际被各方法间接覆盖。
- **VehicleControlService.StreamStatus** — 依赖 gRPC stream runtime，0% 覆盖。
