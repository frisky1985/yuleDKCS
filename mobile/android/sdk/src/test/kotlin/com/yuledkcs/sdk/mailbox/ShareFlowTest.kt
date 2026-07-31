package com.yuledkcs.sdk.mailbox

import com.google.gson.JsonObject
import com.google.gson.JsonParser
import com.yuledkcs.sdk.hub.YDKError
import kotlinx.coroutines.test.runTest
import okhttp3.OkHttpClient
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import okhttp3.mockwebserver.RecordedRequest
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import java.security.SecureRandom
import java.security.cert.X509Certificate
import java.util.Base64
import java.util.concurrent.TimeUnit
import javax.net.ssl.SSLContext
import javax.net.ssl.SSLSocketFactory
import javax.net.ssl.TrustManager
import javax.net.ssl.X509TrustManager

/**
 * Phase 4.4 / W2 — CCC 分享高层编排层测试（Android）
 *
 * 覆盖 PHASE4-4-SHARE-FLOW-CONTRACT.md:
 *  - B1 Sender 分享流（B1.1 调 CreateMailbox 返回分享 URL / B1.2 URL 格式 / B1.3 失败抛错不生成 URL）
 *  - B2 Receiver 接受流（B2.1 display→content 读取顺序 / B2.2 KEY_SIGNING→IMPORT 更新顺序 /
 *                       B2.3 非法 URL 抛错 / B2.4 content 读取失败流程中止）
 *  - B3 取消流（B3.1 SENDER_CANCEL=4 / B3.2 RECEIVER_CANCEL=5）
 *  - B4 数据模型（B4.1 枚举值与 relay.proto 一致 / B4.2 请求形状 camelCase + base64）
 *
 * 通过 MockWebServer(HTTPS) 捕获真实 wire 请求，验证调用顺序与请求形状
 * （MailboxClient 构造器新增 OkHttpClient 注入缝）。
 */
class ShareFlowTest {

    private lateinit var mockServer: MockWebServer
    private lateinit var client: MailboxClient

    @Before
    fun setup() {
        val ssl = testSslContext()
        mockServer = MockWebServer()
        mockServer.useHttps(ssl.socketFactory, false)
        mockServer.start()
        client = MailboxClient(
            hubEndpoint = mockServer.hostName,
            port = mockServer.port,
            client = OkHttpClient.Builder()
                .sslSocketFactory(ssl.socketFactory, ssl.trustManager)
                .hostnameVerifier { _, _ -> true }
                .build()
        )
    }

    @After
    fun tearDown() {
        mockServer.shutdown()
    }

    // ─── 测试辅助 ─────────────────────────────────────────

    private fun enqueue(code: Int, body: String) {
        mockServer.enqueue(MockResponse().setResponseCode(code).setBody(body))
    }

    /** 取下一个请求；超时未收到则直接失败（happy path 用） */
    private fun nextRequest(timeoutMs: Long = 5000): RecordedRequest =
        mockServer.takeRequest(timeoutMs, TimeUnit.MILLISECONDS)
            ?: throw AssertionError("未收到预期请求（timeout=${timeoutMs}ms）")

    /** 断言没有更多请求（错误路径/流程中止用） */
    private fun assertNoMoreRequests() {
        assertNull("不应有更多请求", mockServer.takeRequest(300, TimeUnit.MILLISECONDS))
    }

    private fun b64(s: String) = Base64.getEncoder().encodeToString(s.toByteArray())

    private fun bodyJson(request: RecordedRequest): JsonObject =
        JsonParser.parseString(request.body.readUtf8()).asJsonObject

    private fun mockServerUrl(mailboxId: String = "mb-1", secret: String = "s3cr3t") =
        "https://${mockServer.hostName}:${mockServer.port}/api/v1/mailbox/$mailboxId#$secret"

    // ─── B1.2 / AC2: URL 生成与解析 ───────────────────────

    @Test
    fun `buildSharingURL 生成规范格式 URL`() {
        val url = buildSharingURL("hub.yuletech.com:8080", "mb-123", "s3cr3t")
        assertEquals("https://hub.yuletech.com:8080/api/v1/mailbox/mb-123#s3cr3t", url)
    }

    @Test
    fun `buildSharingURL 与 parseSharingURL 往返一致`() {
        val url = buildSharingURL("hub.yuletech.com:8080", "mb-123", "s3cr3t")
        val info = MailboxClient.parseSharingURL(url)
        assertNotNull(info)
        assertEquals("mb-123", info!!.mailboxId)
        assertEquals("s3cr3t", info.secret)
    }

