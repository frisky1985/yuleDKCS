package services

import (
	"context"
	"errors"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/mock"
	"yuleDKCS/backend/internal/models"
	"yuleDKCS/backend/internal/repository"
	"yuleDKCS/backend/tests/fixtures"
)

// MockKeyRepository 钥匙仓库 mock
type MockKeyRepository struct {
	mock.Mock
}

func (m *MockKeyRepository) Create(ctx context.Context, key *models.Key) error {
	args := m.Called(ctx, key)
	return args.Error(0)
}

func (m *MockKeyRepository) GetByID(ctx context.Context, id uint) (*models.Key, error) {
	args := m.Called(ctx, id)
	if args.Get(0) == nil {
		return nil, args.Error(1)
	}
	return args.Get(0).(*models.Key), args.Error(1)
}

func (m *MockKeyRepository) GetByIdentifier(ctx context.Context, identifier string) (*models.Key, error) {
	args := m.Called(ctx, identifier)
	if args.Get(0) == nil {
		return nil, args.Error(1)
	}
	return args.Get(0).(*models.Key), args.Error(1)
}

func (m *MockKeyRepository) GetByUserID(ctx context.Context, userID uint, offset, limit int) ([]models.Key, int64, error) {
	args := m.Called(ctx, userID, offset, limit)
	return args.Get(0).([]models.Key), args.Get(1).(int64), args.Error(2)
}

func (m *MockKeyRepository) GetByVehicleID(ctx context.Context, vehicleID uint, offset, limit int) ([]models.Key, int64, error) {
	args := m.Called(ctx, vehicleID, offset, limit)
	return args.Get(0).([]models.Key), args.Get(1).(int64), args.Error(2)
}

func (m *MockKeyRepository) Update(ctx context.Context, key *models.Key) error {
	args := m.Called(ctx, key)
	return args.Error(0)
}

func (m *MockKeyRepository) Delete(ctx context.Context, id uint) error {
	args := m.Called(ctx, id)
	return args.Error(0)
}

func (m *MockKeyRepository) UpdateStatus(ctx context.Context, id uint, status models.KeyStatus) error {
	args := m.Called(ctx, id, status)
	return args.Error(0)
}

func (m *MockKeyRepository) UpdatePermissions(ctx context.Context, id uint, permissions models.KeyPermissions) error {
	args := m.Called(ctx, id, permissions)
	return args.Error(0)
}

func (m *MockKeyRepository) RecordUsage(ctx context.Context, id uint) error {
	args := m.Called(ctx, id)
	return args.Error(0)
}

func (m *MockKeyRepository) CreateShare(ctx context.Context, share *models.KeyShare) error {
	args := m.Called(ctx, share)
	return args.Error(0)
}

func (m *MockKeyRepository) GetShareByID(ctx context.Context, id uint) (*models.KeyShare, error) {
	args := m.Called(ctx, id)
	if args.Get(0) == nil {
		return nil, args.Error(1)
	}
	return args.Get(0).(*models.KeyShare), args.Error(1)
}

func (m *MockKeyRepository) GetSharesByKeyID(ctx context.Context, keyID uint) ([]models.KeyShare, error) {
	args := m.Called(ctx, keyID)
	return args.Get(0).([]models.KeyShare), args.Error(1)
}

func (m *MockKeyRepository) GetSharesByUserID(ctx context.Context, userID uint) ([]models.KeyShare, error) {
	args := m.Called(ctx, userID)
	return args.Get(0).([]models.KeyShare), args.Error(1)
}

func (m *MockKeyRepository) UpdateShareStatus(ctx context.Context, id uint, status string) error {
	args := m.Called(ctx, id, status)
	return args.Error(0)
}

func (m *MockKeyRepository) RevokeShare(ctx context.Context, id uint) error {
	args := m.Called(ctx, id)
	return args.Error(0)
}

func (m *MockKeyRepository) DeleteShare(ctx context.Context, id uint) error {
	args := m.Called(ctx, id)
	return args.Error(0)
}

