import Foundation
import YDKHubClient

// MARK: - 离线授权裁决结果

/// 离线授权拒绝原因
public enum YDKOfflineDenialReason: String, Equatable {
    /// 钥匙已被撤销（REVOKED / 未知状态 fail-closed 兜底）
    case revoked
    /// 钥匙已被挂起（SUSPENDED）
    case suspended
    /// 钥匙已过期（云端状态 EXPIRED 或 now > validUntil）
    case expired
    /// 钥匙尚未生效（now < validFrom）
    case notYetValid
    /// 缓存过旧（距上次同步超过离线宽限期）
    case staleCache
}

/// 离线授权裁决结果
public struct YDKOfflineAuthorization: Equatable {
    /// 是否允许离线使用
    public let allowed: Bool
    /// 拒绝原因（allowed == true 时为 nil）
    public let reason: YDKOfflineDenialReason?

    public init(allowed: Bool, reason: YDKOfflineDenialReason?) {
        self.allowed = allowed
        self.reason = reason
    }
}

// MARK: - 离线授权裁决器

/// 离线授权回退机制（方案 A）— 纯本地纯函数裁决器
///
/// 语义: 当 Hub 云端不可达时, SDK 回退到本地缓存进行离线授权判断。
/// 在 BLE/NFC 离线解锁前调用本裁决器, 回答「这把钥匙现在还能不能用」。
///
/// 裁决规则（fail-closed, 按顺序短路）:
/// 1. status == REVOKED   → 拒绝 (revoked)      // 撤销立即生效
/// 2. status == SUSPENDED → 拒绝 (suspended)    // 挂起立即生效
/// 3. status == EXPIRED   → 拒绝 (expired)      // 云端已标过期
/// 4. status 非 ACTIVE    → 拒绝 (revoked)      // 未知状态 fail-closed 兜底
/// 5. now > validUntil    → 拒绝 (expired)      // 有效期保障 (PRD 模块五 / RS-007-34)
/// 6. now < validFrom     → 拒绝 (notYetValid)  // 未到生效时间
/// 7. now - lastSyncAt > 离线宽限期 → 拒绝 (staleCache) // 限制撤销后无限期离线使用
/// 8. 全部通过            → 允许
///
/// 边界约定:
/// - validUntil == 0 视为永久有效（与后端语义一致: 0 表示未设上限）
/// - lastSyncAt == 0（无缓存历史）跳过宽限期检查, 由状态/有效期规则兜底
///
/// 参考: docs/sdk/OFFLINE-FALLBACK-DESIGN.md
public enum YDKOfflineAuthorizer {

    /// 默认离线宽限期: 7 天（毫秒语义由调用方换算）
    public static let defaultMaxOfflineGrace: TimeInterval = 7 * 24 * 3600

    /// 对单把钥匙做离线授权裁决（纯函数, 无副作用）
    ///
    /// - Parameters:
    ///   - key: 本地缓存的钥匙
    ///   - now: 当前时间
    ///   - lastSyncAtMillis: 缓存最近一次成功同步时间戳（毫秒）
    ///   - maxOfflineGrace: 允许的最大离线时长（秒）
    /// - Returns: 裁决结果（allowed + 拒绝原因）
    public static func authorize(
        key: YDKKey,
        now: Date,
        lastSyncAtMillis: Int64,
        maxOfflineGrace: TimeInterval = defaultMaxOfflineGrace
    ) -> YDKOfflineAuthorization {
        let nowMillis = Int64(now.timeIntervalSince1970 * 1000)

        // 1-4. 状态裁决（fail-closed: 仅 ACTIVE 放行）
        switch key.status {
        case "ACTIVE":
            break
        case "REVOKED":
            return YDKOfflineAuthorization(allowed: false, reason: .revoked)
        case "SUSPENDED":
            return YDKOfflineAuthorization(allowed: false, reason: .suspended)
        case "EXPIRED":
            return YDKOfflineAuthorization(allowed: false, reason: .expired)
        default:
            // 未知状态: fail-closed, 按撤销处理
            return YDKOfflineAuthorization(allowed: false, reason: .revoked)
        }

        // 5. 有效期上限（validUntil == 0 表示永久有效）
        if key.validUntil > 0 && nowMillis > key.validUntil {
            return YDKOfflineAuthorization(allowed: false, reason: .expired)
        }

        // 6. 生效时间
        if key.validFrom > 0 && nowMillis < key.validFrom {
            return YDKOfflineAuthorization(allowed: false, reason: .notYetValid)
        }

        // 7. 离线宽限期（lastSyncAt == 0 跳过, 由上述规则兜底）
        if lastSyncAtMillis > 0 {
            let graceMillis = Int64(maxOfflineGrace * 1000)
            if nowMillis - lastSyncAtMillis > graceMillis {
                return YDKOfflineAuthorization(allowed: false, reason: .staleCache)
            }
        }

        // 8. 全部通过
        return YDKOfflineAuthorization(allowed: true, reason: nil)
    }
}
