package models

import (
	"time"

	"gorm.io/gorm"
)

// OTAStatus OTA更新状态
type OTAStatus string

const (
	OTAStatusPending      OTAStatus = "pending"       // 待更新
	OTAStatusDownloading  OTAStatus = "downloading"   // 下载中
	OTAStatusInstalling   OTAStatus = "installing"    // 安装中
	OTAStatusCompleted    OTAStatus = "completed"     // 已完成
	OTAStatusFailed       OTAStatus = "failed"        // 失败
	OTAStatusRolledBack   OTAStatus = "rolled_back"   // 已回滚
)

// FirmwareType 固件类型
type FirmwareType string

const (
	FirmwareTypeECU      FirmwareType = "ecu"       // ECU固件
	FirmwareTypeIVI      FirmwareType = "ivi"       // 车载信息娱乐系统
	FirmwareTypeTBox     FirmwareType = "tbox"      // T-Box固件
	FirmwareTypeADAS     FirmwareType = "adas"      // ADAS固件
	FirmwareTypeGateway  FirmwareType = "gateway"   // 网关固件
	FirmwareTypeBMS      FirmwareType = "bms"       // 电池管理系统
)

// Firmware 固件模型
type Firmware struct {
	ID                 uint           `json:"id" gorm:"primaryKey"`
	Version            string         `json:"version" gorm:"type:varchar(50);not null;index"`
	Type               FirmwareType   `json:"type" gorm:"type:varchar(20);not null;index"`
	URL                string         `json:"url" gorm:"type:varchar(500);not null"`
	Size               int64          `json:"size" gorm:"not null"`
	Checksum           string         `json:"checksum" gorm:"type:varchar(64);not null"`
	Signature          string         `json:"signature" gorm:"type:varchar(512)"`
	ReleaseNotes       string         `json:"release_notes" gorm:"type:text"`
	MinHardwareVersion string         `json:"min_hardware_version" gorm:"type:varchar(50)"`
	MaxHardwareVersion string         `json:"max_hardware_version" gorm:"type:varchar(50)"`
	IsActive           bool           `json:"is_active" gorm:"default:true"`
	ReleaseDate        time.Time      `json:"release_date"`
	CreatedAt          time.Time      `json:"created_at"`
	UpdatedAt          time.Time      `json:"updated_at"`
	DeletedAt          gorm.DeletedAt `json:"-" gorm:"index"`
}

// TableName 指定表名
func (Firmware) TableName() string {
	return "firmwares"
}

// VehicleOTAStatus 车辆OTA状态
type VehicleOTAStatus struct {
	ID              uint           `json:"id" gorm:"primaryKey"`
	VehicleID       uint           `json:"vehicle_id" gorm:"index;not null"`
	CurrentVersion  string         `json:"current_version" gorm:"type:varchar(50)"`
	TargetVersion   string         `json:"target_version" gorm:"type:varchar(50)"`
	Status          OTAStatus      `json:"status" gorm:"type:varchar(20);default:'pending'"`
	Progress        int            `json:"progress" gorm:"default:0"`
	ErrorMessage    string         `json:"error_message" gorm:"type:text"`
	StartedAt       *time.Time     `json:"started_at"`
	CompletedAt     *time.Time     `json:"completed_at"`
	CreatedAt       time.Time      `json:"created_at"`
	UpdatedAt       time.Time      `json:"updated_at"`
	DeletedAt       gorm.DeletedAt `json:"-" gorm:"index"`

	// 关联
	Vehicle Vehicle `json:"vehicle,omitempty" gorm:"foreignKey:VehicleID"`
}

// TableName 指定表名
func (VehicleOTAStatus) TableName() string {
	return "vehicle_ota_status"
}

// OTADownloadRecord OTA下载记录
type OTADownloadRecord struct {
	ID           uint      `json:"id" gorm:"primaryKey"`
	VehicleID    uint      `json:"vehicle_id" gorm:"index;not null"`
	FirmwareID   uint      `json:"firmware_id" gorm:"index;not null"`
	DownloadedAt time.Time `json:"downloaded_at"`
	IPAddress    string    `json:"ip_address" gorm:"type:varchar(45)"`
	Status       string    `json:"status" gorm:"type:varchar(20);default:'success'"`
	CreatedAt    time.Time `json:"created_at"`
}

// TableName 指定表名
func (OTADownloadRecord) TableName() string {
	return "ota_download_records"
}

// CheckUpdateRequest 检查更新请求
type CheckUpdateRequest struct {
	CurrentVersion     string `json:"current_version" binding:"required"`
	HardwareVersion    string `json:"hardware_version" binding:"required"`
	FirmwareType       string `json:"firmware_type" binding:"required"`
	VehicleID          uint   `json:"vehicle_id"`
}

// CheckUpdateResponse 检查更新响应
type CheckUpdateResponse struct {
	HasUpdate       bool       `json:"has_update"`
	Firmware        *Firmware  `json:"firmware,omitempty"`
	UpdateRequired  bool       `json:"update_required"`
	Message         string     `json:"message"`
}

// UpdateOTAStatusRequest 更新OTA状态请求
type UpdateOTAStatusRequest struct {
	Status       OTAStatus `json:"status" binding:"required,oneof=pending downloading installing completed failed rolled_back"`
	Progress     int       `json:"progress" binding:"min=0,max=100"`
	ErrorMessage string    `json:"error_message"`
}

// OTADownloadRequest OTA下载请求
type OTADownloadRequest struct {
	VehicleID  uint   `json:"vehicle_id"`
	IPAddress  string `json:"ip_address"`
}

// FirmwareResponse 固件响应
type FirmwareResponse struct {
	ID                 uint       `json:"id"`
	Version            string     `json:"version"`
	Type               string     `json:"type"`
	Size               int64      `json:"size"`
	Checksum           string     `json:"checksum"`
	ReleaseNotes       string     `json:"release_notes"`
	MinHardwareVersion string     `json:"min_hardware_version"`
	MaxHardwareVersion string     `json:"max_hardware_version"`
	ReleaseDate        time.Time  `json:"release_date"`
	CreatedAt          time.Time  `json:"created_at"`
}

// VehicleOTAStatusResponse 车辆OTA状态响应
type VehicleOTAStatusResponse struct {
	ID             string    `json:"id"`
	VehicleID      uint      `json:"vehicle_id"`
	CurrentVersion string    `json:"current_version"`
	TargetVersion  string    `json:"target_version"`
	Status         string    `json:"status"`
	Progress       int       `json:"progress"`
	ErrorMessage   string    `json:"error_message,omitempty"`
	StartedAt      *time.Time `json:"started_at,omitempty"`
	CompletedAt    *time.Time `json:"completed_at,omitempty"`
	UpdatedAt      time.Time `json:"updated_at"`
}
