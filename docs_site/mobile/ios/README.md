# YuleDKCS iOS SDK

Digital Key Control System SDK for iOS — Secure vehicle access via Bluetooth Low Energy, backed by a REST API and hardware-level crypto via Secure Enclave.

## Requirements

- iOS 13.0+
- Xcode 15+
- Swift 5.7+

## Installation

### Swift Package Manager

Add the YuleDKCS package via Xcode:

1. **File → Add Package Dependencies…**
2. Enter the repository URL: `https://github.com/nousresearch/yuleDKCS`
3. Set **Dependency Rule** → **Up to Next Major Version** → `1.0.0`
4. Add the **YuleDKCS** library to your target

Or add it to your `Package.swift` directly:

```swift
dependencies: [
    .package(url: "https://github.com/nousresearch/yuleDKCS", from: "1.0.0")
],
targets: [
    .target(name: "YourApp", dependencies: ["YuleDKCS"])
]
```

### Manual Integration

Drag `Sources/YuleDKCS/` into your Xcode project. The package depends on:

- `CoreBluetooth` — BLE communication
- `Combine` — Reactive API
- `CryptoKit` / `CommonCrypto` — Cryptographic operations
- `LocalAuthentication` — Biometric key activation
- `CoreLocation` — Location metadata for usage logs

---

## Quick Start

### 1. Initialize the SDK

Call `initialize(apiKey:environment:)` early in your app lifecycle (e.g., `AppDelegate` or `SceneDelegate`):

```swift
import YuleDKCS

// In application(_:didFinishLaunchingWithOptions:)
YuleDKCS.shared.initialize(
    apiKey: "your_api_key_here",
    environment: .production   // .development | .staging | .production
)
```

### 2. Issue a Digital Key

```swift
YuleDKCS.shared.keyManager.issueKey(vehicleId: "vehicle_123") { result in
    switch result {
    case .success(let key):
        print("Key issued: \(key.id), status: \(key.status)")
    case .failure(let error):
        print("Failed: \(error.localizedDescription)")
    }
}
```

### 3. List All Keys

```swift
let keys = YuleDKCS.shared.keyManager.listKeys()
for key in keys {
    print("\(key.id) — \(key.vehicleId ?? "unknown") — \(key.status)")
}
```

### 4. Connect to a Vehicle via BLE

```swift
let bleManager = YuleDKCS.shared.bleManager

// Start scanning
bleManager.startScan(timeout: 10.0)

// Observe scan results (published property)
bleManager.$scanResults.sink { results in
    for result in results {
        print("Found: \(result.peripheral.identifier) RSSI: \(result.rssi)")
    }
}

// Connect to a specific device
bleManager.connect(to: peripheralUUID)
```

### 5. Send Commands

```swift
// Convenience methods
bleManager.unlock(keyId: "key_123") { result in
    if case .success(let response) = result {
        print("Unlocked: \(response.message)")
    }
}

bleManager.lock(keyId: "key_123") { result in
    if case .success = result {
        print("Locked")
    }
}

bleManager.startEngine(keyId: "key_123") { result in
    // ...
}

// Or use the generic command API
bleManager.sendCommand(keyId: "key_123", command: .openTrunk) { result in
    // ...
}
```

### 6. Share a Key

```swift
YuleDKCS.shared.keyManager.shareKey(
    keyId: "key_123",
    recipient: "user@example.com",
    permissions: [.unlock, .lock, .startEngine],
    expiresInDays: 7
) { result in
    switch result {
    case .success(let shareLink):
        print("Share link: \(shareLink)")
    case .failure(let error):
        print("Share failed: \(error)")
    }
}
```

### 7. Revoke a Key

```swift
YuleDKCS.shared.keyManager.revokeKey(keyId: "key_123") { result in
    if case .success = result {
        print("Key revoked")
    }
}
```

---

## API Overview

### `YuleDKCS` — SDK Entry Point

