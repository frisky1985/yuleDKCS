import Foundation

// MARK: - MailboxClient 协议接缝（测试注入 / 业务侧自定义实现）

/// `YDKMailboxClient` 的协议抽象。
///
/// 方法签名与 `YDKMailboxClient` 的 6 个 Mailbox API 一一对应，
/// 用于：
/// 1. 单元测试注入 mock，验证 `YDKShareFlow` 编排顺序；
/// 2. 业务侧替换底层 HTTP 实现（如自定义 URLSession / 网关地址策略）。
///
/// 注意：协议方法不含默认参数（Swift 协议不允许），调用方需显式传全参。
public protocol YDKMailboxClientProtocol {
    /// 创建邮箱（发送方）
    func createMailbox(
        payload: Data,
        displayInfo: Data?,
        senderVendor: String,
        senderDeviceId: String,
        expirationSeconds: Int64,
        maxUpdates: Int32,
        notificationToken: String?,
        deviceAttestation: Data?
    ) async throws -> MailboxCreateResult

    /// 读取展示信息（接收方）
    func readDisplayInfo(mailboxId: String) async throws -> MailboxDisplayInfo

    /// 读取加密内容（接收方）
    func readSecureContent(mailboxId: String) async throws -> MailboxContent

    /// 更新邮箱（KeySigning / Import / Cancel）
    @discardableResult
    func updateMailbox(
        mailboxId: String,
        dataType: MailboxDataType,
        payload: Data,
        notificationToken: String?,
        updaterDeviceId: String?
    ) async throws -> MailboxUpdateResult

    /// 删除邮箱
    @discardableResult
    func deleteMailbox(
        mailboxId: String,
        reason: String,
        deleterDeviceId: String?
    ) async throws -> MailboxDeleteResult

    /// 转移邮箱到另一设备
    @discardableResult
    func relinquishMailbox(
        mailboxId: String,
        fromDeviceId: String,
        toDeviceId: String
    ) async throws -> MailboxRelinquishResult
}

/// `YDKMailboxClient` 天然满足协议接缝（不改动其 6 API 签名）。
extension YDKMailboxClient: YDKMailboxClientProtocol {}

// MARK: - 错误类型

/// YDKShareFlow 编排层错误
public enum YDKShareFlowError: Error, LocalizedError {
    /// 分享 URL 非法：无法解析 / 缺 mailbox_id / 缺 secret fragment
    case invalidSharingURL(String)
    /// mailboxId 为空
    case invalidMailboxId(String)
    /// CreateMailbox 成功但服务端既未返回 sharingUrl 也未返回 mailboxId
    case createMailboxMissingURL
    /// 服务端 MailboxContent 返回 errorCode（200 但业务失败）
    case mailboxContentError(String, String)

    public var errorDescription: String? {
        switch self {
        case .invalidSharingURL(let url):
            return "分享 URL 非法（需含 mailbox_id 与 secret fragment）: \(url)"
        case .invalidMailboxId(let id):
            return "mailboxId 非法: \(id)"
        case .createMailboxMissingURL:
            return "CreateMailbox 响应缺少 sharingUrl 与 mailboxId，无法生成分享 URL"
        case .mailboxContentError(let code, let msg):
            return "Mailbox content 业务错误: [\(code)] \(msg)"
        }
    }
}

// MARK: - 高层分享编排层

/// CCC 分享高层编排层（CCC-TS-101 §11.4 Inter-Account Key Sharing Flow）
///
/// 把 `YDKMailboxClient` 的 6 个 Mailbox API 串成完整分享流程：
///
/// - **Sender 流**（§11.4.1 Steps 1-5）: `shareKeyViaMailbox`
///   CreateMailbox → 返回分享 URL（`https://{host}/api/v1/mailbox/{id}#{secret}`）
/// - **Receiver 接受流**（§11.4.2-11.4.4）: `acceptSharedKeyViaMailbox`
///   parseSharingURL → readDisplayInfo → readSecureContent
///   → updateMailbox(KEY_SIGNING) → updateMailbox(IMPORT)
/// - **取消流**: `cancelMailboxShare`（asSender=true → SENDER_CANCEL，
///   false → RECEIVER_CANCEL）
///
/// 密钥材料（KeySigning / Import payload）由钱包层/KeyManager 生成后传入，
/// 本层只编排顺序，不制造密钥材料。
///
/// 用法:
/// ```swift
/// let flow = YDKShareFlow(hubEndpoint: "hub.yuletech.com")
///
/// // 发送方
/// let url = try await flow.shareKeyViaMailbox(
///     payload: encryptedKeyData,
///     senderVendor: "APPLE",
///     senderDeviceId: "iphone-001",
///     host: "hub.yuletech.com:8080"
/// )
///
/// // 接收方
/// let content = try await flow.acceptSharedKeyViaMailbox(
///     urlString: url,
///     updaterDeviceId: "xiaomi-001",
///     keySigningPayload: signedKeyData,   // 钱包层
///     importPayload: importAckData        // 钱包层
/// )
/// ```
public final class YDKShareFlow {

