package gateway

import (
	"fmt"
	"net/http"
	"net/http/httptest"
	"sync"
	"sync/atomic"
	"testing"
	"time"
)

// ── JWKS kid 未命中负缓存 / 刷新冷却 (评审 MINOR #4 防放大) ───────────────────
//
// 攻击模型: 恶意令牌携带随机 kid, 若每次 kid 未命中都触发远端 JWKS 拉取,
// 攻击者可用极低成本打爆 OEM JWKS 端点 (放大攻击)。
// 防御: ① oem 级刷新冷却 — 拉取后 30s 内任何 kid 未命中直接拒绝, 不再拉取;
//       ② kid 级负缓存 — 同一 kid 在冷却期内重复出现直接拒绝。

// newCountingJWKSServer 启动一个返回固定 JWKS 文档的 httptest server, 并统计拉取次数
func newCountingJWKSServer(t *testing.T, doc string) (*httptest.Server, *int32) {
	t.Helper()
	var count int32
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		atomic.AddInt32(&count, 1)
		w.Header().Set("Content-Type", "application/json")
		fmt.Fprint(w, doc)
	}))
	t.Cleanup(srv.Close)
	return srv, &count
}

// TestOEMJWKS_KnownKid_CachedNoRefetch 已知 kid: 缓存命中, 不重复拉取
func TestOEMJWKS_KnownKid_CachedNoRefetch(t *testing.T) {
	priv, doc := rsaKeyAndDoc(t, "real-kid")
	srv, count := newCountingJWKSServer(t, doc)

	g := newTestGateway()
	g.WithOEMJWKS(map[string]string{"oemA": srv.URL})
	g.oemJWKS.missCooldown = 50 * time.Millisecond

	tok := mintRS256Token(priv, "real-kid", "oemA", "driver-1", time.Now().Add(time.Hour))
	if _, _, err := g.validateToken(tok); err != nil {
		t.Fatalf("first validation of known kid failed: %v", err)
	}
	if _, _, err := g.validateToken(tok); err != nil {
		t.Fatalf("second validation of known kid failed: %v", err)
	}
	if got := atomic.LoadInt32(count); got != 1 {
		t.Fatalf("expected exactly 1 JWKS fetch for cached known kid, got %d", got)
	}
}

// TestOEMJWKS_KidMiss_CooldownSuppressesRefetch 未知 kid: 首次拉取后进入冷却,
// 冷却期内同一 kid 与随机新 kid 均不再触发拉取 (防放大核心)
func TestOEMJWKS_KidMiss_CooldownSuppressesRefetch(t *testing.T) {
	priv, doc := rsaKeyAndDoc(t, "real-kid")
	srv, count := newCountingJWKSServer(t, doc)

	g := newTestGateway()
	g.WithOEMJWKS(map[string]string{"oemA": srv.URL})
	g.oemJWKS.missCooldown = 50 * time.Millisecond

	// 第一次未知 kid → 触发一次拉取后被拒绝
	tok1 := mintRS256Token(priv, "evil-kid-1", "oemA", "driver-1", time.Now().Add(time.Hour))
	if _, _, err := g.validateToken(tok1); err == nil {
		t.Fatal("expected unknown kid rejected")
	}
	if got := atomic.LoadInt32(count); got != 1 {
		t.Fatalf("expected 1 fetch after first miss, got %d", got)
	}

	// 冷却期内: 同一 kid 再次出现 → 负缓存生效, 不拉取
	if _, _, err := g.validateToken(tok1); err == nil {
		t.Fatal("expected unknown kid rejected during cooldown")
	}
	// 冷却期内: 随机新 kid → oem 级刷新冷却生效, 不拉取
	tok2 := mintRS256Token(priv, "evil-kid-2", "oemA", "driver-1", time.Now().Add(time.Hour))
	if _, _, err := g.validateToken(tok2); err == nil {
		t.Fatal("expected random new kid rejected during cooldown")
	}
	if got := atomic.LoadInt32(count); got != 1 {
		t.Fatalf("expected still 1 fetch during cooldown (no amplification), got %d", got)
	}
}

// TestOEMJWKS_KidMiss_RefetchAfterCooldown 冷却期过后: 允许再次拉取刷新
func TestOEMJWKS_KidMiss_RefetchAfterCooldown(t *testing.T) {
	priv, doc := rsaKeyAndDoc(t, "real-kid")
	srv, count := newCountingJWKSServer(t, doc)

	g := newTestGateway()
	g.WithOEMJWKS(map[string]string{"oemA": srv.URL})
	g.oemJWKS.missCooldown = 50 * time.Millisecond

	tok := mintRS256Token(priv, "evil-kid-1", "oemA", "driver-1", time.Now().Add(time.Hour))
	if _, _, err := g.validateToken(tok); err == nil {
		t.Fatal("expected unknown kid rejected")
	}
	if got := atomic.LoadInt32(count); got != 1 {
		t.Fatalf("expected 1 fetch, got %d", got)
	}

	// 等待冷却期结束 → 再次未命中允许重新拉取
	time.Sleep(70 * time.Millisecond)
	if _, _, err := g.validateToken(tok); err == nil {
		t.Fatal("expected unknown kid rejected after cooldown")
	}
	if got := atomic.LoadInt32(count); got != 2 {
		t.Fatalf("expected refetch after cooldown (2 total), got %d", got)
	}
}

// TestOEMJWKS_KidMiss_Concurrent 并发安全: 多 goroutine 同时验证同一未知 kid,
// 单飞 + 负缓存保证只触发一次拉取, 且无数据竞争 (-race 验证)
func TestOEMJWKS_KidMiss_Concurrent(t *testing.T) {
	priv, doc := rsaKeyAndDoc(t, "real-kid")
	srv, count := newCountingJWKSServer(t, doc)

	g := newTestGateway()
	g.WithOEMJWKS(map[string]string{"oemA": srv.URL})
	g.oemJWKS.missCooldown = 50 * time.Millisecond

	const goroutines = 16
	var wg sync.WaitGroup
	rejected := int32(0)
	for i := 0; i < goroutines; i++ {
		wg.Add(1)
		go func(i int) {
			defer wg.Done()
			tok := mintRS256Token(priv, "evil-kid-"+fmt.Sprint(i), "oemA", "driver-1", time.Now().Add(time.Hour))
			if _, _, err := g.validateToken(tok); err != nil {
				atomic.AddInt32(&rejected, 1)
			}
		}(i)
	}
	wg.Wait()

	if got := atomic.LoadInt32(&rejected); got != goroutines {
		t.Fatalf("expected all %d concurrent requests rejected, got %d", goroutines, got)
	}
	// 16 个不同随机 kid 并发 → 单飞保证并发窗口内只拉取 1 次 (或极少数竞态下的少量)
	if got := atomic.LoadInt32(count); got > 2 {
		t.Fatalf("expected at most 2 fetches under concurrency (singleflight+cooldown), got %d", got)
	}
}
