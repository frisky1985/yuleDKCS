# yuleDKCS SDK Phase 2a — HubClient 实现计划

> **平台**: iOS (Swift) + Android (Kotlin)  
> **通信**: HTTP/JSON → yuleDKCS Hub REST Gateway (:8080)  
> **Proto**: api/sdk/v1/sdk.proto（仅作为 JSON 字段名和类型的合约参考）  
> **工时**: ~3 天 (双平台并行)

---

## 架构决策

```
手机 SDK                              Hub
URLSession/OkHttp ──POST JSON──→ REST Gateway (:8080)
                  ←──JSON/SSE────
```

- **不生成 gRPC stubs** — gRPC-Swift 2.x 强制 iOS 18+，不现实
- **URLSession (iOS) / OkHttp (Android)** — 零额外依赖
- **JSON 字段名与 proto 一致** — REST Gateway 用 `protojson` 序列化，字段名 camelCase
- **StreamStatus → SSE** — 标准 Server-Sent Events
- **proto 仅作为接口合约** — 不用于代码生成，只供参考字段名和类型

---

## 1. REST 端点一览

Base URL: `https://<hub-endpoint>:8080`

| 方法 | 路径 | SDK 接口 |
|:----|:-----|:---------|
| POST | `/api/v1/keys` | `bindKey(vehicleId)` |
| DELETE | `/api/v1/keys/:keyId` | `unbindKey(keyId)` |
| PUT | `/api/v1/keys/:keyId/suspend` | `suspendKey(keyId)` |
| PUT | `/api/v1/keys/:keyId/resume` | `resumeKey(keyId)` |
| PUT | `/api/v1/keys/:keyId/revoke` | `revokeKey(keyId)` |
| PUT | `/api/v1/keys/:keyId/renew` | `renewKey(keyId, validUntil)` |
| GET | `/api/v1/keys/:keyId` | `getKey(keyId)` |
| GET | `/api/v1/keys` | `listKeys(vehicleId?, status?)` |
| POST | `/api/v1/shares` | `createShare(keyId, toVendor, ...)` |
| POST | `/api/v1/shares/accept` | `acceptShare(shareCode)` |
| DELETE | `/api/v1/shares/:shareId` | `cancelShare(shareId)` |
| GET | `/api/v1/shares/:shareId` | `getShare(shareId)` |
| POST | `/api/v1/vehicles/:vehicleId/command` | `sendCommand(vehicleId, action)` |
| GET | `/api/v1/vehicles/:vehicleId/status` | `streamStatus(vehicleId)` → SSE |

## 1.5 额外端点（Token + 设备管理）

SDK 内部也用到这些：

| 方法 | 路径 | 用途 |
|:----|:-----|:-----|
| POST | `/api/v1/tokens` | issueToken |
| GET | `/api/v1/tokens/:tokenId` | verifyToken |
| DELETE | `/api/v1/tokens/:tokenId` | revokeToken |
| POST | `/api/v1/devices` | registerDevice（SDK 自动调用） |
| GET | `/api/v1/devices` | listDevices |
| DELETE | `/api/v1/devices/:deviceId` | deleteDevice |

---

## 2. 项目结构

### iOS (Swift Package Manager)

```
mobile/ios/
├── Package.swift
├── Sources/
│   ├── YDKHubClient/
│   │   ├── YDKHubClient.swift              # 公开入口 + HTTP 客户端
│   │   ├── YDKHubClient+Keys.swift         # BindKey / UnbindKey / ListKeys / GetKey
│   │   ├── YDKHubClient+Share.swift        # CreateShare / AcceptShare / CancelShare
│   │   ├── YDKHubClient+Remote.swift       # RemoteLock / Unlock / Start / Stop
│   │   ├── YDKHubClient+Stream.swift       # SSE 车辆状态流
│   │   ├── YDKHubClient+Token.swift         # Token 管理
│   │   ├── YDKHubClient+Device.swift        # 设备注册/管理
│   │   ├── YDKHubClient+Push.swift          # Push 通知解析 + 回调
│   │   ├── YDKHubClient+Error.swift         # 错误类型映射
│   │   └── YDKHubClient+Config.swift        # SDKConfig 解析
│   └── YDKProto/                           # proto 生成的消息类型（可选参考，不依赖）
└── Tests/
    └── YDKHubClientTests/
```

