# YuleDKCS Android SDK

Digital Key Control System SDK for Android - Secure vehicle access via Bluetooth Low Energy.

## Installation

### JitPack

Add to your root `build.gradle`:
```groovy
allprojects {
    repositories {
        ...
        maven { url 'https://jitpack.io' }
    }
}
```

Add dependency:
```groovy
dependencies {
    implementation 'com.github.yuledkcs:yuledkcs-android:1.0.0'
}
```

## Quick Start

### Initialize SDK

```kotlin
import com.yuledkcs.YuleDKCS

class MyApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        
        YuleDKCS.initialize(
            context = this,
            apiKey = "your_api_key_here"
        )
    }
}
```

### Issue a Digital Key

```kotlin
val keyManager = YuleDKCS.getKeyManager()

keyManager.issueKey("vehicle_123") { result ->
    result.onSuccess { key ->
        Log.d("DKCS", "Key issued: ${key.id}")
    }.onFailure { error ->
        Log.e("DKCS", "Failed to issue key", error)
    }
}
```

### List All Keys

```kotlin
val keys = keyManager.listKeys()
keys.forEach { key ->
    println("Key: ${key.id}, Vehicle: ${key.vehicleName}")
}
```

### Share a Key

```kotlin
keyManager.shareKey(
    keyId = "key_123",
    to = "user@example.com",
    permissions = listOf(Permission.UNLOCK, Permission.LOCK)
) { result ->
    result.onSuccess {
        Log.d("DKCS", "Key shared successfully")
    }
}
```

### Revoke a Key

```kotlin
keyManager.revokeKey("key_123") { result ->
    result.onSuccess {
        Log.d("DKCS", "Key revoked")
    }
}
```

## BLE Communication

### Connect to Vehicle

```kotlin
val bleManager = YuleDKCS.getBLEManager()

bleManager.connect("AA:BB:CC:DD:EE:FF") { connected ->
    if (connected) {
        Log.d("DKCS", "Connected to vehicle")
    }
}
```

### Send Commands

```kotlin
// Unlock
bleManager.sendCommand(Command.Unlock("key_123")) { result ->
    result.onSuccess {
        Log.d("DKCS", "Vehicle unlocked")
    }
}

// Lock
bleManager.sendCommand(Command.Lock("key_123")) { result ->
    result.onSuccess {
        Log.d("DKCS", "Vehicle locked")
    }
}

// Start engine
bleManager.sendCommand(Command.StartEngine("key_123")) { result ->
    result.onSuccess {
        Log.d("DKCS", "Engine started")
    }
}
```

## Permissions

Add to your `AndroidManifest.xml`:

```xml
<!-- Bluetooth -->
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
<uses-permission android:name="android.permission.BLUETOOTH_SCAN" />

<!-- Location (required for BLE scanning) -->
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />

<!-- Internet -->
<uses-permission android:name="android.permission.INTERNET" />
```

## Crypto Utilities

```kotlin
// Encrypt data
val encrypted = CryptoWrapper.encryptString("sensitive data", "my_alias")

// Decrypt data
val decrypted = CryptoWrapper.decryptToString(encrypted, "my_alias")

// Hash data
val hash = CryptoWrapper.hash(byteArrayOf(1, 2, 3))

// Generate random bytes
val random = CryptoWrapper.randomBytes(32)
```

## API Reference

See the [full API documentation](https://yuledkcs.com/docs/android).

## License

MIT License - See LICENSE file for details.
