package models

import (
	"time"
	"gorm.io/gorm"
)

// VehicleStatus 车辆状态类型
type VehicleStatus string

const (
	VehicleStatusOnline    VehicleStatus = "online"
	VehicleStatusOffline   VehicleStatus = "offline"
	VehicleStatusLocked    VehicleStatus = "locked"
	VehicleStatusUnlocked  VehicleStatus = "unlocked"
	VehicleStatusActive    VehicleStatus = "active"
	VehicleStatusInactive  VehicleStatus = "inactive"
	VehicleStatusMaintenance VehicleStatus = "maintenance"
)

// VehicleCommand 车辆命令类型
type VehicleCommand string

const (
	VehicleCommandUnlock      VehicleCommand = "unlock"
	VehicleCommandLock        VehicleCommand = "lock"
	VehicleCommandEngineStart VehicleCommand = "engine_start"
	VehicleCommandEngineStop  VehicleCommand = "engine_stop"
	VehicleCommandTrunk       VehicleCommand = "trunk"
	VehicleCommandWindowsUp   VehicleCommand = "windows_up"
	VehicleCommandWindowsDown VehicleCommand = "windows_down"
	VehicleCommandFindMyCar   VehicleCommand = "find_my_car"
)

// VehicleLocation 车辆位置信息
type VehicleLocation struct {
	Latitude  float64 `json:"latitude"`
	Longitude float64 `json:"longitude"`
	Altitude  float64 `json:"altitude,omitempty"`
	Accuracy  float64 `json:"accuracy,omitempty"`
	Timestamp int64   `json:"timestamp"`
}

// Vehicle 车辆模型
type Vehicle struct {
	ID           uint           `json:"id" gorm:"primaryKey"`
	OwnerID      uint           `json:"owner_id" gorm:"index;not null"`
	Name         string         `json:"name" gorm:"type:varchar(100);not null"`
	Vin          string         `json:"vin" gorm:"type:varchar(17);uniqueIndex"`
	Plate        string         `json:"plate" gorm:"type:varchar(20)"`
	Brand        string         `json:"brand" gorm:"type:varchar(50)"`
	Model        string         `json:"model" gorm:"type:varchar(50)"`
	Year         int            `json:"year"`
	Color        string         `json:"color" gorm:"type:varchar(30)"`
	Status       VehicleStatus  `json:"status" gorm:"type:varchar(20);default:'active'"`
	LockStatus   string         `json:"lock_status" gorm:"type:varchar(20);default:'locked'"`
	EngineStatus string         `json:"engine_status" gorm:"type:varchar(20);default:'off'"`
	OnlineStatus string         `json:"online_status" gorm:"type:varchar(20);default:'offline'"`
	
	// BLE/UWB 相关
	BleMAC      string    `json:"ble_mac" gorm:"type:varchar(17);uniqueIndex"`
	UWBCapable  bool      `json:"uwb_capable" gorm:"default:false"`
	UWBEnabled  bool      `json:"uwb_enabled" gorm:"default:false"`
	
	// 位置信息 (存储为JSON)
	Location    string    `json:"location" gorm:"type:text"`
	LastSeenAt  *time.Time `json:"last_seen_at"`
	LastLocationAt *time.Time `json:"last_location_at"`
	
	// OTA 相关信息
	SoftwareVersion string `json:"software_version" gorm:"type:varchar(50)"`
	HardwareVersion string `json:"hardware_version" gorm:"type:varchar(50)"`
	OTAAvailable    bool   `json:"ota_available" gorm:"default:false"`
	OTATargetVersion string `json:"ota_target_version" gorm:"type:varchar(50)"`
	
	CreatedAt  time.Time      `json:"created_at"`
	UpdatedAt  time.Time      `json:"updated_at"`
	DeletedAt  gorm.DeletedAt `json:"-" gorm:"index"`
	
	// 关联
	Owner User   `json:"owner,omitempty" gorm:"foreignKey:OwnerID"`
	Keys  []Key  `json:"keys,omitempty" gorm:"foreignKey:VehicleID"`
}

// VehicleRegisterRequest 车辆注册请求
type VehicleRegisterRequest struct {
	Vin          string `json:"vin" binding:"required,len=17"`
	Brand        string `json:"brand" binding:"required"`
	Model        string `json:"model" binding:"required"`
	Year         int    `json:"year" binding:"required,min=1900,max=2100"`
	Color        string `json:"color"`
	Plate        string `json:"plate"`
	Name         string `json:"name" binding:"required"`
	BleMAC       string `json:"ble_mac"`
	UWBCapable   bool   `json:"uwb_capable"`
}

