package services

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"fmt"
	"io"
	"net/http"
	"time"

	"yuleDKCS/backend/internal/models"
	"gorm.io/gorm"
)

var (
	ErrFirmwareNotFound    = errors.New("固件不存在")
	ErrInvalidFirmwareType = errors.New("无效的固件类型")
	ErrVersionExists       = errors.New("该版本已存在")
	ErrDownloadFailed      = errors.New("固件下载失败")
	ErrInvalidChecksum     = errors.New("校验和不匹配")
	ErrOTAInProgress       = errors.New("已有OTA更新正在进行")
	ErrInvalidStatus       = errors.New("无效的OTA状态")
	ErrVehicleNotFound     = errors.New("车辆不存在")
)

// OTAService OTA服务接口
type OTAService interface {
	// 固件管理
	CreateFirmware(ctx context.Context, firmware *models.Firmware) (*models.Firmware, error)
	GetFirmwareByID(ctx context.Context, id uint) (*models.Firmware, error)
	GetFirmwareByVersion(ctx context.Context, version string, firmwareType models.FirmwareType) (*models.Firmware, error)
	ListFirmwares(ctx context.Context, firmwareType models.FirmwareType, page, pageSize int) ([]models.Firmware, int64, error)
	UpdateFirmware(ctx context.Context, id uint, updates map[string]interface{}) (*models.Firmware, error)
	DeleteFirmware(ctx context.Context, id uint) error
	DeactivateFirmware(ctx context.Context, id uint) error

	// OTA更新检查
	CheckUpdate(ctx context.Context, req *models.CheckUpdateRequest) (*models.CheckUpdateResponse, error)
	GetLatestFirmware(ctx context.Context, firmwareType models.FirmwareType, hardwareVersion string) (*models.Firmware, error)

	// OTA状态管理
	GetVehicleOTAStatus(ctx context.Context, vehicleID uint) (*models.VehicleOTAStatus, error)
	CreateOTAStatus(ctx context.Context, status *models.VehicleOTAStatus) (*models.VehicleOTAStatus, error)
	UpdateOTAStatus(ctx context.Context, vehicleID uint, req *models.UpdateOTAStatusRequest) (*models.VehicleOTAStatus, error)
	ListVehicleOTAHistory(ctx context.Context, vehicleID uint, page, pageSize int) ([]models.VehicleOTAStatus, int64, error)

	// OTA下载
	DownloadFirmware(ctx context.Context, firmwareID uint, vehicleID uint, ipAddress string) (string, error)
	RecordDownload(ctx context.Context, firmwareID, vehicleID uint, ipAddress string) error

	// 验证
	VerifyFirmwareChecksum(firmware *models.Firmware, data []byte) bool
	IsHardwareCompatible(firmware *models.Firmware, hardwareVersion string) bool

	// 批量操作
	GetVehiclesNeedingUpdate(ctx context.Context, firmwareID uint) ([]uint, error)
	ScheduleBatchUpdate(ctx context.Context, firmwareID uint, vehicleIDs []uint) error
}

// otaService OTA服务实现
type otaService struct {
	db *gorm.DB
}

// NewOTAService 创建OTA服务实例
func NewOTAService(db *gorm.DB) OTAService {
	return &otaService{
		db: db,
	}
}

// CreateFirmware 创建新固件
func (s *otaService) CreateFirmware(ctx context.Context, firmware *models.Firmware) (*models.Firmware, error) {
	// 验证固件类型
	if !isValidFirmwareType(firmware.Type) {
		return nil, ErrInvalidFirmwareType
	}

	// 检查版本是否已存在
	var existing models.Firmware
	result := s.db.WithContext(ctx).Where("version = ? AND type = ?", firmware.Version, firmware.Type).First(&existing)
	if result.Error == nil {
		return nil, ErrVersionExists
	}

	// 设置发布时间
	if firmware.ReleaseDate.IsZero() {
		firmware.ReleaseDate = time.Now()
	}

	if err := s.db.WithContext(ctx).Create(firmware).Error; err != nil {
		return nil, err
	}

	return firmware, nil
}

// GetFirmwareByID 根据ID获取固件
func (s *otaService) GetFirmwareByID(ctx context.Context, id uint) (*models.Firmware, error) {
	var firmware models.Firmware
	if err := s.db.WithContext(ctx).First(&firmware, id).Error; err != nil {
		if errors.Is(err, gorm.ErrRecordNotFound) {
			return nil, ErrFirmwareNotFound
		}
		return nil, err
	}
	return &firmware, nil
}

