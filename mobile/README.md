# YuleDKCS Mobile SDK

Cross-platform digital key SDK for the YuleDKCS Digital Key Connectivity System. Provides secure vehicle access via Bluetooth Low Energy across iOS, Android, and Flutter.

## Architecture

```
mobile/
├── ios/              # iOS SDK (Swift Package)
│   ├── Package.swift
│   └── Sources/YuleDKCS/
│       ├── YuleDKCS.swift        # SDK entry point & initialization
│       ├── KeyManager.swift      # Digital key lifecycle management
│       ├── BLEManager.swift      # BLE scanning, connection, commands
│       ├── Services/APIService.swift  # REST API client
│       ├── CryptoWrapper.swift   # AES-256-GCM / ChaCha20-Poly1305
│       ├── Models.swift          # DigitalKey, VehicleStatus, etc.
│       └── FFIBridge/            # C native library bindings
│
├── android/          # Android SDK (Kotlin)
│   ├── build.gradle.kts
│   └── src/main/java/com/yuledkcs/
│       ├── YuleDKCS.kt
│       ├── KeyManager.kt
│       ├── BLEManager.kt
│       ├── CryptoWrapper.kt
│       ├── api/
│       ├── models/
│       └── security/
│
├── flutter/          # Flutter SDK (Dart + FFI)
│   ├── pubspec.yaml
│   ├── lib/
│   │   ├── yuledkcs.dart         # Entry point & top-level API
│   │   └── src/
│   │       ├── ffi_bridge.dart   # dart:ffi native bindings
│   │       ├── key_manager.dart  # Key management
│   │       └── ble_manager.dart  # BLE via flutter_blue_plus
│   └── example/                  # Reference Flutter app
│       └── lib/main.dart
│
├── deploy_ios.sh     # iOS deployment helper
└── README.md         # This file
```

## Platform SDKs

| Platform | Technology | README | Build |
|---|---|---|---|
| **iOS** | Swift 5.7+ (Swift Package) | [ios/README.md](./ios/README.md) | `cd ios && swift build` |
| **Android** | Kotlin (Gradle) | [android/README.md](./android/README.md) | `cd android && ./gradlew build` |
| **Flutter** | Dart 3+ (FFI) | [flutter/README.md](./flutter/README.md) | `cd flutter && flutter build apk` |

## Common Features

All three SDKs share the same capability set:

### Key Management
- **Issue** — Request a new digital key for a vehicle from the backend
- **List** — Retrieve all owned/shared keys
- **Activate** — Bind a key to the device's secure hardware
- **Share** — Grant key access to another user with permissions + expiration
- **Revoke** — Remove a key from both server and local storage
- **Permissions** — Granular control: unlock, lock, startEngine, openTrunk, etc.
- **Usage Logging** — Record and retrieve key usage history

### BLE Vehicle Control
- **Scan** — Discover nearby vehicles advertising the DKCS BLE service
- **Connect** — Establish an authenticated BLE session
- **Commands** — lock, unlock, startEngine, stopEngine, openTrunk, openWindows, closeWindows, findVehicle
- **Status** — Real-time vehicle state: door, engine, trunk, windows, battery, fuel
- **Security** — All commands are signed with the digital key before transmission

### Security & Crypto
- **Encryption**: AES-256-GCM and ChaCha20-Poly1305
- **Key Storage**: iOS Keychain / Secure Enclave, Android KeyStore
- **FFI Bridge**: Common C library for key generation, signing, session key derivation
- **Protocols**: Compatible with CCC, ICCOA, and ICCE digital key standards

## Quick Links

- **iOS SDK Docs** → [ios/README.md](./ios/README.md)
- **Android SDK Docs** → [android/README.md](./android/README.md)
- **Flutter SDK Docs** → [flutter/README.md](./flutter/README.md)
- **Example App** → [flutter/example/](./flutter/example/)

## Build Commands

```bash
# iOS
cd ios && swift build && swift test

# Android
cd android && ./gradlew build && ./gradlew test

# Flutter
cd flutter && flutter pub get && flutter build apk
```

## Dependencies

| Component | Requirements |
|---|---|
| iOS SDK | Xcode 15+, Swift 5.7+, iOS 13.0+ |
| Android SDK | Android Studio Hedgehog+, Kotlin 1.9+, API 23+ |
| Flutter SDK | Flutter 3.10+, Dart 3.0+, Xcode 15+ / Android Studio |
| Native Library | `libyuledkcs` (bundled C library for crypto + BLE auth) |

## Deployment

iOS deployment uses the `deploy_ios.sh` script at the mobile root:

```bash
./deploy_ios.sh
```

This script handles archive, signing, and TestFlight/App Store submission.

---

## License

MIT License — See LICENSE file for details.
