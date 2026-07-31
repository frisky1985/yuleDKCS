import Foundation

// MARK: - 数据模型

/// Mailbox 信息（从 Sharing URL 解析）
public struct MailboxInfo: Equatable {
    public let mailboxId: String
    public let secret: String

    /// 从 URL fragment 提取的 secret（空字符串表示无）
    public var hasSecret: Bool { !secret.isEmpty }
}

/// 邮箱更新操作类型（CCC-TS-101 §11.3.4）
public enum MailboxDataType: Int, Codable {
    case keyCreation = 1
    case keySigning = 2
    case `import` = 3
    case senderCancel = 4
    case receiverCancel = 5
}

/// 邮箱更新结果
public struct MailboxUpdateResult: Codable {
    public let status: String?
    public let version: Int64?
    public let errorCode: String?
    public let errorMsg: String?
}

/// 邮箱展示信息
public struct MailboxDisplayInfo: Codable {
    public let displayInfo: Data?
    public let version: Int64?
}

/// 邮箱加密内容
public struct MailboxContent: Codable {
    public let payload: Data?
    public let version: Int64?
    public let errorCode: String?
    public let errorMsg: String?
}

/// 创建邮箱结果
public struct MailboxCreateResult: Codable {
    public let mailboxId: String?
    public let sharingUrl: String?
    public let expiresAt: Int64?
}

/// 删除邮箱结果
public struct MailboxDeleteResult: Codable {
    public let success: Bool?
    public let errorCode: String?
}

/// 转移邮箱结果
public struct MailboxRelinquishResult: Codable {
    public let success: Bool?
    public let errorCode: String?
    public let errorMsg: String?
}

// MARK: - MailboxClient

/// CCC 协议 Mailbox 客户端
///
/// 通过 HTTP/JSON 调用 Hub REST Gateway 的公开 Mailbox API。
/// 无需 JWT token — 安全由 mailbox_id 随机性和 E2E 加密保障。
///
/// Sharing URL 格式: `https://hub.yuletech.com:8080/api/v1/mailbox/{id}#{secret}`
/// 注意: #secret 是 URL fragment，永不发送到服务器。
///
/// 用法:
/// ```swift
/// let client = YDKMailboxClient(hubEndpoint: "hub.yuletech.com")
///
/// // 发送方: 创建邮箱
/// let result = try await client.createMailbox(payload: ..., displayInfo: ...)
/// // result.sharingUrl → 分享给好友
///
/// // 接收方: 解析 URL，读取展示信息
/// guard let info = YDKMailboxClient.parseSharingURL(sharingUrl) else { return }
/// let display = try await client.readDisplayInfo(mailboxId: info.mailboxId)
///
/// // 接收方: 更新邮箱（KeySigning）
/// let update = try await client.updateMailbox(
///     mailboxId: info.mailboxId,
///     dataType: .keySigning,
///     payload: signedKeyData
/// )
/// ```
public final class YDKMailboxClient {

    private let baseURL: URL
    private let session: URLSession
    private let decoder = JSONDecoder()
    private let encoder = JSONEncoder()

    public init(hubEndpoint: String, port: Int = 8080) {
        self.baseURL = URL(string: "https://\(hubEndpoint):\(port)/api/v1/mailbox")!
        self.session = URLSession(configuration: .ephemeral)
    }

    // MARK: - Sharing URL 解析

    /// 解析 Sharing URL，提取 mailbox_id 和 secret
    ///
    /// URL 格式: `https://host/api/v1/mailbox/{mailbox_id}#{secret}`
    /// 或: `https://host/api/v1/mailbox/{mailbox_id}#secret={secret}`
    public static func parseSharingURL(_ urlString: String) -> MailboxInfo? {
        guard let url = URL(string: urlString),
              let host = url.host else { return nil }

        // 提取 mailbox_id: /api/v1/mailbox/{id}
        let pathComponents = url.path.split(separator: "/")
        guard pathComponents.count >= 4,
              pathComponents[pathComponents.count - 2] == "mailbox" else { return nil }
        let mailboxId = String(pathComponents.last!)

        // 提取 secret: fragment 中的值
        var secret = ""
        if let fragment = url.fragment {
            if fragment.hasPrefix("secret=") {
                secret = String(fragment.dropFirst(7))
            } else {
                secret = fragment
            }
        }

        return MailboxInfo(mailboxId: mailboxId, secret: secret)
    }

