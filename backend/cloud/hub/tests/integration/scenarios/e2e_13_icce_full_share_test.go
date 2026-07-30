// E2E-13: ICCE 完整分享链路（通过 S2S 客户端）
//
// 场景描述:
//   ICCE.TS.004 §3.1 — Key Sharing Flow
//
//   验证 ICCEAdapter → S2S client → 厂商服务器 链路的正确性。
//   S2S 端点是 mock HTTP server，确保 adapter 正确调用并解析。
//
//   测试范围:
//   1. ShareKey → S2S /share → 返回 shareCode
//   2. AcceptShare → S2S /share/accept → 签发好友钥匙
//   3. BindKey → S2S /bind → 绑定钥匙
//   4. UnbindKey → S2S /unbind
//   5. RevokeNotify → S2S /revoke
//   6. S2S 故障 → graceful degradation（stub 回退）

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

// icceS2SRecorder 记录 mock ICCE S2S 调用
type icceS2SRecorder struct {
	bind       int
	unbind     int
	revoke     int
	share      int
	acceptShare int
}

func newICCES2SMock(t *testing.T, r *icceS2SRecorder) *httptest.Server {
	t.Helper()
	return httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, rq *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		switch rq.URL.Path {
		case "/bind":
			r.bind++
			w.WriteHeader(http.StatusOK)
			json.NewEncoder(w).Encode(s2s.ICCEBindResponse{
				KeyID: "icce-e2e-key-001", VehiclePubKey: "icce-vpk",
				SharedSecret: "icce-ss", Status: "ACTIVE",
			})
		case "/unbind":
			r.unbind++
			w.WriteHeader(http.StatusOK)
			json.NewEncoder(w).Encode(map[string]string{"status": "ok"})
		case "/revoke":
			r.revoke++
			w.WriteHeader(http.StatusOK)
			json.NewEncoder(w).Encode(map[string]string{"status": "ok"})
		case "/share":
			r.share++
			w.WriteHeader(http.StatusOK)
			json.NewEncoder(w).Encode(s2s.ICCEShareResponse{
				ShareID: "icce-e2e-share-001", ShareCode: "999999",
			})
		case "/share/accept":
			r.acceptShare++
			w.WriteHeader(http.StatusOK)
			json.NewEncoder(w).Encode(s2s.ICCEBindResponse{
				KeyID: "icce-e2e-friend-key-001", VehiclePubKey: "icce-vpk",
				Status: "ACTIVE",
			})
		default:
			t.Logf("unexpected ICCE S2S path: %s", rq.URL.Path)
			w.WriteHeader(http.StatusNotFound)
		}
	}))
}

func mkICCEAdapter(mockURL, vendor string) *adapter.ICCEAdapter {
	ep := s2s.DefaultICCEConfig()
	ep.BaseURL = mockURL
	ep.RetryCount = 0
	ep.RetryWait = 1 * time.Millisecond
	client := s2s.NewICCEClient(vendor, ep, zap.NewNop())
	return adapter.NewICCEAdapterWithClient(vendor, zap.NewNop(), client)
}

func newBrokenICCEServer() *httptest.Server {
	return httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusInternalServerError)
		json.NewEncoder(w).Encode(s2s.ICCEAPIError{Code: 50001, Message: "internal error"})
	}))
}

