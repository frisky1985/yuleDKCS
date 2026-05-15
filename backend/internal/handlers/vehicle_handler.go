package handlers

import (
	"net/http"
	"strconv"

	"github.com/frisky1985/yuleDKCS/backend/internal/models"
	"github.com/frisky1985/yuleDKCS/backend/internal/services"
	"github.com/gin-gonic/gin"
)

type VehicleHandler struct {
	vehicleService services.VehicleService
}

func NewVehicleHandler(vehicleService services.VehicleService) *VehicleHandler {
	return &VehicleHandler{vehicleService: vehicleService}
}

// RegisterVehicle 注册新车辆
func (h *VehicleHandler) RegisterVehicle(c *gin.Context) {
	userID, exists := c.Get("userID")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}

	var req models.VehicleRegisterRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	vehicle, err := h.vehicleService.RegisterVehicle(c.Request.Context(), userID.(uint), &req)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusCreated, vehicle)
}

// GetUserVehicles 获取用户车辆列表
func (h *VehicleHandler) GetUserVehicles(c *gin.Context) {
	userID, exists := c.Get("userID")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}

	page, _ := strconv.Atoi(c.DefaultQuery("page", "1"))
	pageSize, _ := strconv.Atoi(c.DefaultQuery("page_size", "10"))

	vehicles, total, err := h.vehicleService.GetUserVehicles(c.Request.Context(), userID.(uint), page, pageSize)
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
	userID, exists := c.Get("userID")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}

	vehicleID, err := strconv.ParseUint(c.Param("id"), 10, 32)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid vehicle id"})
		return
	}

	vehicle, err := h.vehicleService.GetVehicle(c.Request.Context(), uint(vehicleID), userID.(uint))
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "vehicle not found"})
		return
	}

	c.JSON(http.StatusOK, vehicle)
}

// GetVehicleStatus 获取车辆实时状态
func (h *VehicleHandler) GetVehicleStatus(c *gin.Context) {
	userID, exists := c.Get("userID")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}

	vehicleID, err := strconv.ParseUint(c.Param("id"), 10, 32)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid vehicle id"})
		return
	}

	status, err := h.vehicleService.GetVehicleStatus(c.Request.Context(), uint(vehicleID), userID.(uint))
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, status)
}

// SendCommand 发送车辆控制命令
func (h *VehicleHandler) SendCommand(c *gin.Context) {
	userID, exists := c.Get("userID")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}

	vehicleID, err := strconv.ParseUint(c.Param("id"), 10, 32)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid vehicle id"})
		return
	}

	var req models.VehicleCommandRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	status, err := h.vehicleService.SendCommand(c.Request.Context(), uint(vehicleID), userID.(uint), &req)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusAccepted, gin.H{
		"command_id": status.CommandID,
		"status":     status.Status,
	})
}

// UpdateLocation 更新车辆位置
func (h *VehicleHandler) UpdateLocation(c *gin.Context) {
	userID, exists := c.Get("userID")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}

	vehicleID, err := strconv.ParseUint(c.Param("id"), 10, 32)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid vehicle id"})
		return
	}

	var req models.VehicleLocationRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	if err := h.vehicleService.UpdateLocation(c.Request.Context(), uint(vehicleID), userID.(uint), &req); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "location updated"})
}

// GetCommandStatus 获取命令执行状态
func (h *VehicleHandler) GetCommandStatus(c *gin.Context) {
	commandID := c.Param("command_id")

	status, err := h.vehicleService.GetCommandStatus(c.Request.Context(), commandID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "command not found"})
		return
	}

	c.JSON(http.StatusOK, status)
}

// Heartbeat 车辆心跳上报
func (h *VehicleHandler) Heartbeat(c *gin.Context) {
	vehicleID, err := strconv.ParseUint(c.Param("id"), 10, 32)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid vehicle id"})
		return
	}

	var data map[string]interface{}
	if err := c.ShouldBindJSON(&data); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	if err := h.vehicleService.Heartbeat(c.Request.Context(), uint(vehicleID), data); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "heartbeat received"})
}
