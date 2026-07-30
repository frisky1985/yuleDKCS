# yuleDKCS SDK Phase 2a — HubClient 实现计划

> **平台**: iOS (Swift) + Android (Kotlin)  
> **通信**: gRPC → yuleDKCS Hub (:9090)  
> **Proto**: api/sdk/v1/sdk.proto、api/v1/hub.proto  
> **工时**: ~4天 (双平台并行)

---

## 1. 项目结构

### iOS (Swift Package Manager)

```
yuleDKCS-SDK/
├── Package.swift
├── Sources/
│   ├── YDKHubClient/
│   │   ├── YDKHubClient.swift           # gRPC 连接管理 (公开入口)
│   │   ├── YDKHubClient+Auth.swift      # Token 注入 + gRPC metadata
│   │   ├── YDKHubClient+Keys.swift      # BindKey / UnbindKey / ListKeys / GetKey
│   │   ├── YDKHubClient+Share.swift     # CreateShare / AcceptShare / CancelShare
│   │   ├── YDKHubClient+Remote.swift    # RemoteLock / RemoteUnlock / RemoteStart
│   │   ├── YDKHubClient+Push.swift      # Push 通知解析 + 回调
│   │   ├── YDKHubClient+Error.swift     # 错误类型映射
│   │   └── YDKHubClient+Config.swift    # SDKConfig 解析
│   ├── YDKProto/                        # 由 proto 代码生成
│   │   ├── hub.pb.swift
│   │   ├── hub.grpc.swift
│   │   ├── relay.pb.swift
│   │   └── relay.grpc.swift
│   └── YDKUtils/
│       ├── YDKLogger.swift
│       └── YDKTLS.swift
└── Tests/
    └── YDKHubClientTests/
```

### Android (Kotlin)

```
yuleDKCS-SDK-Android/
├── build.gradle.kts
├── settings.gradle.kts
├── sdk/
│   ├── build.gradle.kts
│   └── src/main/kotlin/com/yuledkcs/sdk/
│       ├── hub/
│       │   ├── HubClient.kt             # gRPC 连接管理 (公开入口)
│       │   ├── HubClient+Auth.kt        # Token 注入 + gRPC metadata
│       │   ├── HubClient+Keys.kt        # BindKey / UnbindKey / ListKeys / GetKey
│       │   ├── HubClient+Share.kt       # CreateShare / AcceptShare / CancelShare
│       │   ├── HubClient+Remote.kt      # RemoteLock / RemoteUnlock / RemoteStart
│       │   ├── HubClient+Push.kt        # Push 通知解析 + 回调
│       │   ├── HubClient+Error.kt       # 错误类型映射
│       │   └── HubClient+Config.kt      # SDKConfig 解析
│       ├── proto/                        # 由 proto 代码生成
│       │   ├── HubProtoGrpcKt.kt
│       │   ├── HubProto.kt
│       │   └── relay/
│       └── util/
│           ├── Logger.kt
│           └── TLS.kt
└── sdk-test/
    ├── build.gradle.kts
    └── src/test/kotlin/com/yuledkcs/sdk/hub/
```

---

## 2. 代码生成

### 需要生成的 proto

| 文件 | 用途 |
|:-----|:------|
| `api/v1/hub.proto` | Hub 的 gRPC 服务定义 (KeyManagementService + KeyShareService + VehicleControlService) |
| `api/relay/v1/relay.proto` | CCC Relay Server 的 Mailbox API (Phase 2d 用, 生成本地 stub 备用) |
| `api/sdk/v1/sdk.proto` | SDK 接口合约 (参考, 不生成 server stub, 用于生成消息类型) |

### 生成命令

```bash
# iOS
protoc --swift_opt=Visibility=Public --swift_out=./Sources/YDKProto \
       --grpc-swift_opt=Visibility=Public --grpc-swift_out=./Sources/YDKProto \
       -I=../../api \
       ../../api/v1/hub.proto ../../api/relay/v1/relay.proto

# Android (Gradle 插件的 protobuf-gradle-plugin 自动生成)
# build.gradle.kts:
#   protobuf {
#       protoc { artifact = "com.google.protobuf:protoc:4.29.3" }
#       plugins {
#           id("grpc") { artifact = "io.grpc:protoc-gen-grpc-java:1.66.0" }
#           id("grpckt") { artifact = "io.grpc:protoc-gen-grpc-kotlin:1.4.1" }
#       }
#       generateProtoTasks { all().forEach { it.plugins { id("grpc") } } }
#   }
```

