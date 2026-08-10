package mq

import (
	"context"
	"crypto/tls"
	"crypto/x509"
	"encoding/json"
	"fmt"
	"os"
	"sync"
	"time"

	mqtt "github.com/eclipse/paho.mqtt.golang"
)

// MQTTConfig holds MQTT broker configuration for the TCU vehicle channel.
// Kept dependency-free (mirrors config.MQTTConfig) so the mq package stays
// self-contained, consistent with KafkaConfig.
type MQTTConfig struct {
	Broker         string        // Broker address, e.g. tcp://emqx:1883 or ssl://emqx:8883
	ClientID       string        // Client identifier
	Username       string        // Username for authentication
	Password       string        // Password for authentication
	QoS            byte          // Default QoS level (0/1/2)
	KeepAlive      time.Duration // Keep-alive interval
	ConnectTimeout time.Duration // Connection timeout
	AutoReconnect  bool          // Automatically reconnect
	MaxReconnect   int           // Maximum reconnection attempts (0 = unlimited)
	ReconnectDelay time.Duration // Delay between reconnection attempts
	TLSEnabled     bool          // Enable TLS
	TLSCACert      string        // Path to CA certificate
	TLSClientCert  string        // Path to client certificate
	TLSClientKey   string        // Path to client private key
	Topic          string        // Default topic for producer publishes / subscriber subscription
	Retained       bool          // Whether published messages are retained
}

// MQTTProducer wraps an MQTT client for publishing messages.
type MQTTProducer struct {
	client   mqtt.Client
	topic    string
	qos      byte
	retained bool
	retries  int
	mu       sync.Mutex
}

// NewMQTTProducer creates and connects an MQTT producer.
func NewMQTTProducer(config MQTTConfig) (*MQTTProducer, error) {
	client, err := newMQTTClient(config)
	if err != nil {
		return nil, fmt.Errorf("failed to create MQTT producer: %w", err)
	}
	return &MQTTProducer{
		client:   client,
		topic:    config.Topic,
		qos:      config.QoS,
		retained: config.Retained,
		retries:  3,
	}, nil
}

// Publish publishes a raw payload to the configured topic.
func (p *MQTTProducer) Publish(ctx context.Context, topic string, payload []byte) error {
	if topic == "" {
		topic = p.topic
	}
	if topic == "" {
		return fmt.Errorf("mqtt publish: empty topic")
	}

	token := p.client.Publish(topic, p.qos, p.retained, payload)
	// WaitTimeout covers both connect-in-flight and publish completion.
	if !token.WaitTimeout(30 * time.Second) {
		return fmt.Errorf("mqtt publish timeout on topic %s", topic)
	}
	if err := token.Error(); err != nil {
		return fmt.Errorf("mqtt publish to %s: %w", topic, err)
	}
	return nil
}

// PublishMessage publishes a structured Message (same JSON envelope as Kafka,
// so downstream consumers can be broker-agnostic).
func (p *MQTTProducer) PublishMessage(ctx context.Context, key string, msgType string, data map[string]interface{}) error {
	msg := Message{
		Key:       key,
		Type:      msgType,
		Timestamp: time.Now().UnixMilli(),
		Data:      data,
	}
	payload, err := json.Marshal(msg)
	if err != nil {
		return fmt.Errorf("mqtt marshal message: %w", err)
	}
	return p.Publish(ctx, p.topic, payload)
}

// IsConnected reports whether the underlying MQTT connection is up.
func (p *MQTTProducer) IsConnected() bool {
	return p.client != nil && p.client.IsConnected()
}

// Close disconnects the underlying client.
func (p *MQTTProducer) Close() {
	if p.client != nil && p.client.IsConnected() {
		p.client.Disconnect(250)
	}
}

// MQTTSubscriber subscribes to a topic and dispatches decoded messages to a
// MessageHandler (same interface as the Kafka consumer).
type MQTTSubscriber struct {
	client  mqtt.Client
	topic   string
	qos     byte
	handler MessageHandler
}

