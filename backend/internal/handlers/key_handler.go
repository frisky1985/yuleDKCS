package handlers

import (
	"strconv"

	"github.com/frisky1985/yuleDKCS/backend/internal/models"
	"github.com/frisky1985/yuleDKCS/backend/internal/services"
	"github.com/gin-gonic/gin"
)

// KeyHandler 钥匙处理器
type KeyHandler struct {
	keyService services.KeyService
}

// NewKeyHandler 创建钥匙处理器
func NewKeyHandler(keyService services.KeyService) *KeyHandler {
	return &KeyHandler{
		keyService: keyService,
	}
}

// RegisterRoutes 注册路由
func (h *KeyHandler) RegisterRoutes(router *gin.RouterGroup) {
	keys := router.Group("/keys")
	{
		keys.POST("/issue", h.IssueKey)
		keys.GET("", h.GetUserKeys)
		keys.GET("/:id", h.GetKeyDetail)
		keys.POST("/:id/activate", h.ActivateKey)
		keys.POST("/:id/deactivate", h.DeactivateKey)
		keys.POST("/:id/share", h.ShareKey)
		keys.DELETE("/:id", h.RevokeKey)
		keys.PUT("/:id/permissions", h.UpdatePermissions)
		keys.GET("/:id/logs", h.GetKeyLogs)
		keys.GET("/shared/list", h.GetSharedKeys)
		keys.GET("/:id/shares", h.GetKeyShares)
		keys.DELETE("/shares/:share_id", h.RevokeShare)
	}
}

// IssueKey 发行新钥匙
// POST /api/v1/keys/issue
func (h *KeyHandler) IssueKey(c *gin.Context) {
	var req models.IssueKeyRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		BadRequest(c, "请求参数错误", err.Error())
		return
	}

	// 从上下文中获取用户ID
	userID, exists := c.Get("userID")
	if !exists {
		Unauthorized(c, "未认证")
		return
	}

	key, err := h.keyService.IssueKey(c.Request.Context(), userID.(uint), &req)
	if err != nil {
		switch err {
		case services.ErrInvalidKeyType:
			BadRequest(c, "无效的钥匙类型")
		case services.ErrUnauthorized:
			Forbidden(c, "无权为此车辆发行钥匙")
		default:
			InternalError(c, "发行钥匙失败", err.Error())
		}
		return
	}

	SuccessCreated(c, keyToResponse(key))
}

// GetUserKeys 获取用户钥匙列表
// GET /api/v1/keys
func (h *KeyHandler) GetUserKeys(c *gin.Context) {
	userID, exists := c.Get("userID")
	if !exists {
		Unauthorized(c, "未认证")
		return
	}

	// 分页参数
	page, _ := strconv.Atoi(c.DefaultQuery("page", "1"))
	pageSize, _ := strconv.Atoi(c.DefaultQuery("page_size", "10"))

	keys, total, err := h.keyService.GetUserKeys(c.Request.Context(), userID.(uint), page, pageSize)
	if err != nil {
		InternalError(c, "获取钥匙列表失败", err.Error())
		return
	}

	response := make([]models.KeyResponse, len(keys))
	for i, key := range keys {
		response[i] = keyToResponse(&key)
	}

	SuccessList(c, map[string]interface{}{
		"list":      response,
		"page":      page,
		"page_size": pageSize,
	}, total)
}

// GetKeyDetail 获取钥匙详情
// GET /api/v1/keys/:id
func (h *KeyHandler) GetKeyDetail(c *gin.Context) {
	userID, exists := c.Get("userID")
	if !exists {
		Unauthorized(c, "未认证")
		return
	}

	keyID, err := strconv.ParseUint(c.Param("id"), 10, 32)
	if err != nil {
		BadRequest(c, "无效的钥匙ID")
		return
	}

	key, err := h.keyService.GetKey(c.Request.Context(), uint(keyID), userID.(uint))
	if err != nil {
		switch err {
		case services.ErrKeyNotFound:
			NotFound(c, "钥匙不存在")
		case services.ErrUnauthorized:
			Forbidden(c, "无权访问此钥匙")
		default:
			InternalError(c, "获取钥匙详情失败", err.Error())
		}
		return
	}

	SuccessMsg(c, "获取成功", keyToResponse(key))
}

