package mqtt

import (
	"context"
	"crypto/tls"
	"encoding/json"
	"fmt"
	"sync"
	"time"

	mqtt "github.com/eclipse/paho.mqtt.golang"
	"github.com/frisky1985/yuleDKCS/backend/internal/service"
	"yuleDKCS/backend/pkg/logger"
)

// Bridge MQTT 桥接服务
type Bridge struct {
	client         mqtt.Client
	vehicleService *service.VehicleService
	config         *Config
	
	// 订阅管理
	subscriptions  map[string]mqtt.MessageHandler
	subMutex       sync.RWMutex
	
	// 通道
	vehicleStatusChan chan VehicleStatusMessage
	commandResultChan chan CommandResultMessage
}

// Config MQTT 配置
type Config struct {
	BrokerHost   string
	BrokerPort   int
	ClientID     string
	Username     string
	Password     string
	UseTLS       bool
	TLSCertPath  string
	TLSKeyPath   string
	CACertPath   string
	KeepAlive    time.Duration
	PingTimeout  time.Duration
	WriteTimeout time.Duration
}

// DefaultConfig 默认配置
func DefaultConfig() *Config {
	return &Config{
		BrokerHost:   "localhost",
		BrokerPort:   1883,
		ClientID:     "yuledkcs-bridge",
		KeepAlive:    30 * time.Second,
		PingTimeout:  10 * time.Second,
		WriteTimeout: 10 * time.Second,
	}
}

// VehicleStatusMessage 车辆状态消息
type VehicleStatusMessage struct {
	VehicleID string                 `json:"vehicle_id"`
	Status    map[string]interface{} `json:"status"`
	Location  *LocationInfo          `json:"location,omitempty"`
	Timestamp time.Time              `json:"timestamp"`
}

// LocationInfo 位置信息
type LocationInfo struct {
	Lat      float64 `json:"lat"`
	Lon      float64 `json:"lon"`
	Accuracy float64 `json:"accuracy"`
}

// CommandResultMessage 命令结果消息
type CommandResultMessage struct {
	CommandID string    `json:"command_id"`
	VehicleID string    `json:"vehicle_id"`
	Command   string    `json:"command"`
	Status    string    `json:"status"` // success, failed, pending
	Result    interface{} `json:"result,omitempty"`
	Error     string    `json:"error,omitempty"`
	Timestamp time.Time `json:"timestamp"`
}

// NewBridge 创建 MQTT 桥接
func NewBridge(config *Config, vehicleService *service.VehicleService) (*Bridge, error) {
	if config == nil {
		config = DefaultConfig()
	}

	bridge := &Bridge{
		config:            config,
		vehicleService:    vehicleService,
		subscriptions:     make(map[string]mqtt.MessageHandler),
		vehicleStatusChan: make(chan VehicleStatusMessage, 100),
		commandResultChan: make(chan CommandResultMessage, 100),
	}

	// 配置 MQTT 客户端
	opts := mqtt.NewClientOptions()
	
	brokerURL := fmt.Sprintf("tcp://%s:%d", config.BrokerHost, config.BrokerPort)
	if config.UseTLS {
		brokerURL = fmt.Sprintf("ssl://%s:%d", config.BrokerHost, config.BrokerPort)
	}
	
	opts.AddBroker(brokerURL)
	opts.SetClientID(config.ClientID)
	opts.SetKeepAlive(config.KeepAlive)
	opts.SetPingTimeout(config.PingTimeout)
	opts.SetWriteTimeout(config.WriteTimeout)
	opts.SetAutoReconnect(true)
	opts.SetMaxReconnectInterval(5 * time.Minute)
	opts.SetCleanSession(false)
	opts.SetOrderMatters(false)
	
	// 认证
	if config.Username != "" {
		opts.SetUsername(config.Username)
		opts.SetPassword(config.Password)
	}
	
	// TLS 配置
	if config.UseTLS {
		tlsConfig := &tls.Config{
			InsecureSkipVerify: true, // 开发环境，生产环境应使用正确证书验证
		}
		opts.SetTLSConfig(tlsConfig)
	}
	
	// 设置回调
	opts.SetOnConnectHandler(bridge.onConnect)
	opts.SetConnectionLostHandler(bridge.onConnectionLost)
	opts.SetReconnectingHandler(bridge.onReconnecting)

	bridge.client = mqtt.NewClient(opts)

	return bridge, nil
}

