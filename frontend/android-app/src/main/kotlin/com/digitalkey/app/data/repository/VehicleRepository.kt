/**
 * DigitalKey App - 车辆Repository
 */
package com.digitalkey.app.data.repository

import com.digitalkey.app.data.model.*
import com.digitalkey.sdk.DigitalKeySdk
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import java.text.SimpleDateFormat
import java.util.*

/**
 * 车辆仓库
 * 负责车辆控制相关的业务逻辑和数据管理
 */
class VehicleRepository {

    private val sdk = DigitalKeySdk.getInstance()
        ?: throw IllegalStateException("SDK未初始化，请先调用 DigitalKeySdk.init()")

    private val vehicleController = sdk.vehicleController

    // 模拟车辆在线状态
    private var isVehicleConnected = true

    /**
     * 获取所有车辆列表
     */
    suspend fun getVehicles(): Result<List<VehicleModel>> = withContext(Dispatchers.IO) {
        try {
            delay(400)
            val vehicles = listOf(
                VehicleModel(
                    id = "v001",
                    brand = "特斯拉",
                    model = "Model 3",
                    year = 2024,
                    plate = "京A·12345",
                    vin = "LVZZK58A9MA123456",
                    color = "珍珠白",
                    imageUrl = null,
                    isConnected = isVehicleConnected,
                    lastSeenAt = "2026-05-06T10:00:00Z",
                    location = VehicleLocation(
                        latitude = 39.9042,
                        longitude = 116.4074,
                        address = "北京市朝阳区建国路",
                        timestamp = "2026-05-06T10:00:00Z"
                    )
                ),
                VehicleModel(
                    id = "v002",
                    brand = "比亚迪",
                    model = "汉 EV",
                    year = 2025,
                    plate = "沪B·67890",
                    vin = "LVZZK58A9MB654321",
                    color = "玄空灰",
                    imageUrl = null,
                    isConnected = false,
                    lastSeenAt = "2026-05-05T18:30:00Z",
                    location = VehicleLocation(
                        latitude = 31.2304,
                        longitude = 121.4737,
                        address = "上海市浦东新区",
                        timestamp = "2026-05-05T18:30:00Z"
                    )
                )
            )
            Result.success(vehicles)
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    /**
     * 根据ID获取车辆详情
     */
    suspend fun getVehicleById(vehicleId: String): Result<VehicleModel> = withContext(Dispatchers.IO) {
        try {
            val vehicles = getVehicles().getOrThrow()
            val vehicle = vehicles.find { it.id == vehicleId }
            if (vehicle != null) {
                Result.success(vehicle)
            } else {
                Result.failure(NoSuchElementException("车辆不存在: $vehicleId"))
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    /**
     * 发送车辆控制命令
     */
    suspend fun sendCommand(
        vehicleId: String,
        command: VehicleCommand
    ): Result<VehicleControlResult> = withContext(Dispatchers.IO) {
        try {
            val vehicle = getVehicleById(vehicleId).getOrThrow()

            if (!vehicle.isConnected) {
                return@withContext Result.success(
                    VehicleControlResult(
                        commandId = UUID.randomUUID().toString(),
                        command = command,
                        status = ControlStatus.OFFLINE,
                        message = "车辆离线，无法执行命令",
                        vehicleId = vehicleId,
                        timestamp = getCurrentTimestamp()
                    )
                )
            }

            // 模拟命令发送延迟
            delay(1000 + (Math.random() * 500).toLong())

            // 模拟95%成功率
            val isSuccess = Math.random() > 0.05

            val result = VehicleControlResult(
                commandId = UUID.randomUUID().toString(),
                command = command,
                status = if (isSuccess) ControlStatus.SUCCESS else ControlStatus.FAILED,
                message = getCommandMessage(command, isSuccess),
                vehicleId = vehicleId,
                timestamp = getCurrentTimestamp()
            )
            Result.success(result)
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    /**
     * 获取操作历史
     */
    suspend fun getHistory(keyId: String? = null, limit: Int = 50): Result<List<HistoryRecord>> = withContext(Dispatchers.IO) {
        try {
            delay(600)
            val now = System.currentTimeMillis()
            val history = listOf(
                HistoryRecord(
                    id = UUID.randomUUID().toString(),
                    keyId = keyId ?: "k001",
                    vehicleId = "v001",
                    vehicleName = "特斯拉 Model 3",
                    command = VehicleCommand.UNLOCK,
                    status = ControlStatus.SUCCESS,
                    message = "解锁成功",
                    location = VehicleLocation(39.9042, 116.4074, "北京市朝阳区"),
                    timestamp = Date(now - 3600000).toISOString()
                ),
                HistoryRecord(
                    id = UUID.randomUUID().toString(),
                    keyId = keyId ?: "k001",
                    vehicleId = "v001",
                    vehicleName = "特斯拉 Model 3",
                    command = VehicleCommand.START_ENGINE,
                    status = ControlStatus.SUCCESS,
                    message = "引擎已启动，空调已开启",
                    location = VehicleLocation(39.9042, 116.4074, "北京市朝阳区"),
                    timestamp = Date(now - 7200000).toISOString()
                ),
                HistoryRecord(
                    id = UUID.randomUUID().toString(),
                    keyId = keyId ?: "k001",
                    vehicleId = "v001",
                    vehicleName = "特斯拉 Model 3",
                    command = VehicleCommand.LOCK,
                    status = ControlStatus.SUCCESS,
                    message = "锁车成功",
                    location = VehicleLocation(39.9042, 116.4074, "北京市朝阳区"),
                    timestamp = Date(now - 86400000).toISOString()
                ),
                HistoryRecord(
                    id = UUID.randomUUID().toString(),
                    keyId = keyId ?: "k001",
                    vehicleId = "v001",
                    vehicleName = "特斯拉 Model 3",
                    command = VehicleCommand.FIND_CAR,
                    status = ControlStatus.SUCCESS,
                    message = "车辆已鸣笛闪灯",
                    location = null,
                    timestamp = Date(now - 172800000).toISOString()
                ),
                HistoryRecord(
                    id = UUID.randomUUID().toString(),
                    keyId = keyId ?: "k001",
                    vehicleId = "v001",
                    vehicleName = "特斯拉 Model 3",
                    command = VehicleCommand.OPEN_TRUNK,
                    status = ControlStatus.FAILED,
                    message = "操作失败: 后备箱传感器异常",
                    location = null,
                    timestamp = Date(now - 259200000).toISOString()
                )
            )
            Result.success(history.take(limit))
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    /**
     * 获取命令描述
     */
    private fun getCommandMessage(command: VehicleCommand, success: Boolean): String {
        if (!success) return "操作失败，请稍后重试"
        return when (command) {
            VehicleCommand.LOCK -> "锁车成功"
            VehicleCommand.UNLOCK -> "解锁成功"
            VehicleCommand.START_ENGINE -> "引擎已启动"
            VehicleCommand.STOP_ENGINE -> "引擎已熄火"
            VehicleCommand.OPEN_TRUNK -> "后备箱已打开"
            VehicleCommand.CLOSE_TRUNK -> "后备箱已关闭"
            VehicleCommand.OPEN_HOOD -> "前备箱已打开"
            VehicleCommand.CLOSE_HOOD -> "前备箱已关闭"
            VehicleCommand.START_CLIMATE -> "空调已开启"
            VehicleCommand.STOP_CLIMATE -> "空调已关闭"
            VehicleCommand.SET_CLIMATE_TEMP -> "温度已调节"
            VehicleCommand.OPEN_WINDOWS -> "车窗已下降"
            VehicleCommand.CLOSE_WINDOWS -> "车窗已升起"
            VehicleCommand.FLASH_LIGHTS -> "车灯已闪烁"
            VehicleCommand.HONK_HORN -> "喇叭已响起"
            VehicleCommand.FIND_CAR -> "寻车成功"
            VehicleCommand.REMOTE_START -> "远程启动成功"
        }
    }

    private fun getCurrentTimestamp(): String {
        return SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'", Locale.US).apply {
            timeZone = TimeZone.getTimeZone("UTC")
        }.format(Date())
    }

    private fun Date.toISOString(): String {
        return SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'", Locale.US).apply {
            timeZone = TimeZone.getTimeZone("UTC")
        }.format(this)
    }
}
