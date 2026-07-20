# Mobile SDK Specification - Android/iOS

**版本**: 1.0.0
**日期**: 2026-05-06

---

## 1. SDK概述

### 1.1 架构

```
┌─────────────────────────────────────────────────────────┐
│                    Mobile App                           │
├─────────────────────────────────────────────────────────┤
│  DigitalKey SDK (Android/iOS)                          │
├──────────┬──────────┬──────────┬──────────┬──────────────┤
│ KeyMgr   │ Vehicle  │ Share    │ Channel  │ Security     │
│          │ Controller│          │ Manager  │ Module       │
├──────────┴──────────┴──────────┴──────────┴──────────────┤
│              Protocol Layer (BERTLV)                    │
├─────────────────────────────────────────────────────────┤
│  Transport Layer (HTTPS/WebSocket)                      │
└─────────────────────────────────────────────────────────┘
```

### 1.2 SDK能力

| 模块 | 功能 |
|------|------|
| KeyManager | 密钥全生命周期管理 |
| VehicleController | 车辆控制指令 |
| ShareManager | 密钥分享管理 |
| ChannelManager | NFC/BLE/UWB通道管理 |
| SecurityModule | 加密/签名/SE交互 |

---

## 2. Android SDK

### 2.1 初始化

```kotlin
// DigitalKeySdk.kt
object DigitalKeySdk {
    
    fun init(context: Context, config: SdkConfig): Result<Unit>
    
    data class SdkConfig(
        val serverUrl: String,        // "https://api.digitalkey.cn"
        val appId: String,            // 应用标识
        val clientId: String,         // 客户端ID
        val apiKey: String,           // API Key
        val enableLog: Boolean = false
    )
}

// 使用示例
DigitalKeySdk.init(context, SdkConfig(
    serverUrl = "https://api.digitalkey.cn",
    appId = "com.example.app",
    clientId = "android_xxxxx",
    apiKey = "your_api_key"
))
```

### 2.2 密钥管理 - KeyManager

```kotlin
interface KeyManager {
    
    // 用户认证
    suspend fun login(phone: String, code: String): Result<UserSession>
    suspend fun register(userInfo: UserInfo): Result<UserSession>
    suspend fun logout(): Result<Unit>
    
    // 设备绑定
    suspend fun bindDevice(vehicleId: String): Result<PairingSession>
    suspend fun confirmPairing(code: String, bleDevice: BleDevice): Result<DigitalKey>
    
    // 密钥管理
    suspend fun getKeys(): Result<List<DigitalKey>>
    suspend fun getKeyDetail(keyId: String): Result<DigitalKey>
    suspend fun deleteKey(keyId: String): Result<Unit>
    suspend fun suspendKey(keyId: String): Result<Unit>
    suspend fun resumeKey(keyId: String): Result<Unit>
    
    // 刷新密钥
    suspend fun refreshKey(keyId: String): Result<DigitalKey>
}

// 数据模型
data class UserSession(
    val accessToken: String,
    val refreshToken: String,
    val expiresIn: Long,        // 秒
    val userId: String
)

data class UserInfo(
    val phone: String,
    val nickname: String?,
    val email: String?
)

data class PairingSession(
    val sessionId: String,
    val vehicleId: String,
    val expiresAt: Long
)

data class DigitalKey(
    val keyId: String,
    val vehicleId: String,
    val vehicleName: String,
    val keyType: KeyType,
    val status: KeyStatus,
    val accessLevel: AccessLevel,
    val validFrom: Long,
    val validUntil: Long,
    val maxUses: Int?,
    val usedCount: Int,
    val createdAt: Long
)

enum class KeyType { OWNER, FRIEND, SERVICE, TEMPORARY }
enum class KeyStatus { ACTIVE, SUSPENDED, REVOKED, EXPIRED, PENDING }

@IntDef(flag = true)
@Retention(AnnotationRetention.BINARY)
annotation class AccessLevel
```

### 2.3 车辆控制 - VehicleController

