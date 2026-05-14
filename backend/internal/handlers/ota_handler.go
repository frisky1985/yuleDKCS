package handlers

import (
	"net/http"
	"strconv"

	"github.com/frisky1985/yuleDKCS/backend/internal/models"
	"github.com/frisky1985/yuleDKCS/backend/internal/services"
	"github.com/gin-gonic/gin"
)

// OTAHandler OTA处理器
type OTAHandler struct {
	otaService services.OTAService
}

// NewOTAHandler 创建OTA处理器
func NewOTAHandler(otaService services.OTAService) *OTAHandler {
	return &OTAHandler{
		otaService: otaService,
	}
}

// RegisterRoutes 注册路由
func (h *OTAHandler) RegisterRoutes(router *gin.RouterGroup) {
	ota := router.Group("/ota")
	{
		// 固件管理
		ota.POST("/firmwares", h.CreateFirmware)
		ota.GET("/firmwares", h.ListFirmwares)
		ota.GET("/firmwares/:id", h.GetFirmware)
		ota.PUT("/firmwares/:id", h.UpdateFirmware)
		ota.DELETE("/firmwares/:id", h.DeleteFirmware)
		ota.PUT("/firmwares/:id/deactivate", h.DeactivateFirmware)

		// 更新检查
		ota.POST("/check", h.CheckUpdate)
		ota.GET("/updates", h.CheckUpdateQuery)

		// 下载
		ota.POST("/firmwares/:id/download", h.DownloadFirmware)
	}

	// 车辆OTA状态
	vehicles := router.Group("/vehicles")
	{
		vehicles.GET("/:id/ota/status", h.GetVehicleOTAStatus)
		vehicles.PUT("/:id/ota/status", h.UpdateOTAStatus)
		vehicles.POST("/:id/ota/start", h.StartOTA)
		vehicles.GET("/:id/ota/history", h.GetOTAHistory)
	}
}

// CreateFirmware 创建固件
// POST /api/v1/ota/firmwares
func (h *OTAHandler) CreateFirmware(c *gin.Context) {
	var firmware models.Firmware
	if err := c.ShouldBindJSON(&firmware); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"code":    400,
			"message": "请求参数错误",
			"error":   err.Error(),
		})
		return
	}

	created, err := h.otaService.CreateFirmware(c.Request.Context(), &firmware)
	if err != nil {
		switch err {
		case services.ErrInvalidFirmwareType:
			c.JSON(http.StatusBadRequest, gin.H{
				"code":    400,
				"message": "无效的固件类型",
			})
		case services.ErrVersionExists:
			c.JSON(http.StatusConflict, gin.H{
				"code":    409,
				"message": "该版本已存在",
			})
		default:
			c.JSON(http.StatusInternalServerError, gin.H{
				"code":    500,
				"message": "创建固件失败",
				"error":   err.Error(),
			})
		}
		return
	}

	c.JSON(http.StatusCreated, gin.H{
		"code":    201,
		"message": "固件创建成功",
		"data":    firmwareToResponse(created),
	})
}

// ListFirmwares 列出固件
// GET /api/v1/ota/firmwares
func (h *OTAHandler) ListFirmwares(c *gin.Context) {
	firmwareType := models.FirmwareType(c.Query("type"))
	page, _ := strconv.Atoi(c.DefaultQuery("page", "1"))
	pageSize, _ := strconv.Atoi(c.DefaultQuery("page_size", "10"))

	firmwares, total, err := h.otaService.ListFirmwares(c.Request.Context(), firmwareType, page, pageSize)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"code":    500,
			"message": "获取固件列表失败",
			"error":   err.Error(),
		})
		return
	}

	response := make([]models.FirmwareResponse, len(firmwares))
	for i, f := range firmwares {
		response[i] = firmwareToResponse(&f)
	}

	c.JSON(http.StatusOK, gin.H{
		"code":    200,
		"message": "获取成功",
		"data": gin.H{
			"list":      response,
			"total":     total,
			"page":      page,
			"page_size": pageSize,
		},
	})
}

