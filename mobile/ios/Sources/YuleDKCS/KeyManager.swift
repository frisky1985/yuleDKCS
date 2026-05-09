import Foundation

/// 数字钥匙模型
public struct DigitalKey: Codable, Identifiable {
    public let id: String
    public let vehicleId: String
    public let ownerId: String
    public let issuedAt: Date
    public let expiresAt: Date?
    public let permissions: [Permission]
    public let status: KeyStatus
    public let sharedWith: [String]?
    
    public init(
        id: String,
        vehicleId: String,
        ownerId: String,
        issuedAt: Date,
        expiresAt: Date? = nil,
        permissions: [Permission] = [],
        status: KeyStatus = .active,
        sharedWith: [String]? = nil
    ) {
        self.id = id
        self.vehicleId = vehicleId
        self.ownerId = ownerId
        self.issuedAt = issuedAt
        self.expiresAt = expiresAt
        self.permissions = permissions
        self.status = status
        self.sharedWith = sharedWith
    }
}

/// 权限类型
public struct Permission: Codable, OptionSet {
    public let rawValue: Int
    
    public init(rawValue: Int) {
        self.rawValue = rawValue
    }
    
    public static let unlock = Permission(rawValue: 1 << 0)
    public static let lock = Permission(rawValue: 1 << 1)
    public static let startEngine = Permission(rawValue: 1 << 2)
    public static let openTrunk = Permission(rawValue: 1 << 3)
    public static let openWindows = Permission(rawValue: 1 << 4)
    public static let shareKey = Permission(rawValue: 1 << 5)
    public static let revokeKey = Permission(rawValue: 1 << 6)
    public static let all: Permission = [.unlock, .lock, .startEngine, .openTrunk, .openWindows, .shareKey, .revokeKey]
}

/// 钥匙状态
public enum KeyStatus: String, Codable {
    case active
    case inactive
    case revoked
    case expired
    case pending
}

/// 钥匙管理器
public final class KeyManager {
    
    // MARK: - Singleton
    
    public static let shared = KeyManager()
    
    // MARK: - Properties
    
    private var keys: [DigitalKey] = []
    private let queue = DispatchQueue(label: "com.yuledkcs.keymanager", qos: .userInitiated)
    private let userDefaults = UserDefaults.standard
    private let keysKey = "com.yuledkcs.keys"
    
    // MARK: - Initialization
    
    private init() {
        loadKeysFromStorage()
    }
    
    // MARK: - Public Methods
    
    /// 发放新钥匙
    /// - Parameters:
    ///   - vehicleId: 车辆 ID
    ///   - permissions: 权限列表
    ///   - completion: 回调
    public func issueKey(
        vehicleId: String,
        permissions: [Permission] = [.unlock, .lock],
        completion: @escaping (Result<DigitalKey, Error>) -> Void
    ) {
        queue.async {
            do {
                try YuleDKCS.shared.checkInitialized()
                
                // 调用 FFI 生成钥匙
                let keyData = try self.generateKeyViaFFI(vehicleId: vehicleId, permissions: permissions)
                
                // 创建钥匙对象
                let key = DigitalKey(
                    id: UUID().uuidString,
                    vehicleId: vehicleId,
                    ownerId: "current_user",
                    issuedAt: Date(),
                    expiresAt: nil,
                    permissions: permissions,
                    status: .active
                )
                
                // 存储钥匙
                self.saveKey(key)
                
                DispatchQueue.main.async {
                    completion(.success(key))
                }
            } catch {
                DispatchQueue.main.async {
                    completion(.failure(error))
                }
            }
        }
    }
    
    /// 获取所有钥匙列表
    public func listKeys() -> [DigitalKey] {
        return queue.sync {
            return keys
        }
    }
    
    /// 根据 ID 获取钥匙
    public func getKey(id: String) -> DigitalKey? {
        return queue.sync {
            return keys.first { $0.id == id }
        }
    }
    
