package repository

import (
	"context"
	"errors"
	"time"

	"github.com/frisky1985/yuleDKCS/backend/internal/models"
	"gorm.io/gorm"
)

var (
	ErrKeyNotFound      = errors.New("钥匙不存在")
	ErrKeyAlreadyExists = errors.New("钥匙已存在")
	ErrInvalidKeyData   = errors.New("无效的钥匙数据")
)

// KeyRepository 钥匙仓库接口
type KeyRepository interface {
	// 钥匙 CRUD
	Create(ctx context.Context, key *models.Key) error
	GetByID(ctx context.Context, id uint) (*models.Key, error)
	GetByIdentifier(ctx context.Context, identifier string) (*models.Key, error)
	GetByUserID(ctx context.Context, userID uint, offset, limit int) ([]models.Key, int64, error)
	GetByVehicleID(ctx context.Context, vehicleID uint, offset, limit int) ([]models.Key, int64, error)
	Update(ctx context.Context, key *models.Key) error
	Delete(ctx context.Context, id uint) error
	UpdateStatus(ctx context.Context, id uint, status models.KeyStatus) error
	UpdatePermissions(ctx context.Context, id uint, permissions models.KeyPermissions) error
	RecordUsage(ctx context.Context, id uint) error
	
	// 分享管理
	CreateShare(ctx context.Context, share *models.KeyShare) error
	GetShareByID(ctx context.Context, id uint) (*models.KeyShare, error)
	GetSharesByKeyID(ctx context.Context, keyID uint) ([]models.KeyShare, error)
	GetSharesByUserID(ctx context.Context, userID uint) ([]models.KeyShare, error)
	UpdateShareStatus(ctx context.Context, id uint, status string) error
	RevokeShare(ctx context.Context, id uint) error
	DeleteShare(ctx context.Context, id uint) error
	
	// 过期检查
	GetExpiredKeys(ctx context.Context) ([]models.Key, error)
	GetExpiredShares(ctx context.Context) ([]models.KeyShare, error)
}

// keyRepository 钥匙仓库实现
type keyRepository struct {
	db *gorm.DB
}

// NewKeyRepository 创建钥匙仓库实例
func NewKeyRepository(db *gorm.DB) KeyRepository {
	return &keyRepository{db: db}
}

// Create 创建钥匙
func (r *keyRepository) Create(ctx context.Context, key *models.Key) error {
	return r.db.WithContext(ctx).Create(key).Error
}

// GetByID 根据ID获取钥匙
func (r *keyRepository) GetByID(ctx context.Context, id uint) (*models.Key, error) {
	var key models.Key
	result := r.db.WithContext(ctx).
		Preload("User").
		Preload("Vehicle").
		Preload("Shares").
		First(&key, id)
	if result.Error != nil {
		if errors.Is(result.Error, gorm.ErrRecordNotFound) {
			return nil, ErrKeyNotFound
		}
		return nil, result.Error
	}
	return &key, nil
}

// GetByIdentifier 根据标识符获取钥匙
func (r *keyRepository) GetByIdentifier(ctx context.Context, identifier string) (*models.Key, error) {
	var key models.Key
	result := r.db.WithContext(ctx).
		Where("key_identifier = ?", identifier).
		First(&key)
	if result.Error != nil {
		if errors.Is(result.Error, gorm.ErrRecordNotFound) {
			return nil, ErrKeyNotFound
		}
		return nil, result.Error
	}
	return &key, nil
}

// GetByUserID 获取用户的所有钥匙
func (r *keyRepository) GetByUserID(ctx context.Context, userID uint, offset, limit int) ([]models.Key, int64, error) {
	var keys []models.Key
	var total int64

	db := r.db.WithContext(ctx).Model(&models.Key{}).Where("user_id = ?", userID)
	
	if err := db.Count(&total).Error; err != nil {
		return nil, 0, err
	}

	result := db.Order("created_at DESC").
		Offset(offset).
		Limit(limit).
		Preload("Vehicle").
		Find(&keys)
	
	return keys, total, result.Error
}

// GetByVehicleID 获取车辆的所有钥匙
func (r *keyRepository) GetByVehicleID(ctx context.Context, vehicleID uint, offset, limit int) ([]models.Key, int64, error) {
	var keys []models.Key
	var total int64

	db := r.db.WithContext(ctx).Model(&models.Key{}).Where("vehicle_id = ?", vehicleID)
	
	if err := db.Count(&total).Error; err != nil {
		return nil, 0, err
	}

	result := db.Order("created_at DESC").
		Offset(offset).
		Limit(limit).
		Preload("User").
		Find(&keys)
	
	return keys, total, result.Error
}

