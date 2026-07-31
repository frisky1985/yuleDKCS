package gateway

import (
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rand"
	"crypto/rsa"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"math/big"
	"net/http"
	"net/http/httptest"
	"sync"
	"testing"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/golang-jwt/jwt/v5"
)

// ── 测试辅助: RSA/EC 密钥对 + JWKS 文档 + 令牌签发 ─────────────────────────

func rsaKeyAndDoc(t *testing.T, kid string) (*rsa.PrivateKey, string) {
	t.Helper()
	priv, err := rsa.GenerateKey(rand.Reader, 2048)
	if err != nil {
		t.Fatalf("failed to generate RSA key: %v", err)
	}
	n := base64.RawURLEncoding.EncodeToString(priv.PublicKey.N.Bytes())
	e := base64.RawURLEncoding.EncodeToString(big.NewInt(int64(priv.PublicKey.E)).Bytes())
	doc := fmt.Sprintf(`{"keys":[{"kty":"RSA","kid":%q,"use":"sig","alg":"RS256","n":%q,"e":%q}]}`, kid, n, e)
	return priv, doc
}

func ecKeyAndDoc(t *testing.T, kid string) (*ecdsa.PrivateKey, string) {
	t.Helper()
	priv, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	if err != nil {
		t.Fatalf("failed to generate EC key: %v", err)
	}
	x := base64.RawURLEncoding.EncodeToString(priv.PublicKey.X.Bytes())
	y := base64.RawURLEncoding.EncodeToString(priv.PublicKey.Y.Bytes())
	doc := fmt.Sprintf(`{"keys":[{"kty":"EC","kid":%q,"use":"sig","crv":"P-256","x":%q,"y":%q}]}`, kid, x, y)
	return priv, doc
}

// mintRS256Token 签发 RS256 令牌 (iss/sub/kid/exp)
func mintRS256Token(priv *rsa.PrivateKey, kid, iss, sub string, exp time.Time) string {
	tok := jwt.NewWithClaims(jwt.SigningMethodRS256, jwt.MapClaims{
		"iss": iss,
		"sub": sub,
		"exp": exp.Unix(),
	})
	tok.Header["kid"] = kid
	s, _ := tok.SignedString(priv)
	return s
}

// mintES256Token 签发 ES256 令牌 (iss/sub/kid/exp)
func mintES256Token(priv *ecdsa.PrivateKey, kid, iss, sub string, exp time.Time) string {
	tok := jwt.NewWithClaims(jwt.SigningMethodES256, jwt.MapClaims{
		"iss": iss,
		"sub": sub,
		"exp": exp.Unix(),
	})
	tok.Header["kid"] = kid
	s, _ := tok.SignedString(priv)
	return s
}

// mintHS256Token 用网关密钥签发 HS256 令牌 (用于算法混淆攻击测试)
func mintHS256Token(g *RESTGateway, iss, sub string, exp time.Time) string {
	tok := jwt.NewWithClaims(jwt.SigningMethodHS256, jwt.MapClaims{
		"iss": iss,
		"sub": sub,
		"exp": exp.Unix(),
	})
	s, _ := tok.SignedString([]byte(g.jwtSecret))
	return s
}

// newJWKSServer 启动一个返回固定 JWKS 文档的 httptest server
func newJWKSServer(t *testing.T, doc string) *httptest.Server {
	t.Helper()
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		fmt.Fprint(w, doc)
	}))
	t.Cleanup(srv.Close)
	return srv
}

// ── RSA JWKS 验证 ──────────────────────────────────────────────────────────

// TestOEMJWKS_RSA_Accept RS256 OEM 令牌被接受, user_id 带 oem 命名空间
func TestOEMJWKS_RSA_Accept(t *testing.T) {
	priv, doc := rsaKeyAndDoc(t, "oem-kid-1")
	srv := newJWKSServer(t, doc)

	g := newTestGateway()
	g.WithOEMJWKS(map[string]string{"oemA": srv.URL})

	tok := mintRS256Token(priv, "oem-kid-1", "oemA", "driver-1", time.Now().Add(time.Hour))
	uid, role, err := g.validateToken(tok)
	if err != nil {
		t.Fatalf("expected OEM token accepted, got error: %v", err)
	}
	if uid != "oem:oemA:driver-1" {
		t.Fatalf("expected user_id 'oem:oemA:driver-1', got %q", uid)
	}
	if role != "user" {
		t.Fatalf("expected role 'user', got %q", role)
	}
}

