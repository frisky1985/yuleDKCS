import XCTest
@testable import YDKBLEManager

// MARK: - 2b-G UWB 模拟测 (不依赖硬件)
//
// Contract 映射 (docs/sdk/PHASE2B-GH-P1-CONTRACT.md 工作流 1):
//   U-1 → #if canImport(NearbyInteraction) 下 YDKNIUWBManager 纯逻辑测试 (token 校验/停止安全)
//   U-3 → Mock 保留 + 接口契约: 现有调用零破坏
//   U-4 → 本文件: start/stop 状态机 + 回调注入, 全部不依赖硬件
//
// 平台事实 (验证前提):
//   - NearbyInteraction 框架仅 iOS 可用; macOS 宿主 canImport == true 但 API 全部
//     API_UNAVAILABLE(macos), 故真实实现测试仍以条件编译包裹 (与 YDKUWBManager.swift 一致)。
//   - 模拟器/无 U1/U2 设备上能力探测为 false,
//     故 startRanging 无 token 场景允许抛 unsupportedPlatform 或 missingPeerDiscoveryToken。
//   - 完整测距链路 (真实距离/角度回调) 属真机联调范畴, 见 docs/sdk/PHASE2G-UWB-PLATFORM.md。

final class YDKUWBManagerTests: XCTestCase {

    // MARK: - U-3/U-4: 接口契约 (Mock, 无硬件)

    /// U-4: startRanging 后回调被注入并产生测量 (vehicleId 透传 + 距离为正)
    func testMockStartRangingEmitsMeasurement() async throws {
        let manager = YDKMockUWBManager()
        let expectation = expectation(description: "收到测距回调")
        manager.rangingResultHandler = { measurement in
            XCTAssertEqual(measurement.vehicleId, "VH-001")
            XCTAssertGreaterThan(measurement.distance, 0)
            expectation.fulfill()
        }

        try await manager.startRanging(vehicleId: "VH-001")
        await fulfillment(of: [expectation], timeout: 3)
        manager.stopRanging()
    }

    /// U-4: stopRanging 后不再产生回调 (状态机终止)
    func testMockStopRangingHaltsCallbacks() async throws {
        let manager = YDKMockUWBManager()
        let expectation = expectation(description: "停止后不得再回调")
        expectation.isInverted = true
        manager.rangingResultHandler = { _ in expectation.fulfill() }

        try await manager.startRanging(vehicleId: "VH-001")
        try? await Task.sleep(nanoseconds: 100_000_000) // 等待首个回调窗口
        manager.stopRanging()

        await fulfillment(of: [expectation], timeout: 1.5)
    }

    /// U-3: 回调 handler 可注入、替换、置空
    func testRangingResultHandlerInjection() async throws {
        let manager = YDKMockUWBManager()
        XCTAssertNil(manager.rangingResultHandler, "初始 handler 应为空")

        var count = 0
        manager.rangingResultHandler = { _ in count += 1 }
        try await manager.startRanging(vehicleId: "VH-001")
        try? await Task.sleep(nanoseconds: 1_100_000_000)
        manager.stopRanging()
        XCTAssertGreaterThanOrEqual(count, 1, "运行期间应至少收到一次回调")

        manager.rangingResultHandler = nil
        XCTAssertNil(manager.rangingResultHandler, "handler 可置空")
    }

    /// U-4: 重复 start 不崩溃 (幂等, 定时器重建)
    func testMockRestartIsIdempotent() async throws {
        let manager = YDKMockUWBManager()
        try await manager.startRanging(vehicleId: "A")
        try await manager.startRanging(vehicleId: "B")
        manager.stopRanging()
        manager.stopRanging() // 重复 stop 亦安全
        XCTAssertTrue(true)   // 未崩溃即通过
    }

    // MARK: - 平台能力检测 (任意宿主)

    /// 平台能力查询不崩溃; iOS 真机 (U1/U2) 为 true, 模拟器/macOS 为 false
    func testPlatformCapabilityQueryNeverThrows() {
        _ = YDKUWBPlatform.supportsNearbyInteraction
    }

    // MARK: - U-1: YDKNIUWBManager 纯逻辑 (无硬件, 仅 iOS 宿主编译)

    #if canImport(NearbyInteraction)
    @available(iOS 14.0, *)
    func testNIUWBManagerFailsWithoutPeerToken() async {
        let manager = YDKNIUWBManager()
        do {
            try await manager.startRanging(vehicleId: "VH-001")
            XCTFail("未注入车端 token 时应抛错")
        } catch let error as YDKUWBError {
            // 模拟器(无 UWB 硬件) → unsupportedPlatform; 真机 → missingPeerDiscoveryToken
            XCTAssertTrue(
                error == .missingPeerDiscoveryToken || error == .unsupportedPlatform,
                "预期 token/平台错误, 实际: \(error)"
            )
        } catch {
            XCTFail("意外错误: \(error)")
        }
    }

    @available(iOS 14.0, *)
    func testNIUWBManagerRejectsInvalidTokenData() {
        let manager = YDKNIUWBManager()
        // 空 Data / 垃圾 Data → NSSecureCoding 反序列化失败 → invalidTokenData
        do {
            try manager.injectPeerDiscoveryToken(data: Data())
            XCTFail("空 Data 不应注入成功")
        } catch let error as YDKUWBError {
            XCTAssertEqual(error, .invalidTokenData, "预期 invalidTokenData, 实际: \(error)")
        } catch {
            XCTFail("意外错误: \(error)")
        }

        // 垃圾字节同样失败
        do {
            try manager.injectPeerDiscoveryToken(data: Data([0xDE, 0xAD, 0xBE, 0xEF]))
            XCTFail("垃圾 Data 不应注入成功")
        } catch let error as YDKUWBError {
            XCTAssertEqual(error, .invalidTokenData)
        } catch {
            XCTFail("意外错误: \(error)")
        }
    }

    @available(iOS 14.0, *)
    func testNIUWBManagerStopWithoutStartIsSafe() {
        let manager = YDKNIUWBManager()
        manager.stopRanging() // 未 start 直接 stop 不应崩溃
        XCTAssertNil(manager.lastSessionError)
    }
    #endif
}
