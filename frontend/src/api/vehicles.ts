/**
 * 车辆管理 API 模块
 * 提供车辆 CRUD、状态查询、控制命令等接口
 */
import apiClient from './client'

// =============================================================================
// 类型定义
// =============================================================================

export interface Vehicle {
  id: number
  vin: string
  brand: string
  model: string
  year: number
  color?: string
  plate_number?: string
  status: 'online' | 'offline' | 'sleeping' | 'unknown'
  battery_level: number
  fuel_level?: number
  location?: VehicleLocation
  created_at: string
  updated_at: string
}

export interface VehicleLocation {
  latitude: number
  longitude: number
  altitude?: number
  accuracy?: number
  timestamp?: string
}

export interface VehicleStatus {
  vehicle_id: number
  status: 'online' | 'offline' | 'sleeping' | 'unknown'
  lock_state: 'locked' | 'unlocked' | 'unknown'
  engine_state: 'running' | 'stopped' | 'unknown'
  battery_level: number
  fuel_level?: number
  location?: VehicleLocation
  last_updated: string
}

export type VehicleCommand =
  | 'unlock'
  | 'lock'
  | 'engine_start'
  | 'engine_stop'
  | 'trunk_open'
  | 'trunk_close'
  | 'climate_on'
  | 'climate_off'
  | 'horn'
  | 'lights'
  | 'locate'

export interface CommandResult {
  command_id: string
  status: 'success' | 'failed' | 'pending' | 'timeout'
  result?: Record<string, unknown>
  error?: string
  executed_at?: string
}

export interface CreateVehicleRequest {
  vin: string
  brand: string
  model: string
  year?: number
  color?: string
  plate_number?: string
}

export interface UpdateVehicleRequest {
  brand?: string
  model?: string
  year?: number
  color?: string
  plate_number?: string
}

export interface VehicleListResponse {
  vehicles: Vehicle[]
  total: number
  page: number
  limit: number
}

// =============================================================================
// API 函数
// =============================================================================

export const vehiclesApi = {
  /**
   * 获取车辆列表
   */
  getMyVehicles: async (params?: { page?: number; limit?: number }) => {
    const response = await apiClient.get('/vehicles', { params })
    return response.data
  },

  /**
   * 获取车辆详情
   */
  getVehicleDetail: async (vehicleId: number) => {
    const response = await apiClient.get(`/vehicles/${vehicleId}`)
    return response.data
  },

  /**
   * 获取车辆实时状态
   */
  getVehicleStatus: async (vehicleId: number) => {
    const response = await apiClient.get(`/vehicles/${vehicleId}/status`)
    return response.data
  },

  /**
   * 添加/注册车辆
   */
  createVehicle: async (data: CreateVehicleRequest) => {
    const response = await apiClient.post('/vehicles', data)
    return response.data
  },

  /**
   * 更新车辆信息
   */
  updateVehicle: async (vehicleId: number, data: UpdateVehicleRequest) => {
    const response = await apiClient.put(`/vehicles/${vehicleId}`, data)
    return response.data
  },

  /**
   * 删除车辆
   */
  deleteVehicle: async (vehicleId: number) => {
    const response = await apiClient.delete(`/vehicles/${vehicleId}`)
    return response.data
  },

  /**
   * 发送车辆控制命令
   */
  sendCommand: async (vehicleId: number, command: VehicleCommand, params?: Record<string, unknown>) => {
    const response = await apiClient.post(`/vehicles/${vehicleId}/control`, { command, params })
    return response.data
  },

  /**
   * 获取车辆关联的钥匙列表
   */
  getVehicleKeys: async (vehicleId: number) => {
    const response = await apiClient.get(`/vehicles/${vehicleId}/keys`)
    return response.data
  },
}

export default vehiclesApi