    private let mailboxClient: YDKMailboxClientProtocol

    /// 注入自定义 MailboxClient（测试 / 自定义实现）
    public init(mailboxClient: YDKMailboxClientProtocol) {
        self.mailboxClient = mailboxClient
    }

    /// 便捷初始化：内部创建真实 `YDKMailboxClient`
    public convenience init(hubEndpoint: String, port: Int = 8080) {
        self.init(mailboxClient: YDKMailboxClient(hubEndpoint: hubEndpoint, port: port))
    }

    // MARK: - 分享 URL 生成

    /// 生成分享 URL，格式: `https://{host}/api/v1/mailbox/{mailbox_id}#{secret}`
    ///
    /// - Parameters:
    ///   - host: 裸主机名（可含端口），如 `hub.yuletech.com` 或 `hub.yuletech.com:8080`。
    ///     若误传完整 scheme（`https://...`），会自动剥离。
    ///   - mailboxId: 服务端分配的 mailbox ID
    ///   - secret: fragment secret（接收方凭此解密）。注意：不做百分号编码，
    ///     若 secret 含 `#`/`/` 等保留字符需调用方自行编码，与双端 parse 约定一致。
    public static func buildSharingURL(host: String, mailboxId: String, secret: String) -> String {
        var bareHost = host
        if let schemeRange = bareHost.range(of: "://") {
            bareHost = String(bareHost[schemeRange.upperBound...])
        }
        while bareHost.hasSuffix("/") {
            bareHost.removeLast()
        }
        return "https://\(bareHost)/api/v1/mailbox/\(mailboxId)#\(secret)"
    }

    // MARK: - Sender 流（§11.4.1 Steps 1-5）

    /// 发送方分享钥匙：创建 Mailbox 并返回分享 URL。
    ///
    /// 流程: `createMailbox` → 返回分享 URL。
    ///
    /// - Parameters:
    ///   - payload: 加密后的钥匙材料（钱包层生成）
    ///   - displayInfo: 展示信息（品牌/车型等，可选）
    ///   - senderVendor: 发送方厂商（如 "APPLE"）
    ///   - senderDeviceId: 发送方设备 ID
    ///   - host: 降级路径用裸主机名（服务端返回 sharingUrl 时不使用）
    ///   - expirationSeconds: 过期秒数（默认 24h）
    ///   - maxUpdates: 最大更新次数
    ///   - notificationToken / deviceAttestation: 透传给 Hub（可选）
    /// - Returns: 完整分享 URL `https://{host}/api/v1/mailbox/{id}#{secret}`
    /// - Throws: `YDKShareFlowError` / `YDKError`（网络、Hub 业务错误）
    public func shareKeyViaMailbox(
        payload: Data,
        displayInfo: Data? = nil,
        senderVendor: String,
        senderDeviceId: String,
        host: String,
        expirationSeconds: Int64 = 86400,
        maxUpdates: Int32 = 10,
        notificationToken: String? = nil,
        deviceAttestation: Data? = nil
    ) async throws -> String {
        // Step 1: CreateMailbox（失败直接抛出，不生成 URL）
        let result = try await mailboxClient.createMailbox(
            payload: payload,
            displayInfo: displayInfo,
            senderVendor: senderVendor,
            senderDeviceId: senderDeviceId,
            expirationSeconds: expirationSeconds,
            maxUpdates: maxUpdates,
            notificationToken: notificationToken,
            deviceAttestation: deviceAttestation
        )

        // Step 2: 优先使用服务端返回的完整 sharingUrl（含 secret fragment，
        // 见 relay.proto CreateMailboxResponse.sharing_url 注释）。
        if let sharingUrl = result.sharingUrl, !sharingUrl.isEmpty {
            // 校验形状，避免把垃圾字符串当分享 URL 返回（AC4: 不静默失败）
            guard let info = YDKMailboxClient.parseSharingURL(sharingUrl),
                  !info.mailboxId.isEmpty else {
                throw YDKShareFlowError.invalidSharingURL(sharingUrl)
            }
            return sharingUrl
        }

        // 降级路径: 服务端未返回 sharingUrl（旧版/降级实现）。
        // ⚠️ secret 由服务端生成且只存在于 sharingUrl 中，此处无法重建，
        // fragment 只能置空 —— 接收方会因缺 secret 拒绝该 URL
        // （见 acceptSharedKeyViaMailbox 的 hasSecret 校验）。
        // 若服务端单独下发 secret，可在此拼接 `#\(secret)` 另行约定。
        guard let mailboxId = result.mailboxId, !mailboxId.isEmpty else {
            throw YDKShareFlowError.createMailboxMissingURL
        }
        return Self.buildSharingURL(host: host, mailboxId: mailboxId, secret: "")
    }

