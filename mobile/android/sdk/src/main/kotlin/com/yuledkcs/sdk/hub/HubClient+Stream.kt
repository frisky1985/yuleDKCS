package com.yuledkcs.sdk.hub

import com.google.gson.Gson
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.channels.awaitClose
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.callbackFlow
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import okhttp3.sse.EventSource
import okhttp3.sse.EventSourceListener
import okhttp3.sse.EventSources
import java.util.concurrent.TimeUnit

/**
 * 车辆状态更新（SSE 流）
 */
data class VehicleStatusUpdate(
    val vehicleId: String? = null,
    val lockStatus: Int? = null,
    val engineStatus: Int? = null,
    val batteryPct: Int? = null,
    val latitude: Double? = null,
    val longitude: Double? = null,
    val timestamp: Long? = null
)

/**
 * 通过 SSE 流获取实时车辆状态
 *
 * 返回 Flow<VehicleStatusUpdate>，每次有新事件 emit 一个更新。
 * 连接断开或 SDK 关闭时 flow 自动完成。
 */
fun HubClient.streamStatus(vehicleId: String): Flow<VehicleStatusUpdate> = callbackFlow {
    // 为 SSE 构建独立的 OkHttpClient（长连接，无超时）
    val sseClient = OkHttpClient.Builder()
        .connectTimeout(10, TimeUnit.SECONDS)
        .readTimeout(0, TimeUnit.SECONDS)  // 不超时
        .build()

    val request = Request.Builder()
        .url("$baseURL/vehicles/$vehicleId/status")
        .addHeader("Accept", "text/event-stream")
        .addHeader("X-SDK-Version", Version.CURRENT)
        .addHeader("X-Platform", "android")
        .apply {
            token?.let { addHeader("Authorization", "Bearer $it") }
        }
        .build()

    val gson = Gson()

    val factory = EventSources.createFactory(sseClient)
    val eventSource = factory.newEventSource(request, object : EventSourceListener() {
        override fun onEvent(
            eventSource: EventSource,
            id: String?,
            type: String?,
            data: String
        ) {
            try {
                val update = gson.fromJson(data, VehicleStatusUpdate::class.java)
                trySend(update)
            } catch (_: Exception) {
                // SSE 解析失败，跳过该事件
            }
        }

        override fun onFailure(
            eventSource: EventSource,
            t: Throwable?,
            response: Response?
        ) {
            close(t)
        }
    })

    awaitClose {
        eventSource.cancel()
        sseClient.dispatcher.executorService.shutdown()
    }
}
