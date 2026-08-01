import XCTest
@testable import YDKKeyManager
@testable import YDKHubClient

/// Phase 4.1 / W1 — KeyManager Push 触发同步 + 状态同步/离线推断测试 (iOS)
///
/// 覆盖审计缺口:
///   - Push 回调入口 handleKeyStatusPush(keyId:): 错误传播 + delegate 失败回调
///   - 状态同步: SyncResult.hasChanges 差异判定纯逻辑
///   - 离线推断: getKey(preferCache:) 缓存优先语义 + 缓存跨实例持久化（无网可用）
///
/// 说明: YDKHubClient 的 URLSession transport 不可注入, 成功路径（listKeys
/// 返回云端钥匙后的 added/updated/removed 归类）由 Android MockWebServer 测试
/// 与 E2E 脚本覆盖; 本文件验证 KeyManager 自身可离线断言的行为与错误路径。
final class YDKKeyManagerPushSyncTests: XCTestCase {

    /// 记录回调次数的 Mock Delegate
    private final class MockDelegate: YDKKeyManagerDelegate {
        var failureCount = 0
        var changeCount = 0
        var lastError: Error?

        func keyManager(_ manager: YDKKeyManager, didDetectChanges changes: [KeyChange]) {
            changeCount += 1
        }

        func keyManager(_ manager: YDKKeyManager, syncDidFailWith error: Error) {
            failureCount += 1
            lastError = error
        }
    }

    var keyManager: YDKKeyManager!
    var tempCacheURL: URL!

    override func setUp() {
        super.setUp()
        tempCacheURL = FileManager.default.temporaryDirectory
            .appendingPathComponent("test_push_sync_\(UUID().uuidString).json")

        let client = YDKHubClient(config: SDKConfig(
            hubEndpoint: "hub.test.local",
            enableLogging: false
        ))
        keyManager = YDKKeyManager(
            hubClient: client,
            cacheFileURL: tempCacheURL,
            enableLogging: false
        )
    }

    override func tearDown() {
        keyManager.clearCache()
        try? FileManager.default.removeItem(at: tempCacheURL)
        keyManager = nil
        super.tearDown()
    }

    /// 向缓存写入一把钥匙（模拟上次同步落盘）
    private func seedCache(keyId: String, status: String) {
        let cache = YDKKeyCache(fileURL: tempCacheURL, logger: YDKLogger(enabled: false))
        cache.write(keys: [
            YDKKey(keyId: keyId, vehicleId: "VH-1", deviceId: "dev-1",
                   vehicleName: nil, keyType: "OWNER", protocol: nil,
                   status: status, validFrom: 0, validUntil: 0, createdAt: 0)
        ])
    }

    private func makeKey(keyId: String, status: String = "ACTIVE") -> YDKKey {
        YDKKey(keyId: keyId, vehicleId: "VH-1", deviceId: "dev-1",
               vehicleName: nil, keyType: "OWNER", protocol: nil,
               status: status, validFrom: 0, validUntil: 0, createdAt: 0)
    }

    // MARK: - Push 回调入口

    /// Push 触发的同步在无网络时必须向调用方传播错误（不静默吞掉）。
    func testHandleKeyStatusPushPropagatesNetworkError() async {
        do {
            _ = try await keyManager.handleKeyStatusPush(keyId: "key-1")
            XCTFail("Should have thrown network error")
        } catch {
            XCTAssertTrue(error is YDKError, "expected YDKError, got \(error)")
        }
    }

    /// 同步失败必须回调 delegate.syncDidFailWith, 且不得触发 didDetectChanges。
    func testSyncFailureNotifiesDelegateFailureCallback() async {
        let delegate = MockDelegate()
        keyManager.delegate = delegate

        do {
            _ = try await keyManager.syncFromHub()
            XCTFail("Should have thrown network error")
        } catch {
            XCTAssertTrue(error is YDKError)
        }

        XCTAssertEqual(delegate.failureCount, 1, "同步失败必须回调 syncDidFailWith")
        XCTAssertTrue(delegate.lastError is YDKError, "失败回调必须携带 YDKError")
        XCTAssertEqual(delegate.changeCount, 0, "失败路径不得触发 didDetectChanges")
    }

    // MARK: - 状态同步纯逻辑

    /// SyncResult.hasChanges: added/updated/removed 任一非空即为有变更。
    func testSyncResultHasChangesPureLogic() {
        let key = makeKey(keyId: "key-1")

        XCTAssertTrue(SyncResult(added: [key], updated: [], removed: [], unchanged: 0).hasChanges)
        XCTAssertTrue(SyncResult(added: [], updated: [key], removed: [], unchanged: 1).hasChanges)
        XCTAssertTrue(SyncResult(added: [], updated: [], removed: [key], unchanged: 0).hasChanges)
        XCTAssertFalse(SyncResult(added: [], updated: [], removed: [], unchanged: 3).hasChanges,
                       "无任何差异时 hasChanges 必须为 false")
    }

    /// KeyChange.ChangeType 的 rawValue 与 delegate 通知语义一致。
    func testKeyChangeChangeTypeRawValues() {
        XCTAssertEqual(KeyChange.ChangeType.added.rawValue, "added")
        XCTAssertEqual(KeyChange.ChangeType.updated.rawValue, "updated")
        XCTAssertEqual(KeyChange.ChangeType.removed.rawValue, "removed")
    }

    // MARK: - 离线推断

    /// 默认 preferCache=true: 无网络时从本地缓存读取（离线推断）;
    /// preferCache=false 跳过缓存返回 nil; 不存在的钥匙返回 nil。
    func testGetKeyOfflineCacheSemantics() {
        seedCache(keyId: "key-offline-1", status: "ACTIVE")

        let cached = keyManager.getKey(keyId: "key-offline-1")
        XCTAssertNotNil(cached, "离线时 preferCache=true 必须命中本地缓存")
        XCTAssertEqual(cached?.keyId, "key-offline-1")
        XCTAssertEqual(cached?.status, "ACTIVE")

        XCTAssertNil(keyManager.getKey(keyId: "key-offline-1", preferCache: false),
                     "preferCache=false 必须跳过本地缓存")
        XCTAssertNil(keyManager.getKey(keyId: "missing"))
    }

    /// 缓存文件持久化: 新 KeyManager 实例（同一缓存文件）可离线恢复上次同步结果。
    func testLocalKeysPersistAcrossKeyManagerInstances() {
        seedCache(keyId: "key-persist-1", status: "ACTIVE")

        let client2 = YDKHubClient(config: SDKConfig(
            hubEndpoint: "hub.test.local",
            enableLogging: false
        ))
        let manager2 = YDKKeyManager(hubClient: client2, cacheFileURL: tempCacheURL, enableLogging: false)
        defer { manager2.clearCache() }

        XCTAssertEqual(manager2.getLocalKeys().count, 1, "缓存必须跨 KeyManager 实例持久化")
        XCTAssertEqual(manager2.getLocalKeys().first?.keyId, "key-persist-1")
    }
}