// TestOEMJWKS_SubFallback sub 缺失时回退 "unknown"
func TestOEMJWKS_SubFallback(t *testing.T) {
	priv, doc := rsaKeyAndDoc(t, "oem-kid-1")
	srv := newJWKSServer(t, doc)

	g := newTestGateway()
	g.WithOEMJWKS(map[string]string{"oemA": srv.URL})

	tok := jwt.NewWithClaims(jwt.SigningMethodRS256, jwt.MapClaims{
		"iss": "oemA",
		"exp": time.Now().Add(time.Hour).Unix(),
	})
	tok.Header["kid"] = "oem-kid-1"
	s, _ := tok.SignedString(priv)

	// [P1-2 评审加固] 缺失 sub 现在被拒绝 (原行为: 回退 oem:oemA:unknown, 身份碰撞风险)
	if _, _, err := g.validateToken(s); err == nil {
		t.Fatal("expected OEM token without sub rejected (was: sub fallback to 'unknown')")
	}
}

// TestOEMJWKS_WrongIssuer 未配置的 iss → 拒绝
func TestOEMJWKS_WrongIssuer(t *testing.T) {
	priv, doc := rsaKeyAndDoc(t, "oem-kid-1")
	srv := newJWKSServer(t, doc)

	g := newTestGateway()
	g.WithOEMJWKS(map[string]string{"oemA": srv.URL})

	tok := mintRS256Token(priv, "oem-kid-1", "oemB", "driver-1", time.Now().Add(time.Hour))
	_, _, err := g.validateToken(tok)
	if err == nil {
		t.Fatal("expected rejection for unknown OEM issuer")
	}
}

// TestOEMJWKS_UnknownKid JWKS 中不存在的 kid → 拒绝
func TestOEMJWKS_UnknownKid(t *testing.T) {
	priv, doc := rsaKeyAndDoc(t, "oem-kid-1")
	srv := newJWKSServer(t, doc)

	g := newTestGateway()
	g.WithOEMJWKS(map[string]string{"oemA": srv.URL})

	tok := mintRS256Token(priv, "oem-kid-unknown", "oemA", "driver-1", time.Now().Add(time.Hour))
	_, _, err := g.validateToken(tok)
	if err == nil {
		t.Fatal("expected rejection for unknown kid")
	}
}

// TestOEMJWKS_MissingKid 令牌头缺少 kid → 拒绝
func TestOEMJWKS_MissingKid(t *testing.T) {
	priv, doc := rsaKeyAndDoc(t, "oem-kid-1")
	srv := newJWKSServer(t, doc)

	g := newTestGateway()
	g.WithOEMJWKS(map[string]string{"oemA": srv.URL})

	tok := jwt.NewWithClaims(jwt.SigningMethodRS256, jwt.MapClaims{
		"iss": "oemA",
		"sub": "driver-1",
		"exp": time.Now().Add(time.Hour).Unix(),
	})
	s, _ := tok.SignedString(priv)

	_, _, err := g.validateToken(s)
	if err == nil {
		t.Fatal("expected rejection for missing kid")
	}
}

// TestOEMJWKS_HS256AlgConfusion 算法混淆: HS256 签名的 OEM 令牌 → 拒绝
func TestOEMJWKS_HS256AlgConfusion(t *testing.T) {
	_, doc := rsaKeyAndDoc(t, "oem-kid-1")
	srv := newJWKSServer(t, doc)

	g := newTestGateway()
	g.WithOEMJWKS(map[string]string{"oemA": srv.URL})

	// 攻击者用已知的 JWT_SECRET 签发 iss=oemA 的 HS256 令牌
	tok := mintHS256Token(g, "oemA", "driver-1", time.Now().Add(time.Hour))
	_, _, err := g.validateToken(tok)
	if err == nil {
		t.Fatal("expected rejection for HS256 algorithm confusion attempt")
	}
}

