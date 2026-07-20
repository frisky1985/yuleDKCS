package com.digitalkey.wallet

/**
 * 数字钥匙钱包 - 核心数据模型
 */
object Models {

    // ── 协议类型 ──
    enum class Protocol {
        CCC_DK3, ICCOA_DK30, ICCOA_DK40, ICCE
    }

    // ── 密钥类型 ──
    enum class KeyType {
        OWNER,    // 车主
        FRIEND,   // 朋友
        SERVICE,  // 服务(代客泊车等)
        TEMPORARY // 临时
    }

    // ── 密钥状态 ──
    enum class KeyStatus {
        ACTIVE, SUSPENDED, REVOKED, EXPIRED
    }

    // ── 权限 ──
    data class AccessLevel(
        val lock: Boolean = false,
        val unlock: Boolean = false,
        val engine: Boolean = false,
        val trunk: Boolean = false,
        val window: Boolean = false,
        val climate: Boolean = false,
        val find: Boolean = false,
        val seat: Boolean = false
    ) {
        fun toBitmask(): Int {
            var mask = 0
            if (lock) mask = mask or 0x01
            if (unlock) mask = mask or 0x02
            if (engine) mask = mask or 0x04
            if (trunk) mask = mask or 0x08
            if (window) mask = mask or 0x10
            if (climate) mask = mask or 0x20
            if (find) mask = mask or 0x40
            if (seat) mask = mask or 0x80
            return mask
        }

        companion object {
            fun fromBitmask(mask: Int): AccessLevel = AccessLevel(
                lock = mask and 0x01 != 0,
                unlock = mask and 0x02 != 0,
                engine = mask and 0x04 != 0,
                trunk = mask and 0x08 != 0,
                window = mask and 0x10 != 0,
                climate = mask and 0x20 != 0,
                find = mask and 0x40 != 0,
                seat = mask and 0x80 != 0
            )
        }
    }

    // ── 数字密钥 ──
    data class DigitalKey(
        val keyId: String,
        val vehicleId: String,
        val vehicleBrand: String,
        val vehicleModel: String,
        val keyType: KeyType,
        val protocol: Protocol,
        val accessLevel: AccessLevel,
        val validFrom: Long,
        val validUntil: Long,
        val status: KeyStatus,
        val isDefault: Boolean = false
    )

    // ── 车辆状态 ──
    data class VehicleStatus(
        val vehicleId: String,
        val lockStatus: Int,       // 0=解锁 1=闭锁
        val engineStatus: Int,     // 0=熄火 1=运行
        val doorStatus: Int,       // 位掩码
        val windowStatus: Int,     // 位掩码
        val batteryPercent: Int,
        val interiorTemp: Float,
        val odometerKm: Int,
        val alarmStatus: Int,
        val latitude: Double?,
        val longitude: Double?,
        val timestamp: Long
    )

    // ── 分享码 ──
    data class ShareCode(
        val shareId: String,
        val shareCode: String,
        val vehicleBrand: String,
        val vehicleModel: String,
        val accessLevel: AccessLevel,
        val validFrom: Long,
        val validUntil: Long,
        val maxUses: Int?
    )

    // ── 控制命令 ──
    enum class ControlAction {
        UNLOCK, LOCK, ENGINE_ON, ENGINE_OFF,
        TRUNK_OPEN, WINDOW_UP, WINDOW_DOWN,
        CLIMATE_ON, CLIMATE_OFF, FIND, HORN
    }
}
