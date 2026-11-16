package revocation

import (
	"bytes"
	"context"
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rand"
	"crypto/x509"
	"encoding/pem"
	"os"
	"path/filepath"
	"testing"

	"go.uber.org/zap"
)

func newTestKey(t *testing.T) *ecdsa.PrivateKey {
	t.Helper()
	key, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	if err != nil {
		t.Fatalf("generate key: %v", err)
	}
	return key
}

func TestTokenForValidatesAndSeparatesDomains(t *testing.T) {
	devA := bytes.Repeat([]byte{0x11}, 16)
	devB := bytes.Repeat([]byte{0x22}, 16)
	serial := []byte{0x01, 0x02, 0x03}
	pub := bytes.Repeat([]byte{0x44}, 64)

	a, err := TokenFor(EntryDeviceID, devA)
	if err != nil {
		t.Fatalf("device token: %v", err)
	}
	b, err := TokenFor(EntryDeviceID, devB)
	if err != nil {
		t.Fatalf("device token: %v", err)
	}
	s, err := TokenFor(EntryCertSerial, serial)
	if err != nil {
		t.Fatalf("serial token: %v", err)
	}
	p, err := TokenFor(EntryPubkey, pub)
	if err != nil {
		t.Fatalf("pubkey token: %v", err)
	}

	if a == b {
		t.Fatal("distinct device ids produced the same token")
	}
	if a == s || a == p || s == p {
		t.Fatal("domain separation failed between entry types")
	}

	// Determinism and domain binding: same 16 bytes as device id vs serial
	// must not collide.
	raw := bytes.Repeat([]byte{0x11}, 16)
	tDev, _ := TokenFor(EntryDeviceID, raw)
	tSer, _ := TokenFor(EntryCertSerial, raw)
	if tDev == tSer {
		t.Fatal("same payload under different domains must not collide")
	}

	if _, err := TokenFor(EntryDeviceID, []byte{1, 2, 3}); err == nil {
		t.Fatal("expected device_id length error")
	}
	if _, err := TokenFor(EntryCertSerial, nil); err == nil {
		t.Fatal("expected empty serial error")
	}
	if _, err := TokenFor(EntryPubkey, bytes.Repeat([]byte{1}, 63)); err == nil {
		t.Fatal("expected pubkey length error")
	}
	if _, err := TokenFor(EntryType(9), devA); err == nil {
		t.Fatal("expected unsupported entry type error")
	}
}

func TestMarshalAndVerifyRoundTrip(t *testing.T) {
	key := newTestKey(t)
	tok, _ := TokenFor(EntryDeviceID, bytes.Repeat([]byte{0xAB}, 16))

	list := List{Epoch: 7, IssuedAt: 1800000000, Tokens: [][16]byte{tok}}
	blob, err := list.MarshalSigned(key)
	if err != nil {
		t.Fatalf("marshal: %v", err)
	}
	if len(blob) != HeaderLen+TokenLen+SigLen {
		t.Fatalf("unexpected blob length %d", len(blob))
	}
	if !bytes.Equal(blob[:4], []byte(Magic)) {
		t.Fatalf("bad magic: %q", blob[:4])
	}

	got, err := ParseAndVerify(blob, &key.PublicKey)
	if err != nil {
		t.Fatalf("verify: %v", err)
	}
	if got.Epoch != 7 || got.IssuedAt != 1800000000 || len(got.Tokens) != 1 || got.Tokens[0] != tok {
		t.Fatalf("round trip mismatch: %+v", got)
	}

	// Tampering must be rejected.
	tampered := append([]byte(nil), blob...)
	tampered[HeaderLen] ^= 0xFF
	if _, err := ParseAndVerify(tampered, &key.PublicKey); err == nil {
		t.Fatal("tampered blob accepted")
	}

	// A different key must not verify.
	other := newTestKey(t)
	if _, err := ParseAndVerify(blob, &other.PublicKey); err == nil {
		t.Fatal("blob verified under the wrong key")
	}

	// Truncation must be rejected.
	if _, err := ParseAndVerify(blob[:len(blob)-1], &key.PublicKey); err == nil {
		t.Fatal("truncated blob accepted")
	}
}

