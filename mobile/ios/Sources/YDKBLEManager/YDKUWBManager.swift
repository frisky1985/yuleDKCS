import Foundation

// MARK: - UWB 测距 (FiRa)

/// UWB 测距结果
public struct UWBMeasurement {
    public let vehicleId: String
    public let distance: Double       // 米
    public let azimuth: Double?       // 角度
    public let elevation: Double?
    public let timestamp: Int64

    public init(vehicleId: String, distance: Double, azimuth: Double? = nil, elevation: Double? = nil, timestamp: Int64) {
        self.vehicleId = vehicleId
        self.distance = distance
        self.azimuth = azimuth
        self.elevation = elevation
        self.timestamp = timestamp
    }
}

/// UWB 管理器接口 (FiRa 标准抽象)
///
/// 实现说明:
/// - iOS: 依赖 NearbyInteraction framework (U1/U2 chip) + 车厂 TCU 支持
/// - Android: 依赖 UWB 硬件 (FiRa 兼容)
/// - 本期提供接口 + Mock 实现，真实集成需硬件环境
public protocol YDKUWBManaging: AnyObject {
    /// 开始测距
    func startRanging(vehicleId: String) async throws
    /// 停止测距
    func stopRanging()
    /// 测距结果回调
    var rangingResultHandler: ((UWBMeasurement) -> Void)? { get set }
}

/// Mock UWB 管理器（开发调试用）
public final class YDKMockUWBManager: YDKUWBManaging {
    public var rangingResultHandler: ((UWBMeasurement) -> Void)?
    private var timer: DispatchSourceTimer?
    private var vehicleId: String = ""

    public init() {}

    public func startRanging(vehicleId: String) async throws {
        self.vehicleId = vehicleId
        let timer = DispatchSource.makeTimerSource(queue: .main)
        timer.schedule(deadline: .now() + 1, repeating: 1)
        timer.setEventHandler { [weak self] in
            guard let self = self else { return }
            let measurement = UWBMeasurement(
                vehicleId: self.vehicleId,
                distance: Double.random(in: 0.5...5.0),
                azimuth: Double.random(in: -45...45),
                elevation: Double.random(in: -10...10),
                timestamp: Int64(Date().timeIntervalSince1970 * 1000)
            )
            self.rangingResultHandler?(measurement)
        }
        timer.resume()
        self.timer = timer
    }

    public func stopRanging() {
        timer?.cancel()
        timer = nil
    }
}
