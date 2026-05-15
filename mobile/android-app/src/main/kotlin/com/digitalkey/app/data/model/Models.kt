/**
 * DigitalKey App - 数据模型
 */
package com.digitalkey.app.data.model

import kotlinx.serialization.Serializable

/**
 * 钥匙数据模型
 *
 * @property id          钥匙唯一标识 (UUID)
 * @property name        钥匙名称/昵称
 * @property vehicleId   关联的车辆ID
 * @property vehicleName 车辆名称
 * @property vehiclePlate 车牌号
 * @property vehicleImageUrl 车辆图片URL
 * @property permissions 钥匙权限列表
 * @property status      钥匙状态 (active/inactive/expired)
 * @property issuerId    钥匙发放者ID
 * @property issuerName  钥匙发放者名称
 * @property issuedAt    发放时间 (ISO 8601)
 * @property expiresAt   过期时间 (可选, ISO 8601)
 * @property createdAt   创建时间
 * @property updatedAt    更新时间
 */
@Serializable
data class KeyModel(
    val id: String,
    val name: String,
    val vehicleId: String,
    val vehicleName: String,
    val vehiclePlate: String,
    val vehicleImageUrl: String? = null,
    val permissions: List<Permission> = emptyList(),
    val status: KeyStatus = KeyStatus.ACTIVE,
    val issuerId: String,
    val issuerName: String,
    val issuedAt: String,
    val expiresAt: String? = null,
    val createdAt: String? = null,
    val updatedAt: String? = null
)

/**
 * 钥匙状态枚举
 */
@Serializable
enum class KeyStatus {
    ACTIVE,      // 正常可用
    INACTIVE,    // 已停用
    EXPIRED,     // 已过期
    PENDING,     // 等待激活
    REVOKED      // 已撤销
}

/**
 * 钥匙权限
 *
 * @property type    权限类型
 * @property granted  是否已授权
 * @property expiresAt 权限过期时间 (可选)
 */
@Serializable
data class Permission(
    val type: PermissionType,
    val granted: Boolean = true,
    val expiresAt: String? = null
)

/**
 * 权限类型枚举
 */
@Serializable
enum class PermissionType {
    UNLOCK,       // 解锁车辆
    LOCK,         // 锁车
    START_ENGINE,  // 启动引擎
    STOP_ENGINE,   // 熄火
    OPEN_TRUNK,    // 打开后备箱
    OPEN_HOOD,     // 打开发动机盖
    CLIMATE_CONTROL, // 空调控制
    WINDOW_CONTROL,  // 车窗控制
    VALET_MODE,   // 代客泊车模式
    SPEED_LIMIT,  // 速度限制
    GEO_FENCE,    // 地理围栏
    REMOTE_START, // 远程启动
    CAR_FIND,     // 寻车
    SHARE,        // 分享钥匙
    MANAGE        // 管理钥匙
}

/**
 * 车辆数据模型
 *
 * @property id          车辆ID
 * @property brand       品牌
 * @property model       车型
 * @property year        年份
 * @property plate       车牌号
 * @property vin         车架号
 * @property color       颜色
 * @property imageUrl    车辆图片
 * @property isConnected 是否在线
 * @property lastSeenAt  最后在线时间
 * @property location    当前位置 (可选)
 */
@Serializable
data class VehicleModel(
    val id: String,
    val brand: String,
    val model: String,
    val year: Int,
    val plate: String,
    val vin: String,
    val color: String,
    val imageUrl: String? = null,
    val isConnected: Boolean = false,
    val lastSeenAt: String? = null,
    val location: VehicleLocation? = null
)

/**
 * 车辆位置
 */
@Serializable
data class VehicleLocation(
    val latitude: Double,
    val longitude: Double,
    val address: String? = null,
    val timestamp: String? = null
)

/**
 * 车辆控制命令结果
 */
@Serializable
data class VehicleControlResult(
    val commandId: String,
    val command: VehicleCommand,
    val status: ControlStatus,
    val message: String,
    val vehicleId: String,
    val timestamp: String
)

/**
 * 车辆控制命令枚举
 */
@Serializable
enum class VehicleCommand {
    LOCK,
    UNLOCK,
    START_ENGINE,
    STOP_ENGINE,
    OPEN_TRUNK,
    CLOSE_TRUNK,
    OPEN_HOOD,
    CLOSE_HOOD,
    START_CLIMATE,
    STOP_CLIMATE,
    SET_CLIMATE_TEMP,
    OPEN_WINDOWS,
    CLOSE_WINDOWS,
    FLASH_LIGHTS,
    HONK_HORN,
    FIND_CAR,
    REMOTE_START
}

/**
 * 控制命令状态
 */
@Serializable
enum class ControlStatus {
    SUCCESS,
    FAILED,
    PENDING,
    TIMEOUT,
    OFFLINE
}

/**
 * 操作历史记录
 *
 * @property id          记录ID
 * @property keyId       关联钥匙ID
 * @property vehicleId   关联车辆ID
 * @property vehicleName  车辆名称
 * @property command     执行的操作
 * @property status      操作结果
 * @property message     操作消息
 * @property location    操作时的位置 (可选)
 * @property timestamp   操作时间
 */
@Serializable
data class HistoryRecord(
    val id: String,
    val keyId: String,
    val vehicleId: String,
    val vehicleName: String,
    val command: VehicleCommand,
    val status: ControlStatus,
    val message: String,
    val location: VehicleLocation? = null,
    val timestamp: String
)

/**
 * 钥匙分享请求
 *
 * @property keyId       要分享的钥匙ID
 * @property recipientId 接收者ID
 * @property recipientName 接收者名称
 * @property permissions 要授予的权限列表
 * @property expiresAt   分享过期时间 (可选)
 * @property message     附言 (可选)
 */
@Serializable
data class ShareKeyRequest(
    val keyId: String,
    val recipientId: String,
    val recipientName: String,
    val permissions: List<PermissionType>,
    val expiresAt: String? = null,
    val message: String? = null
)

/**
 * 分享结果
 */
@Serializable
data class ShareKeyResult(
    val shareId: String,
    val status: ShareStatus,
    val message: String,
    val shareCode: String? = null,
    val shareUrl: String? = null,
    val expiresAt: String? = null
)

/**
 * 分享状态
 */
@Serializable
enum class ShareStatus {
    SUCCESS,
    PENDING,
    FAILED,
    CANCELLED,
    EXPIRED
}

/**
 * 用户信息
 */
@Serializable
data class UserInfo(
    val id: String,
    val name: String,
    val phone: String,
    val email: String? = null,
    val avatarUrl: String? = null,
    val userType: UserType = UserType.OWNER
)

/**
 * 用户类型
 */
@Serializable
enum class UserType {
    OWNER,     // 车主
    FAMILY,    // 家人
    FRIEND,    // 朋友
    VALET,     // 代客泊车
    GUEST      // 访客
}

/**
 * 添加钥匙请求
 */
@Serializable
data class AddKeyRequest(
    val activationCode: String,
    val keyName: String,
    val vehicleId: String? = null
)

/**
 * SDK初始化参数 (从上层传入)
 */
data class AppConfig(
    val serverUrl: String = "https://api.digitalkey.cn",
    val appId: String = "com.digitalkey.app",
    val enableLog: Boolean = true
)

/**
 * UI状态包装类 (用于ViewModel)
 */
sealed class UiState<out T> {
    object Loading : UiState<Nothing>()
    data class Success<T>(val data: T) : UiState<T>()
    data class Error(val message: String, val code: Int? = null) : UiState<Nothing>()
    object Empty : UiState<Nothing>()
}
