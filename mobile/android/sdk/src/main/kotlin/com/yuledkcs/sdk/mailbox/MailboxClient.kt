package com.yuledkcs.sdk.mailbox

import com.google.gson.Gson
import com.google.gson.GsonBuilder
import com.google.gson.JsonDeserializationContext
import com.google.gson.JsonDeserializer
import com.google.gson.JsonElement
import com.google.gson.annotations.SerializedName
import com.yuledkcs.sdk.hub.YDKError
import java.lang.reflect.Type
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import java.util.Base64
import java.util.UUID
import java.util.concurrent.TimeUnit

// ─── 数据模型 ──────────────────────────────────────────────

/**
 * Mailbox 信息（从 Sharing URL 解析）
 */
data class MailboxInfo(
    val mailboxId: String,
    val secret: String
)

/**
 * 邮箱更新操作类型
 */
enum class MailboxDataType(val value: Int) {
    KEY_CREATION(1),
    KEY_SIGNING(2),
    IMPORT(3),
    SENDER_CANCEL(4),
    RECEIVER_CANCEL(5);

    companion object {
        fun fromValue(v: Int) = entries.firstOrNull { it.value == v }
    }
}

data class MailboxCreateResult(
    @SerializedName("mailboxId") val mailboxId: String? = null,
    @SerializedName("sharingUrl") val sharingUrl: String? = null,
    @SerializedName("expiresAt") val expiresAt: Long? = null
)

data class MailboxDisplayInfo(
    @SerializedName("displayInfo") val displayInfo: ByteArray? = null,
    @SerializedName("version") val version: Long? = null
)

data class MailboxContent(
    @SerializedName("payload") val payload: ByteArray? = null,
    @SerializedName("version") val version: Long? = null,
    @SerializedName("errorCode") val errorCode: String? = null,
    @SerializedName("errorMsg") val errorMsg: String? = null
)

data class MailboxUpdateResult(
    @SerializedName("status") val status: String? = null,
    @SerializedName("version") val version: Long? = null,
    @SerializedName("errorCode") val errorCode: String? = null,
    @SerializedName("errorMsg") val errorMsg: String? = null
)

data class MailboxDeleteResult(
    @SerializedName("success") val success: Boolean? = null,
    @SerializedName("errorCode") val errorCode: String? = null
)

data class MailboxRelinquishResult(
    @SerializedName("success") val success: Boolean? = null,
    @SerializedName("errorCode") val errorCode: String? = null,
    @SerializedName("errorMsg") val errorMsg: String? = null
)

// ─── MailboxClient ──────────────────────────────────────────

/**
 * CCC 协议 Mailbox 客户端
 *
 * 通过 HTTP/JSON 调用 Hub REST Gateway 的公开 Mailbox API。
 * 无需 JWT token — 安全由 mailbox_id 随机性和 E2E 加密保障。
 */