// TestOEMJWKS_Expired 过期的 OEM 令牌 → 拒绝
func TestOEMJWKS_Expired(t *testing.T) {
	priv, doc := rsaKeyAndDoc(t, "oem-kid-1")
	srv := newJWKSServer(t, doc)

	g := newTestGateway()
	g.WithOEMJWKS(map[string]string{"oemA": srv.URL})

	tok := mintRS256Token(priv, "oem-kid-1", "oemA", "driver-1", time.Now().Add(-time.Hour))
	_, _, err := g.validateToken(tok)
	if err == nil {
		t.Fatal("expected rejection for expired OEM token")
	}
}

// TestOEMJWKS_NotConfigured 未配置任何 OEM JWKS 时 OEM 令牌 → 拒绝 (失败即关闭)
func TestOEMJWKS_NotConfigured(t *testing.T) {
	priv, _ := rsaKeyAndDoc(t, "oem-kid-1")

	g := newTestGateway() // 不调用 WithOEMJWKS

	tok := mintRS256Token(priv, "oem-kid-1", "oemA", "driver-1", time.Now().Add(time.Hour))
	_, _, err := g.validateToken(tok)
	if err == nil {
		t.Fatal("expected rejection when no OEM JWKS configured")
	}
}

// TestOEMJWKS_FetchFailureFailsClosed JWKS 端点返回错误 → 拒绝 (失败即关闭)
func TestOEMJWKS_FetchFailureFailsClosed(t *testing.T) {
	priv, _ := rsaKeyAndDoc(t, "oem-kid-1")
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		http.Error(w, "boom", http.StatusInternalServerError)
	}))
	t.Cleanup(srv.Close)

	g := newTestGateway()
	g.WithOEMJWKS(map[string]string{"oemA": srv.URL})

	tok := mintRS256Token(priv, "oem-kid-1", "oemA", "driver-1", time.Now().Add(time.Hour))
	_, _, err := g.validateToken(tok)
	if err == nil {
		t.Fatal("expected fail-closed on JWKS fetch failure")
	}
}

// TestOEMJWKS_InvalidJWKSBody JWKS 响应不是合法 JSON → 拒绝
func TestOEMJWKS_InvalidJWKSBody(t *testing.T) {
	priv, _ := rsaKeyAndDoc(t, "oem-kid-1")
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		fmt.Fprint(w, "not-json")
	}))
	t.Cleanup(srv.Close)

	g := newTestGateway()
	g.WithOEMJWKS(map[string]string{"oemA": srv.URL})

	tok := mintRS256Token(priv, "oem-kid-1", "oemA", "driver-1", time.Now().Add(time.Hour))
	_, _, err := g.validateToken(tok)
	if err == nil {
		t.Fatal("expected fail-closed on invalid JWKS body")
	}
}

// TestOEMJWKS_KeyRotation 密钥轮换: kid 未命中/缓存过期后自动刷新 JWKS
func TestOEMJWKS_KeyRotation(t *testing.T) {
	priv1, doc1 := rsaKeyAndDoc(t, "kid-1")
	priv2, doc2 := rsaKeyAndDoc(t, "kid-2")

	var mu sync.Mutex
	current := doc1
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		mu.Lock()
		defer mu.Unlock()
		fmt.Fprint(w, current)
	}))
	t.Cleanup(srv.Close)

	g := newTestGateway()
	g.WithOEMJWKS(map[string]string{"oemA": srv.URL})
	g.oemJWKS.cacheTTL = 50 * time.Millisecond

	// 第一次验证: 拉取 doc1, kid-1 通过
	tok1 := mintRS256Token(priv1, "kid-1", "oemA", "driver-1", time.Now().Add(time.Hour))
	uid, _, err := g.validateToken(tok1)
	if err != nil || uid != "oem:oemA:driver-1" {
		t.Fatalf("expected first token accepted, got uid=%q err=%v", uid, err)
	}

	// 轮换: 服务端仅保留 kid-2
	mu.Lock()
	current = doc2
	mu.Unlock()

	// 缓存过期后应重新拉取并接受 kid-2
	time.Sleep(70 * time.Millisecond)
	tok2 := mintRS256Token(priv2, "kid-2", "oemA", "driver-2", time.Now().Add(time.Hour))
	uid2, _, err := g.validateToken(tok2)
	if err != nil || uid2 != "oem:oemA:driver-2" {
		t.Fatalf("expected rotated token accepted after refresh, got uid=%q err=%v", uid2, err)
	}

	// 旧 kid-1 令牌在轮换后应被拒绝
	_, _, err = g.validateToken(tok1)
	if err == nil {
		t.Fatal("expected old kid to be rejected after key rotation")
	}
}

