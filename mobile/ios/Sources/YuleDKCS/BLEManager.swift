import Foundation
import CoreBluetooth
import Combine

/// BLE Manager for vehicle communication
///
/// Handles Bluetooth Low Energy connections, command sending, scanning,
/// and real-time status monitoring for vehicles.
public class BLEManager: NSObject, ObservableObject {
    
    public static let shared = BLEManager()
    
    // MARK: - Published Properties
    
    @Published public private(set) var scanResults: [ScanResult] = []
    @Published public private(set) var isScanning = false
    @Published public private(set) var connectionState: ConnectionState = .disconnected
    @Published public private(set) var vehicleStatus: VehicleStatus?
    
    // MARK: - BLE UUIDs
    
    public static let serviceUUID = CBUUID(string: "0000yule-0000-1000-8000-00805f9b34fb")
    public static let commandCharUUID = CBUUID(string: "0000cmd1-0000-1000-8000-00805f9b34fb")
    public static let responseCharUUID = CBUUID(string: "0000resp-0000-1000-8000-00805f9b34fb")
    public static let statusCharUUID = CBUUID(string: "0000stat-0000-1000-8000-00805f9b34fb")
    
    // MARK: - Command Codes
    
    public enum CommandCode: UInt8 {
        case lock = 0x01
        case unlock = 0x02
        case startEngine = 0x03
        case stopEngine = 0x04
        case openTrunk = 0x05
        case openWindows = 0x06
        case closeWindows = 0x07
        case findVehicle = 0x08
        case getStatus = 0x09
    }
    
    // MARK: - Connection State
    
    public enum ConnectionState: Equatable {
        case disconnected
        case scanning
        case connecting
        case connected(String)
        case error(String)
    }
    
    // MARK: - Private Properties
    
    private var centralManager: CBCentralManager!
    private var connectedPeripheral: CBPeripheral?
    private var commandCharacteristic: CBCharacteristic?
    private var responseCharacteristic: CBCharacteristic?
    private var statusCharacteristic: CBCharacteristic?
    
    private var pendingCommands: [String: (Result<Data, Error>) -> Void] = [:]
    private var scanTimer: Timer?
    
    private var cancellables = Set<AnyCancellable>()
    private let queue = DispatchQueue(label: "com.yuledkcs.ble", qos: .userInitiated)
    
    // MARK: - Initialization
    
