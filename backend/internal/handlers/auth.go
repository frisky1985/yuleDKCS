package handlers

import (
	"github.com/frisky1985/yuleDKCS/backend/internal/middleware"
	"github.com/frisky1985/yuleDKCS/backend/internal/services"
	"github.com/gin-gonic/gin"
)

// AuthHandler 认证处理器
type AuthHandler struct {
	userService services.UserService
}

// NewAuthHandler 创建认证处理器
func NewAuthHandler(userService services.UserService) *AuthHandler {
	return &AuthHandler{
		userService: userService,
	}
}

// RefreshToken 刷新 Token
func (h *AuthHandler) RefreshToken(c *gin.Context) {
	// 委托给中间件的 RefreshToken 实现
	middleware.RefreshToken(c)
}