```kotlin
interface VehicleController {
    
    // 发送控制指令
    suspend fun sendCommand(
        vehicleId: String,
        keyId: String,
        action: VehicleAction,
        params: Map<String, Any>? = null
    ): Result<CommandResult>
    
    // 查询指令状态
    suspend fun getCommandStatus(cmdId: String): Result<CommandStatus>
    
    // 批量查询
    suspend fun getVehicleStatus(vehicleId: String): Result<VehicleStatus>
    
    // 监听车辆状态变化
    fun observeVehicleStatus(vehicleId: String): Flow<VehicleStatus>
}

enum class VehicleAction {
    UNLOCK,           // 解锁
    LOCK,             // 闭锁
    ENGINE_START,     // 启动引擎
    ENGINE_STOP,      // 熄火
    TRUNK_OPEN,       // 开启后备箱
    WINDOW_UP,        // 车窗升起
    WINDOW_DOWN,      // 车窗降下
    CLIMATE_ON,       // 开启空调
    CLIMATE_OFF,      // 关闭空调
    FIND_VEHICLE,     // 寻车
    HORN              // 鸣笛
}

data class CommandResult(
    val cmdId: String,
    val status: CommandStatusType,
    val estimatedTime: Long?
)

enum class CommandStatusType { 
    PENDING, 
    SENT, 
    EXECUTING, 
    SUCCESS, 
    FAILED, 
    TIMEOUT 
}

data class VehicleStatus(
    val vehicleId: String,
    val lockStatus: LockStatus,
    val engineStatus: EngineStatus,
    val doorStatus: DoorStatus,
    val windowStatus: WindowStatus,
    val batteryPct: Int,
    val interiorTemp: Float?,
    val latitude: Double?,
    val longitude: Double?,
    val timestamp: Long
)

enum class LockStatus { UNLOCKED, LOCKED, UNKNOWN }
enum class EngineStatus { OFF, RUNNING, STARTING }
data class DoorStatus(val frontLeft: Boolean, val frontRight: Boolean, 
                      val rearLeft: Boolean, val rearRight: Boolean,
                      val trunk: Boolean)
data class WindowStatus(val frontLeft: Float, val frontRight: Float,
                       val rearLeft: Float, val rearRight: Float)
```

### 2.4 密钥分享 - ShareManager

```kotlin
interface ShareManager {
    
    // 创建分享
    suspend fun createShare(
        keyId: String,
        toUserId: String?,
        toVendor: String?,
        accessLevel: Int,
        validFrom: Long,
        validUntil: Long,
        maxUses: Int?
    ): Result<KeyShare>
    
    // 创建分享码
    suspend fun createShareWithCode(
        keyId: String,
        accessLevel: Int,
        validFrom: Long,
        validUntil: Long,
        maxUses: Int?
    ): Result<ShareCode>
    
    // 接受分享
    suspend fun acceptShare(shareCode: String): Result<DigitalKey>
    
    // 取消分享
    suspend fun cancelShare(shareId: String): Result<Unit>
    
    // 查询分享列表
    suspend fun getSentShares(): Result<List<KeyShare>>
    suspend fun getReceivedShares(): Result<List<KeyShare>>
}

data class KeyShare(
    val shareId: String,
    val keyId: String,
    val fromUserId: String,
    val toUserId: String?,
    val toVendor: String?,
    val status: ShareStatus,
    val accessLevel: Int,
    val validFrom: Long,
    val validUntil: Long,
    val usedCount: Int,
    val maxUses: Int?,
    val createdAt: Long
)

data class ShareCode(
    val code: String,          // 6位数字
    val url: String,            // 分享链接
    val expiresAt: Long
)

enum class ShareStatus { PENDING, ACCEPTED, EXPIRED, CANCELLED }
```

### 2.5 通道管理 - ChannelManager

