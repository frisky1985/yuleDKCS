# App 端开发指南（手机端）

> **版本**：v1.0  
> **日期**：2026-04-26  
> **作者**：code_designer  
> **适用范围**：App 团队  
> **目标平台**：iOS 15+ / Android 10+

---

## 目录

1. [技术栈总览](#1-技术栈总览)
2. [开发环境搭建](#2-开发环境搭建)
3. [代码架构设计](#3-代码架构设计)
4. [核心模块开发指南](#4-核心模块开发指南)
5. [关键算法/流程实现](#5-关键算法流程实现)
6. [接口调用示例](#6-接口调用示例)
7. [测试要点](#7-测试要点)
8. [参考资源](#8-参考资源)

---

## 1. 技术栈总览

### 1.1 技术选型

| 层级 | 技术选型 | 说明 |
|------|---------|------|
| **跨平台框架** | Flutter 3.x | 主要 UI 开发框架 |
| **原生开发** | Swift 5.x / Kotlin 1.9 | BLE/UWB/NFC 通信层 |
| **桥接层** | Flutter MethodChannel | Flutter ↔ 原生双向通信 |
| **状态管理** | Riverpod 2.x | 编译时安全、依赖注入 |
| **HTTP 客户端** | Dio 5.x | REST API 调用 |
| **gRPC** | grpc-dart | 云端内部通信 |
| **本地存储** | flutter_secure_storage | 安全存储（Keychain/Keystore） |
| **生物认证** | local_auth | Face ID / 指纹 |
| **CI/CD** | Fastlane + GitHub Actions | 自动化构建与分发 |

### 1.2 平台能力矩阵

| 能力 | iOS | Android |
|------|-----|---------|
| BLE | CoreBluetooth | Android BLE API |
| UWB | NearbyInteraction (iPhone 11+) | Android UWB API (API 31+) |
| NFC 读卡 | CoreNFC | NFC Adapter |
| NFC 卡模拟 (HCE) | 不支持（仅 SE）| Android HCE |
| 安全存储 | Secure Enclave + Keychain | TEE / StrongBox + Keystore |
| 生物认证 | Face ID / Touch ID | 指纹 / 人脸 |

### 1.3 项目结构

```
digital_key_app/
├── lib/
│   ├── main.dart
│   ├── app.dart
│   ├── core/                      # 核心模块
│   │   ├── config/               # 配置
│   │   ├── constants/             # 常量
│   │   ├── errors/               # 错误定义
│   │   ├── network/               # 网络层（Dio + gRPC）
│   │   └── utils/                 # 工具类
│   ├── data/                      # 数据层
│   │   ├── models/                # 数据模型
│   │   ├── repositories/          # 仓库实现
│   │   └── datasources/           # 数据源（远程/本地）
│   ├── domain/                     # 领域层
│   │   ├── entities/              # 实体
│   │   ├── repositories/          # 仓库接口
│   │   └── usecases/              # 用例
│   ├── presentation/              # 展示层
│   │   ├── pages/                # 页面
│   │   ├── widgets/              # 组件
│   │   └── viewmodels/           # ViewModel（Riverpod）
│   ├── services/                  # 服务层
│   │   ├── auth/                 # 认证服务
│   │   ├── key/                  # 钥匙服务
│   │   ├── vehicle/              # 车辆服务
│   │   └── sharing/              # 分享服务
│   └── protocol/                  # 协议层
│       ├── icce/                 # ICCE SDK 封装
│       └── ccc/                  # CCC SDK 封装
├── ios/
│   ├── Runner/
│   │   ├── AppDelegate.swift
│   │   ├── BluetoothManager.swift  # BLE 原生实现
│   │   ├── UWBManager.swift        # UWB 原生实现
│   │   ├── NFCManager.swift        # NFC 原生实现
│   │   ├── SecureStorage.swift     # 安全存储
│   │   └── KeyPlugin.swift         # Flutter MethodChannel
├── android/
│   └── app/src/main/kotlin/
│       ├── MainActivity.kt
│       ├── BluetoothManager.kt    # BLE 原生实现
│       ├── UWBManager.kt           # UWB 原生实现
│       ├── NFCManager.kt           # NFC 原生实现
│       ├── SecureStorage.kt        # 安全存储
│       └── KeyPlugin.kt            # Flutter MethodChannel
└── test/
```

---

## 2. 开发环境搭建

### 2.1 通用环境

| 项目 | 版本要求 |
|------|---------|
| Flutter SDK | 3.13+ |
| Dart | 3.0+ |
| Xcode | 15.0+ (iOS 开发) |
| Android Studio | 2023.2+ |
| CocoaPods | 1.14+ |
| Fastlane | 2.215+ |

### 2.2 Flutter 项目创建

```bash
# 1. 检查 Flutter 版本
flutter --version
# Flutter 3.13.x • channel stable

# 2. 创建项目
flutter create --org com.digitalkey --project-name digital_key_app
cd digital_key_app

# 3. 添加依赖
flutter pub add:
  flutter_riverpod: ^2.4.0
  riverpod_annotation: ^2.3.0
  dio: ^5.4.0
  flutter_secure_storage: ^9.0.0
  local_auth: ^2.1.8
  go_router: ^13.0.0
  freezed_annotation: ^2.4.1
  json_annotation: ^4.8.1
  grpc: ^3.2.4
  protobuf: ^3.1.0

# 4. 添加开发依赖
flutter pub add --dev:
  riverpod_generator: ^2.3.0
  build_runner: ^2.4.0
  freezed: ^2.4.6
  json_serializable: ^6.7.1

# 5. 生成代码
dart run build_runner build --delete-conflicting-outputs

# 6. iOS 依赖安装
cd ios && pod install && cd ..
```

### 2.3 iOS 原生模块配置

**Info.plist 配置：**

```xml
<!-- iOS/Runner/Info.plist -->

<!-- BLE 权限 -->
<key>NSBluetoothAlwaysUsageDescription</key>
<string>需要蓝牙权限来连接车辆并解锁</string>
<key>NSBluetoothPeripheralUsageDescription</key>
<string>需要蓝牙权限来连接车辆</string>

<!-- NFC 权限 -->
<key>ISO7816_ApplicationIDs_MaxCount</key>
<array>
    <string>A000000F5A3</string>        <!-- CCC DK AID -->
    <string>A000000F5A3ICCE</string>   <!-- ICCE AID -->
</array>
<key>ISO7816_ApplicationIDs</key>
<array>
    <string>A000000F5A3</string>
    <string>A000000F5A3ICCE</string>
</array>

<!-- UWB 权限 -->
<key>NSUltraWidebandUsageDescription</key>
<string>需要超宽带权限来精确测量与车辆的距离</string>

<!-- 定位权限（用于地理围栏） -->
<key>NSLocationWhenInUseUsageDescription</key>
<string>需要位置权限来启用地理围栏功能</string>
<key>NSLocationAlwaysAndWhenInUseUsageDescription</key>
<string>需要始终访问位置以在后台使用数字钥匙</string>

<!-- 后台模式 -->
<key>UIBackgroundModes</key>
<array>
    <string>bluetooth-central</string>
    <string>location</string>
</array>
```

### 2.4 Android 原生模块配置

**AndroidManifest.xml 配置：**

```xml
<!-- android/app/src/main/AndroidManifest.xml -->

<!-- BLE 权限 -->
<uses-permission android:name="android.permission.BLUETOOTH" android:maxSdkVersion="30"/>
<uses-permission android:name="android.permission.BLUETOOTH_ADMIN" android:maxSdkVersion="30"/>
<uses-permission android:name="android.permission.BLUETOOTH_SCAN" android:usesPermissionFlags="neverForLocation"/>
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT"/>
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION"/>
<uses-permission android:name="android.permission.ACCESS_COARSE_LOCATION"/>

<!-- NFC 权限 -->
<uses-permission android:name="android.permission.NFC"/>

<!-- UWB 权限 (API 31+) -->
<uses-permission android:name="android.permission.UWB"/>

<!-- 定位权限 -->
<uses-permission android:name="android.permission.ACCESS_BACKGROUND_LOCATION"/>

<!-- UWB 特性声明 -->
<uses-feature android:name="android.hardware.uwb" android:required="false"/>
<!-- NFC 特性声明 -->
<uses-feature android:name="android.hardware.nfc" android:required="false"/>
<!-- BLE 特性声明 -->
<uses-feature android:name="android.hardware.bluetooth_le" android:required="true"/>
```

---

## 3. 代码架构设计

### 3.1 架构分层

```
┌─────────────────────────────────────────────────────────────┐
│                    展示层 (Presentation)                     │
│  Pages → ViewModels (Riverpod) → Widgets                   │
├─────────────────────────────────────────────────────────────┤
│                    领域层 (Domain)                           │
│  UseCases → Entities → Repository Interfaces                │
├─────────────────────────────────────────────────────────────┤
│                    数据层 (Data)                             │
│  Repositories → DataSources (Remote/Local) → Models        │
├─────────────────────────────────────────────────────────────┤
│                    服务层 (Services)                         │
│  AuthService │ KeyService │ VehicleService │ SharingService │
├─────────────────────────────────────────────────────────────┤
│                    协议层 (Protocol)                         │
│  ICCE SDK │ CCC SDK │ ProtocolRouter                       │
├─────────────────────────────────────────────────────────────┤
│               原生桥接层 (Platform Channel)                  │
│  MethodChannel ← Flutter → Swift/Kotlin                     │
├─────────────────────────────────────────────────────────────┤
│                    原生模块 (Native)                         │
│  BLE │ UWB │ NFC │ SecureStorage │ Biometric               │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 核心类设计

```dart
// ========== KeyRepository（钥匙仓库）====================
abstract class KeyRepository {
  /// 创建钥匙
  Future<Key> createKey({
    required String vehicleId,
    required KeyType keyType,
    required Protocol protocol,
  });
  
  /// 获取钥匙列表
  Future<List<Key>> getKeys({String? vehicleId});
  
  /// 获取钥匙详情
  Future<Key> getKey(String keyId);
  
  /// 暂停钥匙
  Future<void> suspendKey(String keyId, {String? reason});
  
  /// 恢复钥匙
  Future<void> resumeKey(String keyId);
  
  /// 吊销钥匙
  Future<void> revokeKey(String keyId, {String? reason});
  
  /// 创建分享
  Future<KeyShare> createShare({
    required String keyId,
    required List<Permission> permissions,
    required ShareValidity validity,
  });
}

// ========== VehicleRepository（车辆仓库）================
abstract class VehicleRepository {
  /// 获取车辆列表
  Future<List<Vehicle>> getVehicles();
  
  /// 获取车辆详情
  Future<Vehicle> getVehicle(String vehicleId);
  
  /// 获取车辆状态
  Future<VehicleStatus> getVehicleStatus(String vehicleId);
  
  /// 发送控制指令
  Future<ControlResult> sendControl({
    required String vehicleId,
    required ControlAction action,
    Map<String, dynamic>? parameters,
  });
}

// ========== KeyService（钥匙服务）=====================
class KeyService {
  final KeyRepository _repository;
  final ProtocolService _protocolService;
  final SecureStorageService _secureStorage;
  
  /// 钥匙创建（完整流程）
  Future<KeyCreationResult> createKeyWithPairing({
    required String vehicleId,
    required KeyType keyType,
    required Protocol protocol,
    required BuildContext context,
  }) async {
    // 1. 准备配对参数
    final pairingParams = await _preparePairingParams(protocol);
    
    // 2. 扫描车辆
    final device = await _discoverVehicle(vehicleId);
    
    // 3. BLE 连接
    await _bleService.connect(device);
    
    // 4. 协议握手
    final sessionKey = await _protocolService.pairingHandshake(
      device: device,
      protocol: protocol,
      params: pairingParams,
    );
    
    // 5. 云端注册
    final key = await _repository.createKey(
      vehicleId: vehicleId,
      keyType: keyType,
      protocol: protocol,
    );
    
    // 6. 保存密钥到安全存储
    await _secureStorage.storeSessionKey(key.keyId, sessionKey);
    
    return KeyCreationResult(key: key, sessionKey: sessionKey);
  }
  
  /// 解锁车辆（完整流程）
  Future<UnlockResult> unlockVehicle({
    required String keyId,
    required String vehicleId,
  }) async {
    // 1. 获取会话密钥
    final sessionKey = await _secureStorage.getSessionKey(keyId);
    if (sessionKey == null) {
      return UnlockResult.failed('Session key not found');
    }
    
    // 2. 连接车辆 BLE
    final bleConnection = await _bleService.connectVehicle(vehicleId);
    
    // 3. 启动 UWB 测距
    final rangingResult = await _uwbService.startRanging();
    
    // 4. 防中继检查（本地）
    if (!_antiRelayCheck(rangingResult)) {
      return UnlockResult.failed('Relay attack detected');
    }
    
    // 5. 发送解锁指令
    final result = await _protocolService.sendControl(
      connection: bleConnection,
      action: ControlAction.unlock,
      sessionKey: sessionKey,
      rangingResult: rangingResult,
    );
    
    return result;
  }
}
```

### 3.3 Riverpod Provider 设计

```dart
// providers/key_providers.dart

/// 钥匙列表 Provider
@riverpod
class KeyList extends _$KeyList {
  @override
  Future<List<Key>> build() async {
    return _repository.getKeys();
  }
  
  /// 刷新钥匙列表
  Future<void> refresh() async {
    state = const AsyncLoading();
    state = await AsyncValue.guard(() => _repository.getKeys());
  }
  
  /// 创建钥匙
  Future<Key> createKey(CreateKeyParams params) async {
    final key = await _keyService.createKeyWithPairing(
      vehicleId: params.vehicleId,
      keyType: params.keyType,
      protocol: params.protocol,
    );
    // 刷新列表
    await refresh();
    return key;
  }
  
  /// 吊销钥匙
  Future<void> revokeKey(String keyId) async {
    await _repository.revokeKey(keyId);
    await refresh();
  }
}

/// 当前激活的钥匙 Provider
@riverpod
class ActiveKey extends _$ActiveKey {
  @override
  Key? build() {
    final keyList = ref.watch(keyListProvider);
    return keyList.valueOrNull?.firstWhereOrNull(
      (k) => k.isDefault && k.status == KeyStatus.active,
    );
  }
}

/// 车辆状态 Provider
@riverpod
class VehicleStatus extends _$VehicleStatus {
  @override
  Future<VehicleStatus> build(String vehicleId) async {
    // 首次加载
    return _repository.getVehicleStatus(vehicleId);
  }
  
  /// 刷新状态
  Future<void> refresh() async {
    state = const AsyncLoading();
    state = await AsyncValue.guard(
      () => _repository.getVehicleStatus(arg),
    );
  }
  
  /// 定时刷新（每 10 秒）
  Stream<VehicleStatus> watchStatus(String vehicleId) async* {
    while (true) {
      yield await _repository.getVehicleStatus(vehicleId);
      await Future.delayed(const Duration(seconds: 10));
    }
  }
}
```

---

## 4. 核心模块开发指南

### 4.1 BLE 通信模块（原生实现）

#### Swift（iOS）

```swift
// ios/Runner/BluetoothManager.swift

import CoreBluetooth
import Flutter

class BluetoothManager: NSObject {
    private var centralManager: CBCentralManager!
    private var connectedPeripheral: CBPeripheral?
    private var channel: FlutterMethodChannel!
    
    // GATT Service UUID
    private let digitalKeyServiceUUID = CBUUID(string: "0000F5A3-0000-1000-8000-00805F9B34FB")
    private let keyPairingCharUUID = CBUUID(string: "0000F5A4-0000-1000-8000-00805F9B34FB")
    private let vehicleControlCharUUID = CBUUID(string: "0000F5A7-0000-1000-8000-00805F9B34FB")
    private let uwbRangingCharUUID = CBUUID(string: "0000F5A8-0000-1000-8000-00805F9B34FB")
    
    // 特征值缓存
    private var pairingChar: CBCharacteristic?
    private var controlChar: CBCharacteristic?
    private var uwbRangingChar: CBCharacteristic?
    
    func register(with registrar: FlutterPluginRegistrar) {
        channel = FlutterMethodChannel(
            name: "com.digitalkey.app/ble",
            binaryMessenger: registrar.messenger()
        )
        channel.setMethodCallHandler(handleMethodCall)
        
        centralManager = CBCentralManager(delegate: self, queue: nil)
    }
    
    // MARK: - Flutter MethodChannel 入口
    private func handleMethodCall(_ call: FlutterMethodCall, 
                                   result: @escaping FlutterResult) {
        switch call.method {
        case "startScan":
            startScanning(result: result)
        case "stopScan":
            stopScanning(result: result)
        case "connect":
            guard let args = call.arguments as? [String: Any],
                  let deviceId = args["deviceId"] as? String else {
                result(FlutterError(code: "INVALID_ARGS", 
                                   message: "Missing deviceId", 
                                   details: nil))
                return
            }
            connect(to: deviceId, result: result)
        case "disconnect":
            disconnect(result: result)
        case "writeCharacteristic":
            guard let args = call.arguments as? [String: Any],
                  let serviceUuid = args["serviceUuid"] as? String,
                  let charUuid = args["charUuid"] as? String,
                  let data = args["data"] as? FlutterStandardTypedData else {
                result(FlutterError(code: "INVALID_ARGS", message: nil, details: nil))
                return
            }
            writeCharacteristic(
                serviceUuid: serviceUuid, 
                charUuid: charUuid, 
                data: data.data,
                result: result
            )
        case "readCharacteristic":
            // ...
        default:
            result(FlutterMethodNotImplemented)
        }
    }
    
    // MARK: - 扫描
    private func startScanning(result: @escaping FlutterResult) {
        guard centralManager.state == .poweredOn else {
            result(FlutterError(code: "BT_OFF", 
                               message: "Bluetooth is not powered on", 
                               details: nil))
            return
        }
        
        centralManager.scanForPeripherals(
            withServices: [digitalKeyServiceUUID],
            options: [CBCentralManagerScanOptionAllowDuplicatesKey: false]
        )
        result(["status": "scanning"])
    }
    
    private func stopScanning(result: @escaping FlutterResult) {
        centralManager.stopScan()
        result(["status": "stopped"])
    }
    
    // MARK: - 连接
    private func connect(to deviceId: String, result: @escaping FlutterResult) {
        guard let peripheral = discoveredPeripherals[deviceId] else {
            result(FlutterError(code: "NOT_FOUND", 
                               message: "Device not found", 
                               details: nil))
            return
        }
        
        connectedPeripheral = peripheral
        centralManager.connect(peripheral, options: nil)
        result(["status": "connecting"])
    }
    
    // MARK: - 写特征值
    private func writeCharacteristic(serviceUuid: String, 
                                      charUuid: String, 
                                      data: Data,
                                      result: @escaping FlutterResult) {
        guard let peripheral = connectedPeripheral else {
            result(FlutterError(code: "NOT_CONNECTED", 
                               message: "Not connected", 
                               details: nil))
            return
        }
        
        // 查找特征值
        guard let service = peripheral.services?.first(where: {
            $0.uuid == CBUUID(string: serviceUuid)
        }),
        let characteristic = service.characteristics?.first(where: {
            $0.uuid == CBUUID(string: charUuid)
        }) else {
            result(FlutterError(code: "CHAR_NOT_FOUND", 
                               message: "Characteristic not found", 
                               details: nil))
            return
        }
        
        peripheral.writeValue(data, 
                             for: characteristic, 
                             type: .withResponse)
        result(["status": "written"])
    }
}

// MARK: - CBCentralManagerDelegate
extension BluetoothManager: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        channel.invokeMethod("onStateChanged", arguments: [
            "state": central.state.rawValue
        ])
    }
    
    func centralManager(_ central: CBCentralManager,
                        didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any],
                        rssi RSSI: NSNumber) {
        // 发送设备发现事件给 Flutter
        channel.invokeMethod("onDeviceDiscovered", arguments: [
            "deviceId": peripheral.identifier.uuidString,
            "name": peripheral.name ?? "Unknown",
            "rssi": RSSI.intValue,
            "advertisementData": advertisementData
        ])
    }
    
    func centralManager(_ central: CBCentralManager,
                        didConnect peripheral: CBPeripheral) {
        peripheral.delegate = self
        peripheral.discoverServices([digitalKeyServiceUUID])
        channel.invokeMethod("onConnected", arguments: [
            "deviceId": peripheral.identifier.uuidString
        ])
    }
    
    func centralManager(_ central: CBCentralManager,
                        didDisconnectPeripheral peripheral: CBPeripheral,
                        error: Error?) {
        channel.invokeMethod("onDisconnected", arguments: [
            "deviceId": peripheral.identifier.uuidString,
            "error": error?.localizedDescription ?? nil
        ])
    }
}

// MARK: - CBPeripheralDelegate
extension BluetoothManager: CBPeripheralDelegate {
    func peripheral(_ peripheral: CBPeripheral,
                    didDiscoverServices error: Error?) {
        guard let services = peripheral.services else { return }
        
        for service in services {
            peripheral.discoverCharacteristics(nil, for: service)
        }
    }
    
    func peripheral(_ peripheral: CBPeripheral,
                    didDiscoverCharacteristicsFor service: CBService,
                    error: Error?) {
        guard let characteristics = service.characteristics else { return }
        
        for char in characteristics {
            if char.uuid == keyPairingCharUUID {
                pairingChar = char
                // 订阅通知
                peripheral.setNotifyValue(true, for: char)
            } else if char.uuid == vehicleControlCharUUID {
                controlChar = char
                peripheral.setNotifyValue(true, for: char)
            } else if char.uuid == uwbRangingCharUUID {
                uwbRangingChar = char
                peripheral.setNotifyValue(true, for: char)
            }
        }
        
        // 通知 Flutter 服务发现完成
        channel.invokeMethod("onServicesDiscovered", arguments: [
            "deviceId": peripheral.identifier.uuidString,
            "services": services.map { $0.uuid.uuidString }
        ])
    }
    
    func peripheral(_ peripheral: CBPeripheral,
                    didUpdateValueFor characteristic: CBCharacteristic,
                    error: Error?) {
        guard let data = characteristic.value else { return }
        
        channel.invokeMethod("onCharacteristicChanged", arguments: [
            "deviceId": peripheral.identifier.uuidString,
            "characteristicUuid": characteristic.uuid.uuidString,
            "value": FlutterStandardTypedData(bytes: data)
        ])
    }
}
```

#### Kotlin（Android）

```kotlin
// android/app/src/main/kotlin/.../BluetoothManager.kt

package com.digitalkey.app

import android.bluetooth.*
import android.bluetooth.le.*
import android.content.Context
import android.os.ParcelUuid
import io.flutter.embedding.engine.plugins.FlutterPlugin
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel
import kotlinx.coroutines.*

class BluetoothManager(context: Context) : FlutterPlugin, MethodChannel.MethodCallHandler {
    
    private lateinit var channel: MethodChannel
    private val context = context
    
    private val bluetoothManager: BluetoothManager by lazy {
        context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
    }
    
    private var bluetoothAdapter: BluetoothAdapter? = bluetoothManager.adapter
    private var bluetoothLeScanner: BluetoothLeScanner? = bluetoothAdapter?.bluetoothLeScanner
    private var connectedDevice: BluetoothDevice? = null
    private var bluetoothGatt: BluetoothGatt? = null
    
    // GATT UUIDs
    private val digitalKeyServiceUUID = UUID.fromString("0000F5A3-0000-1000-8000-00805F9B34FB")
    private val keyPairingCharUUID = UUID.fromString("0000F5A4-0000-1000-8000-00805F9B34FB")
    private val vehicleControlCharUUID = UUID.fromString("0000F5A7-0000-1000-8000-00805F9B34FB")
    
    override fun onAttachedToEngine(binding: FlutterPlugin.FlutterPluginBinding) {
        channel = MethodChannel(binding.binaryMessenger, "com.digitalkey.app/ble")
        channel.setMethodCallHandler(this)
    }
    
    override fun onMethodCall(call: MethodCall, result: MethodChannel.Result) {
        when (call.method) {
            "startScan" -> startScan(result)
            "stopScan" -> stopScan(result)
            "connect" -> connect(call.argument<String>("deviceId")!!, result)
            "disconnect" -> disconnect(result)
            "writeCharacteristic" -> writeCharacteristic(
                call.argument<String>("serviceUuid")!!,
                call.argument<String>("charUuid")!!,
                call.argument<ByteArray>("data")!!,
                result
            )
            else -> result.notImplemented()
        }
    }
    
    private fun startScan(result: MethodChannel.Result) {
        val scanSettings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()
        
        bluetoothLeScanner?.startScan(
            listOf(android.os.ParcelUuid(digitalKeyServiceUUID)),
            scanSettings,
            scanCallback
        )
        result.success(mapOf("status" to "scanning"))
    }
    
    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            channel.invokeMethod("onDeviceDiscovered", mapOf(
                "deviceId" to result.device.address,
                "name" to (result.device.name ?: "Unknown"),
                "rssi" to result.rssi
            ))
        }
        
        override fun onScanFailed(errorCode: Int) {
            channel.invokeMethod("onScanFailed", mapOf("errorCode" to errorCode))
        }
    }
    
    private fun connect(deviceId: String, result: MethodChannel.Result) {
        val device = bluetoothAdapter?.getRemoteDevice(deviceId)
        connectedDevice = device
        bluetoothGatt = device?.connectGatt(context, false, gattCallback)
        result.success(mapOf("status" to "connecting"))
    }
    
    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    gatt.discoverServices()
                    channel.invokeMethod("onConnected", null)
                }
                BluetoothProfile.STATE_DISCONNECTED -> {
                    channel.invokeMethod("onDisconnected", null)
                }
            }
        }
        
        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                val services = gatt.services.map { it.uuid.toString() }
                channel.invokeMethod("onServicesDiscovered", mapOf("services" to services))
            }
        }
        
        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            value: ByteArray
        ) {
            channel.invokeMethod("onCharacteristicChanged", mapOf(
                "characteristicUuid" to characteristic.uuid.toString(),
                "value" to value
            ))
        }
    }
}
```

### 4.2 Flutter 桥接层

```dart
// lib/services/ble_bridge.dart

