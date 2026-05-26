package handlers

import "github.com/gin-gonic/gin"

// Success 成功响应
func Success(c *gin.Context, data interface{}) {
	c.JSON(200, gin.H{"code": 200, "message": "success", "data": data})
}

// SuccessMsg 成功响应（自定义消息）
func SuccessMsg(c *gin.Context, msg string, data interface{}) {
	c.JSON(200, gin.H{"code": 200, "message": msg, "data": data})
}

// SuccessCreated 创建成功
func SuccessCreated(c *gin.Context, data interface{}) {
	c.JSON(201, gin.H{"code": 201, "message": "created", "data": data})
}

// SuccessList 分页列表成功响应
func SuccessList(c *gin.Context, data interface{}, total int64) {
	c.JSON(200, gin.H{"code": 200, "message": "success", "data": data, "total": total})
}

// Error 错误响应
func Error(c *gin.Context, status int, msg string, err ...string) {
	body := gin.H{"code": status, "message": msg}
	if len(err) > 0 && err[0] != "" {
		body["error"] = err[0]
	}
	c.JSON(status, body)
}

// BadRequest 400 参数错误
func BadRequest(c *gin.Context, msg string, err ...string) {
	Error(c, 400, msg, err...)
}

// Unauthorized 401 未认证
func Unauthorized(c *gin.Context, msg string) {
	Error(c, 401, msg)
}

// Forbidden 403 无权限
func Forbidden(c *gin.Context, msg string) {
	Error(c, 403, msg)
}

// NotFound 404 资源不存在
func NotFound(c *gin.Context, msg string) {
	Error(c, 404, msg)
}

// InternalError 500 服务器内部错误
func InternalError(c *gin.Context, msg string, err ...string) {
	Error(c, 500, msg, err...)
}
