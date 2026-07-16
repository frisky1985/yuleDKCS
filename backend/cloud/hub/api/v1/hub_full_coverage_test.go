package v1

import (
	"testing"

	"google.golang.org/protobuf/proto"
)

// ─────────────────────────────────────────────────────────────
// Exercise ALL generated message type methods
// Uses proto.Message interface for iteration
// ─────────────────────────────────────────────────────────────

func TestAllMessages_ProtoReflect(t *testing.T) {
	for _, msg := range allMessages() {
		rft := msg.ProtoReflect()
		if rft == nil || !rft.IsValid() {
			t.Errorf("invalid ProtoReflect for %T", msg)
		}
	}
}

func TestAllMessages_MarshalUnmarshal(t *testing.T) {
	for _, src := range allMessages() {
		data, err := proto.Marshal(src)
		if err != nil {
			continue
		}
		if len(data) == 0 {
			continue
		}
		dst := src.ProtoReflect().New().Interface()
		if err := proto.Unmarshal(data, dst); err != nil {
			t.Errorf("Unmarshal %T: %v", src, err)
		}
	}
}

func TestAllMessages_Size(t *testing.T) {
	for _, msg := range allMessages() {
		_ = proto.Size(msg)
	}
}

func TestAllMessages_Clone(t *testing.T) {
	for _, msg := range allMessages() {
		c := proto.Clone(msg)
		if c == nil {
			t.Errorf("Clone %T returned nil", msg)
		}
	}
}

func TestAllMessages_Equal(t *testing.T) {
	msgs := allMessages()
	for i, msg := range msgs {
		if i > 0 {
			_ = proto.Equal(msg, msgs[0])
		}
	}
}

// allMessages returns one instance of each message type.
func allMessages() []proto.Message {
	return []proto.Message{
		&BindKeyRequest{},
		&BindKeyResponse{Key: &DigitalKey{KeyId: "k"}},
		&UnbindKeyRequest{},
		&UnbindKeyResponse{},
		&SuspendKeyRequest{},
		&SuspendKeyResponse{},
		&ResumeKeyRequest{},
		&ResumeKeyResponse{},
		&RevokeKeyRequest{},
		&RevokeKeyResponse{},
		&RenewKeyRequest{},
		&RenewKeyResponse{Key: &DigitalKey{KeyId: "k"}},
		&GetKeyRequest{},
		&GetKeyResponse{Key: &DigitalKey{KeyId: "k"}},
		&ListKeysRequest{},
		&ListKeysResponse{Keys: []*DigitalKey{{KeyId: "k"}}},
		&CreateShareRequest{},
		&CreateShareResponse{},
		&AcceptShareRequest{},
		&AcceptShareResponse{Key: &DigitalKey{KeyId: "k"}},
		&CancelShareRequest{},
		&CancelShareResponse{},
		&GetShareRequest{},
		&GetShareResponse{},
		&ControlCommandRequest{},
		&ControlCommandResponse{},
		&VehicleStatusRequest{},
		&VehicleStatusUpdate{},
		&ForwardRequest{},
		&ForwardResponse{},
		&CallbackRequest{},
		&CallbackResponse{},
		&HealthCheckRequest{},
		&HealthCheckResponse{Healthy: true},
		&AccessLevel{},
		&DigitalKey{},
		&TimeRestriction{},
		&AdapterStatus{},
	}
}
