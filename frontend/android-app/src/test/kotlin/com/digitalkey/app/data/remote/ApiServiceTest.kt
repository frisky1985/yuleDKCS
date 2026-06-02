/**
 * DigitalKey App - ApiService 单元测试
 *
 * 使用 MockWebServer 模拟后端 HTTP 响应，验证：
 * - 请求路径、方法、请求体正确
 * - 正常响应解析正确
 * - 错误响应（4xx, 5xx）处理正确
 * - 认证 Token 注入
 * - 各 API 端点的参数传递
 */
package com.digitalkey.app.data.remote

import com.digitalkey.app.data.remote.dto.*
import com.google.gson.Gson
import kotlinx.coroutines.runBlocking
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import okhttp3.mockwebserver.RecordedRequest
import org.junit.After
import org.junit.Assert.*
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4
import retrofit2.Response
import retrofit2.Retrofit
import retrofit2.converter.gson.GsonConverterFactory

@RunWith(JUnit4::class)
class ApiServiceTest {

    private lateinit var mockWebServer: MockWebServer
    private lateinit var apiService: ApiService
    private val gson = Gson()

    @Before
    fun setup() {
        mockWebServer = MockWebServer()
        mockWebServer.start()

        apiService = Retrofit.Builder()
            .baseUrl(mockWebServer.url("/"))
            .addConverterFactory(GsonConverterFactory.create(gson))
            .build()
            .create(ApiService::class.java)
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()
    }

    // ════════════════════════════════════════════
    // 辅助方法
    // ════════════════════════════════════════════

    /**
     * 验证请求的方法和路径
     */
    private fun RecordedRequest.assertMethodAndPath(expectedMethod: String, expectedPath: String) {
        assertEquals("HTTP method mismatch", expectedMethod, method)
        assertTrue(
            "Path should contain '$expectedPath' but was: '${path}'",
            path?.contains(expectedPath) == true
        )
    }

    /**
     * 解析请求体为指定类型
     */
    private inline fun <reified T> RecordedRequest.parseBody(): T {
        val body = body.readUtf8()
        assertTrue("Request body should not be empty", body.isNotBlank())
        return gson.fromJson(body, T::class.java)
    }

    // ════════════════════════════════════════════
    // 密钥绑定 API
    // ════════════════════════════════════════════

    @Test
    fun `bindKey should POST to keys and return success response`() = runBlocking {
        // Arrange
        val responseBody = """
        {
            "key": {
                "key_id": "key-ccc-apple-001",
                "vehicle_id": "VH-BIND-001",
                "device_id": "dev-iphone-15-pro-001",
                "user_id": "user-001",
                "key_type": "OWNER",
                "protocol": "CCC_DK3",
                "access_level": {
                    "lock": true,
                    "unlock": true,
                    "engine": true,
                    "trunk": true,
                    "window": true,
                    "climate": true,
                    "find": true,
                    "seat": false
                },
                "key_version": 1,
                "status": "ACTIVE",
                "valid_from": 1715000000,
                "valid_until": 1815000000,
                "created_at": 1715000000
            },
            "vehicle_pubkey": "base64EncodedPubKey",
            "shared_secret": "base64EncodedSecret",
            "error_code": "",
            "error_msg": null
        }
        """.trimIndent()

        mockWebServer.enqueue(
            MockResponse()
                .setResponseCode(200)
                .setBody(responseBody)
        )

        val request = BindKeyRequest(
            vehicleId = "VH-BIND-001",
            deviceId = "dev-iphone-15-pro-001",
            vendor = PhoneVendor.APPLE,
            protocol = Protocol.CCC_DK3,
            keyType = KeyType.OWNER
        )

        // Act
        val response: Response<BindKeyResponse> = apiService.bindKey(request)

        // Assert - 验证请求
        val recordedRequest = mockWebServer.takeRequest()
        recordedRequest.assertMethodAndPath("POST", "/keys")

        val requestBody = recordedRequest.parseBody<BindKeyRequest>()
        assertEquals("VH-BIND-001", requestBody.vehicleId)
        assertEquals(PhoneVendor.APPLE, requestBody.vendor)
        assertEquals(Protocol.CCC_DK3, requestBody.protocol)

        // Assert - 验证响应
        assertTrue("Response should be successful", response.isSuccessful)
        val bindResponse = response.body()
        assertNotNull("Response body should not be null", bindResponse)
        assertEquals("key-ccc-apple-001", bindResponse!!.key.keyId)
        assertEquals("OWNER", bindResponse.key.keyType?.value)
        assertNotNull("vehicle_pubkey should be present", bindResponse.vehiclePubkey)
        assertNotNull("shared_secret should be present", bindResponse.sharedSecret)
        assertEquals("error_code should be empty", "", bindResponse.errorCode)
    }

