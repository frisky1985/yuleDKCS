//
//  DkErrorTests.swift
//  DigitalKeySDKTests
//
//  验证:
//  - 错误码常量值正确性
//  - ErrorCategory 编解码
//  - DigitalKeyError 属性计算与工厂方法
//  - 链式构造 (withTraceId, withDetails)
//  - 序列化 (toMap, description)
//

import XCTest
@testable import DigitalKeySDK

final class DkErrorTests: XCTestCase {

    // MARK: - ErrorCategory

    func testErrorCategoryFromCodeMapsCorrectly() {
        let cases: [(UInt16, ErrorCategory)] = [
            (0x0000, .success),
            (0x0100, .request),
            (0x0200, .auth),
            (0x0300, .key),
            (0x0400, .vehicle),
            (0x0500, .share),
            (0x0600, .device),
            (0x0700, .vendor),
            (0x0800, .transport),
            (0x0900, .system),
            (0xAC00, .tcu),
        ]
        for (code, expected) in cases {
            let actual = ErrorCategory.from(code: code)
            XCTAssertEqual(actual, expected, "Code 0x\(String(format: "%04X", code)) should map to \(expected)")
        }
    }

    func testErrorCategoryFromCodeUnknownReturnsSystem() {
        XCTAssertEqual(ErrorCategory.from(code: 0xFFFF), .system)
        XCTAssertEqual(ErrorCategory.from(code: 0x0A00), .system)
    }

    func testErrorCategoryRawValues() {
        XCTAssertEqual(ErrorCategory.success.rawValue, 0x00)
        XCTAssertEqual(ErrorCategory.request.rawValue, 0x01)
        XCTAssertEqual(ErrorCategory.system.rawValue, 0x09)
        XCTAssertEqual(ErrorCategory.tcu.rawValue, 0xAC)
    }

    // MARK: - DkErrorCode 常量值

    func testSuccessCodeValues() {
        XCTAssertEqual(DkErrorCode.success, 0x0000)
        XCTAssertEqual(DkErrorCode.successAsync, 0x0001)
        XCTAssertEqual(DkErrorCode.successPartial, 0x0002)
    }

    func testKeyErrorCodeValues() {
        XCTAssertEqual(DkErrorCode.keyNotFound, 0x0301)
        XCTAssertEqual(DkErrorCode.keyExists, 0x0302)
        XCTAssertEqual(DkErrorCode.keyExpired, 0x0303)
        XCTAssertEqual(DkErrorCode.keyRevoked, 0x0304)
        XCTAssertEqual(DkErrorCode.keyBindFailed, 0x030D)
        XCTAssertEqual(DkErrorCode.keyUnbindFailed, 0x030E)
    }

    func testVehicleErrorCodeValues() {
        XCTAssertEqual(DkErrorCode.vehicleNotFound, 0x0401)
        XCTAssertEqual(DkErrorCode.vehicleOffline, 0x0402)
        XCTAssertEqual(DkErrorCode.commandFailed, 0x0407)
        XCTAssertEqual(DkErrorCode.batteryLow, 0x040D)
        XCTAssertEqual(DkErrorCode.doorOpen, 0x0410)
    }

    func testTransportErrorCodeValues() {
        XCTAssertEqual(DkErrorCode.networkError, 0x0801)
        XCTAssertEqual(DkErrorCode.mqttDisconnected, 0x0805)
        XCTAssertEqual(DkErrorCode.grpcUnavailable, 0x0807)
    }

    // MARK: - DigitalKeyError 基础属性

    func testSuccessFactory() {
        let err = DigitalKeyError.success
        XCTAssertEqual(err.code, DkErrorCode.success)
        XCTAssertEqual(err.category, .success)
        XCTAssertEqual(err.hexCode, "0x0000")
        XCTAssertTrue(err.isSuccess)
        XCTAssertEqual(err.name, "SUCCESS")
    }

    func testHexCodeFormat() {
        let err = DigitalKeyError(code: DkErrorCode.keyNotFound)
        XCTAssertEqual(err.hexCode, "0x0301")
    }

