import Foundation
import CoreBluetooth
import YDKHubClient

// MARK: - BLE 管理器协议

/// 对 App 暴露的 BLE 管理器接口（对应 sdk.proto BLEManager service）
public protocol YDKBLEManaging: AnyObject {
    var state: YDKBLEState { get }
    var connectionState: YDKConnectionState { get }

    // 扫描
    func scanVehicles(timeout: TimeInterval) async throws -> [VehicleAdvertise]
    func stopScan()

    // 连接
    func connectVehicle(vehicleId: String) async throws
    func disconnect() async throws

    // 控制
    func unlock(vehicleId: String) async throws
    func lock(vehicleId: String) async throws
    func startEngine(vehicleId: String) async throws
    func readVehicleStatus(vehicleId: String) async throws -> VehicleStatus

    // 回调
    var connectionChangeHandler: ((Int32) -> Void)? { get set }
}

// MARK: - CoreBluetooth 抽象层 (可注入 fake 测试)

/// 中央管理器抽象 — 测试时注入 fake, 生产用 `YDKCoreBluetoothCentral` 包装 CBCentralManager。
protocol YDKCentralManaging: AnyObject {
    var state: CBManagerState { get }
    var isScanning: Bool { get }

    var onStateChange: ((CBManagerState) -> Void)? { get set }
    var onPeripheralDiscovered: ((YDKPeripheralManaging, [String: Any], Int) -> Void)? { get set }
    var onPeripheralConnected: ((YDKPeripheralManaging) -> Void)? { get set }
    var onPeripheralFailedToConnect: ((YDKPeripheralManaging, Error?) -> Void)? { get set }
    var onPeripheralDisconnected: ((YDKPeripheralManaging, Error?) -> Void)? { get set }
    /// 系统状态恢复回调 (2b-I AD-4): CBCentralManager 携带 restore identifier 重建后,
    /// 系统把被杀前已连接/扫描中的外设经 `willRestoreState` 交回, 这里包装成
    /// `YDKPeripheralManaging` 数组传出。未启用 state restoration 时不会触发。
    var onRestoreState: (([YDKPeripheralManaging]) -> Void)? { get set }

    func scanForPeripherals(withServices serviceUUIDs: [CBUUID]?, options: [String: Any]?)
    func stopScan()
    func connect(_ peripheral: YDKPeripheralManaging, options: [String: Any]?)
    func cancelPeripheralConnection(_ peripheral: YDKPeripheralManaging)
    func retrieveConnectedPeripherals(withServices serviceUUIDs: [CBUUID]) -> [YDKPeripheralManaging]
}

/// 外设抽象 — 测试时注入 fake。
protocol YDKPeripheralManaging: AnyObject {
    var identifier: UUID { get }
    var name: String? { get }
    var services: [YDKServiceManaging]? { get }

    var onServicesDiscovered: ((Error?) -> Void)? { get set }
    var onCharacteristicsDiscovered: ((Error?) -> Void)? { get set }
    var onWriteCompleted: ((Error?) -> Void)? { get set }
    var onValueUpdated: ((Data?, Error?) -> Void)? { get set }

    func discoverServices(_ serviceUUIDs: [CBUUID]?)
    func discoverCharacteristics(_ characteristicUUIDs: [CBUUID]?, for service: YDKServiceManaging)
    func writeValue(_ data: Data, for characteristic: YDKCharacteristicManaging, type: CBCharacteristicWriteType)
    func readValue(for characteristic: YDKCharacteristicManaging)
}

/// GATT 服务抽象
protocol YDKServiceManaging: AnyObject {
    var uuid: CBUUID { get }
    var characteristics: [YDKCharacteristicManaging]? { get }
}

/// GATT 特征抽象
protocol YDKCharacteristicManaging: AnyObject {
    var uuid: CBUUID { get }
    var value: Data? { get }
}

// MARK: - CoreBluetooth 真实实现 (包装系统框架)

/// CBCentralManager 包装: 把 delegate 回调转为闭包, 供 YDKBLEManager 使用。
final class YDKCoreBluetoothCentral: NSObject, YDKCentralManaging, CBCentralManagerDelegate {
    private let manager: CBCentralManager

