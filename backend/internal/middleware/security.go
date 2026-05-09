package middleware

import (
	"github.com/gin-gonic/gin"
)

// SecurityHeaders 安全响应头中间件
func SecurityHeaders() gin.HandlerFunc {
	return func(c *gin.Context) {
		// 防止 MIME 类型探测攻击
		c.Header("X-Content-Type-Options", "nosniff")
		
		// 防止点击劫持
		c.Header("X-Frame-Options", "DENY")
		
		// XSS 保护
		c.Header("X-XSS-Protection", "1; mode=block")
		
		// 强制 HTTPS
		c.Header("Strict-Transport-Security", "max-age=31536000; includeSubDomains; preload")
		
		// 内容安全策略
		c.Header("Content-Security-Policy", 
			"default-src 'self'; "+
			"script-src 'self' 'unsafe-inline'; "+
			"style-src 'self' 'unsafe-inline'; "+
			"img-src 'self' data: https:; "+
			"font-src 'self'; "+
			"connect-src 'self'; "+
			"frame-ancestors 'none'; "+
			"base-uri 'self'; "+
			"form-action 'self';")
		
		// Referrer 策略
		c.Header("Referrer-Policy", "strict-origin-when-cross-origin")
		
		// 权限策略
		c.Header("Permissions-Policy", 
			"accelerometer=(), "+
			"camera=(), "+
			"geolocation=(), "+
			"gyroscope=(), "+
			"magnetometer=(), "+
			"microphone=(), "+
			"payment=(), "+
			"usb=()")
		
		c.Next()
	}
}

// CSPReportOnly 仅报告模式的 CSP
func CSPReportOnly() gin.HandlerFunc {
	return func(c *gin.Context) {
		c.Header("Content-Security-Policy-Report-Only", 
			"default-src 'self'; report-uri /api/v1/csp-report")
		c.Next()
	}
}
