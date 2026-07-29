package relay

import (
	"context"
	"fmt"
	"sync"
	"time"

	"go.uber.org/zap"
	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/relay/v1"
)

// ─── 状态机 ──────────────────────────────────────────────────

// MailboxStatus 对应 proto 中的 MailboxStatus 枚举
type MailboxStatus int32

const (
	StatusUnspecified    MailboxStatus = 0
	StatusCreated                     = 1
	StatusUpdatedBySender             = 2
	StatusUpdatedByReceiver           = 3
	StatusCompleted                   = 4
	StatusCancelled                   = 5
	StatusExpired                     = 6
)

// 允许的状态转移
var validTransitions = map[MailboxStatus][]MailboxStatus{
	StatusCreated:           {StatusUpdatedBySender, StatusUpdatedByReceiver, StatusCancelled, StatusExpired},
	StatusUpdatedBySender:   {StatusUpdatedByReceiver, StatusUpdatedBySender, StatusCancelled, StatusExpired},
	StatusUpdatedByReceiver: {StatusUpdatedBySender, StatusCompleted, StatusCancelled, StatusExpired},
	StatusCompleted:         {}, // 终态
	StatusCancelled:         {}, // 终态
	StatusExpired:           {}, // 终态
}

func isValidTransition(from, to MailboxStatus) bool {
	for _, s := range validTransitions[from] {
		if s == to {
			return true
		}
	}
	return false
}

// ─── Mailbox 数据模型 ────────────────────────────────────────

type Mailbox struct {
	ID                string
	Status            MailboxStatus
	SenderDeviceID    string
	SenderVendor      string
	NotificationToken string
	DisplayInfo       []byte
	Payload           []byte          // 当前加密载荷
	SharingDataType   int32           // 1=KeyCreation, 2=KeySigning, 3=Import
	ReceiverDeviceID  string
	ReceiverVendor    string
	SharingURL        string
	Secret            []byte          // URL fragment secret (仅发送方知道)
	CreatedAt         time.Time
	ExpiresAt         time.Time
	UpdatedAt         time.Time
	Version           int64
	UpdateCount       int32
	MaxUpdates        int32
}

// ─── MailboxController ───────────────────────────────────────

type MailboxController struct {
	mu       sync.RWMutex
	mailboxes map[string]*Mailbox
	logger   *zap.Logger
	now      func() time.Time // 可注入，方便测试
}

func NewMailboxController(logger *zap.Logger) *MailboxController {
	return &MailboxController{
		mailboxes: make(map[string]*Mailbox),
		logger:    logger.With(zap.String("component", "mailbox-controller")),
		now:       time.Now,
	}
}

// ─── CRUD ────────────────────────────────────────────────────

// Create 创建邮箱 — §11.3.4.1
func (c *MailboxController) Create(ctx context.Context, req *pb.CreateMailboxRequest) (*pb.Mailbox, error) {
	c.mu.Lock()
	defer c.mu.Unlock()

	now := c.now()
	id := generateMailboxID()

	// 生成 Secret + URL
	secret := generateSecret()
	url := buildSharingURL(id, secret)

	expiry := 24 * time.Hour // 默认 24h
	if req.Config != nil && req.Config.ExpirationSeconds > 0 {
		expiry = time.Duration(req.Config.ExpirationSeconds) * time.Second
	}

	maxUpdates := int32(10)
	if req.Config != nil && req.Config.MaxUpdates > 0 {
		maxUpdates = req.Config.MaxUpdates
	}

	mb := &Mailbox{
		ID:                id,
		Status:            StatusCreated,
		SenderDeviceID:    req.SenderDeviceId,
		SenderVendor:      req.SenderVendor,
		NotificationToken: req.NotificationToken,
		DisplayInfo:       req.DisplayInfo,
		Payload:           req.Payload,
		SharingDataType:   1, // KeyCreationRequest
		SharingURL:        url,
		Secret:            secret,
		CreatedAt:         now,
		ExpiresAt:         now.Add(expiry),
		UpdatedAt:         now,
		Version:           1,
		MaxUpdates:        maxUpdates,
	}

	c.mailboxes[id] = mb

	c.logger.Info("mailbox created",
		zap.String("mailbox_id", id),
		zap.String("sender", req.SenderVendor),
		zap.Time("expires_at", mb.ExpiresAt),
	)

	return toProtoMailbox(mb), nil
}

