package mq

import (
	"context"
	"encoding/json"
	"fmt"
	"time"

	"github.com/IBM/sarama"
)

// KafkaProducer wraps Kafka producer
type KafkaProducer struct {
	producer sarama.SyncProducer
	topic    string
}

// KafkaConfig holds Kafka configuration
type KafkaConfig struct {
	Brokers []string
	Topic   string
}

// NewKafkaProducer creates a new Kafka producer
func NewKafkaProducer(config KafkaConfig) (*KafkaProducer, error) {
	cfg := sarama.NewConfig()
	cfg.Producer.RequiredAcks = sarama.WaitForAll
	cfg.Producer.Retry.Max = 5
	cfg.Producer.Return.Successes = true

	producer, err := sarama.NewSyncProducer(config.Brokers, cfg)
	if err != nil {
		return nil, fmt.Errorf("failed to create Kafka producer: %w", err)
	}

	return &KafkaProducer{
		producer: producer,
		topic:    config.Topic,
	}, nil
}

// Message represents a Kafka message
type Message struct {
	Key       string                 `json:"key"`
	Type      string                 `json:"type"`
	Timestamp int64                  `json:"timestamp"`
	Data      map[string]interface{} `json:"data"`
}

// Publish publishes a message to Kafka
func (p *KafkaProducer) Publish(ctx context.Context, key string, msgType string, data map[string]interface{}) error {
	msg := Message{
		Key:       key,
		Type:      msgType,
		Timestamp: time.Now().UnixMilli(),
		Data:      data,
	}

	msgBytes, err := json.Marshal(msg)
	if err != nil {
		return fmt.Errorf("failed to marshal message: %w", err)
	}

	kafkaMsg := &sarama.ProducerMessage{
		Topic: p.topic,
		Key:   sarama.StringEncoder(key),
		Value: sarama.ByteEncoder(msgBytes),
	}

	_, _, err = p.producer.SendMessage(kafkaMsg)
	if err != nil {
		return fmt.Errorf("failed to send message: %w", err)
	}

	return nil
}

// PublishWithPartition publishes to specific partition
func (p *KafkaProducer) PublishWithPartition(ctx context.Context, key string, msgType string, data map[string]interface{}, partition int32) error {
	msg := Message{
		Key:       key,
		Type:      msgType,
		Timestamp: time.Now().UnixMilli(),
		Data:      data,
	}

	msgBytes, err := json.Marshal(msg)
	if err != nil {
		return fmt.Errorf("failed to marshal message: %w", err)
	}

	kafkaMsg := &sarama.ProducerMessage{
		Topic:     p.topic,
		Key:       sarama.StringEncoder(key),
		Value:     sarama.ByteEncoder(msgBytes),
		Partition: partition,
	}

	_, _, err = p.producer.SendMessage(kafkaMsg)
	if err != nil {
		return fmt.Errorf("failed to send message: %w", err)
	}

	return nil
}

// Close closes the producer
func (p *KafkaProducer) Close() error {
	return p.producer.Close()
}

// KafkaConsumer wraps Kafka consumer
type KafkaConsumer struct {
	consumer sarama.ConsumerGroup
	topic    string
	handler  MessageHandler
}

// MessageHandler handles consumed messages
type MessageHandler interface {
	HandleMessage(ctx context.Context, msg *Message) error
}

// MessageHandlerFunc is an adapter for functions
type MessageHandlerFunc func(ctx context.Context, msg *Message) error

func (f MessageHandlerFunc) HandleMessage(ctx context.Context, msg *Message) error {
	return f(ctx, msg)
}

// NewKafkaConsumer creates a new Kafka consumer
func NewKafkaConsumer(config KafkaConfig, groupID string, handler MessageHandler) (*KafkaConsumer, error) {
	cfg := sarama.NewConfig()
	cfg.Consumer.Return.Errors = true
	cfg.Consumer.Group.Rebalance.Strategy = sarama.BalanceStrategyRoundRobin
	cfg.Consumer.Offsets.Initial = sarama.OffsetNewest

	consumer, err := sarama.NewConsumerGroup(config.Brorokers, groupID, cfg)
	if err != nil {
		return nil, fmt.Errorf("failed to create consumer group: %w", err)
	}

	return &KafkaConsumer{
		consumer: consumer,
		topic:    config.Topic,
		handler:  handler,
	}, nil
}

// consumerGroupHandler implements sarama.ConsumerGroupHandler
type consumerGroupHandler struct {
	handler MessageHandler
}

func (h *consumerGroupHandler) Setup(sarama.ConsumerGroupSession) error   { return nil }
func (h *consumerGroupHandler) Cleanup(sarama.ConsumerGroupSession) error { return nil }

func (h *consumerGroupHandler) ConsumeClaim(session sarama.ConsumerGroupSession, claim sarama.ConsumerGroupClaim) error {
	for msg := range claim.Messages() {
		var message Message
		if err := json.Unmarshal(msg.Value, &message); err != nil {
			// Log error and continue
			continue
		}

		if err := h.handler.HandleMessage(session.Context(), &message); err != nil {
			// Log error
			continue
		}

		session.MarkMessage(msg, "")
	}
	return nil
}

// Start starts consuming messages
func (c *KafkaConsumer) Start(ctx context.Context) error {
	handler := &consumerGroupHandler{handler: c.handler}
	for {
		select {
		case <-ctx.Done():
			return ctx.Err()
		default:
			if err := c.consumer.Consume(ctx, []string{c.topic}, handler); err != nil {
				return fmt.Errorf("consume error: %w", err)
			}
		}
	}
}

// Close closes the consumer
func (c *KafkaConsumer) Close() error {
	return c.consumer.Close()
}

// Message types for digital key events
const (
	MsgTypeKeyCreated       = "key_created"
	MsgTypeKeyActivated     = "key_activated"
	MsgTypeKeyRevoked       = "key_revoked"
	MsgTypeKeyShared        = "key_shared"
	MsgTypeKeyExpired       = "key_expired"
	MsgTypeCommandSent      = "command_sent"
	MsgTypeCommandCompleted = "command_completed"
	MsgTypeCommandFailed    = "command_failed"
	MsgTypeVehicleOnline    = "vehicle_online"
	MsgTypeVehicleOffline   = "vehicle_offline"
	MsgTypeVehicleTelemetry = "vehicle_telemetry"
)

// Topics
const (
	TopicKeyEvents     = "dkcs.key.events"
	TopicCommands      = "dkcs.commands"
	TopicVehicleEvents = "dkcs.vehicle.events"
	TopicTelemetry     = "dkcs.telemetry"
)
