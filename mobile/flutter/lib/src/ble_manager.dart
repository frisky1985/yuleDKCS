import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:permission_handler/permission_handler.dart';
import 'ffi_bridge.dart';
import '../yuledkcs.dart';

/// BLE 管理类
class BLEManager {
  final FFIBridge _ffi = FFIBridge();
  
  BluetoothDevice? _connectedDevice;
  BluetoothCharacteristic? _commandCharacteristic;
  BluetoothCharacteristic? _responseCharacteristic;
  
  final _connectionStateController = StreamController<ConnectionState>.broadcast();
  ConnectionState _connectionState = ConnectionState.disconnected;
  
  // BLE UUIDs
  static final String serviceUuid = '0000dkcs-0000-1000-8000-00805f9b34fb';
  static final String commandCharUuid = '0000dkc1-0000-1000-8000-00805f9b34fb';
  static final String responseCharUuid = '0000dkc2-0000-1000-8000-00805f9b34fb';

  /// 获取连接状态流
  Stream<ConnectionState> get connectionStateStream => _connectionStateController.stream;
  
  /// 获取当前连接状态
  ConnectionState get connectionState => _connectionState;

  /// 初始化 BLE
  Future<void> initialize() async {
    // 请求必要的权限
    await _requestPermissions();
    
    // 监听蓝牙状态
    FlutterBluePlus.adapterState.listen((state) {
      if (state == BluetoothAdapterState.off) {
        _updateConnectionState(ConnectionState.error);
      }
    });
  }

  /// 请求 BLE 权限
  Future<void> _requestPermissions() async {
    if (await Permission.bluetooth.isDenied) {
      await Permission.bluetooth.request();
    }
    if (await Permission.bluetoothScan.isDenied) {
      await Permission.bluetoothScan.request();
    }
    if (await Permission.bluetoothConnect.isDenied) {
      await Permission.bluetoothConnect.request();
    }
    if (await Permission.location.isDenied) {
      await Permission.location.request();
    }
  }

  /// 连接到 BLE 设备
  Future<void> connect(String address) async {
    _updateConnectionState(ConnectionState.scanning);
    
    try {
      // 先调用 FFI 准备连接
      final resultJson = _ffi.connect(address);
      final result = jsonDecode(resultJson) as Map<String, dynamic>;
      
      if (result['success'] != true) {
        throw BLEException(result['error'] ?? 'FFI connect failed');
      }

      // 停止扫描
      await FlutterBluePlus.stopScan();
      
      // 查找设备
      _updateConnectionState(ConnectionState.connecting);
      
      // 从地址创建设备
      final device = BluetoothDevice.fromId(address);
      _connectedDevice = device;
      
      // 连接设备
      await device.connect(autoConnect: false, mtu: null);
      
      // 发现服务
      final services = await device.discoverServices();
      
      // 查找 DKCS 服务
      final service = services.firstWhere(
        (s) => s.uuid.toString().toLowerCase() == serviceUuid.toLowerCase(),
        orElse: () => throw BLEException('DKCS service not found'),
      );
      
      // 查找特征
      for (final characteristic in service.characteristics) {
        final uuid = characteristic.uuid.toString().toLowerCase();
        if (uuid == commandCharUuid.toLowerCase()) {
          _commandCharacteristic = characteristic;
        } else if (uuid == responseCharUuid.toLowerCase()) {
          _responseCharacteristic = characteristic;
        }
      }
      
      if (_commandCharacteristic == null || _responseCharacteristic == null) {
        throw BLEException('Required characteristics not found');
      }
      
      // 启用响应通知
      await _responseCharacteristic!.setNotifyValue(true);
      
      _updateConnectionState(ConnectionState.authenticating);
      
      // 执行认证握手
      await _authenticate();
      
      _updateConnectionState(ConnectionState.ready);
      
    } catch (e) {
      _updateConnectionState(ConnectionState.error);
      throw BLEException('Connection failed: $e');
    }
  }

  /// 断开连接
  Future<void> disconnect() async {
    if (_connectedDevice != null) {
      await _connectedDevice!.disconnect();
      _connectedDevice = null;
      _commandCharacteristic = null;
      _responseCharacteristic = null;
      _updateConnectionState(ConnectionState.disconnected);
    }
  }

  /// 发送命令
  Future<void> sendCommand(Command command) async {
    if (_connectionState != ConnectionState.ready) {
      throw BLEException('Not connected to vehicle');
    }
    
    if (_commandCharacteristic == null) {
      throw BLEException('Command characteristic not available');
    }
    
    // 调用 FFI 准备命令
    final resultJson = _ffi.sendCommand(command.name);
    final result = jsonDecode(resultJson) as Map<String, dynamic>;
    
    if (result['success'] != true) {
      throw BLEException(result['error'] ?? 'FFI command preparation failed');
    }
    
    final commandData = base64Decode(result['data']['payload'] as String);
    
    // 分片发送（如果数据大于 MTU）
    await _sendDataInChunks(commandData);
    
    // 等待响应
    final completer = Completer<void>();
    late StreamSubscription subscription;
    
    subscription = _responseCharacteristic!.lastValueStream.listen((value) {
      if (value.isNotEmpty) {
        subscription.cancel();
        completer.complete();
      }
    });
    
    await completer.future.timeout(
      const Duration(seconds: 5),
      onTimeout: () {
        subscription.cancel();
        throw BLEException('Command timeout');
      },
    );
  }

  /// 分片发送数据
  Future<void> _sendDataInChunks(Uint8List data) async {
    const chunkSize = 512; // BLE MTU 限制
    
    for (var i = 0; i < data.length; i += chunkSize) {
      final end = (i + chunkSize < data.length) ? i + chunkSize : data.length;
      final chunk = data.sublist(i, end);
      
      await _commandCharacteristic!.write(
        chunk,
        withoutResponse: false,
      );
    }
  }

  /// 认证握手
  Future<void> _authenticate() async {
    // 实现认证逻辑
    // 1. 发送认证请求
    // 2. 等待挑战
    // 3. 发送签名响应
    // 4. 验证成功
    
    // 这里使用 FFI 进行认证
    final resultJson = _ffi.sendCommand('authenticate');
    final result = jsonDecode(resultJson) as Map<String, dynamic>;
    
    if (result['success'] != true) {
      throw BLEException('Authentication failed');
    }
  }

  /// 更新连接状态
  void _updateConnectionState(ConnectionState state) {
    _connectionState = state;
    _connectionStateController.add(state);
  }

  /// 扫描设备
  Stream<ScanResult> scanDevices({Duration timeout = const Duration(seconds: 10)}) {
    FlutterBluePlus.startScan(
      withServices: [Guid(serviceUuid)],
      timeout: timeout,
    );
    return FlutterBluePlus.scanResults;
  }

  /// 停止扫描
  Future<void> stopScan() async {
    await FlutterBluePlus.stopScan();
  }

  /// 释放资源
  void dispose() {
    _connectionStateController.close();
  }
}

/// BLE 异常类
class BLEException implements Exception {
  final String message;
  BLEException(this.message);
  
  @override
  String toString() => 'BLEException: $message';
}