func (m *MockKeyRepository) GetExpiredKeys(ctx context.Context) ([]models.Key, error) {
	args := m.Called(ctx)
	return args.Get(0).([]models.Key), args.Error(1)
}

func (m *MockKeyRepository) GetExpiredShares(ctx context.Context) ([]models.KeyShare, error) {
	args := m.Called(ctx)
	return args.Get(0).([]models.KeyShare), args.Error(1)
}

// MockVehicleRepository 车辆仓库 mock
type MockVehicleRepository struct {
	mock.Mock
}

func (m *MockVehicleRepository) Create(ctx context.Context, vehicle *models.Vehicle) error {
	args := m.Called(ctx, vehicle)
	return args.Error(0)
}

func (m *MockVehicleRepository) GetByID(ctx context.Context, id uint) (*models.Vehicle, error) {
	args := m.Called(ctx, id)
	if args.Get(0) == nil {
		return nil, args.Error(1)
	}
	return args.Get(0).(*models.Vehicle), args.Error(1)
}

func (m *MockVehicleRepository) GetByVIN(ctx context.Context, vin string) (*models.Vehicle, error) {
	args := m.Called(ctx, vin)
	if args.Get(0) == nil {
		return nil, args.Error(1)
	}
	return args.Get(0).(*models.Vehicle), args.Error(1)
}

func (m *MockVehicleRepository) GetByOwnerID(ctx context.Context, ownerID uint) ([]models.Vehicle, error) {
	args := m.Called(ctx, ownerID)
	return args.Get(0).([]models.Vehicle), args.Error(1)
}

func (m *MockVehicleRepository) Update(ctx context.Context, vehicle *models.Vehicle) error {
	args := m.Called(ctx, vehicle)
	return args.Error(0)
}

func (m *MockVehicleRepository) Delete(ctx context.Context, id uint) error {
	args := m.Called(ctx, id)
	return args.Error(0)
}

func (m *MockVehicleRepository) List(ctx context.Context, ownerID uint, page, pageSize int) ([]models.Vehicle, int64, error) {
	args := m.Called(ctx, ownerID, page, pageSize)
	return args.Get(0).([]models.Vehicle), args.Get(1).(int64), args.Error(2)
}

func (m *MockVehicleRepository) Search(ctx context.Context, ownerID uint, keyword string) ([]models.Vehicle, error) {
	args := m.Called(ctx, ownerID, keyword)
	return args.Get(0).([]models.Vehicle), args.Error(1)
}

func (m *MockVehicleRepository) UpdateStatus(ctx context.Context, id uint, status models.VehicleStatus) error {
	args := m.Called(ctx, id, status)
	return args.Error(0)
}

func (m *MockVehicleRepository) UpdateLockStatus(ctx context.Context, id uint, status string) error {
	args := m.Called(ctx, id, status)
	return args.Error(0)
}

func (m *MockVehicleRepository) UpdateEngineStatus(ctx context.Context, id uint, status string) error {
	args := m.Called(ctx, id, status)
	return args.Error(0)
}

func (m *MockVehicleRepository) UpdateOnlineStatus(ctx context.Context, id uint, status string) error {
	args := m.Called(ctx, id, status)
	return args.Error(0)
}

func (m *MockVehicleRepository) UpdateLastSeen(ctx context.Context, id uint) error {
	args := m.Called(ctx, id)
	return args.Error(0)
}

func (m *MockVehicleRepository) UpdateLocation(ctx context.Context, id uint, location string) error {
	args := m.Called(ctx, id, location)
	return args.Error(0)
}

func (m *MockVehicleRepository) GetByBleMAC(ctx context.Context, mac string) (*models.Vehicle, error) {
	args := m.Called(ctx, mac)
	if args.Get(0) == nil {
		return nil, args.Error(1)
	}
	return args.Get(0).(*models.Vehicle), args.Error(1)
}

