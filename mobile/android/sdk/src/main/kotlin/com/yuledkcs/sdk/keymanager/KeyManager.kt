package com.yuledkcs.sdk.keymanager

import com.yuledkcs.sdk.hub.HubClient
import com.yuledkcs.sdk.hub.listKeys
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.io.File

/**
 * yuleDKCS 钥匙状态管理器
 *
 * 职责:
 * 1. 本地钥匙缓存（JSON 文件持久化）
 * 2. 手动/定时同步 Hub 云端钥匙列表
 * 3. 差异检测 → StateFlow 通知 App
 * 4. Push 触发增量同步
 * 5. 离线访问（无网返回缓存）
 *
 * 用法:
 * ```kotlin
 * val keyManager = KeyManager(hubClient, cacheDir)
 * val localKeys = keyManager.getLocalKeys()  // 离线可用
 * keyManager.syncFromHub()                   // 手动同步
 * ```
 */
class KeyManager(
    private val hubClient: HubClient,
    cacheDir: File,
    private val scope: CoroutineScope = CoroutineScope(Dispatchers.Default + SupervisorJob()),
    enableLogging: Boolean = false
) {
    private val cache = KeyCache.create(cacheDir)
    private val logger = Logger(enableLogging)

    // ─── 可观察状态 ──────────────────────────────────────

    /** 同步状态 */
    sealed class SyncState {
        object Idle : SyncState()
        object Syncing : SyncState()
        data class Success(val result: SyncResult) : SyncState()
        data class Failed(val error: Throwable) : SyncState()
    }

    private val _syncState = MutableStateFlow<SyncState>(SyncState.Idle)
    val syncState: StateFlow<SyncState> = _syncState.asStateFlow()

    /** 钥匙列表（可观察 Flow） */
    private val _keys = MutableStateFlow<List<YDKKey>>(emptyList())
    val keys: StateFlow<List<YDKKey>> = _keys.asStateFlow()

    // 自动同步定时器
    private var autoSyncJob: Job? = null

    init {
        // 初始化时加载缓存
        _keys.value = cache.getLocalKeys()
    }

    // ─── 公开接口 ────────────────────────────────────────

    /** 获取本地缓存的钥匙列表（无网可用） */
    fun getLocalKeys(): List<YDKKey> = cache.getLocalKeys()

    /** 获取单把钥匙 */
    fun getKey(keyId: String, preferCache: Boolean = true): YDKKey? {
        if (preferCache) {
            return cache.getLocalKeys().find { it.keyId == keyId }
        }
        return null
    }

    /** 手动触发同步 */
    suspend fun syncFromHub(): SyncResult {
        _syncState.value = SyncState.Syncing
        logger.log("Sync: starting...")

        return try {
            // 1. 获取云端数据
            val cloudKeys = hubClient.listKeys()

            // 2. 读取本地缓存
            val localKeys = cache.getLocalKeys()
            val localIndex = localKeys.associateBy { it.keyId }
            val cloudIndex = cloudKeys.associateBy { it.keyId }

            // 3. 差异检测
            val added = mutableListOf<YDKKey>()
            val updated = mutableListOf<YDKKey>()
            var unchanged = 0

            for (cloudKey in cloudKeys) {
                val localKey = localIndex[cloudKey.keyId]
                if (localKey != null) {
                    if (cloudKey.status != localKey.status ||
                        cloudKey.validUntil != localKey.validUntil
                    ) {
                        updated.add(cloudKey)
                    } else {
                        unchanged++
                    }
                } else {
                    added.add(cloudKey)
                }
            }

            val removed = localKeys.filter { cloudIndex[it.keyId] == null }

            val result = SyncResult(
                added = added,
                updated = updated,
                removed = removed,
                unchanged = unchanged
            )

            // 4. 更新缓存
            cache.write(keys = cloudKeys)
            _keys.value = cloudKeys

            // 5. 通知
            if (result.hasChanges) {
                logger.log("Sync: ${added.size} added, ${updated.size} updated, ${removed.size} removed, $unchanged unchanged")
            } else {
                logger.log("Sync: no changes ($unchanged keys)")
            }

            _syncState.value = SyncState.Success(result)
            result

        } catch (e: Exception) {
            logger.log("Sync: failed — ${e.message}")
            _syncState.value = SyncState.Failed(e)
            throw e
        }
    }

    /** 处理 Push 通知 → 增量同步 */
    suspend fun handleKeyStatusPush(keyId: String): Boolean {
        logger.log("Push: triggered for key $keyId")
        val result = syncFromHub()
        return result.hasChanges
    }

    /** 启动自动同步 */
    fun startAutoSync(intervalMs: Long = 5 * 60 * 1000) {
        stopAutoSync()
        autoSyncJob = scope.launch {
            while (isActive) {
                delay(intervalMs)
                try {
                    syncFromHub()
                } catch (_: Exception) {
                    // 自动同步不抛给调用方
                }
            }
        }
        logger.log("AutoSync: started every ${intervalMs / 1000}s")
    }

    /** 停止自动同步 */
    fun stopAutoSync() {
        autoSyncJob?.cancel()
        autoSyncJob = null
        logger.log("AutoSync: stopped")
    }

    /** 清除本地缓存 */
    fun clearCache() {
        cache.clear()
        _keys.value = emptyList()
        logger.log("Cache: cleared")
    }

    /** 释放资源 */
    fun shutdown() {
        stopAutoSync()
    }

    private class Logger(private val enabled: Boolean) {
        fun log(msg: String) {
            if (enabled) println("[KeyManager] $msg")
        }
    }
}
