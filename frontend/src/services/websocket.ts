/**
 * WebSocket Service - 实时通信客户端
 * 
 * 功能:
 * - 与 Backend WebSocket 服务器建立持久连接
 * - 支持自动重连和心跳机制
 * - JWT 认证
 * - 订阅车辆状态和命令结果
 */

import { API_BASE_URL, getAuthToken } from '../api/client';

// WebSocket 消息类型
export enum WsMessageType {
  VEHICLE_STATUS = 'vehicle_status',
  COMMAND_RESULT = 'command_result',
  ERROR = 'error',
  PING = 'ping',
  PONG = 'pong',
  AUTH = 'auth',
  SUBSCRIBE = 'subscribe',
  UNSUBSCRIBE = 'unsubscribe'
}

// WebSocket 连接状态
export enum WsConnectionState {
  CONNECTING = 'connecting',
  CONNECTED = 'connected',
  RECONNECTING = 'reconnecting',
  DISCONNECTED = 'disconnected',
  ERROR = 'error'
}

// 消息接口
export interface WsMessage {
  type: WsMessageType;
  payload?: any;
  timestamp?: number;
  id?: string;
}

// 车辆状态消息
export interface VehicleStatusMessage {
  vehicle_id: string;
  status: {
    engine_on: boolean;
    locked: boolean;
    doors: {
      driver: boolean;
      passenger: boolean;
      rear_left: boolean;
      rear_right: boolean;
      trunk: boolean;
    };
    windows: {
      driver: number;
      passenger: number;
      rear_left: number;
      rear_right: number;
    };
    climate: {
      temperature: number;
      ac_on: boolean;
    };
    battery_level?: number;
    fuel_level?: number;
    odometer: number;
  };
  location?: {
    lat: number;
    lon: number;
    accuracy: number;
    timestamp: string;
  };
  timestamp: string;
}

// 命令结果消息
export interface CommandResultMessage {
  command_id: string;
  vehicle_id: string;
  command: string;
  status: 'success' | 'failed' | 'pending';
  result?: any;
  error?: string;
  timestamp: string;
}

// 连接配置
interface WebSocketConfig {
  url: string;
  reconnectInterval: number;
  maxReconnectAttempts: number;
  heartbeatInterval: number;
  heartbeatTimeout: number;
}

// 默认配置
const DEFAULT_CONFIG: WebSocketConfig = {
  url: `${API_BASE_URL.replace(/^http/, 'ws')}/ws`,
  reconnectInterval: 3000,      // 3秒后重连
  maxReconnectAttempts: 5,      // 最多重连5次
  heartbeatInterval: 30000,     // 30秒心跳
  heartbeatTimeout: 10000       // 10秒超时
};

// 事件回调类型
type MessageHandler = (message: WsMessage) => void;
type StateChangeHandler = (state: WsConnectionState) => void;

class WebSocketService {
  private ws: WebSocket | null = null;
  private config: WebSocketConfig;
  private connectionState: WsConnectionState = WsConnectionState.DISCONNECTED;
  private reconnectAttempts = 0;
  private heartbeatTimer: NodeJS.Timeout | null = null;
  private heartbeatTimeoutTimer: NodeJS.Timeout | null = null;
  private reconnectTimer: NodeJS.Timeout | null = null;
  private messageHandlers: Map<WsMessageType, Set<MessageHandler>> = new Map();
  private stateChangeHandlers: Set<StateChangeHandler> = new Set();
  private pendingMessages: WsMessage[] = [];
  private subscribedTopics: Set<string> = new Set();

  constructor(config: Partial<WebSocketConfig> = {}) {
    this.config = { ...DEFAULT_CONFIG, ...config };
    
    // 初始化消息处理器映射
    Object.values(WsMessageType).forEach(type => {
      this.messageHandlers.set(type, new Set());
    });
  }

  /**
   * 连接到 WebSocket 服务器
   */
  async connect(): Promise<void> {
    if (this.ws?.readyState === WebSocket.OPEN) {
      console.log('[WebSocket] 已连接');
      return;
    }

    this.setConnectionState(WsConnectionState.CONNECTING);

    try {
      const token = await getAuthToken();
      if (!token) {
        throw new Error('未登录，无法建立 WebSocket 连接');
      }

      // 构建带认证的 URL
      const url = new URL(this.config.url);
      url.searchParams.set('token', token);

      this.ws = new WebSocket(url.toString());
      
      this.ws.onopen = this.handleOpen.bind(this);
      this.ws.onmessage = this.handleMessage.bind(this);
      this.ws.onclose = this.handleClose.bind(this);
      this.ws.onerror = this.handleError.bind(this);

    } catch (error) {
      console.error('[WebSocket] 连接失败:', error);
      this.setConnectionState(WsConnectionState.ERROR);
      this.scheduleReconnect();
    }
  }

