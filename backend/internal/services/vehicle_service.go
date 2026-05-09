package services

import (
	"context"
	"crypto/rand"
	"encoding/json"
	"errors"
	"fmt"
	"strings"
	"time"

	"yuleDKCS/backend/internal/models"
	"yuleDKCS/backend/internal/repository"
)

var (
	ErrVehicleNotFound    = errors.New("车辆不存在")
	ErrVehicleExists      = errors.New("车辆已存在")
	ErrNotVehicleOwner    = errors.New("不是车辆所有者")
	ErrInvalidCommand     = errors.New("无效的命令")
	ErrVehicleOffline     = errors.New("车辆离线")
	ErrCommandTimeout     = errors.New("命令超时")
	ErrInvalidLocation    = errors.New("无效的位置信息")
)

// VehicleCommandStatus 车辆命令状态
type VehicleCommandStatus struct {
	CommandID  string
	Command    models.VehicleCommand
	Status     string // pending, executing, completed, failed
	Message    string
	CreatedAt  time.Time
	ExecutedAt *time.Time
}

// VehicleService 车辆服务接口
type VehicleService interface {
	// 车辆管理
	RegisterVehicle(ctx context.Context, ownerID uint, req *models.VehicleRegisterRequest) (*models.Vehicle, error)
	GetVehicle(ctx context.Context, vehicleID uint, userID uint) (*models.Vehicle, error)
	GetUserVehicles(ctx context.Context, userID uint, page, pageSize int) ([]models.Vehicle, int64, error)
	UpdateVehicle(ctx context.Context, vehicleID uint, userID uint, req *models.VehicleUpdateRequest) (*models.Vehicle, error)
	DeleteVehicle(ctx context.Context, vehicleID uint, userID uint) error
	SearchVehicles(ctx context.Context, userID uint, keyword string) ([]models.Vehicle, error)
	
	// 状态查询
	GetVehicleStatus(ctx context.Context, vehicleID uint, userID uint) (*models.VehicleStatusResponse, error)
	
	// 远程控制
	SendCommand(ctx context.Context, vehicleID uint, userID uint, req *models.VehicleCommandRequest) (*VehicleCommandStatus, error)
	GetCommandStatus(ctx context.Context, commandID string) (*VehicleCommandStatus, error)
	
	// 位置服务
	UpdateLocation(ctx context.Context, vehicleID uint, userID uint, req *models.VehicleLocationRequest) error
	GetVehicleLocation(ctx context.Context, vehicleID uint, userID uint) (*models.VehicleLocation, error)
	
	// OTA服务
	GetOTAInfo(ctx context.Context, vehicleID uint, userID uint) (*models.VehicleOTAInfo, error)
	RequestOTAUpdate(ctx context.Context, vehicleID uint, userID uint) error
	
	// BLE/UWB服务
	EnableUWB(ctx context.Context, vehicleID uint, userID uint) error
	DisableUWB(ctx context.Context, vehicleID uint, userID uint) error
	GetVehicleByBleMAC(ctx context.Context, mac string) (*models.Vehicle, error)
	
	// 车辆心跳
	Heartbeat(ctx context.Context, vehicleID uint, data map[string]interface{}) error
}

// vehicleService 车辆服务实现
type vehicleService struct {
	vehicleRepo repository.VehicleRepository
	keyRepo     repository.KeyRepository
	commands    map[string]*VehicleCommandStatus // 存储命令状态
}

// NewVehicleService 创建车辆服务实例
func NewVehicleService(vehicleRepo repository.VehicleRepository, keyRepo repository.KeyRepository) VehicleService {
	return &vehicleService{
		vehicleRepo: vehicleRepo,
		keyRepo:     keyRepo,
		commands:    make(map[string]*VehicleCommandStatus),
	}
}

// generateCommandID 生成命令ID
func generateCommandID() string {
	bytes := make([]byte, 8)
	rand.Read(bytes)
	return fmt.Sprintf("cmd_%s_%d", fmt.Sprintf("%x", bytes), time.Now().Unix())
}

