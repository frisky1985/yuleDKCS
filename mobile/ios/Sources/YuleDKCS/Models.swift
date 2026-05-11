import Foundation
import CoreLocation

// MARK: - Digital Key

public struct DigitalKey: Codable, Identifiable {
    public let id: String
    public let keyData: Data
    public let vehicleId: String?
    public let protocolType: ProtocolType
    public let type: KeyType
    public var status: KeyStatus
    public let permissions: [KeyPermission]
    public let issuedAt: Date
    public let expiresAt: Date?
    public let sharedBy: String?
    public let sharedTo: String?
    public let useCount: Int
    public let lastUsedAt: Date?
    
    public enum ProtocolType: String, Codable {
        case ccc = "CCC"
        case iccoa = "ICCOA"
        case icce = "ICCE"
    }
    
    public enum KeyType: String, Codable {
        case owner = "OWNER"
        case shared = "SHARED"
    }
    
    public enum KeyStatus: String, Codable {
        case active = "active"
        case inactive = "inactive"
        case expired = "expired"
        case revoked = "revoked"
    }
}

// MARK: - Permission

public struct KeyPermission: Codable {
    public let type: String
    public let enabled: Bool
    public let constraints: PermissionConstraints?
}

public struct PermissionConstraints: Codable {
    public let timeRange: TimeRange?
    public let daysOfWeek: [Int]?
    public let maxUses: Int?
    public let geofence: GeoFence?
}

public struct TimeRange: Codable {
    public let start: String
    public let end: String
}

public struct GeoFence: Codable {
    public let type: String
    public let center: GeoPoint?
    public let radius: Double?
    public let points: [GeoPoint]?
}

public struct GeoPoint: Codable {
    public let lat: Double
    public let lng: Double
}

public enum Permission: String, CaseIterable {
    case unlock = "unlock"
    case lock = "lock"
    case startEngine = "start_engine"
    case stopEngine = "stop_engine"
    case openTrunk = "open_trunk"
    case openWindows = "open_windows"
    case closeWindows = "close_windows"
    case findVehicle = "find_vehicle"
    
    public var displayName: String {
        switch self {
        case .unlock: return "解锁"
        case .lock: return "锁车"
        case .startEngine: return "启动引擎"
        case .stopEngine: return "停止引擎"
        case .openTrunk: return "开后备箱"
        case .openWindows: return "开窗"
        case .closeWindows: return "关窗"
        case .findVehicle: return "寻车"
        }
    }
}

// MARK: - Key Usage Log

public struct KeyUsageLog: Codable, Identifiable {
    public let id: String
    public let keyId: String
    public let operation: String
    public let status: Status
    public let timestamp: Date
    public let location: Location?
    public let deviceInfo: String
    public let failureReason: String?
    
    public enum Status: String, Codable {
        case success = "SUCCESS"
        case failure = "FAILURE"
        case timeout = "TIMEOUT"
    }
    
    public struct Location: Codable {
        public let lat: Double
        public let lng: Double
        
        public init(lat: Double, lng: Double) {
            self.lat = lat
            self.lng = lng
        }
        
        public var coordinate: CLLocationCoordinate2D {
            CLLocationCoordinate2D(latitude: lat, longitude: lng)
        }
    }
}

// MARK: - Vehicle Status

public struct VehicleStatus: Codable {
    public let doorLocked: Bool
    public let engineRunning: Bool
    public let trunkOpen: Bool
    public let windowsOpen: Bool
    public let batteryLevel: Int
    public let fuelLevel: Int
    public let alarmActive: Bool
    public let timestamp: TimeInterval
    
    public var isOnline: Bool {
        Date().timeIntervalSince1970 - timestamp < 60
    }
}

// MARK: - Command

public enum Command: CustomStringConvertible {
    case lock
    case unlock
    case startEngine
    case stopEngine
    case openTrunk
    case openWindows
    case closeWindows
    case findVehicle
    case custom(Data)
    
    public var description: String {
        switch self {
        case .lock: return "lock"
        case .unlock: return "unlock"
        case .startEngine: return "start_engine"
        case .stopEngine: return "stop_engine"
        case .openTrunk: return "open_trunk"
        case .openWindows: return "open_windows"
        case .closeWindows: return "close_windows"
        case .findVehicle: return "find_vehicle"
        case .custom: return "custom"
        }
    }
}

// MARK: - Share

public struct ShareKeyRequest: Codable {
    public let recipient: String
    public let permissions: [String]
    public let expiresInDays: Int
    public let message: String?
    
    public init(recipient: String, permissions: [String], expiresInDays: Int, message: String? = nil) {
        self.recipient = recipient
        self.permissions = permissions
        self.expiresInDays = expiresInDays
        self.message = message
    }
}

public struct ShareKeyResponse: Codable {
    public let shareId: String
    public let qrCodeUrl: String
    public let shareLink: String
    public let expiresAt: Date
}

// MARK: - Vehicle

public struct Vehicle: Codable, Identifiable {
    public let id: String
    public let vin: String
    public let brand: String
    public let model: String
    public let year: Int
    public let color: String
    public let plateNumber: String?
    public let bleAddress: String?
}

// MARK: - Connection Info

public struct ConnectionInfo: Codable {
    public let address: String
    public let rssi: Int
    public let name: String?
    public let isBonded: Bool
    
    public var signalStrength: SignalStrength {
        switch rssi {
        case ...(-80): return .weak
        case -80..<(-60): return .medium
        case -60...: return .strong
        default: return .unknown
        }
    }
    
    public enum SignalStrength {
        case weak, medium, strong, unknown
        
        public var description: String {
            switch self {
            case .weak: return "信号弱"
            case .medium: return "信号一般"
            case .strong: return "信号强"
            case .unknown: return "未知"
            }
        }
    }
}

// MARK: - Native Lib

public enum NativeLib {
    public static func signCommand(data: Data, keyData: Data) throws -> Data {
        // Implementation in native code
        // This is a placeholder - actual implementation uses Secure Enclave
        return Data() // Replace with actual signing
    }
}

// MARK: - Keychain Manager

public class KeychainManager {
    public static let shared = KeychainManager()
    
    private init() {}
    
    public func storeKey(keyId: String, keyData: Data) throws {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrAccount as String: keyId,
            kSecValueData as String: keyData,
            kSecAttrAccessible as String: kSecAttrAccessibleWhenUnlockedThisDeviceOnly
        ]
        
        SecItemDelete(query as CFDictionary)
        let status = SecItemAdd(query as CFDictionary, nil)
        
        guard status == errSecSuccess else {
            throw KeychainError.unableToStore
        }
    }
    
    public func getKey(keyId: String) -> Data? {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrAccount as String: keyId,
            kSecReturnData as String: true,
            kSecMatchLimit as String: kSecMatchLimitOne
        ]
        
        var result: AnyObject?
        SecItemCopyMatching(query as CFDictionary, &result)
        return result as? Data
    }
    
    public func deleteKey(keyId: String) throws {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrAccount as String: keyId
        ]
        
        let status = SecItemDelete(query as CFDictionary)
        guard status == errSecSuccess || status == errSecItemNotFound else {
            throw KeychainError.unableToDelete
        }
    }
}

public enum KeychainError: Error {
    case unableToStore
    case unableToDelete
}