  /**
   * 断开连接
   */
  disconnect(): void {
    console.log('[WebSocket] 主动断开连接');
    
    // 清除所有定时器
    this.clearTimers();
    
    // 清空重连计数
    this.reconnectAttempts = 0;
    
    if (this.reconnectTimer) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = null;
    }

    if (this.ws) {
      // 移除事件监听器避免回调
      this.ws.onopen = null;
      this.ws.onmessage = null;
      this.ws.onclose = null;
      this.ws.onerror = null;
      
      if (this.ws.readyState === WebSocket.OPEN || 
          this.ws.readyState === WebSocket.CONNECTING) {
        this.ws.close(1000, '用户主动断开');
      }
      
      this.ws = null;
    }

    this.setConnectionState(WsConnectionState.DISCONNECTED);
  }

  /**
   * 发送消息
   */
  send(message: WsMessage): boolean {
    if (this.ws?.readyState !== WebSocket.OPEN) {
      // 缓存消息等待重连
      this.pendingMessages.push(message);
      console.warn('[WebSocket] 连接未建立，消息已缓存');
      return false;
    }

    try {
      this.ws.send(JSON.stringify(message));
      return true;
    } catch (error) {
      console.error('[WebSocket] 发送消息失败:', error);
      this.pendingMessages.push(message);
      return false;
    }
  }

  /**
   * 订阅车辆状态
   */
  subscribeVehicle(vehicleId: string): void {
    const topic = `vehicle:${vehicleId}`;
    
    if (this.subscribedTopics.has(topic)) {
      return;
    }

    this.send({
      type: WsMessageType.SUBSCRIBE,
      payload: { topic: 'vehicle_status', vehicle_id: vehicleId }
    });

    this.subscribedTopics.add(topic);
    console.log(`[WebSocket] 订阅车辆: ${vehicleId}`);
  }

  /**
   * 取消订阅车辆状态
   */
  unsubscribeVehicle(vehicleId: string): void {
    const topic = `vehicle:${vehicleId}`;
    
    if (!this.subscribedTopics.has(topic)) {
      return;
    }

    this.send({
      type: WsMessageType.UNSUBSCRIBE,
      payload: { topic: 'vehicle_status', vehicle_id: vehicleId }
    });

    this.subscribedTopics.delete(topic);
    console.log(`[WebSocket] 取消订阅车辆: ${vehicleId}`);
  }

  /**
   * 注册消息处理器
   */
  onMessage(type: WsMessageType, handler: MessageHandler): () => void {
    const handlers = this.messageHandlers.get(type);
    if (handlers) {
      handlers.add(handler);
    }

    // 返回注销函数
    return () => {
      handlers?.delete(handler);
    };
  }

  /**
   * 注册状态变化处理器
   */
  onStateChange(handler: StateChangeHandler): () => void {
    this.stateChangeHandlers.add(handler);
    
    // 立即通知当前状态
    handler(this.connectionState);

    return () => {
      this.stateChangeHandlers.delete(handler);
    };
  }

  /**
   * 获取当前连接状态
   */
  getConnectionState(): WsConnectionState {
    return this.connectionState;
  }

  /**
   * 检查是否已连接
   */
  isConnected(): boolean {
    return this.connectionState === WsConnectionState.CONNECTED;
  }

  // ==================== 私有方法 ====================

  private handleOpen(): void {
    console.log('[WebSocket] 连接已建立');
    this.reconnectAttempts = 0;
    this.setConnectionState(WsConnectionState.CONNECTED);
    
    // 发送认证消息
    this.send({
      type: WsMessageType.AUTH,
      payload: { client_type: 'web' }
    });

    // 启动心跳
    this.startHeartbeat();

    // 重新订阅之前的主题
    this.resubscribeTopics();

    // 发送缓存的消息
    this.flushPendingMessages();
  }

  private handleMessage(event: MessageEvent): void {
    try {
      const message: WsMessage = JSON.parse(event.data);
      
      // 处理心跳响应
      if (message.type === WsMessageType.PONG) {
        this.handlePong();
        return;
      }

      // 通知所有注册的处理器
      const handlers = this.messageHandlers.get(message.type);
      handlers?.forEach(handler => {
        try {
          handler(message);
        } catch (error) {
          console.error('[WebSocket] 消息处理器错误:', error);
        }
      });

    } catch (error) {
      console.error('[WebSocket] 解析消息失败:', error);
    }
  }

  private handleClose(event: CloseEvent): void {
    console.log(`[WebSocket] 连接关闭: ${event.code} - ${event.reason}`);
    
    this.clearTimers();
    
    // 正常关闭不重连
    if (event.code === 1000) {
      this.setConnectionState(WsConnectionState.DISCONNECTED);
      return;
    }

    this.setConnectionState(WsConnectionState.DISCONNECTED);
    this.scheduleReconnect();
  }

  private handleError(error: Event): void {
    console.error('[WebSocket] 连接错误:', error);
    this.setConnectionState(WsConnectionState.ERROR);
  }

  private scheduleReconnect(): void {
    if (this.reconnectAttempts >= this.config.maxReconnectAttempts) {
      console.error('[WebSocket] 重连次数超限，放弃重连');
      this.setConnectionState(WsConnectionState.ERROR);
      return;
    }

    this.reconnectAttempts++;
    this.setConnectionState(WsConnectionState.RECONNECTING);

    const delay = this.config.reconnectInterval * Math.pow(2, this.reconnectAttempts - 1);
    const jitter = Math.random() * 1000;
    const actualDelay = Math.min(delay + jitter, 30000); // 最大30秒

    console.log(`[WebSocket] ${actualDelay/1000}秒后重连 (第${this.reconnectAttempts}次)`);

    this.reconnectTimer = setTimeout(() => {
      this.connect();
    }, actualDelay);
  }

  private startHeartbeat(): void {
    this.heartbeatTimer = setInterval(() => {
      if (this.ws?.readyState === WebSocket.OPEN) {
        this.send({ type: WsMessageType.PING, timestamp: Date.now() });
        
        // 设置超时检测
        this.heartbeatTimeoutTimer = setTimeout(() => {
          console.warn('[WebSocket] 心跳超时，重新连接');
          this.ws?.close();
        }, this.config.heartbeatTimeout);
      }
    }, this.config.heartbeatInterval);
  }

  private handlePong(): void {
    if (this.heartbeatTimeoutTimer) {
      clearTimeout(this.heartbeatTimeoutTimer);
      this.heartbeatTimeoutTimer = null;
    }
  }

  private clearTimers(): void {
    if (this.heartbeatTimer) {
      clearInterval(this.heartbeatTimer);
      this.heartbeatTimer = null;
    }
    if (this.heartbeatTimeoutTimer) {
      clearTimeout(this.heartbeatTimeoutTimer);
      this.heartbeatTimeoutTimer = null;
    }
  }

  private resubscribeTopics(): void {
    this.subscribedTopics.forEach(topic => {
      const vehicleId = topic.replace('vehicle:', '');
      this.send({
        type: WsMessageType.SUBSCRIBE,
        payload: { topic: 'vehicle_status', vehicle_id: vehicleId }
      });
    });
  }

  private flushPendingMessages(): void {
    while (this.pendingMessages.length > 0) {
      const message = this.pendingMessages.shift();
      if (message) {
        this.send(message);
      }
    }
  }

  private setConnectionState(state: WsConnectionState): void {
    if (this.connectionState !== state) {
      this.connectionState = state;
      this.stateChangeHandlers.forEach(handler => {
        try {
          handler(state);
        } catch (error) {
          console.error('[WebSocket] 状态变化处理器错误:', error);
        }
      });
    }
  }
}

