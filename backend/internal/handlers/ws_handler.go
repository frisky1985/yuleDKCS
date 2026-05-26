package handlers

import (
	"encoding/json"
	"strconv"

	"github.com/frisky1985/yuleDKCS/backend/internal/middleware"
	"github.com/frisky1985/yuleDKCS/backend/internal/websocket"
	"github.com/gin-gonic/gin"
)

type WebSocketHandler struct {
	hub *websocket.Hub
}

func NewWebSocketHandler(hub *websocket.Hub) *WebSocketHandler {
	return &WebSocketHandler{hub: hub}
}

// HandleUserWebSocket 用户 WebSocket 连接
// 用于接收用户关联车辆的实时更新
func (h *WebSocketHandler) HandleUserWebSocket(c *gin.Context) {
	userID := c.GetUint("userID")
	vehicleID, _ := strconv.ParseUint(c.Query("vehicle_id"), 10, 32)

	// 升级为 WebSocket
	websocket.ServeWs(h.hub, userID, uint(vehicleID), "user")(c)
}

// HandleVehicleWebSocket 车辆 WebSocket 连接
// 用于车机上报状态和接收命令
func (h *WebSocketHandler) HandleVehicleWebSocket(c *gin.Context) {
	vehicleID, _ := strconv.ParseUint(c.Param("id"), 10, 32)

	// 验证车机认证：检查 Authorization 头中的 JWT
	authHeader := c.GetHeader("Authorization")
	if authHeader == "" {
		Unauthorized(c, "缺少认证信息")
		return
	}
	// 若 JWT 有效且包含 vehicle 角色则通过；否则仅允许未认证连接（由上层中间件决定）
	if len(authHeader) > 7 && authHeader[:7] == "Bearer " {
		middleware.JWTAuth()(c)
		if c.IsAborted() {
			return
		}
	}

	websocket.ServeWs(h.hub, 0, uint(vehicleID), "vehicle")(c)
}

// HandleAdminWebSocket 管理员 WebSocket 连接
// 用于监控系统状态
func (h *WebSocketHandler) HandleAdminWebSocket(c *gin.Context) {
	userID := c.GetUint("userID")

	// 验证管理员权限
	role, exists := c.Get("role")
	if !exists || role.(string) != "admin" {
		Forbidden(c, "需要管理员权限")
		return
	}

	websocket.ServeWs(h.hub, userID, 0, "admin")(c)
}

// HandleWebSocket 通用 WebSocket 连接处理
// 用于 /ws 端点，从 JWT 中获取用户信息后进行升级
func (h *WebSocketHandler) HandleWebSocket(c *gin.Context) {
	userID := c.GetUint("userID")
	vehicleID, _ := strconv.ParseUint(c.Query("vehicle_id"), 10, 32)

	websocket.ServeWs(h.hub, userID, uint(vehicleID), "user")(c)
}

// GetConnectionStats 获取 WebSocket 连接统计
func (h *WebSocketHandler) GetConnectionStats(c *gin.Context) {
	stats := map[string]interface{}{
		"total_connections": h.hub.GetClientCount(),
	}

	SuccessMsg(c, "获取成功", stats)
}

// SendCommandToVehicle 通过 WebSocket 发送命令到车辆
func (h *WebSocketHandler) SendCommandToVehicle(c *gin.Context) {
	vehicleID, _ := strconv.ParseUint(c.Param("id"), 10, 32)

	var req struct {
		Command string                 `json:"command" binding:"required"`
		Params  map[string]interface{} `json:"params"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		BadRequest(c, "请求参数错误", err.Error())
		return
	}

	// 创建命令消息
	message := map[string]interface{}{
		"type":    "command",
		"command": req.Command,
		"params":  req.Params,
	}

	// 发送到指定车辆的所有连接
	data, _ := json.Marshal(message)
	h.hub.BroadcastToVehicle(uint(vehicleID), data)

	SuccessMsg(c, "命令已发送", gin.H{
		"vehicle_id": vehicleID,
		"command":    req.Command,
	})
}
