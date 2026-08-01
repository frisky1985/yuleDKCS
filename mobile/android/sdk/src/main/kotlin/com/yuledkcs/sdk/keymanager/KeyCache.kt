package com.yuledkcs.sdk.keymanager

import com.google.gson.Gson
import com.google.gson.annotations.SerializedName
import com.yuledkcs.sdk.hub.YDKKey
import java.io.File

/**
 * 本地钥匙缓存 — JSON 文件持久化
 *
 * 缓存文件位置: [cacheDir]/yuledkcs_keys_cache.json
 */
class KeyCache private constructor(
    private val cacheFile: File,
    private val gson: Gson = Gson()
) {
    data class CacheData(
        val version: Int = 1,
        @SerializedName("lastSyncAt") val lastSyncAt: Long = 0,
        val keys: List<YDKKey> = emptyList()
    )

    companion object {
        fun create(cacheDir: File): KeyCache {
            cacheDir.mkdirs()
            return KeyCache(File(cacheDir, "yuledkcs_keys_cache.json"))
        }
    }

    /** 读取缓存 */
    fun read(): CacheData {
        return try {
            if (!cacheFile.exists()) return CacheData()
            val text = cacheFile.readText()
            if (text.isBlank()) return CacheData()
            gson.fromJson(text, CacheData::class.java) ?: CacheData()
        } catch (e: Exception) {
            CacheData()
        }
    }

    /** 获取本地钥匙列表 */
    fun getLocalKeys(): List<YDKKey> = read().keys

    /** 最近一次成功同步时间戳（毫秒）; 无缓存时返回 0 */
    fun lastSyncAtMillis(): Long = read().lastSyncAt

    /** 写入缓存（覆盖） */
    fun write(keys: List<YDKKey>) {
        val data = CacheData(
            version = 1,
            lastSyncAt = System.currentTimeMillis(),
            keys = keys
        )
        try {
            cacheFile.writeText(gson.toJson(data))
        } catch (_: Exception) {
            // 写入失败 — 静默处理
        }
    }

    /** 清空缓存 */
    fun clear() {
        cacheFile.delete()
    }
}
