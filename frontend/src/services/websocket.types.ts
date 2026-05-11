/**
 * WebSocket Types for yuleDKCS Frontend
 * 
 * Type definitions for WebSocket messages, payloads, and configuration.
 * Compatible with Backend WebSocket protocol.
 */

// =============================================================================
// Message Types
// =============================================================================

export type WSMessageType =
  | 'ping'
  | 'pong'
  | 'auth'
  | 'auth_success'
  | 'auth_error'
  | 'vehicle_status'
  | 'vehicle_status_update'
  | 'command'
  | 'command_result'
  | 'notification'
  | 'error'
  | 'subscribe'
  | 'unsubscribe'

// =============================================================================
// Base Message Interface
// =============================================================================

export interface WSMessage {
  version: string
  type: WSMessageType
  timestamp: string
  id: string
  payload: Record<string, unknown>
}

// =============================================================================
// Ping/Pong Messages (Heartbeat)
// =============================================================================

export interface WSPingMessage extends WSMessage {
  type: 'ping'
  payload: Record<string, never> | Record<string, unknown>
}

export interface WSPongMessage extends WSMessage {
  type: 'pong'
  payload: {
    ping_id: string
  }
}

// =============================================================================
// Authentication Messages
// =============================================================================

export interface WSAuthMessage extends WSMessage {
  type: 'auth'
  payload: {
    token: string
    type: 'jwt'
  }
}

export interface WSAuthSuccessMessage extends WSMessage {
  type: 'auth_success'
  payload: {
    user_id: number
    session_id: string
    expires_at: string
  }
}

export interface WSAuthErrorMessage extends WSMessage {
  type: 'auth_error'
  payload: {
    code: number
    message: string
  }
}

// =============================================================================
// Vehicle Status Messages
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

export interface WSVehicleStatusMessage extends WSMessage {
  type: 'vehicle_status'
  payload: VehicleStatus
}

export interface WSVehicleStatusUpdateMessage extends WSMessage {
  type: 'vehicle_status_update'
  payload: Partial<VehicleStatus> & {
    vehicle_id: number
    last_updated: string
  }
}

// =============================================================================
// Command Messages
// =============================================================================

export interface WSCommandMessage extends WSMessage {
  type: 'command'
  payload: {
    command_id: string
    vehicle_id: number
    command: string
    params: Record<string, unknown>
  }
}

export interface CommandResult {
  command_id: string
  status: 'success' | 'failed' | 'pending' | 'timeout'
  result?: Record<string, unknown>
  error?: string
  executed_at?: string
}

export interface WSCommandResultMessage extends WSMessage {
  type: 'command_result'
  payload: CommandResult
}

// =============================================================================
// Notification Messages
// =============================================================================

export type NotificationLevel = 'info' | 'warning' | 'error' | 'success'

export interface NotificationPayload {
  level: NotificationLevel
  title: string
  message: string
  data?: Record<string, unknown>
  created_at?: string
}

export interface WSNotificationMessage extends WSMessage {
  type: 'notification'
  payload: NotificationPayload
}

// =============================================================================
// Error Messages
// =============================================================================

export interface WSErrorPayload {
  code: number
  message: string
  details?: string
  timestamp?: string
}

export interface WSErrorMessage extends WSMessage {
  type: 'error'
  payload: WSErrorPayload
}

// =============================================================================
// Subscribe/Unsubscribe Messages
// =============================================================================

export interface WSSubscribeMessage extends WSMessage {
  type: 'subscribe'
  payload: {
    topic: string
    vehicle_id?: number
    key_id?: string
  }
}

export interface WSUnsubscribeMessage extends WSMessage {
  type: 'unsubscribe'
  payload: {
    topic: string
    vehicle_id?: number
    key_id?: string
  }
}

// =============================================================================
// Configuration Types
// =============================================================================

export interface WebSocketConfig {
  /** WebSocket base URL (ws:// or wss://) */
  baseURL?: string
  /** Initial reconnection interval in milliseconds */
  reconnectInterval?: number
  /** Maximum reconnection interval in milliseconds */
  maxReconnectInterval?: number
  /** Maximum number of reconnection attempts */
  maxReconnectAttempts?: number
  /** Heartbeat interval in milliseconds */
  heartbeatInterval?: number
  /** Heartbeat timeout in milliseconds */
  heartbeatTimeout?: number
  /** Connection timeout in milliseconds */
  connectionTimeout?: number
}

// =============================================================================
// Connection State
// =============================================================================

export enum WebSocketState {
  CONNECTING = 'CONNECTING',
  OPEN = 'OPEN',
  CLOSING = 'CLOSING',
  CLOSED = 'CLOSED',
  RECONNECTING = 'RECONNECTING',
  ERROR = 'ERROR',
}

// =============================================================================
// Event Types
// =============================================================================

export type MessageHandler = (payload: unknown, type?: WSMessageType) => void

export interface WebSocketEventMap {
  connect: void
  disconnect: { code: number; reason: string }
  error: Event | Error
  reconnecting: { attempt: number; delay: number }
  max_reconnect_exceeded: { attempts: number }
  auth_success: WSAuthSuccessMessage['payload']
  auth_error: WSAuthErrorMessage['payload']
  parse_error: { error: Error; data: string }
  ping: WSPingMessage['payload']
  pong: WSPongMessage['payload']
  vehicle_status: VehicleStatus
  vehicle_status_update: Partial<VehicleStatus>
  command: WSCommandMessage['payload']
  command_result: CommandResult
  notification: NotificationPayload
  error_message: WSErrorPayload
}

// =============================================================================
// Hook Types
// =============================================================================

export interface UseWebSocketReturn {
  /** Whether the WebSocket is currently connected */
  isConnected: boolean
  /** Whether the WebSocket is currently connecting */
  isConnecting: boolean
  /** Last error that occurred */
  error: Error | null
  /** Connect to WebSocket server */
  connect: () => Promise<void>
  /** Disconnect from WebSocket server */
  disconnect: () => void
  /** Send a command to the vehicle */
  sendCommand: (command: string, params?: Record<string, unknown>) => boolean
  /** WebSocket service instance */
  service: WebSocketService
}

// Forward declaration for service type
export interface WebSocketService {
  connect: (vehicleId?: string) => Promise<void>
  disconnect: () => void
  isConnected: () => boolean
  getState: () => WebSocketState
  send: (type: WSMessageType, payload: Record<string, unknown>) => boolean
  sendCommand: (vehicleId: number, command: string, params?: Record<string, unknown>) => boolean
  subscribeVehicle: (vehicleId: number) => boolean
  unsubscribeVehicle: (vehicleId: number) => boolean
  on: (type: WSMessageType | '*', handler: MessageHandler) => () => void
  off: (type: WSMessageType | '*', handler: MessageHandler) => void
  onVehicleStatus: (handler: (status: VehicleStatus) => void) => () => void
  onCommandResult: (handler: (result: CommandResult) => void) => () => void
  onNotification: (handler: (notification: NotificationPayload) => void) => () => void
  onError: (handler: (error: WSErrorPayload) => void) => () => void
}

// =============================================================================
// Vehicle Command Types
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

export interface VehicleCommandParams {
  unlock?: { delay?: number }
  lock?: { delay?: number }
  engine_start?: { duration?: number }
  engine_stop?: {}
  trunk_open?: {}
  trunk_close?: {}
  climate_on?: { temperature?: number; duration?: number }
  climate_off?: {}
  horn?: { duration?: number }
  lights?: { duration?: number; flash?: boolean }
  locate?: {}
}
