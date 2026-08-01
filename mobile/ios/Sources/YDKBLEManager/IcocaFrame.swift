import Foundation

// MARK: - ICCOA DK3.0 协议帧

/// ICCOA Digital Key 3.0 BLE 指令帧编解码 — 2b-F
///
/// 帧格式 (事实来源: embedded/iccoa_protocol/src/iccoa/dk30/iccoa_dk30.c +
/// include/iccoa_digital_key.h, 裁决 AD-2):
///
/// ```
///   [0]     SOP        起始符 0xAA
///   [1]     CMD_ID     命令 ID (见下方 cmd* 常量, iccoa_digital_key.h:56-69)
///   [2-3]   SEQ_NUM    序列号 (小端 LE u16 — dk30.c:120)
///   [4-5]   LEN        payload 长度 (小端 LE u16 — dk30.c:121)
///   [6..]   PAYLOAD    负载
///   [..]    CHECKSUM   XOR 校验: 仅覆盖 CMD_ID+SEQ_NUM+LEN+PAYLOAD, 不含 SOP
///                      (dk30.c:131-132, send_response 同款 dk30.c:236)
///   [..]    EOP        结束符 0x55
/// ```
///
/// ⚠️ ICCOA DK3.0 应用层无加密 (裁决 AD-1): 链路加密由 BLE LE Secure
/// Connections 负责, 本帧不承载任何 SM4 密文, 禁止在本协议指令上做 SM4。
public enum IcocaFrame {

    /// 起始符
    public static let sop: UInt8 = 0xAA
    /// 结束符
    public static let eop: UInt8 = 0x55
    /// 帧头长度: SOP + CMD + SEQ(2) + LEN(2)
    public static let headerSize = 6
    /// 帧尾长度: CHECKSUM + EOP
    public static let trailerSize = 2
    /// ICCOA 最大负载 (iccoa_digital_key.h:38 ICCOA_MAX_PAYLOAD)
    public static let maxPayload = 244

    // MARK: - 命令 ID (iccoa_digital_key.h:56-69)

    public static let cmdBindRequest: UInt8 = 0x01
    public static let cmdBindResponse: UInt8 = 0x02
    public static let cmdUnbindRequest: UInt8 = 0x03
    public static let cmdUnbindResponse: UInt8 = 0x04
    public static let cmdAuthRequest: UInt8 = 0x10
    public static let cmdAuthResponse: UInt8 = 0x11
    public static let cmdCtrlRequest: UInt8 = 0x20
    public static let cmdCtrlResponse: UInt8 = 0x21
    public static let cmdStatusNotify: UInt8 = 0x30
    public static let cmdKeyShare: UInt8 = 0x40
    public static let cmdKeyShareAck: UInt8 = 0x41
    public static let cmdError: UInt8 = 0xFF

    // MARK: - CTRL 命令枚举 (iccoa_digital_key.h:155-167)

    public static let ctrlLock: UInt8 = 0x01
    public static let ctrlUnlock: UInt8 = 0x02
    public static let ctrlEngineOn: UInt8 = 0x03
    public static let ctrlEngineOff: UInt8 = 0x04
    public static let ctrlTrunkOpen: UInt8 = 0x05
    public static let ctrlWindowUp: UInt8 = 0x06
    public static let ctrlWindowDown: UInt8 = 0x07
    public static let ctrlClimateOn: UInt8 = 0x08
    public static let ctrlClimateOff: UInt8 = 0x09
    public static let ctrlFind: UInt8 = 0x0A
    public static let ctrlHorn: UInt8 = 0x0B

    // MARK: - 帧结构

    /// 解析后的 DK3.0 帧
    public struct Frame: Equatable {
        /// 命令 ID
        public let cmdId: UInt8
        /// 序列号 (小端解析)
        public let seqNum: UInt16
        /// 负载
        public let payload: Data
        /// 原始帧字节
        public let raw: Data

        public init(cmdId: UInt8, seqNum: UInt16, payload: Data, raw: Data) {
            self.cmdId = cmdId
            self.seqNum = seqNum
            self.payload = payload
            self.raw = raw
        }
    }

    // MARK: - 构造

    /// 构造 DK3.0 帧: SOP + CMD + SEQ(LE) + LEN(LE) + PAYLOAD + XOR 校验(不含 SOP) + EOP
    ///
    /// - Parameters:
    ///   - cmdId:   命令 ID (0..255)
    ///   - seqNum:  序列号 (0..65535, 小端)
    ///   - payload: 负载 (明文; ICCOA 无应用层加密)
    public static func build(cmdId: UInt8, seqNum: UInt16, payload: Data) -> Data {
        precondition(payload.count <= 0xFFFF, "payload too large: \(payload.count)")

        var frame = Data(capacity: headerSize + payload.count + trailerSize)
        frame.append(sop)
        frame.append(cmdId)
        // SEQ 小端 (dk30.c:120)
        frame.append(UInt8(seqNum & 0x00FF))
        frame.append(UInt8((seqNum >> 8) & 0x00FF))
        // LEN 小端 (dk30.c:121)
        let len = payload.count
        frame.append(UInt8(len & 0xFF))
        frame.append(UInt8((len >> 8) & 0xFF))
        frame.append(payload)
        // XOR 校验: 不含 SOP, 覆盖 CMD+SEQ+LEN+PAYLOAD (dk30.c:131-132, 236)
        frame.append(checksum(frame.dropFirst()))
        frame.append(eop)
        return frame
    }

    // MARK: - 校验

    /// XOR 校验和: 对全部字节异或
    public static func checksum(_ bytes: Data) -> UInt8 {
        bytes.reduce(0) { $0 ^ $1 }
    }

    // MARK: - 解析

    /// 解析 DK3.0 帧; 坏 SOP/EOP / 长度不符 / 校验失败 → nil (裁决 B1.2)
    public static func parse(_ data: Data) -> Frame? {
        guard data.count >= headerSize + trailerSize else { return nil }
        guard data[data.startIndex] == sop, data[data.endIndex - 1] == eop else { return nil }

        let payloadLen = Int(data[data.startIndex + 4]) | (Int(data[data.startIndex + 5]) << 8)
        guard data.count == headerSize + payloadLen + trailerSize else { return nil }

        let checksumIndex = headerSize + payloadLen
        // 校验覆盖: CMD+SEQ+LEN+PAYLOAD = 跳过 SOP 后前 5+payloadLen 字节
        // (dk30.c:131-132: cs_len = sizeof(cmd_id)+sizeof(seq_num)+sizeof(payload_len)+payload_len
        //  = 1+2+2+payloadLen; 注意契约 AD-2 中"4+len"为笔误, send_response dk30.c:236 的
        //  "4+len" 与校验路径不一致, 以车端校验路径 process 为准)
        let expected = checksum(data.dropFirst().prefix(5 + payloadLen))
        guard data[data.startIndex + checksumIndex] == expected else { return nil }

        let cmdId = data[data.startIndex + 1]
        let seqNum = UInt16(data[data.startIndex + 2]) | (UInt16(data[data.startIndex + 3]) << 8)
        let payload = data.subdata(in: (data.startIndex + headerSize)..<(data.startIndex + checksumIndex))

        return Frame(cmdId: cmdId, seqNum: seqNum, payload: payload, raw: data)
    }
}