```kotlin
interface ChannelManager {
    
    // 检查并请求权限
    suspend fun checkPermissions(): Result<Permissions>
    
    // NFC操作
    fun observeNfcTag(): Flow<NfcTagEvent>
    suspend fun readNfcTag(tagId: String): Result<NfcTagData>
    
    // BLE操作
    suspend fun startBleScan(): Flow<BleDevice>
    suspend fun connectBle(device: BleDevice): Result<BleConnection>
    suspend fun disconnectBle(connectionId: String): Result<Unit>
    
    // UWB操作
    suspend fun startUwbRanging(): Flow<UwbDistance>
    suspend fun stopUwbRanging(): Result<Unit>
    
    // 选择最佳通道
    suspend fun selectChannel(vehicleId: String): ChannelType
}

data class Permissions(
    val nfcEnabled: Boolean,
    val bleEnabled: Boolean,
    val locationEnabled: Boolean,
    val cameraEnabled: Boolean  // 用于扫描分享码
)

enum class ChannelType { NFC, BLE, UWB, CLOUD }

sealed class NfcTagEvent {
    data class Discovered(val tagId: String, val techList: List<String>) : NfcTagEvent()
    data class Lost(val tagId: String) : NfcTagEvent()
}

data class NfcTagData(
    val tagId: String,
    val vehicleId: String?,
    val keyId: String?,
    val rssi: Int
)

data class BleDevice(
    val macAddress: String,
    val name: String?,
    val rssi: Int,
    val isConnected: Boolean
)

data class BleConnection(
    val connectionId: String,
    val device: BleDevice,
    val mtu: Int,
    val isEncrypted: Boolean
)

data class UwbDistance(
    val vehicleId: String,
    val distanceMm: Int,
    val accuracyMm: Int,
    val azimuth: Float?,
    val elevation: Float?
)
```

### 2.6 安全模块 - SecurityModule

```kotlin
interface SecurityModule {
    
    // SE (Secure Element) 操作
    suspend fun isSeAvailable(): Boolean
    suspend fun generateKeyPair(alg: KeyAlgorithm): Result<KeyPair>
    suspend fun sign(data: ByteArray, keyId: String): Result<ByteArray>
    suspend fun verify(signature: ByteArray, data: ByteArray, publicKey: ByteArray): Result<Boolean>
    suspend fun encrypt(data: ByteArray, keyId: String): Result<ByteArray>
    suspend fun decrypt(data: ByteArray, keyId: String): Result<ByteArray>
    
    // 证书管理
    suspend fun installCertificate(cert: X509Certificate): Result<Unit>
    suspend fun getCertificate(keyId: String): Result<X509Certificate>
    
    // 密钥存储
    suspend fun storeKey(keyId: String, key: ByteArray): Result<Unit>
    suspend fun deleteKey(keyId: String): Result<Unit>
}

enum class KeyAlgorithm { 
    EC_P256,      // 256-bit EC
    EC_P384,      // 384-bit EC  
    RSA_2048,     // 2048-bit RSA
    AES_128,      // 128-bit AES
    AES_256       // 256-bit AES
}

interface KeyPair {
    val publicKey: ByteArray
    val privateKeyRef: String  // SE内引用
}
```

### 2.7 消息推送 - PushListener

```kotlin
interface PushListener {
    
    // 监听推送消息
    fun observePushMessages(): Flow<PushMessage>
    
    // 处理令牌
    fun updatePushToken(token: String, platform: PushPlatform): Result<Unit>
}

sealed class PushMessage {
    data class KeyChanged(val keyId: String, val action: KeyChangeAction) : PushMessage()
    data class ShareReceived(val shareId: String, val fromUserId: String) : PushMessage()
    data class VehicleStatusChanged(val vehicleId: String, val status: VehicleStatus) : PushMessage()
    data class CommandCompleted(val cmdId: String, val result: CommandResult) : PushMessage()
    data class Alert(val alertType: AlertType, val message: String) : PushMessage()
}

enum class KeyChangeAction { CREATED, UPDATED, SUSPENDED, RESUMED, REVOKED, EXPIRED }
enum class PushPlatform { FCM, HMS, APNS }
enum class AlertType { SECURITY, VEHICLE, BATTERY_LOW, UNKNOWN }
```

---

## 3. iOS SDK

### 3.1 初始化

```swift
// DigitalKeySDK.swift
public class DigitalKeySDK {
    
    public static func configure(
        serverUrl: String,
        appId: String,
        clientId: String,
        apiKey: String
    ) throws
    
    public static var shared: DigitalKeySDK { get }
}

// 使用示例
try DigitalKeySDK.configure(
    serverUrl: "https://api.digitalkey.cn",
    appId: "com.example.app",
    clientId: "ios_xxxxx",
    apiKey: "your_api_key"
)
```

