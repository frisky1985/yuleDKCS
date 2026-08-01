import XCTest
@testable import YDKKeyManager
@testable import YDKHubClient

/// 离线授权回退机制（方案 A）— iOS 单测
///
/// 覆盖: docs/sdk/OFFLINE-FALLBACK-DESIGN.md §3.1 裁决规则全分支
///   - 状态裁决: REVOKED / SUSPENDED / EXPIRED / 未知状态 fail-closed
///   - 有效期窗口: validUntil 过期 / validFrom 未生效 / validUntil==0 永久
///   - 离线宽限期: 超窗拒绝 / 窗内允许 / lastSyncAt==0 跳过 / 自定义宽限
///   - KeyManager 入口: 命中缓存 / 未命中返回 nil / 撤销与陈旧缓存拒绝
final class YDKOfflineAuthorizerTests: XCTestCase {

    // MARK: - 工具

    /// 构造测试钥匙
    private func makeKey(
        keyId: String = "key-1",
        status: String = "ACTIVE",
        validFrom: Int64 = 0,
        validUntil: Int64 = 0
    ) -> YDKKey {
        YDKKey(
            keyId: keyId,
            vehicleId: "VH-1",
            deviceId: "dev-1",
            vehicleName: nil,
            keyType: "OWNER",
            protocol: nil,
            status: status,
            validFrom: validFrom,
            validUntil: validUntil,
            createdAt: 0
        )
    }

    private let now = Date(timeIntervalSince1970: 1_800_000_000) // 固定参考时间

    /// 秒 → 毫秒时间戳
    private func ms(_ t: TimeInterval) -> Int64 { Int64(t * 1000) }

    // MARK: - 状态裁决

    func testActiveKeyIsAllowed() {
        let key = makeKey(status: "ACTIVE")
        let result = YDKOfflineAuthorizer.authorize(key: key, now: now, lastSyncAtMillis: ms(now.timeIntervalSince1970))
        XCTAssertTrue(result.allowed)
        XCTAssertNil(result.reason)
    }

    func testRevokedKeyIsDenied() {
        let key = makeKey(status: "REVOKED")
        let result = YDKOfflineAuthorizer.authorize(key: key, now: now, lastSyncAtMillis: ms(now.timeIntervalSince1970))
        XCTAssertFalse(result.allowed)
        XCTAssertEqual(result.reason, .revoked)
    }

    func testSuspendedKeyIsDenied() {
        let key = makeKey(status: "SUSPENDED")
        let result = YDKOfflineAuthorizer.authorize(key: key, now: now, lastSyncAtMillis: ms(now.timeIntervalSince1970))
        XCTAssertFalse(result.allowed)
        XCTAssertEqual(result.reason, .suspended)
    }

    func testExpiredStatusKeyIsDenied() {
        let key = makeKey(status: "EXPIRED")
        let result = YDKOfflineAuthorizer.authorize(key: key, now: now, lastSyncAtMillis: ms(now.timeIntervalSince1970))
        XCTAssertFalse(result.allowed)
        XCTAssertEqual(result.reason, .expired)
    }

    func testUnknownStatusFailsClosed() {
        let key = makeKey(status: "PENDING_WHATEVER")
        let result = YDKOfflineAuthorizer.authorize(key: key, now: now, lastSyncAtMillis: ms(now.timeIntervalSince1970))
        XCTAssertFalse(result.allowed, "未知状态必须 fail-closed")
        XCTAssertEqual(result.reason, .revoked)
    }

    // MARK: - 有效期窗口

    func testKeyPastValidUntilIsDenied() {
        let key = makeKey(validFrom: 0, validUntil: ms(now.timeIntervalSince1970 - 60))
        let result = YDKOfflineAuthorizer.authorize(key: key, now: now, lastSyncAtMillis: ms(now.timeIntervalSince1970))
        XCTAssertFalse(result.allowed)
        XCTAssertEqual(result.reason, .expired)
    }

    func testKeyWithinValidityIsAllowed() {
        let key = makeKey(
            validFrom: ms(now.timeIntervalSince1970 - 3600),
            validUntil: ms(now.timeIntervalSince1970 + 3600)
        )
        let result = YDKOfflineAuthorizer.authorize(key: key, now: now, lastSyncAtMillis: ms(now.timeIntervalSince1970))
        XCTAssertTrue(result.allowed)
    }

    func testKeyBeforeValidFromIsDenied() {
        let key = makeKey(validFrom: ms(now.timeIntervalSince1970 + 3600), validUntil: 0)
        let result = YDKOfflineAuthorizer.authorize(key: key, now: now, lastSyncAtMillis: ms(now.timeIntervalSince1970))
        XCTAssertFalse(result.allowed)
        XCTAssertEqual(result.reason, .notYetValid)
    }

    func testZeroValidUntilMeansPermanent() {
        // validUntil == 0 表示永久有效, 不应被过期规则拒绝
        let key = makeKey(validFrom: 0, validUntil: 0)
        let result = YDKOfflineAuthorizer.authorize(key: key, now: now, lastSyncAtMillis: ms(now.timeIntervalSince1970))
        XCTAssertTrue(result.allowed)
    }

    // MARK: - 离线宽限期

