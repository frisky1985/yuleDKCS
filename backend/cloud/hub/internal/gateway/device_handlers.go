package gateway

import (
	"fmt"
	"net/http"

	"github.com/gin-gonic/gin"
	"go.uber.org/zap"

	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/service"
)

// ── 5. Multi-Device Management ──

// registerDevice POST /api/v1/devices — 注册设备并上报能力
func (g *RESTGateway) registerDevice(c *gin.Context) {
	userID, _, err := g.extractAuth(c)
	if err != nil {
		c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{"error": "AUTH_FAILED", "message": err.Error()})
		return
	}

	var req struct {
		Platform      string `json:"platform"`
		Model         string `json:"model"`
		OSVersion     string `json:"os_version"`
		AppVersion    string `json:"app_version"`
		BLE           bool   `json:"ble"`
		UWB           bool   `json:"uwb"`
		NFC           bool   `json:"nfc"`
		SecureElement bool   `json:"secure_element"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "BAD_REQUEST", "message": "invalid request body"})
		return
	}

	caps := service.DeviceCapabilities{
		BLE:           req.BLE,
		UWB:           req.UWB,
		NFC:           req.NFC,
		SecureElement: req.SecureElement,
		Platform:      req.Platform,
		Model:         req.Model,
		OSVersion:     req.OSVersion,
		AppVersion:    req.AppVersion,
	}

	device, err := g.deviceService.RegisterDevice(c.Request.Context(), userID, caps)
	if err != nil {
		g.logger.Error("registerDevice failed", zap.Error(err))
		c.JSON(http.StatusConflict, gin.H{"error": "REGISTER_FAILED", "message": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"device_id":     device.DeviceID,
		"platform":      device.Capabilities.Platform,
		"model":         device.Capabilities.Model,
		"ble":           device.Capabilities.BLE,
		"uwb":           device.Capabilities.UWB,
		"nfc":           device.Capabilities.NFC,
		"max_devices":   5,
	})
}

// listDevices GET /api/v1/devices — 列出我所有已注册的设备
func (g *RESTGateway) listDevices(c *gin.Context) {
	userID, _, err := g.extractAuth(c)
	if err != nil {
		c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{"error": "AUTH_FAILED", "message": err.Error()})
		return
	}

	devices, err := g.deviceService.ListUserDevices(c.Request.Context(), userID)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "LIST_FAILED", "message": err.Error()})
		return
	}

	result := make([]gin.H, 0, len(devices))
	for _, d := range devices {
		result = append(result, gin.H{
			"device_id":      d.DeviceID,
			"platform":       d.Capabilities.Platform,
			"model":          d.Capabilities.Model,
			"os_version":     d.Capabilities.OSVersion,
			"ble":            d.Capabilities.BLE,
			"uwb":            d.Capabilities.UWB,
			"nfc":            d.Capabilities.NFC,
			"secure_element": d.Capabilities.SecureElement,
			"last_seen":      d.LastSeen,
		})
	}

	c.JSON(http.StatusOK, gin.H{"devices": result})
}

// getDevice GET /api/v1/devices/:deviceId — 查看设备详情
func (g *RESTGateway) getDevice(c *gin.Context) {
	userID, _, err := g.extractAuth(c)
	if err != nil {
		c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{"error": "AUTH_FAILED", "message": err.Error()})
		return
	}

	deviceID := c.Param("deviceId")
	device, err := g.deviceService.GetDevice(c.Request.Context(), deviceID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "NOT_FOUND", "message": "device not found"})
		return
	}
	if device.UserID != userID {
		c.JSON(http.StatusForbidden, gin.H{"error": "FORBIDDEN", "message": "device does not belong to you"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"device_id":      device.DeviceID,
		"platform":       device.Capabilities.Platform,
		"model":          device.Capabilities.Model,
		"os_version":     device.Capabilities.OSVersion,
		"app_version":    device.Capabilities.AppVersion,
		"ble":            device.Capabilities.BLE,
		"uwb":            device.Capabilities.UWB,
		"nfc":            device.Capabilities.NFC,
		"secure_element": device.Capabilities.SecureElement,
		"last_seen":      device.LastSeen,
		"registered_at":  device.RegisteredAt,
	})
}

// provisionDevice POST /api/v1/devices/:deviceId/provision — 给指定设备配钥匙
// 返回设备已有钥匙（不重复创建）
func (g *RESTGateway) provisionDevice(c *gin.Context) {
	userID, _, err := g.extractAuth(c)
	if err != nil {
		c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{"error": "AUTH_FAILED", "message": err.Error()})
		return
	}

	deviceID := c.Param("deviceId")

	var req struct {
		VehicleID string `json:"vehicle_id" binding:"required"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "BAD_REQUEST", "message": "vehicle_id is required"})
		return
	}

	// 设备能力检测 → 按能力配钥
	device, err := g.deviceService.GetDevice(c.Request.Context(), deviceID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "NOT_FOUND", "message": "device not found"})
		return
	}
	if device.UserID != userID {
		c.JSON(http.StatusForbidden, gin.H{"error": "FORBIDDEN", "message": "device does not belong to you"})
		return
	}

	// 支持的协议协商（基于设备能力）
	protocols := make([]string, 0)
	if device.Capabilities.NFC { protocols = append(protocols, "NFC") }
	if device.Capabilities.BLE { protocols = append(protocols, "BLE") }
	if device.Capabilities.UWB { protocols = append(protocols, "UWB") }

	// 创建钥匙绑定
	binding, err := g.deviceService.ProvisionKey(c.Request.Context(), userID, deviceID, req.VehicleID, protocols)
	if err != nil {
		g.logger.Error("provisionDevice failed", zap.Error(err))
		c.JSON(http.StatusInternalServerError, gin.H{"error": "PROVISION_FAILED", "message": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"key_id":     binding.KeyID,
		"device_id":  binding.DeviceID,
		"vehicle_id": binding.VehicleID,
		"status":     binding.Status,
		"bound_at":   binding.BoundAt,
		"note":       "密钥已下发至设备，请打开App确认",
	})
}

