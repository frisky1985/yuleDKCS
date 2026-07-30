import Foundation
import YDKProto

// MARK: - BindKey

public extension YDKHubClient {

    /// 绑定钥匙到手机
    ///
    /// SDK 自动填充: device_id, user_id, vendor, protocol, key_type, device_pubkey, access_level
    func bindKey(vehicleId: String) async throws -> BindKeyResponse {
        let req = BindKeyRequest.with {
            $0.vehicleID = vehicleId
            // 以下由 SDK 实现时填充（见 Phase 2a 实现计划）
            // $0.deviceID = DeviceManager.shared.deviceId
            // $0.userID = extractUserID(from: token)
            // $0.vendor = detectPhoneVendor()
            // $0.protocol = detectProtocol()
            // $0.keyType = .owner
            // $0.accessLevel = .init(lock: true, unlock: true, engine: true)
            // $0.devicePubkey = try await SecureEnclave.shared.readPublicKey()
            $0.traceID = UUID().uuidString
        }

        let resp = try await keyManagement.bindKey(req)
        if !resp.errorCode.isEmpty {
            throw YDKError.hubError(resp.errorCode, resp.errorMsg)
        }
        return resp
    }

    /// 解绑钥匙
    func unbindKey(keyId: String) async throws {
        let req = UnbindKeyRequest.with {
            $0.keyID = keyId
            $0.traceID = UUID().uuidString
        }
        let resp = try await keyManagement.unbindKey(req)
        if !resp.errorCode.isEmpty {
            throw YDKError.hubError(resp.errorCode, "")
        }
    }

    /// 挂起钥匙
    func suspendKey(keyId: String, reason: String = "") async throws {
        let req = SuspendKeyRequest.with {
            $0.keyID = keyId
            $0.reason = reason
            $0.traceID = UUID().uuidString
        }
        let resp = try await keyManagement.suspendKey(req)
        if !resp.errorCode.isEmpty {
            throw YDKError.hubError(resp.errorCode, "")
        }
    }

    /// 恢复钥匙
    func resumeKey(keyId: String) async throws {
        let req = ResumeKeyRequest.with {
            $0.keyID = keyId
            $0.traceID = UUID().uuidString
        }
        let resp = try await keyManagement.resumeKey(req)
        if !resp.errorCode.isEmpty {
            throw YDKError.hubError(resp.errorCode, "")
        }
    }

    /// 撤销钥匙
    func revokeKey(keyId: String, reason: String) async throws {
        let req = RevokeKeyRequest.with {
            $0.keyID = keyId
            $0.reason = reason
            $0.traceID = UUID().uuidString
        }
        let resp = try await keyManagement.revokeKey(req)
        if !resp.errorCode.isEmpty {
            throw YDKError.hubError(resp.errorCode, "")
        }
    }

    /// 续期钥匙
    func renewKey(keyId: String, validUntil: Int64) async throws {
        let req = RenewKeyRequest.with {
            $0.keyID = keyId
            $0.validUntil = validUntil
            $0.traceID = UUID().uuidString
        }
        let resp = try await keyManagement.renewKey(req)
        if !resp.errorCode.isEmpty {
            throw YDKError.hubError(resp.errorCode, "")
        }
    }

    /// 查询单把钥匙
    func getKey(keyId: String) async throws -> KeyItem {
        let req = GetKeyRequest.with { $0.keyID = keyId }
        let resp = try await keyManagement.getKey(req)
        if !resp.errorCode.isEmpty {
            throw YDKError.hubError(resp.errorCode, "")
        }
        // SDK 补充 vehicle_name（来自本地缓存）
        var item = resp.key
        item.vehicleName = LocalKeyCache.shared.vehicleName(for: item.vehicleID)
        return item
    }

    /// 查询钥匙列表
    func listKeys() async throws -> [KeyItem] {
        let req = ListKeysRequest()
        let resp = try await keyManagement.listKeys(req)

        return resp.keys.map { key in
            var item = key
            item.vehicleName = LocalKeyCache.shared.vehicleName(for: item.vehicleID)
            return item
        }
    }
}