class BLEBridge {
  static const _channel = MethodChannel('com.digitalkey.app/ble');
  
  // 设备流
  Stream<BLEScanResult> get onDeviceDiscovered => 
      _channel
          .receiveBroadcastStream('onDeviceDiscovered')
          .map((e) => BLEScanResult.fromMap(Map<String, dynamic>.from(e)));
  
  // 连接状态流
  Stream<BLEConnectionState> get onConnectionStateChanged =>
      _channel
          .receiveBroadcastStream('onStateChanged')
          .map((e) => BLEConnectionState.fromMap(Map<String, dynamic>.from(e)));
  
  /// 开始扫描
  Future<void> startScan() async {
    await _channel.invokeMethod('startScan');
  }
  
  /// 停止扫描
  Future<void> stopScan() async {
    await _channel.invokeMethod('stopScan');
  }
  
  /// 连接设备
  Future<void> connect(String deviceId) async {
    await _channel.invokeMethod('connect', {'deviceId': deviceId});
  }
  
  /// 断开连接
  Future<void> disconnect() async {
    await _channel.invokeMethod('disconnect');
  }
  
  /// 写入特征值
  Future<void> writeCharacteristic({
    required String serviceUuid,
    required String charUuid,
    required Uint8List data,
  }) async {
    await _channel.invokeMethod('writeCharacteristic', {
      'serviceUuid': serviceUuid,
      'charUuid': charUuid,
      'data': FlutterStandardTypedData bytes: data,
    });
  }
  
