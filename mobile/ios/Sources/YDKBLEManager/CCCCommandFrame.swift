import Foundation

// MARK: - CCC BLE 消息类型

/// CCC BLE 消息类型 — 与参考实现一致
/// 参考: `embedded/ccc_protocol/include/ccc_digital_key.h` `ble_msg_type_e`
public enum CCCMessageType: UInt8, CaseIterable {
    case pairRequest = 0x01
    case pairResponse = 0x02
    case keyCreate = 0x10
    case keyDelete = 0x11
    case keyShare = 0x12
    case authRequest = 0x20
    case authResponse = 0x21
    case uwbConfig = 0x30
    case stateNotify = 0x40
    case error = 0xFF
}

// MARK: - CCC 帧错误

/// CCC 帧解析错误
public enum CCCFrameError: Error, Equatable {
    /// 数据不足 5 字节帧头
    case tooShort(Int)
    /// payload_len 字段与实际负载长度不一致 (防截断/粘包)
    case payloadLengthMismatch(declared: Int, actual: Int)
}

// MARK: - CCC BLE 指令帧

/// CCC BLE 指令帧 — 帧格式来自参考实现, 见同目录 `CCC_FRAME_ANALYSIS.md`
///
/// 线格式 (5 字节帧头 + 负载):
/// ```
///   [0]     msg_type    消息类型 (ble_msg_type_e)
///   [1]     msg_id      消息 ID (递增, 防重放辅助)
///   [2-3]   payload_len 负载长度 (大端)
///   [4]     reserved    预留 (置 0)
///   [5...]  payload
/// ```
/// 参考: `ccc_digital_key.h:119-124` `ble_frame_header_t` (packed)
public struct CCCCommandFrame: Equatable {
    /// 帧头长度 (字节)
    public static let headerLength = 5

    public let messageType: UInt8
    public let messageID: UInt8
    public let payload: Data

    public init(messageType: UInt8, messageID: UInt8, payload: Data) {
        self.messageType = messageType
        self.messageID = messageID
        self.payload = payload
    }

    /// 编码为线格式字节
    public var data: Data {
        var bytes = Data()
        bytes.append(messageType)
        bytes.append(messageID)
        bytes.append(UInt8((payload.count >> 8) & 0xFF))
        bytes.append(UInt8(payload.count & 0xFF))
        bytes.append(0x00) // reserved
        bytes.append(payload)
        return bytes
    }

    /// 严格解析: 帧头 + payload_len 精确匹配, 不匹配返回 nil
    public init?(data: Data) {
        guard data.count >= CCCCommandFrame.headerLength else { return nil }
        let declared = (Int(data[2]) << 8) | Int(data[3])
        guard data.count == CCCCommandFrame.headerLength + declared else { return nil }
        self.messageType = data[0]
        self.messageID = data[1]
        self.payload = data.subdata(in: CCCCommandFrame.headerLength..<data.count)
    }

    /// 严格解析 (抛错版本)
    public static func parse(_ data: Data) throws -> CCCCommandFrame {
        guard data.count >= CCCCommandFrame.headerLength else {
            throw CCCFrameError.tooShort(data.count)
        }
        let declared = (Int(data[2]) << 8) | Int(data[3])
        let actual = data.count - CCCCommandFrame.headerLength
        guard declared == actual else {
            throw CCCFrameError.payloadLengthMismatch(declared: declared, actual: actual)
        }
        return CCCCommandFrame(
            messageType: data[0],
            messageID: data[1],
            payload: data.subdata(in: CCCCommandFrame.headerLength..<data.count)
        )
    }
}

// MARK: - CCC 控制指令载荷 (会话层封装)

