package scenarios

import (
	"log"
	"testing"
	"time"

	"github.com/yuleDKCS/tests/e2e/client"
	"github.com/yuleDKCS/tests/e2e/proto"
)

func TestExtendedVehicleControl(t *testing.T) {
	log.Printf("═══════════════════════════════════════")
	log.Printf("📋 Test 11: 完整车控逻辑验证")
	log.Printf("═══════════════════════════════════════")
	carAddr := getCarAddr(t)
	now := time.Now().UnixMilli()

	phone, _ := client.NewMobileClient("ctrl_phone", "user_ctrl", 1, uint32(proto.ProtoCCC_DK3))
	phone.ConnectCar(carAddr)
	defer phone.DisconnectCar()
	phone.BLEPairing()
	phone.SCP03Establish()
	phone.WriteKeyParams("ctrl_master_001", 1, 0xFF, now, now+365*24*3600*1000)

	controls := []struct {
		name string
		run  func() error
	}{
		{"Unlock", func() error { return phone.Unlock("ctrl_master_001") }},
		{"Lock", func() error {
			_, e := phone.SendFrame(proto.MsgTypeLockCmd, encodePayload(&proto.CommandPayload{Action: "lock", KeyID: "ctrl_master_001"})); return e
		}},
		{"EngineStart", func() error {
			_, e := phone.SendFrame(proto.MsgTypeEngineCmd, encodePayload(&proto.CommandPayload{Action: "engine_on", KeyID: "ctrl_master_001"})); return e
		}},
		{"EngineStop", func() error {
			_, e := phone.SendFrame(proto.MsgTypeEngineCmd, encodePayload(&proto.CommandPayload{Action: "engine_off", KeyID: "ctrl_master_001"})); return e
		}},
		{"TrunkOpen", func() error {
			_, e := phone.SendFrame(proto.MsgTypeTrunkCmd, encodePayload(&proto.CommandPayload{Action: "trunk", KeyID: "ctrl_master_001"})); return e
		}},
		{"WindowDown", func() error {
			_, e := phone.SendFrame(proto.MsgTypeWindowCmd, encodePayload(&proto.CommandPayload{Action: "window_down", KeyID: "ctrl_master_001"})); return e
		}},
		{"WindowUp", func() error {
			_, e := phone.SendFrame(proto.MsgTypeWindowCmd, encodePayload(&proto.CommandPayload{Action: "window_up", KeyID: "ctrl_master_001"})); return e
		}},
		{"ClimateOn", func() error {
			_, e := phone.SendFrame(proto.MsgTypeClimateCmd, encodePayload(&proto.CommandPayload{Action: "climate_on", KeyID: "ctrl_master_001"})); return e
		}},
		{"FindMyCar", func() error {
			_, e := phone.SendFrame(proto.MsgTypeFindCmd, encodePayload(&proto.CommandPayload{Action: "find", KeyID: "ctrl_master_001"})); return e
		}},
		{"HornLight", func() error {
			_, e := phone.SendFrame(proto.MsgTypeHornLightCmd, encodePayload(&proto.CommandPayload{Action: "honk", KeyID: "ctrl_master_001"})); return e
		}},
	}
	for _, ctrl := range controls {
		t.Run(ctrl.name, func(t *testing.T) {
			if err := ctrl.run(); err != nil {
				t.Fatalf("❌ %s: %v", ctrl.name, err)
			}
			log.Printf("   ✅ %s OK", ctrl.name)
		})
	}
}
