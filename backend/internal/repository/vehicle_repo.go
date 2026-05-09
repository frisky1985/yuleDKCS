package repository

import (
	"context"
	"errors"
	"time"

	"yuleDKCS/backend/internal/models"
	"gorm.io/gorm"
)

// VehicleRepository 车辆仓库接口
type VehicleRepository interface {
	// CRUD操作
	Create(ctx context.Context, vehicle *models.Vehicle) error
	GetByID(ctx context.Context, id uint) (*models.Vehicle, error)
	GetByVIN(ctx context.Context, vin string) (*models.Vehicle, error)
	GetByOwnerID(ctx context.Context, ownerID uint) ([]models.Vehicle, error)
	Update(ctx context.Context, vehicle *models.Vehicle) error
	Delete(ctx context.Context, id uint) error
	
	// 搜索和列表
	List(ctx context.Context, ownerID uint, page, pageSize int) ([]models.Vehicle, int64, error)
	Search(ctx context.Context, ownerID uint, keyword string) ([]models.Vehicle, error)
	
	// 状态更新
	UpdateStatus(ctx context.Context, id uint, status models.VehicleStatus) error
	UpdateLockStatus(ctx context.Context, id uint, status string) error
	UpdateEngineStatus(ctx context.Context, id uint, status string) error
	UpdateOnlineStatus(ctx context.Context, id uint, status string) error
	UpdateLastSeen(ctx context.Context, id uint) error
	UpdateLocation(ctx context.Context, id uint, location string) error
	
	// BLE/UWB 相关
	GetByBleMAC(ctx context.Context, mac string) (*models.Vehicle, error)
	UpdateUWBEnabled(ctx context.Context, id uint, enabled bool) error
	
	// OTA 相关
	UpdateSoftwareVersion(ctx context.Context, id uint, version string) error
	GetOTAInfo(ctx context.Context, id uint) (*models.VehicleOTAInfo, error)
	
	// 检查所有权
	IsOwner(ctx context.Context, vehicleID, userID uint) (bool, error)
}

// vehicleRepository 车辆仓库实现
type vehicleRepository struct {
	db *gorm.DB
}

// NewVehicleRepository 创建车辆仓库实例
func NewVehicleRepository(db *gorm.DB) VehicleRepository {
	return &vehicleRepository{db: db}
}

// Create 创建车辆
func (r *vehicleRepository) Create(ctx context.Context, vehicle *models.Vehicle) error {
	return r.db.WithContext(ctx).Create(vehicle).Error
}

// GetByID 根据ID获取车辆
func (r *vehicleRepository) GetByID(ctx context.Context, id uint) (*models.Vehicle, error) {
	var vehicle models.Vehicle
	result := r.db.WithContext(ctx).First(&vehicle, id)
	if result.Error != nil {
		if errors.Is(result.Error, gorm.ErrRecordNotFound) {
			return nil, errors.New("车辆不存在")
		}
		return nil, result.Error
	}
	return &vehicle, nil
}

// GetByVIN 根据VIN码获取车辆
func (r *vehicleRepository) GetByVIN(ctx context.Context, vin string) (*models.Vehicle, error) {
	var vehicle models.Vehicle
	result := r.db.WithContext(ctx).Where("vin = ?", vin).First(&vehicle)
	if result.Error != nil {
		if errors.Is(result.Error, gorm.ErrRecordNotFound) {
			return nil, errors.New("车辆不存在")
		}
		return nil, result.Error
	}
	return &vehicle, nil
}

// GetByOwnerID 根据所有者ID获取车辆列表
func (r *vehicleRepository) GetByOwnerID(ctx context.Context, ownerID uint) ([]models.Vehicle, error) {
	var vehicles []models.Vehicle
	result := r.db.WithContext(ctx).Where("owner_id = ?", ownerID).Find(&vehicles)
	return vehicles, result.Error
}

// Update 更新车辆信息
func (r *vehicleRepository) Update(ctx context.Context, vehicle *models.Vehicle) error {
	return r.db.WithContext(ctx).Save(vehicle).Error
}

// Delete 删除车辆
func (r *vehicleRepository) Delete(ctx context.Context, id uint) error {
	return r.db.WithContext(ctx).Delete(&models.Vehicle{}, id).Error
}

// List 获取车辆列表（分页）
func (r *vehicleRepository) List(ctx context.Context, ownerID uint, page, pageSize int) ([]models.Vehicle, int64, error) {
	var vehicles []models.Vehicle
	var total int64
	
	offset := (page - 1) * pageSize
	if pageSize <= 0 {
		pageSize = 10
	}
	
	query := r.db.WithContext(ctx).Model(&models.Vehicle{})
	if ownerID > 0 {
		query = query.Where("owner_id = ?", ownerID)
	}
	
	if err := query.Count(&total).Error; err != nil {
		return nil, 0, err
	}
	
	result := query.Offset(offset).Limit(pageSize).Order("created_at DESC").Find(&vehicles)
	return vehicles, total, result.Error
}

