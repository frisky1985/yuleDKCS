// swift-tools-version:5.9
import PackageDescription

let package = Package(
    name: "DemoApp",

    platforms: [
        .iOS(.v15),
        .macOS(.v12)  // 仅用于 macOS 宿主编译/语法验证
    ],

    dependencies: [
        // 本地路径引用 yuleDKCS SDK（发布后替换为 git 依赖）
        .package(name: "yuleDKCS-SDK", path: "../../../../mobile/ios")
    ],

    targets: [
        .executableTarget(
            name: "DemoApp",
            dependencies: [
                .product(name: "YDKHubClient", package: "yuleDKCS-SDK"),
                .product(name: "YDKKeyManager", package: "yuleDKCS-SDK"),
                .product(name: "YDKBLEManager", package: "yuleDKCS-SDK"),
            ],
            path: "Sources/DemoApp"
        )
    ]
)