  /// 订阅特征值通知
  Stream<Uint8List> subscribeCharacteristic(String charUuid) =>
      _channel
          .receiveBroadcastStream('onCharacteristicChanged')
          .where((e) => e['characteristicUuid'] == charUuid)
          .map((e) => Uint8List.fromList(List<int>.from(e['value'])));
}

@freezed
class BLEScanResult with _$BLEScanResult {
  const factory BLEScanResult({
    required String deviceId,
    required String name,
    required int rssi,
  }) = _BLEScanResult;
  
  factory BLEScanResult.fromMap(Map<String, dynamic> map) =>
      BLEScanResult(
        deviceId: map['deviceId'] as String,
        name: map['name'] as String,
        rssi: map['rssi'] as int,
      );
}
```

### 4.3 UWB 测距模块

```swift
// iOS UWB 实现

import Foundation
import NearbyInteraction

class UWBManager: NSObject, FlutterPlugin {
    private var session: NISession?
    private var channel: FlutterMethodChannel!
    
    func register(with registrar: FlutterPluginRegistrar) {
        channel = FlutterMethodChannel(
            name: "com.digitalkey.app/uwb",
            binaryMessenger: registrar.messenger()
        )
        channel.setMethodCallHandler(handleMethodCall)
    }
    
    private func handleMethodCall(_ call: MethodCall, result: @escaping FlutterResult) {
        switch call.method {
        case "isSupported":
            result(NISession.isSupported)
        case "startRanging":
            startRanging(result: result)
        case "stopRanging":
            stopRanging(result: result)
        default:
            result(FlutterMethodNotImplemented)
        }
    }
    
