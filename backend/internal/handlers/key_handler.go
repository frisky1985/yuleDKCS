package handlers

import (
	"net/http"
	"strconv"

	"yuleDKCS/backend/internal/models"
	"yuleDKCS/backend/internal/services"
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
		keys.POST("/:id/share", h.ShareKey)
		keys.DELETE("/:id", h.RevokeKey)
		keys.PUT("/:id/permissions", h.UpdatePermissions)
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
		c.JSON(http.StatusBadRequest, gin.H{
			"code":    400,
			"message": "请求参数错误",
			"error":   err.Error(),
		})
		return
	}

	// 从上下文中获取用户ID
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{
			"code":    401,
			"message": "未认证",
		})
		return
	}

	key, err := h.keyService.IssueKey(c.Request.Context(), userID.(uint), &req)
	if err != nil {
		switch err {
		case services.ErrInvalidKeyType:
			c.JSON(http.StatusBadRequest, gin.H{
				"code":    400,
				"message": "无效的钥匙类型",
			})
		case services.ErrUnauthorized:
			c.JSON(http.StatusForbidden, gin.H{
				"code":    403,
				"message": "无权为此车辆发行钥匙",
			})
		default:
			c.JSON(http.StatusInternalServerError, gin.H{
				"code":    500,
				"message": "发行钥匙失败",
				"error":   err.Error(),
			})
		}
		return
	}

	c.JSON(http.StatusCreated, gin.H{
		"code":    201,
		"message": "钥匙发行成功",
		"data":    keyToResponse(key),
	})
}

// GetUserKeys 获取用户钥匙列表
// GET /api/v1/keys
func (h *KeyHandler) GetUserKeys(c *gin.Context) {
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{
			"code":    401,
			"message": "未认证",
		})
		return
	}

	// 分页参数
	page, _ := strconv.Atoi(c.DefaultQuery("page", "1"))
	pageSize, _ := strconv.Atoi(c.DefaultQuery("page_size", "10"))

	keys, total, err := h.keyService.GetUserKeys(c.Request.Context(), userID.(uint), page, pageSize)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"code":    500,
			"message": "获取钥匙列表失败",
			"error":   err.Error(),
		})
		return
	}

	response := make([]models.KeyResponse, len(keys))
	for i, key := range keys {
		response[i] = keyToResponse(&key)
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

// GetKeyDetail 获取钥匙详情
// GET /api/v1/keys/:id
func (h *KeyHandler) GetKeyDetail(c *gin.Context) {
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{
			"code":    401,
			"message": "未认证",
		})
		return
	}

	keyID, err := strconv.ParseUint(c.Param("id"), 10, 32)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"code":    400,
			"message": "无效的钥匙ID",
		})
		return
	}

	key, err := h.keyService.GetKey(c.Request.Context(), uint(keyID), userID.(uint))
	if err != nil {
		switch err {
		case services.ErrKeyNotFound:
			c.JSON(http.StatusNotFound, gin.H{
				"code":    404,
				"message": "钥匙不存在",
			})
		case services.ErrUnauthorized:
			c.JSON(http.StatusForbidden, gin.H{
				"code":    403,
				"message": "无权访问此钥匙",
			})
		default:
			c.JSON(http.StatusInternalServerError, gin.H{
				"code":    500,
				"message": "获取钥匙详情失败",
				"error":   err.Error(),
			})
		}
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"code":    200,
		"message": "获取成功",
		"data":    keyToResponse(key),
	})
}

// ShareKey 分享钥匙
// POST /api/v1/keys/:id/share
func (h *KeyHandler) ShareKey(c *gin.Context) {
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{
			"code":    401,
			"message": "未认证",
		})
		return
	}

	keyID, err := strconv.ParseUint(c.Param("id"), 10, 32)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"code":    400,
			"message": "无效的钥匙ID",
		})
		return
	}

	var req models.ShareKeyRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"code":    400,
			"message": "请求参数错误",
			"error":   err.Error(),
		})
		return
	}

	share, err := h.keyService.ShareKey(c.Request.Context(), uint(keyID), userID.(uint), &req)
	if err != nil {
		switch err {
		case services.ErrKeyNotFound:
			c.JSON(http.StatusNotFound, gin.H{
				"code":    404,
				"message": "钥匙不存在",
			})
		case services.ErrUnauthorized:
			c.JSON(http.StatusForbidden, gin.H{
				"code":    403,
				"message": "无权分享此钥匙",
			})
		case services.ErrCannotShareToSelf:
			c.JSON(http.StatusBadRequest, gin.H{
				"code":    400,
				"message": "不能分享给自己",
			})
		case services.ErrInvalidPermissions:
			c.JSON(http.StatusBadRequest, gin.H{
				"code":    400,
				"message": "分享权限超过原钥匙权限",
			})
		default:
			c.JSON(http.StatusInternalServerError, gin.H{
				"code":    500,
				"message": "分享钥匙失败",
				"error":   err.Error(),
			})
		}
		return
	}

	c.JSON(http.StatusCreated, gin.H{
		"code":    201,
		"message": "钥匙分享成功",
		"data":    share,
	})
}

