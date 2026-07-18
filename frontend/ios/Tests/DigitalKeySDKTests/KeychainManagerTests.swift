//
//  KeychainManagerTests.swift
//  DigitalKeySDKTests
//
//  Keychain 安全存储单元测试
//
//  ⚠️ 这些测试操作真机 Keychain。
//  在 macOS 上测试时使用单独的 service 名称避免影响生产环境。
//
//  验证:
//  - store/retrieve roundtrip (String 和 Data)
//  - 更新 (update) 和删除 (delete)
//  - 存在性检查 (contains)
//  - 条目不存在时抛出 itemNotFound
//  - clear() 清空
//  - SDK 便捷扩展
//

import XCTest
@testable import DigitalKeySDK

final class KeychainManagerTests: XCTestCase {

    /// 测试专用 Keychain 实例（独立 service，不影响生产）
    private let keychain = KeychainManager(service: "com.digitalkey.sdk.test")

    override func tearDown() {
        keychain.clear()
        super.tearDown()
    }

    // MARK: - String Roundtrip

    func testStoreAndRetrieveString() throws {
        try keychain.store(key: "testKey1", value: "HelloKeychain")
        let retrieved = try keychain.retrieve(key: "testKey1")
        XCTAssertEqual(retrieved, "HelloKeychain")
    }

    func testStoreAndRetrieveEmptyString() throws {
        try keychain.store(key: "emptyKey", value: "")
        let retrieved = try keychain.retrieve(key: "emptyKey")
        XCTAssertEqual(retrieved, "")
    }

    func testStoreAndRetrieveUnicodeString() throws {
        let unicode = "数字钥匙SDK 🔑👍"
        try keychain.store(key: "unicodeKey", value: unicode)
        let retrieved = try keychain.retrieve(key: "unicodeKey")
        XCTAssertEqual(retrieved, unicode)
    }

    // MARK: - Data Roundtrip

    func testStoreAndRetrieveData() throws {
        let data = "BinaryData".data(using: .utf8)!
        try keychain.store(key: "dataKey", data: data)
        let retrieved = try keychain.retrieve(key: "dataKey")
        XCTAssertEqual(retrieved, data)
    }

    func testStoreAndRetrieveLargeData() throws {
        // 64KB data
        let largeData = Data(repeating: 0xAB, count: 65_536)
        try keychain.store(key: "largeKey", data: largeData)
        let retrieved = try keychain.retrieve(key: "largeKey")
        XCTAssertEqual(retrieved, largeData)
    }

    func testStoreAndRetrieveEmptyData() throws {
        let emptyData = Data()
        try keychain.store(key: "emptyDataKey", data: emptyData)
        let retrieved = try keychain.retrieve(key: "emptyDataKey")
        XCTAssertEqual(retrieved, emptyData)
    }

    // MARK: - Update

    func testUpdateExistingItem() throws {
        try keychain.store(key: "updateKey", value: "oldValue")
        try keychain.update(key: "updateKey", value: "newValue")
        let retrieved = try keychain.retrieve(key: "updateKey")
        XCTAssertEqual(retrieved, "newValue")
    }

    func testUpdateDataItem() throws {
        let oldData = "old".data(using: .utf8)!
        let newData = "newLonger".data(using: .utf8)!
        try keychain.store(key: "updateDataKey", data: oldData)
        try keychain.update(key: "updateDataKey", data: newData)
        let retrieved = try keychain.retrieve(key: "updateDataKey")
        XCTAssertEqual(retrieved, newData)
    }

    // MARK: - Delete

    func testDeleteExistingItem() throws {
        try keychain.store(key: "deleteKey", value: "toDelete")
        try keychain.delete(key: "deleteKey")
        XCTAssertThrowsError(try keychain.retrieve(key: "deleteKey")) { error in
            XCTAssertTrue(error is KeychainError)
            if case KeychainError.itemNotFound = error {
                // 正确
            } else {
                XCTFail("Expected itemNotFound, got \(error)")
            }
        }
    }

    func testDeleteNonExistentItemDoesNotThrow() {
        XCTAssertNoThrow(try keychain.delete(key: "nonExistentKey"))
    }

    // MARK: - Contains

    func testContainsExistingItem() throws {
        try keychain.store(key: "existsKey", value: "yes")
        XCTAssertTrue(keychain.contains(key: "existsKey"))
    }

    func testContainsNonExistentItem() {
        XCTAssertFalse(keychain.contains(key: "neverStoredKey"))
    }

    func testContainsAfterDelete() throws {
        try keychain.store(key: "tempKey", value: "temp")
        XCTAssertTrue(keychain.contains(key: "tempKey"))
        try keychain.delete(key: "tempKey")
        XCTAssertFalse(keychain.contains(key: "tempKey"))
    }

    // MARK: - Clear

    func testClearRemovesAllItems() throws {
        try keychain.store(key: "key1", value: "val1")
        try keychain.store(key: "key2", value: "val2")
        try keychain.store(key: "key3", value: "val3")

        keychain.clear()

        XCTAssertFalse(keychain.contains(key: "key1"))
        XCTAssertFalse(keychain.contains(key: "key2"))
        XCTAssertFalse(keychain.contains(key: "key3"))
    }

    // MARK: - retrieve throws itemNotFound

    func testRetrieveNonExistentKeyThrows() {
        XCTAssertThrowsError(try keychain.retrieve(key: "neverStored")) { error in
            guard let keychainError = error as? KeychainError else {
                XCTFail("Expected KeychainError, got \(error)")
                return
            }
            if case KeychainError.itemNotFound = keychainError {
                // 正确
            } else {
                XCTFail("Expected itemNotFound, got \(keychainError)")
            }
        }
    }

    // MARK: - SDK 便捷扩展

    func testSDKInstanceUsesCorrectService() {
        let sdkInstance = KeychainManager.sdkInstance
        // 验证通过 store/retrieve roundtrip
        XCTAssertNoThrow(try sdkInstance.storeApiKey("test-sdk-api-key"))
        let retrieved = try? sdkInstance.retrieveApiKey()
        XCTAssertEqual(retrieved, "test-sdk-api-key")
        try? sdkInstance.deleteApiKey()
    }

    func testHasApiKey() throws {
        let sdkInstance = KeychainManager.sdkInstance
        try? sdkInstance.deleteApiKey()

        XCTAssertFalse(sdkInstance.hasApiKey)

        try sdkInstance.storeApiKey("test-key")
        XCTAssertTrue(sdkInstance.hasApiKey)

        try sdkInstance.deleteApiKey()
        XCTAssertFalse(sdkInstance.hasApiKey)
    }
}
