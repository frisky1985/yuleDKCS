package tenant

import (
	"context"
	"fmt"
	"strings"

	"github.com/gin-gonic/gin"
)

// TenantContextKey 租户上下文键
type TenantContextKey struct{}

// Tenant 租户信息
type Tenant struct {
	ID        uint   `json:"id"`
	Name      string `json:"name"`
	Domain    string `json:"domain"`
	Schema    string `json:"schema"`
	IsActive  bool   `json:"is_active"`
	Settings  map[string]interface{} `json:"settings"`
}

// Context 租户上下文
type Context struct {
	Tenant *Tenant
	ctx    context.Context
}

// WithTenant 将租户信息注入上下文
func WithTenant(ctx context.Context, tenant *Tenant) context.Context {
	return context.WithValue(ctx, TenantContextKey{}, tenant)
}

// FromContext 从上下文获取租户
func FromContext(ctx context.Context) (*Tenant, error) {
	tenant, ok := ctx.Value(TenantContextKey{}).(*Tenant)
	if !ok || tenant == nil {
		return nil, fmt.Errorf("tenant not found in context")
	}
	return tenant, nil
}

// Middleware 租户中间件
// 支持通过 subdomain 或 header 识别租户
func Middleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		var tenantID uint
		var err error

		// 优先从 header 获取
		tenantHeader := c.GetHeader("X-Tenant-ID")
		if tenantHeader != "" {
			// 解析 tenant ID
			// 实际项目中需要查询数据库验证
			tenantID = parseTenantID(tenantHeader)
		} else {
			// 从 subdomain 获取
			host := c.Request.Host
			subdomain := extractSubdomain(host)
			tenantID = getTenantIDBySubdomain(subdomain)
		}

		if tenantID == 0 {
			c.JSON(400, gin.H{"error": "tenant not found"})
			c.Abort()
			return
		}

		// 获取租户信息
		tenant := &Tenant{
			ID:     tenantID,
			Schema: fmt.Sprintf("tenant_%d", tenantID),
		}

		// 将租户信息存入上下文
		c.Set(string(TenantContextKey{}), tenant)
		c.Request = c.Request.WithContext(WithTenant(c.Request.Context(), tenant))

		c.Next()
	}
}

// GetTenant 从 gin 上下文获取租户
func GetTenant(c *gin.Context) (*Tenant, error) {
	tenant, exists := c.Get(string(TenantContextKey{}))
	if !exists {
		return nil, fmt.Errorf("tenant not found in context")
	}
	return tenant.(*Tenant), nil
}

// parseTenantID 解析 tenant ID (示例实现)
func parseTenantID(id string) uint {
	// 实际项目中需要验证 ID 格式并查询数据库
	// 这里返回示例值
	return 1
}

// extractSubdomain 提取 subdomain
func extractSubdomain(host string) string {
	parts := strings.Split(host, ".")
	if len(parts) > 2 {
		return parts[0]
	}
	return ""
}

// getTenantIDBySubdomain 通过 subdomain 获取 tenant ID
func getTenantIDBySubdomain(subdomain string) uint {
	// 实际项目中需要查询数据库
	// 这里返回示例值
	if subdomain == "" {
		return 1 // 默认租户
	}
	return 1
}
