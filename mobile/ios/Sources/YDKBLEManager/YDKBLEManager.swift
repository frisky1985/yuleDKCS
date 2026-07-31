import Foundation
import CoreBluetooth

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

// MARK: - BLEManager

/// iOS CoreBluetooth BLE 管理器
///
/// 负责:
/// 1. 扫描车辆（解析广告包）
/// 2. 连接车辆（GATT 服务发现）
/// 3. 发送解锁/锁车/启动指令
/// 4. 读取车辆状态
///
/// 多协议支持: 通过 BleProtocolAdapter 工厂按协议创建适配器。
public final class YDKBLEManager: NSObject, YDKBLEManaging {

    // MARK: - 公开状态

    public private(set) var state: YDKBLEState = .unknown
    public private(set) var connectionState: YDKConnectionState = .disconnected

    /// BLE 连接状态变化回调 (0=disconnected 1=scanning 2=connecting 3=connected)
    public var connectionChangeHandler: ((Int32) -> Void)?

    // MARK: - 私有

    private var centralManager: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var protocolType: YDKBleProtocolType = .ccc
    private var adapter: BleProtocolAdapter { BleProtocolAdapterFactory.makeAdapter(for: protocolType) }

    // GATT 特征
    private var controlChar: CBCharacteristic?
    private var statusChar: CBCharacteristic?
    private var notifyChar: CBCharacteristic?

    // 扫描状态
    private var discoveredDevices: [String: VehicleAdvertise] = [:]
    private var scanContinuation: CheckedContinuation<[VehicleAdvertise], Error>?
    private var connectContinuation: CheckedContinuation<Void, Error>?
    private var controlContinuation: CheckedContinuation<Data, Error>?
    private var scanTimer: DispatchSourceTimer?

    private let logger: YDKLogger

    // MARK: - 初始化

    public init(enableLogging: Bool = false) {
        self.logger = YDKLogger(enabled: enableLogging)
        super.init()
        self.centralManager = CBCentralManager(delegate: self, queue: .main)
    }

    // MARK: - 扫描

    public func scanVehicles(timeout: TimeInterval = 10) async throws -> [VehicleAdvertise] {
        guard state == .poweredOn else {
            throw YDKError.internal_("BLE not powered on, state=\(state.rawValue)")
        }

        discoveredDevices.removeAll()

        return try await withCheckedThrowingContinuation { continuation in
            self.scanContinuation = continuation
            centralManager.scanForPeripherals(withServices: nil, options: [CBCentralManagerScanOptionAllowDuplicatesKey: false])

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
        centralManager.stopScan()
        scanTimer?.cancel()
        scanTimer = nil
        connectionChangeHandler?(1)
    }

    // MARK: - 连接

    public func connectVehicle(vehicleId: String) async throws {
        guard state == .poweredOn else {
            throw YDKError.internal_("BLE not powered on")
        }
        // 简化: vehicleId → 需要扫描找到对应 peripheral
        // 完整实现: 扫描后按 vehicleId 匹配，此处占位
        guard let peripheral = centralManager.retrieveConnectedPeripherals(withServices: [adapter.protocolType.serviceUUID]).first else {
            throw YDKError.internal_("no connected peripheral for \(vehicleId)")
        }

        self.peripheral = peripheral
        connectionState = .connecting
        connectionChangeHandler?(2)

        try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Void, Error>) in
            self.connectContinuation = continuation
            peripheral.delegate = self
            centralManager.connect(peripheral, options: nil)
        }
    }

    public func disconnect() async throws {
        guard let peripheral = peripheral else { return }
        centralManager.cancelPeripheralConnection(peripheral)
        self.peripheral = nil
        connectionState = .disconnected
        connectionChangeHandler?(0)
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

    // MARK: - 内部

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

        let response = try await write(command, to: controlChar, on: peripheral)
        let result = try adapter.parseCommandResponse(response)
        if !result.success {
            throw YDKError.hubError("BLE_CMD_\(result.errorCode)", result.errorMessage ?? "")
        }
    }

    /// 写入并等待响应
    private func write(_ data: Data, to characteristic: CBCharacteristic, on peripheral: CBPeripheral) async throws -> Data {
        try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Data, Error>) in
            self.controlContinuation = continuation
            peripheral.writeValue(data, for: characteristic, type: .withResponse)
        }
    }

    /// 读取特征值
    private func read(from characteristic: CBCharacteristic, on peripheral: CBPeripheral) async throws -> Data {
        try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Data, Error>) in
            self.controlContinuation = continuation
            peripheral.readValue(for: characteristic)
        }
    }
}

