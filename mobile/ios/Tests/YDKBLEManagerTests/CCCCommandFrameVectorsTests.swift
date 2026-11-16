import XCTest
@testable import YDKBLEManager

/// CCC Digital Key v4.0 帧 — 跨语言固定字节向量测试
///
/// 向量单一来源: `embedded/tests/protocol_vectors/ccc_frame_v4.h`
/// 车端 (test_ccc_frame.c) / iOS (本文件) / Android (CccFrameVectorsTest)
/// 必须使用完全相同的字节序列. 改动任一端都需要同步更新该头文件与三端测试.
final class CCCCommandFrameVectorsTests: XCTestCase {

    // ── 向量 1: SELECT APDU (规范 §19.3 示例) ──
    private let selectApduPayload: [UInt8] = [
        0x00, 0xA4, 0x04, 0x00, 0x0D, 0xA0, 0x00, 0x00,
        0x00, 0x80, 0x94, 0x34, 0x34, 0x34, 0x34, 0x44,
        0x4B, 0x41, 0x76, 0x31, 0x00
    ]
    private let selectApduFrame: [UInt8] = [
        0x01, 0x0B, 0x00, 0x15,
        0x00, 0xA4, 0x04, 0x00, 0x0D, 0xA0, 0x00, 0x00,
        0x00, 0x80, 0x94, 0x34, 0x34, 0x34, 0x34, 0x44,
        0x4B, 0x41, 0x76, 0x31, 0x00
    ]

    // ── 向量 2: DK Event Notification (车 → 手机) ──
    private let dkEventFrame: [UInt8] = [0x03, 0x0C, 0x00, 0x03, 0x01, 0x00, 0x45]

    // ── 负向向量 ──
    private let invalidRfuFrame: [UInt8] = [0xC1, 0x0B, 0x00, 0x04, 0xAA, 0xBB, 0xCC, 0xDD]
    private let truncatedFrame: [UInt8] = [0x01, 0x0B, 0x00, 0x0A, 0x01, 0x02, 0x03, 0x04, 0x05]
    private let stuffedFrame: [UInt8] = [0x01, 0x0B, 0x00, 0x02, 0x01, 0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE]

    func testBuildMatchesCanonicalSelectApduVector() {
        let frame = CCCCommandFrame(
            messageType: CCCMessageType.se.rawValue,
            messageID: CCCApduMessageID.dkApduRq.rawValue,
            payload: Data(selectApduPayload)
        )
        XCTAssertEqual([UInt8](frame.data), selectApduFrame)
    }

    func testParseCanonicalSelectApduVector() throws {
        let frame = try CCCCommandFrame.parse(Data(selectApduFrame))
        XCTAssertEqual(frame.messageType, CCCMessageType.se.rawValue)
        XCTAssertEqual(frame.messageID, CCCApduMessageID.dkApduRq.rawValue)
        XCTAssertEqual([UInt8](frame.payload), selectApduPayload)
    }

    func testParseCanonicalDkEventVector() throws {
        let frame = try CCCCommandFrame.parse(Data(dkEventFrame))
        XCTAssertEqual(frame.messageType, CCCMessageType.dkEventNotification.rawValue)
        XCTAssertEqual(frame.messageID, CCCApduMessageID.dkApduRs.rawValue)
        XCTAssertEqual([UInt8](frame.payload), [0x01, 0x00, 0x45])
    }

    func testParseRejectsNonZeroRfuBits() {
        XCTAssertThrowsError(try CCCCommandFrame.parse(Data(invalidRfuFrame))) { error in
            XCTAssertEqual(error as? CCCFrameError, .rfuBitsSet(0xC1))
        }
        XCTAssertNil(CCCCommandFrame(data: Data(invalidRfuFrame)))
    }

    func testParseRejectsTruncatedPayload() {
        XCTAssertThrowsError(try CCCCommandFrame.parse(Data(truncatedFrame))) { error in
            XCTAssertEqual(error as? CCCFrameError, .payloadLengthMismatch(declared: 10, actual: 5))
        }
        XCTAssertNil(CCCCommandFrame(data: Data(truncatedFrame)))
    }

    func testParseRejectsStuffedPayload() {
        XCTAssertThrowsError(try CCCCommandFrame.parse(Data(stuffedFrame))) { error in
            XCTAssertEqual(error as? CCCFrameError, .payloadLengthMismatch(declared: 2, actual: 7))
        }
        XCTAssertNil(CCCCommandFrame(data: Data(stuffedFrame)))
    }

    func testParseRejectsShortHeader() {
        XCTAssertThrowsError(try CCCCommandFrame.parse(Data([0x01, 0x0B, 0x00]))) { error in
            XCTAssertEqual(error as? CCCFrameError, .tooShort(3))
        }
    }

    /// 编码时 Bit[7:6] 必须被清零
    func testBuildZeroesRfuBitsEvenIfSet() {
        let frame = CCCCommandFrame(messageType: 0xC1, messageID: 0x0B, payload: Data())
        XCTAssertEqual(frame.data[0], 0x01)
    }
}
