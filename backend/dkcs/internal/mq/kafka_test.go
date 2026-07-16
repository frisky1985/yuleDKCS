package mq

import (
	"context"
	"encoding/json"
	"sync"
	"testing"
	"time"

	"github.com/IBM/sarama"
)

// mockSyncProducer implements sarama.SyncProducer for testing
type mockSyncProducer struct {
	mu       sync.Mutex
	messages []*sarama.ProducerMessage
	sendErr  error
}

func (m *mockSyncProducer) SendMessage(msg *sarama.ProducerMessage) (partition int32, offset int64, err error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	if m.sendErr != nil {
		return 0, 0, m.sendErr
	}
	m.messages = append(m.messages, msg)
	return 0, 1, nil
}

func (m *mockSyncProducer) SendMessages(msgs []*sarama.ProducerMessage) error {
	for _, msg := range msgs {
		if _, _, err := m.SendMessage(msg); err != nil {
			return err
		}
	}
	return nil
}

func (m *mockSyncProducer) Close() error {
	return nil
}

func (m *mockSyncProducer) AbortTxn() error {
	return nil
}

func (m *mockSyncProducer) BeginTxn() error {
	return nil
}

func (m *mockSyncProducer) AddMessageToTxn(msg *sarama.ConsumerMessage, groupID string, metadata *string) error {
	return nil
}

func (m *mockSyncProducer) AddOffsetsToTxn(offsets map[string][]*sarama.PartitionOffsetMetadata, groupID string) error {
	return nil
}

func (m *mockSyncProducer) AddOffsetsToTxnWithGroupMetadata(offsets map[string][]*sarama.PartitionOffsetMetadata, groupMetadata *sarama.ConsumerGroupMetadata) error {
	return nil
}

func (m *mockSyncProducer) IsTransactional() bool {
	return false
}

func (m *mockSyncProducer) TxnStatus() sarama.ProducerTxnStatusFlag {
	return 0
}

func (m *mockSyncProducer) CommitTxn() error {
	return nil
}

func (m *mockSyncProducer) RollbackTxn() error {
	return nil
}

func (m *mockSyncProducer) WithTxnID(txnID string) sarama.SyncProducer {
	return m
}

func (m *mockSyncProducer) AddPartitionToTxn(topic string, partition int32) error {
	return nil
}

func (m *mockSyncProducer) AddMessageToTxnWithGroupMetadata(msg *sarama.ConsumerMessage, groupMetadata *sarama.ConsumerGroupMetadata, metadata *string) error {
	return nil
}

// mockConsumerGroup implements sarama.ConsumerGroup for testing
type mockConsumerGroup struct {
	mu       sync.Mutex
	consumed bool
	closed   bool
	claimCh  chan *sarama.ConsumerMessage
}

func newMockConsumerGroup() *mockConsumerGroup {
	return &mockConsumerGroup{
		claimCh: make(chan *sarama.ConsumerMessage, 10),
	}
}

func (m *mockConsumerGroup) Consume(ctx context.Context, topics []string, handler sarama.ConsumerGroupHandler) error {
	m.mu.Lock()
	m.consumed = true
	m.mu.Unlock()

	// Simulate the consumption loop
	session := &mockConsumerGroupSession{ctx: ctx}
	for {
		select {
		case <-ctx.Done():
			return ctx.Err()
		case msg, ok := <-m.claimCh:
			if !ok {
				return nil
			}
			if err := handler.ConsumeClaim(session, &mockConsumerGroupClaim{msg: msg}); err != nil {
				return err
			}
		}
	}
}

func (m *mockConsumerGroup) Close() error {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.closed = true
	return nil
}

func (m *mockConsumerGroup) Errors() <-chan error {
	errCh := make(chan error)
	close(errCh)
	return errCh
}

func (m *mockConsumerGroup) Pause(partitions map[string][]int32)  {}
func (m *mockConsumerGroup) Resume(partitions map[string][]int32) {}
func (m *mockConsumerGroup) PauseAll()                             {}
func (m *mockConsumerGroup) ResumeAll()                            {}

