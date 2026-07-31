import Foundation

/// 车辆状态更新（SSE 流）
public struct VehicleStatusUpdate: Codable {
    public let vehicleId: String
    public let lockStatus: Int32?
    public let engineStatus: Int32?
    public let batteryPct: Int32?
    public let latitude: Double?
    public let longitude: Double?
    public let timestamp: Int64?
}

/// 远程控车响应
public struct VehicleStatusResponse: Codable {
    public let vehicleId: String
    public let lockStatus: Int32?
    public let engineStatus: Int32?
    public let batteryPct: Int32?
    public let latitude: Double?
    public let longitude: Double?
    public let timestamp: Int64?
}

public extension YDKHubClient {

    /// 通过 SSE 流获取实时车辆状态
    ///
    /// 返回 AsyncThrowingStream，每次有新事件 yield 一个 VehicleStatusUpdate。
    /// 连接断开或错误时 stream 自动结束。
    func streamStatus(vehicleId: String) -> AsyncThrowingStream<VehicleStatusUpdate, Error> {
        AsyncThrowingStream { continuation in
            Task {
                var req = URLRequest(url: baseURL.appendingPathComponent("/vehicles/\(vehicleId)/status"))
                req.setValue("text/event-stream", forHTTPHeaderField: "Accept")
                req.setValue("Bearer \(token ?? "")", forHTTPHeaderField: "Authorization")
                req.timeoutInterval = TimeInterval(INT_MAX) // 长连接

                do {
                    let (bytes, response) = try await session.bytes(for: req)

                    guard let httpResp = response as? HTTPURLResponse,
                          httpResp.statusCode == 200 else {
                        continuation.finish(throwing: YDKError.httpError(
                            (response as? HTTPURLResponse)?.statusCode ?? 0
                        ))
                        return
                    }

                    for try await line in bytes.lines {
                        if line.hasPrefix("data: ") {
                            let json = String(line.dropFirst(6))
                            if let data = json.data(using: .utf8),
                               let update = try? decoder.decode(VehicleStatusUpdate.self, from: data) {
                                continuation.yield(update)
                            }
                        }
                    }
                    continuation.finish()
                } catch {
                    continuation.finish(throwing: YDKError.networkError(error))
                }
            }
        }
    }
}
