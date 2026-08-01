package service

import (
	"context"
	"crypto/rand"
	"fmt"
	"sync"
	"time"

	"go.uber.org/zap"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/adapter"
	hub_error "github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/error"
)

// ── 分享状态 (持久化字符串表示, 与 proto ShareStatus 对齐) ──
const (
	ShareStatusPending   = "pending"   // PENDING: 已创建, 等待接受
	ShareStatusAccepted  = "accepted"  // ACCEPTED: 已被接受
	ShareStatusCancelled = "cancelled" // CANCELLED: 已取消
)

// ShareRecord 分享记录的持久化模型。
// 状态机: PENDING → ACCEPTED / CANCELLED
type ShareRecord struct {
	ShareID     string // 分享唯一 id
	KeyID       string // 被分享的钥匙 id
	FromUserID  string // 分享发起方 (车主)
	ToUserID    string // 指定接收方 (为空表示按分享码接受)
	ToVendor    string // 目标厂商 (如 xiaomi)
	ShareCode   string // 分享码 (6位数字, 接收方凭此接受)
	Status      string // pending / accepted / cancelled
	AccessBits  uint32 // 分享授予的权限位掩码
	ValidFrom   int64  // 生效时间 unix millis
	ValidUntil  int64  // 失效时间 unix millis
	MaxUses     int32  // 最大使用次数 (0=不限制)
	FriendKeyID string // 接受后生成的 friend key id
	CreatedAt   int64  // 创建时间 unix millis
	AcceptedAt  int64  // 接受时间 unix millis
	CancelledAt int64  // 取消时间 unix millis
}

// ShareStore 提供分享记录的持久化存储。
// 实现 MUST 是 goroutine-safe。生产环境使用 PostgreSQL 实现
// (internal/store.PostgresStore), 开发/测试使用 InMemoryShareStore。
type ShareStore interface {
	// CreateShare 创建一条分享记录 (状态 PENDING)。
	CreateShare(ctx context.Context, share *ShareRecord) error
	// GetShareByID 按 share_id 查询分享记录。
	GetShareByID(ctx context.Context, shareID string) (*ShareRecord, error)
	// GetShareByCode 按分享码查询分享记录 (用于 AcceptShare 校验)。
	GetShareByCode(ctx context.Context, shareCode string) (*ShareRecord, error)
	// ListSharesByKey 返回某钥匙关联的全部分享记录。
	ListSharesByKey(ctx context.Context, keyID string) ([]*ShareRecord, error)
	// UpdateShare 更新分享记录的可变字段 (状态/接受信息/取消时间)。
	UpdateShare(ctx context.Context, share *ShareRecord) error
}

// InMemoryShareStore 内存版 ShareStore — 进程重启即丢失, 仅用于开发/测试。
type InMemoryShareStore struct {
	mu     sync.RWMutex
	byID   map[string]*ShareRecord
	byCode map[string]string // shareCode -> shareID
}

// NewInMemoryShareStore 创建空的内存分享存储。
func NewInMemoryShareStore() *InMemoryShareStore {
	return &InMemoryShareStore{
		byID:   make(map[string]*ShareRecord),
		byCode: make(map[string]string),
	}
}

func (s *InMemoryShareStore) CreateShare(_ context.Context, share *ShareRecord) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	cp := *share
	s.byID[share.ShareID] = &cp
	if share.ShareCode != "" {
		s.byCode[share.ShareCode] = share.ShareID
	}
	return nil
}

func (s *InMemoryShareStore) GetShareByID(_ context.Context, shareID string) (*ShareRecord, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	rec, ok := s.byID[shareID]
	if !ok {
		return nil, fmt.Errorf("share %s not found", shareID)
	}
	cp := *rec
	return &cp, nil
}

func (s *InMemoryShareStore) GetShareByCode(_ context.Context, shareCode string) (*ShareRecord, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	shareID, ok := s.byCode[shareCode]
	if !ok {
		return nil, fmt.Errorf("share code %s not found", shareCode)
	}
	rec, ok := s.byID[shareID]
	if !ok {
		return nil, fmt.Errorf("share %s not found", shareID)
	}
	cp := *rec
	return &cp, nil
}

func (s *InMemoryShareStore) ListSharesByKey(_ context.Context, keyID string) ([]*ShareRecord, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	var result []*ShareRecord
	for _, rec := range s.byID {
		if rec.KeyID == keyID {
			cp := *rec
			result = append(result, &cp)
		}
	}
	return result, nil
}

