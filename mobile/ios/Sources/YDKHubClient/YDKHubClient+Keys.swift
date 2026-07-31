import Foundation

// MARK: - 请求/响应模型

public struct YDKKey: Codable, Equatable {
    public let keyId: String
    public let vehicleId: String
    public let deviceId: String
    public let vehicleName: String?
    public let keyType: String        // OWNER / FRIEND / SERVICE / TEMPORARY
    public let `protocol`: String?    // CCC / ICCOA / ICCE
    public let status: String         // ACTIVE / SUSPENDED / REVOKED / EXPIRED
    public let validFrom: Int64
    public let validUntil: Int64
    public let createdAt: Int64
}

public struct YDKShare: Codable {
    public let shareId: String
    public let shareCode: String?
    public let sharingUrl: String?    // CCC Mailbox URL（仅 CCC 协议）
    public let keyId: String
    public let fromUserId: String?
    public let toUserId: String?
    public let validFrom: Int64
    public let validUntil: Int64
    public let errorCode: String?
    public let errorMsg: String?
}

public struct BindKeyResponse: Codable {
    public let keyId: String
    public let vehicleId: String
    public let errorCode: String?
    public let errorMsg: String?
}

// MARK: - 发送命令的 body

struct SendCommandBody: Encodable {
    let action: String
    let keyId: String?
    let traceId: String
}

struct CreateShareBody: Encodable {
    let keyId: String
    let toVendor: String
    let toUserId: String?
    let validFrom: Int64
    let validUntil: Int64
    let maxUses: Int32
    let traceId: String
}

struct AcceptShareBody: Encodable {
    let shareCode: String
    let traceId: String
}

// MARK: - 钥匙操作

public extension YDKHubClient {

    /// 绑定钥匙到手机
    /// SDK 自动填充: device_id, user_id, vendor, protocol, key_type, device_pubkey
    func bindKey(
        vehicleId: String,
        deviceId: String? = nil,
        devicePubkey: String? = nil
    ) async throws -> BindKeyResponse {
        let deviceManager = YDKDeviceManager.shared
        let pubkey = try? deviceManager.readPublicKeyBase64()

        let body: [String: String] = [
            "vehicleId": vehicleId,
            "deviceId": deviceId ?? deviceManager.getDeviceId(),
            "devicePubkey": devicePubkey ?? pubkey ?? "",
            "vendor": deviceManager.detectVendor().protoValue.description,
            "protocol": deviceManager.detectProtocol().protoValue.description,
            "keyType": "OWNER",
            "traceId": UUID().uuidString,
        ]
        return try await request(method: "POST", path: "/keys", body: body)
    }

    /// 解绑钥匙
    func unbindKey(keyId: String) async throws {
        let _: EmptyResponse? = try await request(method: "DELETE", path: "/keys/\(keyId)")
    }

    /// 挂起钥匙
    func suspendKey(keyId: String, reason: String? = nil) async throws {
        let body: [String: String] = [
            "reason": reason ?? "",
            "traceId": UUID().uuidString,
        ]
        let _: EmptyResponse? = try await request(method: "PUT", path: "/keys/\(keyId)/suspend", body: body)
    }

    /// 恢复钥匙
    func resumeKey(keyId: String) async throws {
        let body: [String: String] = [
            "traceId": UUID().uuidString,
        ]
        let _: EmptyResponse? = try await request(method: "PUT", path: "/keys/\(keyId)/resume", body: body)
    }

    /// 撤销钥匙
    func revokeKey(keyId: String, reason: String? = nil) async throws {
        let body: [String: String] = [
            "reason": reason ?? "",
            "traceId": UUID().uuidString,
        ]
        let _: EmptyResponse? = try await request(method: "PUT", path: "/keys/\(keyId)/revoke", body: body)
    }

    /// 续期钥匙
    func renewKey(keyId: String, validUntil: Int64) async throws {
        let body: [String: Any] = [
            "validUntil": validUntil,
            "traceId": UUID().uuidString,
        ]
        let _: EmptyResponse? = try await request(method: "PUT", path: "/keys/\(keyId)/renew", body: AnyEncodable(body as Encodable))
    }

    /// 查询单把钥匙
    func getKey(keyId: String) async throws -> YDKKey {
        try await request(method: "GET", path: "/keys/\(keyId)")
    }

    /// 查询钥匙列表
    func listKeys(vehicleId: String? = nil, status: String? = nil) async throws -> [YDKKey] {
        var query: [String: String] = [:]
        if let vid = vehicleId { query["vehicleId"] = vid }
        if let s = status { query["status"] = s }
        let resp: KeyListResponse = try await request(method: "GET", path: "/keys", query: query.isEmpty ? nil : query)
        return resp.keys ?? []
    }
}

struct KeyListResponse: Codable {
    let keys: [YDKKey]?
}

struct EmptyResponse: Codable {}
