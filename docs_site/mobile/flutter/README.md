# YuleDKCS Flutter SDK

Digital Key Control System SDK for Flutter — Cross-platform vehicle access via Bluetooth Low Energy, backed by FFI bindings to the native C library.

Supports **Android** and **iOS** with a single Dart API.

## Requirements

- Flutter 3.10+ / Dart 3.0+
- Android: API 23+ (Android 6.0)
- iOS: 13.0+
- Xcode 15+ (for iOS builds)

## Installation

Add `yuledkcs` to your `pubspec.yaml`:

```yaml
dependencies:
  yuledkcs: ^1.0.0
```

Or use a path dependency if developing locally:

```yaml
dependencies:
  yuledkcs:
    path: ../mobile/flutter
```

Run:

```bash
flutter pub get
```

### iOS Setup

Add these entries to `ios/Runner/Info.plist`:

```xml
<key>NSBluetoothAlwaysUsageDescription</key>
<string>This app needs Bluetooth to connect to your vehicle</string>
<key>NSBluetoothPeripheralUsageDescription</key>
<string>This app needs Bluetooth to communicate with your vehicle</string>
<key>NSLocationWhenInUseUsageDescription</key>
<string>This app uses location to find nearby vehicles</string>
```

### Android Setup

Add these permissions to `android/app/src/main/AndroidManifest.xml`:

```xml
<uses-permission android:name="android.permission.BLUETOOTH" />
<uses-permission android:name="android.permission.BLUETOOTH_SCAN" />
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />
<uses-permission android:name="android.permission.ACCESS_COARSE_LOCATION" />
<uses-permission android:name="android.permission.INTERNET" />
```

---

## Quick Start

### 1. Initialize the SDK

```dart
import 'package:yuledkcs/yuledkcs.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();

  await YuleDKCS.initialize('your_api_key_here');

  runApp(MyApp());
}
```

### 2. Issue a Digital Key

```dart
try {
  final key = await YuleDKCS.issueKey('vehicle_123');
  print('Key issued: ${key.keyId}');
  print('Vehicle: ${key.vehicleId}');
  print('Status: ${key.status}');
} catch (e) {
  print('Failed to issue key: $e');
}
```

### 3. List All Keys

```dart
final keys = YuleDKCS.listKeys();
for (final key in keys) {
  print('${key.keyId} — ${key.vehicleId} — ${key.status}');
}
```

### 4. Share a Key

```dart
await YuleDKCS.shareKey(
  'key_123',
  'user@example.com',
  [Permission.unlock, Permission.lock, Permission.startEngine],
);
```

### 5. Revoke a Key

```dart
await YuleDKCS.revokeKey('key_123');
```

### 6. Connect to a Vehicle via BLE

```dart
// Observe connection state changes
YuleDKCS.connectionStateStream.listen((state) {
  print('Connection state: ${state.name}');
});

// Connect to a specific BLE device by address
await YuleDKCS.connect('AA:BB:CC:DD:EE:FF');

// or scan for nearby vehicles first
final scanStream = BLEManager().scanDevices(
  timeout: const Duration(seconds: 10),
);
await for (final result in scanStream) {
  print('Found device: ${result.device.remoteId}');
}
```

### 7. Send Vehicle Commands

```dart
// Must be connected and authenticated (ConnectionState.ready)
await YuleDKCS.sendCommand(Command.unlock);
await YuleDKCS.sendCommand(Command.lock);
await YuleDKCS.sendCommand(Command.startEngine);
await YuleDKCS.sendCommand(Command.stopEngine);
await YuleDKCS.sendCommand(Command.openTrunk);
await YuleDKCS.sendCommand(Command.closeTrunk);
await YuleDKCS.sendCommand(Command.openWindows);
await YuleDKCS.sendCommand(Command.closeWindows);
await YuleDKCS.sendCommand(Command.startClimate);
await YuleDKCS.sendCommand(Command.stopClimate);
```

### 8. Disconnect

```dart
await YuleDKCS.disconnect();
```

---

## Full Example

A complete Flutter example app is located at [`example/`](./example/). Run it with:

```bash
cd mobile/flutter/example
flutter run
```

The example demonstrates:
- SDK initialization with an API key text field
- Key issuance and listing
- BLE connection by device address
- Command sending (unlock, lock, start engine, stop engine, open trunk)
- Real-time connection state display
- Dynamic key list rendering

---

## API Reference

### `YuleDKCS` — SDK Entry Point (singleton)

| Method | Return Type | Description |
|---|---|---|
| `initialize(String apiKey)` | `Future<void>` | Initialize SDK, FFI bridge, and BLE subsystem |
| `issueKey(String vehicleId)` | `Future<DigitalKey>` | Issue a new digital key for a vehicle |
| `listKeys()` | `List<DigitalKey>` | Return all stored digital keys |
| `shareKey(String keyId, String to, List<Permission> permissions)` | `Future<void>` | Share a key with another user |
| `revokeKey(String keyId)` | `Future<void>` | Revoke a digital key |
| `connect(String address)` | `Future<void>` | Connect to a vehicle BLE device |
| `disconnect()` | `Future<void>` | Disconnect from the current device |
| `sendCommand(Command command)` | `Future<void>` | Send a vehicle command over BLE |
| `connectionState` (getter) | `ConnectionState` | Current BLE connection state |
| `connectionStateStream` (getter) | `Stream<ConnectionState>` | Reactive connection state stream |

