import Foundation
import CoreBluetooth

/// BLE 错误类型
public enum BLEError: Error, LocalizedError {
    case bluetoothNotAvailable
    case bluetoothPoweredOff
    case deviceNotFound
    case connectionFailed
    case disconnected
    case timeout
    case invalidData
    case encryptionFailed
    case commandFailed(String)
    case unauthorized
    case unknown
    
    public var errorDescription: String? {
        switch self {
        case .bluetoothNotAvailable:
            return "蓝牙不可用"
        case .bluetoothPoweredOff:
            return "蓝牙已关闭"
        case .deviceNotFound:
            return "未找到设备"
        case .connectionFailed:
            return "连接失败"
        case .disconnected:
            return "已断开连接"
        case .timeout:
            return "操作超时"
        case .invalidData:
            return "无效数据"
        case .encryptionFailed:
            return "加密失败"
        case .commandFailed(let msg):
            return "命令执行失败: \(msg)"
        case .unauthorized:
            return "未授权"
        case .unknown:
            return "未知错误"
        }
    }
}

/// 车辆命令
public enum Command {
    case unlock
    case lock
    case startEngine
    case stopEngine
    case openTrunk
    case closeTrunk
    case openWindows
    case closeWindows
    case honk
    case flashLights
    case custom(data: Data)
    
    var commandCode: UInt8 {
        switch self {
        case .unlock: return 0x01
        case .lock: return 0x02
        case .startEngine: return 0x03
        case .stopEngine: return 0x04
        case .openTrunk: return 0x05
        case .closeTrunk: return 0x06
        case .openWindows: return 0x07
        case .closeWindows: return 0x08
        case .honk: return 0x09
        case .flashLights: return 0x0A
        case .custom: return 0xFF
        }
    }
    
    var data: Data {
        switch self {
        case .custom(let data):
            return data
        default:
            return Data([commandCode])
        }
    }
}

/// BLE 设备信息
public struct BLEDevice: Identifiable {
    public let id: String
    public let name: String?
    public let rssi: NSNumber
    public let advertisementData: [String: Any]
    public let peripheral: CBPeripheral
}

/// BLE 管理器
public final class BLEManager: NSObject {
    
    // MARK: - Singleton
    
    public static let shared = BLEManager()
    
    // MARK: - Properties
    
    private var centralManager: CBCentralManager!
    private var discoveredDevices: [String: BLEDevice] = [:]
    private var connectedPeripheral: CBPeripheral?
    private var targetServiceUUID: CBUUID?
    private var targetCharacteristicUUID: CBUUID?
    
    private var connectionCompletion: ((Result<Void, Error>) -> Void)?
    private var commandCompletion: ((Result<Void, Error>) -> Void)?
    private var scanCompletion: ((Result<[BLEDevice], Error>) -> Void)?
    
    private let queue = DispatchQueue(label: "com.yuledkcs.ble", qos: .userInitiated)
    private var commandTimeoutTimer: Timer?
    
    // MARK: - Service UUIDs
    
    private let vehicleServiceUUID = CBUUID(string: "A000")
    private let commandCharacteristicUUID = CBUUID(string: "A001")
    private let responseCharacteristicUUID = CBUUID(string: "A002")
    
    // MARK: - Initialization
    
