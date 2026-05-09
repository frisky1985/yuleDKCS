package services

import (
	"context"
	"crypto/rand"
	"encoding/base64"
	"errors"
	"fmt"
	"time"

	"yuleDKCS/backend/internal/models"
	"yuleDKCS/backend/internal/repository"
)

var (
	ErrKeyNotFound       = errors.New("钥匙不存在")
	ErrUnauthorized      = errors.New("未授权的操作")
	ErrInvalidKeyType    = errors.New("无效的钥匙类型")
	ErrKeyRevoked        = errors.New("钥匙已被撤销")
	ErrKeyExpired        = errors.New("钥匙已过期")
	ErrInvalidPermissions = errors.New("无效的权限设置")
	ErrShareNotFound     = errors.New("分享记录不存在")
	ErrCannotShareToSelf = errors.New("不能分享给自己")
)

// KeyService 钥匙服务接口
type KeyService interface {
	// 钥匙管理
	IssueKey(ctx context.Context, userID uint, req *models.IssueKeyRequest) (*models.Key, error)
	GetKey(ctx context.Context, keyID uint, userID uint) (*models.Key, error)
	GetUserKeys(ctx context.Context, userID uint, page, pageSize int) ([]models.Key, int64, error)
	GetVehicleKeys(ctx context.Context, vehicleID uint, userID uint, page, pageSize int) ([]models.Key, int64, error)
	RevokeKey(ctx context.Context, keyID uint, userID uint) error
	UpdateKeyPermissions(ctx context.Context, keyID uint, userID uint, permissions models.KeyPermissions) error
	
	// 钥匙分享
	ShareKey(ctx context.Context, keyID uint, ownerID uint, req *models.ShareKeyRequest) (*models.KeyShare, error)
	GetKeyShares(ctx context.Context, keyID uint, userID uint) ([]models.KeyShare, error)
	GetSharedKeys(ctx context.Context, userID uint) ([]models.KeyShare, error)
	RevokeShare(ctx context.Context, shareID uint, userID uint) error
	
	// 钥匙使用
	ValidateKey(ctx context.Context, keyID uint, action string) error
	UseKey(ctx context.Context, keyID uint, action string) error
	
	// 协议特定操作
	GenerateCCCKey(ctx context.Context, vehicleID uint) (string, error)
	GenerateICCEKey(ctx context.Context, vehicleID uint) (string, error)
	GenerateICCOAKey(ctx context.Context, vehicleID uint) (string, error)
	
	// 过期检查
	CheckAndUpdateExpiredKeys(ctx context.Context) error
}

// keyService 钥匙服务实现
type keyService struct {
	keyRepo    repository.KeyRepository
	vehicleRepo repository.VehicleRepository
	userRepo   repository.UserRepository
}

// NewKeyService 创建钥匙服务实例
func NewKeyService(
	keyRepo repository.KeyRepository,
	vehicleRepo repository.VehicleRepository,
	userRepo repository.UserRepository,
) KeyService {
	return &keyService{
		keyRepo:     keyRepo,
		vehicleRepo: vehicleRepo,
		userRepo:    userRepo,
	}
}

// generateKeyIdentifier 生成唯一的钥匙标识符
func generateKeyIdentifier() (string, error) {
	bytes := make([]byte, 32)
	if _, err := rand.Read(bytes); err != nil {
		return "", err
	}
	return base64.URLEncoding.EncodeToString(bytes), nil
}

