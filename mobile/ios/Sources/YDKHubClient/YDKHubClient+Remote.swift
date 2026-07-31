import Foundation

public extension YDKHubClient {

    /// 远程锁车
    @discardableResult
    func remoteLock(vehicleId: String, keyId: String? = nil) async throws -> ControlCommandResponse {
        try await sendCommand(vehicleId: vehicleId, action: "lock", keyId: keyId)
    }

    /// 远程解锁
    @discardableResult
    func remoteUnlock(vehicleId: String, keyId: String? = nil) async throws -> ControlCommandResponse {
        try await sendCommand(vehicleId: vehicleId, action: "unlock", keyId: keyId)
    }

    /// 远程启动
    @discardableResult
    func remoteStart(vehicleId: String, keyId: String? = nil) async throws -> ControlCommandResponse {
        try await sendCommand(vehicleId: vehicleId, action: "engine_on", keyId: keyId)
    }

    /// 远程熄火
    @discardableResult
    func remoteStop(vehicleId: String, keyId: String? = nil) async throws -> ControlCommandResponse {
        try await sendCommand(vehicleId: vehicleId, action: "engine_off", keyId: keyId)
    }

    // MARK: - Internal

    /// 远程控车：SDK 自动填充 source=4(Remote)
    @discardableResult
    private func sendCommand(vehicleId: String, action: String, keyId: String?) async throws -> ControlCommandResponse {
        let body: [String: Any] = [
            "action": action,
            "keyId": keyId ?? "",
            "traceId": UUID().uuidString,
            // source=4 (Remote) 由 Gateway 自动填充
        ]
        return try await request(
            method: "POST",
            path: "/vehicles/\(vehicleId)/command",
            body: AnyEncodable(body as Encodable)
        )
    }
}

public struct ControlCommandResponse: Codable {
    public let cmdId: String?
    public let resultCode: Int32
    public let errorMsg: String?
}
