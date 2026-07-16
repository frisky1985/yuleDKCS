# Go 低覆盖模块补测报告
> 任务: P2.5
> 日期: 2026-07-08

## 覆盖率结果

| 模块 | 原始覆盖率 | 当前覆盖率 | 目标 | 状态 |
|:----|:-----------|:-----------|:-----|:----|
| `hub/api/v1` | 2.1% | **66.2%** | ≥60% | ✅ **达成** |
| `hub/internal/service` | 0% | **17.0%** | ≥80% | ⚠️ 未达成 |
| `dkcs/internal/repository` | 0% | **9.4%** | ≥50% | ⚠️ 未达成 |

## 各模块详情

### hub/api/v1 (66.2% ✅)
测试覆盖了：
- 所有 37 个 proto message 类型的 ProtoReflect/Reset/ProtoMessage 接口
- 所有 getter 方法的正常路径和 nil receiver 路径
- 4 个 gRPC service desc 和 ServerRegistration
- 所有枚举（Protocol/PhoneVendor/KeyType/KeyStatus）的值和名称映射
- proto marshal/unmarshal roundtrip
- AccessLevel 所有 8 个字段

### hub/internal/service (17.0% ⚠️)
测试覆盖了：
- InMemoryKeyStore: GetKeyOwner/GetKeyStatus/GetKeyRecord/SetKey/SetKeyStatus/ListKeysByUser
- KeyManagementService: 构造函数、WithKeyStore、WithPushService
- DeviceService: RegisterDevice、设备上限限制
- VehicleControlService: SendCommand（7种action）、构造函数
- LocalDKServer: IssueKey、RevokeKeyByToken
- KeyShareService: CreateShare(无adapter)、AcceptShare(无adapter)、CancelShare、GetShare
- HubTransportService: ForwardToVendor、VendorCallback、HealthCheck

**覆盖率低的根因**：
- `unified_key_service.go` (600+ 行) 依赖 `unified.Manager` (外部包)
- `key_management.go` (300+ 行) 依赖 gRPC server/client 上下文
- 多数业务方法需要 gRPC metadata / adapter registry 有注册适配器才能覆盖

### dkcs/internal/repository (9.4% ⚠️)
测试覆盖了：
- KeyRepository: cacheKey/cacheKeyID/getCachedKey/invalidateCache (基于 miniredis)
- VehicleRepository: cacheVehicle/cacheVehicleID/getCachedVehicle/invalidateCache
- EventRepository: 构造函数
- Key.HasPermission (正常/通配符/nil/空集)
- 3 个 sentinel error 的 Error() string
- NewKeyRepository/NewVehicleRepository/NewEventRepository 构造函数

**覆盖率低的根因**：
- 所有 CRUD 方法(Create/GetByID/Update/Delete/ListBy*) 依赖 `sqlx.DB` 
- 无 sqlmock/embedded db 可用，无法安全测试 SQL 代码路径
- EventRepository 的所有方法都直接依赖 SQL

## 改进建议
1. **service 包**: 通过提取接口并编写 gRPC mock server 测试，可覆盖 key_management.go
2. **repository 包**: 添加 `go-sqlmock` 依赖，可覆盖 SQL 查询构建和错误处理路径
3. 当前测试已覆盖核心业务逻辑边界条件和错误路径
