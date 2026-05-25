# yuleDKCS Mobile SDK 使用指南

## 目录

1. [快速开始](#快速开始)
2. [Android SDK](#android-sdk)
3. [iOS SDK](#ios-sdk)
4. [功能详解](#功能详解)
5. [安全指南](#安全指南)
6. [故障排除](#故障排除)

---

## 快速开始

### 安装

#### Android

```gradle
dependencies {
    implementation 'com.yuledkcs:sdk:1.0.0'
}
```

#### iOS

```swift
dependencies: [
    .package(url: "https://github.com/yuledkcs/ios-sdk.git", from: "1.0.0")
]
```

### 初始化

#### Android

```kotlin
// Application.onCreate()
YuleDKCS.initialize(
    context = this,
    apiKey = "your-api-key",
    baseUrl = "https://api.yuledkcs.com"
)
```

#### iOS

```swift
// AppDelegate.swift 或 App.swift
YuleDKCS.shared.initialize(
    apiKey: "your-api-key",
    baseUrl: "https://api.yuledkcs.com"
)
```

---

## Android SDK

### 钥匙管理

```kotlin
val keyManager = KeyManager.getInstance(context)

// 获取所有钥匙
val keys = keyManager.listKeys()

// 设置活动钥匙
keyManager.setActiveKey(keyId)

// 监听钥匙变化
keyManager.keysFlow.observe(lifecycleOwner) { keys ->
    // 更新UI
}
```

### 接收分享钥匙

```kotlin
// 通过二维码/链接接收
keyManager.receiveSharedKey(shareToken) { result ->
    result.onSuccess { key ->
        // 激活钥匙
        keyManager.activateKey(key.id) { activationResult ->
            activationResult.onSuccess {
                Toast.makeText(context, "钥匙激活成功", Toast.LENGTH_SHORT).show()
            }
        }
    }
}
```

### BLE连接

```kotlin
val bleManager = BLEManager.getInstance()

// 开始扫描
bleManager.startScan(timeoutMs = 10000)

// 监听扫描结果
bleManager.scanResults.observe(lifecycleOwner) { results ->
    // 显示设备列表
}

// 连接到车辆
bleManager.connect(deviceAddress) { success ->
    if (success) {
        Log.i("DKCS", "连接成功")
    }
}
```

### 车辆控制

```kotlin
// 获取当前活动钥匙
val keyId = keyManager.getActiveKey()?.id ?: return

// 解锁
bleManager.unlock(keyId) { result ->
    result.onSuccess { response ->
        if (response.success) {
            // 解锁成功
        }
    }
}

// 锁车
bleManager.lock(keyId) { /* ... */ }

// 启动引擎
bleManager.startEngine(keyId) { /* ... */ }

// 打开后备箱
bleManager.openTrunk(keyId) { /* ... */ }

// 寻车
bleManager.findVehicle(keyId) { /* ... */ }
```

### 实时状态

```kotlin
// 监听车辆状态
bleManager.vehicleStatus.observe(lifecycleOwner) { status ->
    status?.let {
        Log.i("DKCS", "门锁状态: ${it.doorLocked}")
        Log.i("DKCS", "电量: ${it.batteryLevel}%")
    }
}
```

---

## iOS SDK

### 钥匙管理

```swift
let keyManager = KeyManager.shared

// 获取所有钥匙
let keys = keyManager.listKeys()

// 设置活动钥匙
keyManager.setActiveKey(keyId)

// 监听钥匙变化
keyManager.$keys
    .sink { keys in
        // 更新UI
    }
    .store(in: &cancellables)
```

### 接收分享钥匙

```swift
// 通过二维码/链接接收
keyManager.receiveSharedKey(shareToken: token) { result in
    switch result {
    case .success(let key):
        // 激活钥匙
        keyManager.activateKey(keyId: key.id) { activationResult in
            if case .success = activationResult {
                print("钥匙激活成功")
            }
        }
    case .failure(let error):
        print("接收失败: \(error)")
    }
}
```

### BLE连接

```swift
let bleManager = BLEManager.shared

// 开始扫描
bleManager.startScan(timeout: 10.0)

// 监听扫描结果
bleManager.$scanResults
    .sink { results in
        // 显示设备列表
    }
    .store(in: &cancellables)

// 连接到车辆
bleManager.connect(to: deviceUUID) { success in
    if success {
        print("连接成功")
    }
}

// 连接到最近的车辆
bleManager.connectToNearest { result in
    switch result {
    case .success(let address):
        print("已连接到: \(address)")
    case .failure(let error):
        print("连接失败: \(error)")
    }
}
```

### 车辆控制

```swift
// 获取当前活动钥匙
guard let keyId = keyManager.getActiveKey()?.id else { return }

// 解锁
bleManager.unlock(keyId: keyId) { result in
    switch result {
    case .success(let response):
        if response.success {
            // 解锁成功
        }
    case .failure(let error):
        print("解锁失败: \(error)")
    }
}

// 其他命令
bleManager.lock(keyId: keyId) { _ in }
bleManager.startEngine(keyId: keyId) { _ in }
bleManager.stopEngine(keyId: keyId) { _ in }
bleManager.openTrunk(keyId: keyId) { _ in }
bleManager.findVehicle(keyId: keyId) { _ in }
```

### 实时状态

```swift
// 监听车辆状态
bleManager.$vehicleStatus
    .compactMap { $0 }
    .sink { status in
        print("门锁: \(status.doorLocked ? "已锁" : "未锁")")
        print("电量: \(status.batteryLevel)%")
        print("引擎: \(status.engineRunning ? "运行中" : "已关闭")")
    }
    .store(in: &cancellables)
```

---

## 功能详解

### 钥匙分享流程

```
1. 车主在 Web 后台发起分享
   - 选择车辆
   - 输入接收方手机号/邮箱
   - 设置有效期和权限
   - 生成分享链接或二维码

2. 接收方通过链接/码获取钥匙

3. 接收方在 App 中激活钥匙
   - 调用 receiveSharedKey()
   - 调用 activateKey() 绑定到 SE

4. 激活后即可使用
```

### 权限系统

| 权限 | 说明 | 默认启用 |
|------|------|---------|
| unlock | 解锁车辆 | ✓ |
| lock | 锁车 | ✓ |
| start_engine | 启动引擎 | - |
| stop_engine | 停止引擎 | - |
| open_trunk | 开后备箱 | - |
| open_windows | 开窗 | - |
| close_windows | 关窗 | - |
| find_vehicle | 寻车(鸣叭/闪灯) | - |

### 离线支持

SDK 支持无网络环境下的操作：

- 钥匙数据本地加密存储
- BLE通信不需要网络
- 操作记录本地缓存
- 网络恢复后自动同步

---

## 安全指南

### SE (安全元) 保护

Android: 使用 Android Keystore System
- 钥匙材料不离开 TEE
- 硬件支持时使用 StrongBox

iOS: 使用 Secure Enclave
- 生物识别验证后才能访问钥匙
- 私钥永不离开 Secure Enclave

### 生物识别

```kotlin
// Android
val biometricPrompt = BiometricPrompt(activity, executor,
    object : BiometricPrompt.AuthenticationCallback() {
        override fun onAuthenticationSucceeded(result: AuthenticationResult) {
            // 验证成功，可以使用钥匙
        }
    })

biometricPrompt.authenticate(promptInfo)
```

```swift
// iOS
let context = LAContext()
context.evaluatePolicy(.deviceOwnerAuthenticationWithBiometrics,
                       localizedReason: "验证以使用数字钥匙") { success, error in
    if success {
        // 验证成功
    }
}
```

### 命令签名

所有通过 BLE 发送的命令都经过签名：
1. 使用钥匙的私钥对命令包进行签名
2. 车辆验证签名后才执行命令
3. 每个命令包含时间戳防重放攻击

---

## 故障排除

### BLE 连接失败

| 问题 | 原因 | 解决方案 |
|-----|------|---------|
| 设备未发现 | 车辆蓝牙未开启 | 确认车辆电池有电，蓝牙已启用 |
| 连接超时 | 距离过远或干扰 | 靠近车辆后重试 |
| 验证失败 | 钥匙未激活 | 检查钥匙状态，重新激活 |
| 命令失败 | 权限不足 | 检查钥匙权限设置 |

### 常见错误码

| 错误码 | 说明 | 处理 |
|--------|------|------|
| 401 | API Key 无效 | 检查配置的 API Key |
| 403 | 权限不足 | 确认钥匙状态和权限 |
| 404 | 钥匙不存在 | 检查 keyId 是否正确 |
| 409 | 钥匙已激活 | 无需重复激活 |
| 410 | 钥匙已过期 | 联系车主重新分享 |

### 调试日志

开启调试日志以诊断问题：

```kotlin
// Android
YuleDKCS.setLogLevel(LogLevel.DEBUG)
```

```swift
// iOS
YuleDKCS.shared.setLogLevel(.debug)
```

---

## 更新日志

### v1.0.0 (2026-05-11)
- 首次发布
- 支持 CCC/ICCOA/ICCE 协议
- 完整的钥匙生命周期管理
- BLE车辆控制
- 离线支持

---

*有问题请联系 support@yuledkcs.com*
