package com.yuledkcs.model

import org.json.JSONObject
import java.util.Date
import java.util.UUID

/**
 * Digital Key model representing a vehicle access key
 */
data class DigitalKey(
    val id: String,
    val keyData: ByteArray,
    val vehicleId: String? = null,
    val protocol: String = "CCC",
    val type: KeyType = KeyType.OWNER,
    var status: String = "active",
    val permissions: List<KeyPermission> = emptyList(),
    val issuedAt: Date = Date(),
    val expiresAt: Date? = null,
    val sharedBy: String? = null,
    val sharedTo: String? = null,
    val useCount: Int = 0,
    val lastUsedAt: Date? = null
) {
    enum class KeyType {
        OWNER, SHARED
    }
    
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (javaClass != other?.javaClass) return false
        other as DigitalKey
        return id == other.id
    }
    
    override fun hashCode(): Int {
        return id.hashCode()
    }
}

/**
 * Key permission model
 */
data class KeyPermission(
    val type: String,
    val enabled: Boolean,
    val constraints: PermissionConstraints? = null
)

data class PermissionConstraints(
    val timeRange: TimeRange? = null,
    val daysOfWeek: List<Int>? = null,
    val maxUses: Int? = null,
    val geofence: GeoFence? = null
)

data class TimeRange(
    val start: String,
    val end: String
)

data class GeoFence(
    val type: String,
    val center: GeoPoint? = null,
    val radius: Double? = null,
    val points: List<GeoPoint>? = null
)

data class GeoPoint(
    val lat: Double,
    val lng: Double
)

/**
 * Permission types
 */
enum class Permission(val displayName: String) {
    UNLOCK("解锁"),
    LOCK("锁车"),
    START_ENGINE("启动引擎"),
    STOP_ENGINE("停止引擎"),
    OPEN_TRUNK("开后备箱"),
    OPEN_WINDOWS("开窗"),
    CLOSE_WINDOWS("关窗"),
    FIND_VEHICLE("寻车")
}

/**
 * Key usage log model
 */
data class KeyUsageLog(
    val id: String,
    val keyId: String,
    val operation: String,
    val status: Status,
    val timestamp: Date,
    val location: Location? = null,
    val deviceInfo: String,
    val failureReason: String? = null
) {
    enum class Status {
        SUCCESS, FAILURE, TIMEOUT
    }
    
    data class Location(
        val lat: Double,
        val lng: Double
    )
    
    fun toJson(): String {
        return JSONObject().apply {
            put("id", id)
            put("key_id", keyId)
            put("operation", operation)
            put("status", status.name)
            put("timestamp", timestamp.time)
            location?.let {
                put("location", JSONObject().apply {
                    put("lat", it.lat)
                    put("lng", it.lng)
                })
            }
            put("device_info", deviceInfo)
            failureReason?.let { put("failure_reason", it) }
        }.toString()
    }
    
    companion object {
        fun fromJson(json: String): KeyUsageLog? {
            return try {
                val obj = JSONObject(json)
                KeyUsageLog(
                    id = obj.getString("id"),
                    keyId = obj.getString("key_id"),
                    operation = obj.getString("operation"),
                    status = Status.valueOf(obj.getString("status")),
                    timestamp = Date(obj.getLong("timestamp")),
                    location = obj.optJSONObject("location")?.let {
                        Location(
                            lat = it.getDouble("lat"),
                            lng = it.getDouble("lng")
                        )
                    },
                    deviceInfo = obj.getString("device_info"),
                    failureReason = obj.optString("failure_reason", null)
                )
            } catch (e: Exception) {
                null
            }
        }
    }
}

/**
 * Vehicle status model
 */
data class VehicleStatus(
    val doorLocked: Boolean,
    val engineRunning: Boolean,
    val trunkOpen: Boolean,
    val windowsOpen: Boolean,
    val batteryLevel: Int, // 0-100
    val fuelLevel: Int, // 0-100
    val alarmActive: Boolean,
    val timestamp: Long
) {
    val isOnline: Boolean
        get() = System.currentTimeMillis() - timestamp < 60000 // Online if updated within 60 seconds
}

/**
 * Command sealed class for type-safe vehicle commands
 */
sealed class Command {
    object Lock : Command()
    object Unlock : Command()
    object StartEngine : Command()
    object StopEngine : Command()
    object TrunkOpen : Command()
    object OpenWindows : Command()
    object CloseWindows : Command()
    object FindVehicle : Command()
    data class Custom(val data: ByteArray) : Command()
    
    override fun toString(): String {
        return when (this) {
            is Lock -> "lock"
            is Unlock -> "unlock"
            is StartEngine -> "start_engine"
            is StopEngine -> "stop_engine"
            is TrunkOpen -> "open_trunk"
            is OpenWindows -> "open_windows"
            is CloseWindows -> "close_windows"
            is FindVehicle -> "find_vehicle"
            is Custom -> "custom"
        }
    }
}

/**
 * Share key request/response
 */
data class ShareKeyRequest(
    val recipient: String,
    val permissions: List<String>,
    val expiresInDays: Int = 7,
    val message: String? = null
)

data class ShareKeyResponse(
    val shareId: String,
    val qrCodeUrl: String,
    val shareLink: String,
    val expiresAt: Date
)

/**
 * Vehicle model
 */
data class Vehicle(
    val id: String,
    val vin: String,
    val brand: String,
    val model: String,
    val year: Int,
    val color: String,
    val plateNumber: String? = null,
    val bleAddress: String? = null
)

/**
 * Connection info for BLE
 */
data class ConnectionInfo(
    val address: String,
    val rssi: Int,
    val name: String?,
    val isBonded: Boolean
)

/**
 * Operation result wrapper
 */
sealed class OperationResult<out T> {
    data class Success<T>(val data: T) : OperationResult<T>()
    data class Error(val exception: Throwable) : OperationResult<Nothing>()
    
    fun isSuccess(): Boolean = this is Success
    fun isError(): Boolean = this is Error
    
    fun getOrNull(): T? = (this as? Success)?.data
    fun exceptionOrNull(): Throwable? = (this as? Error)?.exception
}
