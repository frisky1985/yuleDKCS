package websocket

import (
	"log"
	"sync"
)

// Hub 管理所有 WebSocket 连接
// 实现发布-订阅模式

type Hub struct {
	// 注册的客户端
	clients map[*Client]bool

	// 注册请求
	register chan *Client

	// 注销请求
	unregister chan *Client

	// 广播消息到所有客户端
	broadcast chan []byte

	// 按车辆分组的客户端
	vehicleClients map[uint]map[*Client]bool

	// 按用户分组的客户端
	userClients map[uint]map[*Client]bool

	mu sync.RWMutex
}

// NewHub 创建新的 Hub
func NewHub() *Hub {
	return &Hub{
		clients:        make(map[*Client]bool),
		register:       make(chan *Client),
		unregister:     make(chan *Client),
		broadcast:      make(chan []byte),
		vehicleClients: make(map[uint]map[*Client]bool),
		userClients:    make(map[uint]map[*Client]bool),
	}
}

// Run 启动 Hub 主循环
func (h *Hub) Run() {
	for {
		select {
		case client := <-h.register:
			h.registerClient(client)

		case client := <-h.unregister:
			h.unregisterClient(client)

		case message := <-h.broadcast:
			h.broadcastToAll(message)
		}
	}
}

// registerClient 注册客户端
func (h *Hub) registerClient(client *Client) {
	h.mu.Lock()
	defer h.mu.Unlock()

	h.clients[client] = true

	// 如果客户端关联了车辆，添加到车辆分组
	if client.VehicleID > 0 {
		if h.vehicleClients[client.VehicleID] == nil {
			h.vehicleClients[client.VehicleID] = make(map[*Client]bool)
		}
		h.vehicleClients[client.VehicleID][client] = true
	}

	// 如果客户端关联了用户，添加到用户分组
	if client.UserID > 0 {
		if h.userClients[client.UserID] == nil {
			h.userClients[client.UserID] = make(map[*Client]bool)
		}
		h.userClients[client.UserID][client] = true
	}

	log.Printf("Client registered: %s (User: %d, Vehicle: %d)",
		client.ID, client.UserID, client.VehicleID)
}

// unregisterClient 注销客户端
func (h *Hub) unregisterClient(client *Client) {
	h.mu.Lock()
	defer h.mu.Unlock()

	if _, ok := h.clients[client]; ok {
		delete(h.clients, client)
		close(client.send)

		// 从车辆分组移除
		if client.VehicleID > 0 {
			if clients, ok := h.vehicleClients[client.VehicleID]; ok {
				delete(clients, client)
				if len(clients) == 0 {
					delete(h.vehicleClients, client.VehicleID)
				}
			}
		}

		// 从用户分组移除
		if client.UserID > 0 {
			if clients, ok := h.userClients[client.UserID]; ok {
				delete(clients, client)
				if len(clients) == 0 {
					delete(h.userClients, client.UserID)
				}
			}
		}

		log.Printf("Client unregistered: %s", client.ID)
	}
}

// broadcastToAll 广播到所有客户端
func (h *Hub) broadcastToAll(message []byte) {
	h.mu.RLock()
	defer h.mu.RUnlock()

	for client := range h.clients {
		select {
		case client.send <- message:
		default:
			// 发送缓冲区满，关闭连接
			close(client.send)
			delete(h.clients, client)
		}
	}
}

// BroadcastToVehicle 广播到指定车辆的所有客户端
func (h *Hub) BroadcastToVehicle(vehicleID uint, message []byte) {
	h.mu.RLock()
	defer h.mu.RUnlock()

	if clients, ok := h.vehicleClients[vehicleID]; ok {
		for client := range clients {
			select {
			case client.send <- message:
			default:
				// 发送缓冲区满
				log.Printf("Client %s send buffer full", client.ID)
			}
		}
	}
}

// BroadcastToUser 广播到指定用户的所有客户端
func (h *Hub) BroadcastToUser(userID uint, message []byte) {
	h.mu.RLock()
	defer h.mu.RUnlock()

	if clients, ok := h.userClients[userID]; ok {
		for client := range clients {
			select {
			case client.send <- message:
			default:
				log.Printf("Client %s send buffer full", client.ID)
			}
		}
	}
}

// GetClientCount 获取当前连接数
func (h *Hub) GetClientCount() int {
	h.mu.RLock()
	defer h.mu.RUnlock()
	return len(h.clients)
}

// GetVehicleClientCount 获取指定车辆的客户端数
func (h *Hub) GetVehicleClientCount(vehicleID uint) int {
	h.mu.RLock()
	defer h.mu.RUnlock()
	return len(h.vehicleClients[vehicleID])
}