type mockConsumerGroupSession struct {
	ctx context.Context
}

func (s *mockConsumerGroupSession) Commit()                                   {}
func (s *mockConsumerGroupSession) Context() context.Context                   { return s.ctx }
func (s *mockConsumerGroupSession) MarkMessage(msg *sarama.ConsumerMessage, s2 string) {}
func (s *mockConsumerGroupSession) Claims() map[string][]int32                 { return nil }
func (s *mockConsumerGroupSession) MemberID() string                           { return "" }
func (s *mockConsumerGroupSession) GenerationID() int32                        { return 0 }
func (s *mockConsumerGroupSession) MarkOffset(topic string, partition int32, offset int64, metadata string) {}
func (s *mockConsumerGroupSession) ResetOffset(topic string, partition int32, offset int64, metadata string) {}

type mockConsumerGroupClaim struct {
	msg *sarama.ConsumerMessage
}

func (c *mockConsumerGroupClaim) Topic() string              { return "test-topic" }
func (c *mockConsumerGroupClaim) Partition() int32            { return 0 }
func (c *mockConsumerGroupClaim) InitialOffset() int64        { return 0 }
func (c *mockConsumerGroupClaim) HighWaterMarkOffset() int64  { return 0 }
func (c *mockConsumerGroupClaim) Messages() <-chan *sarama.ConsumerMessage {
	ch := make(chan *sarama.ConsumerMessage, 1)
	ch <- c.msg
	close(ch)
	return ch
}

// --- KafkaProducer tests ---

func newTestProducer(t *testing.T) (*KafkaProducer, *mockSyncProducer) {
	t.Helper()
	mock := &mockSyncProducer{}
	producer := &KafkaProducer{
		producer: mock,
		topic:    "test-topic",
	}
	return producer, mock
}

func TestPublish(t *testing.T) {
	producer, mock := newTestProducer(t)

	ctx := context.Background()
	data := map[string]interface{}{
		"vehicle_id": "VH-001",
		"action":     "unlock",
	}

	err := producer.Publish(ctx, "key-1", MsgTypeCommandSent, data)
	if err != nil {
		t.Fatalf("Publish failed: %v", err)
	}

	mock.mu.Lock()
	defer mock.mu.Unlock()
	if len(mock.messages) != 1 {
		t.Fatalf("expected 1 message, got %d", len(mock.messages))
	}

	msg := mock.messages[0]
	if msg.Topic != "test-topic" {
		t.Errorf("expected topic 'test-topic', got '%s'", msg.Topic)
	}

	// Verify message content
	var decoded Message
	val, _ := msg.Value.Encode()
	if err := json.Unmarshal(val, &decoded); err != nil {
		t.Fatalf("failed to unmarshal message: %v", err)
	}
	if decoded.Key != "key-1" {
		t.Errorf("expected key 'key-1', got '%s'", decoded.Key)
	}
	if decoded.Type != MsgTypeCommandSent {
		t.Errorf("expected type '%s', got '%s'", MsgTypeCommandSent, decoded.Type)
	}
	if decoded.Timestamp <= 0 {
		t.Error("expected positive timestamp")
	}
	if decoded.Data["vehicle_id"] != "VH-001" {
		t.Errorf("expected data.vehicle_id='VH-001', got %v", decoded.Data["vehicle_id"])
	}
}

func TestPublishWithPartition(t *testing.T) {
	producer, mock := newTestProducer(t)

	ctx := context.Background()
	data := map[string]interface{}{"status": "online"}

	err := producer.PublishWithPartition(ctx, "vehicle-1", MsgTypeVehicleOnline, data, 3)
	if err != nil {
		t.Fatalf("PublishWithPartition failed: %v", err)
	}

	mock.mu.Lock()
	defer mock.mu.Unlock()
	if len(mock.messages) != 1 {
		t.Fatalf("expected 1 message, got %d", len(mock.messages))
	}

	msg := mock.messages[0]
	if msg.Partition != 3 {
		t.Errorf("expected partition 3, got %d", msg.Partition)
	}

	// Verify message content
	var decoded Message
	val, _ := msg.Value.Encode()
	if err := json.Unmarshal(val, &decoded); err != nil {
		t.Fatalf("failed to unmarshal message: %v", err)
	}
	if decoded.Type != MsgTypeVehicleOnline {
		t.Errorf("expected type '%s', got '%s'", MsgTypeVehicleOnline, decoded.Type)
	}
}