// 单例实例
let wsInstance: WebSocketService | null = null;

export function getWebSocketService(): WebSocketService {
  if (!wsInstance) {
    wsInstance = new WebSocketService();
  }
  return wsInstance;
}

export function resetWebSocketService(): void {
  if (wsInstance) {
    wsInstance.disconnect();
    wsInstance = null;
  }
}

// React Hook 封装
import { useEffect, useState, useCallback } from 'react';

export function useWebSocket() {
  const [state, setState] = useState<WsConnectionState>(WsConnectionState.DISCONNECTED);
  const [lastMessage, setLastMessage] = useState<WsMessage | null>(null);
  const ws = getWebSocketService();

  useEffect(() => {
    const unsubscribe = ws.onStateChange(setState);
    return unsubscribe;
  }, [ws]);

  const connect = useCallback(() => {
    ws.connect();
  }, [ws]);

  const disconnect = useCallback(() => {
    ws.disconnect();
  }, [ws]);

  const subscribe = useCallback((vehicleId: string) => {
    ws.subscribeVehicle(vehicleId);
  }, [ws]);

  const unsubscribe = useCallback((vehicleId: string) => {
    ws.unsubscribeVehicle(vehicleId);
  }, [ws]);

  const onMessage = useCallback((type: WsMessageType, handler: MessageHandler) => {
    return ws.onMessage(type, handler);
  }, [ws]);

  return {
    state,
    lastMessage,
    connect,
    disconnect,
    subscribe,
    unsubscribe,
    onMessage,
    isConnected: state === WsConnectionState.CONNECTED
  };
}

export default WebSocketService;
