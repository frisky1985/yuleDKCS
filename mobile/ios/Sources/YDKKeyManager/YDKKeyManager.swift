import Foundation
import YDKHubClient

// MARK: - 数据模型

/// 同步结果
public struct SyncResult {
    public let added: [YDKKey]
    public let updated: [YDKKey]
    public let removed: [YDKKey]
    public let unchanged: Int

    public var hasChanges: Bool {
        !added.isEmpty || !updated.isEmpty || !removed.isEmpty
    }
}

/// 钥匙变更事件
public struct KeyChange {
    public let keyId: String
    public let type: ChangeType
    public let key: YDKKey?

    public enum ChangeType: String {
        case added, updated, removed
    }
}

// MARK: - Delegate

/// KeyManager 变更通知协议
/// App 实现此 protocol 接收钥匙状态变更
public protocol YDKKeyManagerDelegate: AnyObject {
    func keyManager(_ manager: YDKKeyManager, didDetectChanges changes: [KeyChange])
    func keyManager(_ manager: YDKKeyManager, syncDidFailWith error: Error)
}

// MARK: - KeyManager

/// yuleDKCS 钥匙状态管理器
///
/// 职责:
/// 1. 本地钥匙缓存（JSON 文件持久化）
/// 2. 定时同步 Hub 云端钥匙列表
/// 3. 差异检测 → delegate 通知 App
/// 4. Push 触发增量同步
/// 5. 离线访问（无网返回缓存）
///
/// 用法:
/// ```swift
/// let keyManager = YDKKeyManager(hubClient: client)
/// keyManager.delegate = self
/// let localKeys = keyManager.getLocalKeys()  // 离线可用
/// try await keyManager.syncFromHub()          // 手动同步
/// ```
public final class YDKKeyManager {

    // MARK: - 公开属性

    /// 变更通知代理
    public weak var delegate: YDKKeyManagerDelegate?

    // MARK: - 内部状态

    private let hubClient: YDKHubClient
    private let cache: YDKKeyCache
    // internal: YDKKeyManager+Sync.swift（同模块）跨文件访问 logger / syncQueue
    let logger: YDKLogger
    let syncQueue = DispatchQueue(label: "com.yuledkcs.keymanager.sync", qos: .background)

    // MARK: - 初始化

    public init(
        hubClient: YDKHubClient,
        cacheFileURL: URL? = nil,
        enableLogging: Bool = false
    ) {
        self.hubClient = hubClient
        self.logger = YDKLogger(enabled: enableLogging)
        self.cache = YDKKeyCache(fileURL: cacheFileURL, logger: logger)
    }

    // MARK: - 公开接口

    /// 获取本地缓存的钥匙列表（无网可用）
    public func getLocalKeys() -> [YDKKey] {
        cache.getLocalKeys()
    }

    /// 获取单把钥匙（优先从缓存读取）
    public func getKey(keyId: String, preferCache: Bool = true) -> YDKKey? {
        if preferCache {
            if let cached = cache.getLocalKeys().first(where: { $0.keyId == keyId }) {
                return cached
            }
        }
        return nil
    }