// ── EC JWKS 验证 ────────────────────────────────────────────────────────────

// TestOEMJWKS_EC_Accept ES256 OEM 令牌被接受
func TestOEMJWKS_EC_Accept(t *testing.T) {
	priv, doc := ecKeyAndDoc(t, "oem-ec-kid")
	srv := newJWKSServer(t, doc)

	g := newTestGateway()
	g.WithOEMJWKS(map[string]string{"oemA": srv.URL})

	tok := mintES256Token(priv, "oem-ec-kid", "oemA", "driver-ec", time.Now().Add(time.Hour))
	uid, role, err := g.validateToken(tok)
	if err != nil {
		t.Fatalf("expected ES256 OEM token accepted, got error: %v", err)
	}
	if uid != "oem:oemA:driver-ec" {
		t.Fatalf("expected user_id 'oem:oemA:driver-ec', got %q", uid)
	}
	if role != "user" {
		t.Fatalf("expected role 'user', got %q", role)
	}
}

// ── 管理端 (HS256) 与双轨边界 ────────────────────────────────────────────────

// TestAdminToken_AcceptedWithOEMConfigured 配置了 OEM JWKS 时管理端 HS256 令牌仍被接受
func TestAdminToken_AcceptedWithOEMConfigured(t *testing.T) {
	_, doc := rsaKeyAndDoc(t, "oem-kid-1")
	srv := newJWKSServer(t, doc)

	g := newTestGateway()
	g.WithOEMJWKS(map[string]string{"oemA": srv.URL})

	tok := issueTestToken(g, "admin-1", "admin")
	uid, role, err := g.validateToken(tok)
	if err != nil {
		t.Fatalf("expected admin token accepted, got error: %v", err)
	}
	if uid != "admin-1" || role != "admin" {
		t.Fatalf("expected (admin-1, admin), got (%q, %q)", uid, role)
	}
}

// TestToken_MissingIss 无 iss 声明的令牌 → 拒绝
func TestToken_MissingIss(t *testing.T) {
	g := newTestGateway()

	tok := jwt.NewWithClaims(jwt.SigningMethodHS256, jwt.MapClaims{
		"user_id": "user-1",
		"role":    "user",
		"exp":     time.Now().Add(time.Hour).Unix(),
	})
	s, _ := tok.SignedString([]byte(g.jwtSecret))

	_, _, err := g.validateToken(s)
	if err == nil {
		t.Fatal("expected rejection for token without iss claim")
	}
}

// TestOEMToken_AuthMiddlewareInjectsUser authMiddleware 端到端: OEM 令牌注入 user_id/user_role
func TestOEMToken_AuthMiddlewareInjectsUser(t *testing.T) {
	priv, doc := rsaKeyAndDoc(t, "oem-kid-1")
	srv := newJWKSServer(t, doc)

	g := newTestGateway()
	g.WithOEMJWKS(map[string]string{"oemA": srv.URL})

	r := gin.New()
	r.Use(g.authMiddleware())
	r.GET("/api/v1/me", func(c *gin.Context) {
		uid, _ := c.Get("user_id")
		role, _ := c.Get("user_role")
		c.JSON(200, gin.H{"user_id": uid, "role": role})
	})

	tok := mintRS256Token(priv, "oem-kid-1", "oemA", "driver-9", time.Now().Add(time.Hour))
	w := httptest.NewRecorder()
	req := httptest.NewRequest("GET", "/api/v1/me", nil)
	req.Header.Set("Authorization", "Bearer "+tok)
	r.ServeHTTP(w, req)

	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", w.Code, w.Body.String())
	}
	var body map[string]string
	json.Unmarshal(w.Body.Bytes(), &body)
	if body["user_id"] != "oem:oemA:driver-9" {
		t.Fatalf("expected user_id 'oem:oemA:driver-9', got %q", body["user_id"])
	}
	if body["role"] != "user" {
		t.Fatalf("expected role 'user', got %q", body["role"])
	}
}

