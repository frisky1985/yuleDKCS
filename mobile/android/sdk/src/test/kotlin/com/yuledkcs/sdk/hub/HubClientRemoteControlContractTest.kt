package com.yuledkcs.sdk.hub

import com.google.gson.JsonObject
import com.google.gson.JsonParser
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.runTest
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test

/**
 * Phase 4.1 / W1 — HubClient 远程控制 (SendCommand) 请求形状契约测试 (Android)
 *
 * 通过 MockWebServer 捕获真实 wire 请求, 固化 remoteLock / remoteUnlock /
 * remoteStart / remoteStop 的请求形状 (与 HubClientRequestShapeContractTest 同模式):
 *   1. 路径: POST /api/v1/vehicles/{vehicleId}/command
 *   2. body 字段: action / keyId / traceId (camelCase)
 *   3. action 枚举字符串: lock / unlock / engine_on / engine_off
 *   4. keyId 缺省为空字符串 ""; 显式传入原样透传
 *   5. body 不含 source 字段 — source=4(Remote) 由 Gateway 自动填充
 *   6. 响应 ControlCommandResponse 正确解码; 请求携带 Bearer 认证头
 * 附带覆盖同属请求形状缺口的 unbindKey / cancelShare (DELETE, 无 body)。
 */
class HubClientRemoteControlContractTest {

    private lateinit var mockServer: MockWebServer
    private lateinit var client: HubClient

    @Before
    fun setup() {
        // HubClient.create 是 suspend 函数, 需在非挂起 setup 中用 runBlocking 创建
        mockServer = MockWebServer()
        mockServer.start()
        client = runBlocking {
            HubClient.create(
                endpoint = mockServer.hostName,
                port = mockServer.port,
                config = SDKConfig(enableLogging = false)
            )
        }
        client.setToken("test-token")
    }

    @After
    fun tearDown() {
        client.shutdown()
        mockServer.shutdown()
    }

    private fun enqueueCommandResponse() {
        mockServer.enqueue(
            MockResponse().setResponseCode(200)
                .setBody("""{"cmdId":"cmd-001","resultCode":0,"errorMsg":null}""")
        )
    }

    private fun capturedBody(): JsonObject {
        val request = mockServer.takeRequest()
        return JsonParser.parseString(request.body.readUtf8()).asJsonObject
    }

    // ── remoteLock: 完整形状 ────────────────────────────────────────────────

    @Test
    fun `remoteLock 请求形状 - POST command 路径 + action lock + 无 source`() = runTest {
        enqueueCommandResponse()

        client.remoteLock("VH-REMO-001")

        val request = mockServer.takeRequest()
        assertEquals("POST", request.method)
        assertEquals("/api/v1/vehicles/VH-REMO-001/command", request.path)
        assertEquals("Bearer test-token", request.getHeader("Authorization"))

        val json = JsonParser.parseString(request.body.readUtf8()).asJsonObject
        assertEquals(setOf("action", "keyId", "traceId"), json.keySet())
        assertEquals("lock", json.get("action").asString)
        assertEquals("", json.get("keyId").asString)
        assertFalse("source 由 Gateway 自动填充, SDK 不得携带", json.has("source"))
        assertTrue("traceId 不得为空", json.get("traceId").asString.isNotEmpty())
    }

    @Test
    fun `remoteUnlock 请求形状 - action unlock`() = runTest {
        enqueueCommandResponse()

        client.remoteUnlock("VH-REMO-001")

        val request = mockServer.takeRequest()
        assertEquals("POST", request.method)
        assertEquals("/api/v1/vehicles/VH-REMO-001/command", request.path)
        val json = JsonParser.parseString(request.body.readUtf8()).asJsonObject
        assertEquals("unlock", json.get("action").asString)
        assertEquals("", json.get("keyId").asString)
    }

    @Test
    fun `remoteStart 请求形状 - action engine_on`() = runTest {
        enqueueCommandResponse()

        client.remoteStart("VH-REMO-001")

        val json = capturedBody()
        assertEquals("engine_on", json.get("action").asString)
        assertEquals("", json.get("keyId").asString)
    }

    @Test
    fun `remoteStop 请求形状 - action engine_off`() = runTest {
        enqueueCommandResponse()

        client.remoteStop("VH-REMO-001")

        val json = capturedBody()
        assertEquals("engine_off", json.get("action").asString)
        assertEquals("", json.get("keyId").asString)
    }

    // ── keyId 透传 ─────────────────────────────────────────────────────────

    @Test
    fun `keyId 显式传入时原样透传`() = runTest {
        enqueueCommandResponse()

        client.remoteLock("VH-REMO-001", keyId = "key-42")

        val json = capturedBody()
        assertEquals("key-42", json.get("keyId").asString)
    }

    // ── 响应解码 ───────────────────────────────────────────────────────────

    @Test
    fun `remoteLock 响应解码为 ControlCommandResponse`() = runTest {
        enqueueCommandResponse()

        val result = client.remoteLock("VH-REMO-001")

        assertEquals("cmd-001", result.cmdId)
        assertEquals(0, result.resultCode)
        assertNull(result.errorMsg)
    }

    // ── 同属请求形状缺口的 DELETE 操作 ─────────────────────────────────────

    @Test
    fun `unbindKey 请求形状 - DELETE keys 无 body`() = runTest {
        mockServer.enqueue(MockResponse().setResponseCode(204))

        client.unbindKey("key-777")

        val request = mockServer.takeRequest()
        assertEquals("DELETE", request.method)
        assertEquals("/api/v1/keys/key-777", request.path)
        assertEquals("", request.body.readUtf8())
    }

    @Test
    fun `cancelShare 请求形状 - DELETE shares 无 body`() = runTest {
        mockServer.enqueue(MockResponse().setResponseCode(204))

        client.cancelShare("share-888")

        val request = mockServer.takeRequest()
        assertEquals("DELETE", request.method)
        assertEquals("/api/v1/shares/share-888", request.path)
        assertEquals("", request.body.readUtf8())
    }
}
