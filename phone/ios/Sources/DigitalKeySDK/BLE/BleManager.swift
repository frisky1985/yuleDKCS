// BleManager.swift
// BLE蓝牙管理器
// 负责BLE设备的扫描、连接、通信和断开

import Foundation
import CoreBluetooth
import Combine

// MARK: - BLE State

/// BLE状态枚举
public enum BleState: String, CustomStringConvertible {
    case unknown = "unknown"
    case resetting = "resetting"
    case unsupported = "unsupported"
    case unauthorized = "unauthorized"
    case poweredOff = "poweredOff"
    case poweredOn = "poweredOn"

    public var description: String { rawValue }

    public init(cbState: CBManagerState) {
        switch cbState {
        case .unknown: self = .unknown
        case .resetting: self = .resetting
        case .unsupported: self = .unsupported
        case .unauthorized: self = .unauthorized
        case .poweredOff: self = .poweredOff
        case .poweredOn: self = .poweredOn
        @unknown default: self = .unknown
        }
    }
}

// MARK: - BLE Device

/// BLE设备信息
public struct BleDevice: Identifiable, Equatable {
    public let id: String          // UUID
    public let name: String?
    public let rssi: Int
    public let advertisementData: [String: Any]
    public let discoveredAt: Date

    public init(peripheral: CBPeripheral, rssi: Int, advertisementData: [String: Any]) {
        self.id = peripheral.identifier.uuidString
        self.name = peripheral.name
        self.rssi = rssi
        self.advertisementData = advertisementData
        self.discoveredAt = Date()
    }

    public static func == (lhs: BleDevice, rhs: BleDevice) -> Bool {
        lhs.id == rhs.id
    }
}

// MARK: - BLE Connection State

/// BLE连接状态
public enum BleConnectionState: String, CustomStringConvertible {
    case disconnected = "disconnected"
    case scanning = "scanning"
    case connecting = "connecting"
    case connected = "connected"
    case discovering = "discovering"
    case ready = "ready"
    case disconnecting = "disconnecting"

    public var description: String { rawValue }
}

// MARK: - BLE Service UUIDs

/// 数字钥匙相关BLE Service/Characteristic UUID
public struct BleUUIDs {
    // 数字钥匙服务
    public static let digitalKeyService = CBUUID(string: "FDE2")
    // 车辆信息服务
    public static let vehicleInfoService = CBUUID(string: "FDE3")

    // 写入特征（手机→车）
    public static let writeCharacteristic = CBUUID(string: "FDE4")
    // 通知特征（车→手机）
    public static let notifyCharacteristic = CBUUID(string: "FDE5")
    // 配对特征
    public static let pairingCharacteristic = CBUUID(string: "FDE6")
    // 车辆状态特征
    public static let vehicleStatusCharacteristic = CBUUID(string: "FDE7")

    // 标准设备信息服务
    public static let deviceInfoService = CBUUID(string: "180A")
    public static let manufacturerName = CBUUID(string: "2A00")
    public static let modelNumber = CBUUID(string: "2A24")
    public static let firmwareRevision = CBUUID(string: "2A26")
}

// MARK: - BLE Protocol

/// BLE管理协议
public protocol BleManaging: AnyObject {
    var state: BleState { get }
    var connectionState: BleConnectionState { get }
    var connectedDevice: BleDevice? { get }
    var statePublisher: AnyPublisher<BleState, Never> { get }
    var connectionStatePublisher: AnyPublisher<BleConnectionState, Never> { get }
    var dataReceivedPublisher: AnyPublisher<Data, Never> { get }

    func startScan(serviceUUIDs: [CBUUID]?, timeout: TimeInterval) async throws -> [BleDevice]
    func stopScan()
    func connect(_ device: BleDevice) async throws
    func disconnect()
    func send(_ data: Data) async throws
    func send(_ data: Data, withResponse: Bool) async throws
    func requestMtu(_ mtu: Int) -> Int
}