### 3.2 密钥管理 - KeyManager

```swift
public protocol KeyManaging {
    
    // 用户认证
    func login(phone: String, code: String) async throws -> UserSession
    func register(userInfo: UserInfo) async throws -> UserSession
    func logout() async throws
    
    // 设备绑定
    func bindDevice(vehicleId: String) async throws -> PairingSession
    func confirmPairing(code: String, bleDevice: BleDevice) async throws -> DigitalKey
    
    // 密钥管理
    func getKeys() async throws -> [DigitalKey]
    func getKeyDetail(keyId: String) async throws -> DigitalKey
    func deleteKey(keyId: String) async throws
    func suspendKey(keyId: String) async throws
    func resumeKey(keyId: String) async throws
    func refreshKey(keyId: String) async throws -> DigitalKey
}

// 数据模型
public struct UserSession {
    public let accessToken: String
    public let refreshToken: String
    public let expiresIn: TimeInterval
    public let userId: String
}

public struct DigitalKey {
    public let keyId: String
    public let vehicleId: String
    public let vehicleName: String
    public let keyType: KeyType
    public let status: KeyStatus
    public let accessLevel: UInt16
    public let validFrom: Date
    public let validUntil: Date
    public let maxUses: Int?
    public let usedCount: Int
    public let createdAt: Date
}

public enum KeyType: Int { case owner = 1, friend = 2, service = 3, temporary = 4 }
public enum KeyStatus: Int { case active = 1, suspended = 2, revoked = 3, expired = 4, pending = 5 }
```

### 3.3 车辆控制 - VehicleController

```swift
public protocol VehicleControlling {
    
    func sendCommand(
        vehicleId: String,
        keyId: String,
        action: VehicleAction,
        params: [String: Any]?
    ) async throws -> CommandResult
    
    func getCommandStatus(cmdId: String) async throws -> CommandStatus
    func getVehicleStatus(vehicleId: String) async throws -> VehicleStatus
    
    // 使用Combine观察状态变化
    func observeVehicleStatus(vehicleId: String) -> AnyPublisher<VehicleStatus, Error>
}

public enum VehicleAction: Int {
    case unlock = 1
    case lock = 2
    case engineStart = 3
    case engineStop = 4
    case trunkOpen = 5
    case windowUp = 6
    case windowDown = 7
    case climateOn = 8
    case climateOff = 9
    case findVehicle = 10
    case horn = 11
}

public struct CommandResult {
    public let cmdId: String
    public let status: CommandStatusType
    public let estimatedTime: TimeInterval?
}

public enum CommandStatusType: Int {
    case pending = 0, sent = 1, executing = 2, success = 3, failed = 4, timeout = 5
}
```

### 3.4 分享管理 - ShareManager

```swift
public protocol ShareManaging {
    
    func createShare(
        keyId: String,
        toUserId: String?,
        toVendor: String?,
        accessLevel: UInt16,
        validFrom: Date,
        validUntil: Date,
        maxUses: Int?
    ) async throws -> KeyShare
    
    func createShareWithCode(
        keyId: String,
        accessLevel: UInt16,
        validFrom: Date,
        validUntil: Date,
        maxUses: Int?
    ) async throws -> ShareCode
    
    func acceptShare(code: String) async throws -> DigitalKey
    func cancelShare(shareId: String) async throws
    func getSentShares() async throws -> [KeyShare]
    func getReceivedShares() async throws -> [KeyShare]
}

public struct ShareCode {
    public let code: String
    public let url: String
    public let expiresAt: Date
}
```

### 3.5 通道管理 - ChannelManager

