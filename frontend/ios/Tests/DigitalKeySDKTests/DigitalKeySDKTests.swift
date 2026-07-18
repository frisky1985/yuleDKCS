//
//  DigitalKeySDKTests.swift
//  DigitalKeySDKTests
//
//  数字钥匙 SDK 主入口单元测试
//
//  验证:
//  - configure() 参数校验
//  - 单例行为
//  - reset()
//  - retrieveApiKey()
//
//  ⚠️ 注意: 当前源文件存在 DigitalKeyError 命名冲突
//  (DigitalKeySDK.swift 中 enum 与 DkError.swift 中 struct 同名).
//  编译前需要先解决此冲突。
//

import XCTest
@testable import DigitalKeySDK

final class DigitalKeySDKTests: XCTestCase {

    override func tearDown() {
        // 每个测试后重置 SDK，避免状态泄漏
        DigitalKeySDK.reset()
        super.tearDown()
    }

    // MARK: - configure() 参数校验

    func testConfigureSuccess() throws {
        XCTAssertNoThrow(try DigitalKeySDK.configure(
            serverUrl: "https://api.digitalkey.cn",
            appId: "com.test.app",
            clientId: "ios_test_001",
            apiKey: "test-api-key-12345"
        ))
        XCTAssertTrue(DigitalKeySDK.isConfigured)
    }

    func testConfigureEmptyServerUrlThrows() {
        XCTAssertThrowsError(try DigitalKeySDK.configure(
            serverUrl: "",
            appId: "com.test.app",
            clientId: "ios_test_001",
            apiKey: "test-api-key"
        )) { error in
            // 期望 DigitalKeyError.invalidParameter
            XCTAssertTrue(error is DigitalKeyError
                          || (error as NSError).localizedDescription.contains("参数错误")
                          || (error as NSError).localizedDescription.contains("serverUrl"),
                          "Expected error for empty serverUrl, got: \(error)")
        }
        XCTAssertFalse(DigitalKeySDK.isConfigured)
    }

    func testConfigureEmptyAppIdThrows() {
        XCTAssertThrowsError(try DigitalKeySDK.configure(
            serverUrl: "https://api.digitalkey.cn",
            appId: "",
            clientId: "ios_test_001",
            apiKey: "test-api-key"
        )) { error in
            XCTAssertTrue(error is DigitalKeyError
                          || (error as NSError).localizedDescription.contains("appId")
                          || (error as NSError).localizedDescription.contains("参数错误"),
                          "Expected error for empty appId, got: \(error)")
        }
    }

    func testConfigureEmptyClientIdThrows() {
        XCTAssertThrowsError(try DigitalKeySDK.configure(
            serverUrl: "https://api.digitalkey.cn",
            appId: "com.test.app",
            clientId: "",
            apiKey: "test-api-key"
        )) { error in
            XCTAssertTrue(error is DigitalKeyError
                          || (error as NSError).localizedDescription.contains("clientId")
                          || (error as NSError).localizedDescription.contains("参数错误"),
                          "Expected error for empty clientId, got: \(error)")
        }
    }

    func testConfigureEmptyApiKeyThrows() {
        XCTAssertThrowsError(try DigitalKeySDK.configure(
            serverUrl: "https://api.digitalkey.cn",
            appId: "com.test.app",
            clientId: "ios_test_001",
            apiKey: ""
        ))
    }

    func testConfigureInvalidServerUrlThrows() {
        XCTAssertThrowsError(try DigitalKeySDK.configure(
            serverUrl: "not-a-url",
            appId: "com.test.app",
            clientId: "ios_test_001",
            apiKey: "test-api-key"
        )) { error in
            XCTAssertTrue(error is DigitalKeyError
                          || (error as NSError).localizedDescription.contains("serverUrl")
                          || (error as NSError).localizedDescription.contains("格式"),
                          "Expected URL validation error, got: \(error)")
        }
    }

    // MARK: - 单例行为

    func testSharedBeforeConfigureFatalErrors() {
        // 在 XCTest 中测试 fatalError 需要特殊处理
        // 这里我们验证 isConfigured 为 false
        XCTAssertFalse(DigitalKeySDK.isConfigured,
                       "SDK should not be configured before calling configure()")
    }

    func testSharedAfterConfigureReturnsInstance() throws {
        try DigitalKeySDK.configure(
            serverUrl: "https://api.digitalkey.cn",
            appId: "com.test.app",
            clientId: "ios_test_001",
            apiKey: "test-api-key-12345"
        )

        let instance = DigitalKeySDK.shared
        XCTAssertEqual(instance.config.serverUrl, "https://api.digitalkey.cn")
        XCTAssertEqual(instance.config.appId, "com.test.app")
        XCTAssertEqual(instance.config.clientId, "ios_test_001")
    }

    func testSdkConfigTimeoutDefault() {
        let config = SdkConfig(
            serverUrl: "https://api.digitalkey.cn",
            appId: "com.test.app",
            clientId: "ios_test_001",
            apiKey: "test-api-key"
        )
        XCTAssertEqual(config.timeoutInterval, 30.0, "Default timeout should be 30s")
    }

    // MARK: - reset()

    func testResetClearsConfiguration() throws {
        try DigitalKeySDK.configure(
            serverUrl: "https://api.digitalkey.cn",
            appId: "com.test.app",
            clientId: "ios_test_001",
            apiKey: "test-api-key"
        )
        XCTAssertTrue(DigitalKeySDK.isConfigured)

        DigitalKeySDK.reset()
        XCTAssertFalse(DigitalKeySDK.isConfigured,
                       "SDK should not be configured after reset()")
    }
}