func TestPublish_WithError(t *testing.T) {
	producer, mock := newTestProducer(t)
	mock.sendErr = sarama.ErrRequestTimedOut

	ctx := context.Background()
	err := producer.Publish(ctx, "key-err", MsgTypeKeyCreated, map[string]interface{}{})
	if err == nil {
		t.Fatal("expected error from producer")
	}
}

func TestClose(t *testing.T) {
	producer, mock := newTestProducer(t)

	err := producer.Close()
	if err != nil {
		t.Fatalf("Close failed: %v", err)
	}
	_ = mock
}

// --- KafkaConsumer tests ---

type testMessageHandler struct {
	mu       sync.Mutex
	messages []*Message
	handleFn func(ctx context.Context, msg *Message) error
}

func newTestHandler() *testMessageHandler {
	return &testMessageHandler{
		handleFn: func(ctx context.Context, msg *Message) error { return nil },
	}
}

func (h *testMessageHandler) HandleMessage(ctx context.Context, msg *Message) error {
	h.mu.Lock()
	defer h.mu.Unlock()
	h.messages = append(h.messages, msg)
	return h.handleFn(ctx, msg)
}

func (h *testMessageHandler) count() int {
	h.mu.Lock()
	defer h.mu.Unlock()
	return len(h.messages)
}

func newTestConsumer(t *testing.T, handler MessageHandler) (*KafkaConsumer, *mockConsumerGroup) {
	t.Helper()
	mock := newMockConsumerGroup()
	consumer := &KafkaConsumer{
		consumer: mock,
		topic:    "test-topic",
		handler:  handler,
	}
	return consumer, mock
}

func TestMessageHandlerFunc(t *testing.T) {
	called := false
	var fn MessageHandlerFunc = func(ctx context.Context, msg *Message) error {
		called = true
		return nil
	}
	err := fn.HandleMessage(context.Background(), &Message{Key: "test"})
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !called {
		t.Error("handler was not called")
	}
}

func TestConsumerGroupHandler_ConsumeClaim(t *testing.T) {
	handler := newTestHandler()
	cgh := &consumerGroupHandler{handler: handler}

	msg := &sarama.ConsumerMessage{
		Value: []byte(`{"key":"k1","type":"key_created","timestamp":123456789,"data":{"id":"1"}}`),
	}
	session := &mockConsumerGroupSession{ctx: context.Background()}
	claim := &mockConsumerGroupClaim{msg: msg}

	err := cgh.ConsumeClaim(session, claim)
	if err != nil {
		t.Fatalf("ConsumeClaim failed: %v", err)
	}

	if handler.count() != 1 {
		t.Fatalf("expected 1 handled message, got %d", handler.count())
	}
}

func TestConsumerGroupHandler_InvalidJSON(t *testing.T) {
	handler := newTestHandler()
	cgh := &consumerGroupHandler{handler: handler}

	msg := &sarama.ConsumerMessage{
		Value: []byte(`invalid json`),
	}
	session := &mockConsumerGroupSession{ctx: context.Background()}
	claim := &mockConsumerGroupClaim{msg: msg}

	err := cgh.ConsumeClaim(session, claim)
	if err != nil {
		t.Fatalf("expected no error (skipped bad message), got: %v", err)
	}

	if handler.count() != 0 {
		t.Errorf("expected 0 handled messages for invalid JSON, got %d", handler.count())
	}
}

