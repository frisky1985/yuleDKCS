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
            name: "YDKBLEManager",
            targets: ["YDKBLEManager"]
        ),
    ],

    dependencies: [],

    targets: [
        // ── Hub HTTPS REST 客户端 ──
        // 通过 URLSession 调用 yuleDKCS Hub REST Gateway (:8080)
        // 无需 gRPC / protobuf 外部依赖
        .target(
            name: "YDKHubClient",
            dependencies: [],
            path: "Sources/YDKHubClient"
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
            name: "YDKBLEManagerTests",
            dependencies: ["YDKBLEManager"],
            path: "Tests/YDKBLEManagerTests"
        ),
    ]
)