// GetFirmwareByVersion 根据版本获取固件
func (s *otaService) GetFirmwareByVersion(ctx context.Context, version string, firmwareType models.FirmwareType) (*models.Firmware, error) {
	var firmware models.Firmware
	if err := s.db.WithContext(ctx).Where("version = ? AND type = ?", version, firmwareType).First(&firmware).Error; err != nil {
		if errors.Is(err, gorm.ErrRecordNotFound) {
			return nil, ErrFirmwareNotFound
		}
		return nil, err
	}
	return &firmware, nil
}

// ListFirmwares 列出固件
func (s *otaService) ListFirmwares(ctx context.Context, firmwareType models.FirmwareType, page, pageSize int) ([]models.Firmware, int64, error) {
	var firmwares []models.Firmware
	var total int64

	query := s.db.WithContext(ctx).Model(&models.Firmware{})
	if firmwareType != "" {
		query = query.Where("type = ?", firmwareType)
	}

	if err := query.Count(&total).Error; err != nil {
		return nil, 0, err
	}

	offset := (page - 1) * pageSize
	if err := query.Order("created_at DESC").Offset(offset).Limit(pageSize).Find(&firmwares).Error; err != nil {
		return nil, 0, err
	}

	return firmwares, total, nil
}

// UpdateFirmware 更新固件信息
func (s *otaService) UpdateFirmware(ctx context.Context, id uint, updates map[string]interface{}) (*models.Firmware, error) {
	firmware, err := s.GetFirmwareByID(ctx, id)
	if err != nil {
		return nil, err
	}

	if err := s.db.WithContext(ctx).Model(firmware).Updates(updates).Error; err != nil {
		return nil, err
	}

	return firmware, nil
}

// DeleteFirmware 删除固件
func (s *otaService) DeleteFirmware(ctx context.Context, id uint) error {
	result := s.db.WithContext(ctx).Delete(&models.Firmware{}, id)
	if result.Error != nil {
		return result.Error
	}
	if result.RowsAffected == 0 {
		return ErrFirmwareNotFound
	}
	return nil
}

// DeactivateFirmware 停用固件
func (s *otaService) DeactivateFirmware(ctx context.Context, id uint) error {
	return s.db.WithContext(ctx).Model(&models.Firmware{}).Where("id = ?", id).Update("is_active", false).Error
}

// CheckUpdate 检查更新
func (s *otaService) CheckUpdate(ctx context.Context, req *models.CheckUpdateRequest) (*models.CheckUpdateResponse, error) {
	// 获取最新固件
	latestFirmware, err := s.GetLatestFirmware(ctx, models.FirmwareType(req.FirmwareType), req.HardwareVersion)
	if err != nil {
		if errors.Is(err, ErrFirmwareNotFound) {
			return &models.CheckUpdateResponse{
				HasUpdate:      false,
				Message:        "当前已是最新版本",
				UpdateRequired: false,
			}, nil
		}
		return nil, err
	}

	// 版本比较（简化版本，实际应该使用语义化版本比较）
	if latestFirmware.Version == req.CurrentVersion {
		return &models.CheckUpdateResponse{
			HasUpdate:      false,
			Message:        "当前已是最新版本",
			UpdateRequired: false,
		}, nil
	}

	// 检查是否需要强制更新（可以根据业务逻辑定义）
	updateRequired := false

	return &models.CheckUpdateResponse{
		HasUpdate:       true,
		Firmware:        latestFirmware,
		UpdateRequired:  updateRequired,
		Message:         fmt.Sprintf("发现新版本 %s", latestFirmware.Version),
	}, nil
}

// GetLatestFirmware 获取最新固件
func (s *otaService) GetLatestFirmware(ctx context.Context, firmwareType models.FirmwareType, hardwareVersion string) (*models.Firmware, error) {
	var firmware models.Firmware
	query := s.db.WithContext(ctx).Where("type = ? AND is_active = ?", firmwareType, true)
	
	if hardwareVersion != "" {
		query = query.Where("(min_hardware_version = '' OR min_hardware_version <= ?) AND (max_hardware_version = '' OR max_hardware_version >= ?)",
			hardwareVersion, hardwareVersion)
	}

	if err := query.Order("created_at DESC").First(&firmware).Error; err != nil {
		if errors.Is(err, gorm.ErrRecordNotFound) {
			return nil, ErrFirmwareNotFound
		}
		return nil, err
	}

	return &firmware, nil
}