// GetFirmware 获取固件详情
// GET /api/v1/ota/firmwares/:id
func (h *OTAHandler) GetFirmware(c *gin.Context) {
	id, err := strconv.ParseUint(c.Param("id"), 10, 32)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"code":    400,
			"message": "无效的固件ID",
		})
		return
	}

	firmware, err := h.otaService.GetFirmwareByID(c.Request.Context(), uint(id))
	if err != nil {
		if err == services.ErrFirmwareNotFound {
			c.JSON(http.StatusNotFound, gin.H{
				"code":    404,
				"message": "固件不存在",
			})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{
			"code":    500,
			"message": "获取固件失败",
			"error":   err.Error(),
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"code":    200,
		"message": "获取成功",
		"data":    firmwareToResponse(firmware),
	})
}

// UpdateFirmware 更新固件
// PUT /api/v1/ota/firmwares/:id
func (h *OTAHandler) UpdateFirmware(c *gin.Context) {
	id, err := strconv.ParseUint(c.Param("id"), 10, 32)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"code":    400,
			"message": "无效的固件ID",
		})
		return
	}

	var updates map[string]interface{}
	if err := c.ShouldBindJSON(&updates); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"code":    400,
			"message": "请求参数错误",
			"error":   err.Error(),
		})
		return
	}

	firmware, err := h.otaService.UpdateFirmware(c.Request.Context(), uint(id), updates)
	if err != nil {
		if err == services.ErrFirmwareNotFound {
			c.JSON(http.StatusNotFound, gin.H{
				"code":    404,
				"message": "固件不存在",
			})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{
			"code":    500,
			"message": "更新固件失败",
			"error":   err.Error(),
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"code":    200,
		"message": "固件更新成功",
		"data":    firmwareToResponse(firmware),
	})
}

// DeleteFirmware 删除固件
// DELETE /api/v1/ota/firmwares/:id
func (h *OTAHandler) DeleteFirmware(c *gin.Context) {
	id, err := strconv.ParseUint(c.Param("id"), 10, 32)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"code":    400,
			"message": "无效的固件ID",
		})
		return
	}

	if err := h.otaService.DeleteFirmware(c.Request.Context(), uint(id)); err != nil {
		if err == services.ErrFirmwareNotFound {
			c.JSON(http.StatusNotFound, gin.H{
				"code":    404,
				"message": "固件不存在",
			})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{
			"code":    500,
			"message": "删除固件失败",
			"error":   err.Error(),
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"code":    200,
		"message": "固件已删除",
	})
}

// DeactivateFirmware 停用固件
// PUT /api/v1/ota/firmwares/:id/deactivate
func (h *OTAHandler) DeactivateFirmware(c *gin.Context) {
	id, err := strconv.ParseUint(c.Param("id"), 10, 32)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"code":    400,
			"message": "无效的固件ID",
		})
		return
	}

	if err := h.otaService.DeactivateFirmware(c.Request.Context(), uint(id)); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"code":    500,
			"message": "停用固件失败",
			"error":   err.Error(),
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"code":    200,
		"message": "固件已停用",
	})
}

// CheckUpdate 检查更新 (POST)
// POST /api/v1/ota/check
func (h *OTAHandler) CheckUpdate(c *gin.Context) {
	var req models.CheckUpdateRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"code":    400,
			"message": "请求参数错误",
			"error":   err.Error(),
		})
		return
	}

	result, err := h.otaService.CheckUpdate(c.Request.Context(), &req)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"code":    500,
			"message": "检查更新失败",
			"error":   err.Error(),
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"code":    200,
		"message": "检查完成",
		"data":    result,
	})
}