    var onStateChange: ((CBManagerState) -> Void)?
    var onPeripheralDiscovered: ((YDKPeripheralManaging, [String: Any], Int) -> Void)?
    var onPeripheralConnected: ((YDKPeripheralManaging) -> Void)?
    var onPeripheralFailedToConnect: ((YDKPeripheralManaging, Error?) -> Void)?
    var onPeripheralDisconnected: ((YDKPeripheralManaging, Error?) -> Void)?
    var onRestoreState: (([YDKPeripheralManaging]) -> Void)?

    init(queue: DispatchQueue? = nil, options: [String: Any]? = nil) {
        self.manager = CBCentralManager(delegate: nil, queue: queue, options: options)
        super.init()
        self.manager.delegate = self
    }

    var state: CBManagerState { manager.state }
    var isScanning: Bool { manager.isScanning }

    func scanForPeripherals(withServices serviceUUIDs: [CBUUID]?, options: [String: Any]?) {
        manager.scanForPeripherals(withServices: serviceUUIDs, options: options)
    }

    func stopScan() {
        manager.stopScan()
    }

    func connect(_ peripheral: YDKPeripheralManaging, options: [String: Any]?) {
        guard let wrapper = peripheral as? YDKCoreBluetoothPeripheral else { return }
        manager.connect(wrapper.peripheral, options: options)
    }

    func cancelPeripheralConnection(_ peripheral: YDKPeripheralManaging) {
        guard let wrapper = peripheral as? YDKCoreBluetoothPeripheral else { return }
        manager.cancelPeripheralConnection(wrapper.peripheral)
    }

    func retrieveConnectedPeripherals(withServices serviceUUIDs: [CBUUID]) -> [YDKPeripheralManaging] {
        manager.retrieveConnectedPeripherals(withServices: serviceUUIDs).map { YDKCoreBluetoothPeripheral.wrap($0) }
    }

    // MARK: CBCentralManagerDelegate

    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        onStateChange?(central.state)
    }

    func centralManager(_ central: CBCentralManager,
                        didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any],
                        rssi RSSI: NSNumber) {
        onPeripheralDiscovered?(YDKCoreBluetoothPeripheral.wrap(peripheral), advertisementData, RSSI.intValue)
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        onPeripheralConnected?(YDKCoreBluetoothPeripheral.wrap(peripheral))
    }

    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        onPeripheralFailedToConnect?(YDKCoreBluetoothPeripheral.wrap(peripheral), error)
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        onPeripheralDisconnected?(YDKCoreBluetoothPeripheral.wrap(peripheral), error)
    }

    /// 状态恢复 (2b-I AD-4): 系统在后台重建 CBCentralManager 后, 把被杀前已连接/扫描中的
    /// 外设经此回调交回。从 dict 取 `CBCentralManagerRestoredStatePeripheralsKey` 对应的
    /// `[CBPeripheral]`, 包装成 `YDKPeripheralManaging` 后经 `onRestoreState` 交给上层。
    func centralManager(_ central: CBCentralManager, willRestoreState dict: [String: Any]) {
        let restored = dict[CBCentralManagerRestoredStatePeripheralsKey] as? [CBPeripheral] ?? []
        onRestoreState?(restored.map { YDKCoreBluetoothPeripheral.wrap($0) })
    }
}

/// CBPeripheral 包装: 同一 CBPeripheral 缓存为同一 wrapper, 保证闭包回调不丢失。
final class YDKCoreBluetoothPeripheral: NSObject, YDKPeripheralManaging, CBPeripheralDelegate {
    let peripheral: CBPeripheral

    var onServicesDiscovered: ((Error?) -> Void)?
    var onCharacteristicsDiscovered: ((Error?) -> Void)?
    var onWriteCompleted: ((Error?) -> Void)?
    var onValueUpdated: ((Data?, Error?) -> Void)?

    init(_ peripheral: CBPeripheral) {
        self.peripheral = peripheral
        super.init()
        peripheral.delegate = self
    }

    // 缓存: ObjectIdentifier(CBPeripheral) → wrapper
    private static var cache: [ObjectIdentifier: YDKCoreBluetoothPeripheral] = [:]
    private static let cacheLock = NSLock()