// IssueKey 发行新钥匙
func (s *keyService) IssueKey(ctx context.Context, userID uint, req *models.IssueKeyRequest) (*models.Key, error) {
	// 验证车辆存在
	vehicle, err := s.vehicleRepo.GetByID(ctx, req.VehicleID)
	if err != nil {
		return nil, err
	}

	// 验证用户有权为这辆车发行钥匙
	if vehicle.OwnerID != userID {
		return nil, ErrUnauthorized
	}

	// 验证钥匙类型
	if !isValidKeyType(req.Type) {
		return nil, ErrInvalidKeyType
	}

	// 生成钥匙标识符
	identifier, err := generateKeyIdentifier()
	if err != nil {
		return nil, err
	}

	// 生成加密数据（协议特定）
	var encryptedData string
	switch req.Type {
	case models.KeyTypeCCC:
		encryptedData, err = s.GenerateCCCKey(ctx, req.VehicleID)
	case models.KeyTypeICCE:
		encryptedData, err = s.GenerateICCEKey(ctx, req.VehicleID)
	case models.KeyTypeICCOA:
		encryptedData, err = s.GenerateICCOAKey(ctx, req.VehicleID)
	}
	if err != nil {
		return nil, err
	}

	// 设置默认权限
	permissions := req.Permissions
	if !permissions.Unlock && !permissions.Lock && !permissions.StartEngine {
		// 如果没有指定权限，设置默认权限
		permissions = models.KeyPermissions{
			Unlock:      true,
			Lock:        true,
			StartEngine: true,
			Trunk:       true,
			Windows:     false,
		}
	}

	key := &models.Key{
		UserID:        userID,
		VehicleID:     req.VehicleID,
		Type:          req.Type,
		Status:        models.KeyStatusActive,
		Permissions:   permissions,
		EncryptedData: encryptedData,
		KeyIdentifier: identifier,
		Name:          req.Name,
		Description:   req.Description,
		ExpiresAt:     req.ExpiresAt,
	}

	if err := s.keyRepo.Create(ctx, key); err != nil {
		return nil, err
	}

	return key, nil
}

// GetKey 获取钥匙详情
func (s *keyService) GetKey(ctx context.Context, keyID uint, userID uint) (*models.Key, error) {
	key, err := s.keyRepo.GetByID(ctx, keyID)
	if err != nil {
		return nil, err
	}

	// 检查用户是否有权访问这把钥匙
	if key.UserID != userID {
		// 检查是否是分享
		shares, err := s.keyRepo.GetSharesByUserID(ctx, userID)
		if err != nil {
			return nil, err
		}
		
		hasAccess := false
		for _, share := range shares {
			if share.KeyID == keyID {
				hasAccess = true
				break
			}
		}
		
		if !hasAccess {
			return nil, ErrUnauthorized
		}
	}

	return key, nil
}

// GetUserKeys 获取用户的所有钥匙
func (s *keyService) GetUserKeys(ctx context.Context, userID uint, page, pageSize int) ([]models.Key, int64, error) {
	offset := (page - 1) * pageSize
	if pageSize <= 0 {
		pageSize = 10
	}
	return s.keyRepo.GetByUserID(ctx, userID, offset, pageSize)
}

// GetVehicleKeys 获取车辆的所有钥匙
func (s *keyService) GetVehicleKeys(ctx context.Context, vehicleID uint, userID uint, page, pageSize int) ([]models.Key, int64, error) {
	// 验证用户有权查看车辆钥匙
	vehicle, err := s.vehicleRepo.GetByID(ctx, vehicleID)
	if err != nil {
		return nil, 0, err
	}

	if vehicle.OwnerID != userID {
		return nil, 0, ErrUnauthorized
	}

	offset := (page - 1) * pageSize
	if pageSize <= 0 {
		pageSize = 10
	}
	return s.keyRepo.GetByVehicleID(ctx, vehicleID, offset, pageSize)
}

// RevokeKey 撤销钥匙
func (s *keyService) RevokeKey(ctx context.Context, keyID uint, userID uint) error {
	key, err := s.keyRepo.GetByID(ctx, keyID)
	if err != nil {
		return err
	}

	// 只有钥匙所有者可以撤销
	if key.UserID != userID {
		return ErrUnauthorized
	}

	// 撤销所有分享
	shares, err := s.keyRepo.GetSharesByKeyID(ctx, keyID)
	if err != nil {
		return err
	}

	for _, share := range shares {
		if err := s.keyRepo.RevokeShare(ctx, share.ID); err != nil {
			return err
		}
	}

	return s.keyRepo.UpdateStatus(ctx, keyID, models.KeyStatusRevoked)
}