func (s *InMemoryShareStore) UpdateShare(_ context.Context, share *ShareRecord) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	old, ok := s.byID[share.ShareID]
	if !ok {
		return fmt.Errorf("share %s not found", share.ShareID)
	}
	// 分享码变更时维护 code 索引
	if old.ShareCode != "" && old.ShareCode != share.ShareCode {
		delete(s.byCode, old.ShareCode)
	}
	cp := *share
	s.byID[share.ShareID] = &cp
	if share.ShareCode != "" {
		s.byCode[share.ShareCode] = share.ShareID
	}
	return nil
}

// ── KeyShareService ─────────────────────────────────────────

type KeyShareService struct {
	pb.UnimplementedKeyShareServiceServer
	registry   *adapter.Registry
	logger     *zap.Logger
	shareStore ShareStore
	keyStore   KeyStore // 可选: 校验被分享的钥匙存在/活跃
}

func NewKeyShareService(registry *adapter.Registry, logger *zap.Logger) *KeyShareService {
	return &KeyShareService{
		registry:   registry,
		logger:     logger.With(zap.String("service", "KeyShare")),
		shareStore: NewInMemoryShareStore(),
	}
}

// WithShareStore 注入分享记录存储 (生产环境传 PostgreSQL 实现)。
func (s *KeyShareService) WithShareStore(ss ShareStore) *KeyShareService {
	s.shareStore = ss
	return s
}

// WithKeyStore 注入钥匙存储, 用于 CreateShare 校验被分享钥匙存在。
func (s *KeyShareService) WithKeyStore(ks KeyStore) *KeyShareService {
	s.keyStore = ks
	return s
}

// shareStatusToProto 将持久化状态字符串映射为 proto ShareStatus。
func shareStatusToProto(st string) pb.ShareStatus {
	switch st {
	case ShareStatusPending:
		return pb.ShareStatus_PENDING
	case ShareStatusAccepted:
		return pb.ShareStatus_ACCEPTED
	case ShareStatusCancelled:
		return pb.ShareStatus_CANCELLED
	default:
		return pb.ShareStatus_SHARE_STATUS_UNSPECIFIED
	}
}

// genShareID 生成唯一分享 id。
func genShareID() string {
	return fmt.Sprintf("share-%s", randHex(16))
}

// genShareCode 生成 6 位数字分享码; 与现有分享码冲突时重新生成。
func (s *KeyShareService) genShareCode(ctx context.Context) (string, error) {
	for i := 0; i < 10; i++ {
		code := fmt.Sprintf("%06d", randInt(1000000))
		if _, err := s.shareStore.GetShareByCode(ctx, code); err != nil {
			return code, nil // 未占用
		}
	}
	return "", fmt.Errorf("failed to generate unique share code")
}

// randHex 生成 n 字节的随机十六进制字符串。
func randHex(n int) string {
	b := make([]byte, n)
	if _, err := rand.Read(b); err != nil {
		return fmt.Sprintf("%d", time.Now().UnixNano())
	}
	return fmt.Sprintf("%x", b)
}

// randInt 生成 [0, max) 范围内的随机整数。
func randInt(max int64) int64 {
	var b [8]byte
	if _, err := rand.Read(b[:]); err != nil {
		return time.Now().UnixNano() % max
	}
	v := int64(b[0])<<56 | int64(b[1])<<48 | int64(b[2])<<40 | int64(b[3])<<32 |
		int64(b[4])<<24 | int64(b[5])<<16 | int64(b[6])<<8 | int64(b[7])
	if v < 0 {
		v = -v
	}
	return v % max
}

