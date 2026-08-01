import Foundation

// MARK: - NFC 备用解锁

/// NFC 车辆信息
public struct NFCVehicleInfo {
    public let vehicleId: String
    public let tagId: String
    public let protocolType: Int
    public init(vehicleId: String, tagId: String, protocolType: Int) {
        self.vehicleId = vehicleId
        self.tagId = tagId
        self.protocolType = protocolType
    }
}

/// NFC 指令类型
public enum NFCCommandType: UInt8 {
    case unlock = 0x01
    case lock = 0x02
    case startEngine = 0x03
}

/// NFC 管理器接口
///
/// 实现说明:
/// - iOS: `YDKCoreNFCManager` — CoreNFC (NFCTagReaderSession, ISO 14443), 编译级真实实现
/// - Android: `AndroidNfcManager` — NfcAdapter + ISO-DEP (见 NfcManager.kt)
/// - 真机依赖: 读卡/发指令需 NFC 硬件 + 系统权限/entitlement;
///   无硬件或非 iOS 编译环境走 `YDKNFCError.coreNFCUnavailable` 降级错误路径
public protocol YDKNFCManaging: AnyObject {
    /// 读取车辆 NFC 标签（手机没电/无网络时）
    func readVehicleTag() async throws -> NFCVehicleInfo
    /// 通过 NFC 通道发送指令
    func sendCommandViaNFC(command: NFCCommandType) async throws
}

// MARK: - NFC 错误

/// NFC 错误类型
public enum YDKNFCError: LocalizedError {
    /// CoreNFC 在当前编译环境不可用（非 iOS 平台 / 无 iOS SDK / 未加 entitlement）
    case coreNFCUnavailable(String)
    /// 设备无 NFC 硬件或系统 NFC 关闭
    case hardwareUnavailable
    /// NFC 会话创建失败
    case sessionCreationFailed
    /// 未检测到标签
    case tagNotDetected
    /// 标签类型不受支持（非 ISO 14443 系列）
    case unsupportedTagType
    /// 连接标签失败
    case connectionFailed(String)
    /// 指令执行失败（SW 非 0x9000 / 通信错误）
    case commandFailed(String)
    /// 用户取消
    case userCancelled
    /// 会话超时
    case timeout

    public var errorDescription: String? {
        switch self {
        case .coreNFCUnavailable(let reason):
            return "CoreNFC 不可用: \(reason)"
        case .hardwareUnavailable:
            return "设备无 NFC 硬件或系统 NFC 已关闭"
        case .sessionCreationFailed:
            return "NFC 会话创建失败"
        case .tagNotDetected:
            return "未检测到 NFC 标签"
        case .unsupportedTagType:
            return "不支持的标签类型（需 ISO 14443 / MiFare 系列）"
        case .connectionFailed(let reason):
            return "标签连接失败: \(reason)"
        case .commandFailed(let reason):
            return "NFC 指令执行失败: \(reason)"
        case .userCancelled:
            return "用户取消了 NFC 会话"
        case .timeout:
            return "NFC 会话超时"
        }
    }
}

// MARK: - APDU 构建（纯逻辑, 不依赖硬件, 可单测）

/// NFC APDU / 命令字节构建与解析 — 双端一致契约
///
/// 与 Android `NfcCommandBuilder` 保持同一字节映射（Python 交叉验证）。
/// 车辆标签安全模块约定的专有指令 (ISO 7816-4):
///   [CLA=0x80][INS=0xD2][P1=指令码][P2=0x00][Lc=0x00][Le=0x00]
/// 响应末两字节为 SW1SW2, 0x9000 表示成功。
public enum YDKNFCApdu {
    /// 构建车辆控制指令 APDU（P1 携带指令码, 与 `NFCCommandType` 对齐）
    public static func buildCommand(_ command: NFCCommandType) -> Data {
        Data([0x80, 0xD2, command.rawValue, 0x00, 0x00, 0x00])
    }

