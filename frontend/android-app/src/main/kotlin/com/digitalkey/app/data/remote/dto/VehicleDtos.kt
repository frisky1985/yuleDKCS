/**
 * DigitalKey App - 车辆控制 DTO
 */
package com.digitalkey.app.data.remote.dto

import com.google.gson.annotations.SerializedName

// ── 发送车辆控制指令 ──

data class SendCommandRequest(
    val action: String,
    @SerializedName("key_id")
    val keyId: String,
    val params: Map<String, Any>? = null,
    val source: Int? = null,
    @SerializedName("trace_id")
    val traceId: String? = null
)

data class SendCommandResponse(
    @SerializedName("cmd_id")
    val cmdId: String,
    @SerializedName("result_code")
    val resultCode: Int = 0,
    @SerializedName("error_msg")
    val errorMsg: String? = null
)

// ── SSE 车辆状态（用于解析 SSE 事件数据） ──

data class VehicleStatusEvent(
    @SerializedName("vehicle_id")
    val vehicleId: String,
    @SerializedName("lock_status")
    val lockStatus: Int = 0,
    @SerializedName("engine_status")
    val engineStatus: Int = 0,
    @SerializedName("door_status")
    val doorStatus: Int = 0,
    @SerializedName("window_status")
    val windowStatus: Int = 0,
    @SerializedName("battery_pct")
    val batteryPct: Int = 0,
    @SerializedName("interior_temp")
    val interiorTemp: Int = 0,
    @SerializedName("alarm_status")
    val alarmStatus: Int = 0,
    val latitude: Double = 0.0,
    val longitude: Double = 0.0,
    val timestamp: Long = 0L
)