### `FFIBridge` — Native Library Binding (singleton)

| Method | Description |
|---|---|
| `initialize()` | Load and bind the native C library |
| `initializeSDK(String apiKey)` | Call native `dkcs_init` |
| `issueKey(String vehicleId, String apiKey)` | Call native `dkcs_issue_key` |
| `listKeys()` | Call native `dkcs_list_keys` |
| `shareKey(String keyId, String to, String permissions)` | Call native `dkcs_share_key` |
| `revokeKey(String keyId)` | Call native `dkcs_revoke_key` |
| `connect(String address)` | Call native `dkcs_connect` |
| `sendCommand(String command)` | Call native `dkcs_send_command` |
| `getError()` | Retrieve the last native error string |

The FFI bridge loads the appropriate library per platform:
- **Android** → `libyuledkcs.so`
- **iOS** → `yuledkcs.framework/yuledkcs`
- **Linux** → `libyuledkcs.so`
- **macOS** → `libyuledkcs.dylib`
- **Windows** → `yuledkcs.dll`

### `BLEManager` — Bluetooth LE Manager

| Method | Return Type | Description |
|---|---|---|
| `initialize()` | `Future<void>` | Request BLE/location permissions, listen for adapter state |
| `connect(String address)` | `Future<void>` | Connect to a BLE device by address, discover services/characteristics, authenticate |
| `disconnect()` | `Future<void>` | Disconnect from the current device |
| `sendCommand(Command command)` | `Future<void>` | Prepare and send a command via FFI, then write to BLE characteristic |
| `scanDevices({Duration timeout})` | `Stream<ScanResult>` | Scan for nearby vehicles advertising the DKCS service |
| `stopScan()` | `Future<void>` | Stop an active scan |
| `dispose()` | `void` | Close the connection state stream |

**Properties**: `connectionState` (getter), `connectionStateStream` (stream)

### `KeyManager` — Key Operations

| Method | Return Type | Description |
|---|---|---|
| `issueKey(String vehicleId, String apiKey)` | `Future<DigitalKey>` | Issue a new key via FFI |
| `listKeys()` | `List<DigitalKey>` | List all keys via FFI |
| `shareKey(String keyId, String to, List<Permission> permissions)` | `Future<void>` | Share a key via FFI |
| `revokeKey(String keyId)` | `Future<void>` | Revoke a key via FFI |

---

## Data Models

### `DigitalKey`

| Field | Type | Description |
|---|---|---|
| `keyId` | `String` | Unique key identifier |
| `vehicleId` | `String` | Associated vehicle ID |
| `ownerId` | `String` | Owner user ID |
| `permissions` | `List<Permission>` | Granted permissions |
| `issuedAt` | `DateTime` | Key creation time |
| `expiresAt` | `DateTime?` | Optional expiration time |
| `status` | `KeyStatus` | Current key status |

### `Permission` (enum)

`unlock`, `lock`, `startEngine`, `openTrunk`, `openWindows`, `climateControl`

### `KeyStatus` (enum)

`active`, `expired`, `revoked`, `suspended`

### `Command` (enum)

`unlock`, `lock`, `startEngine`, `stopEngine`, `openTrunk`, `closeTrunk`, `openWindows`, `closeWindows`, `startClimate`, `stopClimate`

### `ConnectionState` (enum)

| State | Description |
|---|---|
| `disconnected` | Not connected to any device |
| `scanning` | Scanning for BLE devices |
| `connecting` | Connecting to a device |
| `connected` | BLE connection established, services discovered |
| `authenticating` | Performing authentication handshake |
| `ready` | Authenticated and ready to send commands |
| `error` | Connection or authentication error |

---

## Architecture

```
┌──────────────────────────────────────┐
│          Flutter (Dart)              │
│                                      │
│  ┌────────┐  ┌──────────┐           │
│  │YuleDKCS│  │KeyManager│           │
│  │ (main) │  │  (dart)  │           │
│  └────┬───┘  └─────┬────┘           │
│       │             │                │
│  ┌────▼─────────────▼────┐           │
│  │  FFIBridge (dart:ffi) │           │
│  │  DynamicLibrary calls │           │
│  └────┬──────────────────┘           │
│       │                              │
│  ┌────▼──────────────┐               │
│  │  BLEManager (dart)│               │
│  │ flutter_blue_plus │               │
│  └────┬──────────────┘               │
└───────┼──────────────────────────────┘
        │
┌───────▼──────────────────────────────┐
│  Native C Library (libyuledkcs)      │
│  KeyGen · Crypto · BLE Auth · Sign   │
└──────────────────────────────────────┘
        │
┌───────▼──────────────────────────────┐
│  Platform Layer                      │
│  Android: BLE + KeyStore             │
│  iOS: CoreBluetooth + Secure Enclave │
└──────────────────────────────────────┘
```

## Exceptions

| Exception | Description |
|---|---|
| `StateError` | Thrown if SDK methods are called before `initialize()` |
| `KeyException` | Key management failure (FFI returned error) |
| `BLEException` | BLE connection, authentication, or command failure |

---

## License

MIT License — See LICENSE file for details.
