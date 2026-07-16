// Package protocol — unit tests for protocol adapters
package protocol

import (
	"context"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestCCCProtocolAdapter(t *testing.T) {
	a := NewCCCProtocolAdapter("apple")
	require.NotNil(t, a)

	t.Run("vendor and protocol", func(t *testing.T) {
		assert.Equal(t, "apple", a.Vendor())
		assert.Equal(t, "ccc_dk3", a.Protocol())
	})

	t.Run("connect and disconnect", func(t *testing.T) {
		ctx := context.Background()
		err := a.Connect(ctx, "apple", "https://api.apple.com/dk")
		assert.NoError(t, err)

		health, err := a.HealthCheck(ctx, "apple")
		assert.NoError(t, err)
		assert.True(t, health.Healthy)

		err = a.Disconnect(ctx, "apple")
		assert.NoError(t, err)

		health, err = a.HealthCheck(ctx, "apple")
		assert.NoError(t, err)
		assert.False(t, health.Healthy)
	})

	t.Run("send after connect", func(t *testing.T) {
		ctx := context.Background()
		_ = a.Connect(ctx, "apple", "https://api.apple.com/dk")

		resp, err := a.Send(ctx, "apple", &ProtocolMessage{
			Type:    MsgTypeRemoteControl,
			Payload: []byte("lock"),
		})
		assert.NoError(t, err)
		assert.NotNil(t, resp)
		assert.Equal(t, "apple", resp.Vendor)
	})

	t.Run("send without connect returns error", func(t *testing.T) {
		disconnected := NewCCCProtocolAdapter("samsung")
		_, err := disconnected.Send(context.Background(), "samsung", &ProtocolMessage{})
		assert.Error(t, err)
		assert.Contains(t, err.Error(), "not connected")
	})

	t.Run("receive after connect", func(t *testing.T) {
		ctx := context.Background()
		_ = a.Connect(ctx, "apple", "https://api.apple.com/dk")

		msg, err := a.Receive(ctx, "apple")
		assert.NoError(t, err)
		assert.NotNil(t, msg)
	})

	t.Run("bind", func(t *testing.T) {
		ctx := context.Background()
		result, err := a.Bind(ctx, "device-001", &BindParams{
			UserID: "user-001", VehicleID: "vehicle-001",
		})
		assert.NoError(t, err)
		assert.Equal(t, "bound", result.Status)
		assert.Equal(t, "device-001", result.DeviceID)
	})

	t.Run("unbind", func(t *testing.T) {
		err := a.Unbind(context.Background(), "device-001")
		assert.NoError(t, err)
	})

	t.Run("send command", func(t *testing.T) {
		result, err := a.SendCommand(context.Background(), "device-001", &DeviceCommand{
			Type: "unlock",
		})
		assert.NoError(t, err)
		assert.True(t, result.Success)
	})

	t.Run("discover", func(t *testing.T) {
		_, err := a.Discover(context.Background(), &DiscoveryFilter{})
		// CCC returns an error as it needs vendor API
		assert.Error(t, err)
	})
}

func TestICCEProtocolAdapter(t *testing.T) {
	a := NewICCEProtocolAdapter("huawei")
	require.NotNil(t, a)

	t.Run("vendor and protocol", func(t *testing.T) {
		assert.Equal(t, "huawei", a.Vendor())
		assert.Equal(t, "icce", a.Protocol())
	})

	t.Run("connect and health", func(t *testing.T) {
		ctx := context.Background()
		err := a.Connect(ctx, "huawei", "https://api.huawei.com/dk")
		assert.NoError(t, err)

		health, err := a.HealthCheck(ctx, "huawei")
		assert.NoError(t, err)
		assert.True(t, health.Healthy)
	})

	t.Run("send and receive", func(t *testing.T) {
		ctx := context.Background()
		resp, err := a.Send(ctx, "huawei", &ProtocolMessage{
			Type: MsgTypeKeyBind, Payload: []byte("bind"),
		})
		assert.NoError(t, err)
		assert.NotNil(t, resp)

		msg, err := a.Receive(ctx, "huawei")
		assert.NoError(t, err)
		assert.NotNil(t, msg)
	})

	t.Run("bind and unbind", func(t *testing.T) {
		ctx := context.Background()
		result, err := a.Bind(ctx, "device-002", &BindParams{
			VehicleID: "vehicle-002", KeyType: "owner",
		})
		assert.NoError(t, err)
		assert.Equal(t, "icce", result.Protocol)

		err = a.Unbind(ctx, "device-002")
		assert.NoError(t, err)
	})

	t.Run("send command", func(t *testing.T) {
		result, err := a.SendCommand(context.Background(), "device-002", &DeviceCommand{
			Type: "lock",
		})
		assert.NoError(t, err)
		assert.True(t, result.Success)
	})

	t.Run("receive event", func(t *testing.T) {
		event, err := a.ReceiveEvent(context.Background(), "device-002")
		assert.NoError(t, err)
		assert.Equal(t, "device-002", event.DeviceID)
	})
}

func TestICCOAProtocolAdapter(t *testing.T) {
	t.Run("DK30 version", func(t *testing.T) {
		a := NewICCOAProtocolAdapter("xiaomi", "dk30")
		assert.Equal(t, "xiaomi", a.Vendor())
		assert.Equal(t, "iccoa_dk30", a.Protocol())
	})

	t.Run("DK40 version", func(t *testing.T) {
		a := NewICCOAProtocolAdapter("oppo", "dk40")
		assert.Equal(t, "iccoa_dk40", a.Protocol())
	})

	t.Run("connect and send", func(t *testing.T) {
		ctx := context.Background()
		a := NewICCOAProtocolAdapter("vivo", "dk30")
		err := a.Connect(ctx, "vivo", "https://api.vivo.com/dk")
		assert.NoError(t, err)

		resp, err := a.Send(ctx, "vivo", &ProtocolMessage{
			Type: MsgTypeKeyShare, Payload: []byte("share"),
		})
		assert.NoError(t, err)
		assert.Equal(t, "vivo", resp.Vendor)
	})

	t.Run("bind", func(t *testing.T) {
		ctx := context.Background()
		a := NewICCOAProtocolAdapter("xiaomi", "dk40")
		result, err := a.Bind(ctx, "device-003", &BindParams{
			UserID: "user-003", VehicleID: "vehicle-003",
		})
		assert.NoError(t, err)
		assert.Equal(t, "iccoa_dk40", result.Protocol)
	})
}

func TestProtocolMessageTypes(t *testing.T) {
	assert.Equal(t, MessageType(0), MsgTypeUnknown)
	assert.Equal(t, MessageType(1), MsgTypeDeviceInfo)
	assert.Equal(t, MessageType(2), MsgTypeCapability)
	assert.Equal(t, MessageType(3), MsgTypeKeyBind)
	assert.Equal(t, MessageType(4), MsgTypeKeyUnbind)
	assert.Equal(t, MessageType(5), MsgTypeKeyShare)
	assert.Equal(t, MessageType(6), MsgTypeKeyAccept)
	assert.Equal(t, MessageType(7), MsgTypeKeyRevoke)
	assert.Equal(t, MessageType(8), MsgTypeRemoteControl)
	assert.Equal(t, MessageType(9), MsgTypeVehicleStatus)
	assert.Equal(t, MessageType(10), MsgTypeHeartbeat)
}

func TestDeviceBridgeInterface(t *testing.T) {
	// Verify all adapters implement DeviceBridge
	ccc := NewCCCProtocolAdapter("apple")
	icce := NewICCEProtocolAdapter("huawei")
	iccoa := NewICCOAProtocolAdapter("xiaomi", "dk30")

	assert.Implements(t, (*DeviceBridge)(nil), ccc)
	assert.Implements(t, (*DeviceBridge)(nil), icce)
	assert.Implements(t, (*DeviceBridge)(nil), iccoa)
}

func TestProtocolAdapterInterface(t *testing.T) {
	ccc := NewCCCProtocolAdapter("apple")
	icce := NewICCEProtocolAdapter("huawei")
	iccoa := NewICCOAProtocolAdapter("xiaomi", "dk30")

	assert.Implements(t, (*ProtocolAdapter)(nil), ccc)
	assert.Implements(t, (*ProtocolAdapter)(nil), icce)
	assert.Implements(t, (*ProtocolAdapter)(nil), iccoa)
}

func TestAdapterConcurrency(t *testing.T) {
	ctx := context.Background()
	a := NewCCCProtocolAdapter("apple")
	_ = a.Connect(ctx, "apple", "https://api.apple.com")

	t.Run("concurrent sends", func(t *testing.T) {
		done := make(chan struct{}, 2)
		go func() {
			for i := 0; i < 10; i++ {
				_, _ = a.Send(ctx, "apple", &ProtocolMessage{
					Type: MsgTypeRemoteControl,
				})
			}
			done <- struct{}{}
		}()
		go func() {
			for i := 0; i < 10; i++ {
				_, _ = a.HealthCheck(ctx, "apple")
			}
			done <- struct{}{}
		}()
		<-done
		<-done
	})
}
