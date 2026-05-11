import Foundation
import Combine
import CoreBluetooth
import LocalAuthentication
import CryptoKit

/// Manager for digital key operations
///
/// Handles issuing, listing, sharing, and revoking digital keys.
/// Also manages key usage logs and offline operations.
public class KeyManager: ObservableObject {
    
    public static let shared = KeyManager()
    
    @Published public private(set) var keys: [DigitalKey] = []
    @Published public private(set) var activeKey: DigitalKey?
    @Published public private(set) var isLoading = false
    
    private var cancellables = Set<AnyCancellable>()
    private let userDefaults = UserDefaults.standard
    private let keychain = KeychainManager.shared
    private let queue = DispatchQueue(label: "com.yuledkcs.keymanager", qos: .userInitiated)
    
    private init() {
        loadCachedKeys()
    }
    
    // MARK: - Key Lifecycle
    
    /// Issue a new digital key for a vehicle
    /// - Parameters:
    ///   - vehicleId: The vehicle identifier
    ///   - completion: Callback with Result containing the issued DigitalKey
    public func issueKey(
        vehicleId: String,
        completion: @escaping (Result<DigitalKey, Error>) -> Void
    ) {
        guard YuleDKCS.shared.isInitialized else {
            completion(.failure(YuleDKCSError.notInitialized))
            return
        }
        
        isLoading = true
        
        YuleDKCS.shared.api.issueKey(vehicleId: vehicleId)
            .receive(on: DispatchQueue.main)
            .sink(
                receiveCompletion: { [weak self] result in
                    self?.isLoading = false
                    if case .failure(let error) = result {
                        completion(.failure(error))
                    }
                },
                receiveValue: { [weak self] key in
                    self?.storeKey(key)
                    completion(.success(key))
                }
            )
            .store(in: &cancellables)
    }
    
    /// Receive a shared key via QR code or deep link
    /// - Parameters:
    ///   - shareToken: The share token from QR code or link
    ///   - completion: Callback with Result containing the received DigitalKey
    public func receiveSharedKey(
        shareToken: String,
        completion: @escaping (Result<DigitalKey, Error>) -> Void
    ) {
        guard YuleDKCS.shared.isInitialized else {
            completion(.failure(YuleDKCSError.notInitialized))
            return
        }
        
        isLoading = true
        
        YuleDKCS.shared.api.receiveSharedKey(token: shareToken)
            .receive(on: DispatchQueue.main)
            .sink(
                receiveCompletion: { [weak self] result in
                    self?.isLoading = false
                    if case .failure(let error) = result {
                        completion(.failure(error))
                    }
                },
                receiveValue: { [weak self] key in
                    self?.storeKey(key)
                    completion(.success(key))
                }
            )
            .store(in: &cancellables)
    }
    
    /// Activate a key (bind to Secure Enclave)
    /// - Parameters:
    ///   - keyId: The key identifier
    ///   - completion: Callback with Result
    public func activateKey(
        keyId: String,
        completion: ((Result<Void, Error>) -> Void)? = nil
    ) {
        guard YuleDKCS.shared.isInitialized else {
            completion?(.failure(YuleDKCSError.notInitialized))
            return
        }
        
        queue.async { [weak self] in
            // First activate on server
            YuleDKCS.shared.api.activateKey(keyId: keyId)
                .receive(on: DispatchQueue.main)
                .sink(
                    receiveCompletion: { result in
                        if case .failure(let error) = result {
                            completion?(.failure(error))
                        }
                    },
                    receiveValue: { [weak self] _ in
                        guard let self = self else { return }
                        
                        // Store in Secure Enclave
                        if let key = self.keys.first(where: { $0.id == keyId }) {
                            do {
                                try self.keychain.storeKey(keyId: keyId, keyData: key.keyData)
                                
                                // Update local status
                                var updatedKey = key
                                updatedKey.status = .active
                                self.updateKey(updatedKey)
                                self.activeKey = updatedKey
                                
                                completion?(.success(()))
                            } catch {
                                completion?(.failure(error))
                            }
                        }
                    }
                )
                .store(in: &self!.cancellables)
        }
    }
    
