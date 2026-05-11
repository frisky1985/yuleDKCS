package anomaly

import (
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"net/url"
	"sync"
	"time"

	"github.com/gorilla/websocket"
)

// WebSocketConfig holds WebSocket client configuration
type WebSocketConfig struct {
	// BaseURL is the WebSocket URL (ws:// or wss://)
	BaseURL string

	// APIKey for authentication
	APIKey string

	// ReconnectInterval is the interval between reconnection attempts
	ReconnectInterval time.Duration

	// MaxReconnectAttempts is the maximum number of reconnection attempts
	MaxReconnectAttempts int

	// PingInterval is the interval for sending ping frames
	PingInterval time.Duration

	// WriteTimeout is the timeout for write operations
	WriteTimeout time.Duration

	// ReadTimeout is the timeout for read operations
	ReadTimeout time.Duration
}

// DefaultWebSocketConfig returns default WebSocket configuration
func DefaultWebSocketConfig(baseURL string) WebSocketConfig {
	return WebSocketConfig{
		BaseURL:              baseURL,
		ReconnectInterval:    5 * time.Second,
		MaxReconnectAttempts: 10,
		PingInterval:         30 * time.Second,
		WriteTimeout:         10 * time.Second,
		ReadTimeout:          60 * time.Second,
	}
}

// StreamMessage represents a message sent/received over WebSocket
type StreamMessage struct {
	Type      string          `json:"type"`
	Timestamp time.Time       `json:"timestamp"`
	Payload   json.RawMessage `json:"payload"`
}

// DetectionMessage represents a detection request message
type DetectionMessage struct {
	Type          DetectionType `json:"type"`
	Data          DetectionData `json:"data"`
	SaveAnomaly   bool          `json:"save_anomaly"`
	RequestID     string        `json:"request_id"`
}

// DetectionResponseMessage represents a detection response message
type DetectionResponseMessage struct {
	RequestID string          `json:"request_id"`
	Result    DetectionResult `json:"result"`
	Error     string          `json:"error,omitempty"`
}

// WebSocketClient is a WebSocket client for streaming anomaly detection
type WebSocketClient struct {
	config        WebSocketConfig
	conn          *websocket.Conn
	baseURL       *url.URL
	dialer        *websocket.Dialer

	// Channels
	sendCh        chan interface{}
	receiveCh     chan StreamMessage
	errCh         chan error
	stopCh        chan struct{}

	// Callbacks
	onConnect     func()
	onDisconnect  func()
	onMessage     func(StreamMessage)
	onDetectionResult func(DetectionResponseMessage)

	// State
	connected     bool
	mu            sync.RWMutex
	reconnectCnt  int
	wg            sync.WaitGroup
}

// NewWebSocketClient creates a new WebSocket client
func NewWebSocketClient(config WebSocketConfig) (*WebSocketClient, error) {
	if config.BaseURL == "" {
		return nil, fmt.Errorf("base URL is required")
	}

	baseURL, err := url.Parse(config.BaseURL)
	if err != nil {
		return nil, fmt.Errorf("invalid base URL: %w", err)
	}

	return &WebSocketClient{
		config:    config,
		baseURL:   baseURL,
		dialer:    &websocket.Dialer{
			HandshakeTimeout: 10 * time.Second,
		},
		sendCh:    make(chan interface{}, 100),
		receiveCh: make(chan StreamMessage, 100),
		errCh:     make(chan error, 10),
		stopCh:    make(chan struct{}),
	}, nil
}

// Connect establishes a WebSocket connection
func (c *WebSocketClient) Connect(ctx context.Context) error {
	c.mu.Lock()
	defer c.mu.Unlock()

	if c.connected {
		return nil
	}

	// Build WebSocket URL
	wsURL := c.baseURL.ResolveReference(&url.URL{Path: "/api/v1/ws/detect"})

	// Add API key as query parameter if present
	headers := http.Header{}
	if c.config.APIKey != "" {
		headers.Set("Authorization", "Bearer "+c.config.APIKey)
	}

	// Connect
	conn, resp, err := c.dialer.DialContext(ctx, wsURL.String(), headers)
	if err != nil {
		return fmt.Errorf("websocket dial failed: %w", err)
	}
	if resp != nil {
		resp.Body.Close()
	}

	c.conn = conn
	c.connected = true
	c.reconnectCnt = 0

	// Start goroutines
	c.wg.Add(3)
	go c.readPump()
	go c.writePump()
	go c.pingPump()

	if c.onConnect != nil {
		go c.onConnect()
	}

	return nil
}