### Android (Kotlin)

```
mobile/android/
├── build.gradle.kts
├── settings.gradle.kts
└── sdk/
    ├── build.gradle.kts
    └── src/main/kotlin/com/yuledkcs/sdk/
        ├── hub/
        │   ├── HubClient.kt               # 公开入口 + HTTP 客户端
        │   ├── HubClient+Keys.kt          # BindKey / UnbindKey / ListKeys / GetKey
        │   ├── HubClient+Share.kt         # CreateShare / AcceptShare / CancelShare
        │   ├── HubClient+Remote.kt        # RemoteLock / Unlock / Start / Stop
        │   ├── HubClient+Stream.kt        # SSE 车辆状态流
        │   ├── HubClient+Token.kt          # Token 管理
        │   ├── HubClient+Device.kt         # 设备注册/管理
        │   ├── HubClient+Push.kt          # Push 通知解析 + 回调
        │   ├── HubClient+Error.kt         # 错误类型映射
        │   └── HubClient+Config.kt        # SDKConfig 解析
        └── util/
            ├── Logger.kt
            └── TLS.kt
```

---

## 3. 核心实现

### 3.1 HubClient 初始化

```swift
// iOS: 公开入口
public class YDKHubClient {
    private let baseURL: URL
    private let session: URLSession
    private let config: SDKConfig
    private let logger: YDKLogger
    private var token: String?

    public init(config: SDKConfig) throws {
        self.config = config
        self.logger = YDKLogger(enabled: config.enableLogging)
        self.baseURL = URL(string: "https://\(config.hubEndpoint):8080/api/v1")!
        let delegate = YDKURLDelegate()

        // TLS 配置（证书固定 / Pinning）
        self.session = URLSession(configuration: .ephemeral, delegate: delegate, delegateQueue: nil)
    }

    // App 登录成功后调用
    public func setToken(_ token: String) {
        self.token = token
    }
}
```

```kotlin
// Android: 公开入口
class HubClient private constructor(
    private val baseURL: String,
    private val okHttpClient: OkHttpClient,
    private val config: SDKConfig,
    private val logger: Logger
) {
    private var token: String? = null

    companion object {
        suspend fun create(config: SDKConfig): HubClient {
            val client = OkHttpClient.Builder()
                .connectTimeout(10, TimeUnit.SECONDS)
                .readTimeout(30, TimeUnit.SECONDS)
                .addInterceptor { chain ->
                    val request = chain.request()
                    val builder = request.newBuilder()
                        .addHeader("Content-Type", "application/json")
                        .addHeader("Accept", "application/json")
                    token?.let {
                        builder.addHeader("Authorization", "Bearer $it")
                    }
                    chain.proceed(builder.build())
                }
                .build()
            return HubClient("https://${config.hubEndpoint}:8080/api/v1", client, config, Logger(config.enableLogging))
        }
    }

    fun setToken(token: String) { this.token = token }
}
```

### 3.2 HTTP 请求封装

核心原则：每个请求自动注入 `Authorization: Bearer <token>` + 公共 headers，统一处理错误映射。

```swift
// iOS: 通用请求方法
extension YDKHubClient {
    private func request<T: Decodable>(
        method: String,
        path: String,
        body: Encodable? = nil,
        query: [String: String]? = nil
    ) async throws -> T {
        var components = URLComponents(url: baseURL.appendingPathComponent(path),
                                     resolvingAgainstBaseURL: false)!
        if let query = query {
            components.queryItems = query.map { URLQueryItem(name: $0.key, value: $0.value) }
        }

        var req = URLRequest(url: components.url!)
        req.httpMethod = method
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        req.setValue("application/json", forHTTPHeaderField: "Accept")
        req.setValue(SDKVersion, forHTTPHeaderField: "X-SDK-Version")
        req.setValue("ios", forHTTPHeaderField: "X-Platform")

        if let token = token {
            req.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")
        }

        if let body = body {
            req.httpBody = try JSONEncoder().encode(body)
        }

        let (data, response) = try await session.data(for: req)

        guard let httpResponse = response as? HTTPURLResponse else {
            throw YDKError.networkError("invalid response")
        }

        // 错误映射
        if httpResponse.statusCode >= 400 {
            if let errorBody = try? JSONDecoder().decode(HubErrorResponse.self, from: data) {
                throw YDKError.hubError(errorBody.code ?? "", errorBody.message ?? "")
            }
            throw YDKError.httpError(httpResponse.statusCode)
        }

        return try JSONDecoder().decode(T.self, from: data)
    }
}
```

