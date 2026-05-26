package mqttbridge

import (
	"log"
	"sync"

	"github.com/gorilla/websocket"
)

// WebSocketHub 管理所有 WebSocket 连接
type WebSocketHub struct {
	clients    map[*WSClient]bool
	broadcast  chan []byte
	register   chan *WSClient
	unregister chan *WSClient
	stopCh     chan struct{}
	mu         sync.RWMutex
}

// NewWebSocketHub 创建 WebSocket Hub
func NewWebSocketHub() *WebSocketHub {
	return &WebSocketHub{
		clients:    make(map[*WSClient]bool),
		broadcast:  make(chan []byte, 256),
		register:   make(chan *WSClient),
		unregister: make(chan *WSClient),
		stopCh:     make(chan struct{}),
	}
}

// Run 启动 Hub 的事件循环
func (h *WebSocketHub) Run() {
	for {
		select {
		case <-h.stopCh:
			return
		case client := <-h.register:
			h.mu.Lock()
			h.clients[client] = true
			h.mu.Unlock()

		case client := <-h.unregister:
			h.mu.Lock()
			if _, ok := h.clients[client]; ok {
				delete(h.clients, client)
				close(client.send)
			}
			h.mu.Unlock()

		case message := <-h.broadcast:
			h.mu.RLock()
			for client := range h.clients {
				select {
				case client.send <- message:
				default:
					close(client.send)
					delete(h.clients, client)
				}
			}
			h.mu.RUnlock()
		}
	}
}

// Stop 停止 Hub
func (h *WebSocketHub) Stop() {
	close(h.stopCh)
}

// Register 注册客户端
func (h *WebSocketHub) Register(client *WSClient) {
	h.register <- client
}

// Broadcast 广播消息到所有客户端
func (h *WebSocketHub) Broadcast(data []byte) {
	h.broadcast <- data
}

// BroadcastToVehicle 发送消息到指定车辆的客户端
// 简化实现: 广播到所有客户端，生产环境应按 vehicleID 分组
func (h *WebSocketHub) BroadcastToVehicle(vehicleID uint, data []byte) {
	h.mu.RLock()
	defer h.mu.RUnlock()
	for client := range h.clients {
		if client.vehicleID == vehicleID {
			select {
			case client.send <- data:
			default:
			}
		}
	}
}

// BroadcastToUser 发送消息到指定用户的客户端
// 简化实现: 广播到所有客户端，生产环境应按 userID 分组
func (h *WebSocketHub) BroadcastToUser(userID uint, data []byte) {
	h.mu.RLock()
	defer h.mu.RUnlock()
	for client := range h.clients {
		if client.userID == userID {
			select {
			case client.send <- data:
			default:
			}
		}
	}
}

// WSClient WebSocket 客户端
type WSClient struct {
	hub       *WebSocketHub
	conn      *websocket.Conn
	send      chan []byte
	vehicleID uint
	userID    uint
}

// NewWSClient 创建 WebSocket 客户端
func NewWSClient(conn *websocket.Conn, hub *WebSocketHub) *WSClient {
	return &WSClient{
		conn: conn,
		send: make(chan []byte, 256),
		hub:  hub,
	}
}

// ReadPump 从 WebSocket 连接读取消息
func (c *WSClient) ReadPump() {
	defer func() {
		c.hub.unregister <- c
		c.conn.Close()
	}()

	for {
		_, message, err := c.conn.ReadMessage()
		if err != nil {
			break
		}
		log.Printf("收到 WebSocket 消息: %s", message)
	}
}

// WritePump 向 WebSocket 连接写入消息
func (c *WSClient) WritePump() {
	defer c.conn.Close()

	for message := range c.send {
		if err := c.conn.WriteMessage(websocket.TextMessage, message); err != nil {
			break
		}
	}
}
