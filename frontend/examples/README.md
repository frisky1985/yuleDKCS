# yuleDKCS SDK 示例工程骨架

> Phase 3 准备 — 最小可编译骨架，只展示 SDK API 调用面，不复制 SDK 内部逻辑。
> 完整集成说明见 [docs/sdk/SDK-INTEGRATION-GUIDE.md](../../docs/sdk/SDK-INTEGRATION-GUIDE.md)。

## 目录

| 工程 | 路径 | 接入方式 | 验证 |
|:-----|:-----|:---------|:-----|
| iOS DemoApp | `ios/DemoApp/` | SwiftPM path 依赖 `../../../../mobile/ios` | `swiftc -parse Sources/DemoApp/main.swift` |
| Android DemoApp | `android/` | Gradle `settings.gradle.kts` 引用 `../../../mobile/android/sdk` module | 静态审查（禁止 gradle/kotlinc，与 SDK 验证规则一致） |

## iOS

```bash
cd ios/DemoApp
# 语法验证（macOS 宿主）
swiftc -parse Sources/DemoApp/main.swift
# 完整构建（可选；iOS 产物需真机/Xcode 运行）
swift build
```

要点：

- `Package.swift` 以 `.package(name: "yuleDKCS-SDK", path: "../../../../mobile/ios")` 本地引用，
  依赖 SDK 的三个 library 产物：`YDKHubClient` / `YDKKeyManager` / `YDKBLEManager`。
- `Sources/DemoApp/main.swift` 演示：初始化 → 绑定/查询钥匙 → 远程控车 →
  BLE 扫描/连接/解锁 → 分享（分享码 + CCC Mailbox）→ 离线授权 → UWB → NFC。
- 真机运行还需 Info.plist 权限声明（蓝牙后台 / NFC / UWB），见集成指南 §2.11。

## Android

```bash
cd android
# 静态审查（无 Android SDK 环境时）：
#   1) AndroidManifest.xml 权限/服务声明齐全性
#   2) MainActivity.kt 中每个 SDK 调用与 mobile/android/sdk 源码公开签名逐一对照
# 有 Android Studio 时直接打开本目录构建
```

要点：

- `settings.gradle.kts` 以 `project(":sdk").projectDir = File(rootDir, "../../mobile/android/sdk")`
  本地引用 SDK module。
- `app/src/main/kotlin/com/yourcompany/demo/MainActivity.kt` 演示对应全部调用面。
- Manifest 已声明蓝牙/定位/前台服务/NFC 权限与 `YdkBleForegroundService` 服务组件。
- 运行时权限（API 31+ 蓝牙）通过 `BlePermissions.requiredPermissions()` 获取清单。
