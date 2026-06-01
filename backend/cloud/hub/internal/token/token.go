package token

import (
	"crypto/hmac"
	"crypto/rand"
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"sync"
	"time"
)

// ─── 核心模型 ────────────────────────────────────────────────────────────

// Permission 权限位定义
type Permission uint16

const (
	PermLock         Permission = 1 << iota // 解锁/上锁
	PermEngineStart                          // 启动引擎
	PermTrunk                                // 后备箱
	PermWindow                               // 车窗
	PermClimate                              // 空调
	PermSeat                                 // 座椅
	PermFuel                                 // 油箱盖
	PermShare                                // 可再分享
	PermChargePort                           // 充电口
	PermValetMode                            // 代客模式(限速+锁手套箱)
)

// Token 是 DK Hub 签发的授权凭据
type Token struct {
	ID         string       `json:"id"`
	OwnerID    string       `json:"owner_id"`    // 车主
	SubjectID  string       `json:"subject_id"`  // 接收方（人或服务商）
	VehicleID  string       `json:"vehicle_id"`
	Perms      []Permission `json:"permissions"`
	ExpiresAt  int64        `json:"expires_at"`
	MaxUses    int32        `json:"max_uses"`    // 0=不限
	UseCount   int32        `json:"use_count"`
	Status     string       `json:"status"`      // active | revoked | expired | pending | suspended
	Signature  string       `json:"signature"`   // HMAC 签名，防篡改
	CreatedAt  int64        `json:"created_at"`
}

// ─── Token Service ────────────────────────────────────────────────────────

type Service struct {
	mu       sync.RWMutex
	tokens   map[string]*Token
	secret   []byte // HMAC 签名密钥
}

func NewService(secret string) *Service {
	return &Service{
		tokens: make(map[string]*Token),
		secret: []byte(secret),
	}
}

// Issue 签发 Token（车主授权）
func (s *Service) Issue(ownerID, subjectID, vehicleID string, perms []Permission, duration time.Duration, maxUses int32) (*Token, error) {
	id := generateID()
	now := time.Now()

	t := &Token{
		ID:        id,
		OwnerID:   ownerID,
		SubjectID: subjectID,
		VehicleID: vehicleID,
		Perms:     perms,
		ExpiresAt: now.Add(duration).UnixMilli(),
		MaxUses:   maxUses,
		Status:    "active",
		CreatedAt: now.UnixMilli(),
	}

	// 签名防篡改
	t.Signature = s.sign(t)

	s.mu.Lock()
	s.tokens[id] = t
	s.mu.Unlock()

	return t, nil
}

// Verify 验证 Token 有效性（服务商/车端调用）
func (s *Service) Verify(id string) (*Token, error) {
	s.mu.RLock()
	t, ok := s.tokens[id]
	s.mu.RUnlock()

	if !ok {
		return nil, fmt.Errorf("token not found")
	}
	if t.Status != "active" {
		return nil, fmt.Errorf("token %s", t.Status)
	}
	if time.Now().UnixMilli() > t.ExpiresAt {
		s.mu.Lock()
		t.Status = "expired"
		s.mu.Unlock()
		return nil, fmt.Errorf("token expired")
	}
	if t.MaxUses > 0 && t.UseCount >= t.MaxUses {
		return nil, fmt.Errorf("token max uses reached")
	}

	// 验证签名
	if !s.verify(t) {
		return nil, fmt.Errorf("token signature invalid")
	}

	// 使用计数+1
	s.mu.Lock()
	t.UseCount++
	s.mu.Unlock()

	return t, nil
}

// Revoke 吊销 Token（车主取消授权）
func (s *Service) Revoke(id, ownerID string) error {
	s.mu.Lock()
	defer s.mu.Unlock()

	t, ok := s.tokens[id]
	if !ok {
		return fmt.Errorf("token not found")
	}
	if t.OwnerID != ownerID {
		return fmt.Errorf("not the token owner")
	}
	t.Status = "revoked"
	return nil
}

// ─── 签名 ─────────────────────────────────────────────────────────────────

func (s *Service) sign(t *Token) string {
	mac := hmac.New(sha256.New, s.secret)
	// 对全部关键字段签名，防止篡改
	permBits := uint16(0)
	for _, p := range t.Perms {
		permBits |= uint16(p)
	}
	fmt.Fprintf(mac, "%s|%s|%s|%s|%d|%d|%d", t.ID, t.OwnerID, t.SubjectID, t.VehicleID, t.ExpiresAt, t.MaxUses, permBits)
	return hex.EncodeToString(mac.Sum(nil))
}

func (s *Service) verify(t *Token) bool {
	return hmac.Equal([]byte(t.Signature), []byte(s.sign(t)))
}

// ListByOwner 查询车主所有 Token
func (s *Service) ListByOwner(ownerID string) []*Token {
	s.mu.RLock()
	defer s.mu.RUnlock()
	var result []*Token
	for _, t := range s.tokens {
		if t.OwnerID == ownerID {
			result = append(result, t)
		}
	}
	return result
}

// ListBySubject 查询接收方的所有 Token
func (s *Service) ListBySubject(subjectID string) []*Token {
	s.mu.RLock()
	defer s.mu.RUnlock()
	var result []*Token
	for _, t := range s.tokens {
		if t.SubjectID == subjectID {
			result = append(result, t)
		}
	}
	return result
}

// Suspend 临时挂起 Token
func (s *Service) Suspend(id, ownerID string) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	t, ok := s.tokens[id]
	if !ok {
		return fmt.Errorf("token not found")
	}
	if t.OwnerID != ownerID {
		return fmt.Errorf("not the token owner")
	}
	t.Status = "suspended"
	return nil
}

// Resume 恢复被挂起的 Token
func (s *Service) Resume(id, ownerID string) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	t, ok := s.tokens[id]
	if !ok {
		return fmt.Errorf("token not found")
	}
	if t.OwnerID != ownerID {
		return fmt.Errorf("not the token owner")
	}
	if t.Status != "suspended" {
		return fmt.Errorf("token is not suspended")
	}
	t.Status = "active"
	return nil
}

func generateID() string {
	b := make([]byte, 16)
	rand.Read(b)
	return "tok_" + hex.EncodeToString(b)
}