func TestConsumerGroupHandler_HandlerError(t *testing.T) {
	handler := newTestHandler()
	handler.handleFn = func(ctx context.Context, msg *Message) error {
		return context.Canceled
	}
	cgh := &consumerGroupHandler{handler: handler}

	msg := &sarama.ConsumerMessage{
		Value: []byte(`{"key":"k1","type":"test"}`),
	}
	session := &mockConsumerGroupSession{ctx: context.Background()}
	claim := &mockConsumerGroupClaim{msg: msg}

	err := cgh.ConsumeClaim(session, claim)
	if err != nil {
		t.Fatalf("expected no error (skipped on handler error), got: %v", err)
	}
}

func TestConsumerStart(t *testing.T) {
	handler := newTestHandler()
	consumer, mock := newTestConsumer(t, handler)
	defer consumer.Close()

	ctx, cancel := context.WithTimeout(context.Background(), 50*time.Millisecond)
	defer cancel()

	// Feed a message before starting
	mock.claimCh <- &sarama.ConsumerMessage{
		Value: []byte(`{"key":"k1","type":"key_activated","timestamp":123,"data":{}}`),
	}

	// The inline claim channels will process the message, then Start loops and
	// eventually returns context deadline exceeded when the timeout fires.
	err := consumer.Start(ctx)
	if err == nil {
		t.Fatal("expected error from context timeout, got nil")
	}

	if handler.count() != 1 {
		t.Errorf("expected 1 handled message, got %d", handler.count())
	}
}

func TestConsumerClose(t *testing.T) {
	handler := newTestHandler()
	consumer, _ := newTestConsumer(t, handler)

	err := consumer.Close()
	if err != nil {
		t.Fatalf("Close failed: %v", err)
	}
}

func TestMessageConstants(t *testing.T) {
	tests := []struct {
		constant string
		expected string
	}{
		{MsgTypeKeyCreated, "key_created"},
		{MsgTypeKeyActivated, "key_activated"},
		{MsgTypeKeyRevoked, "key_revoked"},
		{MsgTypeKeyShared, "key_shared"},
		{MsgTypeKeyExpired, "key_expired"},
		{MsgTypeCommandSent, "command_sent"},
		{MsgTypeCommandCompleted, "command_completed"},
		{MsgTypeCommandFailed, "command_failed"},
		{MsgTypeVehicleOnline, "vehicle_online"},
		{MsgTypeVehicleOffline, "vehicle_offline"},
		{MsgTypeVehicleTelemetry, "vehicle_telemetry"},
	}
	for _, tt := range tests {
		if tt.constant != tt.expected {
			t.Errorf("expected '%s', got '%s'", tt.expected, tt.constant)
		}
	}
}

func TestTopicConstants(t *testing.T) {
	tests := []struct {
		constant string
		expected string
	}{
		{TopicKeyEvents, "dkcs.key.events"},
		{TopicCommands, "dkcs.commands"},
		{TopicVehicleEvents, "dkcs.vehicle.events"},
		{TopicTelemetry, "dkcs.telemetry"},
	}
	for _, tt := range tests {
		if tt.constant != tt.expected {
			t.Errorf("expected '%s', got '%s'", tt.expected, tt.constant)
		}
	}
}

// --- Additional coverage for Setup/Cleanup ---

func TestConsumerGroupHandler_Setup(t *testing.T) {
	handler := newTestHandler()
	cgh := &consumerGroupHandler{handler: handler}
	// Setup should return nil without side effects
	err := cgh.Setup(nil)
	if err != nil {
		t.Fatalf("Setup returned unexpected error: %v", err)
	}
}

func TestConsumerGroupHandler_Cleanup(t *testing.T) {
	handler := newTestHandler()
	cgh := &consumerGroupHandler{handler: handler}
	// Cleanup should return nil without side effects
	err := cgh.Cleanup(nil)
	if err != nil {
		t.Fatalf("Cleanup returned unexpected error: %v", err)
	}
}