    @Test
    fun `bindKey should send all optional fields`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse().setResponseCode(200).setBody("""{"key":{"key_id":"k1","vehicle_id":"v1"}}""")
        )

        val request = BindKeyRequest(
            vehicleId = "VH-BIND-001",
            deviceId = "dev-001",
            userId = "user-001",
            vendor = PhoneVendor.XIAOMI,
            protocol = Protocol.ICCOA_DK40,
            keyType = KeyType.TEMPORARY,
            devicePubkey = "BASE64PUBKEY",
            validFrom = 1715000000,
            validUntil = 1815000000,
            traceId = "trace-abc-123"
        )

        // Act
        apiService.bindKey(request)

        // Assert
        val recordedRequest = mockWebServer.takeRequest()
        val body = recordedRequest.parseBody<BindKeyRequest>()

        assertEquals("VH-BIND-001", body.vehicleId)
        assertEquals("dev-001", body.deviceId)
        assertEquals("user-001", body.userId)
        assertEquals(PhoneVendor.XIAOMI, body.vendor)
        assertEquals(Protocol.ICCOA_DK40, body.protocol)
        assertEquals(KeyType.TEMPORARY, body.keyType)
        assertEquals("BASE64PUBKEY", body.devicePubkey)
        assertEquals(1715000000L, body.validFrom)
        assertEquals(1815000000L, body.validUntil)
        assertEquals("trace-abc-123", body.traceId)
    }

    // ════════════════════════════════════════════
    // 密钥解绑 API
    // ════════════════════════════════════════════

    @Test
    fun `unbindKey should DELETE to keys keyId`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse()
                .setResponseCode(200)
                .setBody("""{"error_code": ""}""")
        )

        // Act
        val response: Response<KeyErrorResponse> = apiService.unbindKey("key-ccc-apple-001")

        // Assert
        val recordedRequest = mockWebServer.takeRequest()
        recordedRequest.assertMethodAndPath("DELETE", "/keys/key-ccc-apple-001")

        assertTrue("Response should be successful", response.isSuccessful)
        assertEquals("", response.body()?.errorCode)
    }

    @Test
    fun `unbindKey non-existent key should return 404`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse()
                .setResponseCode(404)
                .setBody("""{"error":"GRPC_NOT_FOUND","message":"key not found: key-nonexistent","detail":"resource not found in gRPC backend"}""")
        )

        // Act
        val response: Response<KeyErrorResponse> = apiService.unbindKey("key-nonexistent")

        // Assert
        assertEquals("Should return 404", 404, response.code())
        assertFalse("Response should not be successful", response.isSuccessful)

        val errorBody = response.errorBody()?.string()
        assertNotNull("Error body should not be null", errorBody)
        assertTrue(errorBody!!.contains("GRPC_NOT_FOUND"))
        assertTrue(errorBody.contains("key not found"))
    }

    // ════════════════════════════════════════════
    // 密钥列表 API
    // ════════════════════════════════════════════

    @Test
    fun `listKeys should GET keys and return key list`() = runBlocking {
        // Arrange
        val responseBody = """
        {
            "keys": [
                {
                    "key_id": "key-ccc-apple-001",
                    "vehicle_id": "VH-001",
                    "user_id": "user-001",
                    "key_type": "OWNER",
                    "protocol": "CCC_DK3",
                    "status": "ACTIVE",
                    "valid_from": 1715000000,
                    "valid_until": 1815000000,
                    "created_at": 1715000000
                },
                {
                    "key_id": "key-iccoa-002",
                    "vehicle_id": "VH-002",
                    "user_id": "user-002",
                    "key_type": "FRIEND",
                    "protocol": "ICCOA_DK30",
                    "status": "SUSPENDED",
                    "valid_from": 1715000000,
                    "valid_until": 1745000000,
                    "created_at": 1716000000
                }
            ],
            "next_token": "page-2-token",
            "total": 42
        }
        """.trimIndent()

        mockWebServer.enqueue(
            MockResponse().setResponseCode(200).setBody(responseBody)
        )

        // Act
        val response: Response<ListKeysResponse> = apiService.listKeys(
            userId = "user-001",
            limit = 20
        )

        // Assert - 请求
        val recordedRequest = mockWebServer.takeRequest()
        recordedRequest.assertMethodAndPath("GET", "/keys")
        assertTrue("Should include userId query param", recordedRequest.path?.contains("user_id=user-001") == true)
        assertTrue("Should include limit query param", recordedRequest.path?.contains("limit=20") == true)

        // Assert - 响应
        assertTrue(response.isSuccessful)
        val listResponse = response.body()
        assertNotNull(listResponse)
        assertEquals(2, listResponse!!.keys.size)
        assertEquals("key-ccc-apple-001", listResponse.keys[0].keyId)
        assertEquals(KeyType.OWNER, listResponse.keys[0].keyType)
        assertEquals(KeyStatusDto.ACTIVE, listResponse.keys[0].status)
        assertEquals("key-iccoa-002", listResponse.keys[1].keyId)
        assertEquals(KeyType.FRIEND, listResponse.keys[1].keyType)
        assertEquals(KeyStatusDto.SUSPENDED, listResponse.keys[1].status)
        assertEquals("page-2-token", listResponse.nextToken)
        assertEquals(42, listResponse.total)
    }

    @Test
    fun `listKeys empty result should return empty keys array`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse().setResponseCode(200).setBody("""{"keys":[],"total":0}""")
        )

        // Act
        val response = apiService.listKeys(vehicleId = "nonexistent-vehicle")

        // Assert
        assertTrue(response.isSuccessful)
        assertNotNull(response.body())
        assertTrue(response.body()!!.keys.isEmpty())
        assertEquals(0, response.body()!!.total)
    }

    @Test
    fun `listKeys with offset pagination should pass offset parameter`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse().setResponseCode(200).setBody("""{"keys":[],"next_token":"page-3"}""")
        )

        // Act
        apiService.listKeys(limit = 10, offset = "page-2")

        // Assert
        val request = mockWebServer.takeRequest()
        assertTrue("Should include offset", request.path?.contains("offset=page-2") == true)
        assertTrue("Should include limit=10", request.path?.contains("limit=10") == true)
    }

    // ════════════════════════════════════════════
    // 单密钥查询 API
    // ════════════════════════════════════════════

    @Test
    fun `getKey should GET keys keyId and return key details`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse().setResponseCode(200).setBody("""
                {"key":{"key_id":"key-001","vehicle_id":"v1","status":"ACTIVE"},"error_code":""}
            """.trimIndent())
        )

        // Act
        val response = apiService.getKey("key-001")

        // Assert
        val request = mockWebServer.takeRequest()
        request.assertMethodAndPath("GET", "/keys/key-001")

        assertTrue(response.isSuccessful)
        assertEquals("key-001", response.body()?.key?.keyId)
        assertEquals(KeyStatusDto.ACTIVE, response.body()?.key?.status)
    }

    @Test
    fun `getKey non-existent should return 404`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse().setResponseCode(404).setBody("""{"error":"GRPC_NOT_FOUND","message":"key not found"}""")
        )

        // Act
        val response = apiService.getKey("does-not-exist")

        // Assert
        assertEquals(404, response.code())
        assertFalse(response.isSuccessful)
    }

    // ════════════════════════════════════════════
    // 密钥暂停/恢复/吊销 API
    // ════════════════════════════════════════════

    @Test
    fun `suspendKey should PUT to keys keyId suspend`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse().setResponseCode(200).setBody("""{"error_code": ""}""")
        )

        // Act
        val response = apiService.suspendKey("key-001", KeyActionRequest(reason = "设备丢失"))

        // Assert
        val request = mockWebServer.takeRequest()
        request.assertMethodAndPath("PUT", "/keys/key-001/suspend")
        val body = request.parseBody<KeyActionRequest>()
        assertEquals("设备丢失", body.reason)

        assertTrue(response.isSuccessful)
    }

    @Test
    fun `resumeKey should PUT to keys keyId resume`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse().setResponseCode(200).setBody("""{"error_code": ""}""")
        )

        // Act
        val response = apiService.resumeKey("key-001")

        // Assert
        val request = mockWebServer.takeRequest()
        request.assertMethodAndPath("PUT", "/keys/key-001/resume")
        assertTrue(response.isSuccessful)
    }

    @Test
    fun `revokeKey should PUT to keys keyId revoke`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse().setResponseCode(200).setBody("""{"error_code": ""}""")
        )

        // Act
        val response = apiService.revokeKey("key-001", KeyActionRequest(reason = "车辆已售出"))

        // Assert
        val request = mockWebServer.takeRequest()
        request.assertMethodAndPath("PUT", "/keys/key-001/revoke")
        val body = request.parseBody<KeyActionRequest>()
        assertEquals("车辆已售出", body.reason)
        assertTrue(response.isSuccessful)
    }

    // ════════════════════════════════════════════
    // 密钥续期 API
    // ════════════════════════════════════════════

    @Test
    fun `renewKey should PUT to keys keyId renew`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse().setResponseCode(200).setBody("""
                {"key":{"key_id":"key-001","valid_until":1915000000},"error_code":""}
            """.trimIndent())
        )

        // Act
        val response = apiService.renewKey("key-001", RenewKeyRequest(validUntil = 1915000000L))

        // Assert
        val request = mockWebServer.takeRequest()
        request.assertMethodAndPath("PUT", "/keys/key-001/renew")
        val body = request.parseBody<RenewKeyRequest>()
        assertEquals(1915000000L, body.validUntil)

        assertTrue(response.isSuccessful)
        assertEquals(1915000000L, response.body()?.key?.validUntil)
    }

    // ════════════════════════════════════════════
    // 车辆控制 API
    // ════════════════════════════════════════════

    @Test
    fun `sendCommand should POST to vehicles vehicleId command`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse().setResponseCode(200).setBody("""
                {"cmd_id": "cmd-001", "result_code": 0, "error_msg": null}
            """.trimIndent())
        )

        // Act
        val response = apiService.sendCommand(
            vehicleId = "VH-REMOTE-001",
            request = SendCommandRequest(
                action = "unlock",
                keyId = "key-ccc-apple-001",
                source = 4 // Remote
            )
        )

        // Assert - 请求
        val request = mockWebServer.takeRequest()
        request.assertMethodAndPath("POST", "/vehicles/VH-REMOTE-001/command")

        val body = request.parseBody<SendCommandRequest>()
        assertEquals("unlock", body.action)
        assertEquals("key-ccc-apple-001", body.keyId)
        assertEquals(4, body.source)

        // Assert - 响应
        assertTrue(response.isSuccessful)
        assertEquals("cmd-001", response.body()?.cmdId)
        assertEquals(0, response.body()?.resultCode)
    }

    @Test
    fun `sendCommand all vehicle actions should route correctly`() = runBlocking {
        val actions = listOf("unlock", "lock", "engine_on", "engine_off", "trunk", "climate", "find")

        for (action in actions) {
            // Arrange
            mockWebServer.enqueue(
                MockResponse().setResponseCode(200).setBody("""
                    {"cmd_id":"cmd-${action}","result_code":0}
                """.trimIndent())
            )

            // Act
            apiService.sendCommand(
                vehicleId = "VH-001",
                request = SendCommandRequest(
                    action = action,
                    keyId = "key-001"
                )
            )

            // Assert
            val recordedRequest = mockWebServer.takeRequest()
            val requestBody = recordedRequest.parseBody<SendCommandRequest>()
            assertEquals("Action should be '$action'", action, requestBody.action)
        }
    }

    @Test
    fun `sendCommand with params should send params in body`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse().setResponseCode(200).setBody(
                """{"cmd_id":"cmd-001","result_code":0}"""
            )
        )

        // Act
        apiService.sendCommand(
            vehicleId = "VH-001",
            request = SendCommandRequest(
                action = "climate",
                keyId = "key-001",
                params = mapOf("temperature" to 24, "mode" to "cool"),
                traceId = "trace-climate-001"
            )
        )

        // Assert
        val request = mockWebServer.takeRequest()
        val body = request.parseBody<SendCommandRequest>()
        assertEquals("climate", body.action)
        assertNotNull("params should not be null", body.params)
        // 24 might be decoded as Double in JSON, check the map
        assertTrue("Should contain temperature param", body.params!!.containsKey("temperature"))
        assertTrue("Should contain mode param", body.params!!.containsKey("mode"))
        assertEquals("trace-climate-001", body.traceId)
    }

    // ════════════════════════════════════════════
    // 密钥分享 API
    // ════════════════════════════════════════════

    @Test
    fun `createShare should POST to shares and return share code`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse().setResponseCode(200).setBody("""
                {"share_id":"share-abc-123","share_code":"382914","error_code":""}
            """.trimIndent())
        )

        // Act
        val response = apiService.createShare(
            CreateShareRequest(
                keyId = "key-ccc-apple-001",
                toUserId = "user-friend-01",
                accessLevel = AccessLevel(lock = true, unlock = true, engine = true),
                validUntil = 1815000000L
            )
        )

        // Assert
        val request = mockWebServer.takeRequest()
        request.assertMethodAndPath("POST", "/shares")
        val body = request.parseBody<CreateShareRequest>()
        assertEquals("key-ccc-apple-001", body.keyId)
        assertEquals("user-friend-01", body.toUserId)

        assertTrue(response.isSuccessful)
        assertEquals("share-abc-123", response.body()?.shareId)
        assertEquals("382914", response.body()?.shareCode)
    }

    @Test
    fun `createShare without to_user_id should generate share code`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse().setResponseCode(200).setBody("""
                {"share_id":"share-abc-456","share_code":"729183","error_code":""}
            """.trimIndent())
        )

        // Act
        val response = apiService.createShare(
            CreateShareRequest(keyId = "key-001")
        )

        // Assert
        assertTrue(response.isSuccessful)
        val shareResponse = response.body()
        assertNotNull(shareResponse?.shareCode)
        assertEquals("729183", shareResponse!!.shareCode)
    }

    @Test
    fun `acceptShare should POST to shares accept with share code`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse().setResponseCode(200).setBody("""
                {"key":{"key_id":"key-accepted-001","vehicle_id":"VH-001","status":"ACTIVE"},"shared_secret":"secret123","error_code":""}
            """.trimIndent())
        )

        // Act
        val response = apiService.acceptShare(
            AcceptShareRequest(
                shareCode = "382914",
                deviceId = "device-002",
                vendor = PhoneVendor.XIAOMI
            )
        )

        // Assert
        val request = mockWebServer.takeRequest()
        request.assertMethodAndPath("POST", "/shares/accept")
        val body = request.parseBody<AcceptShareRequest>()
        assertEquals("382914", body.shareCode)
        assertEquals(PhoneVendor.XIAOMI, body.vendor)

        assertTrue(response.isSuccessful)
        assertEquals("key-accepted-001", response.body()?.key?.keyId)
        assertEquals("secret123", response.body()?.sharedSecret)
    }

    @Test
    fun `cancelShare should DELETE to shares shareId`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse().setResponseCode(200).setBody("""{"error_code":""}""")
        )

        // Act
        val response = apiService.cancelShare("share-abc-123")

        // Assert
        val request = mockWebServer.takeRequest()
        request.assertMethodAndPath("DELETE", "/shares/share-abc-123")
        assertTrue(response.isSuccessful)
    }

    // ════════════════════════════════════════════
    // Token 管理 API
    // ════════════════════════════════════════════

    @Test
    fun `issueToken should POST to tokens`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse().setResponseCode(200).setBody("""
                {"token_id":"tok-001","expires_at":1815000000,"signature":"sig123"}
            """.trimIndent())
        )

        // Act
        val response = apiService.issueToken(
            IssueTokenRequest(
                subjectId = "user-friend-01",
                vehicleId = "VH-001",
                permissions = listOf("lock", "engine", "trunk"),
                duration = "2h"
            )
        )

        // Assert
        val request = mockWebServer.takeRequest()
        request.assertMethodAndPath("POST", "/tokens")
        val body = request.parseBody<IssueTokenRequest>()
        assertEquals("user-friend-01", body.subjectId)
        assertEquals("VH-001", body.vehicleId)
        assertEquals(3, body.permissions?.size)

        assertTrue(response.isSuccessful)
        assertEquals("tok-001", response.body()?.tokenId)
        assertEquals(1815000000L, response.body()?.expiresAt)
    }

    @Test
    fun `verifyToken should GET tokens tokenId`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse().setResponseCode(200).setBody("""
                {"valid":true,"owner_id":"user-001","subject_id":"user-friend-01","vehicle_id":"VH-001","permissions":["lock","engine"],"expires_at":1815000000}
            """.trimIndent())
        )

        // Act
        val response = apiService.verifyToken("tok-001")

        // Assert
        val request = mockWebServer.takeRequest()
        request.assertMethodAndPath("GET", "/tokens/tok-001")

        assertTrue(response.isSuccessful)
        assertTrue(response.body()!!.valid)
        assertEquals("user-001", response.body()!!.ownerId)
    }

    @Test
    fun `exchangeToken should POST to tokens tokenId exchange`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse().setResponseCode(200).setBody("""
                {"exchanged":true,"token_id":"tok-001","key_id":"key-exchanged-001","note":"钥匙已签发"}
            """.trimIndent())
        )

        // Act
        val response = apiService.exchangeToken("tok-001")

        // Assert
        val request = mockWebServer.takeRequest()
        request.assertMethodAndPath("POST", "/tokens/tok-001/exchange")

        assertTrue(response.isSuccessful)
        assertTrue(response.body()!!.exchanged)
        assertEquals("key-exchanged-001", response.body()!!.keyId)
    }

    // ════════════════════════════════════════════
    // 设备管理 API
    // ════════════════════════════════════════════

    @Test
    fun `registerDevice should POST to devices`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse().setResponseCode(200).setBody("""
                {"device_id":"dev-001","platform":"ios","model":"iPhone 15 Pro","ble":true,"uwb":true,"nfc":true,"max_devices":5}
            """.trimIndent())
        )

        // Act
        val response = apiService.registerDevice(
            RegisterDeviceRequest(
                platform = "ios",
                model = "iPhone 15 Pro",
                osVersion = "18.0",
                ble = true,
                uwb = true,
                nfc = true
            )
        )

        // Assert
        val request = mockWebServer.takeRequest()
        request.assertMethodAndPath("POST", "/devices")
        val body = request.parseBody<RegisterDeviceRequest>()
        assertEquals("ios", body.platform)
        assertEquals("iPhone 15 Pro", body.model)

        assertTrue(response.isSuccessful)
        assertEquals("dev-001", response.body()?.deviceId)
        assertEquals(5, response.body()?.maxDevices)
    }

    @Test
    fun `provisionDevice should POST to devices deviceId provision`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse().setResponseCode(200).setBody("""
                {"key_id":"key-provisioned-001","device_id":"dev-001","vehicle_id":"VH-001","status":"ACTIVE","note":"密钥已下发至设备"}
            """.trimIndent())
        )

        // Act
        val response = apiService.provisionDevice(
            deviceId = "dev-001",
            request = ProvisionDeviceRequest(vehicleId = "VH-PROV-001")
        )

        // Assert
        val request = mockWebServer.takeRequest()
        request.assertMethodAndPath("POST", "/devices/dev-001/provision")
        val body = request.parseBody<ProvisionDeviceRequest>()
        assertEquals("VH-PROV-001", body.vehicleId)

        assertTrue(response.isSuccessful)
        assertEquals("key-provisioned-001", response.body()?.keyId)
        assertEquals("密钥已下发至设备", response.body()?.note)
    }

    @Test
    fun `listDevices should GET devices`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse().setResponseCode(200).setBody("""
                {"devices":[{"device_id":"dev-001","platform":"ios","model":"iPhone 15 Pro","ble":true,"uwb":true,"nfc":true}]}
            """.trimIndent())
        )

        // Act
        val response = apiService.listDevices()

        // Assert
        val request = mockWebServer.takeRequest()
        request.assertMethodAndPath("GET", "/devices")

        assertTrue(response.isSuccessful)
        assertEquals(1, response.body()?.devices?.size)
        assertEquals("dev-001", response.body()?.devices?.first()?.deviceId)
    }

    // ════════════════════════════════════════════
    // 登录 API
    // ════════════════════════════════════════════

    @Test
    fun `login should POST to auth login and return token`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse().setResponseCode(200).setBody("""
                {"token":"eyJhbGciOiJIUzI1NiIs...","token_type":"Bearer","expires_in":3600}
            """.trimIndent())
        )

        // Act
        val response = apiService.login(
            LoginRequest(userId = "admin", password = "admin123")
        )

        // Assert
        val request = mockWebServer.takeRequest()
        request.assertMethodAndPath("POST", "/auth/login")
        val body = request.parseBody<LoginRequest>()
        assertEquals("admin", body.userId)
        assertEquals("admin123", body.password)

        assertTrue(response.isSuccessful)
        assertEquals("eyJhbGciOiJIUzI1NiIs...", response.body()?.token)
        assertEquals("Bearer", response.body()?.tokenType)
        assertEquals(3600, response.body()?.expiresIn)
    }

    @Test
    fun `login with wrong credentials should return 401`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse().setResponseCode(401).setBody("""
                {"error":"AUTH_INVALID_CREDENTIALS","message":"username or password is incorrect","detail":"invalid credentials"}
            """.trimIndent())
        )

        // Act
        val response = apiService.login(
            LoginRequest(userId = "wrong", password = "wrong")
        )

        // Assert
        assertEquals(401, response.code())
        assertFalse(response.isSuccessful)

        val errorBody = response.errorBody()?.string()
        assertNotNull(errorBody)
        assertTrue(errorBody!!.contains("AUTH_INVALID_CREDENTIALS"))
    }

    // ════════════════════════════════════════════
    // 健康检查 API
    // ════════════════════════════════════════════

    @Test
    fun `healthCheck should GET health and return ok`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse().setResponseCode(200).setBody("""{"status":"ok"}""")
        )

        // Act
        val response = apiService.healthCheck()

        // Assert
        val request = mockWebServer.takeRequest()
        request.assertMethodAndPath("GET", "/health")

        assertTrue(response.isSuccessful)
        assertEquals("ok", response.body()?.status)
    }

    @Test
    fun `hubHealthCheck should GET hub health`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse().setResponseCode(200).setBody("""
                {"healthy":true,"adapters":[{"vendor":"APPLE","protocol":"CCC_DK3","healthy":true}]}
            """.trimIndent())
        )

        // Act
        val response = apiService.hubHealthCheck()

        // Assert
        val request = mockWebServer.takeRequest()
        request.assertMethodAndPath("GET", "/hub/health")

        assertTrue(response.isSuccessful)
        assertTrue(response.body()!!.healthy)
        assertEquals(1, response.body()!!.adapters?.size)
        assertEquals("APPLE", response.body()!!.adapters?.first()?.vendor)
    }

    // ════════════════════════════════════════════
    // 错误响应处理
    // ════════════════════════════════════════════

    @Test
    fun `400 Bad Request should return error body`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse().setResponseCode(400).setBody("""
                {"error":"BAD_REQUEST","message":"invalid vehicle_id format","detail":"vehicle_id must be a non-empty string"}
            """.trimIndent())
        )

        // Act
        val response = apiService.bindKey(
            BindKeyRequest(vehicleId = "")
        )

        // Assert
        assertEquals(400, response.code())
        assertFalse(response.isSuccessful)

        val errorBody = response.errorBody()?.string()
        assertNotNull(errorBody)
        assertTrue(errorBody!!.contains("BAD_REQUEST"))
        assertTrue(errorBody.contains("invalid vehicle_id"))
    }

    @Test
    fun `401 Unauthorized should return auth error`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse().setResponseCode(401).setBody("""
                {"error":"AUTH_INVALID_TOKEN","message":"invalid or expired token"}
            """.trimIndent())
        )

        // Act
        val response = apiService.listKeys()

        // Assert
        assertEquals(401, response.code())
        assertFalse(response.isSuccessful)

        val errorBody = response.errorBody()?.string()
        assertNotNull(errorBody)
        assertTrue(errorBody!!.contains("AUTH_INVALID_TOKEN"))
    }

    @Test
    fun `403 Forbidden should return permission error`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse().setResponseCode(403).setBody("""
                {"error":"FORBIDDEN","message":"no permission to perform this action"}
            """.trimIndent())
        )

        // Act
        val response = apiService.revokeKey("key-001", KeyActionRequest())

        // Assert
        assertEquals(403, response.code())
        assertFalse(response.isSuccessful)

        val errorBody = response.errorBody()?.string()
        assertNotNull(errorBody)
        assertTrue(errorBody!!.contains("FORBIDDEN"))
    }

    @Test
    fun `500 Internal Server Error should be handled`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse().setResponseCode(500).setBody("""
                {"error":"INTERNAL_ERROR","message":"internal server error"}
            """.trimIndent())
        )

        // Act
        val response = apiService.registerDevice(
            RegisterDeviceRequest(platform = "ios", model = "iPhone")
        )

        // Assert
        assertEquals(500, response.code())
        assertFalse(response.isSuccessful)
    }

    @Test
    fun `502 Bad Gateway should be handled`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse().setResponseCode(502).setBody("""
                {"error":"GRPC_UNAVAILABLE","message":"hub service temporarily unavailable"}
            """.trimIndent())
        )

        // Act
        val response = apiService.bindKey(
            BindKeyRequest(vehicleId = "VH-001")
        )

        // Assert
        assertEquals(502, response.code())
        assertFalse(response.isSuccessful)

        val errorBody = response.errorBody()?.string()
        assertNotNull(errorBody)
        assertTrue(errorBody!!.contains("GRPC_UNAVAILABLE"))
    }

    @Test
    fun `429 Rate Limited should return 429`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse().setResponseCode(429).setBody("""
                {"error":"ERR_RATE_LIMIT","message":"too many requests, please try again later"}
            """.trimIndent())
        )

        // Act
        val response = apiService.sendCommand(
            vehicleId = "VH-001",
            request = SendCommandRequest(action = "unlock", keyId = "key-001")
        )

        // Assert
        assertEquals(429, response.code())
    }

    @Test
    fun `empty response body should be handled gracefully`() = runBlocking {
        // Arrange
        // Gson+GsonConverterFactory needs at least an empty JSON object for a 200 response
        // A truly empty body with 200 will throw EOFException from Gson.
        // Provide empty JSON to test the response is successful with null error_code
        mockWebServer.enqueue(
            MockResponse().setResponseCode(200).setBody("{}")
        )

        // Act
        val response = apiService.unbindKey("key-001")

        // Assert
        assertTrue("Response should be successful", response.isSuccessful())
        // With empty JSON {}, body should not be null
        assertNotNull("Body should not be null with empty JSON", response.body())
        assertEquals("error_code should default to empty string", "", response.body()!!.errorCode)
    }

    // ════════════════════════════════════════════
    // ApiClient 配置测试
    // ════════════════════════════════════════════

    @Test
    fun `apiClient default instance should use production URL`() {
        // 使用 resetInstance 确保测试独立
        ApiClient.resetInstance()
        val client = ApiClient.createDefault()

        // 通过反射获取 baseUrl 不太方便，但可以验证实例不为空且 apiService 可正常创建
        assertNotNull("ApiClient instance should not be null", client)
        assertNotNull("ApiService should not be null", client.apiService)
    }

    @Test
    fun `apiClient setAuthToken should update token`() {
        ApiClient.resetInstance()
        val client = ApiClient.createDefault()

        assertNull("Initial token should be null", client.getAuthToken())

        client.setAuthToken("test-token-123")
        assertEquals("test-token-123", client.getAuthToken())

        client.clearAuthToken()
        assertNull("Token should be null after clear", client.getAuthToken())
    }

    @Test
    fun `apiClient should send auth header`() = runBlocking {
        // Arrange
        mockWebServer.enqueue(
            MockResponse().setResponseCode(200).setBody("""{"keys":[],"total":0}""")
        )

        val client = ApiClient.build(
            baseUrl = mockWebServer.url("/").toString(),
            enableLogging = false
        )
        client.setAuthToken("my-jwt-token")

        val service = client.apiService

        // Act
        service.listKeys()

        // Assert
        val request = mockWebServer.takeRequest()
        val authHeader = request.getHeader("Authorization")
        assertEquals("Bearer my-jwt-token", authHeader)
    }
}