```swift
public protocol ChannelManaging {
    
    func checkPermissions() async throws -> Permissions
    
    // NFC
    func observeNFCTag() -> AnyPublisher<NFCEvent, Error>
    
    // BLE
    func startBLEScan() -> AnyPublisher<BleDevice, Error>
    func connectBLE(device: BleDevice) async throws -> BLEConnection
    func disconnectBLE(connectionId: String) async throws
    
    // UWB
    func startUWBRanging() -> AnyPublisher<UWBDistance, Error>
    func stopUWBRanging() async throws
    
    func selectChannel(vehicleId: String) async throws -> ChannelType
}

public enum ChannelType: Int { case nfc = 1, ble = 2, uwb = 3, cloud = 4 }

public struct BLEConnection {
    public let connectionId: String
    public let device: BleDevice
    public let mtu: Int
    public let isEncrypted: Bool
}
```

### 3.6 安全模块 - SecurityModule

```swift
public protocol SecurityManaging {
    
    var isSecureEnclaveAvailable: Bool { get }
    
    func generateKeyPair(algorithm: KeyAlgorithm) async throws -> KeyPair
    func sign(data: Data, keyId: String) async throws -> Data
    func verify(signature: Data, data: Data, publicKey: Data) async throws -> Bool
    func encrypt(data: Data, keyId: String) async throws -> Data
    func decrypt(data: Data, keyId: String) async throws -> Data
    func installCertificate(_ cert: SecCertificate) async throws
}

public enum KeyAlgorithm {
    case ecP256, ecP384, rsa2048, aes128, aes256
}

public struct KeyPair {
    public let publicKey: Data
    public let privateKeyRef: String
}
```

---

## 4. 跨平台接口对照

| 功能 | Android | iOS |
|------|---------|-----|
| 初始化 | `DigitalKeySdk.init()` | `DigitalKeySDK.configure()` |
| 登录 | `keyManager.login()` | `keyManaging.login()` |
| 获取密钥 | `keyManager.getKeys()` | `keyManaging.getKeys()` |
| 发送指令 | `controller.sendCommand()` | `controller.sendCommand()` |
| 创建分享 | `shareManager.createShare()` | `shareManaging.createShare()` |
| 接受分享 | `shareManager.acceptShare()` | `shareManaging.acceptShare()` |
| NFC观察 | `channelManager.observeNfcTag()` | `channelManaging.observeNFCTag()` |
| BLE扫描 | `channelManager.startBleScan()` | `channelManaging.startBLEScan()` |
| UWB测距 | `channelManager.startUwbRanging()` | `channelManaging.startUWBRanging()` |
| 签名 | `securityModule.sign()` | `securityManaging.sign()` |

---

## 5. 错误处理

### 5.1 统一错误类型

```kotlin
// Android
sealed class DigitalKeyException : Exception() {
    data class Network(val code: Int, val message: String) : DigitalKeyException()
    data class Auth(val code: Int, val message: String) : DigitalKeyException()
    data class Key(val code: Int, val message: String) : DigitalKeyException()
    data class Vehicle(val code: Int, val message: String) : DigitalKeyException()
    data class Hardware(val message: String) : DigitalKeyException()
}

// iOS
enum DigitalKeyError: Error {
    case network(code: Int, message: String)
    case auth(code: Int, message: String)
    case key(code: Int, message: String)
    case vehicle(code: Int, message: String)
    case hardware(message: String)
}
```

### 5.2 硬件错误码

| 错误 | Android | iOS | 说明 |
|------|---------|-----|------|
| NFC未启用 | `NfcDisabledException` | `NFCDisabledError` | |
| 蓝牙未启用 | `BluetoothDisabledException` | `BluetoothDisabledError` | |
| 位置未启用 | `LocationDisabledException` | `LocationDisabledError` | |
| SE不可用 | `SecureElementException` | `SecureElementError` | |
| UWB不可用 | `UwbNotSupportedException` | `UwbNotSupportedError` | |

---

## 6. 版本要求

### 6.1 Android

| 要求 | 版本 |
|------|------|
| Min SDK | 26 (Android 8.0) |
| Target SDK | 34 (Android 14) |
| 权限 | NFC, BLE, Location, Camera |

### 6.2 iOS

| 要求 | 版本 |
|------|------|
| Min iOS | 14.0 |
| Frameworks | CoreNFC, CoreBluetooth, CoreLocation |
| Capabilities | NFC, Background Modes |

---

## 7. 版本演进

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0.0 | 2026-05-06 | 初始版本 |
