package scenarios

import (
	"log"
	"testing"

	"github.com/yuleDKCS/tests/e2e/client"
	"github.com/yuleDKCS/tests/e2e/proto"
)

// TestStatusSyncStrategies verifies vehicle status sync strategies.
func TestStatusSyncStrategies(t *testing.T) {
	log.Printf("═══════════════════════════════════════")
	log.Printf("📋 Test 12: 车况同步策略验证")
	log.Printf("═══════════════════════════════════════")

	carAddr := getCarAddr(t)
	phone, _ := client.NewMobileClient("sync_phone", "user_sync", 1, uint32(proto.ProtoCCC_DK3))
	phone.ConnectCar(carAddr)
	defer phone.DisconnectCar()
	phone.BLEPairing()
	phone.SCP03Establish()

	t.Run("PollBased", func(t *testing.T) {
		for i := 0; i < 3; i++ {
			resp, err := phone.SendFrame(proto.MsgTypeSyncPoll, encodePayload(
				&proto.SyncPayload{SyncMode: 1, DeviceID: phone.DeviceID}))
			if err != nil {
				t.Fatalf("Poll %d: %v", i, err)
			}
			var s proto.SyncPayload
			decodePayload(resp.Payload, &s)
			log.Printf("[POLL %d] lock=%d engine=%d bat=%d%%", i, s.LockStatus, s.EngineStatus, s.BatteryPct)
		}
		log.Printf("   ✅ Poll-based sync OK")
	})

	t.Run("EventDriven", func(t *testing.T) {
		resp, err := phone.SendFrame(proto.MsgTypeSyncEvent, encodePayload(
			&proto.SyncPayload{SyncMode: 2, DeviceID: phone.DeviceID}))
		if err != nil {
			t.Fatalf("Event: %v", err)
		}
		var evt proto.SyncPayload
		decodePayload(resp.Payload, &evt)
		if evt.SyncMode != 2 {
			t.Fatalf("Expected mode 2, got %d", evt.SyncMode)
		}
		log.Printf("   ✅ Event-driven push OK (seq=%d)", evt.EventSeq)
	})

	t.Run("Heartbeat", func(t *testing.T) {
		resp, err := phone.SendFrame(proto.MsgTypeSyncHeartbeat, encodePayload(
			&proto.SyncPayload{SyncMode: 3, DeviceID: phone.DeviceID}))
		if err != nil {
			t.Fatalf("Heartbeat: %v", err)
		}
		var hb proto.SyncPayload
		decodePayload(resp.Payload, &hb)
		if hb.BatteryPct == 0 {
			t.Fatalf("Heartbeat missing battery")
		}
		log.Printf("   ✅ Heartbeat sync OK (bat=%d%%)", hb.BatteryPct)
	})

	t.Run("OfflineBuffer", func(t *testing.T) {
		resp, err := phone.SendFrame(proto.MsgTypeSyncOffline, encodePayload(
			&proto.SyncPayload{
				SyncMode: 4, DeviceID: phone.DeviceID, OfflineCount: 5,
				OfflineEvents: []byte("evt_unlock_1\nevt_lock_2\nevt_trunk_3"),
			}))
		if err != nil {
			t.Fatalf("Offline: %v", err)
		}
		var off proto.SyncPayload
		decodePayload(resp.Payload, &off)
		log.Printf("   ✅ Offline sync OK (%d events)", off.OfflineCount)
	})

	t.Run("ConflictResolution", func(t *testing.T) {
		resp, err := phone.SendFrame(proto.MsgTypeSyncConflict, encodePayload(
			&proto.SyncPayload{
				SyncMode: 4, DeviceID: phone.DeviceID,
				EventSeq: 42, ConflictSeq: 50,
			}))
		if err != nil {
			t.Fatalf("Conflict: %v", err)
		}
		var c proto.SyncPayload
		decodePayload(resp.Payload, &c)
		if c.ResolvedBy == "" {
			t.Fatalf("Conflict not resolved")
		}
		log.Printf("   ✅ Conflict resolution OK (method=%s)", c.ResolvedBy)
	})

	t.Run("BatchSync", func(t *testing.T) {
		resp, err := phone.SendFrame(proto.MsgTypeSyncBatch, encodePayload(
			&proto.SyncPayload{SyncMode: 5, DeviceID: phone.DeviceID}))
		if err != nil {
			t.Fatalf("Batch: %v", err)
		}
		var b proto.SyncPayload
		decodePayload(resp.Payload, &b)
		log.Printf("   ✅ Batch sync OK (lock=%d engine=%d)", b.LockStatus, b.EngineStatus)
	})
}
