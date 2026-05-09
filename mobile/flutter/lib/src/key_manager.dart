import 'dart:convert';
import 'ffi_bridge.dart';
import '../yuledkcs.dart';

/// 钥匙管理类
class KeyManager {
  final FFIBridge _ffi = FFIBridge();

  /// 发放新钥匙
  Future<DigitalKey> issueKey(String vehicleId, String apiKey) async {
    final resultJson = _ffi.issueKey(vehicleId, apiKey);
    
    final result = jsonDecode(resultJson) as Map<String, dynamic>;
    
    if (result['success'] != true) {
      throw KeyException(result['error'] ?? 'Failed to issue key');
    }
    
    final data = result['data'] as Map<String, dynamic>;
    return _parseKey(data);
  }

  /// 列出所有钥匙
  List<DigitalKey> listKeys() {
    final resultJson = _ffi.listKeys();
    
    final result = jsonDecode(resultJson) as Map<String, dynamic>;
    
    if (result['success'] != true) {
      throw KeyException(result['error'] ?? 'Failed to list keys');
    }
    
    final keys = result['data'] as List<dynamic>;
    return keys.map((k) => _parseKey(k as Map<String, dynamic>)).toList();
  }

  /// 共享钥匙
  Future<void> shareKey(
    String keyId,
    String to,
    List<Permission> permissions,
  ) async {
    final permissionsStr = permissions
        .map((p) => p.name)
        .join(',');
    
    final resultJson = _ffi.shareKey(keyId, to, permissionsStr);
    
    final result = jsonDecode(resultJson) as Map<String, dynamic>;
    
    if (result['success'] != true) {
      throw KeyException(result['error'] ?? 'Failed to share key');
    }
  }

  /// 撤销钥匙
  Future<void> revokeKey(String keyId) async {
    final resultJson = _ffi.revokeKey(keyId);
    
    final result = jsonDecode(resultJson) as Map<String, dynamic>;
    
    if (result['success'] != true) {
      throw KeyException(result['error'] ?? 'Failed to revoke key');
    }
  }

  /// 解析钥匙数据
  DigitalKey _parseKey(Map<String, dynamic> data) {
    return DigitalKey(
      keyId: data['key_id'] as String,
      vehicleId: data['vehicle_id'] as String,
      ownerId: data['owner_id'] as String,
      permissions: (data['permissions'] as List<dynamic>)
          .map((p) => Permission.values.firstWhere(
                (e) => e.name == p,
                orElse: () => Permission.unlock,
              ))
          .toList(),
      issuedAt: DateTime.parse(data['issued_at'] as String),
      expiresAt: data['expires_at'] != null
          ? DateTime.parse(data['expires_at'] as String)
          : null,
      status: KeyStatus.values.firstWhere(
        (e) => e.name == data['status'],
        orElse: () => KeyStatus.active,
      ),
    );
  }
}

/// 钥匙异常类
class KeyException implements Exception {
  final String message;
  KeyException(this.message);
  
  @override
  String toString() => 'KeyException: $message';
}