import 'dart:ffi';
import 'dart:io';
import 'dart:typed_data';
import 'package:ffi/ffi.dart';
import 'package:path/path.dart' as path;
import 'package:path_provider/path_provider.dart';

/// FFI 桥接类，负责加载和绑定 C 库
class FFIBridge {
  static final FFIBridge _instance = FFIBridge._internal();
  factory FFIBridge() => _instance;
  FFIBridge._internal();

  DynamicLibrary? _lib;
  bool _initialized = false;

  // 函数类型定义
  late final Pointer<Utf8> Function(Pointer<Utf8>) _dkcs_init;
  late final Pointer<Utf8> Function(Pointer<Utf8>, Pointer<Utf8>) _dkcs_issue_key;
  late final Pointer<Utf8> Function() _dkcs_list_keys;
  late final Pointer<Utf8> Function(Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>) _dkcs_share_key;
  late final Pointer<Utf8> Function(Pointer<Utf8>) _dkcs_revoke_key;
  late final Pointer<Utf8> Function(Pointer<Utf8>) _dkcs_connect;
  late final Pointer<Utf8> Function(Pointer<Utf8>) _dkcs_send_command;
  late final void Function(Pointer<Utf8>) _dkcs_free_string;
  late final Pointer<Utf8> Function() _dkcs_get_error;

  /// 初始化 FFI 库
  Future<void> initialize() async {
    if (_initialized) return;

    final libPath = await _getLibraryPath();
    _lib = DynamicLibrary.open(libPath);

    _bindFunctions();
    _initialized = true;
  }

  /// 获取动态库路径
  Future<String> _getLibraryPath() async {
    if (Platform.isAndroid) {
      return 'libyuledkcs.so';
    } else if (Platform.isIOS) {
      return 'yuledkcs.framework/yuledkcs';
    } else if (Platform.isLinux) {
      return 'libyuledkcs.so';
    } else if (Platform.isMacOS) {
      return 'libyuledkcs.dylib';
    } else if (Platform.isWindows) {
      return 'yuledkcs.dll';
    }
    throw UnsupportedError('Unsupported platform: ${Platform.operatingSystem}');
  }

  /// 绑定 C 函数
  void _bindFunctions() {
    if (_lib == null) throw StateError('Library not loaded');

    _dkcs_init = _lib!.lookupFunction<
        Pointer<Utf8> Function(Pointer<Utf8>),
        Pointer<Utf8> Function(Pointer<Utf8>)>('dkcs_init');

    _dkcs_issue_key = _lib!.lookupFunction<
        Pointer<Utf8> Function(Pointer<Utf8>, Pointer<Utf8>),
        Pointer<Utf8> Function(Pointer<Utf8>, Pointer<Utf8>)>('dkcs_issue_key');

    _dkcs_list_keys = _lib!.lookupFunction<
        Pointer<Utf8> Function(),
        Pointer<Utf8> Function()>('dkcs_list_keys');

    _dkcs_share_key = _lib!.lookupFunction<
        Pointer<Utf8> Function(Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>),
        Pointer<Utf8> Function(Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>)>('dkcs_share_key');

    _dkcs_revoke_key = _lib!.lookupFunction<
        Pointer<Utf8> Function(Pointer<Utf8>),
        Pointer<Utf8> Function(Pointer<Utf8>)>('dkcs_revoke_key');

    _dkcs_connect = _lib!.lookupFunction<
        Pointer<Utf8> Function(Pointer<Utf8>),
        Pointer<Utf8> Function(Pointer<Utf8>)>('dkcs_connect');

    _dkcs_send_command = _lib!.lookupFunction<
        Pointer<Utf8> Function(Pointer<Utf8>),
        Pointer<Utf8> Function(Pointer<Utf8>)>('dkcs_send_command');

    _dkcs_free_string = _lib!.lookupFunction<
        Void Function(Pointer<Utf8>),
        void Function(Pointer<Utf8>)>('dkcs_free_string');

    _dkcs_get_error = _lib!.lookupFunction<
        Pointer<Utf8> Function(),
        Pointer<Utf8> Function()>('dkcs_get_error');
  }

  /// 初始化 SDK
  String initializeSDK(String apiKey) {
    final apiKeyPtr = apiKey.toNativeUtf8();
    try {
      final resultPtr = _dkcs_init(apiKeyPtr);
      final result = resultPtr.toDartString();
      _dkcs_free_string(resultPtr);
      return result;
    } finally {
      calloc.free(apiKeyPtr);
    }
  }

  /// 发放钥匙
  String issueKey(String vehicleId, String apiKey) {
    final vehicleIdPtr = vehicleId.toNativeUtf8();
    final apiKeyPtr = apiKey.toNativeUtf8();
    try {
      final resultPtr = _dkcs_issue_key(vehicleIdPtr, apiKeyPtr);
      final result = resultPtr.toDartString();
      _dkcs_free_string(resultPtr);
      return result;
    } finally {
      calloc.free(vehicleIdPtr);
      calloc.free(apiKeyPtr);
    }
  }

  /// 列出钥匙
  String listKeys() {
    final resultPtr = _dkcs_list_keys();
    final result = resultPtr.toDartString();
    _dkcs_free_string(resultPtr);
    return result;
  }

  /// 共享钥匙
  String shareKey(String keyId, String to, String permissions) {
    final keyIdPtr = keyId.toNativeUtf8();
    final toPtr = to.toNativeUtf8();
    final permissionsPtr = permissions.toNativeUtf8();
    try {
      final resultPtr = _dkcs_share_key(keyIdPtr, toPtr, permissionsPtr);
      final result = resultPtr.toDartString();
      _dkcs_free_string(resultPtr);
      return result;
    } finally {
      calloc.free(keyIdPtr);
      calloc.free(toPtr);
      calloc.free(permissionsPtr);
    }
  }

  /// 撤销钥匙
  String revokeKey(String keyId) {
    final keyIdPtr = keyId.toNativeUtf8();
    try {
      final resultPtr = _dkcs_revoke_key(keyIdPtr);
      final result = resultPtr.toDartString();
      _dkcs_free_string(resultPtr);
      return result;
    } finally {
      calloc.free(keyIdPtr);
    }
  }

  /// 连接 BLE
  String connect(String address) {
    final addressPtr = address.toNativeUtf8();
    try {
      final resultPtr = _dkcs_connect(addressPtr);
      final result = resultPtr.toDartString();
      _dkcs_free_string(resultPtr);
      return result;
    } finally {
      calloc.free(addressPtr);
    }
  }

  /// 发送命令
  String sendCommand(String command) {
    final commandPtr = command.toNativeUtf8();
    try {
      final resultPtr = _dkcs_send_command(commandPtr);
      final result = resultPtr.toDartString();
      _dkcs_free_string(resultPtr);
      return result;
    } finally {
      calloc.free(commandPtr);
    }
  }

  /// 获取错误信息
  String? getError() {
    final errorPtr = _dkcs_get_error();
    if (errorPtr == nullptr) return null;
    final error = errorPtr.toDartString();
    _dkcs_free_string(errorPtr);
    return error;
  }
}