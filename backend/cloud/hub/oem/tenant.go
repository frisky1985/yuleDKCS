package oem

import "context"

// TenantManager 租户管理接口
// 提供多租户的创建、查询、配置能力
type TenantManager interface {
	// CreateTenant 创建新租户（OEM品牌入驻）
	CreateTenant(ctx context.Context, tenant *Tenant) error

	// GetTenant 获取租户信息
	GetTenant(ctx context.Context, tenantID string) (*Tenant, error)

	// UpdateTenant 更新租户配置
	UpdateTenant(ctx context.Context, tenant *Tenant) error

	// DeleteTenant 删除租户
	DeleteTenant(ctx context.Context, tenantID string) error

	// ListTenants 列出所有租户
	ListTenants(ctx context.Context) ([]*Tenant, error)

	// UpdateStatus 更新租户状态
	UpdateStatus(ctx context.Context, tenantID string, status TenantStatus) error
}

// ConfigManager 租户配置管理接口
type ConfigManager interface {
	// GetConfig 获取租户配置
	GetConfig(ctx context.Context, tenantID string) (*TenantConfig, error)

	// UpdateConfig 更新租户配置
	UpdateConfig(ctx context.Context, tenantID string, config *TenantConfig) error

	// GetBranding 获取租户品牌配置
	GetBranding(ctx context.Context, tenantID string) (*BrandingConfig, error)

	// UpdateBranding 更新租户品牌配置
	UpdateBranding(ctx context.Context, tenantID string, branding *BrandingConfig) error

	// IsFeatureEnabled 检查租户功能是否开启
	IsFeatureEnabled(ctx context.Context, tenantID string, feature string) (bool, error)

	// GetSupportedProtocols 获取租户支持的协议列表
	GetSupportedProtocols(ctx context.Context, tenantID string) ([]string, error)
}

// ScopeResolver 租户作用域解析接口
// 根据用户/设备/车辆信息解析所属租户
type ScopeResolver interface {
	// ResolveByUser 根据用户ID解析租户
	ResolveByUser(ctx context.Context, userID string) (*OEMScope, error)

	// ResolveByDevice 根据设备ID解析租户
	ResolveByDevice(ctx context.Context, deviceID string) (*OEMScope, error)

	// ResolveByVehicle 根据车辆ID解析租户
	ResolveByVehicle(ctx context.Context, vehicleID string) (*OEMScope, error)
}