    static func wrap(_ peripheral: CBPeripheral) -> YDKCoreBluetoothPeripheral {
        let key = ObjectIdentifier(peripheral)
        cacheLock.lock()
        defer { cacheLock.unlock() }
        if let existing = cache[key] { return existing }
        let wrapper = YDKCoreBluetoothPeripheral(peripheral)
        cache[key] = wrapper
        return wrapper
    }

    var identifier: UUID { peripheral.identifier }
    var name: String? { peripheral.name }

    var services: [YDKServiceManaging]? {
        peripheral.services?.map { YDKCoreBluetoothService($0) }
    }

    func discoverServices(_ serviceUUIDs: [CBUUID]?) {
        peripheral.discoverServices(serviceUUIDs)
    }

    func discoverCharacteristics(_ characteristicUUIDs: [CBUUID]?, for service: YDKServiceManaging) {
        guard let wrapper = service as? YDKCoreBluetoothService else { return }
        peripheral.discoverCharacteristics(characteristicUUIDs, for: wrapper.service)
    }

    func writeValue(_ data: Data, for characteristic: YDKCharacteristicManaging, type: CBCharacteristicWriteType) {
        guard let wrapper = characteristic as? YDKCoreBluetoothCharacteristic else { return }
        peripheral.writeValue(data, for: wrapper.characteristic, type: type)
    }

    func readValue(for characteristic: YDKCharacteristicManaging) {
        guard let wrapper = characteristic as? YDKCoreBluetoothCharacteristic else { return }
        peripheral.readValue(for: wrapper.characteristic)
    }

    // MARK: CBPeripheralDelegate

    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        onServicesDiscovered?(error)
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        onCharacteristicsDiscovered?(error)
    }

    func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        onWriteCompleted?(error)
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        onValueUpdated?(characteristic.value, error)
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateNotificationStateFor characteristic: CBCharacteristic, error: Error?) {
        onValueUpdated?(characteristic.value, error)
    }
}

/// CBService 包装
final class YDKCoreBluetoothService: YDKServiceManaging {
    let service: CBService

    init(_ service: CBService) {
        self.service = service
    }

    var uuid: CBUUID { service.uuid }

    var characteristics: [YDKCharacteristicManaging]? {
        service.characteristics?.map { YDKCoreBluetoothCharacteristic($0) }
    }
}

/// CBCharacteristic 包装
final class YDKCoreBluetoothCharacteristic: YDKCharacteristicManaging {
    let characteristic: CBCharacteristic

    init(_ characteristic: CBCharacteristic) {
        self.characteristic = characteristic
    }

    var uuid: CBUUID { characteristic.uuid }
    var value: Data? { characteristic.value }
}

// MARK: - BLEManager

/// iOS CoreBluetooth BLE 管理器
///
/// 负责:
/// 1. 扫描车辆 — 按协议 service UUID 过滤 (控制器级) + 广告包解析 (2b-A) 二次过滤
/// 2. 连接车辆 — 从扫描结果按 vehicleId 匹配 peripheral
/// 3. 发送解锁/锁车/启动指令 — 两阶段控制流 (写入 withResponse → 等待车辆 notify 响应)
/// 4. 读取车辆状态
///
/// 多协议支持: 通过 BleProtocolAdapter 工厂按协议创建适配器。
/// 测试: 通过 `init(central:)` 注入 fake central/peripheral (见 Tests)。
public final class YDKBLEManager: NSObject, YDKBLEManaging {

    // MARK: - 公开状态

    public private(set) var state: YDKBLEState = .unknown
    public private(set) var connectionState: YDKConnectionState = .disconnected

    /// BLE 连接状态变化回调 (0=disconnected 1=scanning 2=connecting 3=connected)
    public var connectionChangeHandler: ((Int32) -> Void)?

    // MARK: - 私有

    private let central: YDKCentralManaging
    private var peripheral: YDKPeripheralManaging?
    private var protocolType: YDKBleProtocolType = .ccc
    private var adapter: BleProtocolAdapter { BleProtocolAdapterFactory.makeAdapter(for: protocolType) }

    // GATT 特征
    private var controlChar: YDKCharacteristicManaging?
    private var statusChar: YDKCharacteristicManaging?

    // 扫描状态
    private var discoveredDevices: [UUID: VehicleAdvertise] = [:]
    private var peripheralByVehicleId: [String: YDKPeripheralManaging] = [:]
    private var scanContinuation: CheckedContinuation<[VehicleAdvertise], Error>?
    private var scanTimer: DispatchSourceTimer?
    private var isScanning = false

