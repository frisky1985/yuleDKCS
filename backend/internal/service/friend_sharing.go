package service

import (
	"context"
	"crypto/rand"
	"encoding/base64"
	"encoding/json"
	"errors"
	"fmt"
	"time"

	"github.com/google/uuid"
	"gorm.io/gorm"
	"yuleDKCS/backend/internal/model"
	"yuleDKCS/backend/internal/repository"
	"yuleDKCS/backend/pkg/logger"
	"yuleDKCS/backend/pkg/notification"
)

// FriendSharingService 朋友分享服务
type FriendSharingService struct {
	db             *gorm.DB
	vehicleRepo    repository.VehicleRepository
	userRepo       repository.UserRepository
	sharingRepo    repository.KeySharingRepository
	notifier       notification.Notifier
	vehicleService *VehicleService
}

// NewFriendSharingService 创建朋友分享服务
func NewFriendSharingService(
	db *gorm.DB,
	vehicleRepo repository.VehicleRepository,
	userRepo repository.UserRepository,
	sharingRepo repository.KeySharingRepository,
	notifier notification.Notifier,
	vehicleService *VehicleService,
) *FriendSharingService {
	return &FriendSharingService{
		db:             db,
		vehicleRepo:    vehicleRepo,
		userRepo:       userRepo,
		sharingRepo:    sharingRepo,
		notifier:       notifier,
		vehicleService: vehicleService,
	}
}

// ShareKeyRequest 分享钥匙请求
type ShareKeyRequest struct {
	OwnerID     string   `json:"owner_id" binding:"required"`
	FriendID    string   `json:"friend_id" binding:"required"`
	VehicleID   string   `json:"vehicle_id" binding:"required"`
	Permissions []string `json:"permissions" binding:"required"`
	ExpiryDays  int      `json:"expiry_days" binding:"min=1,max=365"`
	Message     string   `json:"message"`
}

// ShareKeyResponse 分享钥匙响应
type ShareKeyResponse struct {
	ShareID       string    `json:"share_id"`
	InvitationCode string   `json:"invitation_code"`
	Status        string    `json:"status"`
	ExpiresAt     time.Time `json:"expires_at"`
}

// ShareKey 分享数字钥匙给朋友
func (s *FriendSharingService) ShareKey(ctx context.Context, req *ShareKeyRequest) (*ShareKeyResponse, error) {
	logger.Info("ShareKey", logger.String("owner", req.OwnerID), logger.String("friend", req.FriendID))

	// 1. 验证车辆所有权
	vehicle, err := s.vehicleRepo.GetByID(ctx, req.VehicleID)
	if err != nil {
		return nil, fmt.Errorf("获取车辆信息失败: %w", err)
	}
	if vehicle.OwnerID != req.OwnerID {
		return nil, errors.New("无权分享该车辆的钥匙")
	}

	// 2. 验证好友存在
	friend, err := s.userRepo.GetByID(ctx, req.FriendID)
	if err != nil {
		return nil, fmt.Errorf("获取好友信息失败: %w", err)
	}

	// 3. 验证权限
	if err := s.validatePermissions(req.Permissions); err != nil {
		return nil, err
	}

	// 4. 生成唯一邀请码
	invitationCode, err := s.generateInvitationCode()
	if err != nil {
		return nil, fmt.Errorf("生成邀请码失败: %w", err)
	}

	// 5. 计算过期时间
	expiresAt := time.Now().AddDate(0, 0, req.ExpiryDays)

	// 6. 创建分享记录
	share := &model.KeySharing{
		ID:             uuid.New().String(),
		OwnerID:        req.OwnerID,
		FriendID:       req.FriendID,
		VehicleID:      req.VehicleID,
		Permissions:    s.serializePermissions(req.Permissions),
		Status:         model.SharingStatusPending,
		InvitationCode: invitationCode,
		Message:        req.Message,
		CreatedAt:      time.Now(),
		ExpiresAt:      expiresAt,
	}

	if err := s.sharingRepo.Create(ctx, share); err != nil {
		return nil, fmt.Errorf("创建分享记录失败: %w", err)
	}

	// 7. 发送通知给好友
	if err := s.sendInvitationNotification(friend, vehicle, share); err != nil {
		logger.Warn("发送邀请通知失败", logger.Error(err))
	}

	return &ShareKeyResponse{
		ShareID:        share.ID,
		InvitationCode: invitationCode,
		Status:         string(share.Status),
		ExpiresAt:      expiresAt,
	}, nil
}

// AcceptInvitationRequest 接受邀请请求
type AcceptInvitationRequest struct {
	UserID         string `json:"user_id" binding:"required"`
	InvitationCode string `json:"invitation_code" binding:"required"`
}