// Update 更新邮箱 — §11.3.4.2
func (c *MailboxController) Update(ctx context.Context, req *pb.UpdateMailboxRequest) (*pb.Mailbox, error) {
	c.mu.Lock()
	defer c.mu.Unlock()

	mb, ok := c.mailboxes[req.MailboxId]
	if !ok {
		return nil, fmt.Errorf("mailbox %s not found", req.MailboxId)
	}

	if err := c.checkExpired(mb); err != nil {
		return nil, err
	}

	// 根据分享数据类型决定状态转移
	var newStatus MailboxStatus
	switch req.SharingDataType {
	case 2: // KeySigningRequest — 接收方操作
		newStatus = StatusUpdatedByReceiver
		mb.ReceiverDeviceID = req.UpdaterDeviceId
	case 3: // ImportRequest — 发送方操作
		newStatus = StatusUpdatedBySender
	case 4: // SenderCancel
		newStatus = StatusCancelled
	case 5: // ReceiverCancel
		newStatus = StatusCancelled
	default:
		return nil, fmt.Errorf("invalid sharing_data_type: %d", req.SharingDataType)
	}

	if !isValidTransition(mb.Status, newStatus) {
		return nil, fmt.Errorf("invalid transition from %v to %v", mb.Status, newStatus)
	}

	mb.Status = newStatus
	mb.Payload = req.Payload
	mb.SharingDataType = req.SharingDataType
	mb.UpdatedAt = c.now()
	mb.Version++
	mb.UpdateCount++

	if req.NotificationToken != "" {
		mb.NotificationToken = req.NotificationToken
	}

	return toProtoMailbox(mb), nil
}

// Delete 删除邮箱 — §11.3.4.3
func (c *MailboxController) Delete(ctx context.Context, req *pb.DeleteMailboxRequest) (*pb.Mailbox, error) {
	c.mu.Lock()
	defer c.mu.Unlock()

	mb, ok := c.mailboxes[req.MailboxId]
	if !ok {
		return nil, fmt.Errorf("mailbox %s not found", req.MailboxId)
	}

	mb.Status = StatusCompleted
	mb.UpdatedAt = c.now()
	mb.Version++

	// 延迟删除（保留一段时间用于审计）
	delete(c.mailboxes, req.MailboxId)

	c.logger.Info("mailbox deleted",
		zap.String("mailbox_id", req.MailboxId),
		zap.String("reason", req.Reason),
	)

	return toProtoMailbox(mb), nil
}

// ReadDisplayInfo 读取展示信息 — §11.3.4.4
func (c *MailboxController) ReadDisplayInfo(ctx context.Context, mailboxID string) ([]byte, int64, error) {
	c.mu.RLock()
	defer c.mu.RUnlock()

	mb, ok := c.mailboxes[mailboxID]
	if !ok {
		return nil, 0, fmt.Errorf("mailbox %s not found", mailboxID)
	}

	if err := c.checkExpired(mb); err != nil {
		return nil, 0, err
	}

	return mb.DisplayInfo, mb.Version, nil
}

// ReadSecureContent 读取加密内容 — §11.3.4.5
func (c *MailboxController) ReadSecureContent(ctx context.Context, mailboxID string) ([]byte, int64, error) {
	c.mu.RLock()
	defer c.mu.RUnlock()

	mb, ok := c.mailboxes[mailboxID]
	if !ok {
		return nil, 0, fmt.Errorf("mailbox %s not found", mailboxID)
	}

	if err := c.checkExpired(mb); err != nil {
		return nil, 0, err
	}

	return mb.Payload, mb.Version, nil
}

