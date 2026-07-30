import Foundation
import YDKProto

// MARK: - SendCommand (Remote Control)

public extension YDKHubClient {

    /// 远程锁车
    func remoteLock(vehicleId: String) async throws {
        try await sendCommand(vehicleId: vehicleId, action: "lock")
    }

    /// 远程解锁
    func remoteUnlock(vehicleId: String) async throws {
        try await sendCommand(vehicleId: vehicleId, action: "unlock")
    }

    /// 远程启动
    func remoteStart(vehicleId: String) async throws {
        try await sendCommand(vehicleId: vehicleId, action: "engine_on")
    }

    /// 远程熄火
    func remoteStop(vehicleId: String) async throws {
        try await sendCommand(vehicleId: vehicleId, action: "engine_off")
    }

    // MARK: - Internal

    /// 远程控车：SDK 自动填充 source=4(Remote), key_id(本地缓存)
    private func sendCommand(vehicleId: String, action: String) async throws {
        let req = ControlCommandRequest.with {
            $0.vehicleID = vehicleId
            $0.action = action
            // 以下由 SDK 实现时填充
            // $0.userID = extractUserID(from: token)
            // $0.keyID = LocalKeyCache.shared.activeKeyId(for: vehicleId)
            // $0.source = 4  // Remote
            $0.traceID = UUID().uuidString
        }

        let resp = try await vehicleControl.sendCommand(req)
        if resp.resultCode != 0 {
            throw YDKError.hubError("REMOTE_\(resp.resultCode)", resp.errorMsg)
        }
    }
}