// VehicleUpdateRequest 车辆更新请求
type VehicleUpdateRequest struct {
	Name     string `json:"name"`
	Plate    string `json:"plate"`
	Color    string `json:"color"`
}

// VehicleCommandRequest 车辆命令请求
type VehicleCommandRequest struct {
	Command   VehicleCommand `json:"command" binding:"required"`
	Params    map[string]interface{} `json:"params,omitempty"`
}

// VehicleLocationRequest 车辆位置更新请求
type VehicleLocationRequest struct {
	Latitude  float64 `json:"latitude" binding:"required"`
	Longitude float64 `json:"longitude" binding:"required"`
	Altitude  float64 `json:"altitude"`
	Accuracy  float64 `json:"accuracy"`
}

// VehicleStatusResponse 车辆状态响应
type VehicleStatusResponse struct {
	ID           uint           `json:"id"`
	Status       VehicleStatus  `json:"status"`
	LockStatus   string         `json:"lock_status"`
	EngineStatus string         `json:"engine_status"`
	OnlineStatus string         `json:"online_status"`
	Location     *VehicleLocation `json:"location,omitempty"`
	LastSeenAt   *time.Time     `json:"last_seen_at,omitempty"`
	SoftwareVersion string      `json:"software_version,omitempty"`
}

// VehicleCommandResponse 车辆命令响应
type VehicleCommandResponse struct {
	CommandID   string    `json:"command_id"`
	Command     VehicleCommand `json:"command"`
	Status      string    `json:"status"`
	Message     string    `json:"message,omitempty"`
	CreatedAt   time.Time `json:"created_at"`
	ExecutedAt  *time.Time `json:"executed_at,omitempty"`
}

// VehicleResponse 车辆响应
type VehicleResponse struct {
	ID           uint      `json:"id"`
	OwnerID      uint      `json:"owner_id"`
	Name         string    `json:"name"`
	Vin          string    `json:"vin"`
	Plate        string    `json:"plate,omitempty"`
	Brand        string    `json:"brand"`
	Model        string    `json:"model"`
	Year         int       `json:"year"`
	Color        string    `json:"color,omitempty"`
	Status       VehicleStatus `json:"status"`
	LockStatus   string    `json:"lock_status"`
	EngineStatus string    `json:"engine_status"`
	OnlineStatus string    `json:"online_status"`
	BleMAC       string    `json:"ble_mac,omitempty"`
	UWBCapable   bool      `json:"uwb_capable"`
	UWBEnabled   bool      `json:"uwb_enabled"`
	LastSeenAt   *time.Time `json:"last_seen_at,omitempty"`
	SoftwareVersion string `json:"software_version,omitempty"`
	OTAAvailable bool      `json:"ota_available"`
	CreatedAt    time.Time `json:"created_at"`
	UpdatedAt    time.Time `json:"updated_at"`
}

// VehicleOTAInfo OTA信息响应
type VehicleOTAInfo struct {
	CurrentVersion   string    `json:"current_version"`
	TargetVersion    string    `json:"target_version,omitempty"`
	UpdateAvailable  bool      `json:"update_available"`
	UpdateSize       int64     `json:"update_size,omitempty"`
	ReleaseNotes     string    `json:"release_notes,omitempty"`
	EstimatedTime    int       `json:"estimated_time,omitempty"` // 分钟
}

// TableName 指定表名
func (Vehicle) TableName() string {
	return "vehicles"
}

// IsOnline 检查车辆是否在线
func (v *Vehicle) IsOnline() bool {
	if v.LastSeenAt == nil {
		return false
	}
	// 5分钟内活跃认为在线
	return time.Since(*v.LastSeenAt) < 5*time.Minute
}

// ToResponse 转换为响应格式
func (v *Vehicle) ToResponse() VehicleResponse {
	return VehicleResponse{
		ID:              v.ID,
		OwnerID:         v.OwnerID,
		Name:            v.Name,
		Vin:             v.Vin,
		Plate:           v.Plate,
		Brand:           v.Brand,
		Model:           v.Model,
		Year:            v.Year,
		Color:           v.Color,
		Status:          v.Status,
		LockStatus:      v.LockStatus,
		EngineStatus:    v.EngineStatus,
		OnlineStatus:    v.OnlineStatus,
		BleMAC:          v.BleMAC,
		UWBCapable:      v.UWBCapable,
		UWBEnabled:      v.UWBEnabled,
		LastSeenAt:      v.LastSeenAt,
		SoftwareVersion: v.SoftwareVersion,
		OTAAvailable:    v.OTAAvailable,
		CreatedAt:       v.CreatedAt,
		UpdatedAt:       v.UpdatedAt,
	}
}
