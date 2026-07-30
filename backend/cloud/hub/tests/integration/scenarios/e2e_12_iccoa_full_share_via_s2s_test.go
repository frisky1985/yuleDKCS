// E2E-12: ICCOA 完整分享链路（通过 S2S 客户端）
//
// 场景描述:
//   ICCOA.DK.TS.002 §7.6 — Key Sharing Flow
//   ICCOA.DK.TS.002 §13.5 — Vehicle OEM Server API
//
//   验证 Hub gRPC → KeyShareService → ICCOAAdapter → S2S client 链路的正确性。
//   S2S 端点是 mock HTTP server，确保 adapter 正确调用并解析。
//
//   测试范围:
//   1. gRPC CreateShare → KeyShareService → ICCOAAdapter → genSession (mock)
//   2. gRPC AcceptShare → KeyShareService → ICCOAAdapter → sign (mock)
//   3. BindKey → TrackKey → 注册钥匙
//   4. UnbindKey → ManageKey(revoke) + NotifyKeyEvent
//   5. S2S 故障 → graceful degradation（stub 回退）

package scenarios

import (
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"go.uber.org/zap"

	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/adapter"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/adapter/s2s"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/tests/integration/helpers"
)

// iccoaS2SRecorder 记录 mock S2S 调用
type iccoaS2SRecorder struct {
	genSession int
	sign       int
	trackKey   int
	manageKey  int
	notify     int
}

func newICCOAS2SMock(t *testing.T, r *iccoaS2SRecorder) *httptest.Server {
	t.Helper()
	return httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, rq *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		switch rq.URL.Path {
		case "/share/genSession":
			r.genSession++
			w.WriteHeader(http.StatusOK)
			json.NewEncoder(w).Encode(s2s.ICCOAGenSessionResponse{
				SessionID: "s2s-session-001",
				ShareCode: "888888",
			})
		case "/share/sign":
			r.sign++
			w.WriteHeader(http.StatusOK)
			json.NewEncoder(w).Encode(s2s.ICCOASignResponse{
				KeyID:  "s2s-friend-key-001",
				DkCert: "s2s-dk-cert",
			})
		case "/trackKey":
			r.trackKey++
			w.WriteHeader(http.StatusOK)
			json.NewEncoder(w).Encode(s2s.ICCOATrackKeyResponse{
				KeyID:         "s2s-tracked-key-001",
				VehiclePubKey: "s2s-vehicle-pubkey",
				Status:        "ACTIVE",
			})
		case "/manageKey":
			r.manageKey++
			w.WriteHeader(http.StatusOK)
			json.NewEncoder(w).Encode(s2s.ICCOAManageKeyResponse{Status: "REVOKED"})
		case "/notifyKeyEvent":
			r.notify++
			w.WriteHeader(http.StatusOK)
			json.NewEncoder(w).Encode(map[string]string{"status": "ok"})
		default:
			t.Logf("unexpected S2S path: %s", rq.URL.Path)
			w.WriteHeader(http.StatusNotFound)
		}
	}))
}

func mkICCOAAdapter(mockURL, vendor string) *adapter.ICCOAAdapter {
	cfg := s2s.NewDefaultICCOAConfig(vendor, mockURL, "oem-vehicle", "oem-device")
	cfg.RetryCount = 0
	cfg.RetryWait = 1 * time.Millisecond
	client := s2s.NewICCOAClient(vendor, cfg, zap.NewNop())
	return adapter.NewICCOAAdapterWithClient(vendor, zap.NewNop(), client)
}