// Connect 连接到 MQTT Broker
func (b *Bridge) Connect() error {
	logger.Info("连接 MQTT Broker...", 
		logger.String("broker", fmt.Sprintf("%s:%d", b.config.BrokerHost, b.config.BrokerPort)))

	token := b.client.Connect()
	if token.Wait() && token.Error() != nil {
		return fmt.Errorf("MQTT 连接失败: %w", token.Error())
	}

	logger.Info("MQTT 连接成功")
	return nil
}

// Disconnect 断开连接
func (b *Bridge) Disconnect() {
	logger.Info("断开 MQTT 连接")
	b.client.Disconnect(250)
	close(b.vehicleStatusChan)
	close(b.commandResultChan)
}

// IsConnected 检查连接状态
func (b *Bridge) IsConnected() bool {
	return b.client.IsConnected()
}

// Publish 发布消息
func (b *Bridge) Publish(topic string, payload interface{}, qos byte, retained bool) error {
	var data []byte
	var err error
	
	switch v := payload.(type) {
	case []byte:
		data = v
	case string:
		data = []byte(v)
	default:
		data, err = json.Marshal(payload)
		if err != nil {
			return fmt.Errorf("序列化失败: %w", err)
		}
	}

	token := b.client.Publish(topic, qos, retained, data)
	if token.Wait() && token.Error() != nil {
		return fmt.Errorf("发布失败: %w", token.Error())
	}

	return nil
}

// Subscribe 订阅主题
func (b *Bridge) Subscribe(topic string, qos byte, handler mqtt.MessageHandler) error {
	b.subMutex.Lock()
	defer b.subMutex.Unlock()

	token := b.client.Subscribe(topic, qos, handler)
	if token.Wait() && token.Error() != nil {
		return fmt.Errorf("订阅失败: %w", token.Error())
	}

	b.subscriptions[topic] = handler
	logger.Info("订阅主题", logger.String("topic", topic), logger.Uint8("qos", qos))
	return nil
}

// Unsubscribe 取消订阅
func (b *Bridge) Unsubscribe(topics ...string) error {
	b.subMutex.Lock()
	defer b.subMutex.Unlock()

	token := b.client.Unsubscribe(topics...)
	if token.Wait() && token.Error() != nil {
		return fmt.Errorf("取消订阅失败: %w", token.Error())
	}

	for _, topic := range topics {
		delete(b.subscriptions, topic)
	}

	return nil
}

// 回调函数

func (b *Bridge) onConnect(client mqtt.Client) {
	logger.Info("MQTT 已连接")
	
	// 重新订阅之前的主题
	b.subMutex.RLock()
	subscriptions := make(map[string]mqtt.MessageHandler)
	for topic, handler := range b.subscriptions {
		subscriptions[topic] = handler
	}
	b.subMutex.RUnlock()

	for topic, handler := range subscriptions {
		token := client.Subscribe(topic, 1, handler)
		if token.Wait() && token.Error() != nil {
			logger.Error("重新订阅失败", logger.String("topic", topic), logger.Error(token.Error()))
		}
	}

	// 订阅标准主题
	b.subscribeStandardTopics()
}

func (b *Bridge) onConnectionLost(client mqtt.Client, err error) {
	logger.Error("MQTT 连接断开", logger.Error(err))
}

func (b *Bridge) onReconnecting(client mqtt.Client, opts *mqtt.ClientOptions) {
	logger.Info("MQTT 正在重连...")
}

// 订阅标准主题
func (b *Bridge) subscribeStandardTopics() {
	// 订阅车辆状态更新
	b.Subscribe("vehicle/+/status", 1, b.handleVehicleStatus)
	
	// 订阅命令结果
	b.Subscribe("vehicle/+/command/result", 1, b.handleCommandResult)
	
	// 订阅心跳
	b.Subscribe("vehicle/+/heartbeat", 0, b.handleHeartbeat)
}

// 消息处理器