func TestConsumerGroupHandler_SetupAndCleanup_NilHandler(t *testing.T) {
	// Verify Setup/Cleanup are safe even with nil session
	cgh := &consumerGroupHandler{handler: nil}
	if err := cgh.Setup(nil); err != nil {
		t.Errorf("Setup failed: %v", err)
	}
	if err := cgh.Cleanup(nil); err != nil {
		t.Errorf("Cleanup failed: %v", err)
	}
}

// --- NewKafkaProducer error paths ---

func TestNewKafkaProducer_Error(t *testing.T) {
	// Empty brokers should cause connection error
	config := KafkaConfig{
		Brokers: []string{"invalid:1"},
		Topic:   "test",
	}
	_, err := NewKafkaProducer(config)
	if err == nil {
		t.Fatal("expected error with invalid broker, got nil")
	}
}

func TestNewKafkaProducer_EmptyBrokers(t *testing.T) {
	config := KafkaConfig{
		Brokers: []string{},
		Topic:   "test",
	}
	_, err := NewKafkaProducer(config)
	if err == nil {
		t.Fatal("expected error with empty brokers, got nil")
	}
}

// --- NewKafkaConsumer error paths ---

func TestNewKafkaConsumer_Error(t *testing.T) {
	// Empty brokers should cause connection error
	config := KafkaConfig{
		Brokers: []string{"invalid:1"},
		Topic:   "test",
	}
	_, err := NewKafkaConsumer(config, "test-group", newTestHandler())
	if err == nil {
		t.Fatal("expected error with invalid broker, got nil")
	}
}

func TestNewKafkaConsumer_EmptyBrokers(t *testing.T) {
	config := KafkaConfig{
		Brokers: []string{},
		Topic:   "test",
	}
	_, err := NewKafkaConsumer(config, "test-group", newTestHandler())
	if err == nil {
		t.Fatal("expected error with empty brokers, got nil")
	}
}

// --- PublishWithPartition error path ---

func TestPublishWithPartition_WithError(t *testing.T) {
	producer, mock := newTestProducer(t)
	mock.sendErr = sarama.ErrRequestTimedOut

	ctx := context.Background()
	err := producer.PublishWithPartition(ctx, "key-err", MsgTypeKeyCreated, map[string]interface{}{"id": "1"}, 0)
	if err == nil {
		t.Fatal("expected error from producer")
	}
}

func TestPublishWithPartition_ZeroPartition(t *testing.T) {
	producer, mock := newTestProducer(t)

	ctx := context.Background()
	data := map[string]interface{}{"status": "offline"}

	err := producer.PublishWithPartition(ctx, "vehicle-2", MsgTypeVehicleOffline, data, 0)
	if err != nil {
		t.Fatalf("PublishWithPartition failed: %v", err)
	}

	mock.mu.Lock()
	defer mock.mu.Unlock()
	if len(mock.messages) != 1 {
		t.Fatalf("expected 1 message, got %d", len(mock.messages))
	}

	msg := mock.messages[0]
	if msg.Partition != 0 {
		t.Errorf("expected partition 0, got %d", msg.Partition)
	}

	var decoded Message
	val, _ := msg.Value.Encode()
	if err := json.Unmarshal(val, &decoded); err != nil {
		t.Fatalf("failed to unmarshal message: %v", err)
	}
	if decoded.Type != MsgTypeVehicleOffline {
		t.Errorf("expected type '%s', got '%s'", MsgTypeVehicleOffline, decoded.Type)
	}
}

// --- ConsumerStart edge cases ---

func TestConsumerStart_WithConsumeError(t *testing.T) {
	// Create a consumer group mock that returns an error on Consume
	consumer, mock := newTestConsumer(t, newTestHandler())
	defer consumer.Close()

	// Close the claimCh before starting so mock Consume returns nil (no error exit),
	// then context cancellation triggers
	ctx, cancel := context.WithTimeout(context.Background(), 50*time.Millisecond)
	defer cancel()

	close(mock.claimCh) // cause mock Consume to return immediately
	err := consumer.Start(ctx)
	if err == nil {
		t.Fatal("expected error from context cancellation")
	}
}