| Member | Description |
|---|---|
| `shared` | Singleton instance |
| `version` | SDK version string (`"1.0.0"`) |
| `initialize(apiKey:environment:)` | Initializes the SDK, FFI bridge, and environment |
| `keyManager` | Lazy-loaded `KeyManager` instance |
| `bleManager` | Lazy-loaded `BLEManager` instance |
| `Environment` | `.development`, `.staging`, `.production` |

### `KeyManager` — Digital Key Lifecycle

| Method | Description |
|---|---|
| `issueKey(vehicleId:completion:)` | Issue a new digital key from the server |
| `receiveSharedKey(shareToken:completion:)` | Accept a shared key via token (QR/deep link) |
| `activateKey(keyId:completion:)` | Bind a key to the Secure Enclave |
| `listKeys(forceRefresh:)` | Return cached (or server-refreshed) keys |
| `getKey(keyId:)` | Look up a specific key by ID |
| `setActiveKey(keyId:)` | Set the active key for quick access |
| `getActiveKey()` | Return the currently active key |
| `shareKey(keyId:recipient:permissions:expiresInDays:completion:)` | Share a key with another user |
| `revokeKey(keyId:completion:)` | Revoke a key from both server and local storage |
| `deleteLocalKey(keyId:)` | Remove only the local cached key data |
| `hasPermission(keyId:permission:)` | Check whether a key has a given permission |
| `isKeyExpired(keyId:)` | Check expiration |
| `recordUsage(keyId:operation:status:location:failureReason:)` | Log a key usage event |
| `getUsageLogs(keyId:)` | Retrieve usage logs for a key |

**Published Properties**: `$keys`, `$activeKey`, `$isLoading`

### `BLEManager` — Vehicle BLE Communication

| Method | Description |
|---|---|
| `startScan(timeout:)` | Begin scanning for vehicles advertising the DKCS service |
| `stopScan()` | Stop scanning |
| `connect(to:autoReconnect:)` | Connect to a specific BLE peripheral |
| `connectToNearest(completion:)` | Connect to the first device found during scanning |
| `disconnect()` | Disconnect from the current peripheral |
| `sendCommand(keyId:command:completion:)` | Send a signed command to the connected vehicle |
| `lock(keyId:completion:)` | Convenience: lock doors |
| `unlock(keyId:completion:)` | Convenience: unlock doors |
| `startEngine(keyId:completion:)` | Convenience: start engine |
| `stopEngine(keyId:completion:)` | Convenience: stop engine |
| `openTrunk(keyId:completion:)` | Convenience: open trunk |
| `openWindows(keyId:completion:)` | Convenience: open windows |
| `closeWindows(keyId:completion:)` | Convenience: close windows |
| `findVehicle(keyId:completion:)` | Convenience: trigger vehicle finder |
| `requestVehicleStatus()` | Request real-time vehicle status update |

**Published Properties**: `$scanResults`, `$isScanning`, `$connectionState`, `$vehicleStatus`

### `APIService` — REST API Client

| Method | Description |
|---|---|
| `login(username:password:)` | Authenticate and receive JWT token |
| `register(username:email:password:)` | Create a new user account |
| `getKeys(page:pageSize:)` | Paginated key list from server |
| `getKeyDetail(keyId:)` | Single key details |
| `activateKey(keyId:)` | Mark key as active on server |
| `deactivateKey(keyId:)` | Mark key as inactive |
| `getKeyLogs(keyId:page:pageSize:)` | Paginated usage logs |
| `shareKey(keyId:request:)` | Create a share link for a key |
| `revokeKey(keyId:)` | Revoke a key on the server |
| `setAuthToken(_:)` | Persist JWT token to Keychain |
| `clearAuthToken()` | Remove stored token |
| `isAuthenticated()` | Check if a token exists |

### `CryptoWrapper` — Cryptographic Operations

| Method | Description |
|---|---|
| `storeKey(_:keyId:)` | Save a key to the Keychain |
| `getKey(keyId:)` | Retrieve a key from memory or Keychain |
| `deleteKey(keyId:)` | Remove a key |
| `encrypt(data:keyId:algorithm:)` | Encrypt data (AES-256-GCM or ChaCha20-Poly1305) |
| `decrypt(data:keyId:algorithm:)` | Decrypt data |
| `generateRandomKey(size:)` | Generate cryptographically secure random bytes |
| `hmac(data:key:)` | Compute HMAC-SHA256 |
| `sha256(data:)` | Compute SHA-256 hash |

