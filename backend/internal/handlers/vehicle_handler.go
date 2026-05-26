package handlers

import (
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
		Unauthorized(c, "未认证")
		return
	}

	var req models.VehicleRegisterRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		BadRequest(c, "请求参数错误", err.Error())
		return
	}

	vehicle, err := h.vehicleService.RegisterVehicle(c.Request.Context(), userID.(uint), &req)
	if err != nil {
		InternalError(c, err.Error())
		return
	}

	SuccessCreated(c, vehicle)
}

// GetUserVehicles 获取用户车辆列表
func (h *VehicleHandler) GetUserVehicles(c *gin.Context) {
	userID, exists := c.Get("userID")
	if !exists {
		Unauthorized(c, "未认证")
		return
	}

	page, _ := strconv.Atoi(c.DefaultQuery("page", "1"))
	pageSize, _ := strconv.Atoi(c.DefaultQuery("page_size", "10"))

	vehicles, total, err := h.vehicleService.GetUserVehicles(c.Request.Context(), userID.(uint), page, pageSize)
	if err != nil {
		InternalError(c, err.Error())
		return
	}

	SuccessList(c, map[string]interface{}{
		"vehicles": vehicles,
		"page":     page,
		"pageSize": pageSize,
	}, total)
}

// GetVehicle 获取车辆详情
func (h *VehicleHandler) GetVehicle(c *gin.Context) {
	userID, exists := c.Get("userID")
	if !exists {
		Unauthorized(c, "未认证")
		return
	}

	vehicleID, err := strconv.ParseUint(c.Param("id"), 10, 32)
	if err != nil {
		BadRequest(c, "无效的车辆ID")
		return
	}

	vehicle, err := h.vehicleService.GetVehicle(c.Request.Context(), uint(vehicleID), userID.(uint))
	if err != nil {
		NotFound(c, "车辆不存在")
		return
	}

	SuccessMsg(c, "获取成功", vehicle)
}

// GetVehicleStatus 获取车辆实时状态
func (h *VehicleHandler) GetVehicleStatus(c *gin.Context) {
	userID, exists := c.Get("userID")
	if !exists {
		Unauthorized(c, "未认证")
		return
	}

	vehicleID, err := strconv.ParseUint(c.Param("id"), 10, 32)
	if err != nil {
		BadRequest(c, "无效的车辆ID")
		return
	}

	status, err := h.vehicleService.GetVehicleStatus(c.Request.Context(), uint(vehicleID), userID.(uint))
	if err != nil {
		InternalError(c, err.Error())
		return
	}

	SuccessMsg(c, "获取成功", status)
}

// SendCommand 发送车辆控制命令
func (h *VehicleHandler) SendCommand(c *gin.Context) {
	userID, exists := c.Get("userID")
	if !exists {
		Unauthorized(c, "未认证")
		return
	}

	vehicleID, err := strconv.ParseUint(c.Param("id"), 10, 32)
	if err != nil {
		BadRequest(c, "无效的车辆ID")
		return
	}

	var req models.VehicleCommandRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		BadRequest(c, "请求参数错误", err.Error())
		return
	}

	status, err := h.vehicleService.SendCommand(c.Request.Context(), uint(vehicleID), userID.(uint), &req)
	if err != nil {
		InternalError(c, err.Error())
		return
	}

	c.JSON(202, gin.H{
		"code":    202,
		"message": "命令已发送",
		"data": gin.H{
			"command_id": status.CommandID,
			"status":     status.Status,
		},
	})
}

// UpdateLocation 更新车辆位置
func (h *VehicleHandler) UpdateLocation(c *gin.Context) {
	userID, exists := c.Get("userID")
	if !exists {
		Unauthorized(c, "未认证")
		return
	}

	vehicleID, err := strconv.ParseUint(c.Param("id"), 10, 32)
	if err != nil {
		BadRequest(c, "无效的车辆ID")
		return
	}

	var req models.VehicleLocationRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		BadRequest(c, "请求参数错误", err.Error())
		return
	}

	if err := h.vehicleService.UpdateLocation(c.Request.Context(), uint(vehicleID), userID.(uint), &req); err != nil {
		InternalError(c, err.Error())
		return
	}

	SuccessMsg(c, "位置更新成功", nil)
}

// GetCommandStatus 获取命令执行状态
func (h *VehicleHandler) GetCommandStatus(c *gin.Context) {
	commandID := c.Param("command_id")

	status, err := h.vehicleService.GetCommandStatus(c.Request.Context(), commandID)
	if err != nil {
		NotFound(c, "命令不存在")
		return
	}

	SuccessMsg(c, "获取成功", status)
}

// Heartbeat 车辆心跳上报
func (h *VehicleHandler) Heartbeat(c *gin.Context) {
	vehicleID, err := strconv.ParseUint(c.Param("id"), 10, 32)
	if err != nil {
		BadRequest(c, "无效的车辆ID")
		return
	}

	var data map[string]interface{}
	if err := c.ShouldBindJSON(&data); err != nil {
		BadRequest(c, "请求参数错误", err.Error())
		return
	}

	if err := h.vehicleService.Heartbeat(c.Request.Context(), uint(vehicleID), data); err != nil {
		InternalError(c, err.Error())
		return
	}

	SuccessMsg(c, "心跳上报成功", nil)
}