---

## 3. 核心实现

### 3.1 HubClient 初始化

```swift
// iOS: 公开入口
public class YDKHubClient {
    private let channel: GRPCChannel
    private var keyClient: KeyManagementServiceAsyncClient
    private var shareClient: KeyShareServiceAsyncClient
    private var vehicleClient: VehicleControlServiceAsyncClient
    private let config: SDKConfig
    private let logger: YDKLogger
    
    public init(config: SDKConfig) throws {
        self.config = config
        self.logger = YDKLogger(enabled: config.enableLogging)
        
        // TLS 通道
        let group = PlatformSupport.makeEventLoopGroup(loopCount: 1)
        let channel = try GRPCChannelPool.with(
            target: .host(config.hubEndpoint, port: 9090),
            transportSecurity: .tls(GRPCTLSConfiguration.makeClientDefault()),
            eventLoopGroup: group
        )
        self.channel = channel
        self.keyClient = KeyManagementServiceAsyncClient(channel: channel)
        self.shareClient = KeyShareServiceAsyncClient(channel: channel)
        self.vehicleClient = VehicleControlServiceAsyncClient(channel: channel)
    }
    
    // App 登录成功后调用
    public func setToken(_ token: String) {
        // 附加到每个 gRPC 调用的 metadata (见 3.2)
    }
}
```

```kotlin
// Android: 公开入口
class HubClient private constructor(
    private val channel: ManagedChannel,
    private val config: SDKConfig,
    private val logger: Logger
) {
    private val keyStub: KeyManagementServiceGrpcKt.KeyManagementServiceCoroutineStub
    private val shareStub: KeyShareServiceGrpcKt.KeyShareServiceCoroutineStub
    private val vehicleStub: VehicleControlServiceGrpcKt.VehicleControlServiceCoroutineStub
    
    companion object {
        suspend fun create(config: SDKConfig): HubClient {
            val channel = ManagedChannelBuilder
                .forAddress(config.hubEndpoint, 9090)
                .useTransportSecurity()       // TLS
                .keepAliveTime(30, TimeUnit.SECONDS)
                .keepAliveTimeout(10, TimeUnit.SECONDS)
                .build()
            return HubClient(channel, config, Logger(config.enableLogging))
        }
    }
    
    fun setToken(token: String) { /* 见 3.2 */ }
}
```

### 3.2 Auth — Token 注入

关键：**每个 gRPC 请求都带 session token**（附加到 gRPC metadata）。

```swift
// iOS: 拦截器
// gRPC Swift 通过 ClientInterceptor 实现
class AuthInterceptor: ClientInterceptor<Any, Any> {
    private var token: String = ""
    
    func setToken(_ token: String) { self.token = token }
    
    override func intercept<Request, Response>(
        method: GRPCMethodDescriptor,
        request: GRPCRequest<Request>,
        context: StatusCallContext,
        next: (GRPCRequest<Request>, StatusCallContext) -> GRPCEventLoopFuture<GRPCResponse<Response>>
    ) {
        var metadata = request.metadata
        metadata.add(key: "authorization", value: "Bearer \(token)")
        metadata.add(key: "x-sdk-version", value: SDKVersion)
        metadata.add(key: "x-platform", value: "ios")
        let newRequest = GRPCRequest(metadata: metadata, message: request.message)
        return next(newRequest, context)
    }
}
```

```kotlin
// Android: gRPC CallCredentials
class BearerToken(private var token: String) : CallCredentials() {
    override fun applyRequestMetadata(
        requestInfo: RequestInfo,
        appExecutor: Executor,
        applier: StatusApplier
    ) {
        val headers = Metadata()
        headers.put(Metadata.Key.of("authorization", Metadata.ASCII_STRING_MARSHALLER), "Bearer $token")
        headers.put(Metadata.Key.of("x-sdk-version", Metadata.ASCII_STRING_MARSHALLER), BuildConfig.SDK_VERSION)
        applier.apply(headers)
    }
    
    fun updateToken(newToken: String) { token = newToken }
}

// 使用: stub.withCallCredentials(bearerToken).bindKey(request)
```

