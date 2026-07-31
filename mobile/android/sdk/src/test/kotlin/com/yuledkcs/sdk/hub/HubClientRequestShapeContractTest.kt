package com.yuledkcs.sdk.hub

import android.content.Context
import android.content.SharedPreferences
import com.google.gson.JsonObject
import com.google.gson.JsonParser
import com.yuledkcs.sdk.device.DeviceManager
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.runTest
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Assert.*
import org.junit.Before
import org.junit.Test
import java.util.Base64

/**
 * Phase 4.1 / W3 — SDK×Hub 请求形状契约测试 (Android)
 *
 * 固化 SDK → Hub REST Gateway 的请求形状契约 (Hub 端由 protojson 强制执行):
 *   1. 枚举字段必须传 **枚举名字符串** (vendor="APPLE" / protocol="CCC_DK3"), 而非数字字符串
 *   2. 字段名必须 camelCase (vehicleId / deviceId / devicePubkey / keyType / traceId)
 *   3. devicePubkey 必须是 base64 编码的设备公钥
 *
 * 通过 MockWebServer 捕获真实 wire 字节 (与 HubClientTest 同一模式)。
 * 注意: 与既有 HubClientTest 不同, 本文件在 @Before 中用 runBlocking 创建
 * 客户端 (HubClient.create 是 suspend 函数, 不能在非挂起 setup 中直接调用),
 * 并对 DeviceManager 注入内存版 Context, 使 acceptShare 的 getDeviceId() 可在 JVM 运行。
 */
class HubClientRequestShapeContractTest {

    private lateinit var mockServer: MockWebServer
    private lateinit var client: HubClient

    @Before
    fun setup() {
        // JVM 单测环境: android.jar 为 mockable stub (方法抛 Stub! / 返回默认值),
        // 静态字段 (Build.MANUFACTURER) 可安全读取 → detectVendor() 返回兜底值。
        DeviceManager.init(FakeContext())

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

    /** 捕获下一个请求的 JSON body */
    private fun capturedBody(): JsonObject {
        val request = mockServer.takeRequest()
        return JsonParser.parseString(request.body.readUtf8()).asJsonObject
    }

    private fun enqueueKeyResponse(keyId: String) {
        mockServer.enqueue(
            MockResponse().setResponseCode(200)
                .setBody("""{"key":{"keyId":"$keyId","vehicleId":"VH-001"}}""")
        )
    }

    // ── bindKey: 枚举名字符串 + camelCase + base64 ─────────────────────────

    @Test
    fun `bindKey 请求体符合契约 - 枚举名 camelCase base64`() = runTest {
        enqueueKeyResponse("key-001")

        client.bindKey("VH-001", deviceId = "DEV-001", devicePubkey = "dGVzdC1iNjQtcHViLWtleS1jb250cmFjdA==")

        val json = capturedBody()

        // camelCase 字段名 (与 hub.proto BindKeyRequest 的 protojson 字段名一致)
        assertEquals(
            setOf("vehicleId", "deviceId", "devicePubkey", "vendor", "protocol", "keyType", "traceId"),
            json.keySet()
        )

        // 枚举名字符串 (protojson 拒绝数字字符串)
        assertEquals("OWNER", json.get("keyType").asString)
        val vendor = json.get("vendor").asString
        val protocol = json.get("protocol").asString
        assertTrue(
            "vendor 必须是枚举名字符串, 实际: $vendor",
            setOf("APPLE", "SAMSUNG", "XIAOMI", "OPPO", "VIVO", "HUAWEI").contains(vendor)
        )
        assertTrue(
            "protocol 必须是枚举名字符串, 实际: $protocol",
            setOf("CCC_DK3", "ICCOA_DK40", "ICCE").contains(protocol)
        )
        assertFalse("vendor 不得是数字字符串: $vendor", vendor.all { it.isDigit() })
        assertFalse("protocol 不得是数字字符串: $protocol", protocol.all { it.isDigit() })

        // devicePubkey: 合法 base64
        val pubkeyB64 = json.get("devicePubkey").asString
        assertNotNull("devicePubkey 必须是合法 base64: $pubkeyB64", Base64.getDecoder().decode(pubkeyB64))
        assertTrue(Base64.getDecoder().decode(pubkeyB64).isNotEmpty())
    }

    // ── acceptShare: shareCode + camelCase + 枚举名 + base64 ────────────────

    @Test
    fun `acceptShare 请求体符合契约`() = runTest {
        enqueueKeyResponse("key-002")

        client.acceptShare("123456")

        val json = capturedBody()

        assertEquals("123456", json.get("shareCode").asString)
        assertTrue(json.has("deviceId"))
        assertTrue(json.has("devicePubkey"))
        assertTrue(json.has("vendor"))
        assertTrue(json.has("traceId"))
        val vendor = json.get("vendor").asString
        assertTrue(
            "vendor 必须是枚举名字符串, 实际: $vendor",
            setOf("APPLE", "SAMSUNG", "XIAOMI", "OPPO", "VIVO", "HUAWEI").contains(vendor)
        )
        val pubkeyB64 = json.get("devicePubkey").asString
        assertNotNull("devicePubkey 必须是合法 base64", Base64.getDecoder().decode(pubkeyB64))
    }

    // ── createShare: camelCase ──────────────────────────────────────────────

    @Test
    fun `createShare 请求体 camelCase`() = runTest {
        mockServer.enqueue(
            MockResponse().setResponseCode(200)
                .setBody("""{"shareId":"sh-001","shareCode":"654321","keyId":"key-001"}""")
        )

        client.createShare("key-001", toVendor = "APPLE", toUserId = "friend-1")

        val json = capturedBody()
        assertEquals(
            setOf("keyId", "toVendor", "toUserId", "validFrom", "validUntil", "maxUses", "traceId"),
            json.keySet()
        )
        assertEquals("APPLE", json.get("toVendor").asString)
        assertEquals(0L, json.get("validFrom").asLong)
        assertEquals(0, json.get("maxUses").asInt)
    }

    // ── 枚举名与 hub.proto 对齐 ────────────────────────────────────────────

    @Test
    fun `枚举 protoName 与 hub.proto 一致`() {
        assertEquals("APPLE", PhoneVendor.APPLE.protoName)
        assertEquals("CCC_DK3", DigitalKeyProtocol.CCC_DK3.protoName)
        assertEquals("ICCOA_DK40", DigitalKeyProtocol.ICCOA_DK40.protoName)
        assertEquals("ICCE", DigitalKeyProtocol.ICCE.protoName)
    }
}

// ── JVM 测试用内存版 Context/SharedPreferences ──────────────────────────────
// AGP 单测环境的 mockable android.jar 中 Context 为具体类 (方法抛 Stub!),
// 仅需覆写被 DeviceManager 调用的方法即可, 无需 Robolectric。

private class FakeContext : Context() {
    private val prefs = FakeSharedPreferences()