// ── parseJWKSKeys 单元测试 ───────────────────────────────────────────────────

// TestParseJWKSKeys_FiltersNonSigning 跳过 enc/非 RSA/EC 密钥
func TestParseJWKSKeys_FiltersNonSigning(t *testing.T) {
	priv, _ := rsaKeyAndDoc(t, "sig-key")
	n := base64.RawURLEncoding.EncodeToString(priv.PublicKey.N.Bytes())
	e := base64.RawURLEncoding.EncodeToString(big.NewInt(int64(priv.PublicKey.E)).Bytes())

	body := fmt.Sprintf(`{"keys":[
		{"kty":"RSA","kid":"enc-key","use":"enc","n":%q,"e":%q},
		{"kty":"oct","kid":"sym-key","k":"c2VjcmV0"},
		{"kty":"RSA","kid":"sig-key","use":"sig","n":%q,"e":%q}
	]}`, n, e, n, e)

	keys, err := parseJWKSKeys([]byte(body))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if len(keys) != 1 {
		t.Fatalf("expected exactly 1 key, got %d", len(keys))
	}
	if _, ok := keys["sig-key"]; !ok {
		t.Fatal("expected sig-key to be parsed")
	}
}

// TestParseJWKSKeys_BadECPoint 无效 EC 曲线点 → 解析失败 (失败即关闭)
func TestParseJWKSKeys_BadECPoint(t *testing.T) {
	body := `{"keys":[{"kty":"EC","kid":"bad","use":"sig","crv":"P-256","x":"AAAA","y":"AAAA"}]}`
	_, err := parseJWKSKeys([]byte(body))
	if err == nil {
		t.Fatal("expected parse error for EC point not on curve")
	}
}

// ── 评审加固测试 (Evaluator MINOR 项) ─────────────────────────────────────────

// TestOEMJWKS_AlgNoneRejected alg=none 令牌被拒绝 (无签名算法)
func TestOEMJWKS_AlgNoneRejected(t *testing.T) {
	_, doc := rsaKeyAndDoc(t, "oem-kid-1")
	srv := newJWKSServer(t, doc)

	g := newTestGateway()
	g.WithOEMJWKS(map[string]string{"oemA": srv.URL})

	tok := jwt.NewWithClaims(jwt.SigningMethodNone, jwt.MapClaims{
		"iss": "oemA",
		"sub": "driver-1",
		"exp": time.Now().Add(time.Hour).Unix(),
	})
	s, err := tok.SignedString(jwt.UnsafeAllowNoneSignatureType)
	if err != nil {
		t.Fatalf("failed to mint alg=none token: %v", err)
	}
	if _, _, err := g.validateToken(s); err == nil {
		t.Fatal("expected alg=none token rejected")
	}
}

// TestAdminToken_ReverseConfusion RS256 令牌携带 iss=dkcs-admin → 拒绝 (管理轨道仅 HMAC)
func TestAdminToken_ReverseConfusion(t *testing.T) {
	priv, _ := rsaKeyAndDoc(t, "oem-kid-1")
	g := newTestGateway()
	g.WithJWTSecret("test-secret-key")

	tok := jwt.NewWithClaims(jwt.SigningMethodRS256, jwt.MapClaims{
		"iss":     adminIssuer,
		"user_id": "admin-1",
		"role":    "admin",
		"exp":     time.Now().Add(time.Hour).Unix(),
	})
	s, err := tok.SignedString(priv)
	if err != nil {
		t.Fatalf("failed to mint RS256 token: %v", err)
	}
	if _, _, err := g.validateToken(s); err == nil {
		t.Fatal("expected RS256 token with iss=dkcs-admin rejected")
	}
}