// CreateShare 创建密钥分享: 生成 shareId + 持久化 PENDING + 关联 keyId,
// 然后委托厂商适配器做车端侧分享准备 (CCC Mailbox / ICCOA S2S session)。
func (s *KeyShareService) CreateShare(ctx context.Context, req *pb.CreateShareRequest) (*pb.CreateShareResponse, error) {
	s.logger.Info("CreateShare",
		zap.String("key_id", req.KeyId),
		zap.String("from_user", req.FromUserId),
		zap.String("to_vendor", req.ToVendor.String()),
	)

	if req.KeyId == "" {
		return nil, status.Error(codes.InvalidArgument, "key_id is required")
	}

	// 校验被分享的钥匙存在 (配置了 keyStore 时)
	if s.keyStore != nil {
		rec, err := s.keyStore.GetKeyRecord(ctx, req.KeyId)
		if err != nil {
			s.logger.Warn("CreateShare: key not found", zap.String("key_id", req.KeyId), zap.Error(err))
			return nil, status.Error(codes.NotFound, hub_error.GetErrorMessage(hub_error.ERR_KEY_NOT_FOUND))
		}
		if rec.Status != "active" {
			return nil, status.Error(codes.FailedPrecondition,
				fmt.Sprintf("key %s is not active (status=%s)", req.KeyId, rec.Status))
		}
	}

	// 查找目标厂商适配器 (CCC → Mailbox 中继, ICCOA/ICCE → S2S)
	a, ok := s.registry.GetByVendor(req.ToVendor.String())
	if !ok {
		return &pb.CreateShareResponse{
			ErrorCode: "ADAPTER_NOT_FOUND",
		}, nil
	}

	// 委托适配器做车端侧分享准备 (失败时返回错误, 不落库)
	adapterResp, err := a.ShareKey(ctx, req)
	if err != nil {
		s.logger.Error("CreateShare: adapter error", zap.Error(err))
		return nil, err
	}

	// 生成 shareId + 分享码并持久化 PENDING 状态
	shareID := genShareID()
	shareCode := adapterResp.GetShareCode()
	if shareCode == "" {
		code, err := s.genShareCode(ctx)
		if err != nil {
			return nil, status.Error(codes.Internal, "failed to generate share code")
		}
		shareCode = code
	}

	now := time.Now().UnixMilli()
	share := &ShareRecord{
		ShareID:    shareID,
		KeyID:      req.KeyId,
		FromUserID: req.FromUserId,
		ToUserID:   req.ToUserId,
		ToVendor:   req.ToVendor.String(),
		ShareCode:  shareCode,
		Status:     ShareStatusPending,
		AccessBits: accessLevelToBits(req.AccessLevel),
		ValidFrom:  req.ValidFrom,
		ValidUntil: req.ValidUntil,
		MaxUses:    req.MaxUses,
		CreatedAt:  now,
	}
	if err := s.shareStore.CreateShare(ctx, share); err != nil {
		s.logger.Error("CreateShare: persist failed", zap.Error(err))
		return nil, status.Error(codes.Internal, "failed to persist share")
	}

	s.logger.Info("CreateShare persisted",
		zap.String("share_id", shareID),
		zap.String("share_code", shareCode),
		zap.String("status", ShareStatusPending),
	)

	return &pb.CreateShareResponse{
		ShareId:    shareID,
		ShareCode:  shareCode,
		SharingUrl: adapterResp.GetSharingUrl(), // CCC 协议: Mailbox sharing URL
	}, nil
}

// AcceptShare 接受密钥分享: 校验分享码存在且状态为 PENDING,
// 流转为 ACCEPTED 并委托适配器生成 friend key。
func (s *KeyShareService) AcceptShare(ctx context.Context, req *pb.AcceptShareRequest) (*pb.AcceptShareResponse, error) {
	s.logger.Info("AcceptShare",
		zap.String("share_code", req.ShareCode),
		zap.String("user_id", req.UserId),
		zap.String("vendor", req.Vendor.String()),
	)

	if req.ShareCode == "" {
		return nil, status.Error(codes.InvalidArgument, "share_code is required")
	}

	// [HUB-2] 校验分享码有效性
	share, err := s.shareStore.GetShareByCode(ctx, req.ShareCode)
	if err != nil {
		s.logger.Warn("AcceptShare: invalid share code", zap.String("share_code", req.ShareCode))
		return nil, status.Error(codes.NotFound, hub_error.GetErrorMessage(hub_error.ERR_CODE_INVALID))
	}

	// 状态机校验: 仅 PENDING 可接受
	switch share.Status {
	case ShareStatusAccepted:
		return nil, status.Error(codes.FailedPrecondition, hub_error.GetErrorMessage(hub_error.ERR_SHARE_ACCEPTED))
	case ShareStatusCancelled:
		return nil, status.Error(codes.FailedPrecondition, hub_error.GetErrorMessage(hub_error.ERR_SHARE_CANCELLED))
	}

	// 过期校验
	if share.ValidUntil > 0 && time.Now().UnixMilli() > share.ValidUntil {
		return nil, status.Error(codes.FailedPrecondition, hub_error.GetErrorMessage(hub_error.ERR_SHARE_EXPIRED))
	}

	// 指定接收方校验 (CreateShare 指定 to_user_id 时仅其可接受)
	if share.ToUserID != "" && req.UserId != "" && share.ToUserID != req.UserId {
		s.logger.Warn("AcceptShare: user mismatch",
			zap.String("expected", share.ToUserID),
			zap.String("actual", req.UserId),
		)
		return nil, status.Error(codes.PermissionDenied, hub_error.GetErrorMessage(hub_error.ERR_ACCESS_DENIED))
	}

	// 委托适配器生成 friend key (车端侧签发)
	a, ok := s.registry.GetByVendor(req.Vendor.String())
	if !ok {
		return &pb.AcceptShareResponse{ErrorCode: "ADAPTER_NOT_FOUND"}, nil
	}
	resp, err := a.AcceptShare(ctx, req)
	if err != nil {
		s.logger.Error("AcceptShare: adapter error", zap.Error(err))
		return nil, err
	}

	// 状态流转: PENDING → ACCEPTED
	now := time.Now().UnixMilli()
	friendKeyID := ""
	if resp != nil && resp.Key != nil {
		friendKeyID = resp.Key.KeyId
	}
	share.Status = ShareStatusAccepted
	share.AcceptedAt = now
	share.FriendKeyID = friendKeyID
	if err := s.shareStore.UpdateShare(ctx, share); err != nil {
		s.logger.Error("AcceptShare: status update failed", zap.Error(err))
		return nil, status.Error(codes.Internal, "failed to update share status")
	}

	// 持久化 friend key 元数据 (配置了 keyStore 时)
	if s.keyStore != nil && resp != nil && resp.Key != nil && resp.Key.KeyId != "" {
		keyRecord := &KeyRecord{
			KeyID:       resp.Key.KeyId,
			OwnerUserID: req.UserId,
			VehicleID:   resp.Key.VehicleId,
			Vendor:      req.Vendor.String(),
			Status:      "active",
			AccessBits:  share.AccessBits,
			CreatedAt:   now,
		}
		if err := s.keyStore.SetKey(ctx, keyRecord); err != nil {
			s.logger.Warn("AcceptShare: friend key persist failed", zap.Error(err))
		}
	}

	s.logger.Info("AcceptShare accepted",
		zap.String("share_id", share.ShareID),
		zap.String("friend_key_id", friendKeyID),
	)

	return resp, nil
}

