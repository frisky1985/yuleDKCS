package relay

import (
	"context"
	"fmt"
	"strings"
	"sync"
	"time"

	"go.uber.org/zap"
	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/relay/v1"
	apiv1 "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/adapter"
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
	NotificationToken string          // 当前最新 notification token
	SenderToken       string          // 发送方 push token（用于通知发送方）
	ReceiverToken     string          // 接收方 push token（用于通知接收方）
	DisplayInfo       []byte
	Payload           []byte          // 当前加密载荷（已由 sender Secret 加密）
	SharingDataType   int32           // 1=KeyCreation, 2=KeySigning, 3=Import, 6=PinReEntryReq, 7=PinReEntryVal
	ReceiverDeviceID  string
	ReceiverVendor    string
	SharingURL        string          // base URL (不含 secret fragment — secret 由 sender 生成并加到 fragment)
	CreatedAt         time.Time
	ExpiresAt         time.Time
	UpdatedAt         time.Time
	Version           int64
	UpdateCount       int32
	MaxUpdates        int32
	DeviceAttestation []byte // §11.3.5: sender device attestation for cross-OEM auth
}

// ─── MailboxController ───────────────────────────────────────

type MailboxController struct {
	mu              sync.RWMutex
	mailboxes       map[string]*Mailbox
	logger          *zap.Logger
	now             func() time.Time // 可注入，方便测试
	notifier        PushNotifier     // 推送通知接口，默认 NoopPusher
	adapterRegistry *adapter.Registry // 厂商适配器注册中心，可选
}

func NewMailboxController(logger *zap.Logger) *MailboxController {
	return &MailboxController{
		mailboxes: make(map[string]*Mailbox),
		logger:    logger.With(zap.String("component", "mailbox-controller")),
		now:       time.Now,
		notifier:  &NoopPusher{},
	}
}

// WithNotifier 注入推送通知实现
// 默认使用 NoopPusher（不发送任何通知）
func (c *MailboxController) WithNotifier(n PushNotifier) *MailboxController {
	c.notifier = n
	return c
}

// WithAdapterRegistry 注入厂商适配器注册中心
// 设置后在关键状态转移时自动通知对应厂商适配器
func (c *MailboxController) WithAdapterRegistry(reg *adapter.Registry) *MailboxController {
	c.adapterRegistry = reg
	return c
}

// ─── CRUD ────────────────────────────────────────────────────

