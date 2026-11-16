package gateway

import (
	"bytes"
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rand"
	"encoding/base64"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/gin-gonic/gin"
	"go.uber.org/zap"

	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/revocation"
)

func newRevocationRouter(t *testing.T, g *RESTGateway) (*gin.Engine, *ecdsa.PrivateKey) {
	t.Helper()

	key, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	if err != nil {
		t.Fatalf("generate key: %v", err)
	}
	svc, err := revocation.NewService(revocation.NewMemoryStore(), key, zap.NewNop())
	if err != nil {
		t.Fatalf("new service: %v", err)
	}
	g.WithRevocation(svc)

	r := gin.New()
	r.GET("/api/v1/revocation/crl", g.getRevocationList)
	v1 := r.Group("/api/v1")
	v1.Use(g.authMiddleware())
	v1.POST("/revocation/entries", g.addRevocationEntry)
	return r, key
}

func doJSON(r *gin.Engine, method, path, token string, body any) *httptest.ResponseRecorder {
	var buf bytes.Buffer
	if body != nil {
		_ = json.NewEncoder(&buf).Encode(body)
	}
	req := httptest.NewRequest(method, path, &buf)
	req.Header.Set("Content-Type", "application/json")
	if token != "" {
		req.Header.Set("Authorization", "Bearer "+token)
	}
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)
	return w
}

func TestRevocationListUnavailableWithoutService(t *testing.T) {
	g := newTestGateway()
	r := gin.New()
	r.GET("/api/v1/revocation/crl", g.getRevocationList)

	w := doJSON(r, "GET", "/api/v1/revocation/crl", "", nil)
	if w.Code != http.StatusServiceUnavailable {
		t.Fatalf("expected 503, got %d", w.Code)
	}
}

func TestRevocationEntryRequiresAdmin(t *testing.T) {
	g := newTestGateway()
	r, _ := newRevocationRouter(t, g)

	payload := revokeEntryRequest{
		Type: "device_id",
		Data: base64.StdEncoding.EncodeToString(bytes.Repeat([]byte{0x01}, 16)),
	}

	// No token -> 401
	if w := doJSON(r, "POST", "/api/v1/revocation/entries", "", payload); w.Code != http.StatusUnauthorized {
		t.Fatalf("expected 401 without token, got %d", w.Code)
	}

	// User token -> 403
	userTok := issueTestToken(g, "user-1", "user")
	if w := doJSON(r, "POST", "/api/v1/revocation/entries", userTok, payload); w.Code != http.StatusForbidden {
		t.Fatalf("expected 403 for non-admin, got %d", w.Code)
	}

	// Admin token -> 200
	adminTok := issueTestToken(g, "admin-1", "admin")
	if w := doJSON(r, "POST", "/api/v1/revocation/entries", adminTok, payload); w.Code != http.StatusOK {
		t.Fatalf("expected 200 for admin, got %d body=%s", w.Code, w.Body.String())
	}
}

func TestRevocationFlowServesSignedList(t *testing.T) {
	g := newTestGateway()
	r, key := newRevocationRouter(t, g)
	adminTok := issueTestToken(g, "admin-1", "admin")

	devID := bytes.Repeat([]byte{0x42}, 16)
	payload := revokeEntryRequest{
		Type: "device_id",
		Data: base64.StdEncoding.EncodeToString(devID),
	}
	if w := doJSON(r, "POST", "/api/v1/revocation/entries", adminTok, payload); w.Code != http.StatusOK {
		t.Fatalf("revoke failed: %d %s", w.Code, w.Body.String())
	}

	req := httptest.NewRequest("GET", "/api/v1/revocation/crl", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", w.Code)
	}
	if ct := w.Header().Get("Content-Type"); ct != "application/octet-stream" {
		t.Fatalf("unexpected content type %q", ct)
	}
	if w.Header().Get("X-CRL-Epoch") != "1" {
		t.Fatalf("unexpected epoch header %q", w.Header().Get("X-CRL-Epoch"))
	}

	list, err := revocation.ParseAndVerify(w.Body.Bytes(), &key.PublicKey)
	if err != nil {
		t.Fatalf("served CRL failed verification: %v", err)
	}
	if list.Epoch != 1 || len(list.Tokens) != 1 {
		t.Fatalf("unexpected list: epoch=%d entries=%d", list.Epoch, len(list.Tokens))
	}
	want, _ := revocation.TokenFor(revocation.EntryDeviceID, devID)
	if list.Tokens[0] != want {
		t.Fatal("served token does not match revoked identifier")
	}
}

func TestRevocationEntryValidation(t *testing.T) {
	g := newTestGateway()
	r, _ := newRevocationRouter(t, g)
	adminTok := issueTestToken(g, "admin-1", "admin")

	cases := []revokeEntryRequest{
		{Type: "unknown", Data: base64.StdEncoding.EncodeToString([]byte{1})},
		{Type: "device_id", Data: "!!not-base64!!"},
		{Type: "device_id", Data: base64.StdEncoding.EncodeToString([]byte{1, 2, 3})},
	}
	for i, tc := range cases {
		w := doJSON(r, "POST", "/api/v1/revocation/entries", adminTok, tc)
		if w.Code != http.StatusBadRequest {
			t.Fatalf("case %d: expected 400, got %d body=%s", i, w.Code, w.Body.String())
		}
	}
}
