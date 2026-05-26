/**
 * Services exports
 */

export { getWebSocketService, resetWebSocketService, useWebSocket, WsMessageType, WsConnectionState } from './websocket'
export type {
  WsMessage, VehicleStatusMessage, CommandResultMessage,
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
