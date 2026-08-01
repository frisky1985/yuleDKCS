package com.yuledkcs.sdk.keymanager

import com.google.gson.Gson
import com.yuledkcs.sdk.hub.HubClient
import com.yuledkcs.sdk.hub.SDKConfig
import com.yuledkcs.sdk.hub.YDKError
import com.yuledkcs.sdk.hub.YDKKey
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.runTest
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import java.io.File

/**
 * Phase 4.1 / W1 — KeyManager Push 触发增量同步 + 状态同步/离线推断测试 (Android)
 *
 * 覆盖审计缺口:
 *   - Push 回调入口 handleKeyStatusPush(keyId:): 有变更返回 true / 无变更返回 false
 *   - 状态同步: syncState 状态机 Success / Failed
 *   - 离线推断: 预置缓存文件后无网络读取 (getLocalKeys / getKey / keys Flow)
 *   - 差异检测 removed 分支: 云端消失的本地钥匙进入 removed
 *   - SyncResult.hasChanges 纯逻辑
 */
class KeyManagerPushSyncTest {

    private lateinit var mockServer: MockWebServer
    private lateinit var hubClient: HubClient
    private lateinit var keyManager: KeyManager
    private lateinit var tempDir: File

    @Before
    fun setup() {
        // HubClient.create 是 suspend 函数, 需在非挂起 setup 中用 runBlocking 创建
        mockServer = MockWebServer()
        mockServer.start()
        hubClient = runBlocking {
            HubClient.create(
                endpoint = mockServer.hostName,
                port = mockServer.port,
                config = SDKConfig(enableLogging = false)
            )
        }
        hubClient.setToken("test-token")
        tempDir = createTempDir("yuledkcs_push_")
        keyManager = KeyManager(hubClient, tempDir, enableLogging = false)
    }

    @After
    fun tearDown() {
        keyManager.shutdown()
        hubClient.shutdown()
        mockServer.shutdown()
        tempDir.deleteRecursively()
    }

    /** 入队一次 listKeys 云端钥匙列表响应 */
    private fun enqueueKeys(vararg keys: YDKKey) {
        mockServer.enqueue(
            MockResponse().setResponseCode(200)
                .setBody(Gson().toJson(mapOf("keys" to keys.toList())))
        )
    }

    /** 直接写入缓存文件（模拟上次同步落盘） */
    private fun seedCache(dir: File, keys: List<YDKKey>) {
        File(dir, "yuledkcs_keys_cache.json").writeText(
            Gson().toJson(KeyCache.CacheData(
                version = 1,
                lastSyncAt = System.currentTimeMillis(),
                keys = keys
            ))
        )
    }

    // ── Push 回调入口 ──────────────────────────────────────────────────────

    @Test
    fun `handleKeyStatusPush 检测到状态变更返回 true`() = runTest {
        // 首次全量同步: key-001 ACTIVE 入缓存
        enqueueKeys(YDKKey("key-001", "VH-001", status = "ACTIVE"))
        val first = keyManager.syncFromHub()
        assertEquals(1, first.added.size)

        // Push 触发增量同步: 云端 key-001 变为 REVOKED → 有变更
        enqueueKeys(YDKKey("key-001", "VH-001", status = "REVOKED"))
        val changed = keyManager.handleKeyStatusPush("key-001")

        assertTrue("Push 后检测到状态变更必须返回 true", changed)
        assertTrue("同步成功必须进入 Success 状态", keyManager.syncState.value is KeyManager.SyncState.Success)
        assertEquals("REVOKED", keyManager.getLocalKeys().first { it.keyId == "key-001" }.status)
    }

    @Test
    fun `handleKeyStatusPush 无变更时返回 false`() = runTest {
        enqueueKeys(YDKKey("key-001", "VH-001", status = "ACTIVE"))
        keyManager.syncFromHub()

        // 云端数据无变化 → Push 同步后无变更
        enqueueKeys(YDKKey("key-001", "VH-001", status = "ACTIVE"))
        val changed = keyManager.handleKeyStatusPush("key-001")

        assertFalse("无变更时 handleKeyStatusPush 必须返回 false", changed)
        assertEquals(1, keyManager.getLocalKeys().size)
    }

