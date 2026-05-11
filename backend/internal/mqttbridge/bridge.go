package mqttbridge

import (
	"context"
	"crypto/tls"
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"os"
	"sync"
	"time"

	"github.com/gorilla/websocket"
	mqtt "github.com/eclipse/paho.mqtt.golang"
)

// Bridge MQTT 和 WebSocket 之间的桥接服务
type Bridge struct {
	// MQTT 客户端
	mqttClient mqtt.Client

	// WebSocket Hub
	wsHub *WebSocketHub

	// 配置
	config *BridgeConfig

	// 状态
	running bool
	mu      sync.RWMutex

	// 消息缓冲
	messageChan chan *BridgeMessage

	// HTTP 服务器
	httpServer *http.Server
}

// BridgeConfig 桥接配置
type BridgeConfig struct {
	MQTTBroker    string
	MQTTTLSBroker string
	WSBroker      string
	WSTLSBroker   string
	APIURL        string
	JWTSecret     string
	WSPort        string
}

// BridgeMessage 桥接消息格式
type BridgeMessage struct {
	Topic     string          `json:"topic"`
	Payload   json.RawMessage `json:"payload"`
	Timestamp time.Time       `json:"timestamp"`
	Source    string          `json:"source"` // "mqtt" 或 "websocket"
	VehicleID uint            `json:"vehicle_id,omitempty"`
	UserID    uint            `json:"user_id,omitempty"`
}

// NewBridge 创建新的 Bridge 实例
func NewBridge() (*Bridge, error) {
	config := loadConfig()

	bridge := &Bridge{
		config:      config,
		messageChan: make(chan *BridgeMessage, 1000),
		wsHub:       NewWebSocketHub(),
	}

	// 初始化 MQTT 客户端
	if err := bridge.initMQTT(); err != nil {
		return nil, fmt.Errorf("failed to init MQTT: %w", err)
	}

	// 初始化 HTTP 服务器
	bridge.initHTTPServer()

	return bridge, nil
}

// loadConfig 加载配置
func loadConfig() *BridgeConfig {
	return &BridgeConfig{
		MQTTBroker:    getEnv("YULEDKCS_MQTT_BROKER", "localhost:1883"),
		MQTTTLSBroker: getEnv("YULEDKCS_MQTT_TLS_BROKER", "localhost:8883"),
		WSBroker:      getEnv("YULEDKCS_MQTT_WS_BROKER", "localhost:8083"),
		WSTLSBroker:   getEnv("YULEDKCS_MQTT_WSS_BROKER", "localhost:8084"),
		APIURL:        getEnv("YULEDKCS_API_URL", "http://localhost:8080"),
		JWTSecret:     getEnv("YULEDKCS_JWT_SECRET", "your-secret-key"),
		WSPort:        getEnv("YULEDKCS_WS_PORT", "8085"),
	}
}

// initMQTT 初始化 MQTT 客户端
func (b *Bridge) initMQTT() error {
	opts := mqtt.NewClientOptions()

	// 优先使用 TLS 连接
	if b.config.MQTTTLSBroker != "" {
		opts.AddBroker(fmt.Sprintf("ssl://%s", b.config.MQTTTLSBroker))
		opts.SetTLSConfig(&tls.Config{
			InsecureSkipVerify: true, // 生产环境中应使用正确的证书验证
		})
	} else {
		opts.AddBroker(fmt.Sprintf("tcp://%s", b.config.MQTTBroker))
	}

	opts.SetClientID(fmt.Sprintf("yuledkcs-bridge-%d", time.Now().Unix()))
	opts.SetAutoReconnect(true)
	opts.SetConnectRetry(true)
	opts.SetConnectRetryInterval(5 * time.Second)
	opts.SetMaxReconnectInterval(30 * time.Second)
	opts.SetKeepAlive(60 * time.Second)
	opts.SetPingTimeout(10 * time.Second)
	opts.SetCleanSession(false)
	opts.SetOrderMatters(false)

	// 设置回调
	opts.SetDefaultPublishHandler(b.mqttMessageHandler)
	opts.SetOnConnectHandler(b.onConnectHandler)
	opts.SetConnectionLostHandler(b.onConnectionLostHandler)

	// 认证
	opts.SetUsername("bridge")
	opts.SetPassword(b.config.JWTSecret)

	b.mqttClient = mqtt.NewClient(opts)

	return nil
}