// RegisterVehicle 注册新车辆
func (s *vehicleService) RegisterVehicle(ctx context.Context, ownerID uint, req *models.VehicleRegisterRequest) (*models.Vehicle, error) {
	// 检查VIN是否已存在
	existingVehicle, err := s.vehicleRepo.GetByVIN(ctx, req.Vin)
	if err == nil && existingVehicle != nil {
		return nil, ErrVehicleExists
	}
	
	// 标准化BLE MAC地址
	bleMAC := strings.ToUpper(req.BleMAC)
	if bleMAC != "" {
		// 检查MAC地址格式
		if len(bleMAC) != 17 {
			bleMAC = "" // 如果格式不正确则清空
		}
	}
	
	vehicle := &models.Vehicle{
		OwnerID:      ownerID,
		Name:         req.Name,
		Vin:          strings.ToUpper(req.Vin),
		Brand:        req.Brand,
		Model:        req.Model,
		Year:         req.Year,
		Color:        req.Color,
		Plate:        req.Plate,
		BleMAC:       bleMAC,
		UWBCapable:   req.UWBCapable,
		UWBEnabled:   false,
		Status:       models.VehicleStatusActive,
		LockStatus:   "locked",
		EngineStatus: "off",
		OnlineStatus: "offline",
	}
	
	if err := s.vehicleRepo.Create(ctx, vehicle); err != nil {
		return nil, err
	}
	
	return vehicle, nil
}

// GetVehicle 获取车辆详情
func (s *vehicleService) GetVehicle(ctx context.Context, vehicleID uint, userID uint) (*models.Vehicle, error) {
	vehicle, err := s.vehicleRepo.GetByID(ctx, vehicleID)
	if err != nil {
		if err.Error() == "车辆不存在" {
			return nil, ErrVehicleNotFound
		}
		return nil, err
	}
	
	// 验证访问权限（所有者或有钥匙的用户）
	if vehicle.OwnerID != userID {
		// 检查用户是否拥有该车辆的钥匙
		hasKey, err := s.checkUserHasKey(ctx, vehicleID, userID)
		if err != nil {
			return nil, err
		}
		if !hasKey {
			return nil, ErrUnauthorized
		}
	}
	
	return vehicle, nil
}

// checkUserHasKey 检查用户是否有车辆钥匙
func (s *vehicleService) checkUserHasKey(ctx context.Context, vehicleID, userID uint) (bool, error) {
	// 通过钥匙仓库检查（需要实现GetByVehicleIDAndUserID方法）
	// 这里简化处理，直接返回false，实际使用时需要实现完整逻辑
	return false, nil
}

// GetUserVehicles 获取用户的车辆列表
func (s *vehicleService) GetUserVehicles(ctx context.Context, userID uint, page, pageSize int) ([]models.Vehicle, int64, error) {
	return s.vehicleRepo.List(ctx, userID, page, pageSize)
}

// UpdateVehicle 更新车辆信息
func (s *vehicleService) UpdateVehicle(ctx context.Context, vehicleID uint, userID uint, req *models.VehicleUpdateRequest) (*models.Vehicle, error) {
	vehicle, err := s.vehicleRepo.GetByID(ctx, vehicleID)
	if err != nil {
		if err.Error() == "车辆不存在" {
			return nil, ErrVehicleNotFound
		}
		return nil, err
	}
	
	// 只有所有者可以更新
	if vehicle.OwnerID != userID {
		return nil, ErrNotVehicleOwner
	}
	
	// 更新字段
	if req.Name != "" {
		vehicle.Name = req.Name
	}
	if req.Plate != "" {
		vehicle.Plate = req.Plate
	}
	if req.Color != "" {
		vehicle.Color = req.Color
	}
	
	if err := s.vehicleRepo.Update(ctx, vehicle); err != nil {
		return nil, err
	}
	
	return vehicle, nil
}

// DeleteVehicle 删除车辆
func (s *vehicleService) DeleteVehicle(ctx context.Context, vehicleID uint, userID uint) error {
	vehicle, err := s.vehicleRepo.GetByID(ctx, vehicleID)
	if err != nil {
		if err.Error() == "车辆不存在" {
			return ErrVehicleNotFound
		}
		return err
	}
	
	// 只有所有者可以删除
	if vehicle.OwnerID != userID {
		return ErrNotVehicleOwner
	}
	
	return s.vehicleRepo.Delete(ctx, vehicleID)
}

// SearchVehicles 搜索车辆
func (s *vehicleService) SearchVehicles(ctx context.Context, userID uint, keyword string) ([]models.Vehicle, error) {
	return s.vehicleRepo.Search(ctx, userID, keyword)
}

// GetVehicleStatus 获取车辆状态
func (s *vehicleService) GetVehicleStatus(ctx context.Context, vehicleID uint, userID uint) (*models.VehicleStatusResponse, error) {
	vehicle, err := s.GetVehicle(ctx, vehicleID, userID)
	if err != nil {
		return nil, err
	}
	
	status := &models.VehicleStatusResponse{
		ID:              vehicle.ID,
		Status:          vehicle.Status,
		LockStatus:      vehicle.LockStatus,
		EngineStatus:    vehicle.EngineStatus,
		OnlineStatus:    vehicle.OnlineStatus,
		LastSeenAt:      vehicle.LastSeenAt,
		SoftwareVersion: vehicle.SoftwareVersion,
	}
	
	// 解析位置信息
	if vehicle.Location != "" {
		var location models.VehicleLocation
		if err := json.Unmarshal([]byte(vehicle.Location), &location); err == nil {
			status.Location = &location
		}
	}
	
	return status, nil
}