    // MARK: - Mailbox 操作

    /// 创建邮箱（发送方）
    @discardableResult
    public func createMailbox(
        payload: Data,
        displayInfo: Data? = nil,
        senderVendor: String,
        senderDeviceId: String,
        expirationSeconds: Int64 = 86400,  // 默认 24h
        maxUpdates: Int32 = 10,
        notificationToken: String? = nil,
        deviceAttestation: Data? = nil
    ) async throws -> MailboxCreateResult {
        let body: [String: Any] = [
            "payload": payload.base64EncodedString(),
            "displayInfo": displayInfo?.base64EncodedString() ?? "",
            "senderVendor": senderVendor,
            "senderDeviceId": senderDeviceId,
            "expirationSeconds": expirationSeconds,
            "maxUpdates": maxUpdates,
            "notificationToken": notificationToken ?? "",
            "deviceAttestation": deviceAttestation?.base64EncodedString() ?? "",
            "traceId": UUID().uuidString,
        ]
        return try await request(method: "POST", path: "", body: AnyEncodable(body as Encodable))
    }

    /// 读取展示信息（接收方）
    public func readDisplayInfo(mailboxId: String) async throws -> MailboxDisplayInfo {
        try await request(method: "GET", path: "/\(mailboxId)/display")
    }

    /// 读取加密内容
    public func readSecureContent(mailboxId: String) async throws -> MailboxContent {
        try await request(method: "GET", path: "/\(mailboxId)/content")
    }

    /// 更新邮箱（KeySigning / Import / Cancel）
    @discardableResult
    public func updateMailbox(
        mailboxId: String,
        dataType: MailboxDataType,
        payload: Data,
        notificationToken: String? = nil,
        updaterDeviceId: String? = nil
    ) async throws -> MailboxUpdateResult {
        let body: [String: Any] = [
            "payload": payload.base64EncodedString(),
            "sharingDataType": dataType.rawValue,
            "notificationToken": notificationToken ?? "",
            "updaterDeviceId": updaterDeviceId ?? "",
            "traceId": UUID().uuidString,
        ]
        return try await request(method: "PUT", path: "/\(mailboxId)", body: AnyEncodable(body as Encodable))
    }

    /// 删除邮箱
    @discardableResult
    public func deleteMailbox(
        mailboxId: String,
        reason: String = "completed",
        deleterDeviceId: String? = nil
    ) async throws -> MailboxDeleteResult {
        let body: [String: Any] = [
            "reason": reason,
            "deleterDeviceId": deleterDeviceId ?? "",
            "traceId": UUID().uuidString,
        ]
        return try await request(method: "DELETE", path: "/\(mailboxId)", body: AnyEncodable(body as Encodable))
    }

    /// 转移邮箱到另一设备
    @discardableResult
    public func relinquishMailbox(
        mailboxId: String,
        fromDeviceId: String,
        toDeviceId: String
    ) async throws -> MailboxRelinquishResult {
        let body: [String: Any] = [
            "fromDeviceId": fromDeviceId,
            "toDeviceId": toDeviceId,
            "traceId": UUID().uuidString,
        ]
        return try await request(method: "POST", path: "/\(mailboxId)/relinquish", body: AnyEncodable(body as Encodable))
    }
}

// MARK: - 内部 HTTP 请求

extension YDKMailboxClient {
    private func request<T: Decodable>(
        method: String,
        path: String,
        body: Encodable? = nil
    ) async throws -> T {
        let url = URL(string: path, relativeTo: baseURL)!
        var req = URLRequest(url: url)
        req.httpMethod = method
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        req.setValue("application/json", forHTTPHeaderField: "Accept")

        if let body = body {
            req.httpBody = try encoder.encode(AnyEncodable(body))
        }

        let (data, response) = try await session.data(for: req)

        guard let httpResponse = response as? HTTPURLResponse else {
            throw YDKError.internal_("invalid response")
        }

        if httpResponse.statusCode >= 400 {
            if let errorBody = try? decoder.decode(HubErrorResponse.self, from: data),
               let code = errorBody.code ?? errorBody.error {
                throw YDKError.hubError(code, errorBody.message ?? "")
            }
            throw YDKError.httpError(httpResponse.statusCode)
        }

        do {
            return try decoder.decode(T.self, from: data)
        } catch {
            throw YDKError.decodingFailed(error.localizedDescription)
        }
    }
}
