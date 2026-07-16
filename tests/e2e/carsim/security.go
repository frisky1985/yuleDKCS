package main

import (
	"crypto/aes"
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rand"
	"crypto/sha256"
	"crypto/x509"
	"encoding/binary"
	"fmt"
	"math/big"
	"sync"
	"time"
)

// SE050Mock simulates an NXP SE050 secure element with SCP03 secure channel.
type SE050Mock struct {
	mu sync.RWMutex

	// Static keys (simulating SE050 factory-provisioned keys)
	StaticPrivateKey *ecdsa.PrivateKey
	StaticPublicKey  []byte // DER-encoded

	// Session state
	HostChallenge []byte
	CardChallenge []byte
	SessionKey    []byte // AES-128 session key
	SessionMac    []byte
	SessionEnc    []byte

	// Key store
	StoredKeys map[string][]byte // keyID -> encrypted key material

	Authenticated bool
}

// NewSE050Mock creates a new SE050 mock with random static keys.
func NewSE050Mock() *SE050Mock {
	priv, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	if err != nil {
		panic(fmt.Sprintf("failed to generate SE050 key: %v", err))
	}
	pubDER, err := x509.MarshalPKIXPublicKey(&priv.PublicKey)
	if err != nil {
		panic(fmt.Sprintf("failed to marshal SE050 pubkey: %v", err))
	}
	return &SE050Mock{
		StaticPrivateKey: priv,
		StaticPublicKey:  pubDER,
		StoredKeys:       make(map[string][]byte),
	}
}

// GetPublicKey returns the SE050's public key in DER format.
func (s *SE050Mock) GetPublicKey() []byte {
	s.mu.RLock()
	defer s.mu.RUnlock()
	return s.StaticPublicKey
}

// StartSCP03 initiates SCP03 secure channel establishment.
// Returns host challenge and card challenge.
func (s *SE050Mock) StartSCP03(hostChallenge []byte) ([]byte, error) {
	s.mu.Lock()
	defer s.mu.Unlock()

	if len(hostChallenge) != 8 {
		return nil, fmt.Errorf("host challenge must be 8 bytes, got %d", len(hostChallenge))
	}

	s.HostChallenge = make([]byte, 8)
	copy(s.HostChallenge, hostChallenge)

	s.CardChallenge = make([]byte, 8)
	if _, err := rand.Read(s.CardChallenge); err != nil {
		return nil, fmt.Errorf("generate card challenge: %w", err)
	}

	// Derive session keys using static ECDH + KDF
	s.deriveSessionKeys()

	return s.CardChallenge, nil
}

// AuthenticateSCP03 verifies the host MAC and establishes the secure channel.
// Uses simplified MAC (SHA-256 of combined challenges) for simulation.
func (s *SE050Mock) AuthenticateSCP03(hostMac []byte) (bool, error) {
	s.mu.Lock()
	defer s.mu.Unlock()

	if s.SessionKey == nil {
		return false, fmt.Errorf("SCP03 not initialized")
	}

	// Compute expected MAC over (HostChallenge || CardChallenge)
	data := append(s.HostChallenge, s.CardChallenge...)
	hash := sha256.Sum256(data)
	expectedMac := hash[:8]

	if len(hostMac) != len(expectedMac) {
		return false, nil
	}

	match := true
	for i := range expectedMac {
		if hostMac[i] != expectedMac[i] {
			match = false
			break
		}
	}

	if match {
		s.Authenticated = true
	}
	return match, nil
}

func (s *SE050Mock) deriveSessionKeys() {
	// In real SCP03, this uses ECDH shared secret + KDF.
	// For simulation, derive AES-128 keys from a combined seed.
	seed := sha256.Sum256(append(s.HostChallenge, s.CardChallenge...))
	s.SessionKey = seed[:16]

	// SCP03 derives MAC, ENC, and RMAC keys from session key
	// Simplified: use first 16 bytes for MAC, next 16 for ENC
	s.SessionMac = deriveSCP03Key(s.SessionKey, 1)
	s.SessionEnc = deriveSCP03Key(s.SessionKey, 2)
}