// Update 更新钥匙
func (r *keyRepository) Update(ctx context.Context, key *models.Key) error {
	return r.db.WithContext(ctx).Save(key).Error
}

// Delete 删除钥匙
func (r *keyRepository) Delete(ctx context.Context, id uint) error {
	return r.db.WithContext(ctx).Delete(&models.Key{}, id).Error
}

// UpdateStatus 更新钥匙状态
func (r *keyRepository) UpdateStatus(ctx context.Context, id uint, status models.KeyStatus) error {
	return r.db.WithContext(ctx).
		Model(&models.Key{}).
		Where("id = ?", id).
		Update("status", status).Error
}

// UpdatePermissions 更新钥匙权限
func (r *keyRepository) UpdatePermissions(ctx context.Context, id uint, permissions models.KeyPermissions) error {
	return r.db.WithContext(ctx).
		Model(&models.Key{}).
		Where("id = ?", id).
		Updates(map[string]interface{}{
			"unlock":       permissions.Unlock,
			"lock":         permissions.Lock,
			"start_engine": permissions.StartEngine,
			"trunk":        permissions.Trunk,
			"windows":      permissions.Windows,
		}).Error
}

// RecordUsage 记录钥匙使用
func (r *keyRepository) RecordUsage(ctx context.Context, id uint) error {
	return r.db.WithContext(ctx).
		Model(&models.Key{}).
		Where("id = ?", id).
		Updates(map[string]interface{}{
			"last_used_at": time.Now(),
			"usage_count":  gorm.Expr("usage_count + 1"),
		}).Error
}

// CreateShare 创建钥匙分享
func (r *keyRepository) CreateShare(ctx context.Context, share *models.KeyShare) error {
	return r.db.WithContext(ctx).Create(share).Error
}

// GetShareByID 根据ID获取分享记录
func (r *keyRepository) GetShareByID(ctx context.Context, id uint) (*models.KeyShare, error) {
	var share models.KeyShare
	result := r.db.WithContext(ctx).
		Preload("Key").
		Preload("User").
		First(&share, id)
	if result.Error != nil {
		if errors.Is(result.Error, gorm.ErrRecordNotFound) {
			return nil, ErrKeyNotFound
		}
		return nil, result.Error
	}
	return &share, nil
}

// GetSharesByKeyID 获取钥匙的所有分享
func (r *keyRepository) GetSharesByKeyID(ctx context.Context, keyID uint) ([]models.KeyShare, error) {
	var shares []models.KeyShare
	result := r.db.WithContext(ctx).
		Where("key_id = ?", keyID).
		Preload("User").
		Find(&shares)
	return shares, result.Error
}

// GetSharesByUserID 获取用户收到的所有分享
func (r *keyRepository) GetSharesByUserID(ctx context.Context, userID uint) ([]models.KeyShare, error) {
	var shares []models.KeyShare
	result := r.db.WithContext(ctx).
		Where("shared_to = ? AND status = ?", userID, "active").
		Preload("Key").
		Preload("Key.Vehicle").
		Preload("SharedByUser").
		Find(&shares)
	return shares, result.Error
}

// UpdateShareStatus 更新分享状态
func (r *keyRepository) UpdateShareStatus(ctx context.Context, id uint, status string) error {
	return r.db.WithContext(ctx).
		Model(&models.KeyShare{}).
		Where("id = ?", id).
		Update("status", status).Error
}

// RevokeShare 撤销分享
func (r *keyRepository) RevokeShare(ctx context.Context, id uint) error {
	now := time.Now()
	return r.db.WithContext(ctx).
		Model(&models.KeyShare{}).
		Where("id = ?", id).
		Updates(map[string]interface{}{
			"status":     "revoked",
			"revoked_at": now,
		}).Error
}

// DeleteShare 删除分享
func (r *keyRepository) DeleteShare(ctx context.Context, id uint) error {
	return r.db.WithContext(ctx).Delete(&models.KeyShare{}, id).Error
}

// GetExpiredKeys 获取已过期的钥匙
func (r *keyRepository) GetExpiredKeys(ctx context.Context) ([]models.Key, error) {
	var keys []models.Key
	now := time.Now()
	result := r.db.WithContext(ctx).
		Where("expires_at IS NOT NULL AND expires_at < ? AND status = ?", now, models.KeyStatusActive).
		Find(&keys)
	return keys, result.Error
}

// GetExpiredShares 获取已过期的分享
func (r *keyRepository) GetExpiredShares(ctx context.Context) ([]models.KeyShare, error) {
	var shares []models.KeyShare
	now := time.Now()
	result := r.db.WithContext(ctx).
		Where("expires_at IS NOT NULL AND expires_at < ? AND status = ?", now, "active").
		Find(&shares)
	return shares, result.Error
}
