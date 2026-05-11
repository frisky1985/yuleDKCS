package model

import (
	"time"

	"gorm.io/gorm"
)

// SharingStatus 分享状态
type SharingStatus string

const (
	SharingStatusPending  SharingStatus = "pending"   // 待接受
	SharingStatusActive   SharingStatus = "active"    // 已激活
	SharingStatusExpired  SharingStatus = "expired"   // 已过期
	SharingStatusRevoked  SharingStatus = "revoked"   // 已撤回
	SharingStatusRejected SharingStatus = "rejected"  // 已拒绝
)

// KeySharing 钥匙分享记录
type KeySharing struct {
	ID             string        `gorm:"primaryKey;type:varchar(36)" json:"id"`
	OwnerID        string        `gorm:"index;not null;type:varchar(36)" json:"owner_id"`
	FriendID       string        `gorm:"index;not null;type:varchar(36)" json:"friend_id"`
	VehicleID      string        `gorm:"index;not null;type:varchar(36)" json:"vehicle_id"`
	Permissions    string        `gorm:"type:text" json:"permissions"` // JSON 格式
	Status         SharingStatus `gorm:"index;type:varchar(20);default:'pending'" json:"status"`
	InvitationCode string        `gorm:"uniqueIndex;type:varchar(16)" json:"invitation_code"`
	TempKey        string        `gorm:"type:varchar(256)" json:"-"` // 临时钥匙，不序列化
	Message        string        `gorm:"type:varchar(500)" json:"message"`
	CreatedAt      time.Time     `json:"created_at"`
	UpdatedAt      time.Time     `json:"updated_at"`
	DeletedAt      gorm.DeletedAt `gorm:"index" json:"-"`
	ExpiresAt      time.Time     `json:"expires_at"`
	ActivatedAt    *time.Time    `json:"activated_at,omitempty"`
	RevokedAt      *time.Time    `json:"revoked_at,omitempty"`
	RevokeReason   string        `gorm:"type:varchar(200)" json:"revoke_reason,omitempty"`

	// 关联
	Owner   User    `gorm:"foreignKey:OwnerID" json:"owner,omitempty"`
	Friend  User    `gorm:"foreignKey:FriendID" json:"friend,omitempty"`
	Vehicle Vehicle `gorm:"foreignKey:VehicleID" json:"vehicle,omitempty"`
}

// TableName 指定表名
func (KeySharing) TableName() string {
	return "key_sharings"
}

// IsActive 检查分享是否有效
func (ks *KeySharing) IsActive() bool {
	if ks.Status != SharingStatusActive {
		return false
	}
	if time.Now().After(ks.ExpiresAt) {
		return false
	}
	return true
}

// CanRevoke 检查是否可以撤回
func (ks *KeySharing) CanRevoke() bool {
	return ks.Status == SharingStatusPending || ks.Status == SharingStatusActive
}

// CanAccept 检查是否可以接受
func (ks *KeySharing) CanAccept() bool {
	if ks.Status != SharingStatusPending {
		return false
	}
	return time.Now().Before(ks.ExpiresAt)
}
