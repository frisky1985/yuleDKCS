/**
 * Services exports
 */

export { WebSocketService, wsService, useWebSocket, WS_MESSAGE_TYPES } from './websocket'
export type {
  UseWebSocketOptions,
} from './websocket'

export type {
  WSMessage,
  WSMessageType,
  WSPingMessage,
  WSPongMessage,
  WSAuthMessage,
  WSAuthSuccessMessage,
  WSAuthErrorMessage,
  VehicleLocation,
  VehicleStatus,
  WSVehicleStatusMessage,
  WSVehicleStatusUpdateMessage,
  WSCommandMessage,
  CommandResult,
  WSCommandResultMessage,
  NotificationLevel,
  NotificationPayload,
  WSNotificationMessage,
  WSErrorPayload,
  WSErrorMessage,
  WSSubscribeMessage,
  WSUnsubscribeMessage,
  WebSocketConfig,
  WebSocketState,
  MessageHandler,
  WebSocketEventMap,
  UseWebSocketReturn,
  WebSocketService as WebSocketServiceInterface,
  VehicleCommand,
  VehicleCommandParams,
} from './websocket.types'