// AcceptInvitation 接受钥匙分享邀请
func (s *FriendSharingService) AcceptInvitation(ctx context.Context, req *AcceptInvitationRequest) error {
	logger.Info("AcceptInvitation", logger.String("user", req.UserID), logger.String("code", req.InvitationCode))

	// 1. 查找分享记录
	share, err := s.sharingRepo.GetByInvitationCode(ctx, req.InvitationCode)
	if err != nil {
		return fmt.Errorf("无效的邀请码: %w", err)
	}

	// 2. 验证用户
	if share.FriendID != req.UserID {
		return errors.New("该邀请码不属于您")
	}

	// 3. 检查状态
	if share.Status != model.SharingStatusPending {
		return errors.New("邀请已过期或已处理")
	}

	// 4. 检查是否过期
	if time.Now().After(share.ExpiresAt) {
		share.Status = model.SharingStatusExpired
		s.sharingRepo.Update(ctx, share)
		return errors.New("邀请已过期")
	}

	// 5. 生成临时数字钥匙
	tempKey, err := s.generateTemporaryKey()
	if err != nil {
		return fmt.Errorf("生成临时钥匙失败: %w", err)
	}

	// 6. 更新状态
	share.Status = model.SharingStatusActive
	share.TempKey = tempKey
	share.ActivatedAt = time.Now()

	if err := s.sharingRepo.Update(ctx, share); err != nil {
		return fmt.Errorf("激活钥匙失败: %w", err)
	}

	// 7. 向车辆发送配置更新
	if err := s.vehicleService.ConfigureTemporaryKey(ctx, share.VehicleID, share.FriendID, tempKey, share.Permissions); err != nil {
		logger.Error("配置临时钥匙失败", logger.Error(err))
	}

	// 8. 通知所有者
	owner, _ := s.userRepo.GetByID(ctx, share.OwnerID)
	if owner != nil {
		s.notifier.Send(ctx, owner.Phone, fmt.Sprintf("您的好友已接受 %s 的钥匙分享", share.VehicleID))
	}

	return nil
}

// RevokeKeyRequest 撤回钥匙请求
type RevokeKeyRequest struct {
	OwnerID string `json:"owner_id" binding:"required"`
	ShareID string `json:"share_id" binding:"required"`
	Reason  string `json:"reason"`
}

// RevokeKey 撤回分享的数字钥匙
func (s *FriendSharingService) RevokeKey(ctx context.Context, req *RevokeKeyRequest) error {
	logger.Info("RevokeKey", logger.String("owner", req.OwnerID), logger.String("share", req.ShareID))

	// 1. 查找分享记录
	share, err := s.sharingRepo.GetByID(ctx, req.ShareID)
	if err != nil {
		return fmt.Errorf("获取分享记录失败: %w", err)
	}

	// 2. 验证所有权
	if share.OwnerID != req.OwnerID {
		return errors.New("无权撤回该钥匙")
	}

	// 3. 检查是否可撤回
	if share.Status == model.SharingStatusRevoked {
		return errors.New("钥匙已被撤回")
	}

	if share.Status == model.SharingStatusExpired {
		return errors.New("钥匙已过期")
	}

	// 4. 更新状态
	share.Status = model.SharingStatusRevoked
	share.RevokedAt = time.Now()
	share.RevokeReason = req.Reason

	if err := s.sharingRepo.Update(ctx, share); err != nil {
		return fmt.Errorf("撤回钥匙失败: %w", err)
	}

	// 5. 通知车辆禁用该钥匙
	if err := s.vehicleService.RevokeTemporaryKey(ctx, share.VehicleID, share.FriendID); err != nil {
		logger.Error("禁用临时钥匙失败", logger.Error(err))
	}

	// 6. 通知受共享者
	friend, _ := s.userRepo.GetByID(ctx, share.FriendID)
	if friend != nil {
		message := fmt.Sprintf("您对车辆 %s 的访问权限已被撤回", share.VehicleID)
		if req.Reason != "" {
			message += fmt.Sprintf(" (原因: %s)", req.Reason)
		}
		s.notifier.Send(ctx, friend.Phone, message)
	}

	return nil
}

// UpdatePermissionsRequest 更新权限请求
type UpdatePermissionsRequest struct {
	OwnerID     string   `json:"owner_id" binding:"required"`
	ShareID     string   `json:"share_id" binding:"required"`
	Permissions []string `json:"permissions" binding:"required"`
}