    /// 手动触发同步
    ///
    /// 流程:
    /// 1. 调 HubClient.listKeys() 获取云端钥匙列表
    /// 2. 与本地缓存做差异检测
    /// 3. 更新本地缓存
    /// 4. 通过 delegate 通知 App
    ///
    /// - Returns: SyncResult（含新增/更新/删除的钥匙）
    @discardableResult
    public func syncFromHub() async throws -> SyncResult {
        logger.log("Sync: starting...")

        // 1. 获取云端数据
        let cloudKeys: [YDKKey]
        do {
            cloudKeys = try await hubClient.listKeys()
        } catch {
            logger.log("Sync: failed — \(error.localizedDescription)")
            delegate?.keyManager(self, syncDidFailWith: error)
            throw error
        }

        // 2. 读取本地缓存
        let localKeys = cache.getLocalKeys()
        let localIndex = Dictionary(uniqueKeysWithValues: localKeys.map { ($0.keyId, $0) })
        let cloudIndex = Dictionary(uniqueKeysWithValues: cloudKeys.map { ($0.keyId, $0) })

        // 3. 差异检测
        var added: [YDKKey] = []
        var updated: [YDKKey] = []
        var unchanged = 0

        for cloudKey in cloudKeys {
            if let localKey = localIndex[cloudKey.keyId] {
                if cloudKey.status != localKey.status ||
                   cloudKey.validUntil != localKey.validUntil {
                    updated.append(cloudKey)
                } else {
                    unchanged += 1
                }
            } else {
                added.append(cloudKey)
            }
        }

        let removed = localKeys.filter { cloudIndex[$0.keyId] == nil }

        let result = SyncResult(
            added: added,
            updated: updated,
            removed: removed,
            unchanged: unchanged
        )

        // 4. 更新缓存
        cache.write(keys: cloudKeys)

        // 5. 通知
        if result.hasChanges {
            let changes = buildChanges(from: result)
            logger.log("Sync: \(added.count) added, \(updated.count) updated, \(removed.count) removed, \(unchanged) unchanged")
            delegate?.keyManager(self, didDetectChanges: changes)
        } else {
            logger.log("Sync: no changes (\(unchanged) keys)")
        }

        return result
    }

    /// 处理 Push 通知 → 增量同步指定钥匙
    ///
    /// App 收到 Push 后，将 keyId 传给 KeyManager。
    /// KeyManager 做一次完整同步（diff 会自动发现该 key 的状态变化）。
    /// 相比每次都全量 sync，也可以只查单把钥匙——但单把钥匙的 API
    /// (GetKey) 只返回一把，diff 不完整，所以还是做 listKeys()。
    ///
    /// - Parameter keyId: Push 中携带的钥匙 ID
    /// - Returns: 是否检测到变更
    @discardableResult
    public func handleKeyStatusPush(keyId: String) async throws -> Bool {
        logger.log("Push: triggered for key \(keyId)")
        let result = try await syncFromHub()
        return result.hasChanges
    }

    /// 清除本地缓存
    public func clearCache() {
        cache.clear()
    }

    /// 离线授权回退 — BLE/NFC 离线解锁前调用
    ///
    /// 当 Hub 不可达时, SDK 回退到本地缓存做离线授权裁决
    /// （状态 + 有效期窗口 + 离线宽限期, 详见 YDKOfflineAuthorizer）。
    ///
    /// - Parameters:
    ///   - keyId: 待裁决的钥匙 ID
    ///   - now: 当前时间（可注入便于测试）
    ///   - maxOfflineGrace: 允许的最大离线时长（秒）, 默认 7 天
    /// - Returns: nil 表示本地缓存无此钥匙; 否则返回裁决结果
    public func authorizeOfflineUse(
        keyId: String,
        at now: Date = Date(),
        maxOfflineGrace: TimeInterval = YDKOfflineAuthorizer.defaultMaxOfflineGrace
    ) -> YDKOfflineAuthorization? {
        guard let key = getKey(keyId: keyId) else {
            logger.log("OfflineAuth: key \\(keyId) not in local cache")
            return nil
        }
        let result = YDKOfflineAuthorizer.authorize(
            key: key,
            now: now,
            lastSyncAtMillis: cache.lastSyncTimestampMillis(),
            maxOfflineGrace: maxOfflineGrace
        )
        let reason = result.reason?.rawValue ?? "unknown"
        let verdict = result.allowed ? "allowed" : "denied(\(reason))"
        logger.log("OfflineAuth: key \(keyId) \(verdict)")
        return result
    }
}

// MARK: - 内部方法

extension YDKKeyManager {
    private func buildChanges(from result: SyncResult) -> [KeyChange] {
        var changes: [KeyChange] = []
        changes.append(contentsOf: result.added.map {
            KeyChange(keyId: $0.keyId, type: .added, key: $0)
        })
        changes.append(contentsOf: result.updated.map {
            KeyChange(keyId: $0.keyId, type: .updated, key: $0)
        })
        changes.append(contentsOf: result.removed.map {
            KeyChange(keyId: $0.keyId, type: .removed, key: $0)
        })
        return changes
    }
}