    override fun getSharedPreferences(name: String?, mode: Int): SharedPreferences = prefs
    override fun getApplicationContext(): Context = this
    override fun getPackageName(): String = "com.yuledkcs.sdk.test"
}

private class FakeSharedPreferences : SharedPreferences {
    private val map = mutableMapOf<String, Any?>()

    override fun getString(key: String?, defValue: String?): String? =
        map[key] as? String ?: defValue

    override fun getAll(): MutableMap<String, *> = map.toMutableMap()
    override fun getInt(key: String?, defValue: Int): Int = map[key] as? Int ?: defValue
    override fun getLong(key: String?, defValue: Long): Long = map[key] as? Long ?: defValue
    override fun getFloat(key: String?, defValue: Float): Float = map[key] as? Float ?: defValue
    override fun getBoolean(key: String?, defValue: Boolean): Boolean = map[key] as? Boolean ?: defValue
    override fun getStringSet(key: String?, defValues: MutableSet<String>?): MutableSet<String>? =
        map[key] as? MutableSet<String> ?: defValues

    override fun contains(key: String?): Boolean = map.containsKey(key)
    override fun edit(): SharedPreferences.Editor = FakeEditor()
    override fun registerOnSharedPreferenceChangeListener(listener: SharedPreferences.OnSharedPreferenceChangeListener?) {}
    override fun unregisterOnSharedPreferenceChangeListener(listener: SharedPreferences.OnSharedPreferenceChangeListener?) {}

    private inner class FakeEditor : SharedPreferences.Editor {
        override fun putString(key: String?, value: String?): SharedPreferences.Editor {
            map[key] = value; return this
        }
        override fun putInt(key: String?, value: Int): SharedPreferences.Editor {
            map[key] = value; return this
        }
        override fun putLong(key: String?, value: Long): SharedPreferences.Editor {
            map[key] = value; return this
        }
        override fun putFloat(key: String?, value: Float): SharedPreferences.Editor {
            map[key] = value; return this
        }
        override fun putBoolean(key: String?, value: Boolean): SharedPreferences.Editor {
            map[key] = value; return this
        }
        override fun putStringSet(key: String?, values: MutableSet<String>?): SharedPreferences.Editor {
            map[key] = values; return this
        }
        override fun remove(key: String?): SharedPreferences.Editor {
            map.remove(key); return this
        }
        override fun clear(): SharedPreferences.Editor {
            map.clear(); return this
        }
        override fun commit(): Boolean = true
        override fun apply() {}
    }
}