// ShareKey 分享钥匙
// POST /api/v1/keys/:id/share
func (h *KeyHandler) ShareKey(c *gin.Context) {
	userID, exists := c.Get("userID")
	if !exists {
		Unauthorized(c, "未认证")
		return
	}

	keyID, err := strconv.ParseUint(c.Param("id"), 10, 32)
	if err != nil {
		BadRequest(c, "无效的钥匙ID")
		return
	}

	var req models.ShareKeyRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		BadRequest(c, "请求参数错误", err.Error())
		return
	}

	share, err := h.keyService.ShareKey(c.Request.Context(), uint(keyID), userID.(uint), &req)
	if err != nil {
		switch err {
		case services.ErrKeyNotFound:
			NotFound(c, "钥匙不存在")
		case services.ErrUnauthorized:
			Forbidden(c, "无权分享此钥匙")
		case services.ErrCannotShareToSelf:
			BadRequest(c, "不能分享给自己")
		case services.ErrInvalidPermissions:
			BadRequest(c, "分享权限超过原钥匙权限")
		default:
			InternalError(c, "分享钥匙失败", err.Error())
		}
		return
	}

	SuccessCreated(c, share)
}

// RevokeKey 撤销钥匙
// DELETE /api/v1/keys/:id
func (h *KeyHandler) RevokeKey(c *gin.Context) {
	userID, exists := c.Get("userID")
	if !exists {
		Unauthorized(c, "未认证")
		return
	}

	keyID, err := strconv.ParseUint(c.Param("id"), 10, 32)
	if err != nil {
		BadRequest(c, "无效的钥匙ID")
		return
	}

	if err := h.keyService.RevokeKey(c.Request.Context(), uint(keyID), userID.(uint)); err != nil {
		switch err {
		case services.ErrKeyNotFound:
			NotFound(c, "钥匙不存在")
		case services.ErrUnauthorized:
			Forbidden(c, "无权撤销此钥匙")
		default:
			InternalError(c, "撤销钥匙失败", err.Error())
		}
		return
	}

	SuccessMsg(c, "钥匙已撤销", nil)
}

// UpdatePermissions 更新钥匙权限
// PUT /api/v1/keys/:id/permissions
func (h *KeyHandler) UpdatePermissions(c *gin.Context) {
	userID, exists := c.Get("userID")
	if !exists {
		Unauthorized(c, "未认证")
		return
	}

	keyID, err := strconv.ParseUint(c.Param("id"), 10, 32)
	if err != nil {
		BadRequest(c, "无效的钥匙ID")
		return
	}

	var req models.UpdatePermissionsRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		BadRequest(c, "请求参数错误", err.Error())
		return
	}

	if err := h.keyService.UpdateKeyPermissions(c.Request.Context(), uint(keyID), userID.(uint), req.Permissions); err != nil {
		switch err {
		case services.ErrKeyNotFound:
			NotFound(c, "钥匙不存在")
		case services.ErrUnauthorized:
			Forbidden(c, "无权修改此钥匙权限")
		default:
			InternalError(c, "更新权限失败", err.Error())
		}
		return
	}

	SuccessMsg(c, "权限更新成功", nil)
}

// GetSharedKeys 获取用户收到的分享钥匙
// GET /api/v1/keys/shared/list
func (h *KeyHandler) GetSharedKeys(c *gin.Context) {
	userID, exists := c.Get("userID")
	if !exists {
		Unauthorized(c, "未认证")
		return
	}

	shares, err := h.keyService.GetSharedKeys(c.Request.Context(), userID.(uint))
	if err != nil {
		InternalError(c, "获取分享列表失败", err.Error())
		return
	}

	SuccessMsg(c, "获取成功", shares)
}

// GetKeyShares 获取钥匙的分享列表
// GET /api/v1/keys/:id/shares
func (h *KeyHandler) GetKeyShares(c *gin.Context) {
	userID, exists := c.Get("userID")
	if !exists {
		Unauthorized(c, "未认证")
		return
	}

	keyID, err := strconv.ParseUint(c.Param("id"), 10, 32)
	if err != nil {
		BadRequest(c, "无效的钥匙ID")
		return
	}

	shares, err := h.keyService.GetKeyShares(c.Request.Context(), uint(keyID), userID.(uint))
	if err != nil {
		switch err {
		case services.ErrKeyNotFound:
			NotFound(c, "钥匙不存在")
		case services.ErrUnauthorized:
			Forbidden(c, "无权查看此钥匙的分享")
		default:
			InternalError(c, "获取分享列表失败", err.Error())
		}
		return
	}

	SuccessMsg(c, "获取成功", shares)
}

// RevokeShare 撤销分享
// DELETE /api/v1/keys/shares/:share_id
func (h *KeyHandler) RevokeShare(c *gin.Context) {
	userID, exists := c.Get("userID")
	if !exists {
		Unauthorized(c, "未认证")
		return
	}

	shareID, err := strconv.ParseUint(c.Param("share_id"), 10, 32)
	if err != nil {
		BadRequest(c, "无效的分享ID")
		return
	}

	if err := h.keyService.RevokeShare(c.Request.Context(), uint(shareID), userID.(uint)); err != nil {
		switch err {
		case services.ErrShareNotFound:
			NotFound(c, "分享记录不存在")
		case services.ErrUnauthorized:
			Forbidden(c, "无权撤销此分享")
		default:
			InternalError(c, "撤销分享失败", err.Error())
		}
		return
	}

	SuccessMsg(c, "分享已撤销", nil)
}

