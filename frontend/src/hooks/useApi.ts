import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import type {
  Vehicle,
  DigitalKey,
  KeyShare,
  ActivityLog,
  Notification,
  User,
  VehicleControlCommand,
  PermissionType,
} from '../types';

// 模拟 API 基础 URL
const API_BASE = '/api/v1';

// 模拟 API 调用
const fetchApi = async <T>(endpoint: string, options?: RequestInit): Promise<T> => {
  const response = await fetch(`${API_BASE}${endpoint}`, {
    headers: {
      'Content-Type': 'application/json',
    },
    ...options,
  });
  if (!response.ok) {
    throw new Error(`API error: ${response.status}`);
  }
  return response.json();
};

// ========== User Hooks ==========
export const useCurrentUser = () => {
  return useQuery<User>({
    queryKey: ['user', 'current'],
    queryFn: () => fetchApi('/user/me'),
  });
};

// ========== Vehicle Hooks ==========
export const useVehicles = () => {
  return useQuery<Vehicle[]>({
    queryKey: ['vehicles'],
    queryFn: () => fetchApi('/vehicles'),
  });
};

export const useVehicle = (vehicleId: string) => {
  return useQuery<Vehicle>({
    queryKey: ['vehicles', vehicleId],
    queryFn: () => fetchApi(`/vehicles/${vehicleId}`),
    enabled: !!vehicleId,
  });
};

export const useVehicleStatus = (vehicleId: string) => {
  return useQuery<Vehicle['status']>({
    queryKey: ['vehicles', vehicleId, 'status'],
    queryFn: () => fetchApi(`/vehicles/${vehicleId}/status`),
    enabled: !!vehicleId,
    refetchInterval: 30000, // 每30秒刷新状态
  });
};

export const useVehicleLocation = (vehicleId: string) => {
  return useQuery<Vehicle['location']>({
    queryKey: ['vehicles', vehicleId, 'location'],
    queryFn: () => fetchApi(`/vehicles/${vehicleId}/location`),
    enabled: !!vehicleId,
    refetchInterval: 60000, // 每60秒刷新位置
  });
};

// ========== Key Hooks ==========
export const useMyKeys = () => {
  return useQuery<DigitalKey[]>({
    queryKey: ['keys', 'my'],
    queryFn: () => fetchApi('/keys/my'),
  });
};

export const useSharedKeys = () => {
  return useQuery<DigitalKey[]>({
    queryKey: ['keys', 'shared'],
    queryFn: () => fetchApi('/keys/shared'),
  });
};

export const useKey = (keyId: string) => {
  return useQuery<DigitalKey>({
    queryKey: ['keys', keyId],
    queryFn: () => fetchApi(`/keys/${keyId}`),
    enabled: !!keyId,
  });
};

export const useVehicleKeys = (vehicleId: string) => {
  return useQuery<DigitalKey[]>({
    queryKey: ['vehicles', vehicleId, 'keys'],
    queryFn: () => fetchApi(`/vehicles/${vehicleId}/keys`),
    enabled: !!vehicleId,
  });
};

export const useShareKey = () => {
  const queryClient = useQueryClient();
  
  return useMutation({
    mutationFn: (data: {
      keyId: string;
      recipient: { phone?: string; email?: string };
      permissions: PermissionType[];
      expiresAt?: string;
    }) => fetchApi('/keys/share', {
      method: 'POST',
      body: JSON.stringify(data),
    }),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['keys'] });
      queryClient.invalidateQueries({ queryKey: ['keyShares'] });
    },
  });
};

export const useRevokeKey = () => {
  const queryClient = useQueryClient();
  
  return useMutation({
    mutationFn: (shareId: string) => fetchApi(`/keys/share/${shareId}/revoke`, {
      method: 'POST',
    }),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['keys'] });
      queryClient.invalidateQueries({ queryKey: ['keyShares'] });
    },
  });
};

export const useKeyShares = (keyId?: string) => {
  return useQuery<KeyShare[]>({
    queryKey: ['keyShares', keyId],
    queryFn: () => fetchApi(keyId ? `/keys/${keyId}/shares` : '/keys/shares'),
  });
};

// ========== Control Hooks ==========
export const useVehicleControl = () => {
  const queryClient = useQueryClient();
  
  return useMutation({
    mutationFn: (command: VehicleControlCommand) => 
      fetchApi(`/vehicles/${command.vehicleId}/control`, {
        method: 'POST',
        body: JSON.stringify({
          type: command.type,
          params: command.params,
        }),
      }),
    onSuccess: (_, variables) => {
      queryClient.invalidateQueries({ queryKey: ['vehicles', variables.vehicleId] });
      queryClient.invalidateQueries({ queryKey: ['activities'] });
    },
  });
};

// ========== Activity Hooks ==========
export const useActivities = (limit: number = 20) => {
  return useQuery<ActivityLog[]>({
    queryKey: ['activities', limit],
    queryFn: () => fetchApi(`/activities?limit=${limit}`),
  });
};

export const useVehicleActivities = (vehicleId: string, limit: number = 20) => {
  return useQuery<ActivityLog[]>({
    queryKey: ['activities', 'vehicle', vehicleId, limit],
    queryFn: () => fetchApi(`/vehicles/${vehicleId}/activities?limit=${limit}`),
    enabled: !!vehicleId,
  });
};

// ========== Notification Hooks ==========
export const useNotifications = () => {
  return useQuery<Notification[]>({
    queryKey: ['notifications'],
    queryFn: () => fetchApi('/notifications'),
  });
};

export const useUnreadNotifications = () => {
  return useQuery<number>({
    queryKey: ['notifications', 'unread'],
    queryFn: () => fetchApi('/notifications/unread-count'),
  });
};

export const useMarkNotificationRead = () => {
  const queryClient = useQueryClient();
  
  return useMutation({
    mutationFn: (notificationId: string) => 
      fetchApi(`/notifications/${notificationId}/read`, {
        method: 'POST',
      }),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['notifications'] });
    },
  });
};

export const useMarkAllNotificationsRead = () => {
  const queryClient = useQueryClient();
  
  return useMutation({
    mutationFn: () => fetchApi('/notifications/read-all', {
      method: 'POST',
    }),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['notifications'] });
    },
  });
};
