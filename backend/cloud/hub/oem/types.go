// Package oem 定义 OEM 多租户隔离接口
// 对应 Device HUB 的 OEM 管理能力 — 租户隔离、品牌配置、白标支持
package oem

import "time"

// Tenant 租户（OEM 品牌）
type Tenant struct {
	TenantID     string            `json:"tenant_id"`
	Name         string            `json:"name"`          // 品牌名称 (e.g. "NIO", "BYD", "XPeng")
	Domain       string            `json:"domain"`        // 域名标识
	Status       TenantStatus      `json:"status"`
	Config       *TenantConfig     `json:"config"`
	Branding     *BrandingConfig   `json:"branding"`
	CreatedAt    time.Time         `json:"created_at"`
	UpdatedAt    time.Time         `json:"updated_at"`
}

// TenantStatus 租户状态
type TenantStatus string

const (
	TenantActive   TenantStatus = "active"
	TenantInactive TenantStatus = "inactive"
	TenantDisabled TenantStatus = "disabled"
	TenantTrial    TenantStatus = "trial"
)

// TenantConfig 租户配置
type TenantConfig struct {
	MaxDevicesPerUser     int               `json:"max_devices_per_user"`     // 每用户最大设备数
	MaxKeysPerVehicle     int               `json:"max_keys_per_vehicle"`    // 每车最大密钥数
	SupportedProtocols    []string          `json:"supported_protocols"`     // 支持的协议列表
	AllowedVendors        []string          `json:"allowed_vendors"`         // 允许的手机厂商
	Features              map[string]bool   `json:"features"`                // 功能开关
	CustomSettings        map[string]string `json:"custom_settings"`         // 自定义配置
}

// BrandingConfig 品牌配置
type BrandingConfig struct {
	BrandName      string `json:"brand_name"`       // 品牌名
	AppName        string `json:"app_name"`          // App 显示名
	LogoURL        string `json:"logo_url"`          // Logo 地址
	PrimaryColor   string `json:"primary_color"`     // 主色调
	SupportEmail   string `json:"support_email"`     // 技术支持邮箱
	SupportPhone   string `json:"support_phone"`     // 技术支持电话
	PrivacyPolicy  string `json:"privacy_policy"`    // 隐私政策 URL
	TermsOfService string `json:"terms_of_service"`  // 服务条款 URL
}

// OEMScope 租户作用域
// 在多租户场景下标识操作归属
type OEMScope struct {
	TenantID string `json:"tenant_id"`
	Region   string `json:"region,omitempty"` // 地域: CN/EU/US
	Brand    string `json:"brand,omitempty"`  // 品牌标识
}
