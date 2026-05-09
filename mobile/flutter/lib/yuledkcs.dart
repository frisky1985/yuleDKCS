library yuledkcs;

export 'src/ffi_bridge.dart';
export 'src/key_manager.dart';
export 'src/ble_manager.dart';

import 'dart:async';
import 'dart:typed_data';
import 'src/ffi_bridge.dart';
import 'src/key_manager.dart';
import 'src/ble_manager.dart';

/// YuleDKCS SDK 主入口类
class YuleDKCS {
  static final YuleDKCS _instance = YuleDKCS._internal();
  factory YuleDKCS() => _instance;
  YuleDKCS._internal();

  static final FFIBridge _ffi = FFIBridge();
  static final KeyManager _keyManager = KeyManager();
  static final BLEManager _bleManager = BLEManager();

  static bool _initialized = false;
  static String? _apiKey;

  /// 初始化 SDK
  /// 
  /// [apiKey] - API 密钥
  static Future<void> initialize(String apiKey) async {
    if (_initialized) return;

    _apiKey = apiKey;
    
    // 初始化 FFI 库
    await _ffi.initialize();
    
    // 初始化 BLE
    await _bleManager.initialize();
    
    _initialized = true;
  }

  /// 发放数字钥匙
  /// 
  /// [vehicleId] - 车辆 ID
  /// 返回发放的 DigitalKey 对象
  static Future<DigitalKey> issueKey(String vehicleId) async {
    _ensureInitialized();
    return await _keyManager.issueKey(vehicleId, _apiKey!);
  }

  /// 列出所有钥匙
  /// 
  /// 返回钥匙列表
  static List<DigitalKey> listKeys() {
    _ensureInitialized();
    return _keyManager.listKeys();
  }

  /// 共享钥匙
  /// 
  /// [keyId] - 钥匙 ID
  /// [to] - 接收者用户 ID
  /// [permissions] - 权限列表
  static Future<void> shareKey(
    String keyId,
    String to,
    List<Permission> permissions,
  ) async {
    _ensureInitialized();
    await _keyManager.shareKey(keyId, to, permissions);
  }

  /// 撤销钥匙
  /// 
  /// [keyId] - 钥匙 ID
  static Future<void> revokeKey(String keyId) async {
    _ensureInitialized();
    await _keyManager.revokeKey(keyId);
  }

  /// 连接到车辆 BLE
  /// 
  /// [address] - BLE 设备地址
  static Future<void> connect(String address) async {
    _ensureInitialized();
    await _bleManager.connect(address);
  }

  /// 断开连接
  static Future<void> disconnect() async {
    await _bleManager.disconnect();
  }

  /// 发送命令到车辆
  /// 
  /// [command] - 要发送的命令
  static Future<void> sendCommand(Command command) async {
    _ensureInitialized();
    await _bleManager.sendCommand(command);
  }

  /// 获取连接状态
  static ConnectionState get connectionState => _bleManager.connectionState;

  /// 连接状态流
  static Stream<ConnectionState> get connectionStateStream => 
      _bleManager.connectionStateStream;

  static void _ensureInitialized() {
    if (!_initialized) {
      throw StateError('YuleDKCS not initialized. Call initialize() first.');
    }
  }
}

/// 数字钥匙数据类
class DigitalKey {
  final String keyId;
  final String vehicleId;
  final String ownerId;
  final List<Permission> permissions;
  final DateTime issuedAt;
  final DateTime? expiresAt;
  final KeyStatus status;

  DigitalKey({
    required this.keyId,
    required this.vehicleId,
    required this.ownerId,
    required this.permissions,
    required this.issuedAt,
    this.expiresAt,
    this.status = KeyStatus.active,
  });
}

/// 权限枚举
enum Permission {
  unlock,
  lock,
  startEngine,
  openTrunk,
  openWindows,
  climateControl,
}

/// 钥匙状态枚举
enum KeyStatus {
  active,
  expired,
  revoked,
  suspended,
}

/// 命令枚举
enum Command {
  unlock,
  lock,
  startEngine,
  stopEngine,
  openTrunk,
  closeTrunk,
  openWindows,
  closeWindows,
  startClimate,
  stopClimate,
}

/// 连接状态枚举
enum ConnectionState {
  disconnected,
  scanning,
  connecting,
  connected,
  authenticating,
  ready,
  error,
}