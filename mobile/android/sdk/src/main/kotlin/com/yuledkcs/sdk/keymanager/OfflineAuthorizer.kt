package com.yuledkcs.sdk.keymanager

import com.yuledkcs.sdk.hub.YDKKey

/**
 * 离线授权拒绝原因
 */
enum class OfflineDenialReason {
    /** 钥匙已被撤销（REVOKED / 未知状态 fail-closed 兜底） */
    REVOKED,

    /** 钥匙已被挂起（SUSPENDED） */
    SUSPENDED,

    /** 钥匙已过期（云端状态 EXPIRED 或 now > validUntil） */
    EXPIRED,

    /** 钥匙尚未生效（now < validFrom） */
    NOT_YET_VALID,

    /** 缓存过旧（距上次同步超过离线宽限期） */
    STALE_CACHE
}

/**
 * 离线授权裁决结果
 */
data class OfflineAuthorization(
    val allowed: Boolean,
    val reason: OfflineDenialReason? = null
)

/**
 * 离线授权回退机制（方案 A）— 纯本地纯函数裁决器
 *
 * 语义: 当 Hub 云端不可达时, SDK 回退到本地缓存进行离线授权判断。
 * 在 BLE/NFC 离线解锁前调用本裁决器, 回答「这把钥匙现在还能不能用」。
 *
 * 裁决规则（fail-closed, 按顺序短路）:
 * 1. status == REVOKED   → 拒绝 (REVOKED)       // 撤销立即生效
 * 2. status == SUSPENDED → 拒绝 (SUSPENDED)     // 挂起立即生效
 * 3. status == EXPIRED   → 拒绝 (EXPIRED)       // 云端已标过期
 * 4. status 非 ACTIVE    → 拒绝 (REVOKED)       // 未知状态 fail-closed 兜底
 * 5. now > validUntil    → 拒绝 (EXPIRED)       // 有效期保障 (PRD 模块五 / RS-007-34)
 * 6. now < validFrom     → 拒绝 (NOT_YET_VALID) // 未到生效时间
 * 7. now - lastSyncAt > 离线宽限期 → 拒绝 (STALE_CACHE) // 限制撤销后无限期离线使用
 * 8. 全部通过            → 允许
 *
 * 边界约定:
 * - validUntil == 0 视为永久有效（与后端语义一致: 0 表示未设上限）
 * - lastSyncAt == 0（无缓存历史）跳过宽限期检查, 由状态/有效期规则兜底
 *
 * 参考: docs/sdk/OFFLINE-FALLBACK-DESIGN.md
 */
object OfflineAuthorizer {

    /** 默认离线宽限期: 7 天（毫秒） */
    const val DEFAULT_MAX_OFFLINE_GRACE_MILLIS: Long = 7 * 24 * 60 * 60 * 1000L

    /**
     * 对单把钥匙做离线授权裁决（纯函数, 无副作用）
     *
     * @param key 本地缓存的钥匙
     * @param nowMillis 当前时间（毫秒）
     * @param lastSyncAtMillis 缓存最近一次成功同步时间戳（毫秒）
     * @param maxOfflineGraceMillis 允许的最大离线时长（毫秒）
     * @return 裁决结果（allowed + 拒绝原因）
     */
    fun authorize(
        key: YDKKey,
        nowMillis: Long,
        lastSyncAtMillis: Long,
        maxOfflineGraceMillis: Long = DEFAULT_MAX_OFFLINE_GRACE_MILLIS
    ): OfflineAuthorization {
        // 1-4. 状态裁决（fail-closed: 仅 ACTIVE 放行）
        when (key.status) {
            "ACTIVE" -> Unit
            "REVOKED" -> return OfflineAuthorization(allowed = false, reason = OfflineDenialReason.REVOKED)
            "SUSPENDED" -> return OfflineAuthorization(allowed = false, reason = OfflineDenialReason.SUSPENDED)
            "EXPIRED" -> return OfflineAuthorization(allowed = false, reason = OfflineDenialReason.EXPIRED)
            else -> {
                // 未知状态: fail-closed, 按撤销处理
                return OfflineAuthorization(allowed = false, reason = OfflineDenialReason.REVOKED)
            }
        }

        // 5. 有效期上限（validUntil == 0 表示永久有效）
        if (key.validUntil > 0 && nowMillis > key.validUntil) {
            return OfflineAuthorization(allowed = false, reason = OfflineDenialReason.EXPIRED)
        }

        // 6. 生效时间
        if (key.validFrom > 0 && nowMillis < key.validFrom) {
            return OfflineAuthorization(allowed = false, reason = OfflineDenialReason.NOT_YET_VALID)
        }

        // 7. 离线宽限期（lastSyncAt == 0 跳过, 由上述规则兜底）
        if (lastSyncAtMillis > 0 && nowMillis - lastSyncAtMillis > maxOfflineGraceMillis) {
            return OfflineAuthorization(allowed = false, reason = OfflineDenialReason.STALE_CACHE)
        }

        // 8. 全部通过
        return OfflineAuthorization(allowed = true)
    }
}