### `FFIBridge` — C Native Bridge

Handles low-level calls to the bundled C library (`CYuleDKCS`) for key generation, encryption/decryption, session key derivation, and signature verification. Called internally — not typically used directly by app code.

---

## Data Models

| Model | Key Fields |
|---|---|
| `DigitalKey` | `id`, `keyData`, `vehicleId`, `protocolType` (CCC/ICCOA/ICCE), `type` (owner/shared), `status`, `permissions`, `issuedAt`, `expiresAt` |
| `KeyPermission` | `type`, `enabled`, `constraints` |
| `Permission` | Enum: `unlock`, `lock`, `startEngine`, `stopEngine`, `openTrunk`, `openWindows`, `closeWindows`, `findVehicle` |
| `VehicleStatus` | `doorLocked`, `engineRunning`, `trunkOpen`, `windowsOpen`, `batteryLevel`, `fuelLevel`, `alarmActive` |
| `Command` | Enum: `lock`, `unlock`, `startEngine`, `stopEngine`, `openTrunk`, `openWindows`, `closeWindows`, `findVehicle`, `custom(Data)` |
| `ScanResult` | `peripheral`, `rssi` |
| `ConnectionInfo` | `address`, `rssi`, `name`, `isBonded`, `signalStrength` |
| `KeyUsageLog` | `id`, `keyId`, `operation`, `status`, `timestamp`, `location`, `deviceInfo` |

---

## Errors

The SDK throws `YuleDKCSError` cases:

| Error | Description |
|---|---|
| `.notInitialized` | SDK not yet initialized |
| `.invalidApiKey` | API key rejected by server |
| `.networkError(Error)` | Underlying network failure |
| `.bleError(BLEError)` | BLE subsystem failure |
| `.cryptoError(CryptoError)` | Encryption/decryption failure |
| `.keyNotFound` | Specified key ID not in local cache |
| `.permissionDenied` | Key lacks required permission for an operation |
| `.invalidCommand` | Unrecognized or malformed command |
| `.ffiError(String)` | Native library error |

---

## Info.plist Requirements

Add these entries to your app's `Info.plist`:

```xml
<key>NSBluetoothAlwaysUsageDescription</key>
<string>This app needs Bluetooth to connect to your vehicle</string>
<key>NSBluetoothPeripheralUsageDescription</key>
<string>This app needs Bluetooth to communicate with your vehicle</string>
<key>NSLocationWhenInUseUsageDescription</key>
<string>This app needs location to find nearby vehicles and log usage</string>
```

---

## Architecture

```
┌───────────────────────────────────────────────┐
│              Your iOS Application             │
├───────────────────────────────────────────────┤
│                  YuleDKCS SDK                 │
│  ┌──────────┐  ┌──────────┐  ┌─────────────┐  │
│  │KeyManager│  │BLEManager│  │ APIService  │  │
│  │  (swift) │  │  (swift) │  │   (swift)   │  │
│  └────┬─────┘  └────┬─────┘  └──────┬──────┘  │
│       │              │               │         │
│  ┌────▼──────────────▼───────────────▼──────┐  │
│  │           CryptoWrapper (Swift)          │  │
│  │         + FFIBridge (C → Swift)          │  │
│  └────────────────┬─────────────────────────┘  │
│                   │                            │
│  ┌────────────────▼─────────────────────────┐  │
│  │     CYuleDKCS (C Native Library)         │  │
│  │   KeyGen · AES-GCM · ChaCha20 · Signing  │  │
│  └──────────────────────────────────────────┘  │
├───────────────────────────────────────────────┤
│  iOS: CoreBluetooth · Combine · Security · LA  │
└───────────────────────────────────────────────┘
```

---

## Building & Testing

```bash
cd mobile/ios
swift build          # Build the package
swift test           # Run unit tests
```

---

## License

MIT License — See LICENSE file for details.