```kotlin
// Android: 通用请求方法
private suspend inline fun <reified T> request(
    method: String,
    path: String,
    body: Any? = null,
    query: Map<String, String>? = null
): T = withContext(Dispatchers.IO) {
    val urlBuilder = URL("$baseURL$path").toURI().toURL().toHttpUrlOrNull()!!.newBuilder()
    query?.forEach { (k, v) -> urlBuilder.addQueryParameter(k, v) }

    val bodyJson = body?.let { Gson().toJson(it) }
    val request = Request.Builder()
        .url(urlBuilder.build())
        .method(method, bodyJson?.let { it.toRequestBody("application/json".toMediaType()) })
        .addHeader("Accept", "application/json")
        .addHeader("X-SDK-Version", BuildConfig.SDK_VERSION)
        .addHeader("X-Platform", "android")
        .build()

    val response = okHttpClient.newCall(request).await()

    if (!response.isSuccessful) {
        val errorBody = response.body?.string()
        val hubError = Gson().fromJson(errorBody, HubErrorResponse::class.java)
        throw if (hubError?.code != null) {
            YDKError.HubError(hubError.code, hubError.message ?: "")
        } else {
            YDKError.HttpError(response.code)
        }
    }

    Gson().fromJson(response.body?.string(), T::class.java)
}
```

### 3.3 业务接口实现

```swift
// iOS: BindKey
extension YDKHubClient {
    public func bindKey(vehicleId: String) async throws -> YDKKey {
        let body: [String: Any] = [
            "vehicleId": vehicleId,
            "deviceId": await deviceManager.getDeviceId(),
            "devicePubkey": (await seManager.readPublicKey()).base64EncodedString(),
            "protocol": detectProtocol().rawValue,
            "keyType": "OWNER"
        ]
        return try await request(method: "POST", path: "/keys", body: body)
    }
}
```

```kotlin
// Android: BindKey
suspend fun bindKey(vehicleId: String): YDKKey {
    val body = mapOf(
        "vehicleId" to vehicleId,
        "deviceId" to deviceManager.getDeviceId(),
        "devicePubkey" to seManager.readPublicKey().encodeToBase64(),
        "protocol" to detectProtocol().name,
        "keyType" to "OWNER"
    )
    return request("POST", "/keys", body)
}
```

### 3.4 完整接口签名

```swift
// iOS 公开接口
public func bindKey(vehicleId: String) async throws -> YDKKey
public func unbindKey(keyId: String) async throws
public func suspendKey(keyId: String, reason: String?) async throws
public func resumeKey(keyId: String) async throws
public func revokeKey(keyId: String, reason: String?) async throws
public func renewKey(keyId: String, validUntil: Int64) async throws
public func listKeys(vehicleId: String?, status: KeyFilter?) async throws -> [YDKKey]
public func getKey(keyId: String) async throws -> YDKKey

public func createShare(keyId: String, toVendor: PhoneVendor, toUserId: String?,
                       validFrom: Int64?, validUntil: Int64?) async throws -> YDKShare
public func acceptShare(shareCode: String) async throws -> YDKKey
public func cancelShare(shareId: String) async throws
public func getShare(shareId: String) async throws -> YDKShare

public func sendCommand(vehicleId: String, action: VehicleAction) async throws
public func streamStatus(vehicleId: String) -> AsyncThrowingStream<VehicleStatusUpdate, Error>

public func handlePushNotification(_ payload: Data) -> Bool
```