// TestE2E13_ICCEFullShareViaS2S 测试 ICCE 完整 S2S 链路
func TestE2E13_ICCEFullShareViaS2S(t *testing.T) {
	report := helpers.NewTestReport("E2E-13 ICCE S2S 链路验证")
	rec := &icceS2SRecorder{}
	mockSrv := newICCES2SMock(t, rec)
	defer mockSrv.Close()

	ctx := context.Background()
	ad := mkICCEAdapter(mockSrv.URL, "huawei")

	// ── 1: ShareKey → /share ──
	t.Run("E2E-13-01: ShareKey→/share", func(t *testing.T) {
		start := time.Now()
		resp, err := ad.ShareKey(ctx, &pb.CreateShareRequest{
			KeyId: "key-001", FromUserId: "owner-001",
		})
		if err != nil {
			t.Fatalf("ShareKey failed: %v", err)
		}
		if resp.ShareId != "icce-e2e-share-001" {
			t.Errorf("expected icce-e2e-share-001, got %q", resp.ShareId)
		}
		if resp.ShareCode != "999999" {
			t.Errorf("expected 999999, got %q", resp.ShareCode)
		}
		if rec.share != 1 {
			t.Errorf("/share called %d times, want 1", rec.share)
		}
		report.Record("E2E-13-01: ShareKey→/share", true, time.Since(start), "", "E2E-13", "ICCE")
	})

	// ── 2: AcceptShare → /share/accept ──
	t.Run("E2E-13-02: AcceptShare→/share/accept", func(t *testing.T) {
		start := time.Now()
		resp, err := ad.AcceptShare(ctx, &pb.AcceptShareRequest{
			ShareCode: "999999", DeviceId: "friend-001", UserId: "friend-001",
		})
		if err != nil {
			t.Fatalf("AcceptShare failed: %v", err)
		}
		if resp.Key.KeyId != "icce-e2e-friend-key-001" {
			t.Errorf("expected icce-e2e-friend-key-001, got %q", resp.Key.KeyId)
		}
		if resp.Key.KeyType != pb.KeyType_FRIEND {
			t.Errorf("expected FRIEND, got %v", resp.Key.KeyType)
		}
		if rec.acceptShare != 1 {
			t.Errorf("/share/accept called %d times, want 1", rec.acceptShare)
		}
		report.Record("E2E-13-02: AcceptShare→/share/accept", true, time.Since(start), "", "E2E-13", "ICCE")
	})

	// ── 3: BindKey → /bind ──
	t.Run("E2E-13-03: BindKey→/bind", func(t *testing.T) {
		start := time.Now()
		resp, err := ad.BindKey(ctx, &pb.BindKeyRequest{
			VehicleId: "VH-001", DeviceId: "DEV-001", UserId: "owner-001",
			DevicePubkey: make([]byte, 64), KeyType: pb.KeyType_OWNER,
		})
		if err != nil {
			t.Fatalf("BindKey failed: %v", err)
		}
		if resp.Key.KeyId != "icce-e2e-key-001" {
			t.Errorf("expected icce-e2e-key-001, got %q", resp.Key.KeyId)
		}
		if rec.bind != 1 {
			t.Errorf("/bind called %d times, want 1", rec.bind)
		}
		report.Record("E2E-13-03: BindKey→/bind", true, time.Since(start), "", "E2E-13", "ICCE")
	})

	// ── 4: UnbindKey → /unbind ──
	t.Run("E2E-13-04: UnbindKey→/unbind", func(t *testing.T) {
		start := time.Now()
		if err := ad.UnbindKey(ctx, "icce-e2e-key-001"); err != nil {
			t.Fatalf("UnbindKey failed: %v", err)
		}
		if rec.unbind < 1 {
			t.Error("/unbind not called")
		}
		report.Record("E2E-13-04: UnbindKey→/unbind", true, time.Since(start), "", "E2E-13", "ICCE")
	})

	// ── 5: RevokeNotify → /revoke ──
	t.Run("E2E-13-05: RevokeNotify→/revoke", func(t *testing.T) {
		start := time.Now()
		if err := ad.RevokeNotify(ctx, "icce-e2e-key-001", "stolen"); err != nil {
			t.Fatalf("RevokeNotify failed: %v", err)
		}
		if rec.revoke < 1 {
			t.Error("/revoke not called")
		}
		report.Record("E2E-13-05: RevokeNotify→/revoke", true, time.Since(start), "", "E2E-13", "ICCE")
	})

	// ── 6: S2S 故障 → graceful degradation ──
	t.Run("E2E-13-06: S2S故障降级", func(t *testing.T) {
		start := time.Now()
		failSrv := newBrokenICCEServer()
		defer failSrv.Close()

		failAd := mkICCEAdapter(failSrv.URL, "huawei")
		resp, err := failAd.BindKey(ctx, &pb.BindKeyRequest{
			VehicleId: "VH-FAIL-001", DeviceId: "DEV-FAIL-001", UserId: "u-fail",
			DevicePubkey: make([]byte, 64), KeyType: pb.KeyType_OWNER,
		})
		if err != nil {
			t.Fatalf("BindKey should degrade gracefully: %v", err)
		}
		if resp.Key.KeyId == "" || resp.Key.Protocol != pb.Protocol_ICCE {
			t.Errorf("bad degradation: id=%q proto=%v", resp.Key.KeyId, resp.Key.Protocol)
		}
		report.Record("E2E-13-06: S2S故障降级", true, time.Since(start), "", "E2E-13", "ICCE")
	})
}
