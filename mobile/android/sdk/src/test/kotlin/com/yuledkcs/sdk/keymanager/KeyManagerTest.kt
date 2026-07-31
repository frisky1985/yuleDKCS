package com.yuledkcs.sdk.keymanager

import com.google.gson.Gson
import com.yuledkcs.sdk.hub.HubClient
import com.yuledkcs.sdk.hub.SDKConfig
import com.yuledkcs.sdk.hub.YDKKey
import kotlinx.coroutines.test.runTest
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Assert.*
import org.junit.Before
import org.junit.Test
import java.io.File

class KeyManagerTest {

    private lateinit var mockServer: MockWebServer
    private lateinit var hubClient: HubClient
    private lateinit var keyManager: KeyManager
    private lateinit var tempDir: File

    @Before
    fun setup() {
        mockServer = MockWebServer()
        mockServer.start()

        hubClient = HubClient.create(
            endpoint = mockServer.hostName,
            port = mockServer.port,
            config = SDKConfig(enableLogging = false)
        )
        hubClient.setToken("test-token")

        tempDir = createTempDir("yuledkcs_test_")
        keyManager = KeyManager(hubClient, tempDir, enableLogging = false)
    }

    @After
    fun tearDown() {
        keyManager.shutdown()
        hubClient.shutdown()
        mockServer.shutdown()
        tempDir.deleteRecursively()
    }

    @Test
    fun `getLocalKeys returns empty initially`() {
        assertTrue(keyManager.getLocalKeys().isEmpty())
    }

    @Test
    fun `getKey returns null for missing key`() {
        assertNull(keyManager.getKey("nonexistent"))
    }

    @Test
    fun `syncFromHub caches keys from server`() = runTest {
        mockServer.enqueue(MockResponse()
            .setResponseCode(200)
            .setBody("""{"keys":[{"keyId":"key-001","vehicleId":"VH-001","deviceId":"dev-1","status":"ACTIVE"}]}"""))

        val result = keyManager.syncFromHub()

        assertEquals(1, result.added.size)
        assertEquals("key-001", result.added[0].keyId)
        assertEquals(1, keyManager.getLocalKeys().size)
    }

    @Test
    fun `syncFromHub detects diff`() = runTest {
        // First sync: add key-001
        mockServer.enqueue(MockResponse()
            .setResponseCode(200)
            .setBody("""{"keys":[{"keyId":"key-001","vehicleId":"VH-001","deviceId":"dev-1","status":"ACTIVE"}]}"""))

        var result = keyManager.syncFromHub()
        assertEquals(1, result.added.size)

        // Second sync: key-001 revoked, key-002 added
        mockServer.enqueue(MockResponse()
            .setResponseCode(200)
            .setBody("""{"keys":[{"keyId":"key-001","vehicleId":"VH-001","deviceId":"dev-1","status":"REVOKED"},{"keyId":"key-002","vehicleId":"VH-002","deviceId":"dev-2","status":"ACTIVE"}]}"""))

        result = keyManager.syncFromHub()

        assertEquals(1, result.added.size)     // key-002
        assertEquals(1, result.updated.size)    // key-001: ACTIVE → REVOKED
        assertTrue(result.removed.isEmpty())
    }

    @Test
    fun `clearCache empties local keys`() {
        // Seed cache
        val gson = Gson()
        val cacheFile = File(tempDir, "yuledkcs_keys_cache.json")
        cacheFile.writeText(gson.toJson(KeyCache.CacheData(
            version = 1,
            lastSyncAt = System.currentTimeMillis(),
            keys = listOf(YDKKey("key-001", "VH-001"))
        )))

        assertEquals(1, keyManager.getLocalKeys().size)
        keyManager.clearCache()
        assertTrue(keyManager.getLocalKeys().isEmpty())
    }
}
