package mq

import (
	"context"
	"fmt"
	"net"
	"sync"
	"testing"
	"time"

	mochi "github.com/mochi-mqtt/server/v2"
	"github.com/mochi-mqtt/server/v2/hooks/auth"
	"github.com/mochi-mqtt/server/v2/listeners"
)

// startTestBroker boots an in-memory MQTT broker (mochi-mqtt, pure Go) on a
// free localhost port and returns its address plus a cleanup func.
func startTestBroker(t *testing.T) (string, func()) {
	t.Helper()

	// Reserve a free port, then hand it to the broker.
	probe, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("reserve test port: %v", err)
	}
	addr := probe.Addr().String()
	_ = probe.Close()

	server := mochi.New(nil)
	_ = server.AddHook(new(auth.AllowHook), nil)

	tcp := listeners.NewTCP(listeners.Config{ID: "test", Address: addr})
	if err := server.AddListener(tcp); err != nil {
		t.Fatalf("add listener: %v", err)
	}
	go func() { _ = server.Serve() }()

	cleanup := func() {
		ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
		defer cancel()
		_ = server.Close()
		<-ctx.Done()
	}
	return "tcp://" + addr, cleanup
}

func waitForMessage(t *testing.T, ch <-chan *Message, timeout time.Duration) *Message {
	t.Helper()
	select {
	case msg := <-ch:
		return msg
	case <-time.After(timeout):
		t.Fatal("timed out waiting for MQTT message")
		return nil
	}
}

func TestMQTTProducerPublishMessage(t *testing.T) {
	broker, stop := startTestBroker(t)
	defer stop()

	cfg := MQTTConfig{
		Broker:         broker,
		ClientID:       "test-producer",
		QoS:            1,
		KeepAlive:      30 * time.Second,
		ConnectTimeout: 5 * time.Second,
		AutoReconnect:  true,
		Topic:          "dkcs/tcu/commands",
	}

	received := make(chan *Message, 1)
	handler := MessageHandlerFunc(func(ctx context.Context, msg *Message) error {
		received <- msg
		return nil
	})

	subCfg := cfg
	subCfg.ClientID = cfg.ClientID + "-sub"
	sub, err := NewMQTTSubscriber(subCfg, handler)
	if err != nil {
		t.Fatalf("subscribe: %v", err)
	}
	defer sub.Close()

	pubCfg := cfg
	pubCfg.ClientID = cfg.ClientID + "-pub"
	producer, err := NewMQTTProducer(pubCfg)
	if err != nil {
		t.Fatalf("producer: %v", err)
	}
	defer producer.Close()

	if err := producer.PublishMessage(context.Background(), "key-1", MsgTypeCommandSent,
		map[string]interface{}{"vin": "VIN123", "cmd": "lock"}); err != nil {
		t.Fatalf("publish: %v", err)
	}

	msg := waitForMessage(t, received, 5*time.Second)
	if msg.Key != "key-1" {
		t.Errorf("expected key=key-1, got %s", msg.Key)
	}
	if msg.Type != MsgTypeCommandSent {
		t.Errorf("expected type=%s, got %s", MsgTypeCommandSent, msg.Type)
	}
	if msg.Data["vin"] != "VIN123" || msg.Data["cmd"] != "lock" {
		t.Errorf("unexpected data: %v", msg.Data)
	}
	if msg.Timestamp == 0 {
		t.Error("expected non-zero timestamp")
	}
}

func TestMQTTProducerPublishRaw(t *testing.T) {
	broker, stop := startTestBroker(t)
	defer stop()

	cfg := MQTTConfig{
		Broker:         broker,
		ClientID:       "test-raw",
		QoS:            0,
		KeepAlive:      30 * time.Second,
		ConnectTimeout: 5 * time.Second,
		Topic:          "dkcs/tcu/events",
	}

	received := make(chan *Message, 1)
	handler := MessageHandlerFunc(func(ctx context.Context, msg *Message) error {
		received <- msg
		return nil
	})

	subCfg := cfg
	subCfg.ClientID = cfg.ClientID + "-sub"
	sub, err := NewMQTTSubscriber(subCfg, handler)
	if err != nil {
		t.Fatalf("subscribe: %v", err)
	}
	defer sub.Close()

	pubCfg := cfg
	pubCfg.ClientID = cfg.ClientID + "-pub"
	producer, err := NewMQTTProducer(pubCfg)
	if err != nil {
		t.Fatalf("producer: %v", err)
	}
	defer producer.Close()

	// Non-JSON payload → delivered as raw under Data["payload"].
	if err := producer.Publish(context.Background(), cfg.Topic, []byte("RAW-TCU-FRAME")); err != nil {
		t.Fatalf("publish raw: %v", err)
	}

	msg := waitForMessage(t, received, 5*time.Second)
	if msg.Type != "raw" {
		t.Errorf("expected type=raw, got %s", msg.Type)
	}
	if msg.Data["payload"] != "RAW-TCU-FRAME" {
		t.Errorf("unexpected raw payload: %v", msg.Data["payload"])
	}
}

