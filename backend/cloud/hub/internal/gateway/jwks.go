package gateway

import (
	"crypto"
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rsa"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"io"
	"math/big"
	"net/http"
	"sync"
	"time"

	"github.com/golang-jwt/jwt/v5"
	"go.uber.org/zap"
)

// ── OEM JWKS 验证器 ──────────────────────────────────────────────────────────
//
// 设备厂 (OEM) 令牌验证: 从 OEM 的 JWKS 端点拉取公钥, 按 kid 索引并缓存,
// 用于验证 OEM 签发的 RS256/ES256 JWT。拉取/解析失败时返回错误 — 失败即关闭,
// 调用方必须拒绝该令牌。

const (
	// jwksFetchTimeout JWKS 拉取超时
	jwksFetchTimeout = 10 * time.Second
	// jwksCacheTTL JWKS 缓存有效期
	jwksCacheTTL = time.Hour
	// jwksMaxBodyBytes JWKS 响应体大小上限 (防滥用)
	jwksMaxBodyBytes = 1 << 20 // 1 MiB
	// jwksMinRSABits RSA 公钥最小模长 (RFC 7518 建议 ≥2048)
	jwksMinRSABits = 2048
	// jwksMissCooldown kid 未命中负缓存冷却期 (防随机 kid 触发重复拉取的放大攻击)
	jwksMissCooldown = 30 * time.Second
	// jwksMissCacheMax 单个 OEM 负缓存条目上限 (防恶意随机 kid 撑爆内存)
	jwksMissCacheMax = 1024
)

// jwksDocument JWKS 文档结构 (RFC 7517)
type jwksDocument struct {
	Keys []jwkKey `json:"keys"`
}

// jwkKey JWK 公钥字段 (仅解析 RSA/EC 所需字段)
type jwkKey struct {
	Kty string `json:"kty"`
	Kid string `json:"kid"`
	Use string `json:"use"`
	Alg string `json:"alg"`
	N   string `json:"n"`
	E   string `json:"e"`
	Crv string `json:"crv"`
	X   string `json:"x"`
	Y   string `json:"y"`
}

// jwksCacheEntry 单个 OEM 的缓存条目
type jwksCacheEntry struct {
	keys      map[string]crypto.PublicKey // kid -> 公钥
	fetchedAt time.Time
}

// jwksInflight 单飞: 同一 oem 并发验证时只发起一次拉取
type jwksInflight struct {
	done chan struct{}
	keys map[string]crypto.PublicKey
	err  error
}

// oemJWKSVerifier 按 oem_id 从远端 JWKS 端点拉取公钥并缓存 (TTL 1h)。
// 线程安全: 所有公开方法通过互斥锁保护内部状态。
type oemJWKSVerifier struct {
	mu       sync.Mutex
	logger   *zap.Logger
	client   *http.Client
	urls     map[string]string // oem_id -> JWKS URL
	cache    map[string]*jwksCacheEntry
	inflight map[string]*jwksInflight
	cacheTTL time.Duration // 可覆盖 (测试用), 默认 jwksCacheTTL
	// missCache 负缓存: oem_id -> kid -> 首次未命中时间。
	// 恶意令牌携带随机 kid 时, 冷却期内同一 kid 不再触发远端拉取 (防放大攻击)。
	missCache    map[string]map[string]time.Time
	missCooldown time.Duration // 可覆盖 (测试用), 默认 jwksMissCooldown
}

// newOEMJWKSVerifier 创建 OEM JWKS 验证器。
// urls: oem_id -> JWKS URL 映射。
func newOEMJWKSVerifier(logger *zap.Logger, urls map[string]string) *oemJWKSVerifier {
	return &oemJWKSVerifier{
		logger:       logger,
		client:       &http.Client{Timeout: jwksFetchTimeout},
		urls:         urls,
		cache:        make(map[string]*jwksCacheEntry),
		inflight:     make(map[string]*jwksInflight),
		cacheTTL:     jwksCacheTTL,
		missCache:    make(map[string]map[string]time.Time),
		missCooldown: jwksMissCooldown,
	}
}

// keyfunc 返回 jwt.Keyfunc — 仅当签名算法为 RS256/ES256 族且 kid 能在该 OEM
// 的 JWKS 中找到时返回公钥。防算法混淆: HS256/none 等算法一律拒绝。
func (v *oemJWKSVerifier) keyfunc(oemID string) jwt.Keyfunc {
	return func(token *jwt.Token) (interface{}, error) {
		switch token.Method.(type) {
		case *jwt.SigningMethodRSA, *jwt.SigningMethodECDSA:
			// 允许 RS256/RS384/RS512 与 ES256/ES384/ES512
		default:
			return nil, fmt.Errorf("unexpected signing method for OEM token: %v", token.Header["alg"])
		}
		kid, _ := token.Header["kid"].(string)
		if kid == "" {
			return nil, fmt.Errorf("missing kid in OEM token header")
		}
		return v.keyFor(oemID, kid)
	}
}