    /// Get list of all keys
    /// - Parameter forceRefresh: Force refresh from server
    /// - Returns: Array of DigitalKey
    public func listKeys(forceRefresh: Bool = false) -> [DigitalKey] {
        guard YuleDKCS.shared.isInitialized else {
            return []
        }
        
        if forceRefresh || keys.isEmpty {
            refreshKeys()
        }
        
        return keys
    }
    
    /// Get a specific key by ID
    /// - Parameter keyId: The key identifier
    /// - Returns: DigitalKey or nil
    public func getKey(keyId: String) -> DigitalKey? {
        return keys.first { $0.id == keyId }
    }
    
    /// Set active key for quick access
    /// - Parameter keyId: The key identifier
    public func setActiveKey(keyId: String?) {
        activeKey = keyId.flatMap { getKey(keyId: $0) }
        userDefaults.set(keyId, forKey: "active_key_id")
    }
    
    /// Get the currently active key
    public func getActiveKey() -> DigitalKey? {
        return activeKey
    }
    
    // MARK: - Sharing
    
    /// Share a key with another user
    /// - Parameters:
    ///   - keyId: The key identifier to share
    ///   - recipient: Email or phone number of recipient
    ///   - permissions: List of permissions to grant
    ///   - expiresInDays: Expiration in days
    ///   - completion: Callback with Result containing share link
    public func shareKey(
        keyId: String,
        recipient: String,
        permissions: [Permission],
        expiresInDays: Int = 7,
        completion: ((Result<String, Error>) -> Void)? = nil
    ) {
        guard YuleDKCS.shared.isInitialized else {
            completion?(.failure(YuleDKCSError.notInitialized))
            return
        }
        
        YuleDKCS.shared.api.shareKey(
            keyId: keyId,
            recipient: recipient,
            permissions: permissions.map { $0.rawValue },
            expiresInDays: expiresInDays
        )
        .receive(on: DispatchQueue.main)
        .sink(
            receiveCompletion: { result in
                if case .failure(let error) = result {
                    completion?(.failure(error))
                }
            },
            receiveValue: { response in
                completion?(.success(response.shareLink))
            }
        )
        .store(in: &cancellables)
    }
    
    // MARK: - Revocation
    
    /// Revoke a key
    /// - Parameters:
    ///   - keyId: The key identifier to revoke
    ///   - completion: Callback with Result
    public func revokeKey(
        keyId: String,
        completion: ((Result<Void, Error>) -> Void)? = nil
    ) {
        guard YuleDKCS.shared.isInitialized else {
            completion?(.failure(YuleDKCSError.notInitialized))
            return
        }
        
        YuleDKCS.shared.api.revokeKey(keyId: keyId)
            .receive(on: DispatchQueue.main)
            .sink(
                receiveCompletion: { result in
                    if case .failure(let error) = result {
                        completion?(.failure(error))
                    }
                },
                receiveValue: { [weak self] _ in
                    self?.removeKey(keyId)
                    completion?(.success(()))
                }
            )
            .store(in: &cancellables)
    }
    
    /// Delete local key data only (keep server key)
    public func deleteLocalKey(keyId: String) {
        removeKey(keyId)
    }
    
    // MARK: - Usage Logging
    