func (m *MockVehicleRepository) UpdateUWBEnabled(ctx context.Context, id uint, enabled bool) error {
	args := m.Called(ctx, id, enabled)
	return args.Error(0)
}

func (m *MockVehicleRepository) UpdateSoftwareVersion(ctx context.Context, id uint, version string) error {
	args := m.Called(ctx, id, version)
	return args.Error(0)
}

func (m *MockVehicleRepository) GetOTAInfo(ctx context.Context, id uint) (*models.VehicleOTAInfo, error) {
	args := m.Called(ctx, id)
	if args.Get(0) == nil {
		return nil, args.Error(1)
	}
	return args.Get(0).(*models.VehicleOTAInfo), args.Error(1)
}

func (m *MockVehicleRepository) IsOwner(ctx context.Context, vehicleID, userID uint) (bool, error) {
	args := m.Called(ctx, vehicleID, userID)
	return args.Bool(0), args.Error(1)
}

// MockUserRepository 用户仓库 mock
type MockUserRepository struct {
	mock.Mock
}

func (m *MockUserRepository) GetByID(ctx context.Context, id uint) (*models.User, error) {
	args := m.Called(ctx, id)
	if args.Get(0) == nil {
		return nil, args.Error(1)
	}
	return args.Get(0).(*models.User), args.Error(1)
}

func (m *MockUserRepository) GetByUsername(ctx context.Context, username string) (*models.User, error) {
	args := m.Called(ctx, username)
	if args.Get(0) == nil {
		return nil, args.Error(1)
	}
	return args.Get(0).(*models.User), args.Error(1)
}

// 测试辅助函数
func setupKeyServiceTest() (*keyService, *MockKeyRepository, *MockVehicleRepository, *MockUserRepository) {
	keyRepo := new(MockKeyRepository)
	vehicleRepo := new(MockVehicleRepository)
	userRepo := new(MockUserRepository)
	
	service := NewKeyService(keyRepo, vehicleRepo, userRepo).(*keyService)
	return service, keyRepo, vehicleRepo, userRepo
}

// IssueKey 测试
func TestKeyService_IssueKey_Success(t *testing.T) {
	service, keyRepo, vehicleRepo, _ := setupKeyServiceTest()
	ctx := context.Background()
	
	vehicle := fixtures.TestVehicle
	req := fixtures.IssueKeyRequest()
	
	vehicleRepo.On("GetByID", ctx, req.VehicleID).Return(&vehicle, nil)
	keyRepo.On("Create", ctx, mock.AnythingOfType("*models.Key")).Return(nil)
	
	key, err := service.IssueKey(ctx, 1, &req)
	
	assert.NoError(t, err)
	assert.NotNil(t, key)
	assert.Equal(t, req.VehicleID, key.VehicleID)
	assert.Equal(t, req.Type, key.Type)
	assert.Equal(t, models.KeyStatusActive, key.Status)
	assert.NotEmpty(t, key.KeyIdentifier)
	assert.NotEmpty(t, key.EncryptedData)
	
	vehicleRepo.AssertExpectations(t)
	keyRepo.AssertExpectations(t)
}

func TestKeyService_IssueKey_VehicleNotFound(t *testing.T) {
	service, _, vehicleRepo, _ := setupKeyServiceTest()
	ctx := context.Background()
	
	req := fixtures.IssueKeyRequest()
	
	vehicleRepo.On("GetByID", ctx, req.VehicleID).Return(nil, errors.New("车辆不存在"))
	
	key, err := service.IssueKey(ctx, 1, &req)
	
	assert.Error(t, err)
	assert.Nil(t, key)
	assert.Contains(t, err.Error(), "车辆不存在")
}