func (b *Bridge) handleVehicleStatus(client mqtt.Client, msg mqtt.Message) {
	var statusMsg VehicleStatusMessage
	if err := json.Unmarshal(msg.Payload(), &statusMsg); err != nil {
		logger.Error("解析车辆状态失败", logger.Error(err))
		return
	}

	// 将消息发送到处理管道
	select {
	case b.vehicleStatusChan <- statusMsg:
	default:
		logger.Warn("车辆状态通道已满")
	}

	// 更新数据库
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	if err := b.vehicleService.UpdateStatus(ctx, statusMsg.VehicleID, statusMsg.Status); err != nil {
		logger.Error("更新车辆状态失败", 
			logger.String("vehicle", statusMsg.VehicleID), 
			logger.Error(err))
	}
}

func (b *Bridge) handleCommandResult(client mqtt.Client, msg mqtt.Message) {
	var resultMsg CommandResultMessage
	if err := json.Unmarshal(msg.Payload(), &resultMsg); err != nil {
		logger.Error("解析命令结果失败", logger.Error(err))
		return
	}

	select {
	case b.commandResultChan <- resultMsg:
	default:
		logger.Warn("命令结果通道已满")
	}

	// 更新命令状态
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	if err := b.vehicleService.UpdateCommandResult(ctx, resultMsg.CommandID, resultMsg.Status, resultMsg.Result); err != nil {
		logger.Error("更新命令结果失败",
			logger.String("command", resultMsg.CommandID),
			logger.Error(err))
	}
}

func (b *Bridge) handleHeartbeat(client mqtt.Client, msg mqtt.Message) {
	// 解析车辆ID从主题
	vehicleID := extractVehicleID(msg.Topic())
	if vehicleID == "" {
		return
	}

	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	// 更新车辆在线状态
	if err := b.vehicleService.UpdateOnlineStatus(ctx, vehicleID, true); err != nil {
		logger.Error("更新在线状态失败", 
			logger.String("vehicle", vehicleID), 
			logger.Error(err))
	}
}

// 命令发送

// SendCommand 向车辆发送命令
func (b *Bridge) SendCommand(ctx context.Context, vehicleID, commandID, command string, params interface{}) error {
	topic := fmt.Sprintf("vehicle/%s/command", vehicleID)
	
	payload := map[string]interface{}{
		"command_id": commandID,
		"command":    command,
		"params":     params,
		"timestamp":  time.Now().Unix(),
	}

	return b.Publish(topic, payload, 1, false)
}

// BroadcastCommand 广播命令到多个车辆
func (b *Bridge) BroadcastCommand(ctx context.Context, vehicleIDs []string, command string, params interface{}) error {
	for _, vehicleID := range vehicleIDs {
		commandID := fmt.Sprintf("cmd_%d", time.Now().UnixNano())
		if err := b.SendCommand(ctx, vehicleID, commandID, command, params); err != nil {
			logger.Error("发送命令失败",
				logger.String("vehicle", vehicleID),
				logger.Error(err))
		}
	}
	return nil
}

// 订阅管理

// SubscribeVehicleStatus 订阅特定车辆状态
func (b *Bridge) SubscribeVehicleStatus(vehicleID string) error {
	topic := fmt.Sprintf("vehicle/%s/status", vehicleID)
	return b.Subscribe(topic, 1, b.handleVehicleStatus)
}

// UnsubscribeVehicleStatus 取消订阅车辆状态
func (b *Bridge) UnsubscribeVehicleStatus(vehicleID string) error {
	topic := fmt.Sprintf("vehicle/%s/status", vehicleID)
	return b.Unsubscribe(topic)
}

// 辅助函数

func extractVehicleID(topic string) string {
	// 从 "vehicle/{id}/..." 中提取 vehicle ID
	var vehicleID string
	fmt.Sscanf(topic, "vehicle/%s/", &vehicleID)
	return vehicleID
}

// GetVehicleStatusChan 获取车辆状态通道
func (b *Bridge) GetVehicleStatusChan() <-chan VehicleStatusMessage {
	return b.vehicleStatusChan
}

// GetCommandResultChan 获取命令结果通道
func (b *Bridge) GetCommandResultChan() <-chan CommandResultMessage {
	return b.commandResultChan
}
