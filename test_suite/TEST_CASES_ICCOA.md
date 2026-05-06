# ICCOA协议栈测试用例

## 1. 认证模块 (Auth) 测试用例

### 1.1 身份认证

| 用例ID | 用例名称 | 前置条件 | 测试步骤 | 预期结果 | 优先级 |
|--------|----------|----------|----------|----------|--------|
| ICCOA_AUTH_001 | 用户登录 | App已安装 | 调用Auth_Login(user,pass) | 登录成功 | P0 |
| ICCOA_AUTH_002 | 登录失败 | 密码错误 | 调用Auth_Login(user,wrong) | 返回错误 | P0 |
| ICCOA_AUTH_003 | 登出 | 已登录 | 调用Auth_Logout() | 登出成功 | P0 |
| ICCOA_AUTH_004 | Token刷新 | Token过期 | 调用Auth_RefreshToken() | 新Token | P1 |

### 1.2 授权流程

| 用例ID | 用例名称 | 前置条件 | 测试步骤 | 预期结果 | 优先级 |
|--------|----------|----------|----------|----------|--------|
| ICCOA_AUTH_010 | 请求授权 | 已登录 | 调用Auth_RequestGrant() | 授权请求 | P0 |
| ICCOA_AUTH_011 | 授权确认 | 待确认 | 调用Auth_ConfirmGrant() | 授权成功 | P0 |
| ICCOA_AUTH_012 | 撤销授权 | 已授权 | 调用Auth_RevokeGrant() | 撤销成功 | P0 |
| ICCOA_AUTH_013 | 授权列表 | 有授权 | 调用Auth_ListGrants() | 返回列表 | P1 |

---

## 2. BLE模块测试用例

### 2.1 广播连接

| 用例ID | 用例名称 | 前置条件 | 测试步骤 | 预期结果 | 优先级 |
|--------|----------|----------|----------|----------|--------|
| ICCOA_BLE_001 | 开启广播 | BLE初始化 | 调用BLE_StartAdvertising() | 广播开启 | P0 |
| ICCOA_BLE_002 | 停止广播 | 广播中 | 调用BLE_StopAdvertising() | 广播停止 | P0 |
| ICCOA_BLE_003 | 连接建立 | 广播中 | 调用BLE_Connect() | 连接成功 | P0 |
| ICCOA_BLE_004 | 连接断开 | 已连接 | 调用BLE_Disconnect() | 断开成功 | P0 |

### 2.2 数据交互

| 用例ID | 用例名称 | 前置条件 | 测试步骤 | 预期结果 | 优先级 |
|--------|----------|----------|----------|----------|--------|
| ICCOA_BLE_010 | 发送指令 | 已连接 | 调用BLE_SendCommand(cmd) | 发送成功 | P0 |
| ICCOA_BLE_011 | 接收响应 | 已发送 | 调用BLE_ReceiveResponse() | 响应正确 | P0 |
| ICCOA_BLE_012 | OTA升级 | 固件可用 | 调用BLE_StartOTA() | 升级流程 | P1 |
| ICCOA_BLE_013 | 心跳检测 | 已连接 | 调用BLE_Heartbeat() | 心跳正常 | P1 |

---

## 3. ICCOA核心模块测试用例

### 3.1 车辆控制

| 用例ID | 用例名称 | 前置条件 | 测试步骤 | 预期结果 | 优先级 |
|--------|----------|----------|----------|----------|--------|
| ICCOA_CORE_001 | 解锁车门 | 已授权 | 调用VC_UnlockDoor() | 解锁成功 | P0 |
| ICCOA_CORE_002 | 锁闭车门 | 已连接 | 调用VC_LockDoor() | 锁闭成功 | P0 |
| ICCOA_CORE_003 | 启动引擎 | P档+刹车 | 调用VC_StartEngine() | 启动成功 | P0 |
| ICCOA_CORE_004 | 停止引擎 | 怠速中 | 调用VC_StopEngine() | 停止成功 | P0 |
| ICCOA_CORE_005 | 开启后备箱 | 已连接 | 调用VC_OpenTrunk() | 开启成功 | P0 |