// Close closes the WebSocket connection
func (c *WebSocketClient) Close() error {
	c.mu.Lock()
	if !c.connected {
		c.mu.Unlock()
		return nil
	}

	c.connected = false
	c.mu.Unlock()

	close(c.stopCh)

	// Send close message
	if c.conn != nil {
		c.conn.WriteMessage(websocket.CloseMessage, websocket.FormatCloseMessage(websocket.CloseNormalClosure, ""))
		c.conn.Close()
	}

	// Wait for goroutines to finish
	c.wg.Wait()

	return nil
}

// IsConnected returns whether the client is connected
func (c *WebSocketClient) IsConnected() bool {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.connected
}

// Send sends a message over WebSocket
func (c *WebSocketClient) Send(msg interface{}) error {
	select {
	case c.sendCh <- msg:
		return nil
	case <-c.stopCh:
		return fmt.Errorf("websocket client is closed")
	default:
		return fmt.Errorf("send buffer is full")
	}
}

// SendDetection sends a detection request
func (c *WebSocketClient) SendDetection(msg DetectionMessage) error {
	return c.Send(msg)
}

// readPump reads messages from the WebSocket connection
func (c *WebSocketClient) readPump() {
	defer c.wg.Done()

	for {
		select {
		case <-c.stopCh:
			return
		default:
		}

		c.conn.SetReadDeadline(time.Now().Add(c.config.ReadTimeout))
		messageType, data, err := c.conn.ReadMessage()
		if err != nil {
			if websocket.IsUnexpectedCloseError(err, websocket.CloseGoingAway, websocket.CloseAbnormalClosure) {
				select {
				case c.errCh <- fmt.Errorf("websocket read error: %w", err):
				default:
				}
			}
			c.handleDisconnect()
			return
		}

		if messageType == websocket.TextMessage || messageType == websocket.BinaryMessage {
			var msg StreamMessage
			if err := json.Unmarshal(data, &msg); err != nil {
				select {
				case c.errCh <- fmt.Errorf("failed to unmarshal message: %w", err):
				default:
				}
				continue
			}

			// Process message
			c.handleMessage(msg)
		}
	}
}

// writePump writes messages to the WebSocket connection
func (c *WebSocketClient) writePump() {
	defer c.wg.Done()

	for {
		select {
		case msg := <-c.sendCh:
			c.conn.SetWriteDeadline(time.Now().Add(c.config.WriteTimeout))
			if err := c.conn.WriteJSON(msg); err != nil {
				select {
				case c.errCh <- fmt.Errorf("websocket write error: %w", err):
				default:
				}
			}

		case <-c.stopCh:
			return
		}
	}
}

// pingPump sends periodic ping messages
func (c *WebSocketClient) pingPump() {
	defer c.wg.Done()

	ticker := time.NewTicker(c.config.PingInterval)
	defer ticker.Stop()

	for {
		select {
		case <-ticker.C:
			c.mu.RLock()
			conn := c.conn
			c.mu.RUnlock()

			if conn != nil {
				conn.SetWriteDeadline(time.Now().Add(c.config.WriteTimeout))
				if err := conn.WriteMessage(websocket.PingMessage, nil); err != nil {
					select {
					case c.errCh <- fmt.Errorf("ping error: %w", err):
					default:
					}
				}
			}

		case <-c.stopCh:
			return
		}
	}
}

// handleMessage processes received messages
func (c *WebSocketClient) handleMessage(msg StreamMessage) {
	switch msg.Type {
	case "detection_result":
		var result DetectionResponseMessage
		if err := json.Unmarshal(msg.Payload, &result); err == nil {
			if c.onDetectionResult != nil {
				go c.onDetectionResult(result)
			}
		}
	}

	if c.onMessage != nil {
		go c.onMessage(msg)
	}

	select {
	case c.receiveCh <- msg:
	default:
		// Channel full, drop message
	}
}

// handleDisconnect handles disconnection and attempts reconnection
func (c *WebSocketClient) handleDisconnect() {
	c.mu.Lock()
	wasConnected := c.connected
	c.connected = false
	c.mu.Unlock()

	if wasConnected && c.onDisconnect != nil {
		go c.onDisconnect()
	}

	// Attempt reconnection
	if c.config.MaxReconnectAttempts > 0 {
		go c.reconnect()
	}
}

// reconnect attempts to reconnect to the WebSocket server
func (c *WebSocketClient) reconnect() {
	for c.reconnectCnt < c.config.MaxReconnectAttempts {
		select {
		case <-c.stopCh:
			return
		case <-time.After(c.config.ReconnectInterval):
		}

		ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
		err := c.Connect(ctx)
		cancel()

		if err == nil {
			return
		}

		c.reconnectCnt++
	}

	select {
	case c.errCh <- fmt.Errorf("max reconnection attempts reached"):
	default:
	}
}

