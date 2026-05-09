package handlers

import (
	"net/http"
	"strconv"
	"time"

	"github.com/frisky1985/yuleDKCS/backend/internal/models"
	"github.com/frisky1985/yuleDKCS/backend/internal/services"
	"github.com/gin-gonic/gin"
)

type VehicleHandler struct {
	vehicleService *services.VehicleService
}

func NewVehicleHandler(vehicleService *services.VehicleService) *VehicleHandler {
	return &VehicleHandler{vehicleService: vehicleService}
}

// RegisterVehicle 注册新车辆
func (h *VehicleHandler) RegisterVehicle(c *gin.Context) {
	userID := c.GetUint("userID")

	var req struct {
		VIN            string `json:"vin" binding:"required,len=17"`
		Brand          string `json:"brand" binding:"required"`
		Model          string `json:"model" binding:"required"`
		Year           int    `json:"year" binding:"required,min=2000,max=2030"`
		BLEMac         string `json:"ble_mac" binding:"omitempty,mac"`
		UWBCapable     bool   `json:"uwb_capable"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	vehicle := &models.Vehicle{
		VIN:        req.VIN,
		Brand:      req.Brand,
		Model:      req.Model,
		Year:       req.Year,
		OwnerID:    userID,
		Status:     models.VehicleStatusOffline,
		BLEMac:     req.BLEMac,
		UWBCapable: req.UWBCapable,
	}

	if err := h.vehicleService.RegisterVehicle(vehicle); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusCreated, vehicle)
}

// GetUserVehicles 获取用户车辆列表
func (h *VehicleHandler) GetUserVehicles(c *gin.Context) {
	userID := c.GetUint("userID")
	page, _ := strconv.Atoi(c.DefaultQuery("page", "1"))
	pageSize, _ := strconv.Atoi(c.DefaultQuery("page_size", "10"))

	vehicles, total, err := h.vehicleService.GetUserVehicles(userID, page, pageSize)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"vehicles": vehicles,
		"total":    total,
		"page":     page,
		"pageSize": pageSize,
	})
}

// GetVehicle 获取车辆详情
func (h *VehicleHandler) GetVehicle(c *gin.Context) {
	userID := c.GetUint("userID")
	vehicleID, _ := strconv.ParseUint(c.Param("id"), 10, 32)

	vehicle, err := h.vehicleService.GetVehicleByID(uint(vehicleID), userID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "vehicle not found"})
		return
	}

	c.JSON(http.StatusOK, vehicle)
}

// GetVehicleStatus 获取车辆实时状态
func (h *VehicleHandler) GetVehicleStatus(c *gin.Context) {
	userID := c.GetUint("userID")
	vehicleID, _ := strconv.ParseUint(c.Param("id"), 10, 32)

	status, err := h.vehicleService.GetVehicleStatus(uint(vehicleID), userID)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, status)
}

// SendCommand 发送车辆控制命令
func (h *VehicleHandler) SendCommand(c *gin.Context) {
	userID := c.GetUint("userID")
	vehicleID, _ := strconv.ParseUint(c.Param("id"), 10, 32)

	var req struct {
		Command   string                 `json:"command" binding:"required"`
		Params    map[string]interface{} `json:"params"`
		RequestID string                 `json:"request_id"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	// 验证命令类型
	validCommands := map[string]bool{
		"unlock":      true,
		"lock":        true,
		"engine_start": true,
		"engine_stop": true,
		"trunk":       true,
		"windows_up":  true,
		"windows_down": true,
		"find_my_car": true,
	}

	if !validCommands[req.Command] {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid command"})
		return
	}

	command := &models.VehicleCommand{
		VehicleID: uint(vehicleID),
		UserID:    userID,
		Command:   req.Command,
		Params:    req.Params,
		Status:    models.CommandStatusPending,
		RequestID: req.RequestID,
	}

	if err := h.vehicleService.SendCommand(command); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusAccepted, gin.H{
		"command_id": command.ID,
		"status":     command.Status,
		"request_id": command.RequestID,
	})
}

// UpdateLocation 更新车辆位置
func (h *VehicleHandler) UpdateLocation(c *gin.Context) {
	vehicleID, _ := strconv.ParseUint(c.Param("id"), 10, 32)

	var req struct {
		Latitude  float64 `json:"latitude" binding:"required"`
		Longitude float64 `json:"longitude" binding:"required"`
		Altitude  float64 `json:"altitude"`
		Accuracy  float64 `json:"accuracy"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	location := &models.VehicleLocation{
		VehicleID: uint(vehicleID),
		Latitude:  req.Latitude,
		Longitude: req.Longitude,
		Altitude:  req.Altitude,
		Accuracy:  req.Accuracy,
	}

	if err := h.vehicleService.UpdateLocation(location); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "location updated"})
}

// GetCommandResult 获取命令执行结果
func (h *VehicleHandler) GetCommandResult(c *gin.Context) {
	userID := c.GetUint("userID")
	commandID, _ := strconv.ParseUint(c.Param("command_id"), 10, 32)

	command, err := h.vehicleService.GetCommandResult(uint(commandID), userID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "command not found"})
		return
	}

	c.JSON(http.StatusOK, command)
}

// UpdateVehicleStatus 车辆上报状态（由车机调用）
func (h *VehicleHandler) UpdateVehicleStatus(c *gin.Context) {
	vehicleID, _ := strconv.ParseUint(c.Param("id"), 10, 32)

	var req struct {
		Status        models.VehicleStatusType `json:"status"`
		LockState     string                   `json:"lock_state"`
		BatteryLevel  int                      `json:"battery_level"`
		ChargingState string                   `json:"charging_state"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	status := &models.VehicleStatus{
		VehicleID:     uint(vehicleID),
		Status:        req.Status,
		LockState:     req.LockState,
		BatteryLevel:  req.BatteryLevel,
		ChargingState: req.ChargingState,
		LastSeen:      time.Now(),
	}

	if err := h.vehicleService.UpdateStatus(status); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "status updated"})
}