// Create 创建邮箱 — §11.3.4.1
func (c *MailboxController) Create(ctx context.Context, req *pb.CreateMailboxRequest) (*pb.Mailbox, error) {
	c.mu.Lock()
	defer c.mu.Unlock()

	now := c.now()
	id := generateMailboxID()

	// 生成 mailbox base URL（不含 secret fragment — secret 由 sender 设备生成并附加到 URL 中）
	url := buildSharingURL(id)

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
		SenderToken:       req.NotificationToken,  // 保存发送方 token
		DisplayInfo:       req.DisplayInfo,
		Payload:           req.Payload,
		SharingDataType:   1, // KeyCreationRequest
		SharingURL:        url,
		CreatedAt:         now,
		ExpiresAt:         now.Add(expiry),
		UpdatedAt:         now,
		Version:           1,
		UpdateCount:       1,
		MaxUpdates:        maxUpdates,
		DeviceAttestation: req.DeviceAttestation,
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
	var notifyTitle, notifyBody string        // 推送通知内容
	var notifyToken string                    // 通知目标 token
	switch req.SharingDataType {
	case 2: // KeySigningRequest — 接收方操作
		newStatus = StatusUpdatedByReceiver
		mb.ReceiverDeviceID = req.UpdaterDeviceId
		if req.NotificationToken != "" {
			mb.ReceiverToken = req.NotificationToken
		}
		// 通知发送方
		notifyTitle = "数字钥匙分享已更新"
		notifyBody = "对方已签名，请完成钥匙导入"
		notifyToken = mb.SenderToken
	case 3: // ImportRequest — 发送方操作
		newStatus = StatusUpdatedBySender
		// 通知接收方
		notifyTitle = "钥匙导入成功 🎉"
		notifyBody = "你已收到数字钥匙，快去使用吧"
		notifyToken = mb.ReceiverToken
	case 4: // SenderCancel
		newStatus = StatusCancelled
		notifyTitle = "钥匙分享已取消"
		notifyBody = "发送方取消了钥匙分享"
		notifyToken = mb.ReceiverToken
	case 5: // ReceiverCancel
		newStatus = StatusCancelled
		notifyTitle = "钥匙分享已取消"
		notifyBody = "接收方取消了钥匙分享"
		notifyToken = mb.SenderToken
	case 6: // PinReEntryRequest — 发送方请求重新输入 PIN
		// PinReEntry: 不改变邮箱状态，只更新 payload
		newStatus = mb.Status
		notifyTitle = "需要重新输入 PIN"
		notifyBody = "发送方要求重新输入设备 PIN"
		notifyToken = mb.ReceiverToken
	case 7: // PinReEntryValue — 接收方提供新的 PIN 值
		// PinReEntryValue: 不改变邮箱状态，只更新 payload
		newStatus = mb.Status
		notifyTitle = "PIN 已更新"
		notifyBody = "接收方已提供新的 PIN 值"
		notifyToken = mb.SenderToken
	default:
		return nil, fmt.Errorf("invalid sharing_data_type: %d", req.SharingDataType)
	}

	// 检查状态转移合法性（PinReEntry 不改变状态，跳过校验）
	if newStatus != mb.Status && !isValidTransition(mb.Status, newStatus) {
		return nil, fmt.Errorf(ErrCodeInvalidTransition)
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

	// 发送推送通知（非阻塞：忽略错误，不打断业务流程）
	if notifyToken != "" && notifyTitle != "" {
		_ = c.notifier.Notify(ctx, PushMessage{
			Title:     notifyTitle,
			Body:      notifyBody,
			MailboxID: mb.ID,
			Token:     notifyToken,
			Vendor:    mb.SenderVendor,
			Data: map[string]string{
				"mailbox_id": mb.ID,
				"event":      "mailbox_updated",
				"status":     fmt.Sprintf("%d", newStatus),
			},
		})
	}

	// 通知厂商适配器（非阻塞：忽略错误，不打断业务流程）
	switch req.SharingDataType {
	case 2: // KeySigningRequest — 接收方已签名，通知发送方适配器
		if a := c.getSenderAdapter(mb); a != nil {
			_, _ = a.AcceptShare(ctx, &apiv1.AcceptShareRequest{
				ShareCode: mb.ID,
				DeviceId:  mb.ReceiverDeviceID,
				Vendor:    apiv1.PhoneVendor(apiv1.PhoneVendor_value[strings.ToUpper(mb.ReceiverVendor)]),
			})
		}
	case 3: // ImportRequest — 发送方已完成导入，通知发送方适配器
		if a := c.getSenderAdapter(mb); a != nil {
			_, _ = a.ShareKey(ctx, &apiv1.CreateShareRequest{
				FromUserId: mb.SenderDeviceID,
				ToUserId:   mb.ReceiverDeviceID,
			})
		}
	case 4, 5: // SenderCancel / ReceiverCancel — 通知发送方适配器撤销
		if a := c.getSenderAdapter(mb); a != nil {
			_ = a.RevokeNotify(ctx, mb.ID, "share_cancelled")
		}
	}

	return toProtoMailbox(mb), nil
}

// Delete 删除邮箱 — §11.3.4.3
// 只允许在已完成/已取消状态下删除
func (c *MailboxController) Delete(ctx context.Context, req *pb.DeleteMailboxRequest) (*pb.Mailbox, error) {
	c.mu.Lock()
	defer c.mu.Unlock()

	mb, ok := c.mailboxes[req.MailboxId]
	if !ok {
		return nil, fmt.Errorf(ErrCodeMailboxNotFound)
	}

	// 只允许在终态删除
	if mb.Status != StatusCompleted && mb.Status != StatusCancelled {
		return nil, fmt.Errorf(ErrCodeInvalidTransition)
	}

	wasCancelled := mb.Status == StatusCancelled

	mb.Status = StatusCompleted
	mb.UpdatedAt = c.now()
	mb.Version++

	// 延迟删除（保留一段时间用于审计）
	delete(c.mailboxes, req.MailboxId)

	c.logger.Info("mailbox deleted",
		zap.String("mailbox_id", req.MailboxId),
		zap.String("reason", req.Reason),
	)

	// 如果还未完成（已取消），通知厂商适配器撤销密钥
	if wasCancelled {
		if a := c.getSenderAdapter(mb); a != nil {
			_ = a.RevokeNotify(ctx, mb.ID, req.Reason)
		}
	}

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
// 注意: CCC 规范规定 payload 已由设备端 Secret 加密，relay server 不解密
func (c *MailboxController) ReadSecureContent(ctx context.Context, mailboxID string) ([]byte, int64, error) {
	c.mu.RLock()
	defer c.mu.RUnlock()

	mb, ok := c.mailboxes[mailboxID]
	if !ok {
		return nil, 0, fmt.Errorf(ErrCodeMailboxNotFound)
	}

	if err := c.checkExpired(mb); err != nil {
		return nil, 0, err
	}

	return mb.Payload, mb.Version, nil
}

// Relinquish 转移邮箱 — §11.3.4.6
// 只允许在活跃状态下转移（不可在已取消/已过期/已完成后操作）
func (c *MailboxController) Relinquish(ctx context.Context, req *pb.RelinquishMailboxRequest) (*pb.Mailbox, error) {
	c.mu.Lock()
	defer c.mu.Unlock()

	mb, ok := c.mailboxes[req.MailboxId]
	if !ok {
		return nil, fmt.Errorf(ErrCodeMailboxNotFound)
	}

	if err := c.checkExpired(mb); err != nil {
		return nil, err
	}

	// 只允许在活跃状态下 relinquish
	if mb.Status == StatusCompleted || mb.Status == StatusCancelled || mb.Status == StatusExpired {
		return nil, fmt.Errorf(ErrCodeInvalidTransition)
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

// getSenderAdapter 获取发送方对应的厂商适配器
func (c *MailboxController) getSenderAdapter(mb *Mailbox) adapter.Adapter {
	if c.adapterRegistry == nil {
		return nil
	}
	a, ok := c.adapterRegistry.GetByVendor(mb.SenderVendor)
	if !ok {
		c.logger.Warn("sender vendor adapter not found",
			zap.String("mailbox_id", mb.ID),
			zap.String("vendor", mb.SenderVendor),
		)
		return nil
	}
	return a
}

// checkExpired 检查邮箱是否过期，过期则标记并返回错误
func (c *MailboxController) checkExpired(mb *Mailbox) error {
	if c.now().After(mb.ExpiresAt) && mb.Status != StatusExpired {
		mb.Status = StatusExpired
		return fmt.Errorf(ErrCodeMailboxExpired)
	}
	if mb.Status == StatusExpired {
		return fmt.Errorf(ErrCodeMailboxExpired)
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

func buildSharingURL(mailboxID string) string {
	// base URL（不含 secret fragment — secret 由 sender 生成并附加到 URL fragment 中）
	return fmt.Sprintf("https://dk-relay.yuletech.com/mailbox/%s", mailboxID)
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
		DeviceAttestation: mb.DeviceAttestation,
	}
}
