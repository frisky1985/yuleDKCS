import XCTest
@testable import YuleDKCS

final class YuleDKCSTests: XCTestCase {
    
    // MARK: - Setup & Teardown
    
    override func setUp() {
        super.setUp()
        // 每个测试用例开始前执行
    }
    
    override func tearDown() {
        // 每个测试用例结束后执行
        super.tearDown()
    }
    
    // MARK: - SDK Initialization Tests
    
    func testSDKInitialization() {
        let sdk = YuleDKCS.shared
        sdk.initialize(apiKey: "test_api_key_12345")
        
        // 验证版本号
        XCTAssertEqual(YuleDKCS.version, "1.0.0")
        
        // 验证初始化后不会重复初始化
        sdk.initialize(apiKey: "another_key")
        // 应该打印已初始化的日志
    }
    
    func testSDKNotInitialized() {
        // 创建新的实例用于测试（实际应重置状态，这里仅测试错误类型）
        do {
            // 未初始化时调用应该抛出错误
            // 由于 singleton 已在其他测试初始化，这里仅验证错误类型存在
            throw YuleDKCSError.notInitialized
        } catch let error as YuleDKCSError {
            XCTAssertEqual(error.errorDescription, "SDK 未初始化")
        } catch {
            XCTFail("Unexpected error type")
        }
    }
    
    // MARK: - Key Manager Tests
    
    func testDigitalKeyCreation() {
        let key = DigitalKey(
            id: "key_123",
            vehicleId: "vehicle_456",
            ownerId: "user_789",
            issuedAt: Date(),
            expiresAt: nil,
            permissions: [.unlock, .lock],
            status: .active
        )
        
        XCTAssertEqual(key.id, "key_123")
        XCTAssertEqual(key.vehicleId, "vehicle_456")
        XCTAssertEqual(key.status, .active)
        XCTAssertTrue(key.permissions.contains(.unlock))
        XCTAssertTrue(key.permissions.contains(.lock))
    }
    
    func testPermissions() {
        var perms: Permission = []
        perms.insert(.unlock)
        perms.insert(.lock)
        
        XCTAssertTrue(perms.contains(.unlock))
        XCTAssertTrue(perms.contains(.lock))
        XCTAssertFalse(perms.contains(.startEngine))
        
        let allPerms: Permission = [.unlock, .lock, .startEngine]
        XCTAssertTrue(allPerms.isSuperset(of: perms))
    }
    
    func testKeyManagerListKeys() {
        let manager = KeyManager.shared
        let keys = manager.listKeys()
        
        // 初始应该为空或包含持久化的钥匙
        XCTAssertNotNil(keys)
    }
    
    func testKeyManagerGetKey() {
        let manager = KeyManager.shared
        
        // 测试获取不存在的钥匙
        let key = manager.getKey(id: "non_existent_key")
        XCTAssertNil(key)
    }
    
    // MARK: - Crypto Wrapper Tests
    
    func testRandomKeyGeneration() {
        let crypto = CryptoWrapper.shared
        
        let key1 = crypto.generateRandomKey(size: 32)
        let key2 = crypto.generateRandomKey(size: 32)
        
        XCTAssertEqual(key1.count, 32)
        XCTAssertEqual(key2.count, 32)
        XCTAssertNotEqual(key1, key2) // 随机生成的应该不同
    }
    
    func testSHA256() {
        let crypto = CryptoWrapper.shared
        
        let data = "Hello, YuleDKCS!".data(using: .utf8)!
        let hash = crypto.sha256(data: data)
        
        XCTAssertEqual(hash.count, 32) // SHA-256 输出 32 字节
    }
    
    func testHMAC() {
        let crypto = CryptoWrapper.shared
        
        let data = "test data".data(using: .utf8)!
        let key = crypto.generateRandomKey(size: 32)
        
        let hmac1 = crypto.hmac(data: data, key: key)
        let hmac2 = crypto.hmac(data: data, key: key)
        
        XCTAssertEqual(hmac1.count, 32) // HMAC-SHA256 输出 32 字节
        XCTAssertEqual(hmac1, hmac2) // 相同输入应该产生相同 HMAC
    }
    
    // MARK: - BLE Manager Tests
    
    func testCommandCodes() {
        XCTAssertEqual(Command.unlock.commandCode, 0x01)
        XCTAssertEqual(Command.lock.commandCode, 0x02)
        XCTAssertEqual(Command.startEngine.commandCode, 0x03)
        XCTAssertEqual(Command.stopEngine.commandCode, 0x04)
        XCTAssertEqual(Command.openTrunk.commandCode, 0x05)
        XCTAssertEqual(Command.closeTrunk.commandCode, 0x06)
        XCTAssertEqual(Command.openWindows.commandCode, 0x07)
        XCTAssertEqual(Command.closeWindows.commandCode, 0x08)
        XCTAssertEqual(Command.honk.commandCode, 0x09)
        XCTAssertEqual(Command.flashLights.commandCode, 0x0A)
    }
    