    // 连接/指令挂起
    private var connectContinuation: CheckedContinuation<Void, Error>?
    private var controlContinuation: CheckedContinuation<Void, Error>?
    private var readContinuation: CheckedContinuation<Data, Error>?
    private var responseContinuation: CheckedContinuation<Data, Error>?
    private var responseTimer: DispatchSourceTimer?

    /// 等待车辆指令响应的超时
    private static let responseTimeout: TimeInterval = 5

    /// 连接唤醒选项 (2b-I AD-3): 后台连接/断开时系统唤醒宿主 App。
    /// 对应 Apple: `CBConnectPeripheralOptionNotifyOnConnectionKey` /
    /// `CBConnectPeripheralOptionNotifyOnDisconnectionKey`。
    private static let connectWakeOptions: [String: Any] = [
        CBConnectPeripheralOptionNotifyOnConnectionKey: true,
        CBConnectPeripheralOptionNotifyOnDisconnectionKey: true
    ]

    private let logger: YDKLogger

    // MARK: - 初始化

    /// 生产入口 (2b-I AD-1/AD-8): 传 `backgroundRestoreIdentifier` 时启用 CoreBluetooth
    /// state preservation & restoration — central 创建时携带
    /// `[CBCentralManagerOptionRestoreIdentifierKey: id]`, 宿主 App 还需在 Info.plist
    /// 声明 `UIBackgroundModes = [bluetooth-central]` (见 BLE-BACKGROUND-INTEGRATION.md)。
    public init(enableLogging: Bool = false, backgroundRestoreIdentifier: String? = nil) {
        self.logger = YDKLogger(enabled: enableLogging)
        let options = YDKBLEManager.centralOptions(backgroundRestoreIdentifier: backgroundRestoreIdentifier)
        self.central = YDKCoreBluetoothCentral(queue: .main, options: options)
        super.init()
        wireCallbacks(central)
    }

    /// 测试注入入口: 用 fake central 驱动扫描/连接逻辑
    init(central: YDKCentralManaging, enableLogging: Bool = false) {
        self.logger = YDKLogger(enabled: enableLogging)
        self.central = central
        super.init()
        wireCallbacks(central)
    }

    /// 测试注入入口 (2b-I B1.2): 用 central 工厂捕获生产路径的 options —
    /// 断言 `backgroundRestoreIdentifier` 是否被正确传给 central 创建。
    init(centralFactory: @escaping ([String: Any]?) -> YDKCentralManaging,
         enableLogging: Bool = false,
         backgroundRestoreIdentifier: String? = nil) {
        self.logger = YDKLogger(enabled: enableLogging)
        let options = YDKBLEManager.centralOptions(backgroundRestoreIdentifier: backgroundRestoreIdentifier)
        self.central = centralFactory(options)
        super.init()
        wireCallbacks(central)
    }

    /// 构造 CBCentralManager options (2b-I AD-1): restore identifier 非空时启用状态恢复,
    /// 否则返回 nil (保持现有行为, options 缺省)。
    static func centralOptions(backgroundRestoreIdentifier: String?) -> [String: Any]? {
        guard let identifier = backgroundRestoreIdentifier, !identifier.isEmpty else { return nil }
        return [CBCentralManagerOptionRestoreIdentifierKey: identifier]
    }

    deinit {
        scanTimer?.cancel()
        responseTimer?.cancel()
    }

    /// 切换协议 (测试用)
    internal func setProtocol(_ type: YDKBleProtocolType) {
        protocolType = type
    }

    // MARK: - 回调接线

