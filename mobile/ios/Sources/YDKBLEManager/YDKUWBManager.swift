import Foundation

// MARK: - UWB 测距 (FiRa)

/// UWB 测距结果
public struct UWBMeasurement {
    public let vehicleId: String
    public let distance: Double       // 米
    public let azimuth: Double?       // 角度 (度)
    public let elevation: Double?     // 角度 (度)
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
/// - iOS: 依赖 NearbyInteraction framework (U1/U2 chip) + 车厂 TCU 支持 → `YDKNIUWBManager`
/// - Android: 依赖 UWB 硬件 (FiRa 兼容) → `AndroidUwbManager` (android.uwb, API 34+)
/// - 无硬件/调试环境 → `YDKMockUWBManager`
public protocol YDKUWBManaging: AnyObject {
    /// 开始测距
    func startRanging(vehicleId: String) async throws
    /// 停止测距
    func stopRanging()
    /// 测距结果回调
    var rangingResultHandler: ((UWBMeasurement) -> Void)? { get set }
}

/// UWB 真实实现错误 (协议级, 不依赖 NearbyInteraction 框架)
public enum YDKUWBError: Error, Equatable {
    /// 设备无 UWB 硬件 / 非 iOS 平台 (NISession.isSupported == false)
    case unsupportedPlatform
    /// 车端 discovery token 尚未注入 (需先经 BLE 交换)
    case missingPeerDiscoveryToken
    /// token 数据无法反序列化为 NIDiscoveryToken (NSSecureCoding 解析失败)
    case invalidTokenData
    /// 本端 discovery token 尚未就绪 (NISession.discoveryToken == nil)
    case tokenNotReady
    /// NISession 失效 (用户拒绝授权 / 系统限制 / 资源超时)
    case sessionInvalidated(String)
}

/// 平台能力检测 (不依赖 NearbyInteraction 框架, 可在任意宿主编译)
public enum YDKUWBPlatform {
    /// 当前宿主是否支持 NearbyInteraction 真实实现
    /// (iOS 14+ 且编译含 NearbyInteraction 框架; macOS 宿主恒为 false)
    public static var supportsNearbyInteraction: Bool {
        #if canImport(NearbyInteraction)
        if #available(iOS 14.0, *) { return true }
        #endif
        return false
    }
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

// ─────────────────────────────────────────────────────────────────────────────
// 真实实现: NearbyInteraction (iOS 14+, U1/U2 chip)
//
// 平台事实 (Apple 官方头文件 NIConfiguration.h / NISession.h / NINearbyObject.h,
// 详见 docs/sdk/PHASE2G-UWB-PLATFORM.md):
// 1. NISession 是 UWB 测距会话; NINearbyPeerConfiguration 指定对端 (车端 TCU)
//    discovery token — Swift 构造器标签为 `peerToken:` (initWithPeerToken:)。
// 2. NIDiscoveryToken 无任何公开 init (init/new 均 NS_UNAVAILABLE), 但实现
//    NSSecureCoding → 双端 token 交换必须走 NSKeyedArchiver/NSKeyedUnarchiver
//    序列化 (本端: session.discoveryToken → archivedData; 车端: unarchivedObject)。
// 3. NINearbyObject 提供 distance (米, 可选) + direction (单位向量 simd_float3?);
//    无 azimuth/elevation 属性 — 方位角/仰角需由 direction 计算:
//      azimuth = atan2(direction.y, direction.x);  elevation = asin(direction.z)
//    (iOS 16+ 另有 horizontalAngle 属性, 本项目平台 iOS 15 故用 direction 计算)。
// 4. 会话仅在前台有效: 退后台触发 sessionWasSuspended (iOS 14+ 即有),
//    回前台 sessionSuspensionEnded 后可重新 run; iOS 14 退后台也可能直接失效。
// 5. Info.plist 需 NSNearbyInteractionAllowOnceUsageDescription (iOS 15+)
//    或 NSNearbyInteractionUsageDescription (iOS 14), 否则首次 run 即 invalidate
//    (NISession.Error.userDidNotAllow)。
// 6. NISession.delegate 为 weak — 调用方必须强持有本管理器实例;
//    delegateQueue 默认 nil = 主队列回调。
// 7. NISession.isSupported 自 iOS 16 起 deprecated → iOS 16+ 用
//    NISession.deviceCapabilities.supportsPreciseDistanceMeasurement。
//
// macOS 宿主说明: NearbyInteraction 框架仅 iOS 可用 (canImport 在 macOS 上为 true
// 但 API 全部 API_UNAVAILABLE(macos)), 真实实现以 `#if canImport(NearbyInteraction) && !os(macOS)`
// 包裹 — macOS 宿主编译时整块被排除, 不影响 iOS 构建;
// swiftc -parse 语法验证不受影响 (parse 不做语义/可用性分析)。
// ─────────────────────────────────────────────────────────────────────────────
#if canImport(NearbyInteraction) && !os(macOS)
import NearbyInteraction
import simd

@available(iOS 14.0, *)
public final class YDKNIUWBManager: NSObject, YDKUWBManaging, NISessionDelegate {

    // MARK: - 状态

    private let session = NISession()
    private var vehicleId: String = ""
    private var isRanging = false
    /// 车端 discovery token — 由上层经 BLE 交换后调用 `injectPeerDiscoveryToken(data:)` 注入
    private var peerDiscoveryToken: NIDiscoveryToken?

    // MARK: - 对外回调