// RevokeKey 撤销钥匙
// DELETE /api/v1/keys/:id
func (h *KeyHandler) RevokeKey(c *gin.Context) {
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{
			"code":    401,
			"message": "未认证",
		})
		return
	}

	keyID, err := strconv.ParseUint(c.Param("id"), 10, 32)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"code":    400,
			"message": "无效的钥匙ID",
		})
		return
	}

	if err := h.keyService.RevokeKey(c.Request.Context(), uint(keyID), userID.(uint)); err != nil {
		switch err {
		case services.ErrKeyNotFound:
			c.JSON(http.StatusNotFound, gin.H{
				"code":    404,
				"message": "钥匙不存在",
			})
		case services.ErrUnauthorized:
			c.JSON(http.StatusForbidden, gin.H{
				"code":    403,
				"message": "无权撤销此钥匙",
			})
		default:
			c.JSON(http.StatusInternalServerError, gin.H{
				"code":    500,
				"message": "撤销钥匙失败",
				"error":   err.Error(),
			})
		}
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"code":    200,
		"message": "钥匙已撤销",
	})
}

// UpdatePermissions 更新钥匙权限
// PUT /api/v1/keys/:id/permissions
func (h *KeyHandler) UpdatePermissions(c *gin.Context) {
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{
			"code":    401,
			"message": "未认证",
		})
		return
	}

	keyID, err := strconv.ParseUint(c.Param("id"), 10, 32)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"code":    400,
			"message": "无效的钥匙ID",
		})
		return
	}

	var req models.UpdatePermissionsRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"code":    400,
			"message": "请求参数错误",
			"error":   err.Error(),
		})
		return
	}

	if err := h.keyService.UpdateKeyPermissions(c.Request.Context(), uint(keyID), userID.(uint), req.Permissions); err != nil {
		switch err {
		case services.ErrKeyNotFound:
			c.JSON(http.StatusNotFound, gin.H{
				"code":    404,
				"message": "钥匙不存在",
			})
		case services.ErrUnauthorized:
			c.JSON(http.StatusForbidden, gin.H{
				"code":    403,
				"message": "无权修改此钥匙权限",
			})
		default:
			c.JSON(http.StatusInternalServerError, gin.H{
				"code":    500,
				"message": "更新权限失败",
				"error":   err.Error(),
			})
		}
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"code":    200,
		"message": "权限更新成功",
	})
}

// GetSharedKeys 获取用户收到的分享钥匙
// GET /api/v1/keys/shared/list
func (h *KeyHandler) GetSharedKeys(c *gin.Context) {
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{
			"code":    401,
			"message": "未认证",
		})
		return
	}

	shares, err := h.keyService.GetSharedKeys(c.Request.Context(), userID.(uint))
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"code":    500,
			"message": "获取分享列表失败",
			"error":   err.Error(),
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"code":    200,
		"message": "获取成功",
		"data":    shares,
	})
}

// GetKeyShares 获取钥匙的分享列表
// GET /api/v1/keys/:id/shares
func (h *KeyHandler) GetKeyShares(c *gin.Context) {
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{
			"code":    401,
			"message": "未认证",
		})
		return
	}

	keyID, err := strconv.ParseUint(c.Param("id"), 10, 32)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"code":    400,
			"message": "无效的钥匙ID",
		})
		return
	}

	shares, err := h.keyService.GetKeyShares(c.Request.Context(), uint(keyID), userID.(uint))
	if err != nil {
		switch err {
		case services.ErrKeyNotFound:
			c.JSON(http.StatusNotFound, gin.H{
				"code":    404,
				"message": "钥匙不存在",
			})
		case services.ErrUnauthorized:
			c.JSON(http.StatusForbidden, gin.H{
				"code":    403,
				"message": "无权查看此钥匙的分享",
			})
		default:
			c.JSON(http.StatusInternalServerError, gin.H{
				"code":    500,
				"message": "获取分享列表失败",
				"error":   err.Error(),
			})
		}
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"code":    200,
		"message": "获取成功",
		"data":    shares,
	})
}

// RevokeShare 撤销分享
// DELETE /api/v1/keys/shares/:share_id
func (h *KeyHandler) RevokeShare(c *gin.Context) {
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{
			"code":    401,
			"message": "未认证",
		})
		return
	}

	shareID, err := strconv.ParseUint(c.Param("share_id"), 10, 32)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"code":    400,
			"message": "无效的分享ID",
		})
		return
	}

	if err := h.keyService.RevokeShare(c.Request.Context(), uint(shareID), userID.(uint)); err != nil {
		switch err {
		case services.ErrShareNotFound:
			c.JSON(http.StatusNotFound, gin.H{
				"code":    404,
				"message": "分享记录不存在",
			})
		case services.ErrUnauthorized:
			c.JSON(http.StatusForbidden, gin.H{
				"code":    403,
				"message": "无权撤销此分享",
			})
		default:
			c.JSON(http.StatusInternalServerError, gin.H{
				"code":    500,
				"message": "撤销分享失败",
				"error":   err.Error(),
			})
		}
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"code":    200,
		"message": "分享已撤销",
	})
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
