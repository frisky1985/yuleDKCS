import apiClient from './client';
import { transformKeyList, transformKey, transformKeyLogList, parseResponse } from './transform';
import type { BackendKey, BackendKeyLog } from './transform';

export interface Vehicle {
  id: string;
  vin: string;
  brand: string;
  model: string;
  year: number;
  color: string;
  plate_number?: string;
}

export interface DigitalKey {
  id: string;
  vehicle: Vehicle;
  protocol: 'CCC' | 'ICCOA' | 'ICCE';
  type: 'owner' | 'shared';
  status: 'active' | 'inactive' | 'expired' | 'revoked' | 'pending';
  permissions: KeyPermission[];
  issued_at: string;
  expires_at?: string;
  shared_by?: string;
  shared_to?: string;
  share_count: number;
  last_used_at?: string;
  use_count: number;
}

export interface KeyPermission {
  type: 'unlock' | 'lock' | 'start_engine' | 'stop_engine' | 'open_trunk' | 'open_windows' | 'close_windows' | 'find_vehicle';
  enabled: boolean;
  constraints?: {
    time_range?: { start: string; end: string };
    days_of_week?: number[];
    max_uses?: number;
    geofence?: GeoFence;
  };
}

export interface GeoFence {
  type: 'circle' | 'polygon';
  center?: { lat: number; lng: number };
  radius?: number;
  points?: { lat: number; lng: number }[];
}

export interface KeyUsageLog {
  id: string;
  key_id: string;
  operation: string;
  status: 'success' | 'failure' | 'timeout';
  timestamp: string;
  location?: { lat: number; lng: number };
  device_info: string;
  failure_reason?: string;
}

export interface ShareKeyRequest {
  key_id: string;
  recipient_email?: string;
  recipient_phone?: string;
  expires_in_days: number;
  permissions: KeyPermission[];
  message?: string;
}

export interface ShareKeyResponse {
  share_id: string;
  qr_code_url: string;
  share_link: string;
  expires_at: string;
}

export const keysApi = {
  // 获取钥匙列表
  getMyKeys: async (params?: { status?: string; protocol?: string; page?: number; limit?: number }) => {
    const response = await apiClient.get('/keys', { params });
    const parsed = parseResponse<{list: BackendKey[]}>(response.data);
    if (parsed.success && parsed.data?.list) {
      return {
        ...parsed.data,
        list: transformKeyList(parsed.data.list),
      };
    }
    return parsed.data;
  },

  // 获取分享给我的钥匙
  getSharedKeys: async () => {
    const response = await apiClient.get('/keys/shared');
    const parsed = parseResponse<{list: BackendKey[]}>(response.data);
    if (parsed.success && parsed.data?.list) {
      return {
        ...parsed.data,
        list: transformKeyList(parsed.data.list),
      };
    }
    return parsed.data;
  },

  // 根据车辆ID获取钥匙列表
  getKeysByVehicle: async (vehicleId: string) => {
    const response = await apiClient.get('/keys', { params: { vehicle_id: vehicleId } });
    const parsed = parseResponse<{list: BackendKey[]}>(response.data);
    if (parsed.success && parsed.data?.list) {
      return {
        ...parsed.data,
        list: transformKeyList(parsed.data.list),
      };
    }
    return parsed.data;
  },

  // 获取钥匙详情
  getKeyDetail: async (keyId: string) => {
    const response = await apiClient.get(`/keys/${keyId}`);
    const parsed = parseResponse<BackendKey>(response.data);
    if (parsed.success && parsed.data) {
      return transformKey(parsed.data);
    }
    return null;
  },

  // 分享钥匙
  shareKey: async (data: { friend_id: string; vehicle_id: string; permissions: string[] }) => {
    const response = await apiClient.post('/keys/share', data);
    const parsed = parseResponse<BackendKey>(response.data);
    if (parsed.success && parsed.data) {
      return transformKey(parsed.data);
    }
    return null;
  },

  // 撤销钥匙
  revokeKey: async (keyId: string) => {
    const response = await apiClient.delete(`/keys/${keyId}`);
    const parsed = parseResponse<void>(response.data);
    return parsed.success;
  },

  // 激活钥匙
  activateKey: async (keyId: string) => {
    const response = await apiClient.post(`/keys/${keyId}/activate`);
    const parsed = parseResponse<BackendKey>(response.data);
    return parsed.success;
  },

  // 停用钥匙
  deactivateKey: async (keyId: string) => {
    const response = await apiClient.post(`/keys/${keyId}/deactivate`);
    const parsed = parseResponse<BackendKey>(response.data);
    return parsed.success;
  },

  // 获取钥匙使用记录
  getKeyLogs: async (keyId: string, params?: { page?: number; limit?: number }) => {
    const response = await apiClient.get(`/keys/${keyId}/logs`, { params });
    const parsed = parseResponse<{list: BackendKeyLog[]}>(response.data);
    if (parsed.success && parsed.data?.list) {
      return {
        ...parsed.data,
        list: transformKeyLogList(parsed.data.list),
      };
    }
    return parsed.data;
  },

  // 获取所有使用记录（管理员）
  getAllLogs: async (params?: { page?: number; limit?: number }) => {
    const response = await apiClient.get('/keys/logs/all', { params });
    const parsed = parseResponse<{list: BackendKeyLog[]}>(response.data);
    if (parsed.success && parsed.data?.list) {
      return {
        ...parsed.data,
        list: transformKeyLogList(parsed.data.list),
      };
    }
    return parsed.data;
  },

  // 获取使用记录
  getUsageLogs: async (keyId: string, params?: { start_date?: string; end_date?: string; page?: number; limit?: number }) => {
    const response = await apiClient.get(`/keys/${keyId}/logs`, { params });
    const parsed = parseResponse<{list: BackendKeyLog[]}>(response.data);
    if (parsed.success && parsed.data?.list) {
      return {
        ...parsed.data,
        list: transformKeyLogList(parsed.data.list),
      };
    }
    return parsed.data;
  },

  // 获取所有使用记录
  getAllUsageLogs: async (params?: { start_date?: string; end_date?: string; operation?: string; page?: number; limit?: number }) => {
    const response = await apiClient.get('/keys/logs/all', { params });
    const parsed = parseResponse<{list: BackendKeyLog[]}>(response.data);
    if (parsed.success && parsed.data?.list) {
      return {
        ...parsed.data,
        list: transformKeyLogList(parsed.data.list),
      };
    }
    return parsed.data;
  },

  // 生成二维码
  generateQRCode: async (keyId: string, type: 'share' | 'activate' | 'temp') => {
    const response = await apiClient.post(`/keys/${keyId}/qrcode`, { type });
    return parseResponse(response.data);
  },

  // 扫码激活
  scanQRCode: async (qrData: string) => {
    const response = await apiClient.post('/keys/scan', { qr_data: qrData });
    return parseResponse(response.data);
  },

  // 延长有效期
  extendExpiry: async (keyId: string, days: number) => {
    const response = await apiClient.post(`/keys/${keyId}/extend`, { days });
    return parseResponse(response.data);
  },
};

export default keysApi;
