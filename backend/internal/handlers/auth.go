package handlers

import (
	"net/http"

	"github.com/gin-gonic/gin"
)

// AuthHandler 认证处理器
type AuthHandler struct {
	// TODO: 添加 service 依赖
}

// NewAuthHandler 创建认证处理器
func NewAuthHandler() *AuthHandler {
	return &AuthHandler{}
}

// RefreshToken 刷新 Token
func (h *AuthHandler) RefreshToken(c *gin.Context) {
	// TODO: 实现刷新 Token 逻辑
	c.JSON(http.StatusOK, gin.H{
		"token": "new-placeholder-token",
	})
}