// UpdateKeyPermissions 更新钥匙权限
func (s *keyService) UpdateKeyPermissions(ctx context.Context, keyID uint, userID uint, permissions models.KeyPermissions) error {
	key, err := s.keyRepo.GetByID(ctx, keyID)
	if err != nil {
		return err
	}

	// 只有钥匙所有者可以修改权限
	if key.UserID != userID {
		return ErrUnauthorized
	}

	return s.keyRepo.UpdatePermissions(ctx, keyID, permissions)
}

// ShareKey 分享钥匙
func (s *keyService) ShareKey(ctx context.Context, keyID uint, ownerID uint, req *models.ShareKeyRequest) (*models.KeyShare, error) {
	// 验证钥匙存在且属于所有者
	key, err := s.keyRepo.GetByID(ctx, keyID)
	if err != nil {
		return nil, err
	}

	if key.UserID != ownerID {
		return nil, ErrUnauthorized
	}

	// 不能分享给自己
	if req.UserID == ownerID {
		return nil, ErrCannotShareToSelf
	}

	// 验证目标用户存在
	_, err = s.userRepo.GetByID(ctx, req.UserID)
	if err != nil {
		return nil, errors.New("目标用户不存在")
	}

	// 验证分享权限不超过原钥匙权限
	if !isValidSharePermissions(key.Permissions, req.Permissions) {
		return nil, ErrInvalidPermissions
	}

	share := &models.KeyShare{
		KeyID:       keyID,
		SharedTo:    req.UserID,
		SharedBy:    ownerID,
		Permissions: req.Permissions,
		Status:      "active",
		SharedAt:    time.Now(),
		ExpiresAt:   req.ExpiresAt,
	}

	if err := s.keyRepo.CreateShare(ctx, share); err != nil {
		return nil, err
	}

	return share, nil
}

// GetKeyShares 获取钥匙的分享列表
func (s *keyService) GetKeyShares(ctx context.Context, keyID uint, userID uint) ([]models.KeyShare, error) {
	key, err := s.keyRepo.GetByID(ctx, keyID)
	if err != nil {
		return nil, err
	}

	// 只有所有者可以查看分享列表
	if key.UserID != userID {
		return nil, ErrUnauthorized
	}

	return s.keyRepo.GetSharesByKeyID(ctx, keyID)
}

// GetSharedKeys 获取用户收到的分享钥匙
func (s *keyService) GetSharedKeys(ctx context.Context, userID uint) ([]models.KeyShare, error) {
	return s.keyRepo.GetSharesByUserID(ctx, userID)
}

// RevokeShare 撤销分享
func (s *keyService) RevokeShare(ctx context.Context, shareID uint, userID uint) error {
	share, err := s.keyRepo.GetShareByID(ctx, shareID)
	if err != nil {
		return err
	}

	// 只有分享者可以撤销
	if share.SharedBy != userID {
		return ErrUnauthorized
	}

	return s.keyRepo.RevokeShare(ctx, shareID)
}

// ValidateKey 验证钥匙有效性
func (s *keyService) ValidateKey(ctx context.Context, keyID uint, action string) error {
	key, err := s.keyRepo.GetByID(ctx, keyID)
	if err != nil {
		return err
	}

	if !key.IsActive() {
		if key.Status == models.KeyStatusRevoked {
			return ErrKeyRevoked
		}
		if key.Status == models.KeyStatusExpired {
			return ErrKeyExpired
		}
		return errors.New("钥匙无效")
	}

	if !key.CanPerformAction(action) {
		return ErrInvalidPermissions
	}

	return nil
}