// SendCommand 发送远程控制命令
func (s *vehicleService) SendCommand(ctx context.Context, vehicleID uint, userID uint, req *models.VehicleCommandRequest) (*VehicleCommandStatus, error) {
	vehicle, err := s.GetVehicle(ctx, vehicleID, userID)
	if err != nil {
		return nil, err
	}
	
	// 验证命令
	if !isValidCommand(req.Command) {
		return nil, ErrInvalidCommand
	}
	
	// 检查车辆是否在线
	if vehicle.OnlineStatus != "online" {
		return nil, ErrVehicleOffline
	}
	
	commandID := generateCommandID()
	commandStatus := &VehicleCommandStatus{
		CommandID: commandID,
		Command:   req.Command,
		Status:    "pending",
		CreatedAt: time.Now(),
	}
	
	// 保存命令状态
	s.commands[commandID] = commandStatus
	
	// 异步执行命令（实际应用中会通过MQTT/WebSocket发送到车机）
	go s.executeCommand(vehicleID, commandStatus, req.Params)
	
	return commandStatus, nil
}

// isValidCommand 验证命令是否有效
func isValidCommand(cmd models.VehicleCommand) bool {
	validCommands := []models.VehicleCommand{
		models.VehicleCommandUnlock,
		models.VehicleCommandLock,
		models.VehicleCommandEngineStart,
		models.VehicleCommandEngineStop,
		models.VehicleCommandTrunk,
		models.VehicleCommandWindowsUp,
		models.VehicleCommandWindowsDown,
		models.VehicleCommandFindMyCar,
	}
	
	for _, validCmd := range validCommands {
		if cmd == validCmd {
			return true
		}
	}
	return false
}

// executeCommand 执行命令（模拟）
func (s *vehicleService) executeCommand(vehicleID uint, status *VehicleCommandStatus, params map[string]interface{}) {
	// 模拟命令执行过程
	time.Sleep(2 * time.Second)
	
	// 更新状态为执行中
	status.Status = "executing"
	
	// 模拟执行时间
	time.Sleep(3 * time.Second)
	
	// 更新状态为已完成
	now := time.Now()
	status.Status = "completed"
	status.Message = "命令执行成功"
	status.ExecutedAt = &now
	
	// 实际应用中这里会更新数据库中的车辆状态
	ctx := context.Background()
	switch status.Command {
	case models.VehicleCommandLock:
		s.vehicleRepo.UpdateLockStatus(ctx, vehicleID, "locked")
	case models.VehicleCommandUnlock:
		s.vehicleRepo.UpdateLockStatus(ctx, vehicleID, "unlocked")
	case models.VehicleCommandEngineStart:
		s.vehicleRepo.UpdateEngineStatus(ctx, vehicleID, "on")
	case models.VehicleCommandEngineStop:
		s.vehicleRepo.UpdateEngineStatus(ctx, vehicleID, "off")
	}
}

// GetCommandStatus 获取命令状态
func (s *vehicleService) GetCommandStatus(ctx context.Context, commandID string) (*VehicleCommandStatus, error) {
	status, exists := s.commands[commandID]
	if !exists {
		return nil, errors.New("命令不存在")
	}
	return status, nil
}

// UpdateLocation 更新车辆位置
func (s *vehicleService) UpdateLocation(ctx context.Context, vehicleID uint, userID uint, req *models.VehicleLocationRequest) error {
	vehicle, err := s.vehicleRepo.GetByID(ctx, vehicleID)
	if err != nil {
		if err.Error() == "车辆不存在" {
			return ErrVehicleNotFound
		}
		return err
	}
	
	// 验证坐标
	if req.Latitude < -90 || req.Latitude > 90 {
		return ErrInvalidLocation
	}
	if req.Longitude < -180 || req.Longitude > 180 {
		return ErrInvalidLocation
	}
	
	location := models.VehicleLocation{
		Latitude:  req.Latitude,
		Longitude: req.Longitude,
		Altitude:  req.Altitude,
		Accuracy:  req.Accuracy,
		Timestamp: time.Now().Unix(),
	}
	
	locationJSON, err := json.Marshal(location)
	if err != nil {
		return err
	}
	
	return s.vehicleRepo.UpdateLocation(ctx, vehicleID, string(locationJSON))
}

