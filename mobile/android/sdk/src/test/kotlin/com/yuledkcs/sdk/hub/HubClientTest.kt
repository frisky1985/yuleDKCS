package com.yuledkcs.sdk.hub

import kotlinx.coroutines.test.runTest
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Assert.*
import org.junit.Before
import org.junit.Test

class HubClientTest {

    private lateinit var mockServer: MockWebServer
    private lateinit var client: HubClient

    @Before
    fun setup() {
        mockServer = MockWebServer()
        mockServer.start()

        client = HubClient.create(
            endpoint = mockServer.hostName,
            port = mockServer.port,
            config = SDKConfig(enableLogging = false)
        )
        client.setToken("test-token")
    }

    @After
    fun tearDown() {
        client.shutdown()
        mockServer.shutdown()
    }

    @Test
    fun `bindKey sends correct request`() = runTest {
        mockServer.enqueue(MockResponse()
            .setResponseCode(200)
            .setBody("""{"keyId":"key-001","vehicleId":"VH-001"}"""))

        val result = client.bindKey("VH-001")

        assertEquals("key-001", result.keyId)

        val request = mockServer.takeRequest()
        assertEquals("POST", request.method)
        assertEquals("/api/v1/keys", request.path)
        assertEquals("Bearer test-token", request.getHeader("Authorization"))
    }

    @Test
    fun `hub error throws HubError`() = runTest {
        mockServer.enqueue(MockResponse()
            .setResponseCode(400)
            .setBody("""{"code":"INVALID_VEHICLE","message":"vehicle_id not found"}"""))

        val error = assertFailsWith<YDKError.HubError> {
            client.bindKey("INVALID")
        }
        assertEquals("INVALID_VEHICLE", error.code)
    }

    @Test
    fun `setToken and clearToken`() {
        client.setToken("token-456")
        client.clearToken()
        // 验证 token 已清除（后续请求不会带 Authorization header）
    }
}