// GetVehicleOTAStatus 获取车辆OTA状态
func (s *otaService) GetVehicleOTAStatus(ctx context.Context, vehicleID uint) (*models.VehicleOTAStatus, error) {
	var status models.VehicleOTAStatus
	if err := s.db.WithContext(ctx).Where("vehicle_id = ?", vehicleID).Order("created_at DESC").First(&status).Error; err != nil {
		if errors.Is(err, gorm.ErrRecordNotFound) {
			return nil, nil
		}
		return nil, err
	}
	return &status, nil
}

// CreateOTAStatus 创建OTA状态
func (s *otaService) CreateOTAStatus(ctx context.Context, status *models.VehicleOTAStatus) (*models.VehicleOTAStatus, error) {
	// 检查是否已有进行中的更新
	var existing models.VehicleOTAStatus
	err := s.db.WithContext(ctx).Where("vehicle_id = ? AND status IN ?", 
		status.VehicleID, []models.OTAStatus{models.OTAStatusPending, models.OTAStatusDownloading, models.OTAStatusInstalling}).
		First(&existing).Error
	
	if err == nil {
		return nil, ErrOTAInProgress
	}

	status.Status = models.OTAStatusPending
	status.Progress = 0
	
	if err := s.db.WithContext(ctx).Create(status).Error; err != nil {
		return nil, err
	}

	return status, nil
}

// UpdateOTAStatus 更新OTA状态
func (s *otaService) UpdateOTAStatus(ctx context.Context, vehicleID uint, req *models.UpdateOTAStatusRequest) (*models.VehicleOTAStatus, error) {
	// 验证状态值
	if !isValidOTAStatus(req.Status) {
		return nil, ErrInvalidStatus
	}

	// 查找最新的OTA状态记录
	var status models.VehicleOTAStatus
	if err := s.db.WithContext(ctx).Where("vehicle_id = ?", vehicleID).Order("created_at DESC").First(&status).Error; err != nil {
		if errors.Is(err, gorm.ErrRecordNotFound) {
			return nil, errors.New("没有找到OTA状态记录")
		}
		return nil, err
	}

	updates := map[string]interface{}{
		"status":   req.Status,
		"progress": req.Progress,
	}

	if req.ErrorMessage != "" {
		updates["error_message"] = req.ErrorMessage
	}

	// 如果状态变为安装中，记录开始时间
	if req.Status == models.OTAStatusInstalling && status.StartedAt == nil {
		now := time.Now()
		updates["started_at"] = &now
	}

	// 如果状态变为完成或失败，记录完成时间
	if (req.Status == models.OTAStatusCompleted || req.Status == models.OTAStatusFailed || req.Status == models.OTAStatusRolledBack) && status.CompletedAt == nil {
		now := time.Now()
		updates["completed_at"] = &now
	}

	if err := s.db.WithContext(ctx).Model(&status).Updates(updates).Error; err != nil {
		return nil, err
	}

	return &status, nil
}

// ListVehicleOTAHistory 获取车辆OTA历史
func (s *otaService) ListVehicleOTAHistory(ctx context.Context, vehicleID uint, page, pageSize int) ([]models.VehicleOTAStatus, int64, error) {
	var history []models.VehicleOTAStatus
	var total int64

	if err := s.db.WithContext(ctx).Model(&models.VehicleOTAStatus{}).Where("vehicle_id = ?", vehicleID).Count(&total).Error; err != nil {
		return nil, 0, err
	}

	offset := (page - 1) * pageSize
	if err := s.db.WithContext(ctx).Where("vehicle_id = ?", vehicleID).
		Order("created_at DESC").Offset(offset).Limit(pageSize).Find(&history).Error; err != nil {
		return nil, 0, err
	}

	return history, total, nil
}

// DownloadFirmware 下载固件
func (s *otaService) DownloadFirmware(ctx context.Context, firmwareID uint, vehicleID uint, ipAddress string) (string, error) {
	firmware, err := s.GetFirmwareByID(ctx, firmwareID)
	if err != nil {
		return "", err
	}

	// 记录下载
	if err := s.RecordDownload(ctx, firmwareID, vehicleID, ipAddress); err != nil {
		return "", err
	}

	return firmware.URL, nil
}