    private func wireCallbacks(_ central: YDKCentralManaging) {
        central.onStateChange = { [weak self] cbState in
            guard let self = self else { return }
            self.state = YDKBLEState(cbState: cbState)
            self.logger.log("BLE state: \(self.state.rawValue)")
        }

        central.onPeripheralDiscovered = { [weak self] peripheral, advertisementData, rssi in
            self?.handleDiscovered(peripheral, advertisementData: advertisementData, rssi: rssi)
        }

        central.onPeripheralConnected = { [weak self] peripheral in
            guard let self = self else { return }
            self.peripheral = peripheral
            self.configure(peripheral)
            self.connectionState = .discovering
            peripheral.discoverServices([self.adapter.protocolType.serviceUUID])
        }

        central.onPeripheralFailedToConnect = { [weak self] peripheral, error in
            guard let self = self else { return }
            self.failConnect(YDKError.networkError(error ?? YDKError.internal_("connect failed")))
        }

        central.onPeripheralDisconnected = { [weak self] peripheral, error in
            guard let self = self else { return }
            self.connectionState = .disconnected
            self.connectionChangeHandler?(0)
            let failure = YDKError.networkError(error ?? YDKError.internal_("disconnected"))
            self.connectContinuation?.resume(throwing: failure)
            self.connectContinuation = nil
            self.controlContinuation?.resume(throwing: failure)
            self.controlContinuation = nil
            self.readContinuation?.resume(throwing: failure)
            self.readContinuation = nil
            self.responseContinuation?.resume(throwing: failure)
            self.responseContinuation = nil
        }

        // 2b-I AD-4: 系统状态恢复 — 记录恢复的外设并复位连接状态,
        // 重连由 connectVehicle 现有回退路径 (peripheralByVehicleId + retrieveConnectedPeripherals) 负责
        central.onRestoreState = { [weak self] restoredPeripherals in
            self?.handleRestoredState(restoredPeripherals)
        }
    }

    /// 处理系统状态恢复 (2b-I AD-4):
    /// 1. 恢复的外设按 name / identifier 存入 `peripheralByVehicleId` — connectVehicle 可直接命中;
    /// 2. 连接状态复位为 disconnected (恢复不等同于已连接, 连接由 connectVehicle 显式发起)。
    /// 简单处理即可: 系统重建后 `centralManagerDidUpdateState` 会随后触发, 状态机照常推进。
    private func handleRestoredState(_ restoredPeripherals: [YDKPeripheralManaging]) {
        for peripheral in restoredPeripherals {
            if let name = peripheral.name, !name.isEmpty {
                peripheralByVehicleId[name] = peripheral
            }
            peripheralByVehicleId[peripheral.identifier.uuidString] = peripheral
        }
        connectionState = .disconnected
        connectionChangeHandler?(0)
        logger.log("Restored \(restoredPeripherals.count) peripheral(s) from system BLE state")
    }

    /// 为 peripheral 挂接回调 (同一 wrapper 幂等)
    private func configure(_ peripheral: YDKPeripheralManaging) {
        peripheral.onServicesDiscovered = { [weak self] error in
            self?.handleServicesDiscovered(error: error)
        }
        peripheral.onCharacteristicsDiscovered = { [weak self] error in
            self?.handleCharacteristicsDiscovered(error: error)
        }
        peripheral.onWriteCompleted = { [weak self] error in
            self?.handleWriteCompleted(error: error)
        }
        peripheral.onValueUpdated = { [weak self] value, error in
            self?.handleValueUpdated(value: value, error: error)
        }
    }

    // MARK: - 扫描

    public func scanVehicles(timeout: TimeInterval = 10) async throws -> [VehicleAdvertise] {
        guard state == .poweredOn else {
            throw YDKError.internal_("BLE not powered on, state=\(state.rawValue)")
        }

        discoveredDevices.removeAll()
        peripheralByVehicleId.removeAll()

        return try await withCheckedThrowingContinuation { continuation in
            self.scanContinuation = continuation
            self.connectionState = .scanning
            self.connectionChangeHandler?(1)

            // 控制器级过滤: 只扫描数字钥匙协议的 service UUID
            let serviceUUIDs = YDKBleProtocolType.allCases.map { $0.serviceUUID }
            central.scanForPeripherals(withServices: serviceUUIDs,
                                       options: [CBCentralManagerScanOptionAllowDuplicatesKey: false])
            isScanning = true

            let timer = DispatchSource.makeTimerSource(queue: .main)
            timer.schedule(deadline: .now() + timeout)
            timer.setEventHandler { [weak self] in
                guard let self = self else { return }
                self.stopScan()
                let results = Array(self.discoveredDevices.values)
                self.scanContinuation?.resume(returning: results)
                self.scanContinuation = nil
            }
            timer.resume()
            self.scanTimer = timer
        }
    }