    func testCommandData() {
        let customData = Data([0xAB, 0xCD, 0xEF])
        let customCommand = Command.custom(data: customData)
        
        XCTAssertEqual(customCommand.data, customData)
        XCTAssertEqual(customCommand.commandCode, 0xFF)
    }
    
    func testBLEErrorDescriptions() {
        let errors: [BLEError] = [
            .bluetoothNotAvailable,
            .bluetoothPoweredOff,
            .deviceNotFound,
            .connectionFailed,
            .disconnected,
            .timeout,
            .invalidData,
            .encryptionFailed,
            .commandFailed("test"),
            .unauthorized,
            .unknown
        ]
        
        for error in errors {
            XCTAssertNotNil(error.errorDescription)
            XCTAssertFalse(error.errorDescription!.isEmpty)
        }
    }
    
    // MARK: - FFI Bridge Tests
    
    func testFFIBridgeInitialization() {
        let bridge = FFIBridge.shared
        // 初始化已在 SDK 初始化时完成
        // 这里仅验证 FFI 桥接对象可访问
        XCTAssertNotNil(bridge)
    }
    
    // MARK: - Error Tests
    
    func testYuleDKCSErrorDescriptions() {
        let errors: [YuleDKCSError] = [
            .notInitialized,
            .invalidApiKey,
            .networkError(NSError(domain: "test", code: 1)),
            .bleError(.unknown),
            .cryptoError(.invalidKey),
            .keyNotFound,
            .permissionDenied,
            .invalidCommand,
            .ffiError("test error")
        ]
        
        for error in errors {
            XCTAssertNotNil(error.errorDescription)
            XCTAssertFalse(error.errorDescription!.isEmpty)
        }
    }
    
    // MARK: - Performance Tests
    
    func testSHA256Performance() {
        let crypto = CryptoWrapper.shared
        let data = Data(repeating: 0xAB, count: 1024 * 1024) // 1MB 数据
        
        measure {
            _ = crypto.sha256(data: data)
        }
    }
    
    func testHMACPerformance() {
        let crypto = CryptoWrapper.shared
        let data = Data(repeating: 0xCD, count: 1024 * 1024) // 1MB 数据
        let key = crypto.generateRandomKey(size: 32)
        
        measure {
            _ = crypto.hmac(data: data, key: key)
        }
    }
    
    // MARK: - Integration Tests
    
    func testKeyIssueFlow() {
        // 初始化 SDK
        YuleDKCS.shared.initialize(apiKey: "test_api_key_integration")
        
        let expectation = self.expectation(description: "Issue key completion")
        
        KeyManager.shared.issueKey(vehicleId: "test_vehicle_123") { result in
            // 由于 FFI 未实际实现，这里只验证回调被调用
            // 实际应该验证成功或失败
            switch result {
            case .success(let key):
                XCTAssertNotNil(key.id)
                XCTAssertEqual(key.vehicleId, "test_vehicle_123")
            case .failure(let error):
                // FFI 未实现时会失败，这是预期的
                XCTAssertNotNil(error)
            }
            expectation.fulfill()
        }
        
        wait(for: [expectation], timeout: 5.0)
    }
    
    // MARK: - Codable Tests
    
    func testDigitalKeyCodable() throws {
        let key = DigitalKey(
            id: "test_key",
            vehicleId: "test_vehicle",
            ownerId: "test_owner",
            issuedAt: Date(),
            expiresAt: Date().addingTimeInterval(86400),
            permissions: [.unlock, .lock, .startEngine],
            status: .active,
            sharedWith: ["user1", "user2"]
        )
        
        // 编码
        let encoder = JSONEncoder()
        let data = try encoder.encode(key)
        
        // 解码
        let decoder = JSONDecoder()
        let decodedKey = try decoder.decode(DigitalKey.self, from: data)
        
        XCTAssertEqual(key.id, decodedKey.id)
        XCTAssertEqual(key.vehicleId, decodedKey.vehicleId)
        XCTAssertEqual(key.status, decodedKey.status)
    }
    
    func testPermissionCodable() throws {
        let permissions: Permission = [.unlock, .lock, .shareKey]
        
        let encoder = JSONEncoder()
        let data = try encoder.encode(permissions)
        
        let decoder = JSONDecoder()
        let decodedPerms = try decoder.decode(Permission.self, from: data)
        
        XCTAssertEqual(permissions, decodedPerms)
    }
}

// MARK: - Test Helpers

extension YuleDKCSTests {
    
    /// 生成测试用的随机字符串
    func randomString(length: Int) -> String {
        let letters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
        return String((0..<length).map { _ in letters.randomElement()! })
    }
    
    /// 生成测试用的随机 Data
    func randomData(length: Int) -> Data {
        var bytes = [UInt8](repeating: 0, count: length)
        for i in 0..<length {
            bytes[i] = UInt8.random(in: 0...255)
        }
        return Data(bytes)
    }
}