// Search 搜索车辆
func (r *vehicleRepository) Search(ctx context.Context, ownerID uint, keyword string) ([]models.Vehicle, error) {
	var vehicles []models.Vehicle
	query := r.db.WithContext(ctx).Where("owner_id = ?", ownerID)
	
	if keyword != "" {
		query = query.Where("name LIKE ? OR vin LIKE ? OR plate LIKE ? OR brand LIKE ? OR model LIKE ?",
			"%"+keyword+"%", "%"+keyword+"%", "%"+keyword+"%", "%"+keyword+"%", "%"+keyword+"%")
	}
	
	result := query.Find(&vehicles)
	return vehicles, result.Error
}

// UpdateStatus 更新车辆状态
func (r *vehicleRepository) UpdateStatus(ctx context.Context, id uint, status models.VehicleStatus) error {
	return r.db.WithContext(ctx).Model(&models.Vehicle{}).Where("id = ?", id).Update("status", status).Error
}

// UpdateLockStatus 更新锁车状态
func (r *vehicleRepository) UpdateLockStatus(ctx context.Context, id uint, status string) error {
	return r.db.WithContext(ctx).Model(&models.Vehicle{}).Where("id = ?", id).Update("lock_status", status).Error
}

// UpdateEngineStatus 更新发动机状态
func (r *vehicleRepository) UpdateEngineStatus(ctx context.Context, id uint, status string) error {
	return r.db.WithContext(ctx).Model(&models.Vehicle{}).Where("id = ?", id).Update("engine_status", status).Error
}

// UpdateOnlineStatus 更新在线状态
func (r *vehicleRepository) UpdateOnlineStatus(ctx context.Context, id uint, status string) error {
	return r.db.WithContext(ctx).Model(&models.Vehicle{}).Where("id = ?", id).Update("online_status", status).Error
}

// UpdateLastSeen 更新最后在线时间
func (r *vehicleRepository) UpdateLastSeen(ctx context.Context, id uint) error {
	now := time.Now()
	return r.db.WithContext(ctx).Model(&models.Vehicle{}).Where("id = ?", id).Updates(map[string]interface{}{
		"last_seen_at":   now,
		"online_status":  "online",
	}).Error
}

// UpdateLocation 更新位置信息
func (r *vehicleRepository) UpdateLocation(ctx context.Context, id uint, location string) error {
	now := time.Now()
	return r.db.WithContext(ctx).Model(&models.Vehicle{}).Where("id = ?", id).Updates(map[string]interface{}{
		"location":          location,
		"last_location_at":  now,
	}).Error
}

// GetByBleMAC 根据BLE MAC地址获取车辆
func (r *vehicleRepository) GetByBleMAC(ctx context.Context, mac string) (*models.Vehicle, error) {
	var vehicle models.Vehicle
	result := r.db.WithContext(ctx).Where("ble_mac = ?", mac).First(&vehicle)
	if result.Error != nil {
		if errors.Is(result.Error, gorm.ErrRecordNotFound) {
			return nil, errors.New("车辆不存在")
		}
		return nil, result.Error
	}
	return &vehicle, nil
}

// UpdateUWBEnabled 更新UWB启用状态
func (r *vehicleRepository) UpdateUWBEnabled(ctx context.Context, id uint, enabled bool) error {
	return r.db.WithContext(ctx).Model(&models.Vehicle{}).Where("id = ?", id).Update("uwb_enabled", enabled).Error
}

// UpdateSoftwareVersion 更新软件版本
func (r *vehicleRepository) UpdateSoftwareVersion(ctx context.Context, id uint, version string) error {
	return r.db.WithContext(ctx).Model(&models.Vehicle{}).Where("id = ?", id).Update("software_version", version).Error
}

// GetOTAInfo 获取OTA信息
func (r *vehicleRepository) GetOTAInfo(ctx context.Context, id uint) (*models.VehicleOTAInfo, error) {
	var vehicle models.Vehicle
	result := r.db.WithContext(ctx).Select("software_version", "ota_target_version", "ota_available").First(&vehicle, id)
	if result.Error != nil {
		if errors.Is(result.Error, gorm.ErrRecordNotFound) {
			return nil, errors.New("车辆不存在")
		}
		return nil, result.Error
	}
	
	return &models.VehicleOTAInfo{
		CurrentVersion:  vehicle.SoftwareVersion,
		TargetVersion:   vehicle.OTATargetVersion,
		UpdateAvailable: vehicle.OTAAvailable,
	}, nil
}

// IsOwner 检查用户是否是车辆所有者
func (r *vehicleRepository) IsOwner(ctx context.Context, vehicleID, userID uint) (bool, error) {
	var count int64
	result := r.db.WithContext(ctx).Model(&models.Vehicle{}).Where("id = ? AND owner_id = ?", vehicleID, userID).Count(&count)
	if result.Error != nil {
		return false, result.Error
	}
	return count > 0, nil
}
