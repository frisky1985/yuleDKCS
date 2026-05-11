package middleware

import (
	"net/http"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/golang-jwt/jwt/v5"
)

// JWTConfig JWT 配置
type JWTConfig struct {
	SecretKey       string
	TokenExpiration time.Duration
	Issuer          string
}

// 默认配置
var DefaultJWTConfig = JWTConfig{
	SecretKey:       getEnvOrDefault("JWT_SECRET", "yuleDKCS-super-secret-key-change-in-production"),
	TokenExpiration: 24 * time.Hour,
	Issuer:          "yuleDKCS",
}

// Claims JWT Claims 结构
type Claims struct {
	UserID   string `json:"user_id"`
	Username string `json:"username"`
	Role     string `json:"role"`
	jwt.RegisteredClaims
}

// GenerateToken 生成 JWT Token
func GenerateToken(userID, username, role string) (string, error) {
	claims := Claims{
		UserID:   userID,
		Username: username,
		Role:     role,
		RegisteredClaims: jwt.RegisteredClaims{
			ExpiresAt: jwt.NewNumericDate(time.Now().Add(DefaultJWTConfig.TokenExpiration)),
			IssuedAt:  jwt.NewNumericDate(time.Now()),
			Issuer:    DefaultJWTConfig.Issuer,
		},
	}

	token := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	return token.SignedString([]byte(DefaultJWTConfig.SecretKey))
}

// JWTAuth JWT 认证中间件
func JWTAuth() gin.HandlerFunc {
	return func(c *gin.Context) {
		authHeader := c.GetHeader("Authorization")
		if authHeader == "" {
			c.JSON(http.StatusUnauthorized, gin.H{
				"error": "缺少 Authorization 头",
				"code":  "AUTH_MISSING",
			})
			c.Abort()
			return
		}

		// 提取 Bearer Token
		parts := strings.SplitN(authHeader, " ", 2)
		if !(len(parts) == 2 && parts[0] == "Bearer") {
			c.JSON(http.StatusUnauthorized, gin.H{
				"error": "Authorization 格式错误，需要 Bearer Token",
				"code":  "AUTH_INVALID_FORMAT",
			})
			c.Abort()
			return
		}

		tokenString := parts[1]

		// 解析和验证 Token
		claims := &Claims{}
		token, err := jwt.ParseWithClaims(tokenString, claims, func(token *jwt.Token) (interface{}, error) {
			return []byte(DefaultJWTConfig.SecretKey), nil
		})

		if err != nil {
			if err == jwt.ErrTokenExpired {
				c.JSON(http.StatusUnauthorized, gin.H{
					"error": "Token 已过期",
					"code":  "AUTH_TOKEN_EXPIRED",
				})
			} else {
				c.JSON(http.StatusUnauthorized, gin.H{
					"error": "Token 无效",
					"code":  "AUTH_INVALID_TOKEN",
				})
			}
			c.Abort()
			return
		}

		if !token.Valid {
			c.JSON(http.StatusUnauthorized, gin.H{
				"error": "Token 验证失败",
				"code":  "AUTH_TOKEN_INVALID",
			})
			c.Abort()
			return
		}

		// 将用户信息存储到上下文
		c.Set("userID", claims.UserID)
		c.Set("username", claims.Username)
		c.Set("role", claims.Role)
		c.Set("claims", claims)

		c.Next()
	}
}

// RefreshToken 刷新 Token
func RefreshToken(c *gin.Context) {
	authHeader := c.GetHeader("Authorization")
	if authHeader == "" {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "缺少 Authorization 头"})
		return
	}

	parts := strings.SplitN(authHeader, " ", 2)
	if !(len(parts) == 2 && parts[0] == "Bearer") {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Authorization 格式错误"})
		return
	}

	tokenString := parts[1]

	// 解析 Token（带宽限的验证，允许过期 Token 刷新）
	claims := &Claims{}
	token, err := jwt.ParseWithClaims(tokenString, claims, func(token *jwt.Token) (interface{}, error) {
		return []byte(DefaultJWTConfig.SecretKey), nil
	}, jwt.WithoutClaimsValidation())

	if err != nil && token == nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Token 无效"})
		return
	}

	// 检查 Token 是否在刷新窗口期内（60天内）
	if claims.ExpiresAt != nil {
		if time.Since(claims.ExpiresAt.Time) > 60*24*time.Hour {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "Token 已过期太久，无法刷新"})
			return
		}
	}

	// 生成新 Token
	newToken, err := GenerateToken(claims.UserID, claims.Username, claims.Role)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "生成 Token 失败"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"token": newToken,
		"type":  "Bearer",
	})
}

// RequireRole 角色权限中间件
func RequireRole(roles ...string) gin.HandlerFunc {
	return func(c *gin.Context) {
		role, exists := c.Get("role")
		if !exists {
			c.JSON(http.StatusForbidden, gin.H{
				"error": "无权访问",
				"code":  "FORBIDDEN",
			})
			c.Abort()
			return
		}

		userRole := role.(string)
		hasRole := false
		for _, r := range roles {
			if r == userRole {
				hasRole = true
				break
			}
		}

		if !hasRole {
			c.JSON(http.StatusForbidden, gin.H{
				"error": "权限不足",
				"code":  "INSUFFICIENT_PERMISSIONS",
			})
			c.Abort()
			return
		}

		c.Next()
	}
}

// 辅助函数
func getEnvOrDefault(key, defaultValue string) string {
	// 实际实现需要从环境变量读取
	return defaultValue
}