### 3.3 业务接口实现

**原则**: 每个接口先注入 token metadata → 调 gRPC → 错误映射 → 返回简化模型

```swift
// iOS: BindKey 示例
extension YDKHubClient {
    public func bindKey(vehicleId: String) async throws -> YDKKey {
        let req = BindKeyRequest.with {
            $0.vehicleID = vehicleId
            // device_id / device_pubkey / protocol 由 SDK 内部填充
            $0.deviceID = await self.deviceManager.getDeviceId()
            $0.devicePubkey = await self.seManager.readPublicKey()
            $0.protocol = self.detectProtocol()
            $0.keyType = .owner
        }
        
        do {
            let resp = try await keyClient.bindKey(req)
            if !resp.errorCode.isEmpty {
                throw YDKError.hubError(resp.errorCode, resp.errorMsg)
            }
            return YDKKey(from: resp.key)
        } catch {
            throw YDKError.map(error)
        }
    }
}
```

```kotlin
// Android: BindKey 示例 (coroutine)
suspend fun bindKey(vehicleId: String): YDKKey {
    val req = bindKeyRequest {
        this.vehicleId = vehicleId
        deviceId = deviceManager.getDeviceId()
        devicePubkey = seManager.readPublicKey()
        `protocol` = detectProtocol()
        keyType = KEY_TYPE_OWNER
    }
    
    return try {
        val resp = keyStub.withCallCredentials(bearerToken).bindKey(req)
        if (resp.errorCode.isNotEmpty()) {
            throw YDKError.HubError(resp.errorCode, resp.errorMsg)
        }
        YDKKey.from(resp.key)
    } catch (e: Exception) {
        throw YDKError.map(e)
    }
}
```

### 3.4 完整接口签名

```swift
// iOS: YDKHubClient 公开接口
extension YDKHubClient {
    // Keys
    public func bindKey(vehicleId: String) async throws -> YDKKey
    public func unbindKey(keyId: String) async throws
    public func listKeys() async throws -> [YDKKey]
    public func getKey(keyId: String) async throws -> YDKKey
    
    // Share
    public func createShare(keyId: String, toUserId: String?, maxUses: Int32,
                           validFrom: Int64, validUntil: Int64) async throws -> YDKShare
    public func acceptShare(shareCode: String) async throws -> YDKKey
    public func cancelShare(shareId: String) async throws
    
    // Remote
    public func remoteLock(vehicleId: String, keyId: String) async throws
    public func remoteUnlock(vehicleId: String, keyId: String) async throws
    public func remoteStart(vehicleId: String, keyId: String) async throws
    public func remoteStop(vehicleId: String, keyId: String) async throws
    
    // Push
    public func handlePushNotification(_ payload: Data) -> Bool
}
```

```kotlin
// Android: HubClient 公开接口
suspend fun bindKey(vehicleId: String): YDKKey
suspend fun unbindKey(keyId: String)
suspend fun listKeys(): List<YDKKey>
suspend fun getKey(keyId: String): YDKKey
suspend fun createShare(keyId: String, toUserId: String?, maxUses: Int,
                       validFrom: Long, validUntil: Long): YDKShare
suspend fun acceptShare(shareCode: String): YDKKey
suspend fun cancelShare(shareId: String)
suspend fun remoteLock(vehicleId: String, keyId: String)
suspend fun remoteUnlock(vehicleId: String, keyId: String)
suspend fun remoteStart(vehicleId: String, keyId: String)
suspend fun remoteStop(vehicleId: String, keyId: String)
fun handlePushNotification(payload: ByteArray): Boolean
```

---

## 4. 错误处理

```swift
// iOS: 错误类型
public enum YDKError: Error {
    case notInitialized
    case notAuthenticated              // token 未设置
    case hubError(String, String)      // Hub 返回的业务错误 (errorCode, errorMsg)
    case networkError(Error)          // 网络不可达/TLS 失败
    case timeout                       // gRPC 超时
    case internal(String)             // SDK 内部错误
    
    var localizedDescription: String { ... }
}

// grpc-gateway 的错误码 → App 可读消息
extension YDKError {
    static func map(_ error: Error) -> YDKError {
        switch error {
        case is GRPCError.Unauthenticated: return .notAuthenticated
        case is GRPCError.Unavailable:     return .networkError(error)
        case is GRPCError.DeadlineExceeded: return .timeout
        default:                            return .internal(error.localizedDescription)
        }
    }
}
```