    private func startRanging(result: @escaping FlutterResult) {
        guard NISession.isSupported else {
            result(FlutterError(code: "NOT_SUPPORTED", 
                               message: "UWB not supported", 
                               details: nil))
            return
        }
        
        session = NISession()
        session?.delegate = self
        
        // 配置 UWB 会话
        let config = NIConfiguration.default
        config.isAutoPassingUpdatesToPeer = true
        
        session?.run(config)
        result(["status": "ranging"])
    }
    
    private func stopRanging() {
        session?.invalidate()
        session = nil
    }
}

extension UWBManager: NISessionDelegate {
    func session(_ session: NISession, 
                 didUpdate data: NINearbyObject) {
        // 计算距离
        let distance = data.distance ?? 0
        let direction = data.direction
        
        // 发送结果到 Flutter
        channel.invokeMethod("onRangingResult", arguments: [
            "distance": distance,
            "azimuth": direction?.azimuth ?? 0,
            "elevation": direction?.elevation ?? 0,
            "timestamp": Date().timeIntervalSince1970 * 1000
        ])
    }
    
    func session(_ session: NISession, 
                 didRemove nearbyObjects: [NINearbyObject], 
                 with reason: NINearbyObject.RemovalReason) {
        channel.invokeMethod("onRangingEnded", arguments: [
            "reason": reason.rawValue
        ])
    }
}
```

---

## 5. 关键算法/流程实现

### 5.1 钥匙配对流程

```dart
// lib/services/pairing_service.dart

