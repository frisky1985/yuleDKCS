# ICCE协议栈测试用例

## 1. 服务模块 (Service) 测试用例

### 1.1 ICCE服务发现

| 用例ID | 用例名称 | 前置条件 | 测试步骤 | 预期结果 | 优先级 |
|--------|----------|----------|----------|----------|--------|
| ICCE_SVC_001 | 服务初始化 | 上电 | 调用ICCE_Init() | 初始化成功 | P0 |
| ICCE_SVC_002 | 发现服务 | 已初始化 | CallICCE_DiscoverServices() | 返回服务列表 | P0 |
| ICCE_SVC_003 | 服务注册 | 发现后 | CallICCE_RegisterService() | 注册成功 | P0 |
| ICCE_SVC_004 | 服务注销 | 已注册 | CallICCE_UnregisterService() | 注销成功 | P0 |

### 1.2 服务连接

| 用例ID | 用例名称 | 前置条件 | 测试步骤 | 预期结果 | 优先级 |
|--------|----------|----------|----------|----------|--------|
| ICCE_SVC_010 | 连接服务 | 已注册 | CallICCE_ConnectService() | 连接成功 | P0 |
| ICCE_SVC_011 | 断开服务 | 已连接 | CallICCE_DisconnectService() | 断开成功 | P0 |
| ICCE_SVC_012 | 服务心跳 | 已连接 | CallICCE_ServiceHeartbeat() | 心跳正常 | P1 |
| ICCE_SVC_013 | 服务重连 | 断开后 | CallICCE_ReconnectService() | 重连成功 | P1 |

---

## 2. ICCE核心测试用例

### 2.1 车辆交互

| 用例ID | 用例名称 | 前置条件 | 测试步骤 | 预期结果 | 优先级 |
|--------|----------|----------|----------|----------|--------|
| ICCE_CORE_001 | 解锁请求 | 已连接 | CallICCE_UnlockRequest() | 解锁响应 | P0 |
| ICCE_CORE_002 | 锁车请求 | 已连接 | CallICCE_LockRequest() | 锁车响应 | P0 |
| ICCE_CORE_003 | 启动请求 | 认证通过 | CallICCE_StartRequest() | 启动响应 | P0 |
| ICCE_CORE_004 | 停止请求 | 运行中 | CallICCE_StopRequest() | 停止响应 | P0 |
| ICCE_CORE_005 | 空调控制 | 已连接 | CallICCE_ACControl() | 控制响应 | P1 |

### 2.2 状态管理

| 用例ID | 用例名称 | 前置条件 | 测试步骤 | 预期结果 | 优先级 |
|--------|----------|----------|----------|----------|--------|
| ICCE_CORE_010 | 获取状态 | 无 | CallICCE_GetStatus() | 返回状态 | P0 |
| ICCE_CORE_011 | 获取电池 | 无 | CallICCE_GetBattery() | 返回电量 | P0 |
| ICCE_CORE_012 | 获取位置 | GPS可用 | CallICCE_GetLocation() | 返回位置 | P1 |
| ICCE_CORE_013 | 获取诊断 | 异常时 | CallICCE_GetDiagnosis() | 返回诊断 | P1 |

### 2.3 钥匙管理

| 用例ID | 用例名称 | 前置条件 | 测试步骤 | 预期结果 | 优先级 |
|--------|----------|----------|----------|----------|--------|
| ICCE_CORE_020 | 添加钥匙 | 未绑定 | CallICCE_AddKey() | 添加成功 | P0 |
| ICCE_CORE_021 | 删除钥匙 | 已绑定 | CallICCE_DeleteKey() | 删除成功 | P0 |
| ICCE_CORE_022 | 授权钥匙 | 已添加 | CallICCE_AuthorizeKey() | 授权成功 | P0 |
| ICCE_CORE_023 | 撤销钥匙 | 已授权 | CallICCE_RevokeKey() | 撤销成功 | P0 |

---

## 3. ICCE设备管理测试用例

### 3.1 设备注册

| 用例ID | 用例名称 | 前置条件 | 测试步骤 | 预期结果 | 优先级 |
|--------|----------|----------|----------|----------|--------|
| ICCE_DEV_001 | 注册设备 | 首次使用 | CallICCE_RegisterDevice() | 注册成功 | P0 |
| ICCE_DEV_002 | 绑定设备 | 已注册 | CallICCE_BindDevice() | 绑定成功 | P0 |
| ICCE_DEV_003 | 解绑设备 | 已绑定 | CallICCE_UnbindDevice() | 解绑成功 | P0 |
| ICCE_DEV_004 | 设备列表 | 有绑定 | CallICCE_ListDevices() | 返回列表 | P1 |

### 3.2 设备认证

| 用例ID | 用例名称 | 前置条件 | 测试步骤 | 预期结果 | 优先级 |
|--------|----------|----------|----------|----------|--------|
| ICCE_DEV_010 | 设备认证 | 已绑定 | CallICCE_AuthenticateDevice() | 认证成功 | P0 |
| ICCE_DEV_011 | 证书验证 | 证书有效 | CallICCE_VerifyCertificate() | 验证通过 | P0 |
| ICCE_DEV_012 | 密钥协商 | 认证后 | CallICCE_KeyExchange() | 协商成功 | P0 |
| ICCE_DEV_013 | 安全会话 | 密钥后 | CallICCE_EstablishSession() | 会话建立 | P0 |

---

## 4. 数据管理测试用例

### 4.1 数据上报

| 用例ID | 用例名称 | 前置条件 | 测试步骤 | 预期结果 | 优先级 |
|--------|----------|----------|----------|----------|--------|
| ICCE_DATA_001 | 上报状态 | 状态变化 | CallICCE_ReportStatus() | 上报成功 | P0 |
| ICCE_DATA_002 | 上报位置 | 位置变化 | CallICCE_ReportLocation() | 上报成功 | P0 |
| ICCE_DATA_003 | 上报事件 | 事件触发 | CallICCE_ReportEvent() | 上报成功 | P0 |
| ICCE_DATA_004 | 上报日志 | 异常发生 | CallICCE_ReportLog() | 上报成功 | P1 |

### 4.2 数据订阅

| 用例ID | 用例名称 | 前置条件 | 测试步骤 | 预期结果 | 优先级 |
|--------|----------|----------|----------|----------|--------|
| ICCE_DATA_010 | 订阅数据 | 已连接 | CallICCE_SubscribeData() | 订阅成功 | P0 |
| ICCE_DATA_011 | 取消订阅 | 已订阅 | CallICCE_UnsubscribeData() | 取消成功 | P0 |
| ICCE_DATA_012 | 数据推送 | 已订阅 | 推送到达 | 接收正确 | P0 |

---

## 5. ICCE协议合规测试用例

### 5.1 ICCE规范兼容性

| 用例ID | 用例名称 | 测试内容 | 预期结果 | 优先级 |
|--------|----------|----------|----------|--------|
| ICCE_COMP_001 | 服务发现 | 符合ICCE Service Discovery | 通过 | P0 |
| ICCE_COMP_002 | 设备管理 | 符合ICCE Device Management | 通过 | P0 |
| ICCE_COMP_003 | 钥匙管理 | 符合ICCE Key Management | 通过 | P0 |
| ICCE_COMP_004 | 数据交互 | 符合ICCE Data Exchange | 通过 | P0 |
| ICCE_COMP_005 | 安全保障 | 符合ICCE Security Spec | 通过 | P0 |