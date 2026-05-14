/**
 * 数据转换层 - 统一前后端数据格式
 * 用于处理后端API返回的数据与前端模型之间的转换
 */

import { DigitalKey, KeyPermission, KeyUsageLog } from './keys';

// 后端原始数据格式
interface BackendKey {
  id: number;
  user_id: number;
  vehicle_id: number;
  type: 'CCC' | 'ICCE' | 'ICCOA';
  status: 'active' | 'inactive' | 'revoked' | 'expired' | 'pending';
  permissions: BackendPermissions;
  key_identifier: string;
  name?: string;
  description?: string;
  expires_at?: string;
  last_used_at?: string;
  usage_count: number;
  created_at: string;
  updated_at: string;
  vehicle?: BackendVehicle;
  is_shared?: boolean;
  shared_by?: number;
}

interface BackendPermissions {
  unlock: boolean;
  lock: boolean;
  start_engine: boolean;
  trunk: boolean;
  windows: boolean;
}

interface BackendVehicle {
  id: number;
  vin: string;
  brand: string;
  model: string;
  year?: number;
  color?: string;
  plate_number?: string;
}

interface BackendKeyLog {
  id: number;
  key_id: number;
  user_id: number;
  operation: string;
  status: 'success' | 'failed';
  device_info?: Record<string, any>;
  location?: { lat: number; lng: number };
  error_message?: string;
  created_at: string;
}

/**
 * 将后端权限格式转换为前端数组格式
 */
export function transformPermissions(backend: BackendPermissions): KeyPermission[] {
  const permissions: KeyPermission[] = [
    { type: 'unlock', enabled: backend.unlock },
    { type: 'lock', enabled: backend.lock },
    { type: 'start_engine', enabled: backend.start_engine },
    { type: 'open_trunk', enabled: backend.trunk },
    { type: 'close_windows', enabled: backend.windows },
  ];
  return permissions;
}

/**
 * 将前端权限数组转换为后端对象格式
 */
export function transformPermissionsToBackend(permissions: KeyPermission[]): BackendPermissions {
  const backend: BackendPermissions = {
    unlock: false,
    lock: false,
    start_engine: false,
    trunk: false,
    windows: false,
  };

  permissions.forEach(p => {
    switch (p.type) {
      case 'unlock':
        backend.unlock = p.enabled;
        break;
      case 'lock':
        backend.lock = p.enabled;
        break;
      case 'start_engine':
      case 'stop_engine':
        backend.start_engine = p.enabled;
        break;
      case 'open_trunk':
        backend.trunk = p.enabled;
        break;
      case 'open_windows':
      case 'close_windows':
        backend.windows = p.enabled;
        break;
    }
  });

  return backend;
}

/**
 * 将后端Vehicle转换为前端Vehicle
 */
export function transformVehicle(backend: BackendVehicle) {
  return {
    id: String(backend.id),
    vin: backend.vin,
    brand: backend.brand,
    model: backend.model,
    year: backend.year || new Date().getFullYear(),
    color: backend.color || '',
    plate_number: backend.plate_number,
  };
}

/**
 * 将后端Key转换为前端DigitalKey
 */
export function transformKey(backend: BackendKey): DigitalKey {
  // 判断钥匙类型
  const keyType: 'owner' | 'shared' = backend.is_shared ? 'shared' : 'owner';

  return {
    id: String(backend.id),
    vehicle: backend.vehicle 
      ? transformVehicle(backend.vehicle)
      : { id: String(backend.vehicle_id), vin: '', brand: '', model: '', year: 2024, color: '' },
    protocol: backend.type, // 后端type对应前端protocol
    type: keyType,
    status: backend.status,
    permissions: transformPermissions(backend.permissions),
    issued_at: backend.created_at,
    expires_at: backend.expires_at,
    shared_by: backend.shared_by ? String(backend.shared_by) : undefined,
    shared_to: undefined, // 需要从其他字段推断
    share_count: 0, // 后端需要提供这个字段
    last_used_at: backend.last_used_at,
    use_count: backend.usage_count,
  };
}

/**
 * 将后端日志转换为前端日志
 */
export function transformKeyLog(backend: BackendKeyLog): KeyUsageLog {
  return {
    id: String(backend.id),
    key_id: String(backend.key_id),
    operation: backend.operation,
    status: backend.status === 'success' ? 'success' : 'failure',
    timestamp: backend.created_at,
    location: backend.location,
    device_info: JSON.stringify(backend.device_info || {}),
    failure_reason: backend.error_message,
  };
}

/**
 * 批量转换Key列表
 */
export function transformKeyList(backends: BackendKey[]): DigitalKey[] {
  return backends.map(transformKey);
}

/**
 * 批量转换日志列表
 */
export function transformKeyLogList(backends: BackendKeyLog[]): KeyUsageLog[] {
  return backends.map(transformKeyLog);
}

/**
 * 通用响应转换 - 处理后端的统一响应格式
 */
export interface BackendResponse<T> {
  code: number;
  message: string;
  data: T;
}

/**
 * 解析后端响应
 */
export function parseResponse<T>(response: BackendResponse<T>): {
  success: boolean;
  data?: T;
  message: string;
  code: number;
} {
  return {
    success: response.code === 200 || response.code === 201,
    data: response.data,
    message: response.message,
    code: response.code,
  };
}

export default {
  transformKey,
  transformKeyList,
  transformPermissions,
  transformPermissionsToBackend,
  transformKeyLog,
  transformKeyLogList,
  parseResponse,
};
