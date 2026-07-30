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

    dependencies: [
        .package(
            url: "https://github.com/grpc/grpc-swift.git",
            from: "2.0.0"
        ),
    ],

    targets: [
        // ── Hub gRPC 客户端 ──
        .target(
            name: "YDKHubClient",
            dependencies: [
                .product(name: "GRPC", package: "grpc-swift"),
                "YDKProto",
            ],
            path: "Sources/YDKHubClient"
        ),

        // ── BLE/UWB 本地通信 ──
        .target(
            name: "YDKBLEManager",
            dependencies: [
                "YDKProto",
            ],
            path: "Sources/YDKBLEManager"
        ),

        // ── 由 proto 代码生成（手动运行 protoc 后填充） ──
        .target(
            name: "YDKProto",
            dependencies: [
                .product(name: "GRPC", package: "grpc-swift"),
            ],
            path: "Sources/YDKProto",
            exclude: [
                "README.md",
            ]
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