    /// 构建读取车辆记录 APDU（ISO 7816-4 READ BINARY, 读 64 字节）
    public static func buildReadVehicleRecord() -> Data {
        Data([0x00, 0xB0, 0x00, 0x00, 0x40])
    }

    /// 构建 MiFare 读取 APDU（READ block 4, NDEF 起始块）
    public static func buildReadMifareBlock() -> Data {
        Data([0x30, 0x04])
    }

    /// tagId 十六进制格式化（大写, 无分隔符; 与 Android `NfcCommandBuilder.tagIdHex` 一致）
    public static func tagIdHex(_ identifier: Data) -> String {
        identifier.map { String(format: "%02X", $0) }.joined()
    }

    /// 响应成功判定: 末两字节 == 0x90 0x00
    public static func isSuccess(response: Data) -> Bool {
        guard response.count >= 2 else { return false }
        let sw = response.suffix(2)
        return sw[sw.startIndex] == 0x90 && sw[sw.index(after: sw.startIndex)] == 0x00
    }

    /// 从响应解析 vehicleId: 去 SW1SW2 + 去尾零/空白后按 UTF-8 解码
    public static func parseVehicleId(from response: Data) -> String? {
        var bytes = Array(response)
        if bytes.count >= 2 { bytes.removeLast(2) } // 去掉状态字 SW1SW2
        while let last = bytes.last, last == 0x00 || last == 0x20 { bytes.removeLast() }
        guard !bytes.isEmpty else { return nil }
        let text = String(decoding: bytes, as: UTF8.self)
            .trimmingCharacters(in: .whitespacesAndNewlines)
        return text.isEmpty ? nil : text
    }
}

// MARK: - CoreNFC 真实实现（编译级）

#if canImport(CoreNFC)
import CoreNFC
#endif

/// CoreNFC 真实实现 — 车辆 NFC 标签读取 + 指令写入（备用解锁通道）
///
/// 平台事实 (Apple 官方):
/// - `NFCTagReaderSession` (iOS 11+) + pollingOption `.iso14443` 可检测
///   ISO 14443 系列标签 (NFCMiFareTag / NFCISO7816Tag), 二者均暴露 `identifier` (tagId)
/// - `session.connect(to:)` 连接后, ISO 7816 标签经 `sendCommand(apdu:)`,
///   MiFare 标签经 `sendMiFareCommand` 收发指令
/// - 真机前置: Info.plist `NFCReaderUsageDescription` + entitlement
///   `com.apple.developer.nfc.readersession.formats = ["NDEF", "TAG"]`
///   （模拟器 / 无 CoreNFC 编译环境走 `coreNFCUnavailable` 降级错误路径）
@available(iOS 11.0, *)
public final class YDKCoreNFCManager: YDKNFCManaging {

    /// 绑定的车辆标签 ID（可选）; 传入后读到的 tagId 不一致将拒绝操作
    private let expectedTagId: String?

    public init(expectedTagId: String? = nil) {
        self.expectedTagId = expectedTagId
    }

    // MARK: YDKNFCManaging

    public func readVehicleTag() async throws -> NFCVehicleInfo {
        #if canImport(CoreNFC)
        return try await runTagSession(mode: .read)
        #else
        throw YDKNFCError.coreNFCUnavailable(
            "当前编译环境无 CoreNFC（仅 iOS 支持）; 真机联调需 iOS 设备 + entitlement")
        #endif
    }

    public func sendCommandViaNFC(command: NFCCommandType) async throws {
        #if canImport(CoreNFC)
        _ = try await runTagSession(mode: .command(command))
        #else
        throw YDKNFCError.coreNFCUnavailable(
            "当前编译环境无 CoreNFC（仅 iOS 支持）; 真机联调需 iOS 设备 + entitlement")
        #endif
    }

    #if canImport(CoreNFC)

    /// 会话模式: 读取车辆信息 / 发送控制指令
    private enum SessionMode {
        case read
        case command(NFCCommandType)

        /// 是否为读取模式（用于 alertMessage 文案）
        var isRead: Bool {
            if case .read = self { return true }
            return false
        }
    }

