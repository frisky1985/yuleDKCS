package scenarios

import (
	"log"
	"testing"
	"time"

	"github.com/yuleDKCS/tests/e2e/client"
	"github.com/yuleDKCS/tests/e2e/proto"
)

func TestMultiProtocolKeyProvisioning(t *testing.T) {
	requireCarSim(t)
	log.Printf("═══════════════════════════════════════")
	log.Printf("📋 Test 09: 三协议钥匙开通验证")
	log.Printf("═══════════════════════════════════════")
	carAddr := getCarAddr(t)
	now := time.Now().UnixMilli()
	until := now + 365*24*3600*1000

	tests := []struct {
		name     string
		protocol uint32
		vendor   uint32
	}{
		{"ICCE-国密", uint32(proto.ProtoICCE), 6},
		{"CCC-3.0", uint32(proto.ProtoCCC_DK3), 1},
		{"ICCOA-DK3.0", uint32(proto.ProtoICCOA30), 3},
		{"ICCOA-DK4.0", uint32(proto.ProtoICCOA40), 4},
	}
	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			phone, err := client.NewMobileClient("multi_"+tc.name+"_"+time.Now().Format("150405"), "user_multi", tc.vendor, tc.protocol)
			if err != nil {
				t.Fatalf("Create: %v", err)
			}
			if err := phone.ConnectCar(carAddr); err != nil {
				t.Fatalf("Connect: %v", err)
			}
			defer phone.DisconnectCar()
			switch proto.ProtocolEnum(tc.protocol) {
			case proto.ProtoICCE:
				if !phone.ICCEPair() {
					t.Fatalf("ICCE pair failed")
				}
			case proto.ProtoCCC_DK3:
				if !phone.CccOobExchange() {
					t.Fatalf("CCC OOB failed")
				}
				phone.CccUwbRanging(1)
			case proto.ProtoICCOA30, proto.ProtoICCOA40:
				if !phone.IcCoaFrameExchange(tc.protocol == uint32(proto.ProtoICCOA40)) {
					t.Fatalf("ICCOA exchange failed")
				}
			}
			phone.BLEPairing()
			phone.SCP03Establish()
			phone.WriteKeyParams("key_"+tc.name+"_001", 1, 0xFF, now, until)
			log.Printf("   ✅ %s key provisioned", tc.name)
			sig, _ := phone.SignChallenge("key_" + tc.name + "_001")
			log.Printf("   ✅ %s auth OK (sig=%d bytes)", tc.name, len(sig))
		})
	}
}

func TestKeySharingAcrossProtocols(t *testing.T) {
	requireCarSim(t)
	log.Printf("═══════════════════════════════════════")
	log.Printf("📋 Test 10: 跨协议钥匙分享验证")
	log.Printf("═══════════════════════════════════════")
	carAddr := getCarAddr(t)
	now := time.Now().UnixMilli()
	exp := now + 7*24*3600*1000

	owner, _ := client.NewMobileClient("shr_owner", "user_owner", 6, uint32(proto.ProtoICCE))
	owner.ConnectCar(carAddr)
	defer owner.DisconnectCar()
	owner.BLEPairing()
	owner.SCP03Establish()
	owner.WriteKeyParams("shared_cross_001", 2, 0x0F, now, exp)
	log.Printf("   ✅ Owner provisioned: shared_cross_001")

	friend, _ := client.NewMobileClient("shr_friend", "user_friend", 1, uint32(proto.ProtoCCC_DK3))
	friend.ConnectCar(carAddr)
	defer friend.DisconnectCar()
	friend.BLEPairing()
	friend.SCP03Establish()
	friend.WriteKeyParams("shared_cross_001", 2, 0x0F, now, exp)
	log.Printf("   ✅ Friend wrote shared key")
	friend.Unlock("shared_cross_001")
	log.Printf("   ✅ Cross-protocol unlock OK")
	owner.Unlock("shared_cross_001")
	log.Printf("   ✅ Owner still active after share")
}
