package revocation

import (
	"context"
	"crypto/ecdsa"
	"errors"
	"fmt"
	"sync"
	"time"

	"go.uber.org/zap"
)

// StoredToken is one persisted revocation token.
type StoredToken struct {
	Type  EntryType
	Token [TokenLen]byte
}

// Store persists revocation tokens. Epoch must increase monotonically across
// AddRevocationToken calls so vehicles can reject replayed lists.
type Store interface {
	AddRevocationToken(ctx context.Context, typ EntryType, token [TokenLen]byte) (uint32, error)
	RevocationTokens(ctx context.Context) ([]StoredToken, uint32, error)
}

// Service revokes identifiers and serves the signed list consumed by vehicles.
type Service struct {
	store Store
	key   *ecdsa.PrivateKey
	now   func() time.Time
	log   *zap.Logger
}

func NewService(store Store, key *ecdsa.PrivateKey, logger *zap.Logger) (*Service, error) {
	if store == nil {
		return nil, errors.New("revocation store is required")
	}
	if key == nil {
		return nil, errors.New("CRL signing key is required")
	}
	if logger == nil {
		logger = zap.NewNop()
	}
	return &Service{
		store: store,
		key:   key,
		now:   time.Now,
		log:   logger.With(zap.String("service", "revocation")),
	}, nil
}

// Revoke adds one identifier to the list and returns the new epoch.
func (s *Service) Revoke(ctx context.Context, typ EntryType, data []byte) (uint32, error) {
	token, err := TokenFor(typ, data)
	if err != nil {
		return 0, err
	}
	epoch, err := s.store.AddRevocationToken(ctx, typ, token)
	if err != nil {
		return 0, fmt.Errorf("persist revocation token: %w", err)
	}
	s.log.Info("identifier revoked",
		zap.String("entry_type", typ.String()),
		zap.Uint32("epoch", epoch),
	)
	return epoch, nil
}

// Current builds and signs the current list.
func (s *Service) Current(ctx context.Context) ([]byte, uint32, error) {
	tokens, epoch, err := s.store.RevocationTokens(ctx)
	if err != nil {
		return nil, 0, fmt.Errorf("load revocation tokens: %w", err)
	}
	// The firmware blob holds at most MaxEntries tokens. Refusing is safer than
	// silently dropping entries (dropping would un-revoke keys).
	if len(tokens) > MaxEntries {
		return nil, 0, fmt.Errorf("revocation list overflow: %d > %d entries",
			len(tokens), MaxEntries)
	}

	issuedAt := s.now().Unix()
	if issuedAt < 0 {
		issuedAt = 0
	}
	list := List{
		Epoch:    epoch,
		IssuedAt: uint32(issuedAt),
		Tokens:   make([][TokenLen]byte, len(tokens)),
	}
	for i := range tokens {
		list.Tokens[i] = tokens[i].Token
	}

	blob, err := list.MarshalSigned(s.key)
	if err != nil {
		return nil, 0, err
	}
	return blob, epoch, nil
}

// EntryCount returns the number of stored entries (for health/ops endpoints).
func (s *Service) EntryCount(ctx context.Context) (int, error) {
	tokens, _, err := s.store.RevocationTokens(ctx)
	if err != nil {
		return 0, err
	}
	return len(tokens), nil
}

// MemoryStore is an in-memory Store used by tests and local development.
// It is safe for concurrent use.
type MemoryStore struct {
	mu     sync.Mutex
	epoch  uint32
	seen   map[[TokenLen]byte]struct{}
	tokens []StoredToken
}

func NewMemoryStore() *MemoryStore {
	return &MemoryStore{seen: make(map[[TokenLen]byte]struct{})}
}

func (m *MemoryStore) AddRevocationToken(_ context.Context, typ EntryType, token [TokenLen]byte) (uint32, error) {
	m.mu.Lock()
	defer m.mu.Unlock()

	m.epoch++
	if _, ok := m.seen[token]; !ok {
		m.seen[token] = struct{}{}
		m.tokens = append(m.tokens, StoredToken{Type: typ, Token: token})
	}
	return m.epoch, nil
}

func (m *MemoryStore) RevocationTokens(_ context.Context) ([]StoredToken, uint32, error) {
	m.mu.Lock()
	defer m.mu.Unlock()

	out := make([]StoredToken, len(m.tokens))
	copy(out, m.tokens)
	return out, m.epoch, nil
}