// MARK: - BleManager

/// BLE蓝牙管理器
public final class BleManager: NSObject, BleManaging, CBCentralManagerDelegate, CBPeripheralDelegate {

    // MARK: - Published State

    public private(set) var state: BleState = .unknown
    public private(set) var connectionState: BleConnectionState = .disconnected
    public private(set) var connectedDevice: BleDevice?

    // MARK: - Publishers

    private let _stateSubject = CurrentValueSubject<BleState, Never>(.unknown)
    public var statePublisher: AnyPublisher<BleState, Never> { _stateSubject.eraseToAnyPublisher() }

    private let _connectionStateSubject = CurrentValueSubject<BleConnectionState, Never>(.disconnected)
    public var connectionStatePublisher: AnyPublisher<BleConnectionState, Never> { _connectionStateSubject.eraseToAnyPublisher() }

    private let _dataReceivedSubject = PassthroughSubject<Data, Never>()
    public var dataReceivedPublisher: AnyPublisher<Data, Never> { _dataReceivedSubject.eraseToAnyPublisher() }

    // MARK: - Private

    private var centralManager: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var writeCharacteristic: CBCharacteristic?
    private var notifyCharacteristic: CBCharacteristic?
    private var pairingCharacteristic: CBCharacteristic?

    private var discoveredDevices: [String: BleDevice] = [:]
    private var scanContinuation: CheckedContinuation<[BleDevice], Error>?
    private var connectContinuation: CheckedContinuation<Void, Error>?
    private var sendContinuation: CheckedContinuation<Void, Error>?
    private var discoverContinuation: CheckedContinuation<Void, Error>?

    private var scanTimer: Timer?
    private var cancellables = Set<AnyCancellable>()

    private let logger = DkLogger.shared
    private let telemetry = DkTelemetry.shared

    // MTU
    private var negotiatedMtu: Int = 23

    // Write queue for serializing BLE writes
    private let writeQueue = DispatchQueue(label: "com.digitalkey.ble.write", qos: .userInitiated)
    private var pendingWrites: [(Data, Bool, CheckedContinuation<Void, Error>)] = []
    private var isWriting = false

    // MARK: - Initialization