func deriveSCP03Key(base []byte, variant byte) []byte {
	// SCP03 key derivation: AES(Key, constant||label||seed)
	block, _ := aes.NewCipher(base)
	input := make([]byte, 16)
	input[0] = variant // 1=MAC, 2=ENC, 3=RMAC
	input[15] = 1
	result := make([]byte, 16)
	block.Encrypt(result, input)
	return result
}



// EncryptSCP03 encrypts data under the SCP03 session ENC key (simplified).
func (s *SE050Mock) EncryptSCP03(plaintext []byte) ([]byte, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	if !s.Authenticated || s.SessionEnc == nil {
		return nil, fmt.Errorf("SCP03 not authenticated")
	}
	// Simplified: XOR-based encryption for simulation
	result := make([]byte, len(plaintext))
	for i := range plaintext {
		result[i] = plaintext[i] ^ s.SessionEnc[i%len(s.SessionEnc)]
	}
	return result, nil
}

// DecryptSCP03 decrypts data under the SCP03 session ENC key.
func (s *SE050Mock) DecryptSCP03(ciphertext []byte) ([]byte, error) {
	return s.EncryptSCP03(ciphertext) // XOR is symmetric
}

// StoreKey stores key material encrypted under the session key.
func (s *SE050Mock) StoreKey(keyID string, keyData []byte) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	if !s.Authenticated {
		return fmt.Errorf("SCP03 not authenticated")
	}
	encrypted := make([]byte, len(keyData))
	for i := range keyData {
		encrypted[i] = keyData[i] ^ s.SessionEnc[i%len(s.SessionEnc)]
	}
	s.StoredKeys[keyID] = encrypted
	return nil
}

// SignChallenge signs a challenge with the SE050's static ECDSA key.
func (s *SE050Mock) SignChallenge(challenge []byte) ([]byte, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	hash := sha256.Sum256(challenge)
	r, sVal, err := ecdsa.Sign(rand.Reader, s.StaticPrivateKey, hash[:])
	if err != nil {
		return nil, err
	}
	sig := make([]byte, 64)
	r.FillBytes(sig[:32])
	sVal.FillBytes(sig[32:])
	return sig, nil
}

// VerifySignature verifies a signature with the given DER-encoded public key.
func VerifySignature(pubKeyDER, challenge, signature []byte) bool {
	pubKey, err := x509.ParsePKIXPublicKey(pubKeyDER)
	if err != nil {
		return false
	}
	ecdsaPub, ok := pubKey.(*ecdsa.PublicKey)
	if !ok {
		return false
	}
	hash := sha256.Sum256(challenge)
	r := new(big.Int).SetBytes(signature[:32])
	s := new(big.Int).SetBytes(signature[32:])
	return ecdsa.Verify(ecdsaPub, hash[:], r, s)
}

// GenerateChallenge creates a random 16-byte challenge.
func GenerateChallenge() []byte {
	buf := make([]byte, 16)
	rand.Read(buf)
	return buf
}

// GenerateKeyMaterial creates a simulated digital key credential.
func GenerateKeyMaterial(keyID string, keyType uint32) ([]byte, error) {
	priv, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	if err != nil {
		return nil, err
	}
	pubDER, err := x509.MarshalPKIXPublicKey(&priv.PublicKey)
	if err != nil {
		return nil, err
	}
	// Package as simulated key material with keyID, key type
	material := make([]byte, 8+len(keyID)+8+len(pubDER))
	binary.BigEndian.PutUint64(material[0:8], uint64(time.Now().UnixMilli()))
	copy(material[8:], []byte(keyID))
	off := 8 + len(keyID)
	binary.BigEndian.PutUint64(material[off:off+8], uint64(keyType))
	copy(material[off+8:], pubDER)
	return material, nil
}

// GetSessionKeyECDH simulates ECDH key agreement.
// In real life, the phone and SE050 perform ECDH with their respective keys.
// This returns a deterministic shared secret for simulation.
func GetSessionKeyECDH(devicePubKeyDER []byte) []byte {
	hash := sha256.Sum256(devicePubKeyDER)
	return hash[:16]
}


