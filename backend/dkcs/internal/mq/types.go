package mq

import (
	"context"
	"fmt"
)

// KeyEvent represents a digital key lifecycle event for Kafka publication.
// This is the structured payload sent as Publish data.
type KeyEvent struct {
	EventType string `json:"event_type"` // key_shared | key_revoked | key_activated | key_updated
	KeyID     string `json:"key_id"`
	OwnerID   string `json:"owner_id"`
	TargetID  string `json:"target_id,omitempty"` // 接收者 (分享时)
	Timestamp int64  `json:"timestamp"`
}

// PublishKeyEvent publishes a structured KeyEvent via the Kafka producer.
// It converts the KeyEvent into the generic Publish format for downstream consumers.
func (p *KafkaProducer) PublishKeyEvent(ctx context.Context, event KeyEvent) error {
	data := map[string]interface{}{
		"event_type": event.EventType,
		"key_id":     event.KeyID,
		"owner_id":   event.OwnerID,
		"timestamp":  event.Timestamp,
	}
	if event.TargetID != "" {
		data["target_id"] = event.TargetID
	}

	msgType, err := keyEventTypeToMsgType(event.EventType)
	if err != nil {
		return fmt.Errorf("publish key event: %w", err)
	}

	return p.Publish(ctx, event.KeyID, msgType, data)
}

// keyEventTypeToMsgType maps KeyEvent.EventType to the Message.Type constant.
func keyEventTypeToMsgType(eventType string) (string, error) {
	switch eventType {
	case "key_shared":
		return MsgTypeKeyShared, nil
	case "key_revoked":
		return MsgTypeKeyRevoked, nil
	case "key_activated":
		return MsgTypeKeyActivated, nil
	case "key_updated":
		return MsgTypeKeyUpdated, nil
	default:
		return "", fmt.Errorf("unknown key event type: %s", eventType)
	}
}

// key event type constants (durable strings for the KeyEvent struct)
const (
	KeyEventShared   = "key_shared"
	KeyEventRevoked  = "key_revoked"
	KeyEventActivated = "key_activated"
	KeyEventUpdated  = "key_updated"
)

// MsgTypeKeyUpdated is the message type constant for key update events
const MsgTypeKeyUpdated = "key_updated"
