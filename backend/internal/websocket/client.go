package websocket

import (
	"encoding/json"
	"log"
	"net/http"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
	"github.com/gorilla/websocket"
)

const (
	// 写等待时间
	writeWait = 10 * time.Second

	// 从对等方读取下一个 pong 消息的超时时间
	pongWait = 60 * time.Second

	// 发送 ping 到对等方的间隔
	pingPeriod = (pongWait * 9) / 10

	// 最大消息大小
	maxMessageSize = 512 * 1024 // 512KB
)

var upgrader = websocket.Upgrader{
	ReadBufferSize:  1024,
	WriteBufferSize: 1024,
	CheckOrigin: func(r *http.Request) bool {
		// 生产环境中应该检查来源
		return true
	},
}

// Client 表示一个 WebSocket 客户端连接
type Client struct {
	Hub *Hub

	// WebSocket 连接
	conn *websocket.Conn

	// 发送消息的缓冲通道
	send chan []byte

	// 客户端唯一 ID
	ID string

	// 关联的用户 ID
	UserID uint

	// 关联的车辆 ID
	VehicleID uint

	// 客户端类型: "user", "vehicle", "admin"
	ClientType string

	// 连接时间
	ConnectedAt time.Time
}

// Message WebSocket 消息结构
type Message struct {
	Type      string      `json:"type"`
	Timestamp time.Time   `json:"timestamp"`
	Payload   interface{} `json:"payload"`
}

// NewClient 创建新客户端
func NewClient(hub *Hub, conn *websocket.Conn, userID, vehicleID uint, clientType string) *Client {
	return &Client{
		Hub:         hub,
		conn:        conn,
		send:        make(chan []byte, 256),
		ID:          uuid.New().String(),
		UserID:      userID,
		VehicleID:   vehicleID,
		ClientType:  clientType,
		ConnectedAt: time.Now(),
	}
}

// ReadPump 处理从对等方发送的消息
func (c *Client) ReadPump() {
	defer func() {
		c.Hub.unregister <- c
		c.conn.Close()
	}()

	c.conn.SetReadLimit(maxMessageSize)
	c.conn.SetReadDeadline(time.Now().Add(pongWait))
	c.conn.SetPongHandler(func(string) error {
		c.conn.SetReadDeadline(time.Now().Add(pongWait))
		return nil
	})

	for {
		_, message, err := c.conn.ReadMessage()
		if err != nil {
			if websocket.IsUnexpectedCloseError(err, websocket.CloseGoingAway, websocket.CloseAbnormalClosure) {
				log.Printf("WebSocket error: %v", err)
			}
			break
		}

		// 处理接收到的消息
		c.handleMessage(message)
	}
}

// WritePump 处理发送消息到对等方
func (c *Client) WritePump() {
	ticker := time.NewTicker(pingPeriod)
	defer func() {
		ticker.Stop()
		c.conn.Close()
	}()

	for {
		select {
		case message, ok := <-c.send:
			c.conn.SetWriteDeadline(time.Now().Add(writeWait))
			if !ok {
				// Hub 关闭了通道
				c.conn.WriteMessage(websocket.CloseMessage, []byte{})
				return
			}

			w, err := c.conn.NextWriter(websocket.TextMessage)
			if err != nil {
				return
			}
			w.Write(message)

			// 添加待发送的消息
			n := len(c.send)
			for i := 0; i < n; i++ {
				w.Write([]byte{'\n'})
				w.Write(<-c.send)
			}

			if err := w.Close(); err != nil {
				return
			}

		case <-ticker.C:
			c.conn.SetWriteDeadline(time.Now().Add(writeWait))
			if err := c.conn.WriteMessage(websocket.PingMessage, nil); err != nil {
				return
			}
		}
	}
}

// handleMessage 处理接收到的消息
func (c *Client) handleMessage(data []byte) {
	var msg Message
	if err := json.Unmarshal(data, &msg); err != nil {
		log.Printf("Failed to unmarshal message: %v", err)
		return
	}

	switch msg.Type {
	case "vehicle_status":
		// 车辆状态更新
		c.handleVehicleStatus(msg.Payload)

	case "command_result":
		// 命令执行结果
		c.handleCommandResult(msg.Payload)

	case "ping":
		// 响应 ping
		c.SendJSON(Message{
			Type:      "pong",
			Timestamp: time.Now(),
		})

	default:
		log.Printf("Unknown message type: %s", msg.Type)
	}
}

// handleVehicleStatus 处理车辆状态更新
func (c *Client) handleVehicleStatus(payload interface{}) {
	// 将状态广播给订阅该车辆的所有用户客户端
	if c.VehicleID > 0 {
		data, _ := json.Marshal(Message{
			Type:      "vehicle_status_update",
			Timestamp: time.Now(),
			Payload:   payload,
		})
		c.Hub.BroadcastToVehicle(c.VehicleID, data)
	}
}

// handleCommandResult 处理命令执行结果
func (c *Client) handleCommandResult(payload interface{}) {
	// 将结果广播给发送命令的用户
	if c.VehicleID > 0 {
		data, _ := json.Marshal(Message{
			Type:      "command_result",
			Timestamp: time.Now(),
			Payload:   payload,
		})
		c.Hub.BroadcastToVehicle(c.VehicleID, data)
	}
}

// SendJSON 发送 JSON 消息
func (c *Client) SendJSON(v interface{}) error {
	data, err := json.Marshal(v)
	if err != nil {
		return err
	}
	c.send <- data
	return nil
}

// SendMessage 发送消息
func (c *Client) SendMessage(msgType string, payload interface{}) error {
	return c.SendJSON(Message{
		Type:      msgType,
		Timestamp: time.Now(),
		Payload:   payload,
	})
}

// ServeWs 处理 WebSocket 升级请求
func ServeWs(hub *Hub, userID, vehicleID uint, clientType string) gin.HandlerFunc {
	return func(c *gin.Context) {
		conn, err := upgrader.Upgrade(c.Writer, c.Request, nil)
		if err != nil {
			log.Printf("WebSocket upgrade failed: %v", err)
			return
		}

		client := NewClient(hub, conn, userID, vehicleID, clientType)
		client.Hub.register <- client

		// 启动读写 goroutines
		go client.WritePump()
		go client.ReadPump()
	}
}
