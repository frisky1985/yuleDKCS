package fixtures

import (
	"time"

	"github.com/frisky1985/yuleDKCS/backend/internal/models"
)

// TestUser 测试用户数据
var TestUser = models.User{
	ID:        1,
	Username:  "testuser",
	Email:     "test@example.com",
	Phone:     "13800138000",
	CreatedAt: time.Now(),
	UpdatedAt: time.Now(),
}

// TestUser2 第二个测试用户
var TestUser2 = models.User{
	ID:        2,
	Username:  "testuser2",
	Email:     "test2@example.com",
	Phone:     "13800138001",
	CreatedAt: time.Now(),
	UpdatedAt: time.Now(),
}

// TestVehicle 测试车辆数据
var TestVehicle = models.Vehicle{
	ID:              1,
	OwnerID:         1,
	Name:            "我的宝马",
	Vin:             "WBA12345678901234",
	Brand:           "BMW",
	Model:           "X5",
	Year:            2024,
	Color:           "黑色",
	Plate:           "京A12345",
	Status:          models.VehicleStatusActive,
	LockStatus:      "locked",
	EngineStatus:    "off",
	OnlineStatus:    "online",
	BleMAC:          "AA:BB:CC:DD:EE:FF",
	UWBCapable:      true,
	UWBEnabled:      false,
	SoftwareVersion: "1.0.0",
	OTAAvailable:    true,
	OTATargetVersion: "1.1.0",
	CreatedAt:       time.Now(),
	UpdatedAt:       time.Now(),
}

// TestVehicle2 第二个测试车辆
var TestVehicle2 = models.Vehicle{
	ID:           2,
	OwnerID:      2,
	Name:         "我的奥迪",
	Vin:          "WAU12345678901235",
	Brand:        "Audi",
	Model:        "A6L",
	Year:         2024,
	Color:        "白色",
	Plate:        "京B67890",
	Status:       models.VehicleStatusActive,
	LockStatus:   "unlocked",
	EngineStatus: "off",
	OnlineStatus: "offline",
	BleMAC:       "11:22:33:44:55:66",
	UWBCapable:   false,
	UWBEnabled:   false,
	CreatedAt:    time.Now(),
	UpdatedAt:    time.Now(),
}

// TestKey CCC 协议测试钥匙
var TestKey = models.Key{
	ID:            1,
	UserID:        1,
	VehicleID:     1,
	Type:          models.KeyTypeCCC,
	Status:        models.KeyStatusActive,
	Permissions:   FullPermissions(),
	EncryptedData: "ccc:test_encrypted_data",
	KeyIdentifier: "test_key_identifier_1",
	Name:          "我的主钥匙",
	Description:   "CCC 协议数字钥匙",
	UsageCount:    10,
	CreatedAt:     time.Now(),
	UpdatedAt:     time.Now(),
}

// TestKey2 ICCE 协议测试钥匙
var TestKey2 = models.Key{
	ID:            2,
	UserID:        1,
	VehicleID:     1,
	Type:          models.KeyTypeICCE,
	Status:        models.KeyStatusActive,
	Permissions:   FullPermissions(),
	EncryptedData: "icce:test_encrypted_data",
	KeyIdentifier: "test_key_identifier_2",
	Name:          "备用钥匙",
	Description:   "ICCE 协议数字钥匙",
	UsageCount:    5,
	CreatedAt:     time.Now(),
	UpdatedAt:     time.Now(),
}

// TestExpiredKey 已过期的测试钥匙
var TestExpiredKey = models.Key{
	ID:            3,
	UserID:        1,
	VehicleID:     1,
	Type:          models.KeyTypeCCC,
	Status:        models.KeyStatusActive,
	Permissions:   FullPermissions(),
	EncryptedData: "ccc:expired_data",
	KeyIdentifier: "test_key_expired",
	Name:          "过期钥匙",
	ExpiresAt:     TimePtr(time.Now().Add(-24 * time.Hour)),
	UsageCount:    0,
	CreatedAt:     time.Now().Add(-7 * 24 * time.Hour),
	UpdatedAt:     time.Now().Add(-7 * 24 * time.Hour),
}

// TestRevokedKey 已撤销的测试钥匙
var TestRevokedKey = models.Key{
	ID:            4,
	UserID:        1,
	VehicleID:     1,
	Type:          models.KeyTypeICCOA,
	Status:        models.KeyStatusRevoked,
	Permissions:   FullPermissions(),
	EncryptedData: "iccoa:revoked_data",
	KeyIdentifier: "test_key_revoked",
	Name:          "撤销钥匙",
	UsageCount:    3,
	CreatedAt:     time.Now().Add(-30 * 24 * time.Hour),
	UpdatedAt:     time.Now().Add(-7 * 24 * time.Hour),
}

// TestKeyShare 测试钥匙分享
var TestKeyShare = models.KeyShare{
	ID:          1,
	KeyID:       1,
	SharedTo:    2,
	SharedBy:    1,
	Permissions: LimitedPermissions(),
	Status:      "active",
	SharedAt:    time.Now(),
	CreatedAt:   time.Now(),
	UpdatedAt:   time.Now(),
}

