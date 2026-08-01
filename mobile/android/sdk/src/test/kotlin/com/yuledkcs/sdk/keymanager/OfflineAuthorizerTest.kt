package com.yuledkcs.sdk.keymanager

import com.google.gson.Gson
import com.yuledkcs.sdk.hub.HubClient
import com.yuledkcs.sdk.hub.SDKConfig
import com.yuledkcs.sdk.hub.YDKKey
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

/**
 * 离线授权回退机制（方案 A）— Android 单测
 *
 * 覆盖: docs/sdk/OFFLINE-FALLBACK-DESIGN.md §3.1 裁决规则全分支
 *   - 状态裁决: REVOKED / SUSPENDED / EXPIRED / 未知状态 fail-closed
 *   - 有效期窗口: validUntil 过期 / validFrom 未生效 / validUntil==0 永久
 *   - 离线宽限期: 超窗拒绝 / 窗内允许 / lastSyncAt==0 跳过 / 自定义宽限
 *   - KeyManager 入口: 命中缓存 / 未命中返回 null / 撤销与陈旧缓存拒绝
 */
class OfflineAuthorizerTest {

    /** 固定参考时间（毫秒） */
    private val nowMillis = 1_800_000_000_000L

    private fun makeKey(
        keyId: String = "key-1",
        status: String = "ACTIVE",
        validFrom: Long = 0,
        validUntil: Long = 0
    ) = YDKKey(
        keyId = keyId,
        vehicleId = "VH-1",
        validFrom = validFrom,
        validUntil = validUntil,
        status = status
    )

    // ─── 状态裁决 ────────────────────────────────────────

    @Test
    fun activeKeyIsAllowed() {
        val result = OfflineAuthorizer.authorize(makeKey(status = "ACTIVE"), nowMillis, nowMillis)
        assertTrue(result.allowed)
        assertNull(result.reason)
    }

    @Test
    fun revokedKeyIsDenied() {
        val result = OfflineAuthorizer.authorize(makeKey(status = "REVOKED"), nowMillis, nowMillis)
        assertFalse(result.allowed)
        assertEquals(OfflineDenialReason.REVOKED, result.reason)
    }

    @Test
    fun suspendedKeyIsDenied() {
        val result = OfflineAuthorizer.authorize(makeKey(status = "SUSPENDED"), nowMillis, nowMillis)
        assertFalse(result.allowed)
        assertEquals(OfflineDenialReason.SUSPENDED, result.reason)
    }

    @Test
    fun expiredStatusKeyIsDenied() {
        val result = OfflineAuthorizer.authorize(makeKey(status = "EXPIRED"), nowMillis, nowMillis)
        assertFalse(result.allowed)
        assertEquals(OfflineDenialReason.EXPIRED, result.reason)
    }

    @Test
    fun unknownStatusFailsClosed() {
        val result = OfflineAuthorizer.authorize(makeKey(status = "PENDING_WHATEVER"), nowMillis, nowMillis)
        assertFalse("未知状态必须 fail-closed", result.allowed)
        assertEquals(OfflineDenialReason.REVOKED, result.reason)
    }

    // ─── 有效期窗口 ────────────────────────────────────────

    @Test
    fun keyPastValidUntilIsDenied() {
        val key = makeKey(validUntil = nowMillis - 60_000)
        val result = OfflineAuthorizer.authorize(key, nowMillis, nowMillis)
        assertFalse(result.allowed)
        assertEquals(OfflineDenialReason.EXPIRED, result.reason)
    }

    @Test
    fun keyWithinValidityIsAllowed() {
        val key = makeKey(validFrom = nowMillis - 3_600_000, validUntil = nowMillis + 3_600_000)
        val result = OfflineAuthorizer.authorize(key, nowMillis, nowMillis)
        assertTrue(result.allowed)
    }

    @Test
    fun keyBeforeValidFromIsDenied() {
        val key = makeKey(validFrom = nowMillis + 3_600_000, validUntil = 0)
        val result = OfflineAuthorizer.authorize(key, nowMillis, nowMillis)
        assertFalse(result.allowed)
        assertEquals(OfflineDenialReason.NOT_YET_VALID, result.reason)
    }

    @Test
    fun zeroValidUntilMeansPermanent() {
        // validUntil == 0 表示永久有效, 不应被过期规则拒绝
        val result = OfflineAuthorizer.authorize(makeKey(validFrom = 0, validUntil = 0), nowMillis, nowMillis)
        assertTrue(result.allowed)
    }

    // ─── 离线宽限期 ────────────────────────────────────────