```kotlin
// Android 公开接口
suspend fun bindKey(vehicleId: String): YDKKey
suspend fun unbindKey(keyId: String)
suspend fun suspendKey(keyId: String, reason: String? = null)
suspend fun resumeKey(keyId: String)
suspend fun revokeKey(keyId: String, reason: String? = null)
suspend fun renewKey(keyId: String, validUntil: Long)
suspend fun listKeys(vehicleId: String? = null, status: KeyFilter? = null): List<YDKKey>
suspend fun getKey(keyId: String): YDKKey

suspend fun createShare(keyId: String, toVendor: PhoneVendor, toUserId: String? = null,
                       validFrom: Long? = null, validUntil: Long? = null): YDKShare
suspend fun acceptShare(shareCode: String): YDKKey
suspend fun cancelShare(shareId: String)
suspend fun getShare(shareId: String): YDKShare

suspend fun sendCommand(vehicleId: String, action: VehicleAction)
fun streamStatus(vehicleId: String): Flow<VehicleStatusUpdate>

fun handlePushNotification(payload: ByteArray): Boolean
```

### 3.5 SSE 流实现（StreamStatus）

```swift
// iOS: SSE 流
public func streamStatus(vehicleId: String) -> AsyncThrowingStream<VehicleStatusUpdate, Error> {
    AsyncThrowingStream { continuation in
        Task {
            var req = URLRequest(url: baseURL.appendingPathComponent("/vehicles/\(vehicleId)/status"))
            req.setValue("text/event-stream", forHTTPHeaderField: "Accept")
            req.setValue("Bearer \(token ?? "")", forHTTPHeaderField: "Authorization")

            let (bytes, response) = try await session.bytes(for: req)

            var currentData = Data()
            for try await line in bytes.lines {
                if line.hasPrefix("data: ") {
                    let json = String(line.dropFirst(6))
                    if let update = try? JSONDecoder().decode(VehicleStatusUpdate.self, from: Data(json.utf8)) {
                        continuation.yield(update)
                    }
                }
            }
            continuation.finish()
        }
    }
}
```

```kotlin
// Android: SSE 流 (使用 OkHttp EventSource)
fun streamStatus(vehicleId: String): Flow<VehicleStatusUpdate> = callbackFlow {
    val request = Request.Builder()
        .url("$baseURL/vehicles/$vehicleId/status")
        .addHeader("Accept", "text/event-stream")
        .addHeader("Authorization", "Bearer $token")
        .build()

    val eventSource = EventSources.createFactory(okHttpClient)
        .newEventSource(request, object : EventSourceListener() {
            override fun onEvent(eventSource: EventSource, id: String?, type: String?, data: String) {
                val update = Gson().fromJson(data, VehicleStatusUpdate::class.java)
                trySend(update)
            }
        })

    awaitClose { eventSource.close() }
}
```

---

## 4. 错误处理

```swift
// iOS: 错误类型
public enum YDKError: Error {
    case notInitialized
    case notAuthenticated             // token 未设置
    case hubError(String, String)     // Hub 返回的业务错误 (errorCode, errorMsg)
    case httpError(Int)               // HTTP 状态码错误 (4xx/5xx)
    case networkError(String)         // 网络不可达/TLS 失败
    case timeout                      // 请求超时
    case decodingFailed(String)       // JSON 解析失败
    case internal(String)             // SDK 内部错误

    var localizedDescription: String { ... }
}
```

```kotlin
// Android: 错误类型
sealed class YDKError : Exception() {
    object NotInitialized : YDKError()
    object NotAuthenticated : YDKError()
    data class HubError(val code: String, val msg: String) : YDKError()
    data class HttpError(val statusCode: Int) : YDKError()
    data class NetworkError(val msg: String) : YDKError()
    object Timeout : YDKError()
    data class DecodingFailed(val detail: String) : YDKError()
    data class Internal(val msg: String) : YDKError()
}
```

### REST Gateway 错误 JSON 格式

```json
{
    "error": "INVALID_ARGUMENT",
    "message": "vehicle_id is required",
    "details": []
}
```

SDK 解析此格式后统一转换为 `YDKError.hubError(code, message)`。

---

## 5. 测试方案

### 5.1 单元测试

