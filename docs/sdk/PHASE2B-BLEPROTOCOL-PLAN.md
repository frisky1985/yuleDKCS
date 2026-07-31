# yuleDKCS SDK Phase 2b — BLEProtocol 实现计划

> **平台**: iOS (Swift/CoreBluetooth) + Android (Kotlin/android.bluetooth)  
> **依赖**: Phase 1 proto + 既有协议参考（frontend/ 旧实现）  
> **工时**: ~10 天 (双平台并行)

---

## 架构

```
App
 ↓
BLEManager (平台层: CoreBluetooth / android.bluetooth)
 ↓
BleProtocolAdapter (协议层: CCC / ICCOA / ICCE)
 ↓
GATT 通道 (Service/Characteristic UUID)
 ↓
车辆 TCU
```

### 协议 UUID 表（参考 CCC-TS-101 / ICCOA-DK / T-CA-110-2020）

| 协议 | Service UUID | 关键 Characteristic |
|:-----|:-------------|:--------------------|
| **CCC** | `0xFFD1` | FFD2 Pairing · FFD3 KeyData · FFD4 Auth · FFD5 State · FFD6 UWBConfig · FFD7 RSSI |
| **ICCE** | `0xFEFA` | FEFB KeyStatus · FEFC RangingData · FEFD AuthChallenge · FEFE ControlCmd · FEFF SessionKey |
| **ICCOA** | `0xFEF5` | ICCOA 专有（见 ICCOA.DK.TS.002 BLE 章节） |

### 职责划分

| 层 | 职责 | 谁写 |
|:---|:-----|:-----|
| **BLEManager** | 扫描/连接/收发原始 GATT 数据 | 我们（平台差异封装） |
| **BleProtocolAdapter** | 指令编码/响应解析/安全通道 | 我们（按协议） |
| **YDKBLEManager** (proto service) | 对 App 暴露的高层接口 | 我们 |

---

## 模块分解

### 1. 平台 BLE 管理器

**iOS `YDKBLEManager`** (CoreBluetooth):
```swift
public final class YDKBLEManager: NSObject {
    // 状态
    public private(set) var state: BLEState
    public private(set) var connectionState: BLEConnectionState

    // 扫描
    public func scanVehicles(timeout: TimeInterval) async throws -> [VehicleAdvertise]
    public func stopScan()

    // 连接
    public func connect(vehicleId: String) async throws
    public func disconnect()

    // 通信（原始数据）
    func write(_ data: Data, to characteristic: CBUUID) async throws
    func read(_ characteristic: CBUUID) async throws -> Data
    func subscribe(_ characteristic: CBUUID) -> AsyncThrowingStream<Data, Error>
}
```

**Android `BleManager`** (android.bluetooth):
```kotlin
class BleManager(private val context: Context) {
    suspend fun scanVehicles(timeoutMs: Long): List<VehicleAdvertise>
    suspend fun connect(address: String)
    suspend fun disconnect()
    suspend fun write(service: UUID, char: UUID, data: ByteArray)
    suspend fun read(service: UUID, char: UUID): ByteArray
    fun subscribe(service: UUID, char: UUID): Flow<ByteArray>
}
```

### 2. 协议适配器

```swift
public protocol BleProtocolAdapter {
    var protocolType: Protocol { get }
    var serviceUUID: CBUUID { get }

    // 扫描广告包解析
    func parseAdvertisement(_ data: [String: Any], rssi: Int) -> VehicleAdvertise?

    // 解锁/锁车指令
    func buildUnlockCommand(key: YDKKey, sessionContext: SessionContext) throws -> Data
    func buildLockCommand(key: YDKKey, sessionContext: SessionContext) throws -> Data
    func buildStartEngineCommand(key: YDKKey, sessionContext: SessionContext) throws -> Data

    // 响应解析
    func parseCommandResponse(_ data: Data) throws -> CommandResult
}
```

```kotlin
interface BleProtocolAdapter {
    val protocolType: Protocol
    val serviceUuid: UUID

    fun parseAdvertisement(scanRecord: ByteArray?, rssi: Int): VehicleAdvertise?
    fun buildUnlockCommand(key: YDKKey, session: SessionContext): ByteArray
    fun buildLockCommand(key: YDKKey, session: SessionContext): ByteArray
    fun buildStartEngineCommand(key: YDKKey, session: SessionContext): ByteArray
    fun parseCommandResponse(data: ByteArray): CommandResult
}
```

### 3. 高层接口（对应 sdk.proto BLEManager）

```swift
public extension YDKBLEManager {
    func scanVehicles() async throws -> [VehicleAdvertise]       // ScanVehicles
    func connectVehicle(vehicleId: String) async throws         // ConnectVehicle
    func unlock(vehicleId: String) async throws                 // Unlock
    func lock(vehicleId: String) async throws                   // Lock
    func startEngine(vehicleId: String) async throws            // StartEngine
    func readVehicleStatus(vehicleId: String) async throws -> VehicleStatus
    func disconnect() async throws                              // Disconnect
}
```

### 4. UWB (FiRa) — 抽象层

```swift
public protocol YDKUWBManager {
    // 开始测距（对接 FiRa / iOS U1/U2）
    func startRanging(vehicleId: String) async throws
    func stopRanging()
    // 测距结果回调
    var rangingResultHandler: ((UWBMeasurement) -> Void)? { get set }
}
```

> 实现依赖硬件（iOS 需要 NearbyInteraction framework + 车厂 U1 chip 支持），
> 本期提供接口 + mock 实现，真实集成放后续。

### 5. NFC 备用解锁 — 抽象层

```swift
public protocol YDKNFCManager {
    // NFC 读取车辆标签（手机没电/无网络时）
    func readVehicleTag() async throws -> NFCVehicleInfo
    // 发送解锁指令（NFC 通道）
    func sendCommandViaNFC(command: NFCCommand) async throws
}
```

> 依赖 iOS Wallet NFC + Android HCE，本期提供接口 + 说明。

### 6. 后台 BLE

- iOS: `bluetooth-central` background mode + CBCentralManager 恢复
- Android: foreground service 保活

---

## 安全通道（按协议差异）

| 协议 | 配对方式 | 加密 | 说明 |
|:-----|:---------|:-----|:-----|
| CCC | OOB/数字配对 (FFD2) | ECDH + AES-CCM | CCC Reader Protocol |
| ICCOA | 绑定后 Key Agreement | SM4/ECDH | ICCOA.DK.TS.002 BLE 安全章节 |
| ICCE | 挑战-响应 (FEFD) | SM4 | T-CA-110-2020 |

> 安全通道的完整实现需要各协议规范原文，本期实现**接口 + 状态机骨架**，
> 具体加密算法按规范补齐。

---

## 排期

| 天 | iOS | Android | 可并行？ |
|:-:|:----|:--------|:--------:|
| 1-2 | 平台 BLE 管理器 (扫描+连接+收发) | 同 iOS | ✅ |
| 3-4 | 协议适配器 (CCC/ICCOA/ICCE) + 指令编解码 | 同 iOS | ✅ |
| 5-6 | 高层接口 + 状态机 + 测试 | 同 iOS | ✅ |
| 7 | UWB 抽象 + NFC 抽象 | 同 iOS | ✅ |
| 8 | 后台模式 | 同 iOS | ✅ |