func TestKeyService_IssueKey_Unauthorized(t *testing.T) {
	service, _, vehicleRepo, _ := setupKeyServiceTest()
	ctx := context.Background()
	
	vehicle := fixtures.TestVehicle
	vehicle.OwnerID = 999 // 不同的用户
	req := fixtures.IssueKeyRequest()
	
	vehicleRepo.On("GetByID", ctx, req.VehicleID).Return(&vehicle, nil)
	
	key, err := service.IssueKey(ctx, 1, &req)
	
	assert.Error(t, err)
	assert.Equal(t, ErrUnauthorized, err)
	assert.Nil(t, key)
}

func TestKeyService_IssueKey_InvalidKeyType(t *testing.T) {
	service, _, vehicleRepo, _ := setupKeyServiceTest()
	ctx := context.Background()
	
	vehicle := fixtures.TestVehicle
	req := fixtures.IssueKeyRequest()
	req.Type = "INVALID"
	
	vehicleRepo.On("GetByID", ctx, req.VehicleID).Return(&vehicle, nil)
	
	key, err := service.IssueKey(ctx, 1, &req)
	
	assert.Error(t, err)
	assert.Equal(t, ErrInvalidKeyType, err)
	assert.Nil(t, key)
}

// GetKey 测试
func TestKeyService_GetKey_Success_Owner(t *testing.T) {
	service, keyRepo, _, _ := setupKeyServiceTest()
	ctx := context.Background()
	
	key := fixtures.TestKey
	
	keyRepo.On("GetByID", ctx, uint(1)).Return(&key, nil)
	
	result, err := service.GetKey(ctx, 1, 1)
	
	assert.NoError(t, err)
	assert.NotNil(t, result)
	assert.Equal(t, key.ID, result.ID)
}

func TestKeyService_GetKey_Success_ViaShare(t *testing.T) {
	service, keyRepo, _, _ := setupKeyServiceTest()
	ctx := context.Background()
	
	key := fixtures.TestKey
	key.UserID = 999 // 不同的所有者
	shares := []models.KeyShare{fixtures.TestKeyShare}
	
	keyRepo.On("GetByID", ctx, uint(1)).Return(&key, nil)
	keyRepo.On("GetSharesByUserID", ctx, uint(1)).Return(shares, nil)
	
	result, err := service.GetKey(ctx, 1, 1)
	
	assert.NoError(t, err)
	assert.NotNil(t, result)
	assert.Equal(t, key.ID, result.ID)
}

func TestKeyService_GetKey_Unauthorized(t *testing.T) {
	service, keyRepo, _, _ := setupKeyServiceTest()
	ctx := context.Background()
	
	key := fixtures.TestKey
	key.UserID = 999 // 不同的所有者
	shares := []models.KeyShare{} // 没有分享给测试用户
	
	keyRepo.On("GetByID", ctx, uint(1)).Return(&key, nil)
	keyRepo.On("GetSharesByUserID", ctx, uint(1)).Return(shares, nil)
	
	result, err := service.GetKey(ctx, 1, 1)
	
	assert.Error(t, err)
	assert.Equal(t, ErrUnauthorized, err)
	assert.Nil(t, result)
}

func TestKeyService_GetKey_NotFound(t *testing.T) {
	service, keyRepo, _, _ := setupKeyServiceTest()
	ctx := context.Background()
	
	keyRepo.On("GetByID", ctx, uint(999)).Return(nil, ErrKeyNotFound)
	
	result, err := service.GetKey(ctx, 999, 1)
	
	assert.Error(t, err)
	assert.Nil(t, result)
}

// GetUserKeys 测试
func TestKeyService_GetUserKeys_Success(t *testing.T) {
	service, keyRepo, _, _ := setupKeyServiceTest()
	ctx := context.Background()
	
	keys := []models.Key{fixtures.TestKey, fixtures.TestKey2}
	var total int64 = 2
	
	keyRepo.On("GetByUserID", ctx, uint(1), 0, 10).Return(keys, total, nil)
	
	result, count, err := service.GetUserKeys(ctx, 1, 1, 10)
	
	assert.NoError(t, err)
	assert.Equal(t, int64(2), count)
	assert.Len(t, result, 2)
}