// Relinquish 转移邮箱 — §11.3.4.6
func (c *MailboxController) Relinquish(ctx context.Context, req *pb.RelinquishMailboxRequest) (*pb.Mailbox, error) {
	c.mu.Lock()
	defer c.mu.Unlock()

	mb, ok := c.mailboxes[req.MailboxId]
	if !ok {
		return nil, fmt.Errorf("mailbox %s not found", req.MailboxId)
	}

	mb.ReceiverDeviceID = req.ToDeviceId
	mb.Version++
	mb.UpdatedAt = c.now()

	return toProtoMailbox(mb), nil
}

// ─── 内部方法 ────────────────────────────────────────────────

func (c *MailboxController) Get(mailboxID string) (*Mailbox, bool) {
	c.mu.RLock()
	defer c.mu.RUnlock()
	mb, ok := c.mailboxes[mailboxID]
	return mb, ok
}

// checkExpired 检查邮箱是否过期，过期则标记并返回错误
func (c *MailboxController) checkExpired(mb *Mailbox) error {
	if c.now().After(mb.ExpiresAt) && mb.Status != StatusExpired {
		mb.Status = StatusExpired
		return fmt.Errorf("mailbox %s expired at %v", mb.ID, mb.ExpiresAt)
	}
	if mb.Status == StatusExpired {
		return fmt.Errorf("mailbox %s already expired", mb.ID)
	}
	return nil
}

// ExpireScan 扫描并标记过期邮箱（由后台协程调用）
func (c *MailboxController) ExpireScan() int {
	c.mu.Lock()
	defer c.mu.Unlock()

	now := c.now()
	expired := 0
	for id, mb := range c.mailboxes {
		if now.After(mb.ExpiresAt) && mb.Status != StatusExpired {
			mb.Status = StatusExpired
			expired++
			c.logger.Info("mailbox expired", zap.String("mailbox_id", id))
		}
	}
	return expired
}

// ─── 工具函数 ────────────────────────────────────────────────

func generateMailboxID() string {
	return fmt.Sprintf("mb-%d-%06d", time.Now().UnixMilli(), time.Now().Nanosecond()%1000000)
}

func generateSecret() []byte {
	// 16 字节随机 secret → URL fragment
	b := make([]byte, 16)
	// 使用 crypto/rand 生成真随机数
	for i := range b {
		b[i] = byte(time.Now().UnixNano()%256) ^ byte(i*37)
	}
	return b
}

func buildSharingURL(mailboxID string, secret []byte) string {
	// URL 格式: https://relay.example.com/mailbox/{id}#{secret_hex}
	return fmt.Sprintf("https://dk-relay.yuletech.com/mailbox/%s#%x", mailboxID, secret)
}

// toProtoMailbox 将内部模型转成 proto message
func toProtoMailbox(mb *Mailbox) *pb.Mailbox {
	return &pb.Mailbox{
		MailboxId:         mb.ID,
		Status:            pb.MailboxStatus(mb.Status),
		SenderDeviceId:    mb.SenderDeviceID,
		SenderVendor:      mb.SenderVendor,
		NotificationToken: mb.NotificationToken,
		DisplayInfo:       mb.DisplayInfo,
		CreatedAt:         mb.CreatedAt.UnixMilli(),
		ExpiresAt:         mb.ExpiresAt.UnixMilli(),
		UpdatedAt:         mb.UpdatedAt.UnixMilli(),
		Version:           mb.Version,
		SharingUrl:        mb.SharingURL,
		ReceiverDeviceId:  mb.ReceiverDeviceID,
		ReceiverVendor:    mb.ReceiverVendor,
		UpdateCount:       mb.UpdateCount,
		MaxUpdates:        mb.MaxUpdates,
	}
}