// TestExpiredShare 已过期的钥匙分享
var TestExpiredShare = models.KeyShare{
	ID:          2,
	KeyID:       1,
	SharedTo:    2,
	SharedBy:    1,
	Permissions: LimitedPermissions(),
	Status:      "active",
	SharedAt:    time.Now().Add(-7 * 24 * time.Hour),
	ExpiresAt:   TimePtr(time.Now().Add(-24 * time.Hour)),
	CreatedAt:   time.Now().Add(-7 * 24 * time.Hour),
	UpdatedAt:   time.Now().Add(-7 * 24 * time.Hour),
}

// FullPermissions 返回完整权限
func FullPermissions() models.KeyPermissions {
	return models.KeyPermissions{
		Unlock:      true,
		Lock:        true,
		StartEngine: true,
		Trunk:       true,
		Windows:     true,
	}
}

// LimitedPermissions 返回有限权限
func LimitedPermissions() models.KeyPermissions {
	return models.KeyPermissions{
		Unlock:      true,
		Lock:        true,
		StartEngine: false,
		Trunk:       false,
		Windows:     false,
	}
}

// UnlockOnlyPermissions 返回仅解锁权限
func UnlockOnlyPermissions() models.KeyPermissions {
	return models.KeyPermissions{
		Unlock:      true,
		Lock:        false,
		StartEngine: false,
		Trunk:       false,
		Windows:     false,
	}
}

// IssueKeyRequest 测试发行钥匙请求
func IssueKeyRequest() models.IssueKeyRequest {
	return models.IssueKeyRequest{
		VehicleID:   1,
		Type:        models.KeyTypeCCC,
		Permissions: FullPermissions(),
		Name:        "测试钥匙",
		Description: "用于测试的数字钥匙",
	}
}

// ShareKeyRequest 测试分享钥匙请求
func ShareKeyRequest() models.ShareKeyRequest {
	return models.ShareKeyRequest{
		UserID:      2,
		Permissions: LimitedPermissions(),
	}
}

// VehicleRegisterRequest 测试车辆注册请求
func VehicleRegisterRequest() models.VehicleRegisterRequest {
	return models.VehicleRegisterRequest{
		Vin:        "WBA98765432109876",
		Brand:      "Mercedes-Benz",
		Model:      "E300L",
		Year:       2024,
		Color:      "银色",
		Plate:      "京C11111",
		Name:       "新车辆",
		BleMAC:     "FF:EE:DD:CC:BB:AA",
		UWBCapable: true,
	}
}

// VehicleUpdateRequest 测试车辆更新请求
func VehicleUpdateRequest() models.VehicleUpdateRequest {
	return models.VehicleUpdateRequest{
		Name:  "更新后的名称",
		Plate: "京D22222",
		Color: "红色",
	}
}

// VehicleCommandRequest 测试车辆命令请求
func VehicleCommandRequest() models.VehicleCommandRequest {
	return models.VehicleCommandRequest{
		Command: models.VehicleCommandUnlock,
		Params:  map[string]interface{}{},
	}
}

// VehicleLocationRequest 测试车辆位置请求
func VehicleLocationRequest() models.VehicleLocationRequest {
	return models.VehicleLocationRequest{
		Latitude:  39.9042,
		Longitude: 116.4074,
		Altitude:  50.0,
		Accuracy:  5.0,
	}
}

// TimePtr 返回 time.Time 指针
func TimePtr(t time.Time) *time.Time {
	return &t
}

// LoginRequest 测试登录请求
func LoginRequest() map[string]string {
	return map[string]string{
		"username": "testuser",
		"password": "password123",
	}
}

// RegisterRequest 测试注册请求
func RegisterRequest() map[string]string {
	return map[string]string{
		"username": "newuser",
		"email":    "newuser@example.com",
		"password": "password123",
	}
}

// Users 返回测试用户列表
func Users() []models.User {
	return []models.User{TestUser, TestUser2}
}

// Vehicles 返回测试车辆列表
func Vehicles() []models.Vehicle {
	return []models.Vehicle{TestVehicle, TestVehicle2}
}

// Keys 返回测试钥匙列表
func Keys() []models.Key {
	return []models.Key{TestKey, TestKey2, TestExpiredKey, TestRevokedKey}
}

// KeyShares 返回测试钥匙分享列表
func KeyShares() []models.KeyShare {
	return []models.KeyShare{TestKeyShare, TestExpiredShare}
}

// PaginationParams 测试分页参数
func PaginationParams() (page, pageSize int) {
	return 1, 10
}

// InvalidLoginRequest 无效登录请求
func InvalidLoginRequest() map[string]string {
	return map[string]string{
		"username": "",
		"password": "",
	}
}

// InvalidRegisterRequest 无效注册请求
func InvalidRegisterRequest() map[string]string {
	return map[string]string{
		"username": "ab",
		"email":    "invalid-email",
		"password": "123",
	}
}

// InvalidIssueKeyRequest 无效发行钥匙请求
func InvalidIssueKeyRequest() models.IssueKeyRequest {
	return models.IssueKeyRequest{
		VehicleID: 0,
		Type:      "INVALID",
	}
}

// InvalidVehicleRegisterRequest 无效车辆注册请求
func InvalidVehicleRegisterRequest() models.VehicleRegisterRequest {
	return models.VehicleRegisterRequest{
		Vin:   "INVALID",
		Brand: "",
		Model: "",
		Year:  1800,
	}
}