class PairingService {
  final BLEBridge _ble;
  final UWBService _uwb;
  final ProtocolService _protocol;
  final SecureStorageService _secureStorage;
  final KeyRepository _keyRepository;
  
  /// 完整配对流程
  Future<PairingResult> executePairing({
    required String vehicleId,
    required String vin,
    required KeyType keyType,
    required Protocol protocol,
  }) async {
    try {
      // ========== 阶段1: 设备发现 ==========
      print('阶段1: 发现车辆...');
      final device = await _discoverVehicle(vehicleId);
      print('发现设备: ${device.name}');
      
      // ========== 阶段2: BLE 连接 ==========
      print('阶段2: 建立 BLE 连接...');
      await _ble.connect(device.deviceId);
      await _ble.waitForServicesDiscovered();
      print('BLE 已连接');
      
      // ========== 阶段3: 协议协商 ==========
      print('阶段3: 协议协商...');
      final negotiatedProtocol = await _protocol.negotiate(
        bleBridge: _ble,
        vehicleProtocol: protocol,
      );
      print('协商协议: $negotiatedProtocol');
      
      // ========== 阶段4: 密钥协商 ==========
      print('阶段4: 密钥协商...');
      final keyPair = await _generateKeyPair(negotiatedProtocol);
      
      final sessionKey = await _protocol.keyAgreement(
        bleBridge: _ble,
        protocol: negotiatedProtocol,
        localPublicKey: keyPair.publicKey,
      );
      print('会话密钥建立成功');
      
      // ========== 阶段5: 云端注册 ==========
      print('阶段5: 云端注册钥匙...');
      final key = await _keyRepository.createKey(
        vehicleId: vehicleId,
        keyType: keyType,
        protocol: negotiatedProtocol,
        publicKey: keyPair.publicKey,
      );
      
      // ========== 阶段6: 安全存储 ==========
      print('阶段6: 保存密钥...');
      await _secureStorage.storeKeyPair(
        keyId: key.keyId,
        privateKey: keyPair.privateKey,
        sessionKey: sessionKey,
      );
      
      // ========== 阶段7: UWB 校准 ==========
      print('阶段7: UWB 测距校准...');
      await _uwb.calibrate(_ble);
      print('UWB 校准完成');
      
      // ========== 完成 ==========
      await _ble.disconnect();
      print('配对完成! keyId: ${key.keyId}');
      
      return PairingResult.success(key: key);
      
    } on PairingException catch (e) {
      print('配对失败: ${e.message}');
      await _cleanup();
      return PairingResult.failed(e.message);
    }
  }
  