// initHTTPServer 初始化 HTTP 服务器
func (b *Bridge) initHTTPServer() {
	mux := http.NewServeMux()
	mux.HandleFunc("/ws", b.handleWebSocket)
	mux.HandleFunc("/health", b.handleHealth)
	mux.HandleFunc("/api/v1/mqtt/auth", b.handleMQTTAuth)
	mux.HandleFunc("/api/v1/mqtt/acl", b.handleMQTTACL)

	b.httpServer = &http.Server{
		Addr:    ":" + b.config.WSPort,
		Handler: mux,
	}
}

// Start 启动 Bridge
func (b *Bridge) Start(ctx context.Context) error {
	b.mu.Lock()
	defer b.mu.Unlock()

	if b.running {
		return fmt.Errorf("bridge already running")
	}

	// 连接 MQTT
	if token := b.mqttClient.Connect(); token.Wait() && token.Error() != nil {
		return fmt.Errorf("failed to connect to MQTT: %w", token.Error())
	}

	// 订阅主题
	if err := b.subscribeTopics(); err != nil {
		return fmt.Errorf("failed to subscribe: %w", err)
	}

	// 启动 WebSocket Hub
	go b.wsHub.Run()

	// 启动消息转发
	go b.messageForwarder(ctx)

	// 启动 HTTP 服务器
	go func() {
		log.Printf("启动 HTTP 服务器在端口 %s", b.config.WSPort)
		if err := b.httpServer.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			log.Printf("HTTP 服务器错误: %v", err)
		}
	}()

	b.running = true
	return nil
}

// Stop 停止 Bridge
func (b *Bridge) Stop() error {
	b.mu.Lock()
	defer b.mu.Unlock()

	if !b.running {
		return nil
	}

	// 关闭 HTTP 服务器
	if b.httpServer != nil {
		ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer cancel()
		b.httpServer.Shutdown(ctx)
	}

	// 断开 MQTT 连接
	b.mqttClient.Disconnect(250)

	// 关闭 WebSocket Hub
	b.wsHub.Stop()

	// 关闭消息通道
	close(b.messageChan)

	b.running = false
	return nil
}

// subscribeTopics 订阅 MQTT 主题
func (b *Bridge) subscribeTopics() error {
	// 订阅车辆相关主题
	topics := []string{
		"vehicle/+/status",
		"vehicle/+/telemetry",
		"vehicle/+/response",
		"user/+/notification",
		"ota/+/progress",
		"key/+/status",
	}

	for _, topic := range topics {
		if token := b.mqttClient.Subscribe(topic, 1, b.mqttMessageHandler); token.Wait() && token.Error() != nil {
			return fmt.Errorf("failed to subscribe to %s: %w", topic, token.Error())
		}
		log.Printf("已订阅主题: %s", topic)
	}

	return nil
}

// mqttMessageHandler MQTT 消息处理器
func (b *Bridge) mqttMessageHandler(client mqtt.Client, msg mqtt.Message) {
	bridgeMsg := &BridgeMessage{
		Topic:     msg.Topic(),
		Payload:   msg.Payload(),
		Timestamp: time.Now(),
		Source:    "mqtt",
	}

	// 解析 vehicle_id 或 user_id
	// 格式: vehicle/{id}/status 或 user/{id}/notification
	var id uint
	fmt.Sscanf(msg.Topic(), "vehicle/%d/", &id)
	if id > 0 {
		bridgeMsg.VehicleID = id
	}

	select {
	case b.messageChan <- bridgeMsg:
	default:
		log.Println("消息通道已满，丢弃消息")
	}
}

// onConnectHandler MQTT 连接成功处理器
func (b *Bridge) onConnectHandler(client mqtt.Client) {
	log.Println("✅ MQTT 连接成功")
	// 重新订阅主题
	b.subscribeTopics()
}

// onConnectionLostHandler MQTT 连接断开处理器
func (b *Bridge) onConnectionLostHandler(client mqtt.Client, err error) {
	log.Printf("⚠️ MQTT 连接断开: %v", err)
}

// messageForwarder 消息转发器
func (b *Bridge) messageForwarder(ctx context.Context) {
	for {
		select {
		case msg := <-b.messageChan:
			if msg == nil {
				return
			}
			b.forwardMessage(msg)
		case <-ctx.Done():
			return
		}
	}
}