// NewMQTTSubscriber creates an MQTT client and subscribes to the configured topic.
func NewMQTTSubscriber(config MQTTConfig, handler MessageHandler) (*MQTTSubscriber, error) {
	if handler == nil {
		return nil, fmt.Errorf("mqtt subscriber: nil handler")
	}
	client, err := newMQTTClient(config)
	if err != nil {
		return nil, fmt.Errorf("failed to create MQTT subscriber: %w", err)
	}

	topic := config.Topic
	if topic == "" {
		return nil, fmt.Errorf("mqtt subscriber: empty topic")
	}

	sub := &MQTTSubscriber{
		client:  client,
		topic:   topic,
		qos:     config.QoS,
		handler: handler,
	}

	// The onMessage handler decodes the JSON envelope and forwards it.
	client.AddRoute(topic, func(_ mqtt.Client, msg mqtt.Message) {
		var message Message
		if err := json.Unmarshal(msg.Payload(), &message); err != nil {
			// Non-envelope payload: deliver raw under Data["payload"].
			message = Message{
				Key:       msg.Topic(),
				Type:      "raw",
				Timestamp: time.Now().UnixMilli(),
				Data:      map[string]interface{}{"payload": string(msg.Payload())},
			}
		}
		_ = sub.handler.HandleMessage(context.Background(), &message)
	})

	token := client.Subscribe(topic, config.QoS, nil)
	if !token.WaitTimeout(config.ConnectTimeout) {
		return nil, fmt.Errorf("mqtt subscribe timeout on topic %s", topic)
	}
	if err := token.Error(); err != nil {
		return nil, fmt.Errorf("mqtt subscribe to %s: %w", topic, err)
	}
	return sub, nil
}

// IsConnected reports whether the underlying MQTT connection is up.
func (s *MQTTSubscriber) IsConnected() bool {
	return s.client != nil && s.client.IsConnected()
}

// Close unsubscribes and disconnects.
func (s *MQTTSubscriber) Close() {
	if s.client == nil || !s.client.IsConnected() {
		return
	}
	s.client.Unsubscribe(s.topic)
	s.client.Disconnect(250)
}

// newMQTTClient builds a connected paho client from MQTTConfig.
func newMQTTClient(config MQTTConfig) (mqtt.Client, error) {
	if config.Broker == "" {
		return nil, fmt.Errorf("mqtt broker address is required")
	}
	if config.ClientID == "" {
		config.ClientID = fmt.Sprintf("dkcs-%d", time.Now().UnixNano())
	}
	if config.ConnectTimeout <= 0 {
		config.ConnectTimeout = 30 * time.Second
	}
	if config.KeepAlive <= 0 {
		config.KeepAlive = 60 * time.Second
	}
	if config.QoS > 2 {
		return nil, fmt.Errorf("mqtt invalid QoS %d (must be 0-2)", config.QoS)
	}

	opts := mqtt.NewClientOptions().
		AddBroker(config.Broker).
		SetClientID(config.ClientID).
		SetUsername(config.Username).
		SetPassword(config.Password).
		SetKeepAlive(config.KeepAlive).
		SetConnectTimeout(config.ConnectTimeout).
		SetAutoReconnect(config.AutoReconnect).
		SetMaxReconnectInterval(config.ReconnectDelay).
		SetCleanSession(true)

	if config.AutoReconnect && config.MaxReconnect > 0 {
		opts.SetConnectRetry(true).SetConnectRetryInterval(config.ReconnectDelay)
	}

	if config.TLSEnabled {
		tlsConfig, err := buildTLSConfig(config)
		if err != nil {
			return nil, err
		}
		opts.SetTLSConfig(tlsConfig)
	}

	client := mqtt.NewClient(opts)
	token := client.Connect()
	if !token.WaitTimeout(config.ConnectTimeout) {
		client.Disconnect(250)
		return nil, fmt.Errorf("mqtt connect timeout to %s", config.Broker)
	}
	if err := token.Error(); err != nil {
		return nil, fmt.Errorf("mqtt connect to %s: %w", config.Broker, err)
	}
	return client, nil
}

// buildTLSConfig loads CA / client certs for MQTT over TLS.
func buildTLSConfig(config MQTTConfig) (*tls.Config, error) {
	tlsConfig := &tls.Config{
		MinVersion: tls.VersionTLS12,
	}

	if config.TLSCACert != "" {
		caCert, err := os.ReadFile(config.TLSCACert)
		if err != nil {
			return nil, fmt.Errorf("mqtt read CA cert %s: %w", config.TLSCACert, err)
		}
		pool := x509.NewCertPool()
		if !pool.AppendCertsFromPEM(caCert) {
			return nil, fmt.Errorf("mqtt invalid CA cert %s", config.TLSCACert)
		}
		tlsConfig.RootCAs = pool
	}

	if config.TLSClientCert != "" && config.TLSClientKey != "" {
		cert, err := tls.LoadX509KeyPair(config.TLSClientCert, config.TLSClientKey)
		if err != nil {
			return nil, fmt.Errorf("mqtt load client cert: %w", err)
		}
		tlsConfig.Certificates = []tls.Certificate{cert}
	}

	return tlsConfig, nil
}

// MQTT topic conventions for the TCU vehicle channel (EMQX broker).
const (
	MQTTTopicCommands  = "dkcs/tcu/commands"  // cloud → TCU command downlink
	MQTTTopicTelemetry = "dkcs/tcu/telemetry" // TCU → cloud telemetry uplink
	MQTTTopicEvents    = "dkcs/tcu/events"    // TCU → cloud event uplink
)
