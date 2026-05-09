# YuleDKCS Flutter SDK

YuleDKCS Digital Key SDK for Flutter using FFI binding to the native C library.

## Features

- **Key Management**: Issue, list, share, and revoke digital keys
- **BLE Communication**: Connect to vehicles via Bluetooth Low Energy
- **Secure Communication**: End-to-end encryption for all commands
- **Cross-Platform**: Support for Android and iOS

## Installation

Add to your `pubspec.yaml`:

```yaml
dependencies:
  yuledkcs: ^1.0.0
```

## Usage

### Initialize SDK

```dart
import 'package:yuledkcs/yuledkcs.dart';

await YuleDKCS.initialize('your-api-key');
```

### Issue a Key

```dart
final key = await YuleDKCS.issueKey('vehicle-id');
print('Key issued: ${key.keyId}');
```

### List Keys

```dart
final keys = YuleDKCS.listKeys();
for (final key in keys) {
  print('Key: ${key.keyId}, Status: ${key.status}');
}
```

### Share Key

```dart
await YuleDKCS.shareKey(
  keyId,
  'user@example.com',
  [Permission.unlock, Permission.lock],
);
```

### Connect to Vehicle

```dart
await YuleDKCS.connect('AA:BB:CC:DD:EE:FF');
```

### Send Commands

```dart
await YuleDKCS.sendCommand(Command.unlock);
await YuleDKCS.sendCommand(Command.lock);
await YuleDKCS.sendCommand(Command.startEngine);
```

## Permissions

### Android

Add to `android/app/src/main/AndroidManifest.xml`:

```xml
<uses-permission android:name="android.permission.BLUETOOTH" />
<uses-permission android:name="android.permission.BLUETOOTH_SCAN" />
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />
<uses-permission android:name="android.permission.ACCESS_COARSE_LOCATION" />
```

### iOS

Add to `ios/Runner/Info.plist`:

```xml
<key>NSBluetoothAlwaysUsageDescription</key>
<string>This app needs Bluetooth to connect to your vehicle</string>
<key>NSBluetoothPeripheralUsageDescription</key>
<string>This app needs Bluetooth to connect to your vehicle</string>
<key>NSLocationWhenInUseUsageDescription</key>
<string>This app needs location to find nearby vehicles</string>
```

## API Reference

### YuleDKCS

- `initialize(String apiKey)` - Initialize the SDK
- `issueKey(String vehicleId)` - Issue a new digital key
- `listKeys()` - List all digital keys
- `shareKey(String keyId, String to, List<Permission> permissions)` - Share a key
- `revokeKey(String keyId)` - Revoke a key
- `connect(String address)` - Connect to vehicle via BLE
- `disconnect()` - Disconnect from vehicle
- `sendCommand(Command command)` - Send command to vehicle

### Connection States

- `disconnected` - Not connected
- `scanning` - Scanning for devices
- `connecting` - Connecting to device
- `connected` - Connected to device
- `authenticating` - Authenticating with device
- `ready` - Ready to send commands
- `error` - Error occurred

## License

MIT License - Copyright (c) 2025 Nous Research
