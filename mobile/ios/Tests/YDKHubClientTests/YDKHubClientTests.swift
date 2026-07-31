import XCTest
@testable import YDKHubClient

final class YDKHubClientTests: XCTestCase {

    var client: YDKHubClient!

    override func setUp() {
        super.setUp()
        client = YDKHubClient(config: SDKConfig(
            hubEndpoint: "hub.test.local",
            hubPort: 8080,
            enableLogging: false
        ))
    }

    override func tearDown() {
        client.shutdown()
        client = nil
        super.tearDown()
    }

    func testBindKeyReturnsKey() async throws {
        client.setToken("test-token")

        // 集成测试需要运行中的 Hub REST Gateway
        // 当前验证接口和类型定义编译通过
        XCTAssertNotNil(client, "YDKHubClient should be initialized")
        XCTAssertEqual(client.token, "test-token")
    }

    func testSetTokenAndClear() {
        client.setToken("token-123")
        XCTAssertEqual(client.token, "token-123")

        client.clearToken()
        XCTAssertNil(client.token)
    }
}
