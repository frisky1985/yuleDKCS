package models

import (
	"time"
	"gorm.io/gorm"
)

// KeyType 钥匙类型
type KeyType string

const (
	KeyTypeCCC   KeyType = "CCC"
	KeyTypeICCE  KeyType = "ICCE"
	KeyTypeICCOA KeyType = "ICCOA"
)

// KeyStatus 钥匙状态
type KeyStatus string

const (
	KeyStatusActive    KeyStatus = "active"
	KeyStatusInactive  KeyStatus = "inactive"
	KeyStatusRevoked   KeyStatus = "revoked"
	KeyStatusExpired   KeyStatus = "expired"
	KeyStatusPending   KeyStatus = "pending"
)

// KeyPermissions 钥匙权限
type KeyPermissions struct {
	Unlock      bool `json:"unlock"`
	Lock        bool `json:"lock"`
	StartEngine bool `json:"start_engine"`
	Trunk       bool `json:"trunk"`
	Windows     bool `json:"windows"`
}

// Key 钥匙模型
type Key struct {
	ID             uint           `json:"id" gorm:"primaryKey"`
	UserID         uint           `json:"user_id" gorm:"index;not null"`
	VehicleID      uint           `json:"vehicle_id" gorm:"index;not null"`
	Type           KeyType        `json:"type" gorm:"type:varchar(20);not null"`
	Status         KeyStatus      `json:"status" gorm:"type:varchar(20);default:'active'"`
	Permissions    KeyPermissions `json:"permissions" gorm:"embedded"`
	EncryptedData  string         `json:"-" gorm:"type:text;not null"`
	KeyIdentifier  string         `json:"key_identifier" gorm:"type:varchar(255);uniqueIndex;not null"`
	Name           string         `json:"name" gorm:"type:varchar(100)"`
	Description    string         `json:"description" gorm:"type:varchar(255)"`
	ExpiresAt      *time.Time     `json:"expires_at"`
	LastUsedAt     *time.Time     `json:"last_used_at"`
	UsageCount     int            `json:"usage_count" gorm:"default:0"`
	CreatedAt      time.Time      `json:"created_at"`
	UpdatedAt      time.Time      `json:"updated_at"`
	DeletedAt      gorm.DeletedAt `json:"-" gorm:"index"`
	
	// 关联
	User    User    `json:"user,omitempty" gorm:"foreignKey:UserID"`
	Vehicle Vehicle `json:"vehicle,omitempty" gorm:"foreignKey:VehicleID"`
	Shares  []KeyShare `json:"shares,omitempty" gorm:"foreignKey:KeyID"`
}

// KeyShare 钥匙分享模型
type KeyShare struct {
	ID          uint           `json:"id" gorm:"primaryKey"`
	KeyID       uint           `json:"key_id" gorm:"index;not null"`
	SharedTo    uint           `json:"shared_to" gorm:"index;not null"`
	SharedBy    uint           `json:"shared_by" gorm:"not null"`
	Permissions KeyPermissions `json:"permissions" gorm:"embedded"`
	Status      string         `json:"status" gorm:"type:varchar(20);default:'active'"`
	SharedAt    time.Time      `json:"shared_at"`
	ExpiresAt   *time.Time     `json:"expires_at"`
	RevokedAt   *time.Time     `json:"revoked_at,omitempty"`
	CreatedAt   time.Time      `json:"created_at"`
	UpdatedAt   time.Time      `json:"updated_at"`
	DeletedAt   gorm.DeletedAt `json:"-" gorm:"index"`
	
	// 关联
	Key      Key  `json:"key,omitempty" gorm:"foreignKey:KeyID"`
	User     User `json:"user,omitempty" gorm:"foreignKey:SharedTo"`
	SharedByUser User `json:"shared_by_user,omitempty" gorm:"foreignKey:SharedBy"`
}

// IssueKeyRequest 发行钥匙请求
type IssueKeyRequest struct {
	VehicleID     uint           `json:"vehicle_id" binding:"required"`
	Type          KeyType        `json:"type" binding:"required,oneof=CCC ICCE ICCOA"`
	Permissions   KeyPermissions `json:"permissions"`
	Name          string         `json:"name"`
	Description   string         `json:"description"`
	ExpiresAt     *time.Time     `json:"expires_at"`
}

// ShareKeyRequest 分享钥匙请求
type ShareKeyRequest struct {
	UserID      uint           `json:"user_id" binding:"required"`
	Permissions KeyPermissions `json:"permissions"`
	ExpiresAt   *time.Time     `json:"expires_at"`
}

// UpdatePermissionsRequest 更新权限请求
type UpdatePermissionsRequest struct {
	Permissions KeyPermissions `json:"permissions" binding:"required"`
}

// KeyResponse 钥匙响应
type KeyResponse struct {
	ID            uint           `json:"id"`
	UserID        uint           `json:"user_id"`
	VehicleID     uint           `json:"vehicle_id"`
	VehicleName   string         `json:"vehicle_name,omitempty"`
	Type          KeyType        `json:"type"`
	Status        KeyStatus      `json:"status"`
	Permissions   KeyPermissions `json:"permissions"`
	KeyIdentifier string         `json:"key_identifier"`
	Name          string         `json:"name"`
	Description   string         `json:"description"`
	ExpiresAt     *time.Time     `json:"expires_at,omitempty"`
	LastUsedAt    *time.Time     `json:"last_used_at,omitempty"`
	UsageCount    int            `json:"usage_count"`
	CreatedAt     time.Time      `json:"created_at"`
	UpdatedAt     time.Time      `json:"updated_at"`
	IsShared      bool           `json:"is_shared"`
	SharedBy      uint           `json:"shared_by,omitempty"`
}

// TableName 指定表名
func (Key) TableName() string {
	return "keys"
}

// TableName 指定表名
func (KeyShare) TableName() string {
	return "key_shares"
}

// IsActive 检查钥匙是否有效
func (k *Key) IsActive() bool {
	if k.Status != KeyStatusActive {
		return false
	}
	if k.ExpiresAt != nil && time.Now().After(*k.ExpiresAt) {
		return false
	}
	return true
}

// CanPerformAction 检查钥匙是否可执行特定操作
func (k *Key) CanPerformAction(action string) bool {
	if !k.IsActive() {
		return false
	}
	switch action {
	case "unlock":
		return k.Permissions.Unlock
	case "lock":
		return k.Permissions.Lock
	case "start_engine":
		return k.Permissions.StartEngine
	case "trunk":
		return k.Permissions.Trunk
	case "windows":
		return k.Permissions.Windows
	default:
		return false
	}
}

// RecordUsage 记录钥匙使用
func (k *Key) RecordUsage() {
	k.UsageCount++
	now := time.Now()
	k.LastUsedAt = &now
}