    /// 分享钥匙
    /// - Parameters:
    ///   - keyId: 钥匙 ID
    ///   - recipient: 接收者 ID
    ///   - permissions: 要分享的权限
    public func shareKey(
        keyId: String,
        to recipient: String,
        permissions: [Permission] = [.unlock, .lock]
    ) {
        queue.async {
            guard let index = self.keys.firstIndex(where: { $0.id == keyId }) else {
                print("[YuleDKCS] Key not found: \(keyId)")
                return
            }
            
            var key = self.keys[index]
            
            // 检查权限
            guard key.permissions.contains(.shareKey) else {
                print("[YuleDKCS] No permission to share key")
                return
            }
            
            // 更新分享列表
            var sharedList = key.sharedWith ?? []
            if !sharedList.contains(recipient) {
                sharedList.append(recipient)
            }
            
            // 创建分享记录
            let sharedKey = DigitalKey(
                id: UUID().uuidString,
                vehicleId: key.vehicleId,
                ownerId: recipient,
                issuedAt: Date(),
                expiresAt: nil,
                permissions: permissions,
                status: .active,
                sharedWith: nil
            )
            
            // 同步到服务器
            self.syncShareToServer(keyId: keyId, recipient: recipient, permissions: permissions)
            
            print("[YuleDKCS] Key \(keyId) shared to \(recipient)")
        }
    }
    
    /// 撤销钥匙
    /// - Parameter keyId: 钥匙 ID
    public func revokeKey(keyId: String) {
        queue.async {
            guard let index = self.keys.firstIndex(where: { $0.id == keyId }) else {
                print("[YuleDKCS] Key not found: \(keyId)")
                return
            }
            
            var key = self.keys[index]
            
            // 检查权限
            guard key.permissions.contains(.revokeKey) else {
                print("[YuleDKCS] No permission to revoke key")
                return
            }
            
            key = DigitalKey(
                id: key.id,
                vehicleId: key.vehicleId,
                ownerId: key.ownerId,
                issuedAt: key.issuedAt,
                expiresAt: key.expiresAt,
                permissions: key.permissions,
                status: .revoked,
                sharedWith: key.sharedWith
            )
            
            self.keys[index] = key
            self.saveKeysToStorage()
            
            // 同步到服务器
            self.syncRevokeToServer(keyId: keyId)
            
            print("[YuleDKCS] Key \(keyId) revoked")
        }
    }
    
    /// 删除钥匙
    /// - Parameter keyId: 钥匙 ID
    public func deleteKey(keyId: String) {
        queue.async {
            self.keys.removeAll { $0.id == keyId }
            self.saveKeysToStorage()
            print("[YuleDKCS] Key \(keyId) deleted")
        }
    }
    
    // MARK: - Private Methods
    
    private func generateKeyViaFFI(vehicleId: String, permissions: [Permission]) throws -> Data {
        // 通过 FFI 调用底层 C 库生成钥匙
        let result = FFIBridge.shared.generateKey(vehicleId: vehicleId)
        
        switch result {
        case .success(let data):
            return data
        case .failure(let error):
            throw error
        }
    }
    
    private func saveKey(_ key: DigitalKey) {
        queue.sync {
            if let index = keys.firstIndex(where: { $0.id == key.id }) {
                keys[index] = key
            } else {
                keys.append(key)
            }
            saveKeysToStorage()
        }
    }
    
    private func loadKeysFromStorage() {
        guard let data = userDefaults.data(forKey: keysKey),
              let savedKeys = try? JSONDecoder().decode([DigitalKey].self, from: data) else {
            return
        }
        keys = savedKeys
    }
    
    private func saveKeysToStorage() {
        if let data = try? JSONEncoder().encode(keys) {
            userDefaults.set(data, forKey: keysKey)
        }
    }
    
    private func syncShareToServer(keyId: String, recipient: String, permissions: [Permission]) {
        // TODO: 实现服务器同步
        print("[YuleDKCS] Syncing share to server...")
    }
    
    private func syncRevokeToServer(keyId: String) {
        // TODO: 实现服务器同步
        print("[YuleDKCS] Syncing revoke to server...")
    }
}