    func testCategoryDerivedFromCode() {
        XCTAssertEqual(DigitalKeyError(code: 0x0201).category, .auth)
        XCTAssertEqual(DigitalKeyError(code: 0x0401).category, .vehicle)
        XCTAssertEqual(DigitalKeyError(code: 0xAC01).category, .tcu)
    }

    func testFromCodeWithDefaultMessage() {
        let err = DigitalKeyError.from(code: DkErrorCode.keyNotFound)
        XCTAssertEqual(err.message, "密钥不存在")
    }

    func testFromCodeUnknownCode() {
        let err = DigitalKeyError(code: 0x9999)
        XCTAssertEqual(err.message, "Unknown error")
    }

    func testIsSuccessFalseForNonSuccess() {
        XCTAssertFalse(DigitalKeyError(code: DkErrorCode.internalError).isSuccess)
        XCTAssertFalse(DigitalKeyError(code: DkErrorCode.commandFailed).isSuccess)
    }

    // MARK: - 链式构造

    func testWithTraceId() {
        let err = DigitalKeyError(code: DkErrorCode.commandFailed)
            .withTraceId("trace-abc-123")
        XCTAssertEqual(err.code, DkErrorCode.commandFailed)
        XCTAssertEqual(err.traceId, "trace-abc-123")
    }

    func testWithDetails() {
        let err = DigitalKeyError(code: DkErrorCode.batteryLow)
            .withDetails(["vehicle_id": "VH-001", "battery_level": 5])
        XCTAssertEqual(err.details?["vehicle_id"] as? String, "VH-001")
        XCTAssertEqual(err.details?["battery_level"] as? Int, 5)
    }

    func testFullChain() {
        let err = DigitalKeyError(code: DkErrorCode.serverUnreachable)
            .withTraceId("trace-xyz")
            .withDetails(["host": "api.example.com"])
        XCTAssertEqual(err.traceId, "trace-xyz")
        XCTAssertEqual((err.details?["host"] as? String), "api.example.com")
        XCTAssertEqual(err.hexCode, "0x0803")
        XCTAssertEqual(err.category, .transport)
    }

    // MARK: - toMap 序列化

    func testToMap() {
        let err = DigitalKeyError(code: DkErrorCode.keyBindFailed)
            .withTraceId("trace-001")
            .withDetails(["vehicle_id": "VH-001"])
        let map = err.toMap()
        XCTAssertEqual(map["code"] as? UInt16, DkErrorCode.keyBindFailed)
        XCTAssertEqual(map["code_hex"] as? String, "0x030D")
        XCTAssertEqual(map["name"] as? String, "ERR_KEY_BIND_FAILED")
        XCTAssertEqual(map["message"] as? String, "密钥绑定失败")
        XCTAssertEqual(map["category"] as? String, "KEY")
        XCTAssertEqual(map["trace_id"] as? String, "trace-001")
    }

    // MARK: - description

    func testDescription() {
        let err = DigitalKeyError(code: DkErrorCode.invalidToken)
            .withTraceId("tid-001")
        let desc = err.description
        XCTAssertTrue(desc.contains("0x0202"))
        XCTAssertTrue(desc.contains("ERR_INVALID_TOKEN"))
        XCTAssertTrue(desc.contains("tid-001"))
    }

    // MARK: - getErrorName / getErrorMessage

    func testGetErrorName() {
        XCTAssertEqual(getErrorName(DkErrorCode.keyNotFound), "ERR_KEY_NOT_FOUND")
        XCTAssertEqual(getErrorName(DkErrorCode.networkError), "ERR_NETWORK_ERROR")
        XCTAssertEqual(getErrorName(0xFFFF), "ERR_UNKNOWN")
    }

    func testGetErrorMessage() {
        XCTAssertEqual(getErrorMessage(DkErrorCode.keyNotFound), "密钥不存在")
        XCTAssertEqual(getErrorMessage(DkErrorCode.batteryLow), "电瓶电量低")
        XCTAssertEqual(getErrorMessage(0xFFFF), "Unknown error")
    }
}
