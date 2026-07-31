// swift-tools-version:5.9
import PackageDescription

let package = Package(
    name: "yuleDKCS-SDK",

    platforms: [
        .iOS(.v15)
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
        .target(
            name: "YDKBLEManager",
            dependencies: [],
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