// keyFor 返回指定 oem 的 JWKS 中 kid 对应的公钥。
// 缓存未命中/过期时触发拉取; 拉取失败返回错误 (失败即关闭)。
// 负缓存: 冷却期内未命中过的 kid 直接拒绝, 不再重复拉取远端 JWKS (防放大攻击)。
func (v *oemJWKSVerifier) keyFor(oemID, kid string) (crypto.PublicKey, error) {
	v.mu.Lock()
	// 快速路径: 缓存有效且包含 kid
	if entry, ok := v.cache[oemID]; ok && time.Since(entry.fetchedAt) < v.cacheTTL {
		if key, found := entry.keys[kid]; found {
			v.mu.Unlock()
			return key, nil
		}
		// kid 未命中 (密钥轮换场景) → 冷却检查, 命中任一冷却则直接拒绝:
		//   ① oem 级刷新冷却: 30s 内刚拉取过 → 随机新 kid 无法触发重复拉取 (防放大核心)
		//   ② kid 级负缓存: 同一 kid 冷却期内不重复拉取
		if time.Since(entry.fetchedAt) < v.missCooldown {
			v.mu.Unlock()
			return nil, fmt.Errorf("kid %q not found in JWKS for OEM %q (refresh cooldown %s)", kid, oemID, v.missCooldown)
		}
		if v.inMissCooldownLocked(oemID, kid) {
			v.mu.Unlock()
			return nil, fmt.Errorf("kid %q not found in JWKS for OEM %q (negative cache, %s cooldown)", kid, oemID, v.missCooldown)
		}
	}
	// 单飞: 已有拉取在进行中则等待结果
	if f, ok := v.inflight[oemID]; ok {
		v.mu.Unlock()
		<-f.done
		if f.err != nil {
			return nil, f.err
		}
		key, found := f.keys[kid]
		if !found {
			return nil, fmt.Errorf("kid %q not found in JWKS for OEM %q", kid, oemID)
		}
		return key, nil
	}
	f := &jwksInflight{done: make(chan struct{})}
	v.inflight[oemID] = f
	v.mu.Unlock()

	// 锁外执行 HTTP 拉取, 避免长时间持锁
	keys, err := v.fetch(oemID)

	v.mu.Lock()
	delete(v.inflight, oemID)
	if err != nil {
		f.err = err
	} else {
		f.keys = keys
		v.cache[oemID] = &jwksCacheEntry{keys: keys, fetchedAt: time.Now()}
	}
	v.mu.Unlock()
	close(f.done)

	if err != nil {
		return nil, err
	}
	key, found := keys[kid]
	if !found {
		// 拉取成功但 kid 仍不存在 → 记录负缓存, 冷却期内不再重复拉取
		v.recordMiss(oemID, kid)
		return nil, fmt.Errorf("kid %q not found in JWKS for OEM %q", kid, oemID)
	}
	return key, nil
}

// inMissCooldownLocked 判断 (oemID, kid) 是否处于负缓存冷却期内。
// 调用方必须持有 v.mu。
func (v *oemJWKSVerifier) inMissCooldownLocked(oemID, kid string) bool {
	entry, ok := v.missCache[oemID]
	if !ok {
		return false
	}
	missedAt, ok := entry[kid]
	if !ok {
		return false
	}
	// 冷却期已过 → 惰性清理该条目并放行
	if time.Since(missedAt) >= v.missCooldown {
		delete(entry, kid)
		if len(entry) == 0 {
			delete(v.missCache, oemID)
		}
		return false
	}
	return true
}

// recordMiss 记录 (oemID, kid) 的未命中时间, 并清理过期条目/限制缓存大小。
// 调用方不必持有 v.mu (内部加锁)。
func (v *oemJWKSVerifier) recordMiss(oemID, kid string) {
	v.mu.Lock()
	defer v.mu.Unlock()
	// 清理该 OEM 已过期的负缓存条目
	if entry, ok := v.missCache[oemID]; ok {
		for k, t := range entry {
			if time.Since(t) >= v.missCooldown {
				delete(entry, k)
			}
		}
		if len(entry) == 0 {
			delete(v.missCache, oemID)
		}
	}
	entry := v.missCache[oemID]
	if entry == nil {
		entry = make(map[string]time.Time)
		v.missCache[oemID] = entry
	}
	// 上限保护: 恶意随机 kid 撑满后整体清空该 OEM 的负缓存 (防内存无限增长)
	if len(entry) >= jwksMissCacheMax {
		entry = make(map[string]time.Time)
		v.missCache[oemID] = entry
	}
	entry[kid] = time.Now()
}