/// CCC 控制指令载荷 — 会话层明文封装。
///
/// ⚠️ TODO-verify: 参考实现只定义了帧头 (`ble_frame_header_t`) 与消息类型枚举,
/// 未定义控制指令的 payload 字段布局。以下布局为 SDK 会话数据模型 (SessionContext)
/// 的最小编码, 需对照 CCC-TS-101 Reader Protocol "Vehicle Access" 章节的
/// 命令 APDU 结构确认/替换:
/// ```
///   [0]     subcommand    (BleCommandType.rawValue: 0x01 unlock / 0x02 lock / 0x03 engine on)
///   [1-2]   session handle (大端)
///   [3-6]   message counter (大端, 抗重放)
///   [7]     keyId 长度
///   [8...]  keyId (UTF-8)
/// ```
/// 在规范确认前, 此布局仅用于单测与联调, 不得作为量产协议假设。
public enum CCCControlPayload {

    public static func build(subcommand: BleCommandType, session: SessionContext, keyId: String) -> Data {
        var payload = Data()
        payload.append(subcommand.rawValue)
        payload.append(contentsOf: withUnsafeBytes(of: session.sessionHandle.bigEndian) { Array($0) })
        payload.append(contentsOf: withUnsafeBytes(of: session.counter.bigEndian) { Array($0) })
        let keyBytes = Data(keyId.utf8)
        payload.append(UInt8(min(keyBytes.count, 0xFF)))
        payload.append(keyBytes.prefix(0xFF))
        return payload
    }

    public static func parse(_ data: Data) -> (subcommand: UInt8, sessionHandle: UInt16, counter: UInt32, keyId: String)? {
        guard data.count >= 8 else { return nil }
        let subcommand = data[0]
        let sessionHandle = (UInt16(data[1]) << 8) | UInt16(data[2])
        let counter = (UInt32(data[3]) << 24) | (UInt32(data[4]) << 16) | (UInt32(data[5]) << 8) | UInt32(data[6])
        let keyLength = Int(data[7])
        guard data.count >= 8 + keyLength else { return nil }
        let keyId = String(data: data.subdata(in: 8..<(8 + keyLength)), encoding: .utf8) ?? ""
        return (subcommand, sessionHandle, counter, keyId)
    }
}

// MARK: - CCC 消息安全提供者 (加密/签名接缝)

/// CCC 消息安全提供者 — 控制指令载荷的加密/签名接缝。
///
/// ⚠️ TODO(防幻觉): 参考实现与仓库知识库均未给出 CCC Reader Protocol 指令消息的
/// 加密/签名算法细节, 且存在冲突:
/// - `docs/sdk/PHASE2B-BLEPROTOCOL-PLAN.md` 安全通道表: CCC = ECDH + **AES-CCM**
/// - `embedded/ccc_protocol/src/security/security.c` 注释: **AES-256-GCM** (IV 12 + 密文 + Tag 16) + ECDSA P-256 (64B)
/// 两者算法不同, 必须取得 CCC-TS-101 规范原文 (Reader Protocol 章节) 后才能实现真实加密。
/// 当前仅提供接口 + 测试透传实现, 禁止在未确认算法前实现"看起来像加密"的假加密。
public protocol CCCMessageSecurityProviding: AnyObject {
    /// 加密控制载荷 (含完整性保护)
    func encrypt(_ plaintext: Data) throws -> Data
    /// 解密控制载荷 (含完整性校验)
    func decrypt(_ ciphertext: Data) throws -> Data
    /// 对载荷签名
    func sign(_ data: Data) throws -> Data
    /// 验签
    func verify(_ data: Data, signature: Data) throws -> Bool
}

/// 透传安全提供者 — 仅用于单元测试与联调。
/// 明文直通、空签名、验签恒真。**禁止用于生产**。
public final class CCCNullMessageSecurity: CCCMessageSecurityProviding {
    public init() {}

    public func encrypt(_ plaintext: Data) throws -> Data { plaintext }
    public func decrypt(_ ciphertext: Data) throws -> Data { ciphertext }
    public func sign(_ data: Data) throws -> Data { Data() }
    public func verify(_ data: Data, signature: Data) throws -> Bool { true }
}