  /// 发现车辆
  Future<BLEDevice> _discoverVehicle(String vehicleId) async {
    await _ble.startScan();
    
    await for (final result in _ble.onDeviceDiscovered) {
      // 匹配目标车辆
      if (result.deviceId == vehicleId || result.name.contains('DigitalKey')) {
        await _ble.stopScan();
        return BLEDevice(
          deviceId: result.deviceId,
          name: result.name,
          rssi: result.rssi,
        );
      }
    }
    
    throw PairingException('未找到车辆设备');
  }
}
```

### 5.2 钥匙解锁流程

```dart
// lib/services/key_control_service.dart

class KeyControlService {
  final BLEBridge _ble;
  final UWBService _uwb;
  final ProtocolService _protocol;
  final SecureStorageService _secureStorage;
  
  /// 解锁车辆
  Future<UnlockResult> unlock({
    required String keyId,
    required String vehicleId,
  }) async {
    final stopwatch = Stopwatch()..start();
    
    try {
      // 1. 获取会话密钥
      final sessionKey = await _secureStorage.getSessionKey(keyId);
      if (sessionKey == null) {
        return UnlockResult.failed('未找到会话密钥，请重新配对');
      }
      
      // 2. BLE 连接
      await _ble.connect(vehicleId);
      await _ble.waitForServicesDiscovered();
      
      // 3. UWB 测距
      final ranging = await _uwb.startRanging();
      
      // 4. 本地防中继检查
      if (!_antiRelayCheck(ranging)) {
        return UnlockResult.failed('安全检测未通过，请重试');
      }
      
      // 5. 发送解锁指令
      final command = VehicleControlCommand(
        action: ControlAction.unlock,
        authToken: sessionKey,
        rangingResult: ranging,
        timestamp: DateTime.now().millisecondsSinceEpoch,
      );
      
      final response = await _protocol.sendControl(
        bleBridge: _ble,
        command: command,
      );
      
      stopwatch.stop();
      
      if (response.success) {
        // 记录审计日志
        _auditLog.logUnlock(
          keyId: keyId,
          vehicleId: vehicleId,
          duration: stopwatch.elapsedMilliseconds,
          distance: ranging.distance,
          location: ranging.location,
        );
        
        return UnlockResult.success(
          duration: stopwatch.elapsedMilliseconds,
          feedback: '解锁成功',
        );
      } else {
        return UnlockResult.failed(response.errorMessage ?? '解锁失败');
      }
      
    } finally {
      await _ble.disconnect();
      _uwb.stopRanging();
    }
  }
  
  /// 本地防中继检查
  bool _antiRelayCheck(UWBRangingResult ranging) {
    // 距离检查
    if (ranging.distance > 2.0) {
      print('距离超出范围: ${ranging.distance}m');
      return false;
    }
    
    // 置信度检查
    if (ranging.confidence < 0.85) {
      print('测距置信度不足: ${ranging.confidence}');
      return false;
    }
    
    // 安全测距检查
    if (!ranging.isSecure) {
      print('非安全测距，拒绝解锁');
      return false;
    }
    
    return true;
  }
}
```

### 5.3 离线模式处理

```dart
// lib/services/offline_service.dart

class OfflineService {
  final SecureStorageService _secureStorage;
  final KeyRepository _keyRepository;
  final AuditLogService _auditLog;
  
  /// 检查离线能力
  bool canOperateOffline(String keyId) async {
    final keyInfo = await _secureStorage.getKeyInfo(keyId);
    if (keyInfo == null) return false;
    
    // 检查有效期
    final now = DateTime.now();
    if (keyInfo.validUntil != null && now.isAfter(keyInfo.validUntil!)) {
      return false;
    }
    
    // 检查剩余使用次数
    if (keyInfo.maxUsageCount != -1 && 
        keyInfo.usageCount >= keyInfo.maxUsageCount) {
      return false;
    }
    
    return true;
  }
  