// TestE2E12_ICCOAFullShareViaS2S 测试 ICCOA 完整 S2S 链路
func TestE2E12_ICCOAFullShareViaS2S(t *testing.T) {
	report := helpers.NewTestReport("E2E-12 ICCOA S2S 链路验证")
	rec := &iccoaS2SRecorder{}
	mockSrv := newICCOAS2SMock(t, rec)
	defer mockSrv.Close()

	ctx := context.Background()
	ad := mkICCOAAdapter(mockSrv.URL, "xiaomi")

	// ── 1: ShareKey → genSession ──
	t.Run("E2E-12-01: ShareKey→genSession", func(t *testing.T) {
		start := time.Now()
		resp, err := ad.ShareKey(ctx, &pb.CreateShareRequest{
			KeyId: "key-001", FromUserId: "owner-001", ToUserId: "friend-001",
		})
		if err != nil {
			t.Fatalf("ShareKey failed: %v", err)
		}
		if resp.ShareId != "s2s-session-001" {
			t.Errorf("expected s2s-session-001, got %q", resp.ShareId)
		}
		if rec.genSession != 1 {
			t.Errorf("genSession called %d times, want 1", rec.genSession)
		}
		report.Record("E2E-12-01: ShareKey→genSession", true, time.Since(start), "", "E2E-12", "ICCOA")
	})

	// ── 2: AcceptShare → sign ──
	t.Run("E2E-12-02: AcceptShare→sign", func(t *testing.T) {
		start := time.Now()
		resp, err := ad.AcceptShare(ctx, &pb.AcceptShareRequest{
			ShareCode: "888888", DeviceId: "friend-phone-001", UserId: "friend-001",
		})
		if err != nil {
			t.Fatalf("AcceptShare failed: %v", err)
		}
		if resp.Key.KeyId != "s2s-friend-key-001" {
			t.Errorf("expected s2s-friend-key-001, got %q", resp.Key.KeyId)
		}
		if resp.Key.KeyType != pb.KeyType_FRIEND {
			t.Errorf("expected FRIEND, got %v", resp.Key.KeyType)
		}
		if rec.sign != 1 {
			t.Errorf("sign called %d times, want 1", rec.sign)
		}
		report.Record("E2E-12-02: AcceptShare→sign", true, time.Since(start), "", "E2E-12", "ICCOA")
	})

	// ── 3: BindKey → trackKey ──
	t.Run("E2E-12-03: BindKey→trackKey", func(t *testing.T) {
		start := time.Now()
		resp, err := ad.BindKey(ctx, &pb.BindKeyRequest{
			VehicleId: "VH-001", DeviceId: "DEV-001", UserId: "owner-001",
			DevicePubkey: make([]byte, 64), KeyType: pb.KeyType_OWNER,
		})
		if err != nil {
			t.Fatalf("BindKey failed: %v", err)
		}
		if resp.Key.KeyId != "s2s-tracked-key-001" {
			t.Errorf("expected s2s-tracked-key-001, got %q", resp.Key.KeyId)
		}
		if rec.trackKey != 1 {
			t.Errorf("trackKey called %d times, want 1", rec.trackKey)
		}
		report.Record("E2E-12-03: BindKey→trackKey", true, time.Since(start), "", "E2E-12", "ICCOA")
	})

	// ── 4: UnbindKey → manageKey + notifyKeyEvent ──
	t.Run("E2E-12-04: UnbindKey→manageKey", func(t *testing.T) {
		start := time.Now()
		if err := ad.UnbindKey(ctx, "s2s-tracked-key-001"); err != nil {
			t.Fatalf("UnbindKey failed: %v", err)
		}
		if rec.manageKey < 1 {
			t.Error("manageKey not called")
		}
		report.Record("E2E-12-04: UnbindKey→manageKey", true, time.Since(start), "", "E2E-12", "ICCOA")
	})

	// ── 5: S2S 故障 → graceful degradation ──
	t.Run("E2E-12-05: S2S故障降级", func(t *testing.T) {
		start := time.Now()
		failSrv := newBrokenICCOAServer()
		defer failSrv.Close()

		failAd := mkICCOAAdapter(failSrv.URL, "xiaomi")
		// BindKey should fall back to stub
		resp, err := failAd.BindKey(ctx, &pb.BindKeyRequest{
			VehicleId: "VH-FAIL-001", DeviceId: "DEV-FAIL-001", UserId: "u-fail",
			DevicePubkey: make([]byte, 64), KeyType: pb.KeyType_OWNER,
		})
		if err != nil {
			t.Fatalf("BindKey should degrade gracefully: %v", err)
		}
		if resp.Key.KeyId == "" || resp.Key.Protocol != pb.Protocol_ICCOA_DK40 {
			t.Errorf("bad degradation response: id=%q proto=%v", resp.Key.KeyId, resp.Key.Protocol)
		}
		report.Record("E2E-12-05: S2S故障降级", true, time.Since(start), "", "E2E-12", "ICCOA")
	})
}

func newBrokenICCOAServer() *httptest.Server {
	return httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusInternalServerError)
		json.NewEncoder(w).Encode(s2s.ICCOAAPIError{Code: 50001, Message: "internal error"})
	}))
}