// revokeDevice POST /api/v1/devices/:deviceId/revoke — 吊销设备所有钥匙
func (g *RESTGateway) revokeDevice(c *gin.Context) {
	userID, _, err := g.extractAuth(c)
	if err != nil {
		c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{"error": "AUTH_FAILED", "message": err.Error()})
		return
	}

	deviceID := c.Param("deviceId")
	revoked, err := g.deviceService.RevokeDeviceKeys(c.Request.Context(), userID, deviceID)
	if err != nil {
		g.logger.Error("revokeDevice failed", zap.Error(err))
		c.JSON(http.StatusInternalServerError, gin.H{"error": "REVOKE_FAILED", "message": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"device_id":    deviceID,
		"keys_revoked": len(revoked),
		"status":       "revoked",
	})
}

// deleteDevice DELETE /api/v1/devices/:deviceId — 删除设备
func (g *RESTGateway) deleteDevice(c *gin.Context) {
	userID, _, err := g.extractAuth(c)
	if err != nil {
		c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{"error": "AUTH_FAILED", "message": err.Error()})
		return
	}

	deviceID := c.Param("deviceId")
	if err := g.deviceService.DeleteDevice(c.Request.Context(), userID, deviceID); err != nil {
		g.logger.Error("deleteDevice failed", zap.Error(err))
		c.JSON(http.StatusInternalServerError, gin.H{"error": "DELETE_FAILED", "message": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"device_id": deviceID, "status": "deleted"})
}

// extractAuth 从 gin.Context 提取认证用户信息
func (g *RESTGateway) extractAuth(c *gin.Context) (string, string, error) {
	userID, exists := c.Get("user_id")
	if !exists {
		return "", "", fmt.Errorf("unauthenticated")
	}
	userIDStr, ok := userID.(string)
	if !ok {
		return "", "", fmt.Errorf("invalid user_id type")
	}
	role, _ := c.Get("user_role")
	roleStr, _ := role.(string)
	return userIDStr, roleStr, nil
}