// RecordDownload 记录下载
func (s *otaService) RecordDownload(ctx context.Context, firmwareID, vehicleID uint, ipAddress string) error {
	record := &models.OTADownloadRecord{
		VehicleID:    vehicleID,
		FirmwareID:   firmwareID,
		IPAddress:    ipAddress,
		DownloadedAt: time.Now(),
		Status:       "success",
	}
	return s.db.WithContext(ctx).Create(record).Error
}

// VerifyFirmwareChecksum 验证固件校验和
func (s *otaService) VerifyFirmwareChecksum(firmware *models.Firmware, data []byte) bool {
	hash := sha256.Sum256(data)
	calculatedChecksum := hex.EncodeToString(hash[:])
	return calculatedChecksum == firmware.Checksum
}

// IsHardwareCompatible 检查硬件版本兼容性
func (s *otaService) IsHardwareCompatible(firmware *models.Firmware, hardwareVersion string) bool {
	// 如果没有指定最小硬件版本，则兼容所有
	if firmware.MinHardwareVersion == "" {
		// 但如果指定了最大版本，需要检查
		if firmware.MaxHardwareVersion != "" && hardwareVersion > firmware.MaxHardwareVersion {
			return false
		}
		return true
	}

	// 检查是否在范围内
	if hardwareVersion < firmware.MinHardwareVersion {
		return false
	}
	if firmware.MaxHardwareVersion != "" && hardwareVersion > firmware.MaxHardwareVersion {
		return false
	}
	return true
}

// GetVehiclesNeedingUpdate 获取需要更新的车辆列表
func (s *otaService) GetVehiclesNeedingUpdate(ctx context.Context, firmwareID uint) ([]uint, error) {
	firmware, err := s.GetFirmwareByID(ctx, firmwareID)
	if err != nil {
		return nil, err
	}

	// 查询所有不在这个版本的车辆
	var vehicleIDs []uint
	if err := s.db.WithContext(ctx).Model(&models.VehicleOTAStatus{}).
		Where("current_version != ? OR current_version IS NULL", firmware.Version).
		Pluck("vehicle_id", &vehicleIDs).Error; err != nil {
		return nil, err
	}

	return vehicleIDs, nil
}

// ScheduleBatchUpdate 批量调度更新
func (s *otaService) ScheduleBatchUpdate(ctx context.Context, firmwareID uint, vehicleIDs []uint) error {
	firmware, err := s.GetFirmwareByID(ctx, firmwareID)
	if err != nil {
		return err
	}

	now := time.Now()
	for _, vehicleID := range vehicleIDs {
		status := &models.VehicleOTAStatus{
			VehicleID:      vehicleID,
			TargetVersion:  firmware.Version,
			Status:         models.OTAStatusPending,
			Progress:       0,
			StartedAt:      &now,
		}
		if err := s.db.WithContext(ctx).Create(status).Error; err != nil {
			return err
		}
	}

	return nil
}

// isValidFirmwareType 验证固件类型
func isValidFirmwareType(firmwareType models.FirmwareType) bool {
	switch firmwareType {
	case models.FirmwareTypeECU, models.FirmwareTypeIVI, models.FirmwareTypeTBox,
		models.FirmwareTypeADAS, models.FirmwareTypeGateway, models.FirmwareTypeBMS:
		return true
	}
	return false
}

// isValidOTAStatus 验证OTA状态
func isValidOTAStatus(status models.OTAStatus) bool {
	switch status {
	case models.OTAStatusPending, models.OTAStatusDownloading, models.OTAStatusInstalling,
		models.OTAStatusCompleted, models.OTAStatusFailed, models.OTAStatusRolledBack:
		return true
	}
	return false
}

// downloadAndVerify 下载并验证固件
func downloadAndVerify(url string, expectedChecksum string) ([]byte, error) {
	client := &http.Client{
		Timeout: 5 * time.Minute,
	}

	resp, err := client.Get(url)
	if err != nil {
		return nil, ErrDownloadFailed
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return nil, ErrDownloadFailed
	}

	data, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	// 验证校验和
	hash := sha256.Sum256(data)
	calculatedChecksum := hex.EncodeToString(hash[:])
	if calculatedChecksum != expectedChecksum {
		return nil, ErrInvalidChecksum
	}

	return data, nil
}
