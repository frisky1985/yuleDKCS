package handlers

import (
	"strconv"

	"github.com/frisky1985/yuleDKCS/backend/internal/middleware"
	"github.com/frisky1985/yuleDKCS/backend/internal/services"
	"github.com/gin-gonic/gin"
)

// UserHandler 用户处理器
type UserHandler struct {
	userService services.UserService
}

// NewUserHandler 创建用户处理器
func NewUserHandler(userService services.UserService) *UserHandler {
	return &UserHandler{
		userService: userService,
	}
}

// LoginRequest 登录请求
type LoginRequest struct {
	Username string `json:"username" binding:"required"`
	Password string `json:"password" binding:"required"`
}

// RegisterRequest 注册请求
type RegisterRequest struct {
	Username string `json:"username" binding:"required,min=3,max=50"`
	Email    string `json:"email" binding:"required,email"`
	Password string `json:"password" binding:"required,min=6"`
	Phone    string `json:"phone"`
}

// Login 用户登录
// POST /api/v1/auth/login
func (h *UserHandler) Login(c *gin.Context) {
	var req LoginRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		BadRequest(c, "请求参数错误", err.Error())
		return
	}

	user, err := h.userService.ValidateCredentials(c.Request.Context(), req.Username, req.Password)
	if err != nil {
		Unauthorized(c, "用户名或密码错误")
		return
	}

	// 生成 JWT Token
	token, err := middleware.GenerateToken(
		string(rune(user.ID)),
		user.Username,
		user.Role,
	)
	if err != nil {
		InternalError(c, "生成 Token 失败")
		return
	}

	SuccessMsg(c, "登录成功", gin.H{
		"token": token,
		"type":  "Bearer",
		"user": gin.H{
			"id":       user.ID,
			"username": user.Username,
			"email":    user.Email,
			"role":     user.Role,
		},
	})
}

// Register 用户注册
// POST /api/v1/auth/register
func (h *UserHandler) Register(c *gin.Context) {
	var req RegisterRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		BadRequest(c, "请求参数错误", err.Error())
		return
	}

	user, err := h.userService.CreateUser(c.Request.Context(), req.Username, req.Email, req.Password)
	if err != nil {
		BadRequest(c, err.Error())
		return
	}

	SuccessCreated(c, gin.H{
		"id":       user.ID,
		"username": user.Username,
		"email":    user.Email,
	})
}

// GetProfile 获取用户信息
// GET /api/v1/user/profile
func (h *UserHandler) GetProfile(c *gin.Context) {
	userID, exists := c.Get("userID")
	if !exists {
		Unauthorized(c, "未认证")
		return
	}

	// userID 是 string 类型，需要转换
	idStr := userID.(string)
	idUint64, err := strconv.ParseUint(idStr, 10, 64)
	if err != nil {
		BadRequest(c, "无效的用户ID")
		return
	}
	id := uint(idUint64)

	user, err := h.userService.GetUserByID(c.Request.Context(), id)
	if err != nil {
		NotFound(c, "用户不存在")
		return
	}

	Success(c, gin.H{
		"id":         user.ID,
		"username":   user.Username,
		"email":      user.Email,
		"phone":      user.Phone,
		"role":       user.Role,
		"created_at": user.CreatedAt,
	})
}