    public func stopScan() {
        central.stopScan()
        scanTimer?.cancel()
        scanTimer = nil
        isScanning = false
        connectionState = .disconnected
        connectionChangeHandler?(0)
        // 提前停止时立即返回已发现结果
        if let continuation = scanContinuation {
            scanContinuation = nil
            continuation.resume(returning: Array(discoveredDevices.values))
        }
    }

    /// 广告包解析 + vehicleId 匹配 (2b-A)
    private func handleDiscovered(_ peripheral: YDKPeripheralManaging,
                                  advertisementData: [String: Any],
                                  rssi: Int) {
        for type in YDKBleProtocolType.allCases {
            let adapter = BleProtocolAdapterFactory.makeAdapter(for: type)
            if let vehicle = adapter.parseAdvertisement(advertisementData, rssi: rssi) {
                discoveredDevices[peripheral.identifier] = vehicle
                peripheralByVehicleId[vehicle.vehicleId] = peripheral
                logger.log("Discovered \(type) vehicle \(vehicle.vehicleId) rssi=\(rssi)")
                break
            }
        }
    }

    // MARK: - 连接

    public func connectVehicle(vehicleId: String) async throws {
        guard state == .poweredOn else {
            throw YDKError.internal_("BLE not powered on")
        }

        // 真实实现: 从扫描结果中按 vehicleId 匹配 peripheral
        if let matched = peripheralByVehicleId[vehicleId] {
            try await connect(matched)
            return
        }

        // 回退: 系统已连接外设 (后台恢复等场景), 按 identifier/name 匹配
        let connected = central.retrieveConnectedPeripherals(withServices: YDKBleProtocolType.allCases.map { $0.serviceUUID })
        for p in connected {
            if p.identifier.uuidString == vehicleId || p.name == vehicleId {
                try await connect(p)
                return
            }
        }

        throw YDKError.internal_("vehicle not found in scan results: \(vehicleId)")
    }

