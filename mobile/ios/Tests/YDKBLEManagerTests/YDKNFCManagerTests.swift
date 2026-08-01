import XCTest
@testable import YDKBLEManager

// MARK: - 2b-H NFC 模拟测（不依赖硬件）
//
// Contract 映射 (docs/sdk/PHASE2B-GH-P1-CONTRACT.md):
//   N-3 (接口不变)      → testCoreNFCManagerConformsToProtocol
//   N-1 (命令映射)      → testBuildCommandAPDUMapping / testReadAPDUs / testIsSuccess / testParseVehicleId
//   降级路径 (无硬件)   → testCoreNFCManagerGracefulDegradationWithoutCoreNFC
//
// 双端一致契约: 命令字节映射与 Android `NfcCommandBuilder` 完全一致 (Python 交叉验证)。

final class YDKNFCManagerTests: XCTestCase {

    // MARK: N-1 — 指令 APDU 映射 (与 Android 一致)

    func testBuildCommandAPDUMapping() {
        XCTAssertEqual(Array(YDKNFCApdu.buildCommand(.unlock)), [0x80, 0xD2, 0x01, 0x00, 0x00, 0x00])
        XCTAssertEqual(Array(YDKNFCApdu.buildCommand(.lock)), [0x80, 0xD2, 0x02, 0x00, 0x00, 0x00])
        XCTAssertEqual(Array(YDKNFCApdu.buildCommand(.startEngine)), [0x80, 0xD2, 0x03, 0x00, 0x00, 0x00])
    }

    func testReadAPDUs() {
        // READ BINARY (ISO 7816-4)
        XCTAssertEqual(Array(YDKNFCApdu.buildReadVehicleRecord()), [0x00, 0xB0, 0x00, 0x00, 0x40])
        // MiFare READ block 4
        XCTAssertEqual(Array(YDKNFCApdu.buildReadMifareBlock()), [0x30, 0x04])
    }

    // MARK: N-1 — tagId 格式化

    func testTagIdHexUppercaseNoSeparator() {
        XCTAssertEqual(YDKNFCApdu.tagIdHex(Data([0x04, 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6])), "04A1B2C3D4E5F6")
        XCTAssertEqual(YDKNFCApdu.tagIdHex(Data([0x00, 0x0A, 0xFF])), "000AFF")
        XCTAssertEqual(YDKNFCApdu.tagIdHex(Data()), "")
    }

    // MARK: N-1 — 响应校验

    func testIsSuccess() {
        XCTAssertTrue(YDKNFCApdu.isSuccess(response: Data([0x00, 0x90, 0x00])))
        XCTAssertTrue(YDKNFCApdu.isSuccess(response: Data([0x90, 0x00])))
        XCTAssertFalse(YDKNFCApdu.isSuccess(response: Data([0x63, 0x00])))          // SW=6300 条件不满足
        XCTAssertFalse(YDKNFCApdu.isSuccess(response: Data([0x90])))                // 不足 2 字节
        XCTAssertFalse(YDKNFCApdu.isSuccess(response: Data()))
    }

    func testParseVehicleId() {
        // "VH-2026-0001" = 56 48 2D 32 30 32 36 2D 30 30 30 31 + SW 9000
        let payload = Data([0x56, 0x48, 0x2D, 0x32, 0x30, 0x32, 0x36, 0x2D,
                            0x30, 0x30, 0x30, 0x31, 0x90, 0x00])
        XCTAssertEqual(YDKNFCApdu.parseVehicleId(from: payload), "VH-2026-0001")

        // 尾零填充
        let padded = Data([0x56, 0x48, 0x2D, 0x31, 0x00, 0x00, 0x00, 0x90, 0x00])
        XCTAssertEqual(YDKNFCApdu.parseVehicleId(from: padded), "VH-1")

        // 只有状态字 → nil
        XCTAssertNil(YDKNFCApdu.parseVehicleId(from: Data([0x90, 0x00])))
        // 非 UTF-8 数据 → nil（解码后为替换字符, 经 trim 后非空则原样返回; 空串返回 nil）
        XCTAssertNil(YDKNFCApdu.parseVehicleId(from: Data([0x00, 0x00, 0x90, 0x00])))
    }

    // MARK: N-3 — 接口契约

    func testCoreNFCManagerConformsToProtocol() {
        let manager: YDKNFCManaging = YDKCoreNFCManager(expectedTagId: "04A1B2C3D4E5F6")
        XCTAssertNotNil(manager)
    }

    /// 无硬件/非 iOS 编译环境: 必须走 coreNFCUnavailable 降级错误（真机环境跳过）
    func testCoreNFCManagerGracefulDegradationWithoutCoreNFC() async {
        #if canImport(CoreNFC)
        throw XCTSkip("真机环境跳过: CoreNFC 会话需 NFC 硬件")
        #else
        let manager = YDKCoreNFCManager()
        do {
            _ = try await manager.readVehicleTag()
            XCTFail("非 iOS/无 CoreNFC 环境应抛错")
        } catch let error as YDKNFCError {
            guard case .coreNFCUnavailable = error else {
                return XCTFail("期望 coreNFCUnavailable, 实际 \(error)")
            }
        } catch {
            XCTFail("期望 YDKNFCError, 实际 \(error)")
        }

        do {
            try await manager.sendCommandViaNFC(command: .unlock)
            XCTFail("非 iOS/无 CoreNFC 环境应抛错")
        } catch let error as YDKNFCError {
            guard case .coreNFCUnavailable = error else {
                return XCTFail("期望 coreNFCUnavailable, 实际 \(error)")
            }
        } catch {
            XCTFail("期望 YDKNFCError, 实际 \(error)")
        }
        #endif
    }
}