    @Test
    fun `parseSharingURL 支持 secret 前缀与空 fragment 变体`() {
        // #secret=xxx 变体（双端兼容）
        val prefixed = MailboxClient.parseSharingURL(
            "https://hub.yuletech.com:8080/api/v1/mailbox/mb-9#secret=abc123"
        )
        assertEquals("abc123", prefixed!!.secret)

        // 无 fragment → secret 为空（由接受流校验拒绝）
        val noFragment = MailboxClient.parseSharingURL(
            "https://hub.yuletech.com:8080/api/v1/mailbox/mb-9"
        )
        assertEquals("mb-9", noFragment!!.mailboxId)
        assertEquals("", noFragment.secret)

        // 路径不含 /mailbox/ → null
        assertNull(MailboxClient.parseSharingURL("https://hub.yuletech.com/foo/bar"))
        assertNull(MailboxClient.parseSharingURL("not-a-url"))
    }

    // ─── B1: Sender 分享流 ────────────────────────────────

    @Test
    fun `shareKeyViaMailbox 调 CreateMailbox 并返回服务端分享 URL`() = runTest {
        val sharingUrl = "https://hub.yuletech.com:8080/api/v1/mailbox/mb-1#s3cr3t"
        enqueue(200, """{"mailboxId":"mb-1","sharingUrl":"$sharingUrl","expiresAt":1789000000000}""")

        val url = client.shareKeyViaMailbox(
            payload = "key-creation".toByteArray(),
            displayInfo = "display-info".toByteArray(),
            senderVendor = "XIAOMI",
            senderDeviceId = "dev-sender-1",
            host = "hub.yuletech.com:8080"
        )

        // B1.1/B1.2: 直接使用服务端 sharingUrl（含 secret fragment）
        assertEquals(sharingUrl, url)

        // B4.2: 请求形状 camelCase + base64 payload
        val req = nextRequest()
        assertEquals("POST", req.method)
        assertEquals("/api/v1/mailbox", req.path)
        val body = bodyJson(req)
        assertEquals(
            setOf(
                "payload", "displayInfo", "senderVendor", "senderDeviceId",
                "expirationSeconds", "maxUpdates", "notificationToken",
                "deviceAttestation", "traceId"
            ),
            body.keySet()
        )
        assertEquals(b64("key-creation"), body.get("payload").asString)
        assertEquals(b64("display-info"), body.get("displayInfo").asString)
        assertEquals("XIAOMI", body.get("senderVendor").asString)
        assertEquals("dev-sender-1", body.get("senderDeviceId").asString)
        assertEquals(86400L, body.get("expirationSeconds").asLong)
        assertEquals(10, body.get("maxUpdates").asInt)
        assertNoMoreRequests()
    }

    @Test
    fun `shareKeyViaMailbox 服务端缺 sharingUrl 时用 host 兜底组装`() = runTest {
        // 服务端只返回 mailboxId（无 sharingUrl）→ 兜底组装，fragment 为空
        enqueue(200, """{"mailboxId":"mb-2","expiresAt":1789000000000}""")

        val url = client.shareKeyViaMailbox(
            payload = "p".toByteArray(),
            senderVendor = "APPLE",
            senderDeviceId = "dev-2",
            host = "hub.yuletech.com:8080"
        )

        assertEquals("https://hub.yuletech.com:8080/api/v1/mailbox/mb-2#", url)
    }

    @Test
    fun `shareKeyViaMailbox 创建失败抛错且不生成 URL`() = runTest {
        // B1.3: CreateMailbox 失败 → 抛错，不生成 URL
        enqueue(500, """{"error":"INTERNAL","message":"relay unavailable"}""")

        var thrown = false
        try {
            client.shareKeyViaMailbox(
                payload = "p".toByteArray(),
                senderVendor = "APPLE",
                senderDeviceId = "dev-3",
                host = "hub.yuletech.com:8080"
            )
        } catch (e: YDKError) {
            thrown = true
        }
        assertTrue("CreateMailbox 失败应抛 YDKError", thrown)
        assertNotNull("CreateMailbox 请求应已发出", nextRequest(300))
        assertNoMoreRequests()
    }

    @Test
    fun `shareKeyViaMailbox 服务端 sharingUrl 形状非法时抛错`() = runTest {
        // 服务端返回的 sharingUrl 无法解析（AC4: 不静默返回垃圾 URL）
        enqueue(200, """{"mailboxId":"mb-1","sharingUrl":"not-a-url","expiresAt":1}""")

        var thrown = false
        try {
            client.shareKeyViaMailbox(
                payload = "p".toByteArray(),
                senderVendor = "APPLE",
                senderDeviceId = "dev-3",
                host = "hub.yuletech.com:8080"
            )
        } catch (e: YDKError.Internal) {
            thrown = true
        }
        assertTrue("非法 sharingUrl 应抛 YDKError.Internal", thrown)
    }

    // ─── B2: Receiver 接受流 ──────────────────────────────

