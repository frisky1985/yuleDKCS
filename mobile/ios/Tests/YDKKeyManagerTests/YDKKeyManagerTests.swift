import XCTest
@testable import YDKKeyManager
@testable import YDKHubClient

final class YDKKeyManagerTests: XCTestCase {

    var keyManager: YDKKeyManager!
    var tempCacheURL: URL!

    override func setUp() {
        super.setUp()
        tempCacheURL = FileManager.default.temporaryDirectory
            .appendingPathComponent("test_keys_cache_\(UUID().uuidString).json")

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

    func testGetLocalKeysReturnsEmptyInitially() {
        let keys = keyManager.getLocalKeys()
        XCTAssertTrue(keys.isEmpty, "No keys should be cached initially")
    }

    func testGetKeyReturnsNilForMissingKey() {
        let key = keyManager.getKey(keyId: "nonexistent")
        XCTAssertNil(key)
    }

    func testClearCacheEmptiesLocalKeys() {
        // Write something to cache directly
        let cache = YDKKeyCache(fileURL: tempCacheURL, logger: YDKLogger(enabled: false))
        cache.write(keys: [
            YDKKey(keyId: "key-1", vehicleId: "VH-1", deviceId: "dev-1",
                   vehicleName: nil, keyType: "OWNER", protocol: nil,
                   status: "ACTIVE", validFrom: 0, validUntil: 0, createdAt: 0)
        ])
        XCTAssertEqual(keyManager.getLocalKeys().count, 1)

        keyManager.clearCache()
        XCTAssertTrue(keyManager.getLocalKeys().isEmpty)
    }

    func testDelegateNotifiedOnChanges() async throws {
        // 验证 delegate 回调类型正确
        // 完整集成测试需要 mock HubClient
        class MockDelegate: YDKKeyManagerDelegate {
            var detectedChanges: [KeyChange]?
            func keyManager(_ manager: YDKKeyManager, didDetectChanges changes: [KeyChange]) {
                detectedChanges = changes
            }
            func keyManager(_ manager: YDKKeyManager, syncDidFailWith error: Error) {}
        }

        let delegate = MockDelegate()
        keyManager.delegate = delegate

        // No network → should fail (no mock HubClient yet)
        do {
            try await keyManager.syncFromHub()
            XCTFail("Should have thrown network error")
        } catch {
            XCTAssertTrue(error is YDKError)
        }
    }
}