// RevokeKey 测试
func TestKeyService_RevokeKey_Success(t *testing.T) {
	service, keyRepo, _, _ := setupKeyServiceTest()
	ctx := context.Background()
	
	key := fixtures.TestKey
	shares := []models.KeyShare{}
	
	keyRepo.On("GetByID", ctx, uint(1)).Return(&key, nil)
	keyRepo.On("GetSharesByKeyID", ctx, uint(1)).Return(shares, nil)
	keyRepo.On("UpdateStatus", ctx, uint(1), models.KeyStatusRevoked).Return(nil)
	
	err := service.RevokeKey(ctx, 1, 1)
	
	assert.NoError(t, err)
	keyRepo.AssertExpectations(t)
}

func TestKeyService_RevokeKey_WithShares(t *testing.T) {
	service, keyRepo, _, _ := setupKeyServiceTest()
	ctx := context.Background()
	
	key := fixtures.TestKey
	shares := []models.KeyShare{fixtures.TestKeyShare}
	
	keyRepo.On("GetByID", ctx, uint(1)).Return(&key, nil)
	keyRepo.On("GetSharesByKeyID", ctx, uint(1)).Return(shares, nil)
	keyRepo.On("RevokeShare", ctx, uint(1)).Return(nil)
	keyRepo.On("UpdateStatus", ctx, uint(1), models.KeyStatusRevoked).Return(nil)
	
	err := service.RevokeKey(ctx, 1, 1)
	
	assert.NoError(t, err)
	keyRepo.AssertExpectations(t)
}

func TestKeyService_RevokeKey_Unauthorized(t *testing.T) {
	service, keyRepo, _, _ := setupKeyServiceTest()
	ctx := context.Background()
	
	key := fixtures.TestKey
	key.UserID = 999
	
	keyRepo.On("GetByID", ctx, uint(1)).Return(&key, nil)
	
	err := service.RevokeKey(ctx, 1, 1)
	
	assert.Error(t, err)
	assert.Equal(t, ErrUnauthorized, err)
}

// ShareKey 测试
func TestKeyService_ShareKey_Success(t *testing.T) {
	service, keyRepo, _, userRepo := setupKeyServiceTest()
	ctx := context.Background()
	
	key := fixtures.TestKey
	req := fixtures.ShareKeyRequest()
	user := fixtures.TestUser2
	
	keyRepo.On("GetByID", ctx, uint(1)).Return(&key, nil)
	userRepo.On("GetByID", ctx, req.UserID).Return(&user, nil)
	keyRepo.On("CreateShare", ctx, mock.AnythingOfType("*models.KeyShare")).Return(nil)
	
	share, err := service.ShareKey(ctx, 1, 1, &req)
	
	assert.NoError(t, err)
	assert.NotNil(t, share)
	assert.Equal(t, req.UserID, share.SharedTo)
	keyRepo.AssertExpectations(t)
}

func TestKeyService_ShareKey_CannotShareToSelf(t *testing.T) {
	service, keyRepo, _, _ := setupKeyServiceTest()
	ctx := context.Background()
	
	key := fixtures.TestKey
	req := fixtures.ShareKeyRequest()
	req.UserID = 1 // 分享给自己
	
	keyRepo.On("GetByID", ctx, uint(1)).Return(&key, nil)
	
	share, err := service.ShareKey(ctx, 1, 1, &req)
	
	assert.Error(t, err)
	assert.Equal(t, ErrCannotShareToSelf, err)
	assert.Nil(t, share)
}

func TestKeyService_ShareKey_InvalidPermissions(t *testing.T) {
	service, keyRepo, _, userRepo := setupKeyServiceTest()
	ctx := context.Background()
	
	key := fixtures.TestKey
	key.Permissions = fixtures.LimitedPermissions() // 原钥匙没有 start_engine 权限
	req := fixtures.ShareKeyRequest()
	req.Permissions = fixtures.FullPermissions() // 分享请求包含 start_engine
	user := fixtures.TestUser2
	
	keyRepo.On("GetByID", ctx, uint(1)).Return(&key, nil)
	userRepo.On("GetByID", ctx, req.UserID).Return(&user, nil)
	
	share, err := service.ShareKey(ctx, 1, 1, &req)
	
	assert.Error(t, err)
	assert.Equal(t, ErrInvalidPermissions, err)
	assert.Nil(t, share)
}

