// telemetry.go - 统一埋点/遥测接口
// 三端统一埋点规范: Cloud / SDK / Vehicle

package telemetry

import (
	"encoding/json"
	"fmt"
	"sync"
	"time"
)

// 事件类型
type EventType string

const (
	// App事件
	EventAppLaunch         EventType = "app_launch"
	EventAppBackground     EventType = "app_background"
	EventSdkInit           EventType = "sdk_init"
	EventSdkError          EventType = "sdk_error"

	// 用户事件
	EventUserLogin          EventType = "user_login"
	EventUserLogout         EventType = "user_logout"

	// 设备事件
	EventDeviceBindStart    EventType = "device_bind_start"
	EventDeviceBindSuccess  EventType = "device_bind_success"
	EventDeviceBindFailed   EventType = "device_bind_failed"
	EventDeviceUnbind       EventType = "device_unbind"

	// 密钥事件
	EventKeyCreate          EventType = "key_create"
	EventKeyDelete          EventType = "key_delete"
	EventKeyRefresh         EventType = "key_refresh"
	EventKeyUse             EventType = "key_use"
	EventKeyExpired         EventType = "key_expired"

	// 车辆事件
	EventVehicleUnlock      EventType = "vehicle_unlock"
	EventVehicleLock       EventType = "vehicle_lock"
	EventVehicleStart      EventType = "vehicle_start"
	EventVehicleStop       EventType = "vehicle_stop"

	// 分享事件
	EventShareCreate       EventType = "share_create"
	EventShareAccept       EventType = "share_accept"
	EventShareRevoke       EventType = "share_revoke"

	// 通道事件
	EventBleScan           EventType = "ble_scan"
	EventBleConnect        EventType = "ble_connect"
	EventBleDisconnect     EventType = "ble_disconnect"
	EventNfcTap            EventType = "nfc_tap"
	EventUwbRanging        EventType = "uwb_ranging"
	EventChannelSwitch     EventType = "channel_switch"

	// 协议事件
	EventProtocolMsg       EventType = "protocol_msg"

	// 安全事件
	EventSecurityAlert     EventType = "security_alert"
)

// 事件来源
type Source string

const (
	SourceSDK     Source = "SDK"
	SourceTCU     Source = "TCU"
	SourceCloud   Source = "CLOUD"
)

// TelemetryEvent 埋点事件
type TelemetryEvent struct {
	EventID    string                 `json:"event_id"`
	EventType  EventType               `json:"event_type"`
	Timestamp  int64                  `json:"timestamp"`
	Source     Source                  `json:"source"`
	SessionID  string                 `json:"session_id,omitempty"`
	UserID     string                 `json:"user_id,omitempty"`
	DeviceID   string                 `json:"device_id,omitempty"`
	VehicleID  string                 `json:"vehicle_id,omitempty"`
	KeyID      string                 `json:"key_id,omitempty"`
	TraceID    string                 `json:"trace_id,omitempty"`
	Properties map[string]interface{} `json:"properties,omitempty"`
	Context    map[string]interface{} `json:"context,omitempty"`
}

// Telemetry 埋点接口
type Telemetry interface {
	Track(eventType EventType, properties map[string]interface{})
	TrackError(code uint16, errMsg string, context map[string]interface{})
	SetUser(userID string)
	SetDevice(deviceID string)
	SetVehicle(vehicleID string)
	Flush()
}

// DefaultTelemetry 默认埋点实现
type DefaultTelemetry struct {
	mu        sync.Mutex
	events    []*TelemetryEvent
	queue     chan *TelemetryEvent
	userID    string
	deviceID  string
	vehicleID string
	sessionID string
	source    Source
}

// TelemetryConfig 埋点配置
type TelemetryConfig struct {
	Source      Source
	SessionID   string
	BatchSize   int
	FlushInterval time.Duration
}

// NewTelemetry 创建埋点器
func NewTelemetry(config TelemetryConfig) *DefaultTelemetry {
	t := &DefaultTelemetry{
		source:    config.Source,
		sessionID: config.SessionID,
		queue:     make(chan *TelemetryEvent, 1000),
	}

	// 启动异步处理
	go t.processQueue()

	return t
}

// Track 埋点
func (t *DefaultTelemetry) Track(eventType EventType, properties map[string]interface{}) {
	event := &TelemetryEvent{
		EventID:    generateEventID(),
		EventType:  eventType,
		Timestamp:  time.Now().UnixMilli(),
		Source:     t.source,
		SessionID:  t.sessionID,
		UserID:     t.userID,
		DeviceID:   t.deviceID,
		VehicleID:  t.vehicleID,
		Properties: properties,
	}

	t.mu.Lock()
	t.events = append(t.events, event)
	t.mu.Unlock()

	// 发送到队列
	select {
	case t.queue <- event:
	default:
		// 队列满，丢弃
	}
}