func TestMQTTIsConnected(t *testing.T) {
	broker, stop := startTestBroker(t)
	defer stop()

	cfg := MQTTConfig{
		Broker:         broker,
		ClientID:       "test-conn",
		QoS:            1,
		KeepAlive:      30 * time.Second,
		ConnectTimeout: 5 * time.Second,
		Topic:          "dkcs/tcu/commands",
	}

	producer, err := NewMQTTProducer(cfg)
	if err != nil {
		t.Fatalf("producer: %v", err)
	}
	defer producer.Close()

	if !producer.IsConnected() {
		t.Error("expected producer to be connected")
	}
	producer.Close()
	if producer.IsConnected() {
		t.Error("expected producer to be disconnected after Close")
	}
}

func TestMQTTConfigErrors(t *testing.T) {
	tests := []struct {
		name   string
		config MQTTConfig
	}{
		{"empty broker", MQTTConfig{ClientID: "x", ConnectTimeout: time.Second}},
		{"invalid qos", MQTTConfig{Broker: "tcp://127.0.0.1:1883", ClientID: "x", QoS: 5, ConnectTimeout: time.Second}},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if _, err := NewMQTTProducer(tt.config); err == nil {
				t.Error("expected error, got nil")
			}
		})
	}
}

func TestMQTTConnectRefused(t *testing.T) {
	// Grab a free port and close it so nothing listens there.
	probe, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("reserve port: %v", err)
	}
	addr := probe.Addr().String()
	_ = probe.Close()

	cfg := MQTTConfig{
		Broker:         "tcp://" + addr,
		ClientID:       "test-refused",
		QoS:            1,
		ConnectTimeout: 2 * time.Second,
	}

	start := time.Now()
	if _, err := NewMQTTProducer(cfg); err == nil {
		t.Fatal("expected connect error, got nil")
	}
	if elapsed := time.Since(start); elapsed > 10*time.Second {
		t.Errorf("connect refused took too long: %v", elapsed)
	}
}

func TestMQTTBuildTLSConfig(t *testing.T) {
	// Missing CA file must error.
	cfg := MQTTConfig{
		TLSEnabled: true,
		TLSCACert:  "/nonexistent/ca.pem",
	}
	if _, err := buildTLSConfig(cfg); err == nil {
		t.Error("expected error for missing CA cert, got nil")
	}

	// TLS disabled with missing files should still be fine for client cert skip.
	cfg2 := MQTTConfig{TLSEnabled: false}
	tlsCfg, err := buildTLSConfig(cfg2)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if tlsCfg == nil {
		t.Fatal("expected non-nil tls.Config")
	}
}

func TestMQTTConcurrentPublish(t *testing.T) {
	broker, stop := startTestBroker(t)
	defer stop()

	cfg := MQTTConfig{
		Broker:         broker,
		ClientID:       "test-conc",
		QoS:            0,
		KeepAlive:      30 * time.Second,
		ConnectTimeout: 5 * time.Second,
		Topic:          "dkcs/tcu/commands",
	}

	const workers = 8
	const perWorker = 10

	var mu sync.Mutex
	count := 0
	handler := MessageHandlerFunc(func(ctx context.Context, msg *Message) error {
		mu.Lock()
		count++
		mu.Unlock()
		return nil
	})

	subCfg := cfg
	subCfg.ClientID = "test-conc-sub"
	sub, err := NewMQTTSubscriber(subCfg, handler)
	if err != nil {
		t.Fatalf("subscribe: %v", err)
	}
	defer sub.Close()

	pubCfg := cfg
	pubCfg.ClientID = "test-conc-pub"
	producer, err := NewMQTTProducer(pubCfg)
	if err != nil {
		t.Fatalf("producer: %v", err)
	}
	defer producer.Close()

	var wg sync.WaitGroup
	for w := 0; w < workers; w++ {
		wg.Add(1)
		go func(w int) {
			defer wg.Done()
			for i := 0; i < perWorker; i++ {
				key := fmt.Sprintf("k-%d-%d", w, i)
				if err := producer.PublishMessage(context.Background(), key, MsgTypeKeyActivated, nil); err != nil {
					t.Errorf("publish %s: %v", key, err)
					return
				}
			}
		}(w)
	}
	wg.Wait()

	deadline := time.Now().Add(5 * time.Second)
	for time.Now().Before(deadline) {
		mu.Lock()
		n := count
		mu.Unlock()
		if n == workers*perWorker {
			break
		}
		time.Sleep(50 * time.Millisecond)
	}

	mu.Lock()
	defer mu.Unlock()
	if count != workers*perWorker {
		t.Errorf("expected %d messages, got %d", workers*perWorker, count)
	}
}