    @Test
    fun staleCacheBeyondGraceIsDenied() {
        val lastSync = nowMillis - 8 * 24 * 3_600_000L // 8 天前 > 默认 7 天
        val result = OfflineAuthorizer.authorize(makeKey(), nowMillis, lastSync)
        assertFalse(result.allowed)
        assertEquals(OfflineDenialReason.STALE_CACHE, result.reason)
    }

    @Test
    fun freshCacheWithinGraceIsAllowed() {
        val lastSync = nowMillis - 24 * 3_600_000L // 1 天前 < 宽限期
        val result = OfflineAuthorizer.authorize(makeKey(), nowMillis, lastSync)
        assertTrue(result.allowed)
    }

    @Test
    fun customGraceCanBeTightened() {
        val lastSync = nowMillis - 2 * 3_600_000L // 2 小时前
        val tight = OfflineAuthorizer.authorize(
            makeKey(), nowMillis, lastSync, maxOfflineGraceMillis = 3_600_000L // 1 小时
        )
        assertFalse(tight.allowed)
        assertEquals(OfflineDenialReason.STALE_CACHE, tight.reason)

        val loose = OfflineAuthorizer.authorize(
            makeKey(), nowMillis, lastSync, maxOfflineGraceMillis = 3 * 3_600_000L // 3 小时
        )
        assertTrue(loose.allowed)
    }

    @Test
    fun zeroLastSyncSkipsGraceCheck() {
        // 无缓存历史时跳过宽限期检查（避免误杀首次离线）, 由状态/有效期兜底
        val result = OfflineAuthorizer.authorize(makeKey(), nowMillis, 0)
        assertTrue(result.allowed)
    }

    // ─── KeyManager 入口 ──────────────────────────────────

    @Test
    fun authorizeOfflineUseReturnsNullForMissingKey() {
        val (keyManager, _) = makeKeyManager()
        assertNull(keyManager.authorizeOfflineUse("nonexistent", nowMillis = nowMillis))
    }

    @Test
    fun authorizeOfflineUseAllowedWithFreshCache() {
        val (keyManager, cacheDir) = makeKeyManager()
        writeCache(cacheDir, listOf(makeKey(validUntil = nowMillis + 3_600_000)), lastSyncAt = nowMillis)

        val result = keyManager.authorizeOfflineUse("key-1", nowMillis = nowMillis)
        assertNotNull(result)
        assertTrue(result!!.allowed)
    }

    @Test
    fun authorizeOfflineUseDeniedForRevokedCachedKey() {
        val (keyManager, cacheDir) = makeKeyManager()
        writeCache(cacheDir, listOf(makeKey(status = "REVOKED")), lastSyncAt = nowMillis)

        val result = keyManager.authorizeOfflineUse("key-1", nowMillis = nowMillis)
        assertNotNull(result)
        assertFalse(result!!.allowed)
        assertEquals(OfflineDenialReason.REVOKED, result.reason)
    }

    @Test
    fun authorizeOfflineUseDeniedForStaleCache() {
        val (keyManager, cacheDir) = makeKeyManager()
        // 缓存 8 天前同步 → 超默认宽限期 7 天
        writeCache(cacheDir, listOf(makeKey()), lastSyncAt = nowMillis - 8 * 24 * 3_600_000L)

        val result = keyManager.authorizeOfflineUse("key-1", nowMillis = nowMillis)
        assertNotNull(result)
        assertFalse(result!!.allowed)
        assertEquals(OfflineDenialReason.STALE_CACHE, result.reason)
    }

    // ─── 测试夹具 ─────────────────────────────────────────

    /** 创建 keyManager 并返回其缓存目录（两者必须一致, 才能验证入口路径） */
    private fun makeKeyManager(): Pair<KeyManager, File> {
        val cacheDir = makeTempDir("yuledkcs_offline_auth_")
        val hubClient = runBlocking {
            HubClient.create(endpoint = "localhost", port = 1, config = SDKConfig(enableLogging = false))
        }
        val manager = KeyManager(hubClient, cacheDir, enableLogging = false)
        return manager to cacheDir
    }

    /** 手动创建临时目录（避免 createTempDir 的 ERROR 级弃用） */
    private fun makeTempDir(prefix: String): File {
        val dir = File(System.getProperty("java.io.tmpdir"), "$prefix${java.util.UUID.randomUUID()}")
        dir.mkdirs()
        return dir
    }

    /** 直接向缓存文件写入钥匙列表 + 自定义 lastSyncAt（绕过 write 的 now 时间戳） */
    private fun writeCache(cacheDir: File, keys: List<YDKKey>, lastSyncAt: Long) {
        val cacheFile = File(cacheDir, "yuledkcs_keys_cache.json")
        val data = KeyCache.CacheData(version = 1, lastSyncAt = lastSyncAt, keys = keys)
        cacheFile.writeText(Gson().toJson(data))
    }
}