    public var rangingResultHandler: ((UWBMeasurement) -> Void)?
    /// 会话失效回调 (用户拒绝授权 / 系统限制 / 退后台失效) — 上层可据此提示用户
    public var sessionInvalidatedHandler: ((Error) -> Void)?
    /// 前后台挂起/恢复回调 (true=已挂起, false=已恢复) — 上层可据此暂停/恢复 UI
    public var sessionSuspensionHandler: ((Bool) -> Void)?
    /// 最近一次失效错误 (便于诊断)
    public private(set) var lastSessionError: Error?

    // MARK: - 生命周期

    public override init() {
        super.init()
        session.delegate = self
        // delegateQueue 默认 nil → delegate 回调在主队列执行
    }

    deinit {
        session.invalidate()
    }

    // MARK: - Token 交换 (车端 token 经 BLE 通道传输)

    /// 导出本端 discovery token (序列化为 Data, 经 BLE 上行写入车端)。
    /// NIDiscoveryToken 无公开 init, 交换依赖 NSSecureCoding 归档。
    /// - Throws: `YDKUWBError.tokenNotReady` (session.discoveryToken 尚未就绪)
    public func exportLocalDiscoveryToken() throws -> Data {
        guard let token = session.discoveryToken else {
            throw YDKUWBError.tokenNotReady
        }
        return try NSKeyedArchiver.archivedData(withRootObject: token, requiringSecureCoding: true)
    }

    /// 注入车端 discovery token (来自 BLE 交换的二进制数据, NSSecureCoding 反序列化)。
    /// - Throws: `YDKUWBError.invalidTokenData` (数据无法解析为 NIDiscoveryToken)
    public func injectPeerDiscoveryToken(data: Data) throws {
        guard let token = try? NSKeyedUnarchiver.unarchivedObject(
            ofClass: NIDiscoveryToken.self, from: data
        ) else {
            throw YDKUWBError.invalidTokenData
        }
        peerDiscoveryToken = token
    }

    /// 直接注入车端 discovery token 实例 (车端经 BLE 下发归档数据后由上层反序列化)
    public func injectPeerDiscoveryToken(_ token: NIDiscoveryToken) {
        peerDiscoveryToken = token
    }

    // MARK: - YDKUWBManaging

    public func startRanging(vehicleId: String) async throws {
        guard supportsUWB else {
            throw YDKUWBError.unsupportedPlatform
        }
        guard let peerToken = peerDiscoveryToken else {
            throw YDKUWBError.missingPeerDiscoveryToken
        }
        // 若上一轮会话未清理, 先失效重建
        if isRanging {
            session.invalidate()
        }
        self.vehicleId = vehicleId
        // 注意: 构造器标签为 peerToken: (对应 initWithPeerToken:)
        let configuration = NINearbyPeerConfiguration(peerToken: peerToken)
        session.run(configuration)
        // 注: run 不抛; 配置/授权问题经 didInvalidateWithError 回调暴露
    }

    public func stopRanging() {
        isRanging = false
        session.invalidate()
    }

    // MARK: - 平台能力 (内部)

    private var supportsUWB: Bool {
        if #available(iOS 16.0, *) {
            // iOS 16+ 推荐能力探测 (isSupported 已 deprecated)
            return NISession.deviceCapabilities.supportsPreciseDistanceMeasurement
        } else {
            return NISession.isSupported
        }
    }

    // MARK: - NISessionDelegate

    public func session(_ session: NISession, didUpdate nearbyObjects: [NINearbyObject]) {
        for object in nearbyObjects {
            // distance 为 nil 表示本帧未测得 (首次上电/丢包), 跳过
            guard let distance = object.distance else { continue }
            let measurement = UWBMeasurement(
                vehicleId: vehicleId,
                distance: Double(distance),                          // 米 (Float)
                azimuth: azimuthDegrees(from: object.direction),     // 由方向向量计算 (度)
                elevation: elevationDegrees(from: object.direction),
                timestamp: Int64(Date().timeIntervalSince1970 * 1000)
            )
            rangingResultHandler?(measurement)
        }
    }

    public func session(_ session: NISession, didRemove nearbyObjects: [NINearbyObject], reason: NINearbyObject.RemovalReason) {
        // 对端移除 (.timeout 超时 / .peerEnded 车端结束会话)。
        // 保持会话存活等待重连, 由上层依据业务决定是否 stopRanging。
    }

    public func session(_ session: NISession, didInvalidateWith error: Error) {
        isRanging = false
        lastSessionError = error
        sessionInvalidatedHandler?(error)
    }

    public func sessionWasSuspended(_ session: NISession) {
        isRanging = false
        sessionSuspensionHandler?(true)
    }

    public func sessionSuspensionEnded(_ session: NISession) {
        // 回前台后可在此重新 session.run 恢复测距
        sessionSuspensionHandler?(false)
    }

    @available(iOS 15.0, *)
    public func session(_ session: NISession, didGenerateShareableConfigurationData shareableConfigurationData: Data, for object: NINearbyObject) {
        // 仅 NINearbyAccessoryConfiguration (配件) 会话触发; 本实现用 NINearbyPeerConfiguration,
        // 不触发该回调 — 保留占位说明。
    }

    // MARK: - 方向向量 → 角度 (度)

    /// 由单位方向向量计算方位角 (度): 设备参考系 x 向前 / y 向左 / z 向上,
    /// azimuth = atan2(y, x) (绕 z 轴, 弧度), 再转度。
    private func azimuthDegrees(from direction: simd_float3?) -> Double? {
        guard let d = direction else { return nil }
        return Double(atan2(d.y, d.x)) * 180.0 / .pi
    }

    /// 由单位方向向量计算仰角 (度): elevation = asin(z) (相对水平面, 弧度), 再转度。
    private func elevationDegrees(from direction: simd_float3?) -> Double? {
        guard let d = direction else { return nil }
        return Double(asin(d.z)) * 180.0 / .pi
    }
}
#endif
