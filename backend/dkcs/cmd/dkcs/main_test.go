// Package main smoke tests.
//
// These tests verify:
//   - The package and its imports compile correctly
//   - Key adapter types and construction patterns work
//   - Helper functions behave correctly with nil/empty inputs
package main

import (
	"context"
	"testing"
	"time"

	"github.com/frisky1985/yuleDKCS/backend/dkcs/internal/config"
)

// ─────────────────────────────────────────────────────────────
// Compilation check
// ─────────────────────────────────────────────────────────────

// TestBuild ensures the package builds and all imports resolve.
// This is a smoke test — if the package compiles, all dependency
// wiring is at least syntactically correct.
func TestBuild(t *testing.T) {
	// If we got here, the package compiled and linked.
	// No further assertions needed — the build toolchain already
	// validated everything.
}

// ─────────────────────────────────────────────────────────────
// kafkaEventBusAdapter smoke test
// ─────────────────────────────────────────────────────────────

// TestKafkaEventBusAdapter_Construction verifies the adapter
// can be constructed. The KafkaProducer is nil, so PublishKeyEvent
// should return an error (nil producer = no Kafka backend).
func TestKafkaEventBusAdapter_Construction(t *testing.T) {
	adapter := &kafkaEventBusAdapter{producer: nil}
	if adapter == nil {
		t.Fatal("kafkaEventBusAdapter should not be nil")
	}
}

// TestKafkaEventBusAdapter_PublishWithNilProducer verifies that
// calling PublishKeyEvent with a nil producer returns an error.
func TestKafkaEventBusAdapter_PublishWithNilProducer(t *testing.T) {
	adapter := &kafkaEventBusAdapter{producer: nil}
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	err := adapter.PublishKeyEvent(ctx, "test_event", "key-001", "owner-001", "target-001")
	if err == nil {
		t.Error("Expected error from nil producer, got nil")
	}
}

// TestKafkaEventBusAdapter_NilContext verifies the adapter
// handles a nil context gracefully (should still return an error
// from the nil producer rather than panicking).
func TestKafkaEventBusAdapter_NilContext(t *testing.T) {
	adapter := &kafkaEventBusAdapter{producer: nil}
	err := adapter.PublishKeyEvent(nil, "test_event", "key-001", "owner-001", "target-001")
	if err == nil {
		t.Error("Expected error from nil producer, got nil")
	}
}

// TestKafkaEventBusAdapter_EmptyFields verifies the adapter
// works with empty string fields (no panic, returns error from
// nil producer).
func TestKafkaEventBusAdapter_EmptyFields(t *testing.T) {
	adapter := &kafkaEventBusAdapter{producer: nil}
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	err := adapter.PublishKeyEvent(ctx, "", "", "", "")
	if err == nil {
		t.Error("Expected error from nil producer, got nil")
	}
}

// ─────────────────────────────────────────────────────────────
// initDatabase error handling
// ─────────────────────────────────────────────────────────────

// TestInitDatabase_InvalidDSN verifies that initDatabase returns
// an error (not a panic) when given a config that cannot connect.
func TestInitDatabase_InvalidDSN(t *testing.T) {
	// Zero-value config — no real DSN, should fail to connect
	db, err := initDatabase(config.DatabaseConfig{
		Host: "localhost",
		Port: 0,
	})
	if err == nil {
		db.Close()
		t.Error("Expected connection error with invalid DSN, got nil")
	}
}

// ─────────────────────────────────────────────────────────────
// initRedis safety check
// ─────────────────────────────────────────────────────────────

// TestInitRedis_EmptyAddr verifies initRedis does not panic
// when given an empty address (it returns a client that will
// fail on Ping, but should not crash on construction).
func TestInitRedis_EmptyAddr(t *testing.T) {
	client := initRedis(config.RedisConfig{})
	if client == nil {
		t.Fatal("initRedis should return a client, not nil")
	}
	defer client.Close()
}

// TestInitRedis_InvalidAddr verifies the Redis client handles
// invalid addresses without panic during Ping.
func TestInitRedis_InvalidAddr(t *testing.T) {
	client := initRedis(config.RedisConfig{
		Addr: "localhost:0",
	})
	if client == nil {
		t.Fatal("initRedis should return a client, not nil")
	}
	defer client.Close()

	// Ping should fail but not panic
	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
	defer cancel()
	err := client.Ping(ctx).Err()
	if err == nil {
		t.Log("Unexpected: Ping succeeded against localhost:0")
	}
}