// CheckUpdateQuery 检查更新 (GET)
// GET /api/v1/ota/updates
func (h *OTAHandler) CheckUpdateQuery(c *gin.Context) {
	req := models.CheckUpdateRequest{
		CurrentVersion:  c.Query("current_version"),
		HardwareVersion: c.Query("hardware_version"),
		FirmwareType:    c.Query("firmware_type"),
	}

	// 从查询参数获取vehicle_id
	if vehicleID := c.Query("vehicle_id"); vehicleID != "" {
		if id, err := strconv.ParseUint(vehicleID, 10, 32); err == nil {
			req.VehicleID = uint(id)
		}
	}

	if req.CurrentVersion == "" || req.HardwareVersion == "" || req.FirmwareType == "" {
		c.JSON(http.StatusBadRequest, gin.H{
			"code":    400,
			"message": "缺少必要参数: current_version, hardware_version, firmware_type",
		})
		return
	}

	result, err := h.otaService.CheckUpdate(c.Request.Context(), &req)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"code":    500,
			"message": "检查更新失败",
			"error":   err.Error(),
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"code":    200,
		"message": "检查完成",
		"data":    result,
	})
}

// DownloadFirmware 下载固件
// POST /api/v1/ota/firmwares/:id/download
func (h *OTAHandler) DownloadFirmware(c *gin.Context) {
	firmwareID, err := strconv.ParseUint(c.Param("id"), 10, 32)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"code":    400,
			"message": "无效的固件ID",
		})
		return
	}

	var req models.OTADownloadRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		// 允许空请求体，使用默认值
		req = models.OTADownloadRequest{}
	}

	// 获取客户端IP
	if req.IPAddress == "" {
		req.IPAddress = c.ClientIP()
	}

	url, err := h.otaService.DownloadFirmware(c.Request.Context(), uint(firmwareID), req.VehicleID, req.IPAddress)
	if err != nil {
		if err == services.ErrFirmwareNotFound {
			c.JSON(http.StatusNotFound, gin.H{
				"code":    404,
				"message": "固件不存在",
			})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{
			"code":    500,
			"message": "下载固件失败",
			"error":   err.Error(),
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"code":    200,
		"message": "获取下载链接成功",
		"data": gin.H{
			"download_url": url,
		},
	})
}

// GetVehicleOTAStatus 获取车辆OTA状态
// GET /api/v1/vehicles/:id/ota/status
func (h *OTAHandler) GetVehicleOTAStatus(c *gin.Context) {
	vehicleID, err := strconv.ParseUint(c.Param("id"), 10, 32)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"code":    400,
			"message": "无效的车辆ID",
		})
		return
	}

	status, err := h.otaService.GetVehicleOTAStatus(c.Request.Context(), uint(vehicleID))
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"code":    500,
			"message": "获取OTA状态失败",
			"error":   err.Error(),
		})
		return
	}

	if status == nil {
		c.JSON(http.StatusOK, gin.H{
			"code":    200,
			"message": "没有OTA记录",
			"data":    nil,
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"code":    200,
		"message": "获取成功",
		"data":    otaStatusToResponse(status),
	})
}

// UpdateOTAStatus 更新车辆OTA状态
// PUT /api/v1/vehicles/:id/ota/status
func (h *OTAHandler) UpdateOTAStatus(c *gin.Context) {
	vehicleID, err := strconv.ParseUint(c.Param("id"), 10, 32)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"code":    400,
			"message": "无效的车辆ID",
		})
		return
	}

	var req models.UpdateOTAStatusRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"code":    400,
			"message": "请求参数错误",
			"error":   err.Error(),
		})
		return
	}

	status, err := h.otaService.UpdateOTAStatus(c.Request.Context(), uint(vehicleID), &req)
	if err != nil {
		switch err {
		case services.ErrInvalidStatus:
			c.JSON(http.StatusBadRequest, gin.H{
				"code":    400,
				"message": "无效的OTA状态",
			})
		default:
			c.JSON(http.StatusInternalServerError, gin.H{
				"code":    500,
				"message": "更新OTA状态失败",
				"error":   err.Error(),
			})
		}
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"code":    200,
		"message": "OTA状态更新成功",
		"data":    otaStatusToResponse(status),
	})
}