// MARK: - CBCentralManagerDelegate

extension YDKBLEManager: CBCentralManagerDelegate {

    public func centralManagerDidUpdateState(_ central: CBCentralManager) {
        state = YDKBLEState(cbState: central.state)
        logger.log("BLE state: \(state.rawValue)")
    }

    public func centralManager(_ central: CBCentralManager,
                               didDiscover peripheral: CBPeripheral,
                               advertisementData: [String: Any],
                               rssi RSSI: NSNumber) {
        // 按协议解析广告包
        for type in [YDKBleProtocolType.ccc, .iccoa, .icce] {
            let adapter = BleProtocolAdapterFactory.makeAdapter(for: type)
            if let vehicle = adapter.parseAdvertisement(advertisementData, rssi: RSSI.intValue) {
                discoveredDevices[peripheral.identifier.uuidString] = vehicle
                break
            }
        }
    }

    public func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        connectionState = .discovering
        peripheral.discoverServices([adapter.protocolType.serviceUUID])
    }

    public func centralManager(_ central: CBCentralManager,
                               didFailToConnect peripheral: CBPeripheral,
                               error: Error?) {
        connectionState = .disconnected
        connectionChangeHandler?(0)
        connectContinuation?.resume(throwing: YDKError.networkError(error ?? YDKError.internal_("connect failed")))
        connectContinuation = nil
    }

    public func centralManager(_ central: CBCentralManager,
                               didDisconnectPeripheral peripheral: CBPeripheral,
                               error: Error?) {
        connectionState = .disconnected
        connectionChangeHandler?(0)
    }
}

// MARK: - CBPeripheralDelegate

extension YDKBLEManager: CBPeripheralDelegate {

    public func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard error == nil else {
            connectContinuation?.resume(throwing: YDKError.networkError(error!))
            connectContinuation = nil
            return
        }

        guard let service = peripheral.services?.first(where: { $0.uuid == adapter.protocolType.serviceUUID }) else {
            connectContinuation?.resume(throwing: YDKError.internal_("service not found"))
            connectContinuation = nil
            return
        }

        peripheral.discoverCharacteristics(nil, for: service)
    }

    public func peripheral(_ peripheral: CBPeripheral,
                           didDiscoverCharacteristicsFor service: CBService,
                           error: Error?) {
        guard error == nil else {
            connectContinuation?.resume(throwing: YDKError.networkError(error!))
            connectContinuation = nil
            return
        }

        // 保存关键特征
        for characteristic in service.characteristics ?? [] {
            switch characteristic.uuid {
            case YDKBleUUIDs.cccKeyDataChar, YDKBleUUIDs.icceControlCmdChar:
                controlChar = characteristic
            case YDKBleUUIDs.cccStateChar, YDKBleUUIDs.icceKeyStatusChar:
                statusChar = characteristic
            default:
                break
            }
        }

        connectionState = .connected
        connectionChangeHandler?(3)
        connectContinuation?.resume()
        connectContinuation = nil
    }

    public func peripheral(_ peripheral: CBPeripheral,
                           didWriteValueFor characteristic: CBCharacteristic,
                           error: Error?) {
        guard let continuation = controlContinuation else { return }
        controlContinuation = nil
        if let error = error {
            continuation.resume(throwing: YDKError.networkError(error))
        } else {
            // 简化: 写入后立即返回空 Data（真实实现需等待 notify）
            continuation.resume(returning: Data([0x00]))
        }
    }

    public func peripheral(_ peripheral: CBPeripheral,
                           didUpdateValueFor characteristic: CBCharacteristic,
                           error: Error?) {
        guard let value = characteristic.value else { return }
        // notify 数据（车辆状态等）
        logger.log("Notify \(characteristic.uuid): \(value.count) bytes")
    }
}
