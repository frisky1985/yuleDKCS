package repository

import (
	"context"
	"errors"

	"github.com/frisky1985/yuleDKCS/backend/internal/model"
	"gorm.io/gorm"
)

// SharingQuery 分享查询参数
type SharingQuery struct {
	Page      int
	PageSize  int
	OwnerID   string
	FriendID  string
	VehicleID string
	UserID    string
	Status    string
}

// KeySharingRepository 钥匙分享仓库接口
type KeySharingRepository interface {
	// 创建分享记录
	Create(ctx context.Context, share *model.KeySharing) error
	// 根据ID获取分享
	GetByID(ctx context.Context, id string) (*model.KeySharing, error)
	// 根据邀请码获取分享
	GetByInvitationCode(ctx context.Context, code string) (*model.KeySharing, error)
	// 更新分享
	Update(ctx context.Context, share *model.KeySharing) error
	// 查询分享列表
	List(ctx context.Context, query *SharingQuery) ([]*model.KeySharing, int64, error)
	// 获取已过期的分享
	GetExpired(ctx context.Context) ([]*model.KeySharing, error)
}

// keySharingRepository 钥匙分享仓库实现
type keySharingRepository struct {
	db *gorm.DB
}

// NewKeySharingRepository 创建钥匙分享仓库实例
func NewKeySharingRepository(db *gorm.DB) KeySharingRepository {
	return &keySharingRepository{db: db}
}

func (r *keySharingRepository) Create(ctx context.Context, share *model.KeySharing) error {
	return r.db.WithContext(ctx).Create(share).Error
}

func (r *keySharingRepository) GetByID(ctx context.Context, id string) (*model.KeySharing, error) {
	var share model.KeySharing
	result := r.db.WithContext(ctx).First(&share, "id = ?", id)
	if result.Error != nil {
		if errors.Is(result.Error, gorm.ErrRecordNotFound) {
			return nil, errors.New("分享记录不存在")
		}
		return nil, result.Error
	}
	return &share, nil
}

func (r *keySharingRepository) GetByInvitationCode(ctx context.Context, code string) (*model.KeySharing, error) {
	var share model.KeySharing
	result := r.db.WithContext(ctx).Where("invitation_code = ?", code).First(&share)
	if result.Error != nil {
		if errors.Is(result.Error, gorm.ErrRecordNotFound) {
			return nil, errors.New("无效的邀请码")
		}
		return nil, result.Error
	}
	return &share, nil
}

func (r *keySharingRepository) Update(ctx context.Context, share *model.KeySharing) error {
	return r.db.WithContext(ctx).Save(share).Error
}

func (r *keySharingRepository) List(ctx context.Context, query *SharingQuery) ([]*model.KeySharing, int64, error) {
	var shares []*model.KeySharing
	var total int64

	db := r.db.WithContext(ctx).Model(&model.KeySharing{})
	if query.OwnerID != "" {
		db = db.Where("owner_id = ?", query.OwnerID)
	}
	if query.FriendID != "" {
		db = db.Where("friend_id = ?", query.FriendID)
	}
	if query.UserID != "" {
		db = db.Where("(owner_id = ? OR friend_id = ?)", query.UserID, query.UserID)
	}
	if query.VehicleID != "" {
		db = db.Where("vehicle_id = ?", query.VehicleID)
	}
	if query.Status != "" {
		db = db.Where("status = ?", query.Status)
	}

	if err := db.Count(&total).Error; err != nil {
		return nil, 0, err
	}

	offset := (query.Page - 1) * query.PageSize
	result := db.Offset(offset).Limit(query.PageSize).Order("created_at DESC").Find(&shares)
	return shares, total, result.Error
}

func (r *keySharingRepository) GetExpired(ctx context.Context) ([]*model.KeySharing, error) {
	var shares []*model.KeySharing
	result := r.db.WithContext(ctx).
		Where("status = 'active' AND expires_at < NOW()").
		Find(&shares)
	return shares, result.Error
}
