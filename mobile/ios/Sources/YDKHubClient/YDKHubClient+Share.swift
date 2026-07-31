import Foundation

public extension YDKHubClient {

    /// 创建钥匙分享
    ///
    /// - Parameters:
    ///   - keyId: 要分享的钥匙 ID
    ///   - toVendor: 好友的手机厂商（⚠️ App 必须正确传入）
    ///   - toUserId: 好友的用户 ID（可选，为空则生成分享码）
    ///   - validFrom: 有效期起始
    ///   - validUntil: 有效期结束
    ///   - maxUses: 最大使用次数（0=无限制）
    /// - Returns: 分享信息（含 6 位分享码）
    func createShare(
        keyId: String,
        toVendor: String,
        toUserId: String? = nil,
        validFrom: Int64 = 0,
        validUntil: Int64 = 0,
        maxUses: Int32 = 0
    ) async throws -> YDKShare {
        let body: [String: Any] = [
            "keyId": keyId,
            "toVendor": toVendor,
            "toUserId": toUserId ?? "",
            "validFrom": validFrom,
            "validUntil": validUntil,
            "maxUses": maxUses,
            "traceId": UUID().uuidString,
        ]
        return try await request(method: "POST", path: "/shares", body: AnyEncodable.json(body))
    }

    /// 接受分享
    ///
    /// SDK 自动填充: device_id, user_id, vendor, device_pubkey
    func acceptShare(shareCode: String) async throws -> YDKKey {
        let deviceManager = YDKDeviceManager.shared
        let pubkey = try? deviceManager.readPublicKeyBase64()

        let body: [String: String] = [
            "shareCode": shareCode,
            "deviceId": deviceManager.getDeviceId(),
            "devicePubkey": pubkey ?? "",
            "vendor": deviceManager.detectVendor().protoName,
            "traceId": UUID().uuidString,
        ]
        return try await request(method: "POST", path: "/shares/accept", body: body)
    }

    /// 取消分享
    func cancelShare(shareId: String) async throws {
        let _: EmptyResponse? = try await request(method: "DELETE", path: "/shares/\(shareId)")
    }

    /// 查询分享记录
    func getShare(shareId: String) async throws -> YDKShare {
        try await request(method: "GET", path: "/shares/\(shareId)")
    }
}