// StartOTA 开始OTA更新
// POST /api/v1/vehicles/:id/ota/start
func (h *OTAHandler) StartOTA(c *gin.Context) {
	vehicleID, err := strconv.ParseUint(c.Param("id"), 10, 32)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"code":    400,
			"message": "无效的车辆ID",
		})
		return
	}

	var req struct {
		TargetVersion string `json:"target_version" binding:"required"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"code":    400,
			"message": "请求参数错误",
			"error":   err.Error(),
		})
		return
	}

	status := &models.VehicleOTAStatus{
		VehicleID:     uint(vehicleID),
		TargetVersion: req.TargetVersion,
	}

	created, err := h.otaService.CreateOTAStatus(c.Request.Context(), status)
	if err != nil {
		switch err {
		case services.ErrOTAInProgress:
			c.JSON(http.StatusConflict, gin.H{
				"code":    409,
				"message": "已有OTA更新正在进行",
			})
		default:
			c.JSON(http.StatusInternalServerError, gin.H{
				"code":    500,
				"message": "启动OTA更新失败",
				"error":   err.Error(),
			})
		}
		return
	}

	c.JSON(http.StatusCreated, gin.H{
		"code":    201,
		"message": "OTA更新已启动",
		"data":    otaStatusToResponse(created),
	})
}

// GetOTAHistory 获取OTA历史
// GET /api/v1/vehicles/:id/ota/history
func (h *OTAHandler) GetOTAHistory(c *gin.Context) {
	vehicleID, err := strconv.ParseUint(c.Param("id"), 10, 32)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"code":    400,
			"message": "无效的车辆ID",
		})
		return
	}

	page, _ := strconv.Atoi(c.DefaultQuery("page", "1"))
	pageSize, _ := strconv.Atoi(c.DefaultQuery("page_size", "10"))

	history, total, err := h.otaService.ListVehicleOTAHistory(c.Request.Context(), uint(vehicleID), page, pageSize)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"code":    500,
			"message": "获取OTA历史失败",
			"error":   err.Error(),
		})
		return
	}

	response := make([]models.VehicleOTAStatusResponse, len(history))
	for i, h := range history {
		response[i] = otaStatusToResponse(&h)
	}

	c.JSON(http.StatusOK, gin.H{
		"code":    200,
		"message": "获取成功",
		"data": gin.H{
			"list":      response,
			"total":     total,
			"page":      page,
			"page_size": pageSize,
		},
	})
}

// firmwareToResponse 转换固件为响应格式
func firmwareToResponse(f *models.Firmware) models.FirmwareResponse {
	return models.FirmwareResponse{
		ID:                 f.ID,
		Version:            f.Version,
		Type:               string(f.Type),
		Size:               f.Size,
		Checksum:           f.Checksum,
		ReleaseNotes:       f.ReleaseNotes,
		MinHardwareVersion: f.MinHardwareVersion,
		MaxHardwareVersion: f.MaxHardwareVersion,
		ReleaseDate:        f.ReleaseDate,
		CreatedAt:          f.CreatedAt,
	}
}

// otaStatusToResponse 转换OTA状态为响应格式
func otaStatusToResponse(s *models.VehicleOTAStatus) models.VehicleOTAStatusResponse {
	return models.VehicleOTAStatusResponse{
		ID:             strconv.Itoa(int(s.ID)),
		VehicleID:      s.VehicleID,
		CurrentVersion: s.CurrentVersion,
		TargetVersion:  s.TargetVersion,
		Status:         string(s.Status),
		Progress:       s.Progress,
		ErrorMessage:   s.ErrorMessage,
		StartedAt:      s.StartedAt,
		CompletedAt:    s.CompletedAt,
		UpdatedAt:      s.UpdatedAt,
	}
}