### 3.2 状态查询

| 用例ID | 用例名称 | 前置条件 | 测试步骤 | 预期结果 | 优先级 |
|--------|----------|----------|----------|----------|--------|
| ICCOA_CORE_010 | 查询电量 | 无 | 调用VC_QueryBattery() | 返回电量 | P0 |
| ICCOA_CORE_011 | 查询里程 | 无 | 调用VC_QueryMileage() | 返回里程 | P0 |
| ICCOA_CORE_012 | 查询状态 | 无 | 调用VC_QueryStatus() | 返回状态 | P0 |
| ICCOA_CORE_013 | 查询位置 | Gps可用 | 调用VC_QueryLocation() | 返回位置 | P1 |

### 3.3 远程控制

| 用例ID | 用例名称 | 前置条件 | 测试步骤 | 预期结果 | 优先级 |
|--------|----------|----------|----------|----------|--------|
| ICCOA_CORE_020 | 远程解锁 | 网络连接 | 调用VC_RemoteUnlock() | 解锁成功 | P0 |
| ICCOA_CORE_021 | 远程锁车 | 网络连接 | 调���VC_RemoteLock() | 锁车成功 | P0 |
| ICCOA_CORE_022 | 远程鸣笛 | 网络连接 | 调用VC_RemoteHorn() | 鸣笛成功 | P0 |
| ICCOA_CORE_023 | 远程闪灯 | 网络连接 | 调用VC_RemoteFlash() | 闪灯成功 | P0 |

---

## 4. 服务模块测试用例

### 4.1 发现服务

| 用例ID | 用例名称 | 前置条件 | 测试步骤 | 预期结果 | 优先级 |
|--------|----------|----------|----------|----------|--------|
| ICCOA_SVC_001 | 发现车辆 | 已绑定 | 调用Service_DiscoverVehicle() | 发现成功 | P0 |
| ICCOA_SVC_002 | 绑定车辆 | 未绑定 | 调用Service_BindVehicle() | 绑定成功 | P0 |
| ICCOA_SVC_003 | 解绑车辆 | 已绑定 | 调用Service_UnbindVehicle() | 解绑成功 | P0 |
| ICCOA_SVC_004 | 车辆列表 | 有绑定 | 调用Service_ListVehicles() | 返回列表 | P1 |

### 4.2 数据同步

| 用例ID | 用例名称 | 前置条件 | 测试步骤 | 预期结果 | 优先级 |
|--------|----------|----------|----------|----------|--------|
| ICCOA_SVC_010 | 同步日志 | 有记录 | 调用Service_SyncLog() | 同步成功 | P0 |
| ICCOA_SVC_011 | 同步设置 | 有变更 | CallService_SyncSettings() | 同步成功 | P0 |
| ICCOA_SVC_012 | 上传诊断 | 异常发生 | CallService_UploadDiag() | 上传成功 | P1 |

---

## 5. 协议合规测试用例

### 5.1 ICCOA规范兼容性

| 用例ID | 用例名称 | 测试内容 | 预期结果 | 优先级 |
|--------|----------|----------|----------|--------|
| ICCOA_COMP_001 | 接入认证 | 符合ICCOA Auth Spec | 通过 | P0 |
| ICCOA_COMP_002 | 设备绑定 | 符合ICCOA Binding Spec | 通过 | P0 |
| ICCOA_COMP_003 | 钥匙分发 | 符合ICCOA Key Provisioning | 通过 | P0 |
| ICCOA_COMP_004 | 远程控制 | 符合ICCOA Remote Control | 通过 | P0 |
| ICCOA_COMP_005 | 数据同步 | 符合ICCOA Data Sync | 通过 | P0 |