// ValidateKey 测试
func TestKeyService_ValidateKey_Success(t *testing.T) {
	service, keyRepo, _, _ := setupKeyServiceTest()
	ctx := context.Background()
	
	key := fixtures.TestKey
	
	keyRepo.On("GetByID", ctx, uint(1)).Return(&key, nil)
	
	err := service.ValidateKey(ctx, 1, "unlock")
	
	assert.NoError(t, err)
}

func TestKeyService_ValidateKey_Revoked(t *testing.T) {
	service, keyRepo, _, _ := setupKeyServiceTest()
	ctx := context.Background()
	
	key := fixtures.TestRevokedKey
	
	keyRepo.On("GetByID", ctx, uint(4)).Return(&key, nil)
	
	err := service.ValidateKey(ctx, 4, "unlock")
	
	assert.Error(t, err)
	assert.Equal(t, ErrKeyRevoked, err)
}

func TestKeyService_ValidateKey_Expired(t *testing.T) {
	service, keyRepo, _, _ := setupKeyServiceTest()
	ctx := context.Background()
	
	key := fixtures.TestExpiredKey
	key.Status = models.KeyStatusExpired // 明确设置为过期状态
	
	keyRepo.On("GetByID", ctx, uint(3)).Return(&key, nil)
	
	err := service.ValidateKey(ctx, 3, "unlock")
	
	assert.Error(t, err)
	assert.Equal(t, ErrKeyExpired, err)
}

func TestKeyService_ValidateKey_InvalidPermissions(t *testing.T) {
	service, keyRepo, _, _ := setupKeyServiceTest()
	ctx := context.Background()
	
	key := fixtures.TestKey
	key.Permissions = fixtures.UnlockOnlyPermissions() // 只有解锁权限
	
	keyRepo.On("GetByID", ctx, uint(1)).Return(&key, nil)
	
	err := service.ValidateKey(ctx, 1, "start_engine")
	
	assert.Error(t, err)
	assert.Equal(t, ErrInvalidPermissions, err)
}

// UseKey 测试
func TestKeyService_UseKey_Success(t *testing.T) {
	service, keyRepo, _, _ := setupKeyServiceTest()
	ctx := context.Background()
	
	key := fixtures.TestKey
	
	keyRepo.On("GetByID", ctx, uint(1)).Return(&key, nil)
	keyRepo.On("RecordUsage", ctx, uint(1)).Return(nil)
	
	err := service.UseKey(ctx, 1, "unlock")
	
	assert.NoError(t, err)
	keyRepo.AssertExpectations(t)
}

// GenerateCCCKey 测试
func TestKeyService_GenerateCCCKey(t *testing.T) {
	service, _, _, _ := setupKeyServiceTest()
	ctx := context.Background()
	
	key, err := service.GenerateCCCKey(ctx, 1)
	
	assert.NoError(t, err)
	assert.NotEmpty(t, key)
	assert.Contains(t, key, "ccc:")
}

func TestKeyService_GenerateICCEKey(t *testing.T) {
	service, _, _, _ := setupKeyServiceTest()
	ctx := context.Background()
	
	key, err := service.GenerateICCEKey(ctx, 1)
	
	assert.NoError(t, err)
	assert.NotEmpty(t, key)
	assert.Contains(t, key, "icce:")
}

func TestKeyService_GenerateICCOAKey(t *testing.T) {
	service, _, _, _ := setupKeyServiceTest()
	ctx := context.Background()
	
	key, err := service.GenerateICCOAKey(ctx, 1)
	
	assert.NoError(t, err)
	assert.NotEmpty(t, key)
	assert.Contains(t, key, "iccoa:")
}