// TestOEMJWKS_RoleForcedToUser OEM 令牌携带 role=admin 声明 → role 仍强制为 user
func TestOEMJWKS_RoleForcedToUser(t *testing.T) {
	priv, doc := rsaKeyAndDoc(t, "oem-kid-1")
	srv := newJWKSServer(t, doc)

	g := newTestGateway()
	g.WithOEMJWKS(map[string]string{"oemA": srv.URL})

	tok := jwt.NewWithClaims(jwt.SigningMethodRS256, jwt.MapClaims{
		"iss":  "oemA",
		"sub":  "driver-1",
		"role": "admin",
		"exp":  time.Now().Add(time.Hour).Unix(),
	})
	tok.Header["kid"] = "oem-kid-1"
	s, err := tok.SignedString(priv)
	if err != nil {
		t.Fatalf("failed to mint token: %v", err)
	}
	uid, role, err := g.validateToken(s)
	if err != nil {
		t.Fatalf("expected OEM token accepted: %v", err)
	}
	if uid != "oem:oemA:driver-1" {
		t.Fatalf("unexpected user_id %q", uid)
	}
	if role != "user" {
		t.Fatalf("expected role forced to 'user', got %q", role)
	}
}

// TestOEMJWKS_MissingSubRejected OEM 令牌缺失 sub → 拒绝 (防 oem:<iss>:unknown 身份碰撞)
func TestOEMJWKS_MissingSubRejected(t *testing.T) {
	priv, doc := rsaKeyAndDoc(t, "oem-kid-1")
	srv := newJWKSServer(t, doc)

	g := newTestGateway()
	g.WithOEMJWKS(map[string]string{"oemA": srv.URL})

	tok := jwt.NewWithClaims(jwt.SigningMethodRS256, jwt.MapClaims{
		"iss": "oemA",
		"exp": time.Now().Add(time.Hour).Unix(),
	})
	tok.Header["kid"] = "oem-kid-1"
	s, err := tok.SignedString(priv)
	if err != nil {
		t.Fatalf("failed to mint token: %v", err)
	}
	if _, _, err := g.validateToken(s); err == nil {
		t.Fatal("expected OEM token without sub rejected")
	}
}

// TestOEMJWKS_MissingExpRejected OEM 令牌缺失 exp → 拒绝 (无过期时间永不过期)
func TestOEMJWKS_MissingExpRejected(t *testing.T) {
	priv, doc := rsaKeyAndDoc(t, "oem-kid-1")
	srv := newJWKSServer(t, doc)

	g := newTestGateway()
	g.WithOEMJWKS(map[string]string{"oemA": srv.URL})

	tok := jwt.NewWithClaims(jwt.SigningMethodRS256, jwt.MapClaims{
		"iss": "oemA",
		"sub": "driver-1",
	})
	tok.Header["kid"] = "oem-kid-1"
	s, err := tok.SignedString(priv)
	if err != nil {
		t.Fatalf("failed to mint token: %v", err)
	}
	if _, _, err := g.validateToken(s); err == nil {
		t.Fatal("expected OEM token without exp rejected")
	}
}

// TestParseJWKSKeys_RSAExponentOne e=1 公钥 → 拒绝 (签名可伪造: sig = EM mod N)
func TestParseJWKSKeys_RSAExponentOne(t *testing.T) {
	priv, _ := rsaKeyAndDoc(t, "oem-kid-1")
	n := base64.RawURLEncoding.EncodeToString(priv.PublicKey.N.Bytes())
	e := base64.RawURLEncoding.EncodeToString(big.NewInt(1).Bytes()) // e=1

	body := fmt.Sprintf(`{"keys":[{"kty":"RSA","kid":"bad","use":"sig","n":%q,"e":%q}]}`, n, e)
	if _, err := parseJWKSKeys([]byte(body)); err == nil {
		t.Fatal("expected RSA e=1 key rejected")
	}
}

// TestParseJWKSKeys_RSAWeakModulus 1024-bit 模数 → 拒绝 (可被因式分解)
func TestParseJWKSKeys_RSAWeakModulus(t *testing.T) {
	priv, err := rsa.GenerateKey(rand.Reader, 1024)
	if err != nil {
		t.Fatalf("failed to generate 1024-bit RSA key: %v", err)
	}
	n := base64.RawURLEncoding.EncodeToString(priv.PublicKey.N.Bytes())
	e := base64.RawURLEncoding.EncodeToString(big.NewInt(int64(priv.PublicKey.E)).Bytes())

	body := fmt.Sprintf(`{"keys":[{"kty":"RSA","kid":"weak","use":"sig","n":%q,"e":%q}]}`, n, e)
	if _, err := parseJWKSKeys([]byte(body)); err == nil {
		t.Fatal("expected 1024-bit RSA modulus rejected")
	}
}