    /// 启动一次标签会话并等待结果（CoreNFC 回调经续体桥接回 async/await）
    private func runTagSession(mode: SessionMode) async throws -> NFCVehicleInfo {
        guard NFCTagReaderSession.readingAvailable else {
            throw YDKNFCError.hardwareUnavailable
        }
        let proxy = YDKNFCSessionProxy(expectedTagId: expectedTagId, mode: mode)
        return try await withCheckedThrowingContinuation { continuation in
            proxy.continuation = continuation
            guard let session = NFCTagReaderSession(pollingOption: [.iso14443],
                                                    delegate: proxy,
                                                    queue: .main) else {
                continuation.resume(throwing: YDKNFCError.sessionCreationFailed)
                return
            }
            proxy.session = session
            session.alertMessage = mode.isRead
                ? "将 iPhone 靠近车辆 NFC 标签以读取车辆信息"
                : "将 iPhone 靠近车辆 NFC 标签以执行指令"
            session.begin()
        }
    }

    /// 单次会话代理: 桥接 CoreNFC 回调与 Swift 并发续体
    private final class YDKNFCSessionProxy: NSObject, NFCTagReaderSessionDelegate {
        var session: NFCTagReaderSession?
        var continuation: CheckedContinuation<NFCVehicleInfo, Error>?
        private let expectedTagId: String?
        private let mode: SessionMode
        private var finished = false

        init(expectedTagId: String?, mode: SessionMode) {
            self.expectedTagId = expectedTagId
            self.mode = mode
        }

        // MARK: NFCTagReaderSessionDelegate

        func tagReaderSessionDidBecomeActive(_ session: NFCTagReaderSession) {}

        func tagReaderSession(_ session: NFCTagReaderSession, didInvalidateWithError error: Error) {
            // 系统侧已使会话失效; 若业务侧已结束（成功/失败路径已先置 finished）则忽略
            guard !finished else { return }
            if let nfcError = error as? NFCReaderError {
                switch nfcError.code {
                case .readerSessionInvalidationErrorUserCanceled:
                    fail(.userCancelled, session: nil)
                case .readerSessionInvalidationErrorSessionTimeout:
                    fail(.timeout, session: nil)
                default:
                    fail(.commandFailed("NFC 会话失效: \(error.localizedDescription)"), session: nil)
                }
            } else {
                fail(.commandFailed("NFC 会话失效: \(error.localizedDescription)"), session: nil)
            }
        }

        func tagReaderSession(_ session: NFCTagReaderSession, didDetect tags: [NFCTag]) {
            guard !finished else { return }
            guard let tag = tags.first else {
                fail(.tagNotDetected, session: session)
                return
            }
            // 多标签场景系统已暂停轮询; 直接连接第一个标签
            session.connect(to: tag) { [weak self] error in
                guard let self, !self.finished else { return }
                if let error {
                    self.fail(.connectionFailed(error.localizedDescription), session: session)
                    return
                }
                self.handleConnected(tag, session: session)
            }
        }

        // MARK: 标签处理

        private func handleConnected(_ tag: NFCTag, session: NFCTagReaderSession) {
            switch tag {
            case .iso7816(let tag7816):
                handleISO7816(tag7816, session: session)
            case .miFare(let miFare):
                handleMiFare(miFare, session: session)
            default:
                fail(.unsupportedTagType, session: session)
            }
        }