// CheckAndUpdateExpiredKeys 测试
func TestKeyService_CheckAndUpdateExpiredKeys_Success(t *testing.T) {
	service, keyRepo, _, _ := setupKeyServiceTest()
	ctx := context.Background()
	
	expiredKeys := []models.Key{fixtures.TestExpiredKey}
	expiredShares := []models.KeyShare{fixtures.TestExpiredShare}
	
	keyRepo.On("GetExpiredKeys", ctx).Return(expiredKeys, nil)
	keyRepo.On("UpdateStatus", ctx, uint(3), models.KeyStatusExpired).Return(nil)
	keyRepo.On("GetExpiredShares", ctx).Return(expiredShares, nil)
	keyRepo.On("RevokeShare", ctx, uint(2)).Return(nil)
	
	err := service.CheckAndUpdateExpiredKeys(ctx)
	
	assert.NoError(t, err)
	keyRepo.AssertExpectations(t)
}

// RevokeShare 测试
func TestKeyService_RevokeShare_Success(t *testing.T) {
	service, keyRepo, _, _ := setupKeyServiceTest()
	ctx := context.Background()
	
	share := fixtures.TestKeyShare
	
	keyRepo.On("GetShareByID", ctx, uint(1)).Return(&share, nil)
	keyRepo.On("RevokeShare", ctx, uint(1)).Return(nil)
	
	err := service.RevokeShare(ctx, 1, 1)
	
	assert.NoError(t, err)
	keyRepo.AssertExpectations(t)
}

func TestKeyService_RevokeShare_Unauthorized(t *testing.T) {
	service, keyRepo, _, _ := setupKeyServiceTest()
	ctx := context.Background()
	
	share := fixtures.TestKeyShare
	share.SharedBy = 999 // 不同的用户
	
	keyRepo.On("GetShareByID", ctx, uint(1)).Return(&share, nil)
	
	err := service.RevokeShare(ctx, 1, 1)
	
	assert.Error(t, err)
	assert.Equal(t, ErrUnauthorized, err)
}

// 辅助函数测试
func TestIsValidKeyType(t *testing.T) {
	tests := []struct {
		keyType models.KeyType
		valid   bool
	}{
		{models.KeyTypeCCC, true},
		{models.KeyTypeICCE, true},
		{models.KeyTypeICCOA, true},
		{"INVALID", false},
		{"", false},
	}
	
	for _, tt := range tests {
		t.Run(string(tt.keyType), func(t *testing.T) {
			result := isValidKeyType(tt.keyType)
			assert.Equal(t, tt.valid, result)
		})
	}
}

func TestIsValidSharePermissions(t *testing.T) {
	full := fixtures.FullPermissions()
	limited := fixtures.LimitedPermissions()
	unlockOnly := fixtures.UnlockOnlyPermissions()
	
	tests := []struct {
		name      string
		original  models.KeyPermissions
		share     models.KeyPermissions
		valid     bool
	}{
		{"full to limited", full, limited, true},
		{"limited to full", limited, full, false},
		{"limited to unlock only", limited, unlockOnly, true},
		{"unlock only to full", unlockOnly, full, false},
	}
	
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result := isValidSharePermissions(tt.original, tt.share)
			assert.Equal(t, tt.valid, result)
		})
	}
}

// GetVehicleKeys 测试
func TestKeyService_GetVehicleKeys_Success(t *testing.T) {
	service, keyRepo, vehicleRepo, _ := setupKeyServiceTest()
	ctx := context.Background()
	
	vehicle := fixtures.TestVehicle
	keys := []models.Key{fixtures.TestKey}
	var total int64 = 1
	
	vehicleRepo.On("GetByID", ctx, uint(1)).Return(&vehicle, nil)
	keyRepo.On("GetByVehicleID", ctx, uint(1), 0, 10).Return(keys, total, nil)
	
	result, count, err := service.GetVehicleKeys(ctx, 1, 1, 1, 10)
	
	assert.NoError(t, err)
	assert.Equal(t, int64(1), count)
	assert.Len(t, result, 1)
}