    @Test
    fun `acceptSharedKeyViaMailbox 按规范顺序编排并返回 content`() = runTest {
        // B2.1: display → content；B2.2: KEY_SIGNING → IMPORT
        enqueue(200, """{"displayInfo":"${b64("display")}","version":1}""")
        enqueue(200, """{"payload":"${b64("secure-content")}","version":1}""")
        enqueue(200, """{"status":"OK","version":2}""")
        enqueue(200, """{"status":"OK","version":3}""")

        val content = client.acceptSharedKeyViaMailbox(
            urlString = mockServerUrl("mb-1", "s3cr3t"),
            updaterDeviceId = "dev-receiver-1",
            keySigningPayload = "key-signing-req".toByteArray(),
            importPayload = "import-req".toByteArray()
        )

        // 返回 readSecureContent 的载荷（base64 反序列化回字节）
        assertNotNull(content.payload)
        assertArrayEquals("secure-content".toByteArray(), content.payload!!)
        assertEquals(1L, content.version)

        // ① GET display
        val display = nextRequest()
        assertEquals("GET", display.method)
        assertEquals("/api/v1/mailbox/mb-1/display", display.path)

        // ② GET content
        val read = nextRequest()
        assertEquals("GET", read.method)
        assertEquals("/api/v1/mailbox/mb-1/content", read.path)

        // ③ PUT KEY_SIGNING(=2)
        val signing = nextRequest()
        assertEquals("PUT", signing.method)
        assertEquals("/api/v1/mailbox/mb-1", signing.path)
        val signingBody = bodyJson(signing)
        assertEquals(2, signingBody.get("sharingDataType").asInt)
        assertEquals(b64("key-signing-req"), signingBody.get("payload").asString)
        assertEquals("dev-receiver-1", signingBody.get("updaterDeviceId").asString)

        // ④ PUT IMPORT(=3)
        val import = nextRequest()
        assertEquals("PUT", import.method)
        assertEquals("/api/v1/mailbox/mb-1", import.path)
        val importBody = bodyJson(import)
        assertEquals(3, importBody.get("sharingDataType").asInt)
        assertEquals(b64("import-req"), importBody.get("payload").asString)
        assertEquals("dev-receiver-1", importBody.get("updaterDeviceId").asString)

        assertNoMoreRequests()
    }

    @Test
    fun `acceptSharedKeyViaMailbox 非法 URL 抛错且不发任何请求`() = runTest {
        // B2.3a: 无法解析 mailbox_id（路径不含 /mailbox/）
        var thrown = false
        try {
            client.acceptSharedKeyViaMailbox(
                urlString = "https://evil.example/not-a-mailbox#secret",
                updaterDeviceId = "dev-r",
                keySigningPayload = ByteArray(1),
                importPayload = ByteArray(1)
            )
        } catch (e: YDKError.Internal) {
            thrown = true
        }
        assertTrue("非法 URL 应抛 YDKError.Internal", thrown)
        assertNoMoreRequests()

        // 非 URL 字符串同样抛错
        thrown = false
        try {
            client.acceptSharedKeyViaMailbox(
                urlString = "not-a-url",
                updaterDeviceId = "dev-r",
                keySigningPayload = ByteArray(1),
                importPayload = ByteArray(1)
            )
        } catch (e: YDKError.Internal) {
            thrown = true
        }
        assertTrue("非 URL 字符串应抛 YDKError.Internal", thrown)
    }

    @Test
    fun `acceptSharedKeyViaMailbox 缺少 secret fragment 抛错`() = runTest {
        // B2.3b: 有 mailbox_id 但无 #secret → 抛错（接收方无法解密）
        var thrown = false
        try {
            client.acceptSharedKeyViaMailbox(
                urlString = "https://hub.yuletech.com:8080/api/v1/mailbox/mb-1",
                updaterDeviceId = "dev-r",
                keySigningPayload = ByteArray(1),
                importPayload = ByteArray(1)
            )
        } catch (e: YDKError.Internal) {
            thrown = true
        }
        assertTrue("缺 secret 应抛 YDKError.Internal", thrown)
        assertNoMoreRequests()
    }

    @Test
    fun `acceptSharedKeyViaMailbox content 读取失败中止流程`() = runTest {
        // B2.4: readSecureContent 失败 → 流程中止，不执行 KEY_SIGNING/IMPORT
        enqueue(200, """{"displayInfo":"${b64("display")}","version":1}""")
        enqueue(404, """{"error":"NOT_FOUND","message":"mailbox not found"}""")

        var thrown = false
        try {
            client.acceptSharedKeyViaMailbox(
                urlString = mockServerUrl("mb-1", "s3cr3t"),
                updaterDeviceId = "dev-r",
                keySigningPayload = ByteArray(1),
                importPayload = ByteArray(1)
            )
        } catch (e: YDKError) {
            thrown = true
        }
        assertTrue("content 读取失败应抛 YDKError", thrown)

        assertEquals("GET", nextRequest().method)   // display 已发出
        assertEquals("GET", nextRequest().method)   // content 已发出（失败）
        assertNoMoreRequests()                       // 失败后无 update 请求
    }

