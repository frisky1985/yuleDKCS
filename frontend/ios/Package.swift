// swift-tools-version:5.9
// yuleDKCS iOS SDK — Swift Package Manager 配置
// 用于 CI 运行单元测试（`swift test`）

import PackageDescription

let package = Package(
    name: "DigitalKeySDK",
    platforms: [
        .iOS(.v14),
        .macOS(.v14),
    ],
    products: [
        .library(
            name: "DigitalKeySDK",
            targets: ["DigitalKeySDK"]
        ),
    ],
    dependencies: [],
    targets: [
        .target(
            name: "DigitalKeySDK",
            path: "Sources/DigitalKeySDK"
        ),
        .testTarget(
            name: "DigitalKeySDKTests",
            dependencies: ["DigitalKeySDK"],
            path: "Tests/DigitalKeySDKTests"
        ),
    ]
)