    private override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: queue)
    }
    
    // MARK: - Public Methods
    
    /// 扫描附近的车辆设备
    /// - Parameters:
    ///   - timeout: 扫描超时时间（秒）
    ///   - completion: 回调
    public func scanForVehicles(timeout: TimeInterval = 10.0, completion: @escaping (Result<[BLEDevice], Error>) -> Void) {
        queue.async {
            guard self.centralManager.state == .poweredOn else {
                DispatchQueue.main.async {
                    completion(.failure(BLEError.bluetoothPoweredOff))
                }
                return
            }
            
            self.discoveredDevices.removeAll()
            self.scanCompletion = completion
            
            // 开始扫描
            self.centralManager.scanForPeripherals(
                withServices: [self.vehicleServiceUUID],
                options: [CBCentralManagerScanOptionAllowDuplicatesKey: false]
            )
            
            // 设置超时
            DispatchQueue.main.asyncAfter(deadline: .now() + timeout) { [weak self] in
                self?.stopScan()
                let devices = Array(self?.discoveredDevices.values ?? [])
                DispatchQueue.main.async {
                    self?.scanCompletion?(.success(devices))
                    self?.scanCompletion = nil
                }
            }
        }
    }
    
    /// 停止扫描
    public func stopScan() {
        queue.async {
            self.centralManager.stopScan()
        }
    }
    
    /// 连接到车辆设备
    /// - Parameters:
    ///   - deviceId: 设备 ID
    ///   - completion: 回调
    public func connect(to deviceId: String, completion: @escaping (Result<Void, Error>) -> Void) {
        queue.async {
            do {
                try YuleDKCS.shared.checkInitialized()
                
                guard self.centralManager.state == .poweredOn else {
                    DispatchQueue.main.async {
                        completion(.failure(BLEError.bluetoothPoweredOff))
                    }
                    return
                }
                
                guard let device = self.discoveredDevices[deviceId] else {
                    DispatchQueue.main.async {
                        completion(.failure(BLEError.deviceNotFound))
                    }
                    return
                }
                
                self.connectionCompletion = completion
                self.centralManager.connect(device.peripheral, options: nil)
                
                // 设置连接超时
                DispatchQueue.main.asyncAfter(deadline: .now() + 10.0) { [weak self] in
                    if self?.connectedPeripheral == nil {
                        self?.centralManager.cancelPeripheralConnection(device.peripheral)
                        DispatchQueue.main.async {
                            self?.connectionCompletion?(.failure(BLEError.timeout))
                            self?.connectionCompletion = nil
                        }
                    }
                }
                
            } catch {
                DispatchQueue.main.async {
                    completion(.failure(error))
                }
            }
        }
    }
    
    /// 断开连接
    public func disconnect() {
        queue.async {
            guard let peripheral = self.connectedPeripheral else { return }
            self.centralManager.cancelPeripheralConnection(peripheral)
        }
    }
    
    /// 发送命令到车辆
    /// - Parameters:
    ///   - command: 命令
    ///   - completion: 回调
    public func sendCommand(_ command: Command, completion: @escaping (Result<Void, Error>) -> Void) {
        queue.async {
            do {
                try YuleDKCS.shared.checkInitialized()
                
                guard let peripheral = self.connectedPeripheral else {
                    DispatchQueue.main.async {
                        completion(.failure(BLEError.disconnected))
                    }
                    return
                }
                
                guard peripheral.state == .connected else {
                    DispatchQueue.main.async {
                        completion(.failure(BLEError.disconnected))
                    }
                    return
                }
                
                // 加密命令数据
                let encryptedData = try self.encryptCommand(command)
                
                self.commandCompletion = completion
                
                // 查找特征值并写入
                guard let characteristic = self.findCommandCharacteristic() else {
                    DispatchQueue.main.async {
                        completion(.failure(BLEError.invalidData))
                    }
                    return
                }
                
                peripheral.writeValue(encryptedData, for: characteristic, type: .withResponse)
                
                // 设置命令超时
                self.commandTimeoutTimer?.invalidate()
                self.commandTimeoutTimer = Timer.scheduledTimer(withTimeInterval: 5.0, repeats: false) { [weak self] _ in
                    DispatchQueue.main.async {
                        self?.commandCompletion?(.failure(BLEError.timeout))
                        self?.commandCompletion = nil
                    }
                }
                
            } catch {
                DispatchQueue.main.async {
                    completion(.failure(error))
                }
            }
        }
    }
    
    /// 检查是否已连接
    public var isConnected: Bool {
        return connectedPeripheral?.state == .connected
    }
    
    // MARK: - Private Methods
    
    private func encryptCommand(_ command: Command) throws -> Data {
        // 获取当前钥匙
        guard let key = KeyManager.shared.listKeys().first else {
            throw BLEError.unauthorized
        }
        
        // 使用加密包装器加密
        let commandData = command.data
        let result = CryptoWrapper.shared.encrypt(data: commandData, keyId: key.id)
        
        switch result {
        case .success(let encryptedData):
            // 添加协议头
            var packet = Data()
            packet.append(0xAA) // 协议头
            packet.append(UInt8(encryptedData.count))
            packet.append(contentsOf: encryptedData)
            
            // 计算并添加 CRC
            let crc = calculateCRC(data: packet)
            packet.append(crc)
            
            return packet
            
        case .failure(let error):
            throw error
        }
    }
    
    private func calculateCRC(data: Data) -> UInt8 {
        var crc: UInt8 = 0
        for byte in data {
            crc ^= byte
        }
        return crc
    }
    
    private func findCommandCharacteristic() -> CBCharacteristic? {
        guard let peripheral = connectedPeripheral else { return nil }
        
        for service in peripheral.services ?? [] {
            if service.uuid == vehicleServiceUUID {
                for characteristic in service.characteristics ?? [] {
                    if characteristic.uuid == commandCharacteristicUUID {
                        return characteristic
                    }
                }
            }
        }
        return nil
    }
}