    @Test
    fun `acceptSharedKeyViaMailbox content 业务错误码中止流程`() = runTest {
        // HTTP 200 但 content 带 errorCode（如分享已被取消）→ 中止，不执行 update
        enqueue(200, """{"displayInfo":"${b64("display")}","version":1}""")
        enqueue(200, """{"payload":"${b64("content")}","errorCode":"SHARING_CANCELLED","errorMsg":"cancelled by sender"}""")

        var thrown = false
        try {
            client.acceptSharedKeyViaMailbox(
                urlString = mockServerUrl("mb-1", "s3cr3t"),
                updaterDeviceId = "dev-r",
                keySigningPayload = ByteArray(1),
                importPayload = ByteArray(1)
            )
        } catch (e: YDKError.Internal) {
            thrown = true
        }
        assertTrue("content 业务错误码应抛 YDKError.Internal", thrown)

        assertEquals("GET", nextRequest().method)   // display
        assertEquals("GET", nextRequest().method)   // content（带错误码）
        assertNoMoreRequests()                       // 无 update 请求
    }

    // ─── B3: 取消流 ───────────────────────────────────────

    @Test
    fun `cancelMailboxShare asSender 发送 SENDER_CANCEL=4`() = runTest {
        // B3.1: senderCancel → updateMailbox(SENDER_CANCEL)
        enqueue(200, """{"status":"OK","version":2}""")

        client.cancelMailboxShare("mb-1", asSender = true, updaterDeviceId = "dev-sender-1")

        val req = nextRequest()
        assertEquals("PUT", req.method)
        assertEquals("/api/v1/mailbox/mb-1", req.path)
        val body = bodyJson(req)
        assertEquals(4, body.get("sharingDataType").asInt)
        assertEquals("dev-sender-1", body.get("updaterDeviceId").asString)
        assertEquals("", body.get("payload").asString) // 取消信号无载荷
    }

    @Test
    fun `cancelMailboxShare asReceiver 发送 RECEIVER_CANCEL=5`() = runTest {
        // B3.2: receiverCancel → updateMailbox(RECEIVER_CANCEL)
        enqueue(200, """{"status":"OK","version":2}""")

        client.cancelMailboxShare("mb-1", asSender = false, updaterDeviceId = "dev-receiver-1")

        val body = bodyJson(nextRequest())
        assertEquals(5, body.get("sharingDataType").asInt)
        assertEquals("dev-receiver-1", body.get("updaterDeviceId").asString)
    }

    @Test
    fun `senderCancelMailboxShare 与 receiverCancelMailboxShare 别名正确映射`() = runTest {
        enqueue(200, """{"status":"OK"}""")
        enqueue(200, """{"status":"OK"}""")

        client.senderCancelMailboxShare("mb-1", updaterDeviceId = "dev-s")
        client.receiverCancelMailboxShare("mb-1", updaterDeviceId = "dev-r")

        assertEquals(4, bodyJson(nextRequest()).get("sharingDataType").asInt)
        assertEquals(5, bodyJson(nextRequest()).get("sharingDataType").asInt)
    }

    // ─── B4: 数据模型对齐 ─────────────────────────────────

    @Test
    fun `MailboxDataType 枚举值与 relay proto 一致`() {
        // B4.1: KEY_CREATION=1, KEY_SIGNING=2, IMPORT=3, SENDER_CANCEL=4, RECEIVER_CANCEL=5
        assertEquals(1, MailboxDataType.KEY_CREATION.value)
        assertEquals(2, MailboxDataType.KEY_SIGNING.value)
        assertEquals(3, MailboxDataType.IMPORT.value)
        assertEquals(4, MailboxDataType.SENDER_CANCEL.value)
        assertEquals(5, MailboxDataType.RECEIVER_CANCEL.value)
    }
}

// ─── HTTPS 测试缝（MockWebServer 自签名证书 + trust-all 客户端）─────

private data class TestSsl(val socketFactory: SSLSocketFactory, val trustManager: X509TrustManager)

private fun testSslContext(): TestSsl {
    val trustManager = object : X509TrustManager {
        override fun checkClientTrusted(chain: Array<out X509Certificate>?, authType: String?) {}
        override fun checkServerTrusted(chain: Array<out X509Certificate>?, authType: String?) {}
        override fun getAcceptedIssuers(): Array<X509Certificate> = arrayOf()
    }
    val sslContext = SSLContext.getInstance("TLS")
    sslContext.init(null, arrayOf<TrustManager>(trustManager), SecureRandom())
    return TestSsl(sslContext.socketFactory, trustManager)
}