func TestMarshalRejectsOverflow(t *testing.T) {
	key := newTestKey(t)
	list := List{Epoch: 1, Tokens: make([][16]byte, MaxEntries+1)}
	if _, err := list.MarshalSigned(key); err == nil {
		t.Fatal("expected overflow error")
	}
}

func TestServiceRevokesAndSigns(t *testing.T) {
	key := newTestKey(t)
	store := NewMemoryStore()
	svc, err := NewService(store, key, zap.NewNop())
	if err != nil {
		t.Fatalf("new service: %v", err)
	}

	ctx := context.Background()
	dev := bytes.Repeat([]byte{0x01}, 16)

	epoch, err := svc.Revoke(ctx, EntryDeviceID, dev)
	if err != nil || epoch != 1 {
		t.Fatalf("revoke device: epoch=%d err=%v", epoch, err)
	}
	if _, err := svc.Revoke(ctx, EntryCertSerial, []byte{0x02, 0x03}); err != nil {
		t.Fatalf("revoke serial: %v", err)
	}
	if _, err := svc.Revoke(ctx, EntryPubkey, bytes.Repeat([]byte{0x04}, 64)); err != nil {
		t.Fatalf("revoke pubkey: %v", err)
	}
	if _, err := svc.Revoke(ctx, EntryDeviceID, []byte{0x01}); err == nil {
		t.Fatal("expected validation error for short device id")
	}

	blob, epoch, err := svc.Current(ctx)
	if err != nil {
		t.Fatalf("current: %v", err)
	}
	if epoch != 3 {
		t.Fatalf("expected epoch 3, got %d", epoch)
	}

	list, err := ParseAndVerify(blob, &key.PublicKey)
	if err != nil {
		t.Fatalf("verify served blob: %v", err)
	}
	if len(list.Tokens) != 3 {
		t.Fatalf("expected 3 tokens, got %d", len(list.Tokens))
	}

	want, _ := TokenFor(EntryDeviceID, dev)
	if list.Tokens[0] != want {
		t.Fatal("device token not present in served list")
	}

	// Re-revoking the same identifier bumps the epoch but keeps one entry.
	if _, err := svc.Revoke(ctx, EntryDeviceID, dev); err != nil {
		t.Fatalf("re-revoke: %v", err)
	}
	n, err := svc.EntryCount(ctx)
	if err != nil || n != 3 {
		t.Fatalf("duplicate produced extra entry: n=%d err=%v", n, err)
	}
}

func TestServiceFailsClosedOnOverflow(t *testing.T) {
	key := newTestKey(t)
	store := NewMemoryStore()
	svc, err := NewService(store, key, zap.NewNop())
	if err != nil {
		t.Fatalf("new service: %v", err)
	}

	ctx := context.Background()
	for i := 0; i <= MaxEntries; i++ {
		id := bytes.Repeat([]byte{byte(i + 1)}, 16)
		if _, err := svc.Revoke(ctx, EntryDeviceID, id); err != nil {
			t.Fatalf("revoke %d: %v", i, err)
		}
	}
	if _, _, err := svc.Current(ctx); err == nil {
		t.Fatal("expected overflow error instead of silently dropping revocations")
	}
}

func TestLoadPrivateKey(t *testing.T) {
	key := newTestKey(t)

	der, err := x509.MarshalPKCS8PrivateKey(key)
	if err != nil {
		t.Fatalf("marshal pkcs8: %v", err)
	}
	path := filepath.Join(t.TempDir(), "crl-key.pem")
	if err := os.WriteFile(path, pem.EncodeToMemory(&pem.Block{Type: "PRIVATE KEY", Bytes: der}), 0o600); err != nil {
		t.Fatalf("write key: %v", err)
	}

	loaded, err := LoadPrivateKey(path)
	if err != nil {
		t.Fatalf("load key: %v", err)
	}
	if !loaded.Equal(key) {
		t.Fatal("loaded key does not match")
	}

	if _, err := LoadPrivateKey(filepath.Join(t.TempDir(), "missing.pem")); err == nil {
		t.Fatal("expected error for missing key file")
	}
}