// UseKey 使用钥匙
func (s *keyService) UseKey(ctx context.Context, keyID uint, action string) error {
	// 验证钥匙
	if err := s.ValidateKey(ctx, keyID, action); err != nil {
		return err
	}

	// 记录使用
	return s.keyRepo.RecordUsage(ctx, keyID)
}

// GenerateCCCKey 生成CCC协议钥匙
func (s *keyService) GenerateCCCKey(ctx context.Context, vehicleID uint) (string, error) {
	// CCC (Car Connectivity Consortium) 协议钥匙生成
	// 这里实现CCC特定的密钥生成逻辑
	bytes := make([]byte, 64)
	if _, err := rand.Read(bytes); err != nil {
		return "", err
	}
	
	// CCC协议特定的数据结构
	cccData := map[string]interface{}{
		"protocol":    "CCC",
		"version":     "2.0",
		"vehicle_id":  vehicleID,
		"private_key": base64.URLEncoding.EncodeToString(bytes[:32]),
		"public_key":  base64.URLEncoding.EncodeToString(bytes[32:]),
		"created_at":  time.Now().Unix(),
	}
	
	// 实际应用中这里应该使用CCC SDK生成标准格式的钥匙
	return fmt.Sprintf("ccc:%s", base64.URLEncoding.EncodeToString(bytes)), nil
}

// GenerateICCEKey 生成ICCE协议钥匙
func (s *keyService) GenerateICCEKey(ctx context.Context, vehicleID uint) (string, error) {
	// ICCE (Intelligent Car Connectivity Ecosystem) 协议钥匙生成
	bytes := make([]byte, 48)
	if _, err := rand.Read(bytes); err != nil {
		return "", err
	}
	
	// ICCE协议特定的数据结构
	return fmt.Sprintf("icce:%s", base64.URLEncoding.EncodeToString(bytes)), nil
}

// GenerateICCOAKey 生成ICCOA协议钥匙
func (s *keyService) GenerateICCOAKey(ctx context.Context, vehicleID uint) (string, error) {
	// ICCOA (Intelligent Car Connectivity Open Alliance) 协议钥匙生成
	bytes := make([]byte, 48)
	if _, err := rand.Read(bytes); err != nil {
		return "", err
	}
	
	// ICCOA协议特定的数据结构
	return fmt.Sprintf("iccoa:%s", base64.URLEncoding.EncodeToString(bytes)), nil
}

// CheckAndUpdateExpiredKeys 检查并更新过期钥匙
func (s *keyService) CheckAndUpdateExpiredKeys(ctx context.Context) error {
	// 更新过期的钥匙
	expiredKeys, err := s.keyRepo.GetExpiredKeys(ctx)
	if err != nil {
		return err
	}

	for _, key := range expiredKeys {
		if err := s.keyRepo.UpdateStatus(ctx, key.ID, models.KeyStatusExpired); err != nil {
			return err
		}
	}

	// 更新过期的分享
	expiredShares, err := s.keyRepo.GetExpiredShares(ctx)
	if err != nil {
		return err
	}

	for _, share := range expiredShares {
		if err := s.keyRepo.RevokeShare(ctx, share.ID); err != nil {
			return err
		}
	}

	return nil
}

// isValidKeyType 验证钥匙类型
func isValidKeyType(keyType models.KeyType) bool {
	switch keyType {
	case models.KeyTypeCCC, models.KeyTypeICCE, models.KeyTypeICCOA:
		return true
	}
	return false
}

// isValidSharePermissions 验证分享权限是否有效
func isValidSharePermissions(original, share models.KeyPermissions) bool {
	// 分享权限不能超过原钥匙权限
	if share.Unlock && !original.Unlock {
		return false
	}
	if share.Lock && !original.Lock {
		return false
	}
	if share.StartEngine && !original.StartEngine {
		return false
	}
	if share.Trunk && !original.Trunk {
		return false
	}
	if share.Windows && !original.Windows {
		return false
	}
	return true
}
