import Foundation
import YDKProto

// MARK: - CreateShare

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
        toVendor: PhoneVendor,
        toUserId: String? = nil,
        validFrom: Int64 = 0,
        validUntil: Int64 = 0,
        maxUses: Int32 = 0
    ) async throws -> CreateShareResponse {
        let req = CreateShareRequest.with {
            $0.keyID = keyId
            $0.toVendor = toVendor
            if let uid = toUserId { $0.toUserID = uid }
            $0.validFrom = validFrom
            $0.validUntil = validUntil
            $0.maxUses = maxUses
            $0.accessLevel = AccessLevel.with {
                $0.lock = true; $0.unlock = true; $0.engine = true
            }
            $0.traceID = UUID().uuidString
        }

        let resp = try await keyShare.createShare(req)
        if !resp.errorCode.isEmpty {
            throw YDKError.hubError(resp.errorCode, "")
        }
        return resp
    }

    /// 接受分享
    ///
    /// SDK 自动填充: device_id, user_id, vendor, device_pubkey
    func acceptShare(shareCode: String) async throws -> AcceptShareResponse {
        let req = AcceptShareRequest.with {
            $0.shareCode = shareCode
            // 以下由 SDK 实现时填充
            // $0.deviceID = DeviceManager.shared.deviceId
            // $0.userID = extractUserID(from: token)
            // $0.vendor = detectPhoneVendor()
            // $0.devicePubkey = try await SecureEnclave.shared.readPublicKey()
            $0.traceID = UUID().uuidString
        }

        let resp = try await keyShare.acceptShare(req)
        if !resp.errorCode.isEmpty {
            throw YDKError.hubError(resp.errorCode, "")
        }
        return resp
    }

    /// 取消分享
    func cancelShare(shareId: String) async throws {
        let req = CancelShareRequest.with {
            $0.shareID = shareId
            $0.traceID = UUID().uuidString
        }
        let resp = try await keyShare.cancelShare(req)
        if !resp.errorCode.isEmpty {
            throw YDKError.hubError(resp.errorCode, "")
        }
    }

    /// 查询分享记录
    func getShare(shareId: String) async throws -> GetShareResponse {
        let req = GetShareRequest.with { $0.shareID = shareId }
        let resp = try await keyShare.getShare(req)
        if !resp.errorCode.isEmpty {
            throw YDKError.hubError(resp.errorCode, "")
        }
        return resp
    }
}