// UpdatePermissions 更新分享权限
func (s *FriendSharingService) UpdatePermissions(ctx context.Context, req *UpdatePermissionsRequest) error {
	logger.Info("UpdatePermissions", logger.String("share", req.ShareID))

	// 1. 查找分享记录
	share, err := s.sharingRepo.GetByID(ctx, req.ShareID)
	if err != nil {
		return fmt.Errorf("获取分享记录失败: %w", err)
	}

	// 2. 验证所有权
	if share.OwnerID != req.OwnerID {
		return errors.New("无权修改该分享")
	}

	// 3. 验证权限
	if err := s.validatePermissions(req.Permissions); err != nil {
		return err
	}

	// 4. 更新权限
	share.Permissions = s.serializePermissions(req.Permissions)

	if err := s.sharingRepo.Update(ctx, share); err != nil {
		return fmt.Errorf("更新权限失败: %w", err)
	}

	// 5. 更新车辆端权限
	if share.Status == model.SharingStatusActive {
		if err := s.vehicleService.UpdateKeyPermissions(ctx, share.VehicleID, share.FriendID, req.Permissions); err != nil {
			logger.Error("更新车辆端权限失败", logger.Error(err))
		}
	}

	return nil
}

// GetSharedKeysRequest 获取分享列表请求
type GetSharedKeysRequest struct {
	UserID    string `form:"user_id" binding:"required"`
	Role      string `form:"role"` // owner 或 friend
	VehicleID string `form:"vehicle_id"`
	Status    string `form:"status"`
	Page      int    `form:"page,default=1"`
	PageSize  int    `form:"page_size,default=10"`
}

// SharedKeyInfo 分享钥匙信息
type SharedKeyInfo struct {
	ID             string    `json:"id"`
	VehicleID      string    `json:"vehicle_id"`
	VehicleName    string    `json:"vehicle_name"`
	OwnerID        string    `json:"owner_id"`
	OwnerName      string    `json:"owner_name"`
	FriendID       string    `json:"friend_id"`
	FriendName     string    `json:"friend_name"`
	Permissions    []string  `json:"permissions"`
	Status         string    `json:"status"`
	CreatedAt      time.Time `json:"created_at"`
	ExpiresAt      time.Time `json:"expires_at"`
	ActivatedAt    *time.Time `json:"activated_at,omitempty"`
	RevokedAt      *time.Time `json:"revoked_at,omitempty"`
	RevokeReason   string    `json:"revoke_reason,omitempty"`
}

// GetSharedKeys 获取分享的钥匙列表
func (s *FriendSharingService) GetSharedKeys(ctx context.Context, req *GetSharedKeysRequest) ([]*SharedKeyInfo, int64, error) {
	// 默认分页参数
	if req.Page < 1 {
		req.Page = 1
	}
	if req.PageSize < 1 || req.PageSize > 100 {
		req.PageSize = 10
	}

	// 构建查询条件
	query := &repository.SharingQuery{
		Page:     req.Page,
		PageSize: req.PageSize,
	}

	if req.Role == "owner" {
		query.OwnerID = req.UserID
	} else if req.Role == "friend" {
		query.FriendID = req.UserID
	} else {
		// 默认查询作为所有者和好友的记录
		query.UserID = req.UserID
	}

	if req.VehicleID != "" {
		query.VehicleID = req.VehicleID
	}
	if req.Status != "" {
		query.Status = req.Status
	}

	shares, total, err := s.sharingRepo.List(ctx, query)
	if err != nil {
		return nil, 0, fmt.Errorf("查询分享记录失败: %w", err)
	}

	// 转换为响应格式
	result := make([]*SharedKeyInfo, 0, len(shares))
	for _, share := range shares {
		info := &SharedKeyInfo{
			ID:           share.ID,
			VehicleID:    share.VehicleID,
			Permissions:  s.deserializePermissions(share.Permissions),
			Status:       string(share.Status),
			CreatedAt:    share.CreatedAt,
			ExpiresAt:    share.ExpiresAt,
			RevokeReason: share.RevokeReason,
		}

		// 获取车辆信息
		if vehicle, err := s.vehicleRepo.GetByID(ctx, share.VehicleID); err == nil {
			info.VehicleName = vehicle.Name
		}

		// 获取所有者信息
		if owner, err := s.userRepo.GetByID(ctx, share.OwnerID); err == nil {
			info.OwnerID = owner.ID
			info.OwnerName = owner.Nickname
		}

		// 获取好友信息
		if friend, err := s.userRepo.GetByID(ctx, share.FriendID); err == nil {
			info.FriendID = friend.ID
			info.FriendName = friend.Nickname
		}

		if share.ActivatedAt != nil {
			info.ActivatedAt = share.ActivatedAt
		}
		if share.RevokedAt != nil {
			info.RevokedAt = share.RevokedAt
		}

		result = append(result, info)
	}

	return result, total, nil
}

