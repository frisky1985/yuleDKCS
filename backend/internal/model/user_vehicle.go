package model

import (
	"time"
)

// User 用户模型
type User struct {
	ID        uint      `gorm:"primaryKey" json:"id"`
	Phone     string    `gorm:"type:varchar(20);uniqueIndex" json:"phone"`
	Email     string    `gorm:"type:varchar(100)" json:"email,omitempty"`
	Name      string    `gorm:"type:varchar(100)" json:"name"`
	CreatedAt time.Time `json:"created_at"`
	UpdatedAt time.Time `json:"updated_at"`
}

// Vehicle 车辆模型
type Vehicle struct {
	ID              uint   `gorm:"primaryKey" json:"id"`
	Vin             string `gorm:"type:varchar(17);uniqueIndex" json:"vin"`
	Brand           string `gorm:"type:varchar(50)" json:"brand"`
	Model           string `gorm:"type:varchar(100)" json:"model"`
	Year            int    `json:"year"`
	LicensePlate    string `gorm:"type:varchar(20)" json:"license_plate"`
	Color           string `gorm:"type:varchar(20)" json:"color,omitempty"`
	SoftwareVersion string `gorm:"type:varchar(50)" json:"software_version,omitempty"`
	HardwareVersion string `gorm:"type:varchar(50)" json:"hardware_version,omitempty"`
	OwnerID         uint   `gorm:"index" json:"owner_id"`
	CreatedAt       time.Time `json:"created_at"`
	UpdatedAt       time.Time `json:"updated_at"`
}