// ActivateKey 激活钥匙
// POST /api/v1/keys/:id/activate
func (h *KeyHandler) ActivateKey(c *gin.Context) {
	userID, exists := c.Get("userID")
	if !exists {
		Unauthorized(c, "未认证")
		return
	}

	keyID, err := strconv.ParseUint(c.Param("id"), 10, 32)
	if err != nil {
		BadRequest(c, "无效的钥匙ID")
		return
	}

	key, err := h.keyService.GetKey(c.Request.Context(), uint(keyID), userID.(uint))
	if err != nil {
		NotFound(c, "钥匙不存在")
		return
	}

	// 更新状态为激活
	if err := h.keyService.UpdateKeyStatus(c.Request.Context(), uint(keyID), userID.(uint), models.KeyStatusActive); err != nil {
		switch err {
		case services.ErrKeyNotFound:
			NotFound(c, "钥匙不存在")
		case services.ErrUnauthorized:
			Forbidden(c, "无权激活此钥匙")
		default:
			InternalError(c, "激活钥匙失败", err.Error())
		}
		return
	}

	SuccessMsg(c, "钥匙已激活", keyToResponse(key))
}

// DeactivateKey 停用钥匙
// POST /api/v1/keys/:id/deactivate
func (h *KeyHandler) DeactivateKey(c *gin.Context) {
	userID, exists := c.Get("userID")
	if !exists {
		Unauthorized(c, "未认证")
		return
	}

	keyID, err := strconv.ParseUint(c.Param("id"), 10, 32)
	if err != nil {
		BadRequest(c, "无效的钥匙ID")
		return
	}

	key, err := h.keyService.GetKey(c.Request.Context(), uint(keyID), userID.(uint))
	if err != nil {
		NotFound(c, "钥匙不存在")
		return
	}

	// 更新状态为停用
	if err := h.keyService.UpdateKeyStatus(c.Request.Context(), uint(keyID), userID.(uint), models.KeyStatusInactive); err != nil {
		switch err {
		case services.ErrKeyNotFound:
			NotFound(c, "钥匙不存在")
		case services.ErrUnauthorized:
			Forbidden(c, "无权停用此钥匙")
		default:
			InternalError(c, "停用钥匙失败", err.Error())
		}
		return
	}

	SuccessMsg(c, "钥匙已停用", keyToResponse(key))
}

// GetKeyLogs 获取钥匙使用日志
// GET /api/v1/keys/:id/logs
func (h *KeyHandler) GetKeyLogs(c *gin.Context) {
	userID, exists := c.Get("userID")
	if !exists {
		Unauthorized(c, "未认证")
		return
	}

	keyID, err := strconv.ParseUint(c.Param("id"), 10, 32)
	if err != nil {
		BadRequest(c, "无效的钥匙ID")
		return
	}

	// 验证钥匙访问权限
	_, err = h.keyService.GetKey(c.Request.Context(), uint(keyID), userID.(uint))
	if err != nil {
		Forbidden(c, "无权访问此钥匙日志")
		return
	}

	// 分页参数
	page, _ := strconv.Atoi(c.DefaultQuery("page", "1"))
	pageSize, _ := strconv.Atoi(c.DefaultQuery("page_size", "20"))

	// NOTE: 调用服务层获取日志（需先实现 KeyLog 模型与仓库接口）
	// 暂返回空数组
	logs := []gin.H{}

	SuccessList(c, map[string]interface{}{
		"list":      logs,
		"page":      page,
		"page_size": pageSize,
	}, 0)
}

// keyToResponse 转换钥匙为响应格式
func keyToResponse(key *models.Key) models.KeyResponse {
	resp := models.KeyResponse{
		ID:            key.ID,
		UserID:        key.UserID,
		VehicleID:     key.VehicleID,
		Type:          key.Type,
		Status:        key.Status,
		Permissions:   key.Permissions,
		KeyIdentifier: key.KeyIdentifier,
		Name:          key.Name,
		Description:   key.Description,
		ExpiresAt:     key.ExpiresAt,
		LastUsedAt:    key.LastUsedAt,
		UsageCount:    key.UsageCount,
		CreatedAt:     key.CreatedAt,
		UpdatedAt:     key.UpdatedAt,
	}

	if key.Vehicle.ID != 0 {
		resp.VehicleName = key.Vehicle.Name
	}

	return resp
}