    // MARK: - Receiver 接受流（§11.4.2-11.4.4）

    /// 接收方接受分享：解析 URL → 读取 → 回写 KeySigning / Import。
    ///
    /// 流程: `parseSharingURL` → `readDisplayInfo` → `readSecureContent`
    /// → `updateMailbox(KEY_SIGNING)` → `updateMailbox(IMPORT)` → 返回 content。
    /// 任一步失败（网络 / Hub 业务错误）都会抛出并中止后续步骤。
    ///
    /// - Parameters:
    ///   - urlString: 发送方分享的完整 URL（必须含 mailbox_id + secret fragment）
    ///   - updaterDeviceId: 接收方设备 ID（写入 KEY_SIGNING / IMPORT 更新）
    ///   - keySigningPayload: 钱包层/KeyManager 生成的 KeySigning 材料
    ///     （Endpoint Creation 签名结果）。默认空 Data —— SDK 编排层不制造
    ///     密钥材料，调用方务必传入真实签名数据。
    ///   - importPayload: 钱包层生成的 Import 确认材料。默认空 Data，同上。
    /// - Returns: `readSecureContent` 读取到的加密内容
    ///   （钱包层基于该内容执行 trackKey / 导入）。
    /// - Throws: `YDKShareFlowError`（非法 URL / content 业务错误）/ `YDKError`
    public func acceptSharedKeyViaMailbox(
        urlString: String,
        updaterDeviceId: String,
        keySigningPayload: Data = Data(),
        importPayload: Data = Data()
    ) async throws -> MailboxContent {
        // Step 1: 解析分享 URL（缺 mailbox_id 或缺 secret 都视为非法 → B2.3）
        guard let info = YDKMailboxClient.parseSharingURL(urlString),
              !info.mailboxId.isEmpty,
              info.hasSecret else {
            throw YDKShareFlowError.invalidSharingURL(urlString)
        }

        // Step 2: 读取展示信息（失败中止）
        _ = try await mailboxClient.readDisplayInfo(mailboxId: info.mailboxId)

        // Step 3: 读取加密内容（失败中止 → B2.4）
        let content = try await mailboxClient.readSecureContent(mailboxId: info.mailboxId)
        if let errorCode = content.errorCode, !errorCode.isEmpty {
            throw YDKShareFlowError.mailboxContentError(errorCode, content.errorMsg ?? "")
        }

        // Step 4: 回写 KeySigning（钱包层材料；失败中止）
        _ = try await mailboxClient.updateMailbox(
            mailboxId: info.mailboxId,
            dataType: .keySigning,
            payload: keySigningPayload,
            notificationToken: nil,
            updaterDeviceId: updaterDeviceId
        )

        // Step 5: 回写 Import 确认（钱包层材料；失败中止）
        _ = try await mailboxClient.updateMailbox(
            mailboxId: info.mailboxId,
            dataType: .import,
            payload: importPayload,
            notificationToken: nil,
            updaterDeviceId: updaterDeviceId
        )

        // Step 6: 返回读取到的内容（trackKey 由钱包层执行）
        return content
    }

    // MARK: - 取消流（SENDER_CANCEL=4 / RECEIVER_CANCEL=5）

    /// 取消分享：按角色写入对应取消类型。
    ///
    /// - Parameters:
    ///   - mailboxId: 目标 mailbox ID
    ///   - asSender: true → `updateMailbox(.senderCancel)`；
    ///     false → `updateMailbox(.receiverCancel)`
    ///   - updaterDeviceId: 发起取消的设备 ID
    /// - Throws: `YDKShareFlowError` / `YDKError`
    public func cancelMailboxShare(
        mailboxId: String,
        asSender: Bool,
        updaterDeviceId: String
    ) async throws {
        guard !mailboxId.isEmpty else {
            throw YDKShareFlowError.invalidMailboxId(mailboxId)
        }
        let dataType: MailboxDataType = asSender ? .senderCancel : .receiverCancel
        // 取消操作无需业务 payload（规范仅要求类型标记），传空 Data。
        _ = try await mailboxClient.updateMailbox(
            mailboxId: mailboxId,
            dataType: dataType,
            payload: Data(),
            notificationToken: nil,
            updaterDeviceId: updaterDeviceId
        )
    }

    /// 发送方取消分享（等价 `cancelMailboxShare(asSender: true)`）
    public func senderCancelMailboxShare(
        mailboxId: String,
        updaterDeviceId: String
    ) async throws {
        try await cancelMailboxShare(mailboxId: mailboxId, asSender: true, updaterDeviceId: updaterDeviceId)
    }

    /// 接收方取消分享（等价 `cancelMailboxShare(asSender: false)`）
    public func receiverCancelMailboxShare(
        mailboxId: String,
        updaterDeviceId: String
    ) async throws {
        try await cancelMailboxShare(mailboxId: mailboxId, asSender: false, updaterDeviceId: updaterDeviceId)
    }
}