  /// 离线解锁（NFC）
  Future<UnlockResult> offlineUnlock({
    required String keyId,
    required Uint8List nfcData,
  }) async {
    // 验证 NFC 数据
    final sessionKey = await _secureStorage.getSessionKey(keyId);
    if (sessionKey == null) {
      return UnlockResult.failed('未找到离线密钥');
    }
    
    // 解析 NFC 响应
    final nfcResponse = NFCResponse.decode(nfcData);
    
    // 验证时间戳（允许 ±5 分钟漂移）
    final now = DateTime.now().millisecondsSinceEpoch;
    final drift = (now - nfcResponse.timestamp).abs();
    if (drift > 5 * 60 * 1000) {
      return UnlockResult.failed('NFC 凭证已过期');
    }
    
    // 验证计数器
    final lastCounter = await _secureStorage.getLastCounter(keyId);
    if (nfcResponse.counter <= lastCounter) {
      return UnlockResult.failed('防重放检查失败');
    }
    
    // 解锁（通过本地验证后生成解锁指令）
    // 注意：离线模式下无法验证 UWB，需要更高安全级别
    
    // 更新计数器
    await _secureStorage.setLastCounter(keyId, nfcResponse.counter);
    
    // 记录离线操作（待网络恢复后同步）
    await _auditLog.queueOfflineOperation(
      OfflineOperation.unlock(keyId: keyId, timestamp: nfcResponse.timestamp),
    );
    
    return UnlockResult.success(
      feedback: '离线解锁成功（将在联网后同步）',
    );
  }
  
  /// 联网后同步离线操作
  Future<void> syncOfflineOperations() async {
    final pendingOps = await _auditLog.getPendingOperations();
    
    for (final op in pendingOps) {
      try {
        await _keyRepository.syncOfflineOperation(op);
        await _auditLog.markSynced(op.id);
      } catch (e) {
        print('同步失败: $e');
        // 下次再试
      }
    }
  }
}
```

---

## 6. 接口调用示例

### 6.1 用户登录

```dart
// lib/services/auth_service.dart

class AuthService {
  final Dio _dio;
  final SecureStorageService _secureStorage;
  
  /// 手机号 + 验证码登录
  Future<AuthResult> loginWithPhone({
    required String phone,
    required String code,
    required DeviceInfo deviceInfo,
  }) async {
    final response = await _dio.post('/auth/login', data: {
      'phone': phone,
      'code': code,
      'deviceInfo': deviceInfo.toJson(),
    });
    
    final data = response.data['data'];
    
    // 保存 Token
    await _secureStorage.storeTokens(
      accessToken: data['accessToken'],
      refreshToken: data['refreshToken'],
    );
    
    return AuthResult(
      userId: data['userId'],
      accessToken: data['accessToken'],
      refreshToken: data['refreshToken'],
    );
  }
  
  /// 刷新 Token
  Future<String> refreshToken() async {
    final refreshToken = await _secureStorage.getRefreshToken();
    if (refreshToken == null) {
      throw AuthException('Refresh token not found');
    }
    
    final response = await _dio.post('/auth/token/refresh', data: {
      'refreshToken': refreshToken,
    });
    
    final data = response.data['data'];
    
    // 更新存储的 Token
    await _secureStorage.storeTokens(
      accessToken: data['accessToken'],
      refreshToken: data['refreshToken'],
    );
    
    return data['accessToken'];
  }
}
```

### 6.2 创建分享

```dart
// lib/services/sharing_service.dart

class SharingService {
  final Dio _dio;
  
  /// 创建钥匙分享
  Future<ShareResult> createShare({
    required String keyId,
    required List<Permission> permissions,
    required ShareValidity validity,
    String? targetPhone,
    String? message,
    GeoFence? geoFence,
  }) async {
    final response = await _dio.post('/keys/$keyId/shares', data: {
      'shareType': targetPhone != null ? 'PHONE' : 'LINK',
      'targetPhone': targetPhone,
      'permissions': permissions.map((p) => p.name).toList(),
      'validity': validity.toJson(),
      'message': message,
      'restrictions': {
        'geoFence': geoFence?.toJson(),
      },
    });
    
    final data = response.data['data'];
    
    return ShareResult(
      shareId: data['shareId'],
      shareLink: data['shareLink'],
      shareCode: data['shareCode'],
      expiresAt: DateTime.parse(data['expiresAt']),
    );
  }
  
  /// 撤销分享
  Future<void> revokeShare(String keyId, String shareId) async {
    await _dio.delete('/keys/$keyId/shares/$shareId');
  }
}
```

### 6.3 远程控车

```dart
// lib/services/remote_control_service.dart

class RemoteControlService {
  final Dio _dio;
  final VehicleRepository _vehicleRepository;
  
  /// 发送远程控制指令
  Future<ControlResult> sendControl({
    required String vehicleId,
    required ControlAction action,
    Map<String, dynamic>? parameters,
    int timeoutSeconds = 30,
  }) async {
    // 先检查车辆是否在线
    final status = await _vehicleRepository.getVehicleStatus(vehicleId);
    if (!status.online) {
      return ControlResult.failed(
        '车辆离线，请检查网络连接',
        errorCode: 'VEHICLE_OFFLINE',
      );
    }
    
    // 发送控制指令
    final response = await _dio.post(
      '/vehicles/$vehicleId/controls',
      data: {
        'action': action.name,
        'parameters': parameters,
        'priority': 'NORMAL',
        'timeout': timeoutSeconds,
      },
    );
    
    final data = response.data['data'];
    final requestId = data['requestId'];
    
    // 等待执行结果（轮询或 WebSocket）
    return _waitForResult(vehicleId, requestId, timeoutSeconds);
  }
  
