import { api } from './client';

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
  status: 'active' | 'inactive' | 'expired' | 'revoked';
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
  getMyKeys: (params?: { status?: string; protocol?: string; page?: number; limit?: number }) =>
    api.get('/keys', { params }).then(r => r.data),

  // 获取分享给我的钥匙
  getSharedKeys: () =>
    api.get('/keys/shared').then(r => r.data),

  // 获取钥匙详情
  getKeyDetail: (keyId: string) =>
    api.get(`/keys/${keyId}`).then(r => r.data),

  // 激活钥匙
  activateKey: (keyId: string) =>
    api.post(`/keys/${keyId}/activate`).then(r => r.data),

  // 停用钥匙
  deactivateKey: (keyId: string) =>
    api.post(`/keys/${keyId}/deactivate`).then(r => r.data),

  // 撤销钥匙
  revokeKey: (keyId: string) =>
    api.delete(`/keys/${keyId}`).then(r => r.data),

  // 批量撤销
  revokeKeys: (keyIds: string[]) =>
    api.post('/keys/batch/revoke', { key_ids: keyIds }).then(r => r.data),

  // 分享钥匙
  shareKey: (data: ShareKeyRequest) =>
    api.post('/keys/share', data).then(r => r.data as ShareKeyResponse),

  // 获取分享记录
  getShareHistory: (keyId: string) =>
    api.get(`/keys/${keyId}/shares`).then(r => r.data),

  // 撤销分享
  revokeShare: (shareId: string) =>
    api.delete(`/keys/shares/${shareId}`).then(r => r.data),

  // 更新权限
  updatePermissions: (keyId: string, permissions: KeyPermission[]) =>
    api.put(`/keys/${keyId}/permissions`, { permissions }).then(r => r.data),

  // 获取使用记录
  getUsageLogs: (keyId: string, params?: { start_date?: string; end_date?: string; page?: number; limit?: number }) =>
    api.get(`/keys/${keyId}/logs`, { params }).then(r => r.data),

  // 获取所有使用记录
  getAllUsageLogs: (params?: { start_date?: string; end_date?: string; operation?: string; page?: number; limit?: number }) =>
    api.get('/keys/logs/all', { params }).then(r => r.data),

  // 生成二维码
  generateQRCode: (keyId: string, type: 'share' | 'activate' | 'temp') =>
    api.post(`/keys/${keyId}/qrcode`, { type }).then(r => r.data),

  // 扫码激活
  scanQRCode: (qrData: string) =>
    api.post('/keys/scan', { qr_data: qrData }).then(r => r.data),

  // 延长有效期
  extendExpiry: (keyId: string, days: number) =>
    api.post(`/keys/${keyId}/extend`, { days }).then(r => r.data),
};

export default keysApi;
