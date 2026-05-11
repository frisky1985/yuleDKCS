// 通用类型定义

export interface PaginationParams {
  page: number
  pageSize: number
}

export interface PaginatedResponse<T> {
  items: T[]
  total: number
  page: number
  pageSize: number
  totalPages: number
}

// =============================================================================
// User Types
// =============================================================================

export interface User {
  id: number
  username: string
  email: string
  phone?: string
  avatar?: string
  createdAt: string
  updatedAt: string
}

export interface AuthState {
  user: User | null
  token: string | null
  isAuthenticated: boolean
}

// =============================================================================
// Vehicle Types (Compatible with WebSocket Protocol)
// =============================================================================

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

export interface Vehicle {
  id: number
  vin: string
  name: string
  brand: string
  model: string
  year: number
  color?: string
  licensePlate?: string
  status?: VehicleStatus
  createdAt: string
  updatedAt: string
}

// =============================================================================
// Digital Key Types
// =============================================================================

export interface KeyPermissions {
  unlock: boolean
  lock: boolean
  engineStart: boolean
  engineStop: boolean
  trunk: boolean
  climate: boolean
  share: boolean
  revoke: boolean
}

export type KeyType = 'owner' | 'shared' | 'guest' | 'valet'
export type KeyStatus = 'active' | 'suspended' | 'revoked' | 'expired'

export interface DigitalKey {
  id: string
  vehicleId: number
  name: string
  keyType: KeyType
  status: KeyStatus
  permissions: KeyPermissions
  validFrom: string
  validUntil: string | null
  sharedBy?: string
  createdAt: string
}

// =============================================================================
// Command Types (Compatible with WebSocket Protocol)
// =============================================================================

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

export interface CommandRequest {
  vehicleId: number
  command: VehicleCommand
  params?: Record<string, unknown>
}

// =============================================================================
// Notification Types
// =============================================================================

export type NotificationLevel = 'info' | 'warning' | 'error' | 'success'

export interface Notification {
  id: string
  level: NotificationLevel
  title: string
  message: string
  data?: Record<string, unknown>
  read: boolean
  createdAt: string
}

// =============================================================================
// Legacy Types (Deprecated - will be removed)
// =============================================================================

/** @deprecated Use DigitalKey instead */
export interface CardProduct {
  id: string
  name: string
  description: string
  price: number
  faceValue: number
  category: string
  stock: number
  status: 'active' | 'inactive'
  createdAt: string
  updatedAt: string
}

/** @deprecated Use CommandRequest instead */
export interface Order {
  id: string
  userId: string
  productId: string
  quantity: number
  totalAmount: number
  status: 'pending' | 'processing' | 'completed' | 'failed' | 'refunded'
  cardNumbers?: string[]
  createdAt: string
  updatedAt: string
}

/** @deprecated Will be removed in future version */
export interface RechargeTask {
  id: string
  orderId: string
  accountNumber: string
  amount: number
  status: 'pending' | 'processing' | 'success' | 'failed'
  retryCount: number
  resultMessage?: string
  createdAt: string
  updatedAt: string
}