// forwardMessage 转发消息到 WebSocket
func (b *Bridge) forwardMessage(msg *BridgeMessage) {
	data, err := json.Marshal(msg)
	if err != nil {
		log.Printf("序列化消息失败: %v", err)
		return
	}

	// 根据车辆 ID 或用户 ID 转发
	if msg.VehicleID > 0 {
		b.wsHub.BroadcastToVehicle(msg.VehicleID, data)
	} else if msg.UserID > 0 {
		b.wsHub.BroadcastToUser(msg.UserID, data)
	} else {
		// 广播到所有客户端
		b.wsHub.Broadcast(data)
	}
}

// handleWebSocket WebSocket 处理器
func (b *Bridge) handleWebSocket(w http.ResponseWriter, r *http.Request) {
	upgrader := websocket.Upgrader{
		CheckOrigin: func(r *http.Request) bool {
			return true // 生产环境中应进行更严格的检查
		},
	}

	conn, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		log.Printf("WebSocket 升级失败: %v", err)
		return
	}

	client := NewWSClient(conn, b)
	b.wsHub.Register(client)

	// 启动客户端处理
	go client.ReadPump()
	go client.WritePump()
}

// handleHealth 健康检查处理器
func (b *Bridge) handleHealth(w http.ResponseWriter, r *http.Request) {
	status := map[string]interface{}{
		"status":    "healthy",
		"mqtt":      b.mqttClient.IsConnected(),
		"timestamp": time.Now().Unix(),
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(status)
}

// handleMQTTAuth MQTT 认证处理器 (EMQ X HTTP 认证回调)
func (b *Bridge) handleMQTTAuth(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	var req struct {
		Username string `json:"username"`
		Password string `json:"password"`
		ClientID string `json:"clientid"`
	}

	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		http.Error(w, "Invalid request", http.StatusBadRequest)
		return
	}

	// 验证 JWT Token
	// 实际应用中应调用后端 API 进行验证
	if req.Username == "bridge" && req.Password == b.config.JWTSecret {
		w.WriteHeader(http.StatusOK)
		json.NewEncoder(w).Encode(map[string]string{"result": "allow"})
		return
	}

	// 其他用户的 JWT 验证
	// TODO: 调用后端认证服务

	w.WriteHeader(http.StatusOK)
	json.NewEncoder(w).Encode(map[string]string{"result": "deny"})
}

// handleMQTTACL MQTT ACL 处理器 (EMQ X HTTP ACL 回调)
func (b *Bridge) handleMQTTACL(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	var req struct {
		Username string `json:"username"`
		Topic    string `json:"topic"`
		Action   string `json:"action"` // publish, subscribe
		ClientID string `json:"clientid"`
	}

	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		http.Error(w, "Invalid request", http.StatusBadRequest)
		return
	}

	// ACL 检查逻辑
	// 实际应用中应根据用户权限进行检查
	allowed := b.checkACL(req.Username, req.Topic, req.Action)

	w.WriteHeader(http.StatusOK)
	if allowed {
		json.NewEncoder(w).Encode(map[string]string{"result": "allow"})
	} else {
		json.NewEncoder(w).Encode(map[string]string{"result": "deny"})
	}
}

// checkACL 检查 ACL 权限
func (b *Bridge) checkACL(username, topic, action string) bool {
	// 系统用户允许所有操作
	if username == "admin" || username == "bridge" {
		return true
	}

	// 检查主题格式
	// 允许的主题前缀
	allowedPrefixes := []string{
		"vehicle/",
		"user/",
		"ota/",
		"key/",
	}

	for _, prefix := range allowedPrefixes {
		if len(topic) > len(prefix) && topic[:len(prefix)] == prefix {
			return true
		}
	}

	return false
}

// PublishToMQTT 发布消息到 MQTT
func (b *Bridge) PublishToMQTT(topic string, payload interface{}, qos byte) error {
	data, err := json.Marshal(payload)
	if err != nil {
		return fmt.Errorf("failed to marshal payload: %w", err)
	}

	token := b.mqttClient.Publish(topic, qos, false, data)
	token.Wait()

	return token.Error()
}

// getEnv 获取环境变量，如果不存在返回默认值
func getEnv(key, defaultValue string) string {
	if value := os.Getenv(key); value != "" {
		return value
	}
	return defaultValue
}