class MailboxClient(
    hubEndpoint: String,
    port: Int = 8080,
    // 测试缝: 可注入 OkHttpClient（MockWebServer wire 级测试用）；默认行为与原来一致
    private val client: OkHttpClient = OkHttpClient.Builder()
        .connectTimeout(10, TimeUnit.SECONDS)
        .readTimeout(30, TimeUnit.SECONDS)
        .build()
) {
    private val baseURL = "https://$hubEndpoint:$port/api/v1/mailbox"
    private val gson = GsonBuilder()
        .registerTypeAdapter(ByteArray::class.java, MailboxByteArrayDeserializer)
        .create()

    companion object {
        /**
         * 解析 Sharing URL，提取 mailbox_id 和 secret
         *
         * URL 格式: `https://host/api/v1/mailbox/{mailbox_id}#{secret}`
         * 或: `https://host/api/v1/mailbox/{mailbox_id}#secret={secret}`
         */
        fun parseSharingURL(urlString: String): MailboxInfo? {
            return try {
                val url = java.net.URL(urlString)
                val path = url.path
                val parts = path.split("/")
                val idx = parts.indexOfLast { it == "mailbox" }
                if (idx < 0 || idx + 1 >= parts.size) return null
                val mailboxId = parts[idx + 1]

                val fragment = url.ref ?: ""
                val secret = when {
                    fragment.startsWith("secret=") -> fragment.removePrefix("secret=")
                    else -> fragment
                }

                MailboxInfo(mailboxId, secret)
            } catch (_: Exception) { null }
        }
    }

    // ─── Mailbox 操作 ──────────────────────────────────────

    /** 创建邮箱（发送方） */
    suspend fun createMailbox(
        payload: ByteArray,
        displayInfo: ByteArray? = null,
        senderVendor: String,
        senderDeviceId: String,
        expirationSeconds: Long = 86400,
        maxUpdates: Int = 10,
        notificationToken: String? = null,
        deviceAttestation: ByteArray? = null
    ): MailboxCreateResult = request("POST", "", mapOf(
        "payload" to Base64.getEncoder().encodeToString(payload),
        "displayInfo" to (displayInfo?.let { Base64.getEncoder().encodeToString(it) } ?: ""),
        "senderVendor" to senderVendor,
        "senderDeviceId" to senderDeviceId,
        "expirationSeconds" to expirationSeconds,
        "maxUpdates" to maxUpdates,
        "notificationToken" to (notificationToken ?: ""),
        "deviceAttestation" to (deviceAttestation?.let { Base64.getEncoder().encodeToString(it) } ?: ""),
        "traceId" to UUID.randomUUID().toString()
    ))

    /** 读取展示信息（接收方） */
    suspend fun readDisplayInfo(mailboxId: String): MailboxDisplayInfo =
        request("GET", "/$mailboxId/display")

    /** 读取加密内容 */
    suspend fun readSecureContent(mailboxId: String): MailboxContent =
        request("GET", "/$mailboxId/content")

    /** 更新邮箱（KeySigning / Import / Cancel） */
    suspend fun updateMailbox(
        mailboxId: String,
        dataType: MailboxDataType,
        payload: ByteArray,
        notificationToken: String? = null,
        updaterDeviceId: String? = null
    ): MailboxUpdateResult = request("PUT", "/$mailboxId", mapOf(
        "payload" to Base64.getEncoder().encodeToString(payload),
        "sharingDataType" to dataType.value,
        "notificationToken" to (notificationToken ?: ""),
        "updaterDeviceId" to (updaterDeviceId ?: ""),
        "traceId" to UUID.randomUUID().toString()
    ))

    /** 删除邮箱 */
    suspend fun deleteMailbox(
        mailboxId: String,
        reason: String = "completed",
        deleterDeviceId: String? = null
    ): MailboxDeleteResult = request("DELETE", "/$mailboxId", mapOf(
        "reason" to reason,
        "deleterDeviceId" to (deleterDeviceId ?: ""),
        "traceId" to UUID.randomUUID().toString()
    ))

    /** 转移邮箱到另一设备 */
    suspend fun relinquishMailbox(
        mailboxId: String,
        fromDeviceId: String,
        toDeviceId: String
    ): MailboxRelinquishResult = request("POST", "/$mailboxId/relinquish", mapOf(
        "fromDeviceId" to fromDeviceId,
        "toDeviceId" to toDeviceId,
        "traceId" to UUID.randomUUID().toString()
    ))

    // ─── 内部 HTTP 请求 ────────────────────────────────────

    private suspend inline fun <reified T> request(
        method: String,
        path: String,
        body: Any? = null
    ): T = withContext(Dispatchers.IO) {
        val url = "$baseURL$path"
        val bodyJson = body?.let { gson.toJson(it) }
        val requestBody = bodyJson?.toRequestBody("application/json".toMediaType())

        val request = Request.Builder()
            .url(url)
            .method(method, requestBody)
            .addHeader("Accept", "application/json")
            .build()

        val response = client.newCall(request).execute()
        val responseBody = response.body?.string() ?: ""

        if (!response.isSuccessful) {
            val err = try { gson.fromJson(responseBody, MailboxErrorResponse::class.java) }
                catch (_: Exception) { null }
            throw when {
                err?.code != null -> YDKError.HubError(err.code, err.message ?: "")
                err?.error != null -> YDKError.HubError(err.error, err.message ?: "")
                else -> YDKError.HttpError(response.code)
            }
        }

        gson.fromJson(responseBody, T::class.java)
    }
}

data class MailboxErrorResponse(
    val error: String? = null,
    val message: String? = null,
    val code: String? = null
)

/**
 * ByteArray 反序列化器 — 对齐 wire 契约（B4.2）
 *
 * Hub REST Gateway 用 gin/encoding-json 序列化 proto 的 bytes 字段为 **base64 字符串**，
 * 而 Gson 默认把 byte[] 当 JSON 数字数组解析，会导致 readDisplayInfo/readSecureContent
 * 在服务端返回非空 payload 时解析失败。此处兼容三种形态:
 *   - base64 字符串（服务端实际形态）: `"payload":"5L2g5aW9"`
 *   - JSON 数字数组（Gson 默认形态）:  `"payload":[1,2,3]`
 *   - null → null
 */
private object MailboxByteArrayDeserializer : JsonDeserializer<ByteArray> {
    override fun deserialize(
        json: JsonElement,
        typeOfT: Type,
        context: JsonDeserializationContext
    ): ByteArray? = when {
        json.isJsonNull -> null
        json.isJsonPrimitive && json.asJsonPrimitive.isString ->
            Base64.getDecoder().decode(json.asString)
        json.isJsonArray -> {
            val arr = json.asJsonArray
            ByteArray(arr.size()) { arr[it].asInt.toByte() }
        }
        else -> null
    }
}