// SetOnConnect sets the connect callback
func (c *WebSocketClient) SetOnConnect(callback func()) {
	c.onConnect = callback
}

// SetOnDisconnect sets the disconnect callback
func (c *WebSocketClient) SetOnDisconnect(callback func()) {
	c.onDisconnect = callback
}

// SetOnMessage sets the message callback
func (c *WebSocketClient) SetOnMessage(callback func(StreamMessage)) {
	c.onMessage = callback
}

// SetOnDetectionResult sets the detection result callback
func (c *WebSocketClient) SetOnDetectionResult(callback func(DetectionResponseMessage)) {
	c.onDetectionResult = callback
}

// Errors returns the error channel
func (c *WebSocketClient) Errors() <-chan error {
	return c.errCh
}

// Receive returns the receive channel
func (c *WebSocketClient) Receive() <-chan StreamMessage {
	return c.receiveCh
}

// =============================================================================
// Streaming Detector
// =============================================================================

// StreamingDetector provides high-level streaming detection using WebSocket
type StreamingDetector struct {
	wsClient    *WebSocketClient
	config      WebSocketConfig

	// Request tracking
	pendingReqs map[string]chan DetectionResponseMessage
	reqMu       sync.RWMutex
}

// NewStreamingDetector creates a new streaming detector
func NewStreamingDetector(config WebSocketConfig) (*StreamingDetector, error) {
	wsClient, err := NewWebSocketClient(config)
	if err != nil {
		return nil, err
	}

	d := &StreamingDetector{
		wsClient:    wsClient,
		config:      config,
		pendingReqs: make(map[string]chan DetectionResponseMessage),
	}

	// Set up result handler
	wsClient.SetOnDetectionResult(d.handleDetectionResult)

	return d, nil
}

// Connect establishes the WebSocket connection
func (d *StreamingDetector) Connect(ctx context.Context) error {
	return d.wsClient.Connect(ctx)
}

// Close closes the streaming detector
func (d *StreamingDetector) Close() error {
	return d.wsClient.Close()
}

// IsConnected returns whether the detector is connected
func (d *StreamingDetector) IsConnected() bool {
	return d.wsClient.IsConnected()
}

// Detect sends a detection request and waits for the result
func (d *StreamingDetector) Detect(ctx context.Context, detectionType DetectionType, data DetectionData, saveAnomaly bool) (*DetectionResult, error) {
	requestID := generateRequestID()

	// Create response channel
	respCh := make(chan DetectionResponseMessage, 1)

	d.reqMu.Lock()
	d.pendingReqs[requestID] = respCh
	d.reqMu.Unlock()

	defer func() {
		d.reqMu.Lock()
		delete(d.pendingReqs, requestID)
		d.reqMu.Unlock()
	}()

	// Send detection request
	msg := DetectionMessage{
		Type:        detectionType,
		Data:        data,
		SaveAnomaly: saveAnomaly,
		RequestID:   requestID,
	}

	if err := d.wsClient.SendDetection(msg); err != nil {
		return nil, err
	}

	// Wait for response
	select {
	case resp := <-respCh:
		if resp.Error != "" {
			return nil, fmt.Errorf("detection error: %s", resp.Error)
		}
		return &resp.Result, nil
	case <-ctx.Done():
		return nil, ctx.Err()
	}
}

// DetectAsync sends a detection request asynchronously
func (d *StreamingDetector) DetectAsync(detectionType DetectionType, data DetectionData, saveAnomaly bool) error {
	requestID := generateRequestID()

	msg := DetectionMessage{
		Type:        detectionType,
		Data:        data,
		SaveAnomaly: saveAnomaly,
		RequestID:   requestID,
	}

	return d.wsClient.SendDetection(msg)
}

// handleDetectionResult handles incoming detection results
func (d *StreamingDetector) handleDetectionResult(result DetectionResponseMessage) {
	d.reqMu.RLock()
	ch, exists := d.pendingReqs[result.RequestID]
	d.reqMu.RUnlock()

	if exists {
		select {
		case ch <- result:
		default:
		}
	}
}

// generateRequestID generates a unique request ID
func generateRequestID() string {
	return fmt.Sprintf("req-%d", time.Now().UnixNano())
}

// Errors returns the error channel
func (d *StreamingDetector) Errors() <-chan error {
	return d.wsClient.Errors()
}

// SetOnConnect sets the connect callback
func (d *StreamingDetector) SetOnConnect(callback func()) {
	d.wsClient.SetOnConnect(callback)
}

// SetOnDisconnect sets the disconnect callback
func (d *StreamingDetector) SetOnDisconnect(callback func()) {
	d.wsClient.SetOnDisconnect(callback)
}