```swift
// iOS: URLProtocol mock
class MockURLProtocol: URLProtocol {
    static var responseHandler: ((URLRequest) throws -> (HTTPURLResponse, Data))?

    override class func canInit(with request: URLRequest) -> Bool { true }
    override class func canonicalRequest(for request: URLRequest) -> URLRequest { request }
    override func startLoading() {
        // 用 MockURLProtocol.responseHandler 返回模拟数据
    }
    override func stopLoading() {}
}
```

```kotlin
// Android: MockWebServer (OkHttp)
// 依赖: com.squareup.okhttp3:mockwebserver
class HubClientTest {
    private val mockServer = MockWebServer()

    @Test
    fun `bindKey returns key from Hub`() = runTest {
        mockServer.enqueue(MockResponse()
            .setResponseCode(200)
            .setBody("""{"keyId":"key-001","vehicleId":"VH-001"}"""))

        val result = client.bindKey("VH-001")
        assertEquals("key-001", result.keyId)
    }
}
```

### 5.2 集成测试

直接调现有的 yuleDKCS Hub REST Gateway（本地 `:8080`），验证完整通断。

---

## 6. 排期

| 天 | iOS | Android | 可并行？ |
|:-:|:----|:--------|:--------:|
| 1 | HTTP 客户端封装 + Auth + 错误映射 + 单元测试基础设施 | 同 iOS | ✅ |
| 2 | Keys + Share 接口 + 单元测试 | 同 iOS | ✅ |
| 3 | Remote + Stream(SSE) + Push + 集成测试 | 同 iOS | ✅ |

**总计: 3 天**（双平台并行，各自 1.5 人天）

与 gRPC 方案相比减少 1 天，原因：
- 无 gRPC 通道初始化/TLS 配置复杂度
- 无 proto 代码生成步骤
- 无 gRPC 拦截器 → 简化为 HTTP header
- SSE 比 gRPC streaming 更简单

---

## 7. 依赖项

### iOS (Package.swift)

```swift
// 零外部依赖
// 使用 Foundation 原生的 URLSession
// 最低 iOS 版本: iOS 15.0
let package = Package(
    name: "yuleDKCS-SDK",
    platforms: [.iOS(.v15)],
    products: [
        .library(name: "YDKHubClient", targets: ["YDKHubClient"]),
    ],
    targets: [
        .target(name: "YDKHubClient"),
        .testTarget(name: "YDKHubClientTests", dependencies: ["YDKHubClient"]),
    ]
)
```

### Android (build.gradle.kts)

```kotlin
dependencies {
    // HTTP 客户端
    implementation("com.squareup.okhttp3:okhttp:4.12.0")
    // JSON 序列化
    implementation("com.google.code.gson:gson:2.11.0")
    // 协程
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.9.0")

    // 测试
    testImplementation("com.squareup.okhttp3:mockwebserver:4.12.0")
    testImplementation("org.jetbrains.kotlinx:kotlinx-coroutines-test:1.9.0")
}
```

最小 Android SDK: **Android 8.0 (API 26)**

---

## 8. 与 gRPC 方案差异总结

| 维度 | gRPC 方案 (旧) | HTTP/JSON 方案 (新) | 原因 |
|:-----|:--------------|:-------------------|:-----|
| 传输协议 | gRPC (:9090) | HTTP/REST (:8080) | gRPC-Swift 2.x 强制 iOS 18+ |
| 序列化 | protobuf binary | JSON | protojson 自动 JSON↔proto |
| 流式接口 | gRPC Server-Stream | SSE (text/event-stream) | SSE 无需额外依赖 |
| Auth | gRPC Interceptor / CallCredentials | HTTP Header `Authorization: Bearer` | 更简单 |
| 代码生成 | protoc → Swift/Kotlin stubs | 无（JSON 字段名手动对齐 proto） | 减少构建复杂度 |
| iOS 最低版本 | iOS 18+ (grpc-swift 2.x) | iOS 15+ | 关键差异 |
| 外部依赖 | grpc-swift, grpc-kotlin, protobuf | URLSession (内建), OkHttp | 零/极少 |
| 调试 | 需 gRPC tools (grpcurl) | curl / Postman 直接可用 | 开发者友好 |