  /// 等待控制结果
  Future<ControlResult> _waitForResult(
    String vehicleId,
    String requestId,
    int timeoutSeconds,
  ) async {
    final deadline = DateTime.now().add(Duration(seconds: timeoutSeconds));
    
    while (DateTime.now().isBefore(deadline)) {
      await Future.delayed(const Duration(milliseconds: 500));
      
      final response = await _dio.get(
        '/vehicles/$vehicleId/controls/$requestId',
      );
      
      final data = response.data['data'];
      final status = ControlStatus.values.byName(data['status']);
      
      switch (status) {
        case ControlStatus.executed:
          return ControlResult.success(
            executedAt: DateTime.parse(data['executedAt']),
          );
        case ControlStatus.failed:
          return ControlResult.failed(
            data['error'] ?? '执行失败',
            errorCode: data['errorCode'],
          );
        case ControlStatus.timeout:
          return ControlResult.failed(
            '执行超时',
            errorCode: 'TIMEOUT',
          );
        case ControlStatus.pending:
        case ControlStatus.executing:
          continue;
      }
    }
    
    return ControlResult.failed('等待超时', errorCode: 'TIMEOUT');
  }
}
```

---

## 7. 测试要点

### 7.1 单元测试

```dart
// test/services/key_control_service_test.dart

void main() {
  group('KeyControlService', () {
    late MockBLEBridge mockBLE;
    late MockUWBService mockUWB;
    late MockProtocolService mockProtocol;
    late MockSecureStorage mockSecureStorage;
    late KeyControlService service;
    
    setUp(() {
      mockBLE = MockBLEBridge();
      mockUWB = MockUWBService();
      mockProtocol = MockProtocolService();
      mockSecureStorage = MockSecureStorage();
      
      service = KeyControlService(
        ble: mockBLE,
        uwb: mockUWB,
        protocol: mockProtocol,
        secureStorage: mockSecureStorage,
      );
    });
    
    test('解锁成功 - 距离正常', () async {
      // Arrange
      when(() => mockSecureStorage.getSessionKey(any()))
          .thenAnswer((_) async => 'session_key_123');
      when(() => mockBLE.connect(any())).thenAnswer((_) async {});
      when(() => mockBLE.waitForServicesDiscovered())
          .thenAnswer((_) async {});
      when(() => mockBLE.disconnect()).thenAnswer((_) async {});
      
      when(() => mockUWB.startRanging()).thenAnswer((_) async =>
          UWBRangingResult(
            distance: 1.2,
            confidence: 0.95,
            isSecure: true,
          ));
      when(() => mockUWB.stopRanging()).thenAnswer((_) async {});
      
      when(() => mockProtocol.sendControl(any(), any()))
          .thenAnswer((_) async =>
              VehicleControlResponse(success: true));
      
      // Act
      final result = await service.unlock(
        keyId: 'key_123',
        vehicleId: 'vehicle_456',
      );
      
      // Assert
      expect(result.success, true);
      verify(() => mockBLE.connect('vehicle_456')).called(1);
      verify(() => mockUWB.startRanging()).called(1);
    });
    
    test('解锁失败 - 距离超出阈值', () async {
      // Arrange
      when(() => mockSecureStorage.getSessionKey(any()))
          .thenAnswer((_) async => 'session_key_123');
      when(() => mockBLE.connect(any())).thenAnswer((_) async {});
      when(() => mockBLE.disconnect()).thenAnswer((_) async {});
      
      // UWB 返回 5 米距离
      when(() => mockUWB.startRanging()).thenAnswer((_) async =>
          UWBRangingResult(
            distance: 5.0,  // 超出 2m 阈值
            confidence: 0.95,
            isSecure: true,
          ));
      when(() => mockUWB.stopRanging()).thenAnswer((_) async {});
      
      // Act
      final result = await service.unlock(
        keyId: 'key_123',
        vehicleId: 'vehicle_456',
      );
      
      // Assert
      expect(result.success, false);
      expect(result.errorMessage, contains('安全检测'));
    });
    
    test('解锁失败 - 会话密钥不存在', () async {
      // Arrange
      when(() => mockSecureStorage.getSessionKey(any()))
          .thenAnswer((_) async => null);
      
      // Act
      final result = await service.unlock(
        keyId: 'key_invalid',
        vehicleId: 'vehicle_456',
      );
      
      // Assert
      expect(result.success, false);
      expect(result.errorMessage, contains('会话密钥'));
    });
  });
}
```

### 7.2 集成测试

| 测试项 | 方法 | 验收标准 |
|-------|------|---------|
| 配对流程 | E2E 测试：扫码→配对→激活 | 成功率 ≥ 99% |
| 解锁响应时间 | 计时：BLE 连接→车门解锁 | P99 < 1s |
| 多车辆切换 | 连续切换 5 辆车 | 无异常、无数据丢失 |
| 兼容性 | 主流机型测试（华为、小米、iPhone） | 功能正常 |
| 离线解锁 | 手机飞行模式 + NFC 刷卡 | 解锁成功 |
| 性能 | App 冷启动时间 | < 2s |
| 稳定性 | Monkey 测试 24h | 无 Crash |

### 7.3 安全测试

| 测试项 | 方法 | 验收标准 |
|-------|------|---------|
| 密钥安全存储 | 安全审计工具检测 | Keychain/Keystore 存储 |
| 生物认证 | 验证 Face ID / 指纹集成 | 功能正常 |
| 防调试 | 检测 Frida / Xposed | 检测到后拒绝操作 |
| 通信加密 | 抓包分析 | BLE 数据加密 |
| Token 安全 | 检查 Token 存储和传输 | 无明文泄露 |

---

## 8. 参考资源

| 资源 | 链接 |
|------|------|
| Flutter 文档 | https://flutter.dev/docs |
| Flutter Riverpod | https://riverpod.dev |
| Dart gRPC | https://grpc.io/docs/languages/dart |
| iOS CoreBluetooth | https://developer.apple.com/documentation/corebluetooth |
| iOS CoreNFC | https://developer.apple.com/documentation/corenfc |
| iOS NearbyInteraction | https://developer.apple.com/documentation/nearbyinteraction |
| Android BLE | https://developer.android.com/guide/topics/connectivity/bluetooth |
| Android UWB | https://developer.android.com/guide/topics/connectivity/uwb |
| Android HCE | https://developer.android.com/guide/topics/connectivity/nfc/hce |
| Fastlane | https://docs.fastlane.tools |
| ICCE 规范 | T/CA 110-2020 |
| CCC Digital Key | https://carconnectivity.org/digital-key |

---

*文档版本：v1.0 | 最后更新：2026-04-26 | 作者：code_designer*
