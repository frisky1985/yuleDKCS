// swift-tools-version:5.7
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "YuleDKCS",
    platforms: [
        .iOS(.v13)
    ],
    products: [
        .library(
            name: "YuleDKCS",
            targets: ["YuleDKCS"]
        ),
    ],
    dependencies: [],
    targets: [
        .target(
            name: "YuleDKCS",
            dependencies: ["CYuleDKCS"],
            path: "Sources/YuleDKCS",
            exclude: ["FFIBridge/yuledkcs.h"],
            swiftSettings: [
                .define("YULEDKCS_SWIFT_SDK")
            ]
        ),
        .target(
            name: "CYuleDKCS",
            path: "Sources/YuleDKCS/FFIBridge",
            publicHeadersPath: ".",
            cSettings: [
                .headerSearchPath("."),
                .define("YULEDKCS_FFI_EXPORT")
            ]
        ),
        .testTarget(
            name: "YuleDKCSTests",
            dependencies: ["YuleDKCS"],
            path: "Tests"
        ),
    ]
)