// GetVehicleLocation 获取车辆位置
func (s *vehicleService) GetVehicleLocation(ctx context.Context, vehicleID uint, userID uint) (*models.VehicleLocation, error) {
	vehicle, err := s.GetVehicle(ctx, vehicleID, userID)
	if err != nil {
		return nil, err
	}
	
	if vehicle.Location == "" {
		return nil, nil
	}
	
	var location models.VehicleLocation
	if err := json.Unmarshal([]byte(vehicle.Location), &location); err != nil {
		return nil, err
	}
	
	return &location, nil
}

// GetOTAInfo 获取OTA信息
func (s *vehicleService) GetOTAInfo(ctx context.Context, vehicleID uint, userID uint) (*models.VehicleOTAInfo, error) {
	vehicle, err := s.GetVehicle(ctx, vehicleID, userID)
	if err != nil {
		return nil, err
	}
	
	return &models.VehicleOTAInfo{
		CurrentVersion:  vehicle.SoftwareVersion,
		TargetVersion:   vehicle.OTATargetVersion,
		UpdateAvailable: vehicle.OTAAvailable,
	}, nil
}

// RequestOTAUpdate 请求OTA更新
func (s *vehicleService) RequestOTAUpdate(ctx context.Context, vehicleID uint, userID uint) error {
	vehicle, err := s.vehicleRepo.GetByID(ctx, vehicleID)
	if err != nil {
		if err.Error() == "车辆不存在" {
			return ErrVehicleNotFound
		}
		return err
	}
	
	// 只有所有者可以请求OTA
	if vehicle.OwnerID != userID {
		return ErrNotVehicleOwner
	}
	
	if !vehicle.OTAAvailable {
		return errors.New("暂无可用的OTA更新")
	}
	
	// 实际应用中会触发OTA更新流程
	return nil
}

// EnableUWB 启用UWB
func (s *vehicleService) EnableUWB(ctx context.Context, vehicleID uint, userID uint) error {
	vehicle, err := s.vehicleRepo.GetByID(ctx, vehicleID)
	if err != nil {
		if err.Error() == "车辆不存在" {
			return ErrVehicleNotFound
		}
		return err
	}
	
	// 只有所有者可以启用UWB
	if vehicle.OwnerID != userID {
		return ErrNotVehicleOwner
	}
	
	if !vehicle.UWBCapable {
		return errors.New("车辆不支持UWB")
	}
	
	return s.vehicleRepo.UpdateUWBEnabled(ctx, vehicleID, true)
}

// DisableUWB 禁用UWB
func (s *vehicleService) DisableUWB(ctx context.Context, vehicleID uint, userID uint) error {
	vehicle, err := s.vehicleRepo.GetByID(ctx, vehicleID)
	if err != nil {
		if err.Error() == "车辆不存在" {
			return ErrVehicleNotFound
		}
		return err
	}
	
	// 只有所有者可以禁用UWB
	if vehicle.OwnerID != userID {
		return ErrNotVehicleOwner
	}
	
	return s.vehicleRepo.UpdateUWBEnabled(ctx, vehicleID, false)
}

// GetVehicleByBleMAC 通过BLE MAC地址获取车辆
func (s *vehicleService) GetVehicleByBleMAC(ctx context.Context, mac string) (*models.Vehicle, error) {
	mac = strings.ToUpper(mac)
	vehicle, err := s.vehicleRepo.GetByBleMAC(ctx, mac)
	if err != nil {
		if err.Error() == "车辆不存在" {
			return nil, ErrVehicleNotFound
		}
		return nil, err
	}
	return vehicle, nil
}

// Heartbeat 车辆心跳
func (s *vehicleService) Heartbeat(ctx context.Context, vehicleID uint, data map[string]interface{}) error {
	vehicle, err := s.vehicleRepo.GetByID(ctx, vehicleID)
	if err != nil {
		return err
	}
	
	// 更新在线状态和最后在线时间
	if err := s.vehicleRepo.UpdateLastSeen(ctx, vehicleID); err != nil {
		return err
	}
	
	// 更新其他状态
	if lockStatus, ok := data["lock_status"].(string); ok {
		s.vehicleRepo.UpdateLockStatus(ctx, vehicleID, lockStatus)
	}
	if engineStatus, ok := data["engine_status"].(string); ok {
		s.vehicleRepo.UpdateEngineStatus(ctx, vehicleID, engineStatus)
	}
	if location, ok := data["location"].(map[string]interface{}); ok {
		locationJSON, _ := json.Marshal(location)
		s.vehicleRepo.UpdateLocation(ctx, vehicleID, string(locationJSON))
	}
	if softwareVersion, ok := data["software_version"].(string); ok {
		s.vehicleRepo.UpdateSoftwareVersion(ctx, vehicleID, softwareVersion)
	}
	
	return nil
}