// GetSharingDetail 获取分享详情
func (s *FriendSharingService) GetSharingDetail(ctx context.Context, userID, shareID string) (*SharedKeyInfo, error) {
	share, err := s.sharingRepo.GetByID(ctx, shareID)
	if err != nil {
		return nil, err
	}

	// 验证访问权限
	if share.OwnerID != userID && share.FriendID != userID {
		return nil, errors.New("无权查看此分享")
	}

	info := &SharedKeyInfo{
		ID:          share.ID,
		VehicleID:   share.VehicleID,
		OwnerID:     share.OwnerID,
		FriendID:    share.FriendID,
		Permissions: s.deserializePermissions(share.Permissions),
		Status:      string(share.Status),
		CreatedAt:   share.CreatedAt,
		ExpiresAt:   share.ExpiresAt,
	}

	// 填充其他信息...

	return info, nil
}

// ExtendExpiryRequest 延长有效期请求
type ExtendExpiryRequest struct {
	OwnerID   string `json:"owner_id" binding:"required"`
	ShareID   string `json:"share_id" binding:"required"`
	ExtraDays int    `json:"extra_days" binding:"min=1,max=365"`
}

// ExtendExpiry 延长钥匙有效期
func (s *FriendSharingService) ExtendExpiry(ctx context.Context, req *ExtendExpiryRequest) error {
	share, err := s.sharingRepo.GetByID(ctx, req.ShareID)
	if err != nil {
		return err
	}

	if share.OwnerID != req.OwnerID {
		return errors.New("无权操作")
	}

	if share.Status == model.SharingStatusRevoked {
		return errors.New("已撤回的钥匙不能延期")
	}

	// 延长过期时间
	share.ExpiresAt = share.ExpiresAt.AddDate(0, 0, req.ExtraDays)

	return s.sharingRepo.Update(ctx, share)
}

// 辅助函数

func (s *FriendSharingService) validatePermissions(perms []string) error {
	validPerms := map[string]bool{
		"unlock":   true,
		"lock":     true,
		"trunk":    true,
		"engine":   true,
		"location": true,
		"windows":  true,
		"climate":  true,
	}

	for _, p := range perms {
		if !validPerms[p] {
			return fmt.Errorf("无效的权限: %s", p)
		}
	}

	if len(perms) == 0 {
		return errors.New("至少需要一个权限")
	}

	return nil
}

func (s *FriendSharingService) serializePermissions(perms []string) string {
	data, _ := json.Marshal(perms)
	return string(data)
}

func (s *FriendSharingService) deserializePermissions(data string) []string {
	var perms []string
	json.Unmarshal([]byte(data), &perms)
	return perms
}

func (s *FriendSharingService) generateInvitationCode() (string, error) {
	bytes := make([]byte, 6)
	if _, err := rand.Read(bytes); err != nil {
		return "", err
	}
	return base64.URLEncoding.EncodeToString(bytes)[:8], nil
}

func (s *FriendSharingService) generateTemporaryKey() (string, error) {
	bytes := make([]byte, 32)
	if _, err := rand.Read(bytes); err != nil {
		return "", err
	}
	return base64.URLEncoding.EncodeToString(bytes), nil
}

func (s *FriendSharingService) sendInvitationNotification(friend *model.User, vehicle *model.Vehicle, share *model.KeySharing) error {
	message := fmt.Sprintf(
		"您收到了 %s 的数字钥匙分享邀请，邀请码: %s",
		vehicle.Name,
		share.InvitationCode,
	)

	if share.Message != "" {
		message += fmt.Sprintf("\n附言: %s", share.Message)
	}

	return s.notifier.Send(context.Background(), friend.Phone, message)
}

// CheckExpiredShares 检查并处理过期的分享 (定时任务调用)
func (s *FriendSharingService) CheckExpiredShares(ctx context.Context) error {
	shares, err := s.sharingRepo.GetExpired(ctx)
	if err != nil {
		return err
	}

	for _, share := range shares {
		share.Status = model.SharingStatusExpired
		if err := s.sharingRepo.Update(ctx, share); err != nil {
			logger.Error("标记过期分享失败", logger.String("share", share.ID), logger.Error(err))
			continue
		}

		// 禁用车辆端钥匙
		if err := s.vehicleService.RevokeTemporaryKey(ctx, share.VehicleID, share.FriendID); err != nil {
			logger.Error("禁用过期钥匙失败", logger.String("share", share.ID), logger.Error(err))
		}

		// 通知好友
		friend, _ := s.userRepo.GetByID(ctx, share.FriendID)
		if friend != nil {
			s.notifier.Send(ctx, friend.Phone,
				fmt.Sprintf("您对车辆 %s 的访问权限已过期", share.VehicleID))
		}
	}

	return nil
}