// fetch 从远端拉取并解析 oem 的 JWKS, 返回 kid -> 公钥 映射。
func (v *oemJWKSVerifier) fetch(oemID string) (map[string]crypto.PublicKey, error) {
	url, ok := v.urls[oemID]
	if !ok {
		return nil, fmt.Errorf("OEM %q has no JWKS URL configured", oemID)
	}
	v.logger.Debug("Fetching OEM JWKS", zap.String("oem_id", oemID), zap.String("url", url))

	resp, err := v.client.Get(url)
	if err != nil {
		return nil, fmt.Errorf("failed to fetch JWKS for OEM %q: %w", oemID, err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("JWKS endpoint for OEM %q returned status %d", oemID, resp.StatusCode)
	}
	body, err := io.ReadAll(io.LimitReader(resp.Body, jwksMaxBodyBytes))
	if err != nil {
		return nil, fmt.Errorf("failed to read JWKS response for OEM %q: %w", oemID, err)
	}
	keys, err := parseJWKSKeys(body)
	if err != nil {
		return nil, fmt.Errorf("failed to parse JWKS for OEM %q: %w", oemID, err)
	}
	if len(keys) == 0 {
		return nil, fmt.Errorf("JWKS for OEM %q contains no usable RSA/EC keys", oemID)
	}
	return keys, nil
}

// parseJWKSKeys 解析 JWKS JSON, 返回 kid -> 公钥 映射 (仅 RSA/EC 签名密钥)。
// 任一密钥解析失败即返回错误 — 失败即关闭。
func parseJWKSKeys(body []byte) (map[string]crypto.PublicKey, error) {
	var doc jwksDocument
	if err := json.Unmarshal(body, &doc); err != nil {
		return nil, fmt.Errorf("invalid JWKS JSON: %w", err)
	}
	keys := make(map[string]crypto.PublicKey, len(doc.Keys))
	for _, k := range doc.Keys {
		if k.Use == "enc" {
			continue // 仅接受签名密钥
		}
		switch k.Kty {
		case "RSA":
			pub, err := parseRSAJWK(k)
			if err != nil {
				return nil, err
			}
			keys[k.Kid] = pub
		case "EC":
			pub, err := parseECJWK(k)
			if err != nil {
				return nil, err
			}
			keys[k.Kid] = pub
		}
	}
	return keys, nil
}

// decodeJWKBase64 解码 JWK 的 base64url 字段 (兼容带/不带 padding)
func decodeJWKBase64(s string) ([]byte, error) {
	if b, err := base64.RawURLEncoding.DecodeString(s); err == nil {
		return b, nil
	}
	return base64.URLEncoding.DecodeString(s)
}

// parseRSAJWK 解析 RSA 公钥 (n, e)
func parseRSAJWK(k jwkKey) (*rsa.PublicKey, error) {
	if k.N == "" || k.E == "" {
		return nil, fmt.Errorf("RSA JWK missing n/e")
	}
	nBytes, err := decodeJWKBase64(k.N)
	if err != nil {
		return nil, fmt.Errorf("invalid RSA JWK n: %w", err)
	}
	eBytes, err := decodeJWKBase64(k.E)
	if err != nil {
		return nil, fmt.Errorf("invalid RSA JWK e: %w", err)
	}
	n := new(big.Int).SetBytes(nBytes)
	if n.Sign() <= 0 {
		return nil, fmt.Errorf("invalid RSA modulus")
	}
	// [P1-2] 最小模长防御 (RFC 7518 建议 ≥2048; 小模数可被因式分解)
	if n.BitLen() < jwksMinRSABits {
		return nil, fmt.Errorf("RSA modulus too small: %d bits (min %d)", n.BitLen(), jwksMinRSABits)
	}
	e := new(big.Int).SetBytes(eBytes)
	// [P1-2] 指数校验: e=1 时签名可伪造 (sig = EM mod N); 偶数指数无效。
	// 要求 e ≥ 3 且为奇数 (标准为 65537)。
	if !e.IsInt64() || e.Int64() < 3 || e.Int64()%2 == 0 || e.Int64() > int64(^uint(0)>>1) {
		return nil, fmt.Errorf("invalid RSA exponent")
	}
	return &rsa.PublicKey{N: n, E: int(e.Int64())}, nil
}

// parseECJWK 解析 EC 公钥 (crv, x, y), 并校验点在曲线上 (防无效曲线点)
func parseECJWK(k jwkKey) (*ecdsa.PublicKey, error) {
	var curve elliptic.Curve
	switch k.Crv {
	case "P-256":
		curve = elliptic.P256()
	case "P-384":
		curve = elliptic.P384()
	case "P-521":
		curve = elliptic.P521()
	default:
		return nil, fmt.Errorf("unsupported EC curve %q", k.Crv)
	}
	xBytes, err := decodeJWKBase64(k.X)
	if err != nil {
		return nil, fmt.Errorf("invalid EC JWK x: %w", err)
	}
	yBytes, err := decodeJWKBase64(k.Y)
	if err != nil {
		return nil, fmt.Errorf("invalid EC JWK y: %w", err)
	}
	x := new(big.Int).SetBytes(xBytes)
	y := new(big.Int).SetBytes(yBytes)
	if !curve.IsOnCurve(x, y) {
		return nil, fmt.Errorf("EC JWK point is not on curve %q", k.Crv)
	}
	return &ecdsa.PublicKey{Curve: curve, X: x, Y: y}, nil
}