    private override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: queue)
    }
    
    // MARK: - Scanning
    
    /// Start scanning for nearby vehicles
    /// - Parameters:
    ///   - timeout: Scan timeout in seconds
    public func startScan(timeout: TimeInterval = 10.0) {
        guard centralManager.state == .poweredOn else {
            connectionState = .error("蓝牙未开启")
            return
        }
        
        isScanning = true
        connectionState = .scanning
        scanResults = []
        
        centralManager.scanForPeripherals(
            withServices: [BLEManager.serviceUUID],
            options: [CBCentralManagerScanOptionAllowDuplicatesKey: false]
        )
        
        // Auto stop after timeout
        scanTimer?.invalidate()
        scanTimer = Timer.scheduledTimer(withTimeInterval: timeout, repeats: false) { [weak self] _ in
            self?.stopScan()
        }
    }
    
    /// Stop scanning for devices
    public func stopScan() {
        centralManager.stopScan()
        isScanning = false
        scanTimer?.invalidate()
        scanTimer = nil
        
        if case .scanning = connectionState {
            connectionState = .disconnected
        }
    }
    
    // MARK: - Connection
    
    /// Connect to a vehicle via BLE
    /// - Parameters:
    ///   - address: UUID of the BLE device
    ///   - autoReconnect: Enable auto-reconnect
    public func connect(to address: UUID, autoReconnect: Bool = true) {
        stopScan()
        
        guard let peripheral = centralManager.retrievePeripherals(withIdentifiers: [address]).first else {
            connectionState = .error("设备未找到")
            return
        }
        
        connectionState = .connecting
        connectedPeripheral = peripheral
        peripheral.delegate = self
        
        centralManager.connect(peripheral, options: [
            CBConnectPeripheralOptionNotifyOnConnectionKey: true,
            CBConnectPeripheralOptionNotifyOnDisconnectionKey: true
        ])
    }
    
    /// Connect to nearest vehicle
    public func connectToNearest(completion: @escaping (Result<String, Error>) -> Void) {
        guard let nearest = scanResults.first else {
            completion(.failure(YuleDKCSError.bleNotAvailable))
            return
        }
        
        connect(to: nearest.peripheral.identifier)
        
        // Wait for connection
        $connectionState
            .filter { state in
                if case .connected = state { return true }
                if case .error = state { return true }
                return false
            }
            .first()
            .sink { state in
                switch state {
                case .connected(let address):
                    completion(.success(address))
                case .error(let message):
                    completion(.failure(YuleDKCSError.connectionFailed))
                default:
                    break
                }
            }
            .store(in: &cancellables)
    }
    
    /// Disconnect from the current device
    public func disconnect() {
        guard let peripheral = connectedPeripheral else { return }
        centralManager.cancelPeripheralConnection(peripheral)
    }
    
    // MARK: - Commands
    
    /// Send a command to the connected vehicle
    public func sendCommand(
        keyId: String,
        command: Command,
        completion: @escaping (Result<CommandResponse, Error>) -> Void
    ) {
        guard let peripheral = connectedPeripheral,
              peripheral.state == .connected else {
            completion(.failure(YuleDKCSError.connectionFailed))
            return
        }
        
        guard let characteristic = commandCharacteristic else {
            completion(.failure(YuleDKCSError.bleNotAvailable))
            return
        }
        
        // Check permission
        guard KeyManager.shared.hasPermission(keyId: keyId, permission: command.toPermission()) else {
            completion(.failure(YuleDKCSError.permissionDenied))
            return
        }
        
        queue.async { [weak self] in
            do {
                let packet = try self?.buildCommandPacket(keyId: keyId, command: command)
                guard let data = packet else { return }
                
                let commandId = UUID().uuidString
                
                // Store completion handler
                self?.pendingCommands[commandId] = { result in
                    DispatchQueue.main.async {
                        switch result {
                        case .success(let responseData):
                            let response = self?.parseResponse(data: responseData) ?? CommandResponse(success: false, message: "解析失败")
                            
                            // Record usage
                            KeyManager.shared.recordUsage(
                                keyId: keyId,
                                operation: command.description,
                                status: response.success ? .success : .failure,
                                failureReason: response.success ? nil : response.message
                            )
                            
                            completion(.success(response))
                            
                        case .failure(let error):
                            KeyManager.shared.recordUsage(
                                keyId: keyId,
                                operation: command.description,
                                status: .failure,
                                failureReason: error.localizedDescription
                            )
                            completion(.failure(error))
                        }
                    }
                }
                
                peripheral.writeValue(data, for: characteristic, type: .withResponse)
                
            } catch {
                DispatchQueue.main.async {
                    completion(.failure(error))
                }
            }
        }
    }
    
    // MARK: - Convenience Methods
    
    public func lock(keyId: String, completion: @escaping (Result<CommandResponse, Error>) -> Void) {
        sendCommand(keyId: keyId, command: .lock, completion: completion)
    }
    
    public func unlock(keyId: String, completion: @escaping (Result<CommandResponse, Error>) -> Void) {
        sendCommand(keyId: keyId, command: .unlock, completion: completion)
    }
    
    public func startEngine(keyId: String, completion: @escaping (Result<CommandResponse, Error>) -> Void) {
        sendCommand(keyId: keyId, command: .startEngine, completion: completion)
    }
    
    public func stopEngine(keyId: String, completion: @escaping (Result<CommandResponse, Error>) -> Void) {
        sendCommand(keyId: keyId, command: .stopEngine, completion: completion)
    }
    
    public func openTrunk(keyId: String, completion: @escaping (Result<CommandResponse, Error>) -> Void) {
        sendCommand(keyId: keyId, command: .openTrunk, completion: completion)
    }
    
    public func openWindows(keyId: String, completion: @escaping (Result<CommandResponse, Error>) -> Void) {
        sendCommand(keyId: keyId, command: .openWindows, completion: completion)
    }
    
    public func closeWindows(keyId: String, completion: @escaping (Result<CommandResponse, Error>) -> Void) {
        sendCommand(keyId: keyId, command: .closeWindows, completion: completion)
    }
    
    public func findVehicle(keyId: String, completion: @escaping (Result<CommandResponse, Error>) -> Void) {
        sendCommand(keyId: keyId, command: .findVehicle, completion: completion)
    }
    
    /// Request current vehicle status
    public func requestVehicleStatus() {
        guard let peripheral = connectedPeripheral,
              let characteristic = commandCharacteristic else { return }
        
        let data = Data([CommandCode.getStatus.rawValue])
        peripheral.writeValue(data, for: characteristic, type: .withResponse)
    }
    
    // MARK: - Private Methods
    
    private func buildCommandPacket(keyId: String, command: Command) throws -> Data {
        guard let key = KeyManager.shared.getKey(keyId: keyId) else {
            throw YuleDKCSError.keyNotFound
        }
        
        let timestamp = Int64(Date().timeIntervalSince1970)
        
        let commandByte: UInt8 = {
            switch command {
            case .lock: return CommandCode.lock.rawValue
            case .unlock: return CommandCode.unlock.rawValue
            case .startEngine: return CommandCode.startEngine.rawValue
            case .stopEngine: return CommandCode.stopEngine.rawValue
            case .openTrunk: return CommandCode.openTrunk.rawValue
            case .openWindows: return CommandCode.openWindows.rawValue
            case .closeWindows: return CommandCode.closeWindows.rawValue
            case .findVehicle: return CommandCode.findVehicle.rawValue
            case .custom(let data): return data.first ?? 0
            }
        }()
        
        // Build packet: [VERSION][KEY_ID][TIMESTAMP][COMMAND][SIGNATURE]
        var packet = Data()
        packet.append(0x01) // Version
        
        // Key ID (16 bytes)
        let keyIdData = keyId.data(using: .utf8) ?? Data()
        packet.append(keyIdData.prefix(16))
        packet.append(contentsOf: Array(repeating: 0, count: max(0, 16 - keyIdData.count)))
        
        // Timestamp (8 bytes, big-endian)
        var timestampBytes = timestamp.bigEndian
        packet.append(UnsafeBufferPointer(start: &timestampBytes, count: MemoryLayout<Int64>.size))
        
        // Command
        packet.append(commandByte)
        
        // Sign packet
        let dataToSign = packet
        let signature = try NativeLib.signCommand(data: dataToSign, keyData: key.keyData)
        packet.append(signature)
        
        return packet
    }
    
    private func parseResponse(data: Data) -> CommandResponse {
        guard data.count >= 1 else {
            return CommandResponse(success: false, message: "无效响应")
        }
        
        let status = data[0]
        return CommandResponse(
            success: status == 0x00,
            message: status == 0x00 ? "成功" : "失败: \(status)"
        )
    }
    
    private func handleStatusUpdate(data: Data) {
        guard data.count >= 5 else { return }
        
        let statusByte = data[0]
        let battery = data[1]
        let fuel = data[2]
        
        vehicleStatus = VehicleStatus(
            doorLocked: (statusByte & 0x01) != 0,
            engineRunning: (statusByte & 0x02) != 0,
            trunkOpen: (statusByte & 0x04) != 0,
            windowsOpen: (statusByte & 0x08) != 0,
            batteryLevel: Int(battery),
            fuelLevel: Int(fuel),
            alarmActive: (statusByte & 0x10) != 0,
            timestamp: Date().timeIntervalSince1970
        )
    }
}