    func testStaleCacheBeyondGraceIsDenied() {
        let key = makeKey()
        // 上次同步在 8 天前 > 默认宽限期 7 天
        let lastSync = ms(now.timeIntervalSince1970 - 8 * 24 * 3600)
        let result = YDKOfflineAuthorizer.authorize(key: key, now: now, lastSyncAtMillis: lastSync)
        XCTAssertFalse(result.allowed)
        XCTAssertEqual(result.reason, .staleCache)
    }

    func testFreshCacheWithinGraceIsAllowed() {
        let key = makeKey()
        // 上次同步在 1 天前 < 宽限期
        let lastSync = ms(now.timeIntervalSince1970 - 24 * 3600)
        let result = YDKOfflineAuthorizer.authorize(key: key, now: now, lastSyncAtMillis: lastSync)
        XCTAssertTrue(result.allowed)
    }

    func testCustomGraceCanBeTightened() {
        let key = makeKey()
        let lastSync = ms(now.timeIntervalSince1970 - 2 * 3600) // 2 小时前
        // 自定义宽限期 1 小时 → 拒绝
        let tight = YDKOfflineAuthorizer.authorize(
            key: key, now: now, lastSyncAtMillis: lastSync, maxOfflineGrace: 3600
        )
        XCTAssertFalse(tight.allowed)
        XCTAssertEqual(tight.reason, .staleCache)
        // 自定义宽限期 3 小时 → 允许
        let loose = YDKOfflineAuthorizer.authorize(
            key: key, now: now, lastSyncAtMillis: lastSync, maxOfflineGrace: 3 * 3600
        )
        XCTAssertTrue(loose.allowed)
    }

    func testZeroLastSyncSkipsGraceCheck() {
        // 无缓存历史时跳过宽限期检查（避免误杀首次离线）, 由状态/有效期兜底
        let key = makeKey()
        let result = YDKOfflineAuthorizer.authorize(key: key, now: now, lastSyncAtMillis: 0)
        XCTAssertTrue(result.allowed)
    }

    // MARK: - KeyManager 入口

    func testAuthorizeOfflineUseReturnsNilForMissingKey() {
        let (keyManager, _) = makeKeyManager()
        XCTAssertNil(keyManager.authorizeOfflineUse(keyId: "nonexistent", at: now))
    }

    func testAuthorizeOfflineUseAllowedWithFreshCache() {
        let (keyManager, cacheURL) = makeKeyManager()
        writeToCache(cacheURL: cacheURL, keys: [makeKey(validUntil: ms(now.timeIntervalSince1970 + 3600))],
                     lastSyncAt: ms(now.timeIntervalSince1970))

        let result = keyManager.authorizeOfflineUse(keyId: "key-1", at: now)
        XCTAssertNotNil(result)
        XCTAssertTrue(result!.allowed)
    }

    func testAuthorizeOfflineUseDeniedForRevokedCachedKey() {
        let (keyManager, cacheURL) = makeKeyManager()
        writeToCache(cacheURL: cacheURL, keys: [makeKey(status: "REVOKED")], lastSyncAt: ms(now.timeIntervalSince1970))

        let result = keyManager.authorizeOfflineUse(keyId: "key-1", at: now)
        XCTAssertNotNil(result)
        XCTAssertFalse(result!.allowed)
        XCTAssertEqual(result!.reason, .revoked)
    }

    func testAuthorizeOfflineUseDeniedForStaleCache() {
        let (keyManager, cacheURL) = makeKeyManager()
        // 缓存 8 天前同步 → 超默认宽限期 7 天
        writeToCache(cacheURL: cacheURL, keys: [makeKey()], lastSyncAt: ms(now.timeIntervalSince1970 - 8 * 24 * 3600))

        let result = keyManager.authorizeOfflineUse(keyId: "key-1", at: now)
        XCTAssertNotNil(result)
        XCTAssertFalse(result!.allowed)
        XCTAssertEqual(result!.reason, .staleCache)
    }

    // MARK: - 测试夹具

    /// 创建 keyManager 并返回其缓存文件 URL（两者必须一致, 才能验证入口路径）
    private func makeKeyManager() -> (manager: YDKKeyManager, cacheURL: URL) {
        let tempURL = FileManager.default.temporaryDirectory
            .appendingPathComponent("offline_auth_test_\(UUID().uuidString).json")
        let client = YDKHubClient(config: SDKConfig(hubEndpoint: "hub.test.local", enableLogging: false))
        let manager = YDKKeyManager(hubClient: client, cacheFileURL: tempURL, enableLogging: false)
        return (manager, tempURL)
    }

    /// 直接向指定缓存文件写入钥匙列表 + 自定义 lastSyncAt
    private func writeToCache(cacheURL: URL, keys: [YDKKey], lastSyncAt: Int64) {
        let data = YDKKeyCache.CacheData(version: 1, lastSyncAt: lastSyncAt, keys: keys)
        let encoder = JSONEncoder()
        guard let encoded = try? encoder.encode(data) else {
            XCTFail("缓存编码失败")
            return
        }
        do {
            try encoded.write(to: cacheURL, options: .atomic)
        } catch {
            XCTFail("缓存写入失败: \(error)")
        }
    }
}