    /// Record a key usage log
    public func recordUsage(
        keyId: String,
        operation: String,
        status: KeyUsageLog.Status,
        location: CLLocationCoordinate2D? = nil,
        failureReason: String? = nil
    ) {
        let log = KeyUsageLog(
            id: UUID().uuidString,
            keyId: keyId,
            operation: operation,
            status: status,
            timestamp: Date(),
            location: location.flatMap { KeyUsageLog.Location(lat: $0.latitude, lng: $0.longitude) },
            deviceInfo: UIDevice.current.model,
            failureReason: failureReason
        )
        
        // Store locally
        storeUsageLog(keyId: keyId, log: log)
        
        // Try to sync to server
        YuleDKCS.shared.api.recordUsage(keyId: keyId, log: log)
            .sink(receiveCompletion: { _ in }, receiveValue: { _ in })
            .store(in: &cancellables)
    }
    
    /// Get usage logs for a key
    public func getUsageLogs(keyId: String) -> [KeyUsageLog] {
        return loadUsageLogs(keyId: keyId)
    }
    
    // MARK: - Permission Checking
    
    /// Check if a key has a specific permission
    public func hasPermission(keyId: String, permission: Permission) -> Bool {
        guard let key = getKey(keyId: keyId) else { return false }
        return key.permissions.contains { $0.type == permission.rawValue && $0.enabled }
    }
    
    /// Check if key is expired
    public func isKeyExpired(keyId: String) -> Bool {
        guard let key = getKey(keyId: keyId) else { return true }
        guard let expiresAt = key.expiresAt else { return false }
        return Date() > expiresAt
    }
    
    // MARK: - Private Methods
    
    private func loadCachedKeys() {
        if let data = userDefaults.data(forKey: "cached_keys"),
           let keys = try? JSONDecoder().decode([DigitalKey].self, from: data) {
            self.keys = keys
        }
        
        if let activeId = userDefaults.string(forKey: "active_key_id") {
            activeKey = getKey(keyId: activeId)
        }
        
        refreshKeys()
    }
    
    private func refreshKeys() {
        YuleDKCS.shared.api.listKeys()
            .receive(on: DispatchQueue.main)
            .sink(
                receiveCompletion: { _ in },
                receiveValue: { [weak self] keys in
                    self?.keys = keys
                    self?.saveCachedKeys()
                }
            )
            .store(in: &cancellables)
    }
    
    private func storeKey(_ key: DigitalKey) {
        if !keys.contains(where: { $0.id == key.id }) {
            keys.append(key)
            saveCachedKeys()
            
            // Store key data securely
            try? keychain.storeKey(keyId: key.id, keyData: key.keyData)
        }
    }
    
    private func updateKey(_ key: DigitalKey) {
        if let index = keys.firstIndex(where: { $0.id == key.id }) {
            keys[index] = key
            saveCachedKeys()
        }
    }
    
    private func removeKey(_ keyId: String) {
        keys.removeAll { $0.id == keyId }
        saveCachedKeys()
        
        try? keychain.deleteKey(keyId: keyId)
        
        if activeKey?.id == keyId {
            activeKey = nil
            userDefaults.removeObject(forKey: "active_key_id")
        }
    }
    
    private func saveCachedKeys() {
        if let data = try? JSONEncoder().encode(keys) {
            userDefaults.set(data, forKey: "cached_keys")
        }
    }
    
    private func storeUsageLog(keyId: String, log: KeyUsageLog) {
        var logs = loadUsageLogs(keyId: keyId)
        logs.insert(log, at: 0)
        
        // Keep only last 100 logs
        if logs.count > 100 {
            logs = Array(logs.prefix(100))
        }
        
        if let data = try? JSONEncoder().encode(logs) {
            userDefaults.set(data, forKey: "usage_logs_\(keyId)")
        }
    }
    
    private func loadUsageLogs(keyId: String) -> [KeyUsageLog] {
        guard let data = userDefaults.data(forKey: "usage_logs_\(keyId)") else {
            return []
        }
        return (try? JSONDecoder().decode([KeyUsageLog].self, from: data)) ?? []
    }
}

// MARK: - Errors

public enum YuleDKCSError: Error {
    case notInitialized
    case keyNotFound
    case permissionDenied
    case bleNotAvailable
    case connectionFailed
    case commandFailed
}
