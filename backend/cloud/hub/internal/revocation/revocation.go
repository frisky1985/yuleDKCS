// Package revocation builds and signs the vehicle revocation list (CRL)
// consumed by embedded/ccc_protocol/src/security/sec_revocation.c.
//
// The wire format is fixed-layout little-endian (see sec_revocation.h):
//
//	offset  size  field
//	0       4     magic "YRV2"
//	4       1     format version = 2
//	5       1     key type (1 = P-256)
//	6       2     reserved
//	8       4     epoch
//	12      4     issued_at
//	16      2     entry_count (<= 32)
//	18      2     reserved
//	20      16*n  tokens
//	20+16n  64    ECDSA signature (r || s, 32 bytes each)
//
// v2 entries are domain-separated SHA-256 tokens, so the list can revoke by
// device id, certificate serial or public key without exposing raw ids:
//
//	token = SHA-256(domain || id)[0:16]
//	domain 0x01 + device_id(16), 0x02 + cert serial(1..20), 0x03 + pubkey(64)
package revocation

import (
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rand"
	"crypto/sha256"
	"crypto/x509"
	"encoding/binary"
	"encoding/pem"
	"errors"
	"fmt"
	"math/big"
	"os"
)

const (
	Magic         = "YRV2"
	FormatVersion = 2

	KeyTypeP256 = 1

	HeaderLen  = 20
	TokenLen   = 16
	SigLen     = 64
	MaxEntries = 32
	MaxBlobLen = HeaderLen + MaxEntries*TokenLen + SigLen // 596
)

// EntryType mirrors the domain byte used by the firmware.
type EntryType uint8

const (
	EntryDeviceID   EntryType = 0x01
	EntryCertSerial EntryType = 0x02
	EntryPubkey     EntryType = 0x03
)

func (t EntryType) String() string {
	switch t {
	case EntryDeviceID:
		return "device_id"
	case EntryCertSerial:
		return "cert_serial"
	case EntryPubkey:
		return "pubkey"
	default:
		return "unknown"
	}
}

// ParseEntryType maps the REST enum name to an EntryType.
func ParseEntryType(name string) (EntryType, error) {
	switch name {
	case "device_id":
		return EntryDeviceID, nil
	case "cert_serial":
		return EntryCertSerial, nil
	case "pubkey":
		return EntryPubkey, nil
	default:
		return 0, fmt.Errorf("unknown entry type %q", name)
	}
}

// TokenFor validates the identifier length and computes its revocation token.
func TokenFor(typ EntryType, data []byte) ([TokenLen]byte, error) {
	var zero [TokenLen]byte

	switch typ {
	case EntryDeviceID:
		if len(data) != 16 {
			return zero, fmt.Errorf("device_id must be 16 bytes, got %d", len(data))
		}
	case EntryCertSerial:
		if len(data) == 0 || len(data) > 20 {
			return zero, fmt.Errorf("cert_serial must be 1..20 bytes, got %d", len(data))
		}
	case EntryPubkey:
		if len(data) != 64 {
			return zero, fmt.Errorf("pubkey must be 64 bytes (X||Y), got %d", len(data))
		}
	default:
		return zero, fmt.Errorf("unsupported entry type %d", typ)
	}

	h := sha256.New()
	h.Write([]byte{byte(typ)})
	h.Write(data)

	var out [TokenLen]byte
	copy(out[:], h.Sum(nil))
	return out, nil
}

// List is the logical content of a signed CRL.
type List struct {
	Epoch    uint32
	IssuedAt uint32
	Tokens   [][TokenLen]byte
}