    // ── 状态同步状态机 ─────────────────────────────────────────────────────

    @Test
    fun `sync 失败时 syncState 为 Failed 且向上抛错`() = runTest {
        // 非 JSON 错误体 → 走 HttpError 分支
        mockServer.enqueue(MockResponse().setResponseCode(500).setBody("internal server error"))

        // 用 try/catch 捕获 (suspend 调用不能放进 assertFailsWith 的非挂起 lambda)
        var caught: YDKError.HttpError? = null
        try {
            keyManager.syncFromHub()
        } catch (e: YDKError.HttpError) {
            caught = e
        }
        assertNotNull("必须抛出 YDKError.HttpError", caught)
        assertEquals(500, caught!!.statusCode)
        assertTrue("同步失败必须进入 Failed 状态", keyManager.syncState.value is KeyManager.SyncState.Failed)
    }

    // ── 离线推断 ───────────────────────────────────────────────────────────

    @Test
    fun `离线推断 - 预置缓存文件后无网络读取`() {
        val offlineDir = createTempDir("yuledkcs_offline_")
        try {
            seedCache(
                offlineDir,
                listOf(
                    YDKKey("key-off-1", "VH-1", status = "ACTIVE"),
                    YDKKey("key-off-2", "VH-2", status = "SUSPENDED")
                )
            )

            // 离线场景: 不发起任何网络请求, 直接构造 KeyManager 读取缓存
            val offlineKM = KeyManager(hubClient, offlineDir, enableLogging = false)
            try {
                assertEquals(2, offlineKM.getLocalKeys().size)
                assertEquals("keys Flow 初始化时必须加载本地缓存", 2, offlineKM.keys.value.size)
                assertNotNull(offlineKM.getKey("key-off-1"))
                assertEquals("ACTIVE", offlineKM.getKey("key-off-1")?.status)
                assertNull("preferCache=false 必须跳过本地缓存", offlineKM.getKey("key-off-1", preferCache = false))
                assertNull(offlineKM.getKey("missing"))
            } finally {
                offlineKM.shutdown()
            }
        } finally {
            offlineDir.deleteRecursively()
        }
    }

    // ── 差异检测 removed 分支 ──────────────────────────────────────────────

    @Test
    fun `sync 检测 removed - 云端消失的本地钥匙进入 removed`() = runTest {
        // 预置本地缓存: key-001 + key-002
        seedCache(
            tempDir,
            listOf(
                YDKKey("key-001", "VH-001", status = "ACTIVE"),
                YDKKey("key-002", "VH-002", status = "ACTIVE")
            )
        )

        // 云端只剩 key-002 → key-001 应进入 removed
        enqueueKeys(YDKKey("key-002", "VH-002", status = "ACTIVE"))
        val result = keyManager.syncFromHub()

        assertEquals(0, result.added.size)
        assertEquals(0, result.updated.size)
        assertEquals(1, result.removed.size)
        assertEquals("key-001", result.removed[0].keyId)
        assertEquals(listOf("key-002"), keyManager.getLocalKeys().map { it.keyId })
    }

    // ── 纯逻辑 ─────────────────────────────────────────────────────────────

    @Test
    fun `SyncResult hasChanges 纯逻辑`() {
        val key = YDKKey("key-1", "VH-1", status = "ACTIVE")

        assertTrue(SyncResult(listOf(key), emptyList(), emptyList(), 0).hasChanges)
        assertTrue(SyncResult(emptyList(), listOf(key), emptyList(), 1).hasChanges)
        assertTrue(SyncResult(emptyList(), emptyList(), listOf(key), 0).hasChanges)
        assertFalse("无任何差异时 hasChanges 必须为 false", SyncResult(emptyList(), emptyList(), emptyList(), 3).hasChanges)
    }
}