```kotlin
// Android: 错误类型
sealed class YDKError : Exception() {
    object NotInitialized : YDKError()
    object NotAuthenticated : YDKError()
    data class HubError(val code: String, val msg: String) : YDKError()
    data class NetworkError(val cause: Throwable) : YDKError()
    object Timeout : YDKError()
    data class Internal(val msg: String) : YDKError()
    
    companion object {
        fun map(e: Throwable): YDKError = when (e) {
            is StatusRuntimeException -> when (e.status.code) {
                Status.Code.UNAUTHENTICATED -> NotAuthenticated
                Status.Code.UNAVAILABLE -> NetworkError(e)
                Status.Code.DEADLINE_EXCEEDED -> Timeout
                else -> Internal(e.message ?: "unknown")
            }
            else -> NetworkError(e)
        }
    }
}
```

---

## 5. 测试方案

### 5.1 Mock Hub

复用现有 `e2e_11` 模式的 bufconn gRPC server。

```swift
// iOS: 用 bufconn 启动本地 gRPC Hub
// 暂不实现 mock server（依赖 Go 生态）
// → 单元测试用 protocol 注入 mock

protocol KeyManagementServiceStub {
    func bindKey(_ req: BindKeyRequest) async throws -> BindKeyResponse
    func listKeys(_ req: ListKeysRequest) async throws -> ListKeysResponse
    // ...
}
```

```kotlin
// Android: 单元测试用 mock
class HubClientTest {
    private val mockKeyStub = mock<KeyManagementServiceCoroutineStub>()
    
    @Test
    fun `bindKey returns key from Hub`() = runTest {
        // given
        coEvery { mockKeyStub.bindKey(any()) } returns bindKeyResponse {
            key = digitalKey { keyId = "key-001"; status = KEY_STATUS_ACTIVE }
            errorCode = ""
        }
        
        // when
        val result = client.bindKey("VH-001")
        
        // then
        assertEquals("key-001", result.keyId)
        assertEquals("ACTIVE", result.status)
    }
    
    @Test
    fun `bindKey throws on hub error`() = runTest {
        coEvery { mockKeyStub.bindKey(any()) } returns bindKeyResponse {
            errorCode = "ADAPTER_NOT_FOUND"
        }
        assertFailsWith<YDKError.HubError> { client.bindKey("VH-001") }
    }
}
```

### 5.2 集成测试

直接调现有的 yuleDKCS Hub（同一台机器的 `:9090`），验证完整通断。

---

## 6. 排期

| 天 | iOS | Android | 可并行？ |
|:-:|:----|:--------|:--------:|
| 1 | 项目脚手架 + proto 代码生成 + gRPC 通道初始化 + AuthInterceptor | 同 iOS | ✅ 双平台独立 |
| 2 | Keys 接口 (Bind/Unbind/List/Get) + 单元测试 | 同 iOS | ✅ |
| 3 | Share 接口 (Create/Accept/Cancel) + 单元测试 | 同 iOS | ✅ |
| 4 | Remote 接口 (Lock/Unlock/Start) + Push 解析 + 集成测试 | 同 iOS | ✅ |

**总计: 4 天**（双平台并行，各自 2 人天）

关键路径: 第 1 天做完后，第 2-4 天可以拆给不同的开发人员（每人负责一个模块）。

---

## 7. 依赖项

### iOS (Package.swift)

```swift
dependencies: [
    .package(url: "https://github.com/grpc/grpc-swift.git", from: "2.0.0")
]
```

最小 iOS 版本: **iOS 15.0**（gRPC-Swift 2.x 要求）

### Android (build.gradle.kts)

```kotlin
dependencies {
    implementation("io.grpc:grpc-kotlin-stub:1.4.1")
    implementation("io.grpc:grpc-okhttp:1.66.0")   // Android 上的 gRPC transport
    implementation("com.google.protobuf:protobuf-kotlin:4.29.3")
}
```

最小 Android SDK: **Android 8.0 (API 26)**