// CancelShare 取消密钥分享: PENDING/ACCEPTED → CANCELLED。
// 已接受的情况下同时吊销生成的 friend key。
func (s *KeyShareService) CancelShare(ctx context.Context, req *pb.CancelShareRequest) (*pb.CancelShareResponse, error) {
	s.logger.Info("CancelShare", zap.String("share_id", req.ShareId))

	if req.ShareId == "" {
		return nil, status.Error(codes.InvalidArgument, "share_id is required")
	}

	share, err := s.shareStore.GetShareByID(ctx, req.ShareId)
	if err != nil {
		return nil, status.Error(codes.NotFound, hub_error.GetErrorMessage(hub_error.ERR_SHARE_NOT_FOUND))
	}

	// 状态机校验: 仅 PENDING/ACCEPTED 可取消
	switch share.Status {
	case ShareStatusCancelled:
		return nil, status.Error(codes.FailedPrecondition, hub_error.GetErrorMessage(hub_error.ERR_SHARE_CANCELLED))
	case ShareStatusPending, ShareStatusAccepted:
		// 合法流转
	default:
		return nil, status.Error(codes.FailedPrecondition, "share in invalid state to cancel")
	}

	// 状态流转: PENDING/ACCEPTED → CANCELLED
	now := time.Now().UnixMilli()
	share.Status = ShareStatusCancelled
	share.CancelledAt = now
	if err := s.shareStore.UpdateShare(ctx, share); err != nil {
		s.logger.Error("CancelShare: status update failed", zap.Error(err))
		return nil, status.Error(codes.Internal, "failed to update share status")
	}

	// 已接受的分享: 吊销 friend key (配置了 keyStore 时)
	if share.FriendKeyID != "" && s.keyStore != nil {
		if err := s.keyStore.SetKeyStatus(ctx, share.FriendKeyID, "revoked"); err != nil {
			s.logger.Warn("CancelShare: revoke friend key failed",
				zap.String("friend_key_id", share.FriendKeyID),
				zap.Error(err),
			)
		}
	}

	s.logger.Info("CancelShare cancelled", zap.String("share_id", req.ShareId))
	return &pb.CancelShareResponse{}, nil
}

// GetShare 查询分享信息 (含状态)。
func (s *KeyShareService) GetShare(ctx context.Context, req *pb.GetShareRequest) (*pb.GetShareResponse, error) {
	s.logger.Info("GetShare", zap.String("share_id", req.ShareId))

	if req.ShareId == "" {
		return nil, status.Error(codes.InvalidArgument, "share_id is required")
	}

	share, err := s.shareStore.GetShareByID(ctx, req.ShareId)
	if err != nil {
		return nil, status.Error(codes.NotFound, hub_error.GetErrorMessage(hub_error.ERR_SHARE_NOT_FOUND))
	}

	return &pb.GetShareResponse{
		ShareId:     share.ShareID,
		KeyId:       share.KeyID,
		FromUserId:  share.FromUserID,
		AccessLevel: bitsToAccessLevel(share.AccessBits),
		ValidFrom:   share.ValidFrom,
		ValidUntil:  share.ValidUntil,
		Status:      shareStatusToProto(share.Status),
		ShareCode:   share.ShareCode,
		ToUserId:    share.ToUserID,
	}, nil
}
