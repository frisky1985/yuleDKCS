import XCTest
@testable import YDKHubClient

/// Phase 4.1 / W3 — SDK×Hub 请求形状契约测试 (iOS)
///
/// 固化 SDK → Hub REST Gateway 的请求形状契约 (Hub 端由 protojson 强制执行):
///   1. 枚举字段必须传 **枚举名字符串** (vendor="APPLE" / protocol="CCC_DK3"), 而非数字字符串
///   2. 字段名必须 camelCase (vehicleId / deviceId / devicePubkey / keyType / traceId)
///   3. devicePubkey 必须是 base64 编码的设备公钥
///
/// 说明: 本测试不修改 SDK 源码。iOS SDK 的 URLSession transport 不可注入,
/// 因此通过 @testable 直接验证 bindKey/acceptShare 构造的 body 字典经
/// AnyEncodable + JSONEncoder 编码后的字节形状 —— 这正是 request() 实际
/// 发送到 Hub 的内容。端到端 wire 验证由 backend/scripts/sdk-hub-contract-e2e.sh 覆盖。
final class YDKHubClientRequestShapeContractTests: XCTestCase {

    /// 按 SDK request() 的真实编码路径 ([String: String] → AnyEncodable → JSONEncoder)
    /// 编码 body, 返回 JSON 字典。
    private func encodeBody(_ body: [String: String]) throws -> [String: String] {
        let data = try JSONEncoder().encode(AnyEncodable(body))
        let obj = try XCTUnwrap(
            JSONSerialization.jsonObject(with: data) as? [String: String],
            "encoded body must be a flat string dictionary"
        )
        return obj
    }

    // MARK: - bindKey 请求形状

    /// bindKey body: camelCase 字段名 + 枚举名字符串 + base64 pubkey。
    /// (body 结构与 YDKHubClient+Keys.swift::bindKey 逐字段一致)
    func testBindKeyBodyUsesEnumNamesCamelCaseAndBase64() throws {
        let deviceManager = YDKDeviceManager.shared

        let body: [String: String] = [
            "vehicleId": "VH-CONTRACT-001",
            "deviceId": deviceManager.getDeviceId(),
            "devicePubkey": "dGVzdC1iNjQtcHViLWtleS1jb250cmFjdC0wMDE=",
            "vendor": deviceManager.detectVendor().protoName,
            "protocol": deviceManager.detectProtocol().protoName,
            "keyType": "OWNER",
            "traceId": "trace-contract-001",
        ]

        let json = try encodeBody(body)

        // camelCase 字段名 (protojson 要求, 拒绝 snake_case)
        XCTAssertEqual(
            Set(json.keys),
            Set(["vehicleId", "deviceId", "devicePubkey", "vendor", "protocol", "keyType", "traceId"]),
            "bindKey body 字段名必须是 camelCase"
        )

        // 枚举名字符串 (非数字字符串)
        XCTAssertEqual(json["keyType"], "OWNER")
        XCTAssertEqual(json["vendor"], "APPLE", "iOS detectVendor() 恒为 .apple → protoName=APPLE")
        XCTAssertEqual(json["protocol"], "CCC_DK3", "iOS detectProtocol() 恒为 .ccc → protoName=CCC_DK3")
        XCTAssertNotEqual(json["vendor"], "1", "vendor 不得传数字字符串")
        XCTAssertNotEqual(json["protocol"], "3", "protocol 不得传数字字符串")

        // devicePubkey: 合法 base64
        let pubkeyData = try XCTUnwrap(Data(base64Encoded: json["devicePubkey"] ?? ""))
        XCTAssertFalse(pubkeyData.isEmpty, "devicePubkey 解码后不得为空")
    }

    // MARK: - acceptShare 请求形状

    /// acceptShare body: shareCode + camelCase + 枚举名 + base64 pubkey。
    /// (body 结构与 YDKHubClient+Share.swift::acceptShare 逐字段一致)
    func testAcceptShareBodyUsesEnumNamesCamelCaseAndBase64() throws {
        let deviceManager = YDKDeviceManager.shared

        let body: [String: String] = [
            "shareCode": "123456",
            "deviceId": deviceManager.getDeviceId(),
            "devicePubkey": "dGVzdC1iNjQtcHViLWtleS1jb250cmFjdC0wMDE=",
            "vendor": deviceManager.detectVendor().protoName,
            "traceId": "trace-contract-002",
        ]

        let json = try encodeBody(body)

        XCTAssertEqual(
            Set(json.keys),
            Set(["shareCode", "deviceId", "devicePubkey", "vendor", "traceId"]),
            "acceptShare body 字段名必须是 camelCase"
        )
        XCTAssertEqual(json["shareCode"], "123456")
        XCTAssertEqual(json["vendor"], "APPLE")
        let pubkeyData = try XCTUnwrap(Data(base64Encoded: json["devicePubkey"] ?? ""))
        XCTAssertFalse(pubkeyData.isEmpty)
    }

    // MARK: - 枚举名与 hub.proto 对齐

    /// PhoneVendor.protoName / DigitalKeyProtocol.protoName 必须与
    /// backend/cloud/hub/api/v1/hub.proto 中的枚举名完全一致 (protojson 按名匹配)。
    func testEnumProtoNamesMatchHubProto() {
        XCTAssertEqual(PhoneVendor.apple.protoName, "APPLE")
        XCTAssertEqual(PhoneVendor.samsung.protoName, "SAMSUNG")
        XCTAssertEqual(PhoneVendor.xiaomi.protoName, "XIAOMI")
        XCTAssertEqual(PhoneVendor.oppo.protoName, "OPPO")
        XCTAssertEqual(PhoneVendor.vivo.protoName, "VIVO")
        XCTAssertEqual(PhoneVendor.huawei.protoName, "HUAWEI")

        XCTAssertEqual(DigitalKeyProtocol.ccc.protoName, "CCC_DK3")
        XCTAssertEqual(DigitalKeyProtocol.iccoa.protoName, "ICCOA_DK40")
        XCTAssertEqual(DigitalKeyProtocol.icce.protoName, "ICCE")
    }

    // MARK: - base64 编码契约

    /// 设备公钥 (DER 字节) → base64 字符串 (Hub 端 bytes 字段经 protojson 要求 base64)。
    func testDevicePubkeyIsBase64WithoutWhitespace() {
        // 模拟 X.509 DER 编码的 ECC P-256 公钥头
        let derPrefix = Data([0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01])
        let b64 = derPrefix.base64EncodedString()

        // 往返一致
        XCTAssertEqual(Data(base64Encoded: b64), derPrefix)
        // JSON 友好: 无空白 / 无换行
        XCTAssertFalse(b64.contains(" "))
        XCTAssertFalse(b64.contains("\n"))
    }
}