    public override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: nil, options: [
            CBCentralManagerOptionShowPowerAlertKey: false
        ])
    }

    public convenience init(sdk: DigitalKeySDK) {
        self.init()
    }

    // MARK: - Scan

    public func startScan(serviceUUIDs: [CBUUID]? = nil, timeout: TimeInterval = 10.0) async throws -> [BleDevice] {
        guard state == .poweredOn else {
            throw DigitalKeyError(code: DkErrorCode.bleDisabled, message: "BLE未开启，当前状态: \(state)")
        }

        discoveredDevices.removeAll()
        updateConnectionState(.scanning)
        logger.ble("开始扫描BLE设备, serviceUUIDs=\(serviceUUIDs?.map { $0.uuidString } ?? ["all"]), timeout=\(timeout)s")
        telemetry.track(DkEventType.bleScan)

        return try await withCheckedThrowingContinuation { continuation in
            self.scanContinuation = continuation
            centralManager.scanForPeripherals(
                withServices: serviceUUIDs ?? [BleUUIDs.digitalKeyService, BleUUIDs.vehicleInfoService],
                options: [CBCentralManagerScanOptionAllowDuplicatesKey: true]
            )
            // Timeout
            DispatchQueue.main.asyncAfter(deadline: .now() + timeout) { [weak self] in
                self?.handleScanTimeout()
            }
        }
    }

    private func handleScanTimeout() {
        guard let continuation = scanContinuation else { return }
        centralManager.stopScan()
        scanContinuation = nil
        let devices = Array(discoveredDevices.values)
        updateConnectionState(.disconnected)
        logger.ble("BLE扫描超时, 发现\(devices.count)个设备")
        continuation.resume(returning: devices)
    }

    public func stopScan() {
        centralManager.stopScan()
        if let continuation = scanContinuation {
            scanContinuation = nil
            let devices = Array(discoveredDevices.values)
            updateConnectionState(.disconnected)
            continuation.resume(returning: devices)
        }
    }

    // MARK: - Connect

    public func connect(_ device: BleDevice) async throws {
        guard state == .poweredOn else {
            throw DigitalKeyError(code: DkErrorCode.bleDisabled, message: "BLE未开启")
        }

        guard let targetPeripheral = centralManager.retrievePeripherals(withIdentifiers: [UUID(uuidString: device.id)!]).first else {
            throw DigitalKeyError(code: DkErrorCode.deviceNotFound, message: "找不到BLE设备: \(device.id)")
        }

        logger.ble("正在连接BLE设备: \(device.name ?? device.id)")
        updateConnectionState(.connecting)

        return try await withCheckedThrowingContinuation { continuation in
            self.connectContinuation = continuation
            self.peripheral = targetPeripheral
            centralManager.connect(targetPeripheral, options: [
                CBConnectPeripheralOptionNotifyOnDisconnectionKey: true
            ])
        }
    }

    public func disconnect() {
        if let peripheral = peripheral {
            updateConnectionState(.disconnecting)
            centralManager.cancelPeripheralConnection(peripheral)
        }
    }

    // MARK: - Send Data

    public func send(_ data: Data) async throws {
        try await send(data, withResponse: true)
    }

    public func send(_ data: Data, withResponse: Bool = true) async throws {
        guard connectionState == .ready || connectionState == .connected else {
            throw DigitalKeyError(code: DkErrorCode.bleConnectFailed, message: "BLE未连接，无法发送数据")
        }
        guard let characteristic = writeCharacteristic else {
            throw DigitalKeyError(code: DkErrorCode.bleConnectFailed, message: "写入特征未就绪")
        }

        return try await withCheckedThrowingContinuation { continuation in
            writeQueue.async { [weak self] in
                guard let self = self else {
                    continuation.resume(throwing: DigitalKeyError(code: DkErrorCode.internalError, message: "BleManager已释放"))
                    return
                }
                self.pendingWrites.append((data, withResponse, continuation))
                self.processNextWrite()
            }
        }
    }

    private func processNextWrite() {
        guard !isWriting, !pendingWrites.isEmpty else { return }
        isWriting = true

        let (data, withResponse, continuation) = pendingWrites.removeFirst()
        guard let peripheral = peripheral, let characteristic = writeCharacteristic else {
            isWriting = false
            continuation.resume(throwing: DigitalKeyError(code: DkErrorCode.bleConnectFailed, message: "BLE未连接"))
            processNextWrite()
            return
        }

        let writeType: CBCharacteristicWriteType = withResponse ? .withResponse : .withoutResponse
        // Fragment by MTU
        let maxPayload = negotiatedMtu - 3
        if data.count <= maxPayload {
            peripheral.writeValue(data, for: characteristic, type: writeType)
            // For withResponse, the delegate callback will resume
            // For withoutResponse, resume immediately
            if !withResponse {
                isWriting = false
                continuation.resume()
                processNextWrite()
            } else {
                self.sendContinuation = continuation
            }
        } else {
            // Fragmented write
            var offset = 0
            self.writeFragments(data: data, offset: offset, maxPayload: maxPayload,
                                peripheral: peripheral, characteristic: characteristic,
                                writeType: writeType, continuation: continuation)
        }
    }

    private func writeFragments(data: Data, offset: Int, maxPayload: Int,
                                 peripheral: CBPeripheral, characteristic: CBCharacteristic,
                                 writeType: CBCharacteristicWriteType,
                                 continuation: CheckedContinuation<Void, Error>) {
        let end = min(offset + maxPayload, data.count)
        let chunk = data.subdata(in: offset..<end)
        let isLast = end >= data.count

        peripheral.writeValue(chunk, for: characteristic, type: isLast ? writeType : .withoutResponse)

        if isLast {
            if writeType == .withResponse {
                self.sendContinuation = continuation
            } else {
                isWriting = false
                continuation.resume()
                processNextWrite()
            }
        } else {
            // Small delay between fragments
            DispatchQueue.global(qos: .userInitiated).asyncAfter(deadline: .now() + 0.01) { [weak self] in
                self?.writeFragments(data: data, offset: end, maxPayload: maxPayload,
                                      peripheral: peripheral, characteristic: characteristic,
                                      writeType: writeType, continuation: continuation)
            }
        }
    }

    // MARK: - MTU

    public func requestMtu(_ mtu: Int) -> Int {
        // iOS handles MTU negotiation automatically
        // The actual MTU is determined during connection
        negotiatedMtu = peripheral?.maximumWriteValueLength(for: .withResponse) ?? 23
        logger.ble("当前MTU: \(negotiatedMtu)")
        return negotiatedMtu
    }

    // MARK: - CBCentralManagerDelegate

    public func centralManagerDidUpdateState(_ central: CBCentralManager) {
        let newState = BleState(cbState: central.state)
        state = newState
        _stateSubject.send(newState)
        logger.ble("BLE状态变更: \(newState)")

        if newState != .poweredOn {
            // If not powered on, fail any pending operations
            if let continuation = scanContinuation {
                scanContinuation = nil
                centralManager.stopScan()
                continuation.resume(throwing: DigitalKeyError(code: DkErrorCode.bleDisabled, message: "BLE状态: \(newState)"))
            }
            if let continuation = connectContinuation {
                connectContinuation = nil
                continuation.resume(throwing: DigitalKeyError(code: DkErrorCode.bleDisabled, message: "BLE状态: \(newState)"))
            }
        }
    }

    public func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral,
                                advertisementData: [String: Any], rssi RSSI: NSNumber) {
        let device = BleDevice(peripheral: peripheral, rssi: RSSI.intValue, advertisementData: advertisementData)
        discoveredDevices[device.id] = device
        logger.trace("发现BLE设备: \(peripheral.name ?? "未知") RSSI=\(RSSI)", tag: .ble)
    }

    public func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        logger.ble("BLE已连接: \(peripheral.name ?? peripheral.identifier.uuidString)")
        self.peripheral = peripheral
        peripheral.delegate = self
        updateConnectionState(.connected)

        // Start service discovery
        updateConnectionState(.discovering)
        peripheral.discoverServices([BleUUIDs.digitalKeyService, BleUUIDs.vehicleInfoService, BleUUIDs.deviceInfoService])
    }

    public func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        logger.bleE("BLE连接失败", error: error)
        telemetry.track(DkEventType.bleConnect, properties: ["success": false, "error": error?.localizedDescription ?? "unknown"])
        updateConnectionState(.disconnected)

        if let continuation = connectContinuation {
            connectContinuation = nil
            let dkError = DigitalKeyError(code: DkErrorCode.bleConnectFailed,
                                          message: "BLE连接失败: \(error?.localizedDescription ?? "未知错误")",
                                          cause: error)
            continuation.resume(throwing: dkError)
        }
    }

    public func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        logger.ble("BLE已断开: \(peripheral.name ?? peripheral.identifier.uuidString)")
        updateConnectionState(.disconnected)
        self.peripheral = nil
        self.writeCharacteristic = nil
        self.notifyCharacteristic = nil
        self.pairingCharacteristic = nil
        self.connectedDevice = nil
        self.negotiatedMtu = 23

        // Fail pending writes
        isWriting = false
        for (_, _, continuation) in pendingWrites {
            continuation.resume(throwing: DigitalKeyError(code: DkErrorCode.bleConnectFailed, message: "BLE已断开"))
        }
        pendingWrites.removeAll()
    }

    // MARK: - CBPeripheralDelegate

    public func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        if let error = error {
            logger.bleE("服务发现失败", error: error)
            failDiscover(error)
            return
        }

        guard let services = peripheral.services else {
            failDiscover(DigitalKeyError(code: DkErrorCode.bleConnectFailed, message: "未发现服务"))
            return
        }

        logger.ble("发现\(services.count)个服务")

        for service in services {
            logger.trace("服务: \(service.uuid.uuidString)", tag: .ble)
            peripheral.discoverCharacteristics(nil, for: service)
        }
    }

    public func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        if let error = error {
            logger.bleE("特征发现失败: \(service.uuid.uuidString)", error: error)
            return
        }

        guard let characteristics = service.characteristics else { return }

        for characteristic in characteristics {
            logger.trace("特征: \(characteristic.uuid.uuidString) properties=\(characteristic.properties)", tag: .ble)

            switch characteristic.uuid {
            case BleUUIDs.writeCharacteristic:
                writeCharacteristic = characteristic
            case BleUUIDs.notifyCharacteristic:
                notifyCharacteristic = characteristic
                peripheral.setNotifyValue(true, for: characteristic)
            case BleUUIDs.pairingCharacteristic:
                pairingCharacteristic = characteristic
            case BleUUIDs.vehicleStatusCharacteristic:
                peripheral.setNotifyValue(true, for: characteristic)
            default:
                break
            }
        }

        // Check if all required characteristics are found
        if writeCharacteristic != nil && notifyCharacteristic != nil {
            negotiatedMtu = peripheral.maximumWriteValueLength(for: .withResponse)
            updateConnectionState(.ready)
            logger.ble("BLE就绪, MTU=\(negotiatedMtu)")

            if let continuation = discoverContinuation {
                discoverContinuation = nil
                continuation.resume()
            }
            if let continuation = connectContinuation {
                connectContinuation = nil
                continuation.resume()
            }

            telemetry.track(DkEventType.bleConnect, properties: [
                "success": true,
                "mtu": negotiatedMtu
            ])
        }
    }

    public func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        if let error = error {
            logger.bleE("读取特征值失败: \(characteristic.uuid.uuidString)", error: error)
            return
        }

        guard let data = characteristic.value else { return }
        logger.proto("收到BLE数据: \(data.count)字节, 特征=\(characteristic.uuid.uuidString)")
        _dataReceivedSubject.send(data)
    }

    public func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        isWriting = false

        if let error = error {
            logger.bleE("写入特征值失败", error: error)
            if let continuation = sendContinuation {
                sendContinuation = nil
                continuation.resume(throwing: DigitalKeyError(code: DkErrorCode.bleConnectFailed,
                                                               message: "写入失败", cause: error))
            }
        } else {
            if let continuation = sendContinuation {
                sendContinuation = nil
                continuation.resume()
            }
        }

        processNextWrite()
    }

    public func peripheral(_ peripheral: CBPeripheral, didUpdateNotificationStateFor characteristic: CBCharacteristic, error: Error?) {
        if let error = error {
            logger.bleE("通知订阅失败: \(characteristic.uuid.uuidString)", error: error)
        } else {
            logger.ble("通知已订阅: \(characteristic.uuid.uuidString), isNotifying=\(characteristic.isNotifying)")
        }
    }

    // MARK: - Helper Methods

    private func updateConnectionState(_ newState: BleConnectionState) {
        connectionState = newState
        _connectionStateSubject.send(newState)
    }

    private func failDiscover(_ error: Error) {
        updateConnectionState(.disconnected)
        if let continuation = connectContinuation {
            connectContinuation = nil
            continuation.resume(throwing: error)
        }
        if let continuation = discoverContinuation {
            discoverContinuation = nil
            continuation.resume(throwing: error)
        }
    }
}