// MarshalSigned serialises the list and signs it with an ECDSA P-256 key.
func (l List) MarshalSigned(key *ecdsa.PrivateKey) ([]byte, error) {
	if key == nil {
		return nil, errors.New("nil signing key")
	}
	if key.Curve != elliptic.P256() {
		return nil, errors.New("CRL signing key must be P-256")
	}
	if len(l.Tokens) > MaxEntries {
		return nil, fmt.Errorf("too many revocation entries: %d > %d",
			len(l.Tokens), MaxEntries)
	}

	signedLen := HeaderLen + len(l.Tokens)*TokenLen
	buf := make([]byte, signedLen+SigLen)

	copy(buf[0:4], Magic)
	buf[4] = FormatVersion
	buf[5] = KeyTypeP256
	binary.LittleEndian.PutUint32(buf[8:12], l.Epoch)
	binary.LittleEndian.PutUint32(buf[12:16], l.IssuedAt)
	binary.LittleEndian.PutUint16(buf[16:18], uint16(len(l.Tokens)))

	for i, tok := range l.Tokens {
		copy(buf[HeaderLen+i*TokenLen:], tok[:])
	}

	digest := sha256.Sum256(buf[:signedLen])
	r, s, err := ecdsa.Sign(rand.Reader, key, digest[:])
	if err != nil {
		return nil, fmt.Errorf("sign CRL: %w", err)
	}
	r.FillBytes(buf[signedLen : signedLen+32])
	s.FillBytes(buf[signedLen+32:])

	return buf, nil
}

// ParseAndVerify decodes a signed CRL and verifies its signature.
func ParseAndVerify(blob []byte, pub *ecdsa.PublicKey) (List, error) {
	var out List

	if pub == nil {
		return out, errors.New("nil verification key")
	}
	if len(blob) < HeaderLen+SigLen {
		return out, errors.New("CRL too short")
	}
	if string(blob[0:4]) != Magic {
		return out, errors.New("bad CRL magic")
	}
	if blob[4] != FormatVersion {
		return out, fmt.Errorf("unsupported CRL version %d", blob[4])
	}
	if blob[5] != KeyTypeP256 {
		return out, fmt.Errorf("unsupported CRL key type %d", blob[5])
	}

	count := int(binary.LittleEndian.Uint16(blob[16:18]))
	if count > MaxEntries {
		return out, fmt.Errorf("entry count %d exceeds limit %d", count, MaxEntries)
	}

	signedLen := HeaderLen + count*TokenLen
	if len(blob) != signedLen+SigLen {
		return out, fmt.Errorf("CRL length mismatch: got %d want %d",
			len(blob), signedLen+SigLen)
	}

	digest := sha256.Sum256(blob[:signedLen])
	r := new(big.Int).SetBytes(blob[signedLen : signedLen+32])
	s := new(big.Int).SetBytes(blob[signedLen+32:])
	if !ecdsa.Verify(pub, digest[:], r, s) {
		return out, errors.New("CRL signature verification failed")
	}

	out.Epoch = binary.LittleEndian.Uint32(blob[8:12])
	out.IssuedAt = binary.LittleEndian.Uint32(blob[12:16])
	out.Tokens = make([][TokenLen]byte, count)
	for i := 0; i < count; i++ {
		copy(out.Tokens[i][:], blob[HeaderLen+i*TokenLen:])
	}
	return out, nil
}

// LoadPrivateKey reads a PKCS#8 or SEC1 PEM encoded EC private key.
func LoadPrivateKey(path string) (*ecdsa.PrivateKey, error) {
	raw, err := os.ReadFile(path)
	if err != nil {
		return nil, fmt.Errorf("read CRL signing key: %w", err)
	}
	block, _ := pem.Decode(raw)
	if block == nil {
		return nil, errors.New("CRL signing key is not PEM encoded")
	}

	if parsed, err := x509.ParsePKCS8PrivateKey(block.Bytes); err == nil {
		key, ok := parsed.(*ecdsa.PrivateKey)
		if !ok {
			return nil, errors.New("CRL signing key is not an EC private key")
		}
		if key.Curve != elliptic.P256() {
			return nil, errors.New("CRL signing key must be P-256")
		}
		return key, nil
	}

	key, err := x509.ParseECPrivateKey(block.Bytes)
	if err != nil {
		return nil, fmt.Errorf("parse CRL signing key: %w", err)
	}
	if key.Curve != elliptic.P256() {
		return nil, errors.New("CRL signing key must be P-256")
	}
	return key, nil
}