func TestKeyService_GetVehicleKeys_Unauthorized(t *testing.T) {
	service, _, vehicleRepo, _ := setupKeyServiceTest()
	ctx := context.Background()
	
	vehicle := fixtures.TestVehicle
	vehicle.OwnerID = 999
	
	vehicleRepo.On("GetByID", ctx, uint(1)).Return(&vehicle, nil)
	
	result, count, err := service.GetVehicleKeys(ctx, 1, 1, 1, 10)
	
	assert.Error(t, err)
	assert.Equal(t, ErrUnauthorized, err)
	assert.Nil(t, result)
	assert.Equal(t, int64(0), count)
}

// UpdateKeyPermissions 测试
func TestKeyService_UpdateKeyPermissions_Success(t *testing.T) {
	service, keyRepo, _, _ := setupKeyServiceTest()
	ctx := context.Background()
	
	key := fixtures.TestKey
	newPermissions := fixtures.LimitedPermissions()
	
	keyRepo.On("GetByID", ctx, uint(1)).Return(&key, nil)
	keyRepo.On("UpdatePermissions", ctx, uint(1), newPermissions).Return(nil)
	
	err := service.UpdateKeyPermissions(ctx, 1, 1, newPermissions)
	
	assert.NoError(t, err)
	keyRepo.AssertExpectations(t)
}

func TestKeyService_UpdateKeyPermissions_Unauthorized(t *testing.T) {
	service, keyRepo, _, _ := setupKeyServiceTest()
	ctx := context.Background()
	
	key := fixtures.TestKey
	key.UserID = 999
	newPermissions := fixtures.LimitedPermissions()
	
	keyRepo.On("GetByID", ctx, uint(1)).Return(&key, nil)
	
	err := service.UpdateKeyPermissions(ctx, 1, 1, newPermissions)
	
	assert.Error(t, err)
	assert.Equal(t, ErrUnauthorized, err)
}

// GetSharedKeys 测试
func TestKeyService_GetSharedKeys_Success(t *testing.T) {
	service, keyRepo, _, _ := setupKeyServiceTest()
	ctx := context.Background()
	
	shares := []models.KeyShare{fixtures.TestKeyShare}
	
	keyRepo.On("GetSharesByUserID", ctx, uint(2)).Return(shares, nil)
	
	result, err := service.GetSharedKeys(ctx, 2)
	
	assert.NoError(t, err)
	assert.Len(t, result, 1)
	assert.Equal(t, fixtures.TestKeyShare.ID, result[0].ID)
}

// GetKeyShares 测试
func TestKeyService_GetKeyShares_Success(t *testing.T) {
	service, keyRepo, _, _ := setupKeyServiceTest()
	ctx := context.Background()
	
	key := fixtures.TestKey
	shares := []models.KeyShare{fixtures.TestKeyShare}
	
	keyRepo.On("GetByID", ctx, uint(1)).Return(&key, nil)
	keyRepo.On("GetSharesByKeyID", ctx, uint(1)).Return(shares, nil)
	
	result, err := service.GetKeyShares(ctx, 1, 1)
	
	assert.NoError(t, err)
	assert.Len(t, result, 1)
}

func TestKeyService_GetKeyShares_Unauthorized(t *testing.T) {
	service, keyRepo, _, _ := setupKeyServiceTest()
	ctx := context.Background()
	
	key := fixtures.TestKey
	key.UserID = 999
	
	keyRepo.On("GetByID", ctx, uint(1)).Return(&key, nil)
	
	result, err := service.GetKeyShares(ctx, 1, 1)
	
	assert.Error(t, err)
	assert.Equal(t, ErrUnauthorized, err)
	assert.Nil(t, result)
}