// MARK: - CBCentralManagerDelegate

extension BLEManager: CBCentralManagerDelegate {
    
    public func centralManagerDidUpdateState(_ central: CBCentralManager) {
        if central.state != .poweredOn {
            connectionState = .error("蓝牙未开启")
        }
    }
    
    public func centralManager(
        _ central: CBCentralManager,
        didDiscover peripheral: CBPeripheral,
        advertisementData: [String: Any],
        rssi RSSI: NSNumber
    ) {
        let result = ScanResult(peripheral: peripheral, rssi: RSSI.intValue)
        
        DispatchQueue.main.async { [weak self] in
            // Update or add result
            if let index = self?.scanResults.firstIndex(where: { $0.peripheral.identifier == peripheral.identifier }) {
                self?.scanResults[index] = result
            } else {
                self?.scanResults.append(result)
            }
            
            // Sort by RSSI
            self?.scanResults.sort { $0.rssi > $1.rssi }
        }
    }
    
    public func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        DispatchQueue.main.async { [weak self] in
            self?.connectionState = .connected(peripheral.identifier.uuidString)
        }
        peripheral.discoverServices([BLEManager.serviceUUID])
    }
    
    public func centralManager(
        _ central: CBCentralManager,
        didFailToConnect peripheral: CBPeripheral,
        error: Error?
    ) {
        DispatchQueue.main.async { [weak self] in
            self?.connectionState = .error(error?.localizedDescription ?? "连接失败")
        }
    }
    
    public func centralManager(
        _ central: CBCentralManager,
        didDisconnectPeripheral peripheral: CBPeripheral,
        error: Error?
    ) {
        DispatchQueue.main.async { [weak self] in
            self?.connectionState = .disconnected
            self?.connectedPeripheral = nil
            self?.vehicleStatus = nil
        }
    }
}

