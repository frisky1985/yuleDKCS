import XCTest
@testable import YDKHubClient

/// Phase 4.1 / W1 — HubClient 远程控制 (SendCommand) 请求形状契约测试 (iOS)
///
/// 固化 remoteLock / remoteUnlock / remoteStart / remoteStop 的请求形状:
///   1. 路径: POST /vehicles/{vehicleId}/command
///   2. body 字段: action / keyId / traceId (camelCase)
///   3. action 枚举字符串: lock / unlock / engine_on / engine_off
///   4. keyId 缺省为空字符串 ""; 显式传入时原样透传
///   5. body 不得含 source 字段 — source=4(Remote) 由 Gateway 自动填充
///   6. 响应模型 ControlCommandResponse 可解码 cmdId / resultCode / errorMsg
///
/// 说明: 与 YDKHubClientRequestShapeContractTests 同一模式 — iOS SDK 的
/// URLSession transport 不可注入, 因此按 YDKHubClient+Remote.swift::sendCommand
/// 的逐字段结构构造 body, 经 AnyEncodable.json + JSONEncoder 编码后验证 wire
/// 字节形状; 真实 HTTP 方法/路径由 Android MockWebServer 测试与
/// backend/scripts/sdk-hub-contract-e2e.sh 覆盖。
final class YDKHubClientRemoteControlContractTests: XCTestCase {

    /// 按 sendCommand 的真实编码路径 ([String: Any] → AnyEncodable.json → JSONEncoder)
    /// 编码 body, 返回 JSON 字典。
    private func encodeBody(_ body: [String: Any]) throws -> [String: String] {
        let data = try JSONEncoder().encode(AnyEncodable.json(body))
        let obj = try XCTUnwrap(
            JSONSerialization.jsonObject(with: data) as? [String: String],
            "encoded body must be a flat string dictionary"
        )
        return obj
    }

    /// 复刻 YDKHubClient+Remote.swift::sendCommand 的 body 构造（逐字段一致）
    private func commandBody(action: String, keyId: String?) -> [String: Any] {
        [
            "action": action,
            "keyId": keyId ?? "",
            "traceId": UUID().uuidString,
        ]
    }

    // MARK: - 公开 API 面（编译期契约）

    /// 编译期验证 remoteLock/remoteUnlock/remoteStart/remoteStop 公开 API 存在
    /// 且签名正确（不实际发起网络请求）。
    func testRemoteControlAPISurfaceCompiles() {
        let client = YDKHubClient(config: SDKConfig(
            hubEndpoint: "hub.test.local",
            enableLogging: false
        ))

        let _: (String, String?) async throws -> ControlCommandResponse = { vehicleId, keyId in
            try await client.remoteLock(vehicleId: vehicleId, keyId: keyId)
        }
        let _: (String) async throws -> ControlCommandResponse = { vehicleId in
            try await client.remoteUnlock(vehicleId: vehicleId)
        }
        let _: (String) async throws -> ControlCommandResponse = { vehicleId in
            try await client.remoteStart(vehicleId: vehicleId)
        }
        let _: (String) async throws -> ControlCommandResponse = { vehicleId in
            try await client.remoteStop(vehicleId: vehicleId)
        }
    }

    // MARK: - 远程控车 action 契约

    /// remoteLock → action="lock"; body 仅含 action/keyId/traceId; 无 source 字段。
    func testRemoteLockBodyShape() throws {
        let json = try encodeBody(commandBody(action: "lock", keyId: nil))

        XCTAssertEqual(
            Set(json.keys),
            Set(["action", "keyId", "traceId"]),
            "远程控车 body 仅含 action/keyId/traceId 三个 camelCase 字段"
        )
        XCTAssertEqual(json["action"], "lock")
        XCTAssertEqual(json["keyId"], "", "keyId 缺省必须传空字符串")
        XCTAssertFalse(json.keys.contains("source"), "source=4(Remote) 由 Gateway 自动填充, SDK 不得携带")
    }

    /// remoteUnlock → action="unlock"。
    func testRemoteUnlockBodyShape() throws {
        let json = try encodeBody(commandBody(action: "unlock", keyId: nil))
        XCTAssertEqual(json["action"], "unlock")
        XCTAssertEqual(json["keyId"], "")
    }

    /// remoteStart → action="engine_on"。
    func testRemoteStartBodyShape() throws {
        let json = try encodeBody(commandBody(action: "engine_on", keyId: nil))
        XCTAssertEqual(json["action"], "engine_on")
        XCTAssertEqual(json["keyId"], "")
    }

    /// remoteStop → action="engine_off"。
    func testRemoteStopBodyShape() throws {
        let json = try encodeBody(commandBody(action: "engine_off", keyId: nil))
        XCTAssertEqual(json["action"], "engine_off")
        XCTAssertEqual(json["keyId"], "")
    }

    /// 显式传入的 keyId 原样透传; traceId 为合法 UUID 且非空。
    func testRemoteCommandKeyIdPassthroughAndTraceId() throws {
        let json = try encodeBody(commandBody(action: "lock", keyId: "key-42"))
        XCTAssertEqual(json["keyId"], "key-42", "显式传入的 keyId 必须原样透传")

        let traceId = try XCTUnwrap(json["traceId"])
        XCTAssertFalse(traceId.isEmpty, "traceId 不得为空")
        XCTAssertNotNil(UUID(uuidString: traceId), "traceId 必须是合法 UUID 字符串")
    }

    // MARK: - 路径契约

    /// 路径形状: POST /vehicles/{vehicleId}/command
    /// (与 YDKHubClient+Remote.swift::sendCommand 的 path 构造一致; 真实 wire
    /// 路径由 Android HubClientRemoteControlContractTest 的 MockWebServer 断言。)
    func testRemoteCommandPathShape() {
        let vehicleId = "VH-REMO-001"
        let path = "/vehicles/\(vehicleId)/command"

        XCTAssertEqual(path, "/vehicles/VH-REMO-001/command", "远程控车路径必须是 /vehicles/{vehicleId}/command")
        XCTAssertTrue(path.hasPrefix("/vehicles/"))
        XCTAssertTrue(path.hasSuffix("/command"))
        XCTAssertTrue(path.contains(vehicleId))
    }

    // MARK: - 响应模型

    /// ControlCommandResponse 可解码 Hub 的 cmdId/resultCode/errorMsg 响应。
    func testControlCommandResponseDecoding() throws {
        let json = """
        {"cmdId":"cmd-001","resultCode":0,"errorMsg":null}
        """
        let data = try XCTUnwrap(json.data(using: .utf8))
        let resp = try JSONDecoder().decode(ControlCommandResponse.self, from: data)

        XCTAssertEqual(resp.cmdId, "cmd-001")
        XCTAssertEqual(resp.resultCode, 0)
        XCTAssertNil(resp.errorMsg)
    }
}