// MARK: - CBCentralManagerDelegate

extension BLEManager: CBCentralManagerDelegate {
    
    public func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn:
            print("[YuleDKCS] Bluetooth is powered on")
        case .poweredOff:
            print("[YuleDKCS] Bluetooth is powered off")
        case .unauthorized:
            print("[YuleDKCS] Bluetooth unauthorized")
        case .unsupported:
            print("[YuleDKCS] Bluetooth unsupported")
        case .resetting:
            print("[YuleDKCS] Bluetooth resetting")
        case .unknown:
            print("[YuleDKCS] Bluetooth unknown state")
        @unknown default:
            print("[YuleDKCS] Bluetooth unknown state")
        }
    }
    
    public func centralManager(
        _ central: CBCentralManager,
        didDiscover peripheral: CBPeripheral,
        advertisementData: [String: Any],
        rssi RSSI: NSNumber
    ) {
        let device = BLEDevice(
            id: peripheral.identifier.uuidString,
            name: peripheral.name ?? advertisementData[CBAdvertisementDataLocalNameKey] as? String,
            rssi: RSSI,
            advertisementData: advertisementData,
            peripheral: peripheral
        )
        
        discoveredDevices[peripheral.identifier.uuidString] = device
        print("[YuleDKCS] Discovered device: \(device.name ?? "Unknown") (RSSI: \(RSSI))")
    }
    
    public func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        print("[YuleDKCS] Connected to \(peripheral.name ?? "Unknown")")
        connectedPeripheral = peripheral
        peripheral.delegate = self
        peripheral.discoverServices([vehicleServiceUUID])
    }
    
    public func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        print("[YuleDKCS] Failed to connect: \(error?.localizedDescription ?? "Unknown")")
        DispatchQueue.main.async {
            self.connectionCompletion?(.failure(error ?? BLEError.connectionFailed))
            self.connectionCompletion = nil
        }
    }
    
    public func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        print("[YuleDKCS] Disconnected from \(peripheral.name ?? "Unknown")")
        connectedPeripheral = nil
    }
}

// MARK: - CBPeripheralDelegate

extension BLEManager: CBPeripheralDelegate {
    
    public func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard error == nil else {
            print("[YuleDKCS] Error discovering services: \(error!.localizedDescription)")
            DispatchQueue.main.async {
                self.connectionCompletion?(.failure(error!))
                self.connectionCompletion = nil
            }
            return
        }
        
        guard let services = peripheral.services else { return }
        
        for service in services {
            if service.uuid == vehicleServiceUUID {
                peripheral.discoverCharacteristics(
                    [commandCharacteristicUUID, responseCharacteristicUUID],
                    for: service
                )
            }
        }
    }
    
    public func peripheral(
        _ peripheral: CBPeripheral,
        didDiscoverCharacteristicsFor service: CBService,
        error: Error?
    ) {
        guard error == nil else {
            print("[YuleDKCS] Error discovering characteristics: \(error!.localizedDescription)")
            DispatchQueue.main.async {
                self.connectionCompletion?(.failure(error!))
                self.connectionCompletion = nil
            }
            return
        }
        
        // 连接成功
        DispatchQueue.main.async {
            self.connectionCompletion?(.success(()))
            self.connectionCompletion = nil
        }
    }
    
    public func peripheral(
        _ peripheral: CBPeripheral,
        didWriteValueFor characteristic: CBCharacteristic,
        error: Error?
    ) {
        commandTimeoutTimer?.invalidate()
        
        DispatchQueue.main.async {
            if let error = error {
                self.commandCompletion?(.failure(error))
            } else {
                self.commandCompletion?(.success(()))
            }
            self.commandCompletion = nil
        }
    }
    
    public func peripheral(
        _ peripheral: CBPeripheral,
        didUpdateValueFor characteristic: CBCharacteristic,
        error: Error?
    ) {
        guard let data = characteristic.value else { return }
        
        // 处理响应数据
        print("[YuleDKCS] Received response: \(data.hexString)")
    }
}

// MARK: - Data Extension

extension Data {
    var hexString: String {
        return map { String(format: "%02X", $0) }.joined(separator: " ")
    }
}