        private func handleISO7816(_ tag: NFCISO7816Tag, session: NFCTagReaderSession) {
            let tagId = YDKNFCApdu.tagIdHex(tag.identifier)
            guard validate(tagId: tagId, session: session) else { return }

            switch mode {
            case .read:
                tag.sendCommand(apdu: NFCISO7816APDU(instructionClass: 0x00,
                                                     instructionCode: 0xB0,
                                                     p1Parameter: 0x00,
                                                     p2Parameter: 0x00,
                                                     data: Data(),
                                                     expectedResponseLength: 64)) { [weak self] response, error in
                    guard let self, !self.finished else { return }
                    if let error {
                        self.fail(.commandFailed(error.localizedDescription), session: session)
                        return
                    }
                    self.succeed(self.makeInfo(tagId: tagId, response: response), session: session)
                }
            case .command(let command):
                tag.sendCommand(apdu: NFCISO7816APDU(instructionClass: 0x80,
                                                     instructionCode: 0xD2,
                                                     p1Parameter: command.rawValue,
                                                     p2Parameter: 0x00,
                                                     data: Data(),
                                                     expectedResponseLength: 2)) { [weak self] response, error in
                    guard let self, !self.finished else { return }
                    if let error {
                        self.fail(.commandFailed(error.localizedDescription), session: session)
                        return
                    }
                    guard YDKNFCApdu.isSuccess(response: response) else {
                        self.fail(.commandFailed(self.swDescription(response)), session: session)
                        return
                    }
                    self.succeed(self.makeInfo(tagId: tagId, response: response), session: session)
                }
            }
        }

        private func handleMiFare(_ tag: NFCMiFareTag, session: NFCTagReaderSession) {
            let tagId = YDKNFCApdu.tagIdHex(tag.identifier)
            guard validate(tagId: tagId, session: session) else { return }

            switch mode {
            case .read:
                tag.sendMiFareCommand(command: YDKNFCApdu.buildReadMifareBlock()) { [weak self] response, error in
                    guard let self, !self.finished else { return }
                    if let error {
                        self.fail(.commandFailed(error.localizedDescription), session: session)
                        return
                    }
                    self.succeed(self.makeInfo(tagId: tagId, response: response), session: session)
                }
            case .command(let command):
                tag.sendMiFareCommand(command: YDKNFCApdu.buildCommand(command)) { [weak self] response, error in
                    guard let self, !self.finished else { return }
                    if let error {
                        self.fail(.commandFailed(error.localizedDescription), session: session)
                        return
                    }
                    guard YDKNFCApdu.isSuccess(response: response) else {
                        self.fail(.commandFailed(self.swDescription(response)), session: session)
                        return
                    }
                    self.succeed(self.makeInfo(tagId: tagId, response: response), session: session)
                }
            }
        }

        /// tagId 校验（绑定时不一致直接拒绝）
        private func validate(tagId: String, session: NFCTagReaderSession) -> Bool {
            if let expected = expectedTagId, expected != tagId {
                fail(.commandFailed("标签 ID 与绑定不一致"), session: session)
                return false
            }
            return true
        }

        /// 组装 NFCVehicleInfo: vehicleId 优先取记录区解析结果, 兜底用 tagId
        private func makeInfo(tagId: String, response: Data) -> NFCVehicleInfo {
            let vehicleId = YDKNFCApdu.parseVehicleId(from: response) ?? tagId
            // protocolType: 1 = ISO 14443-4 (ISO-DEP) / 2 = MiFare Classic
            return NFCVehicleInfo(vehicleId: vehicleId, tagId: tagId, protocolType: 1)
        }

        /// 响应状态字描述 (SW1SW2 十六进制)
        private func swDescription(_ response: Data) -> String {
            let sw = response.suffix(2).map { String(format: "%02X", $0) }.joined()
            return "车辆 NFC 指令执行失败, SW=\(sw)"
        }

        // MARK: 续体终结（先置 finished 再 invalidate, 避免 didInvalidateWithError 竞态双终结）

        private func succeed(_ info: NFCVehicleInfo, session: NFCTagReaderSession) {
            guard !finished else { return }
            finished = true
            session.invalidate()
            continuation?.resume(returning: info)
            continuation = nil
            self.session = nil
        }

        private func fail(_ error: YDKNFCError, session: NFCTagReaderSession?) {
            guard !finished else { return }
            finished = true
            if let session {
                session.invalidate(errorMessage: error.errorDescription ?? "NFC 操作失败")
            }
            continuation?.resume(throwing: error)
            continuation = nil
            self.session = nil
        }
    }

    #endif
}
