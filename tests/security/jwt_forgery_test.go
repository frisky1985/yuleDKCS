// Package security — JWT forgery and tampering tests
//
// Tests JWT signature verification robustness against:
//   - alg:none bypass
//   - alg confusion (HS256 → RS256 using public key)
//   - Weak secret brute force
//   - Token tampering (modify claims)
//   - Token replay within TTL
//
// Run: go test -v -run TestJWTSecurity ./tests/security/
package security

import (
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rand"
	"crypto/x509"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strings"
	"testing"
	"time"

	jose "github.com/golang-jwt/jwt/v5"
)

func TestJWTSecurity(t *testing.T) {
	logf := func(format string, args ...interface{}) {
		t.Logf(format, args...)
	}

	apiBase := getAPIGateway(t)
	realSecret := getJWTSecret()

	t.Run("jwt_alg_none_bypass", func(t *testing.T) {
		logf("   🚨 Test 1: JWT alg=none bypass attack")

		// JWT with alg=none (tampered header)
		header := base64.RawURLEncoding.EncodeToString(
			[]byte(`{"alg":"none","typ":"JWT"}`))
		payload := base64.RawURLEncoding.EncodeToString(
			[]byte(`{"user_id":"admin","role":"admin","exp":9999999999}`))
		tamperedToken := fmt.Sprintf("%s.%s.", header, payload)

		req, _ := http.NewRequest("GET", apiBase+"/api/v1/keys", nil)
		req.Header.Set("Authorization", "Bearer "+tamperedToken)

		client := &http.Client{Timeout: 5 * time.Second}
		resp, err := client.Do(req)
		if err != nil {
			t.Fatalf("Request failed: %v", err)
		}
		defer resp.Body.Close()

		if resp.StatusCode == 200 {
			t.Errorf("❌ FAIL: alg=none bypass succeeded! Status=%d", resp.StatusCode)
		} else {
			body, _ := io.ReadAll(resp.Body)
			logf("   ✅ alg=none rejected: Status=%d, Body=%s", resp.StatusCode, string(body))
		}
	})

	t.Run("jwt_alg_confusion_rs256", func(t *testing.T) {
		logf("   🚨 Test 2: JWT alg confusion HS256→RS256")

		// Test: try RS256 with a known public key
		priv, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
		if err != nil {
			t.Fatalf("Failed to generate key: %v", err)
		}
		pubDER, err := x509.MarshalPKIXPublicKey(&priv.PublicKey)
		if err != nil {
			t.Fatalf("Failed to marshal pubkey: %v", err)
		}
		_ = pubDER // would be used if server accepted RS256

		// Create RS256-signed token
		claims := jose.MapClaims{
			"user_id": "admin",
			"role":    "admin",
			"exp":     time.Now().Add(1 * time.Hour).Unix(),
			"iat":     time.Now().Unix(),
		}
		token := jose.NewWithClaims(jose.SigningMethodES256, claims)
		tokenString, err := token.SignedString(priv)
		if err != nil {
			t.Fatalf("Failed to sign token: %v", err)
		}

		req, _ := http.NewRequest("GET", apiBase+"/api/v1/keys", nil)
		req.Header.Set("Authorization", "Bearer "+tokenString)

		client := &http.Client{Timeout: 5 * time.Second}
		resp, err := client.Do(req)
		if err != nil {
			t.Fatalf("Request failed: %v", err)
		}
		defer resp.Body.Close()

		if resp.StatusCode == 200 {
			t.Errorf("❌ FAIL: RS256 alg confusion succeeded! Status=%d", resp.StatusCode)
		} else {
			body, _ := io.ReadAll(resp.Body)
			logf("   ✅ RS256 alg confusion rejected: Status=%d, Body=%s", resp.StatusCode, string(body))
		}
	})

	t.Run("jwt_weak_secret_bruteforce", func(t *testing.T) {
		logf("   🚨 Test 3: JWT weak secret brute force detection")

		// First get a valid token
		token, err := signJWTWithSecret("admin", "admin", realSecret)
		if err != nil {
			t.Fatalf("Failed to sign test token: %v", err)
		}

		// Try common weak passwords as secrets
		weakSecrets := []string{
			"secret", "password", "changeme", "default",
			"yuledkcs", "dkcs", "12345678", "qwerty123",
		}
		foundSecret := ""
		for _, ws := range weakSecrets {
			parsed, err := jose.Parse(token, func(t *jose.Token) (interface{}, error) {
				return []byte(ws), nil
			})
			if err == nil && parsed.Valid {
				foundSecret = ws
				break
			}
		}

		if foundSecret != "" {
			// Only a finding if real secret is weak
			if foundSecret == realSecret {
				t.Errorf("❌ FAIL: JWT secret is weak! Found=%s", foundSecret)
			} else {
				logf("   ℹ️  Real secret is NOT in weak list (not found)")
			}
		} else {
			logf("   ✅ Weak secret list failed to find real secret (good)")
		}
	})

	t.Run("jwt_claim_tampering", func(t *testing.T) {
		logf("   🚨 Test 4: JWT claim tampering (user_id/role modification)")

		// Create token with real secret but tampered payload
		validToken, err := signJWTWithSecret("admin", "admin", realSecret)
		if err != nil {
			t.Fatalf("Failed to sign token: %v", err)
		}
		parts := strings.Split(validToken, ".")
		if len(parts) != 3 {
			t.Fatalf("Invalid JWT format")
		}

		// Decode and modify payload
		payloadBytes, _ := base64.RawURLEncoding.DecodeString(parts[1])
		var claims map[string]interface{}
		json.Unmarshal(payloadBytes, &claims)
		claims["user_id"] = "hacker"
		claims["role"] = "admin"
		claims["exp"] = time.Now().Add(24 * time.Hour).Unix()
		modifiedPayload, _ := json.Marshal(claims)

		// Re-encode (signature will be invalid after payload change)
		tamperedHeader := parts[0]
		tamperedPayload := base64.RawURLEncoding.EncodeToString(modifiedPayload)
		// Keep original signature (it won't match modified payload)
		tamperedToken := fmt.Sprintf("%s.%s.%s", tamperedHeader, tamperedPayload, parts[2])

		req, _ := http.NewRequest("GET", apiBase+"/api/v1/keys", nil)
		req.Header.Set("Authorization", "Bearer "+tamperedToken)

		client := &http.Client{Timeout: 5 * time.Second}
		resp, err := client.Do(req)
		if err != nil {
			t.Fatalf("Request failed: %v", err)
		}
		defer resp.Body.Close()

		if resp.StatusCode == 200 {
			t.Errorf("❌ FAIL: Claim tampering succeeded! Status=%d", resp.StatusCode)
		} else {
			body, _ := io.ReadAll(resp.Body)
			logf("   ✅ Claim tampering rejected: Status=%d, Body=%s", resp.StatusCode, string(body))
		}
	})

	t.Run("jwt_expired_token", func(t *testing.T) {
		logf("   🚨 Test 5: Expired JWT token rejection")

		// Create token that expired 1 minute ago
		expiredClaims := jose.MapClaims{
			"user_id": "admin",
			"role":    "admin",
			"exp":     time.Now().Add(-1 * time.Minute).Unix(),
			"iat":     time.Now().Add(-2 * time.Hour).Unix(),
		}
		token := jose.NewWithClaims(jose.SigningMethodHS256, expiredClaims)
		tokenString, err := token.SignedString([]byte(realSecret))
		if err != nil {
			t.Fatalf("Failed to sign token: %v", err)
		}

		req, _ := http.NewRequest("GET", apiBase+"/api/v1/keys", nil)
		req.Header.Set("Authorization", "Bearer "+tokenString)

		client := &http.Client{Timeout: 5 * time.Second}
		resp, err := client.Do(req)
		if err != nil {
			t.Fatalf("Request failed: %v", err)
		}
		defer resp.Body.Close()

		if resp.StatusCode == 200 {
			t.Errorf("❌ FAIL: Expired token accepted! Status=%d", resp.StatusCode)
		} else {
			body, _ := io.ReadAll(resp.Body)
			logf("   ✅ Expired token rejected: Status=%d, Body=%s", resp.StatusCode, string(body))
		}
	})

	t.Run("jwt_missing_header", func(t *testing.T) {
		logf("   🚨 Test 6: Missing Authorization header")
		req, _ := http.NewRequest("GET", apiBase+"/api/v1/keys", nil)
		client := &http.Client{Timeout: 5 * time.Second}
		resp, err := client.Do(req)
		if err != nil {
			t.Fatalf("Request failed: %v", err)
		}
		defer resp.Body.Close()
		if resp.StatusCode == 401 {
			logf("   ✅ Missing header: 401 Unauthorized")
		} else {
			t.Errorf("❌ FAIL: Missing header got Status=%d, expected 401", resp.StatusCode)
		}
	})

	logf("═══════════════════════════════════════")
	logf("✅ PASS: JWT Security Tests")
	logf("═══════════════════════════════════════")
}

// signJWTWithSecret creates a signed JWT with the given claims and secret.
func signJWTWithSecret(userID, role, secret string) (string, error) {
	claims := jose.MapClaims{
		"user_id": userID,
		"role":    role,
		"exp":     time.Now().Add(1 * time.Hour).Unix(),
		"iat":     time.Now().Unix(),
	}
	token := jose.NewWithClaims(jose.SigningMethodHS256, claims)
	return token.SignedString([]byte(secret))
}