// MARK: - CBPeripheralDelegate

extension BLEManager: CBPeripheralDelegate {
    
    public func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let services = peripheral.services else { return }
        
        for service in services where service.uuid == BLEManager.serviceUUID {
            peripheral.discoverCharacteristics(
                [BLEManager.commandCharUUID, BLEManager.responseCharUUID, BLEManager.statusCharUUID],
                for: service
            )
        }
    }
    
    public func peripheral(
        _ peripheral: CBPeripheral,
        didDiscoverCharacteristicsFor service: CBService,
        error: Error?
    ) {
        guard let characteristics = service.characteristics else { return }
        
        for characteristic in characteristics {
            switch characteristic.uuid {
            case BLEManager.commandCharUUID:
                commandCharacteristic = characteristic
            case BLEManager.responseCharUUID:
                responseCharacteristic = characteristic
                peripheral.setNotifyValue(true, for: characteristic)
            case BLEManager.statusCharUUID:
                statusCharacteristic = characteristic
                peripheral.setNotifyValue(true, for: characteristic)
            default:
                break
            }
        }
        
        // Request initial status
        requestVehicleStatus()
    }
    
    public func peripheral(
        _ peripheral: CBPeripheral,
        didUpdateValueFor characteristic: CBCharacteristic,
        error: Error?
    ) {
        guard let data = characteristic.value else { return }
        
        switch characteristic.uuid {
        case BLEManager.responseCharUUID:
            handleResponse(data: data)
        case BLEManager.statusCharUUID:
            handleStatusUpdate(data: data)
        default:
            break
        }
    }
    
    private func handleResponse(data: Data) {
        guard data.count >= 2 else { return }
        
        let commandId = data[1]
        let callback = pendingCommands.removeValue(forKey: String(commandId))
        
        if data[0] == 0x00 {
            callback?(.success(data))
        } else {
            callback?(.failure(YuleDKCSError.commandFailed))
        }
    }
}

// MARK: - Supporting Types

public struct ScanResult: Identifiable {
    public let id = UUID()
    public let peripheral: CBPeripheral
    public let rssi: Int
    
    public var name: String? { peripheral.name }
}

public struct CommandResponse {
    public let success: Bool
    public let message: String
}

private extension Command {
    func toPermission() -> Permission {
        switch self {
        case .lock: return .lock
        case .unlock: return .unlock
        case .startEngine: return .startEngine
        case .stopEngine: return .stopEngine
        case .openTrunk: return .openTrunk
        case .openWindows: return .openWindows
        case .closeWindows: return .closeWindows
        case .findVehicle: return .findVehicle
        case .custom: return .unlock
        }
    }
}