    private func connect(_ peripheral: YDKPeripheralManaging) async throws {
        self.peripheral = peripheral
        configure(peripheral)
        connectionState = .connecting
        connectionChangeHandler?(2)

        try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Void, Error>) in
            self.connectContinuation = continuation
            // 2b-I AD-3: 带连接唤醒选项 — 后台场景下连接/断开事件可唤醒宿主 App
            central.connect(peripheral, options: YDKBLEManager.connectWakeOptions)
        }
    }

    public func disconnect() async throws {
        guard let peripheral = peripheral else { return }
        central.cancelPeripheralConnection(peripheral)
        self.peripheral = nil
        controlChar = nil
        statusChar = nil
        connectionState = .disconnected
        connectionChangeHandler?(0)
    }

    private func failConnect(_ error: Error) {
        connectionState = .disconnected
        connectionChangeHandler?(0)
        connectContinuation?.resume(throwing: error)
        connectContinuation = nil
    }

    // MARK: - 控制指令

    public func unlock(vehicleId: String) async throws {
        try await sendControlCommand(type: .unlock, vehicleId: vehicleId)
    }

    public func lock(vehicleId: String) async throws {
        try await sendControlCommand(type: .lock, vehicleId: vehicleId)
    }

    public func startEngine(vehicleId: String) async throws {
        try await sendControlCommand(type: .engineOn, vehicleId: vehicleId)
    }

    public func readVehicleStatus(vehicleId: String) async throws -> VehicleStatus {
        guard let peripheral = peripheral, let statusChar = statusChar else {
            throw YDKError.internal_("not connected")
        }
        let data = try await read(from: statusChar, on: peripheral)
        return try adapter.parseVehicleStatus(data)
    }

    private func sendControlCommand(type: BleCommandType, vehicleId: String) async throws {
        guard let peripheral = peripheral, let controlChar = controlChar else {
            throw YDKError.internal_("not connected")
        }

        let session = SessionContext(keyId: "local-key", vehicleId: vehicleId)
        let command: Data
        switch type {
        case .unlock:   command = try adapter.buildUnlockCommand(keyId: session.keyId, session: session)
        case .lock:     command = try adapter.buildLockCommand(keyId: session.keyId, session: session)
        case .engineOn: command = try adapter.buildStartEngineCommand(keyId: session.keyId, session: session)
        default:        throw YDKError.internal_("unsupported command")
        }

        // 两阶段控制流: 写入指令 (withResponse) → 等待车辆 notify 响应 (AUTH_RESPONSE)
        try await write(command, to: controlChar, on: peripheral)
        let response = try await waitForResponse(timeout: YDKBLEManager.responseTimeout)
        let result = try adapter.parseCommandResponse(response)
        if !result.success {
            throw YDKError.hubError("BLE_CMD_\(result.errorCode)", result.errorMessage ?? "")
        }
    }

    /// 写入特征值, 等待 didWriteValueFor 完成
    private func write(_ data: Data,
                       to characteristic: YDKCharacteristicManaging,
                       on peripheral: YDKPeripheralManaging) async throws {
        try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Void, Error>) in
            self.controlContinuation = continuation
            peripheral.writeValue(data, for: characteristic, type: .withResponse)
        }
    }

    /// 读取特征值
    private func read(from characteristic: YDKCharacteristicManaging,
                      on peripheral: YDKPeripheralManaging) async throws -> Data {
        try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Data, Error>) in
            self.readContinuation = continuation
            peripheral.readValue(for: characteristic)
        }
    }

    /// 等待车辆 notify 响应 (带超时)
    private func waitForResponse(timeout: TimeInterval) async throws -> Data {
        try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Data, Error>) in
            self.responseContinuation = continuation
            let timer = DispatchSource.makeTimerSource(queue: .main)
            timer.schedule(deadline: .now() + timeout)
            timer.setEventHandler { [weak self] in
                guard let self = self else { return }
                self.responseTimer = nil
                self.responseContinuation?.resume(throwing: YDKError.timeout)
                self.responseContinuation = nil
            }
            timer.resume()
            self.responseTimer = timer
        }
    }

    // MARK: - Peripheral 回调处理

    private func handleServicesDiscovered(error: Error?) {
        if let error = error {
            failConnect(YDKError.networkError(error))
            return
        }
        guard let peripheral = peripheral else {
            failConnect(YDKError.internal_("missing peripheral"))
            return
        }
        guard let service = peripheral.services?.first(where: { $0.uuid == adapter.protocolType.serviceUUID }) else {
            failConnect(YDKError.internal_("service not found"))
            return
        }
        peripheral.discoverCharacteristics(nil, for: service)
    }

    private func handleCharacteristicsDiscovered(error: Error?) {
        if let error = error {
            failConnect(YDKError.networkError(error))
            return
        }
        guard let peripheral = peripheral else {
            failConnect(YDKError.internal_("missing peripheral"))
            return
        }

        // 保存关键特征 (CCC: FFD4 Auth Control 为控制通道, FFD5 State 为状态通道)
        for service in peripheral.services ?? [] {
            for characteristic in service.characteristics ?? [] {
                switch characteristic.uuid {
                case YDKBleUUIDs.cccAuthChar, YDKBleUUIDs.cccKeyDataChar, YDKBleUUIDs.icceControlCmdChar:
                    controlChar = characteristic
                case YDKBleUUIDs.cccStateChar, YDKBleUUIDs.icceKeyStatusChar:
                    statusChar = characteristic
                default:
                    break
                }
            }
        }

        connectionState = .connected
        connectionChangeHandler?(3)
        connectContinuation?.resume()
        connectContinuation = nil
    }

    private func handleWriteCompleted(error: Error?) {
        guard let continuation = controlContinuation else { return }
        controlContinuation = nil
        if let error = error {
            continuation.resume(throwing: YDKError.networkError(error))
        } else {
            continuation.resume()
        }
    }

    private func handleValueUpdated(value: Data?, error: Error?) {
        // 1) 指令响应等待中 (sendControlCommand 第二阶段)
        if let continuation = responseContinuation {
            responseTimer?.cancel()
            responseTimer = nil
            responseContinuation = nil
            if let error = error {
                continuation.resume(throwing: YDKError.networkError(error))
            } else {
                continuation.resume(returning: value ?? Data())
            }
            return
        }
        // 2) 状态读取等待中
        if let continuation = readContinuation {
            readContinuation = nil
            if let error = error {
                continuation.resume(throwing: YDKError.networkError(error))
            } else {
                continuation.resume(returning: value ?? Data())
            }
            return
        }
        // 3) 未跟踪的 notify (车辆主动上报)
        logger.log("Notify/update \(value?.count ?? 0) bytes")
    }
}
