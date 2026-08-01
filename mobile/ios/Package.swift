// swift-tools-version:5.9
import PackageDescription

let package = Package(
    name: "yuleDKCS-SDK",

    platforms: [
        .iOS(.v15),
        // macOS 最低版本声明: 仅用于 macOS 宿主（开发/CI）编译验证。
        // AsyncThrowingStream (10.15+) 与 URLSession.data(for:) (12.0+)
        // 需要 macOS 12+ 部署目标; 不影响 iOS 构建。
        .macOS(.v12)
    ],

    products: [
        .library(
            name: "YDKHubClient",
            targets: ["YDKHubClient"]
        ),
        .library(
            name: "YDKKeyManager",
            targets: ["YDKKeyManager"]
        ),
        .library(
            name: "YDKBLEManager",
            targets: ["YDKBLEManager"]
        ),
    ],

    dependencies: [],

    targets: [
        // ── Hub HTTPS REST 客户端 ──
        .target(
            name: "YDKHubClient",
            dependencies: [],
            path: "Sources/YDKHubClient"
        ),

        // ── 钥匙状态管理（本地缓存 + 定时同步） ──
        .target(
            name: "YDKKeyManager",
            dependencies: ["YDKHubClient"],
            path: "Sources/YDKKeyManager"
        ),

        // ── BLE/UWB 本地通信 ──
        // 依赖 YDKHubClient: 复用其 public YDKLogger / YDKError（预存跨模块引用）
        .target(
            name: "YDKBLEManager",
            dependencies: ["YDKHubClient"],
            path: "Sources/YDKBLEManager"
        ),

        // ── 测试 ──
        .testTarget(
            name: "YDKHubClientTests",
            dependencies: ["YDKHubClient"],
            path: "Tests/YDKHubClientTests"
        ),
        .testTarget(
            name: "YDKKeyManagerTests",
            dependencies: ["YDKKeyManager"],
            path: "Tests/YDKKeyManagerTests"
        ),
        .testTarget(
            name: "YDKBLEManagerTests",
            dependencies: ["YDKBLEManager"],
            path: "Tests/YDKBLEManagerTests"
        ),
    ]
)