// TrackError 错误埋点
func (t *DefaultTelemetry) TrackError(code uint16, errMsg string, context map[string]interface{}) {
	props := map[string]interface{}{
		"error_code": code,
		"error_msg":  errMsg,
	}
	for k, v := range context {
		props[k] = v
	}
	t.Track(EventSdkError, props)
}

// SetUser 设置用户ID
func (t *DefaultTelemetry) SetUser(userID string) {
	t.mu.Lock()
	defer t.mu.Unlock()
	t.userID = userID
}

// SetDevice 设置设备ID
func (t *DefaultTelemetry) SetDevice(deviceID string) {
	t.mu.Lock()
	defer t.mu.Unlock()
	t.deviceID = deviceID
}

// SetVehicle 设置车辆ID
func (t *DefaultTelemetry) SetVehicle(vehicleID string) {
	t.mu.Lock()
	defer t.mu.Unlock()
	t.vehicleID = vehicleID
}

// Flush 刷新
func (t *DefaultTelemetry) Flush() {
	t.mu.Lock()
	defer t.mu.Unlock()

	// 发送所有事件到队列
	for _, event := range t.events {
		select {
		case t.queue <- event:
		default:
		}
	}
	t.events = nil
}

// processQueue 处理队列
func (t *DefaultTelemetry) processQueue() {
	for event := range t.queue {
		t.sendEvent(event)
	}
}

// sendEvent 发送事件到后端
func (t *DefaultTelemetry) sendEvent(event *TelemetryEvent) {
	// TODO: 实现实际的后端发送逻辑
	data, _ := json.Marshal(event)
	fmt.Printf("[TELEMETRY] %s\n", string(data))
}

// 便捷方法
func (t *DefaultTelemetry) TrackKeyUse(keyID, vehicleID, channel string, durationMs int64) {
	t.Track(EventKeyUse, map[string]interface{}{
		"key_id":       keyID,
		"vehicle_id":   vehicleID,
		"channel_type": channel,
		"duration_ms":  durationMs,
	})
}

func (t *DefaultTelemetry) TrackVehicleCommand(action, vehicleID, result string, errorCode *uint16) {
	props := map[string]interface{}{
		"action":     action,
		"vehicle_id": vehicleID,
		"result":     result,
	}
	if errorCode != nil {
		props["error_code"] = *errorCode
	}
	t.Track(EventProtocolMsg, props)
}

func (t *DefaultTelemetry) TrackSecurityEvent(eventType string, threatLevel int, details map[string]interface{}) {
	props := map[string]interface{}{
		"security_event_type": eventType,
		"threat_level":        threatLevel,
	}
	for k, v := range details {
		props[k] = v
	}
	t.Track(EventSecurityAlert, props)
}

func (t *DefaultTelemetry) TrackBleConnect(deviceID, macAddress string, mtu int, success bool) {
	t.Track(EventBleConnect, map[string]interface{}{
		"device_id":   deviceID,
		"mac_address": macAddress,
		"mtu":         mtu,
		"success":     success,
	})
}

func (t *DefaultTelemetry) TrackNfcTap(tagID, vehicleID string, success bool) {
	t.Track(EventNfcTap, map[string]interface{}{
		"nfc_tag_id": tagID,
		"vehicle_id": vehicleID,
		"success":    success,
	})
}

func (t *DefaultTelemetry) TrackUwbRanging(vehicleID string, distanceMm, accuracyMm int) {
	t.Track(EventUwbRanging, map[string]interface{}{
		"vehicle_id":     vehicleID,
		"distance_mm":    distanceMm,
		"accuracy_mm":    accuracyMm,
	})
}

// generateEventID 生成事件ID
func generateEventID() string {
	return fmt.Sprintf("%d-%s", time.Now().UnixNano(), randomString(8))
}

// randomString 生成随机字符串
func randomString(n int) string {
	const letters = "abcdefghijklmnopqrstuvwxyz0123456789"
	b := make([]byte, n)
	for i := range b {
		b[i] = letters[time.Now().UnixNano()%int64(len(letters))]
	}
	return string(b)
}

// 全局埋点器
var globalTelemetry *DefaultTelemetry

// Init 初始化全局埋点器
func Init(config TelemetryConfig) {
	globalTelemetry = NewTelemetry(config)
}

// Default 获取默认埋点器
func Default() *DefaultTelemetry {
	if globalTelemetry == nil {
		globalTelemetry = NewTelemetry(TelemetryConfig{
			Source: SourceCloud,
		})
	}
	return globalTelemetry
}

// Track 便捷函数
func Track(eventType EventType, properties map[string]interface{}) {
	Default().Track(eventType, properties)
}
