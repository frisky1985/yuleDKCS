package s2s

import "time"

// ─── ICCE S2S 基础类型 ──────────────────────────────────────
// ICCE (Intelligent Car Connectivity Ecosystem) — 以车厂 OEM 为中心的 S2S 架构
// 对接华为数字钥匙服务（HUAWEI CarKey API）
// 无 Relay Server，Hub 直接与厂商 S2S 端点通信

// ICCEEndpoint ICCE 厂商 API 端点配置
type ICCEEndpoint struct {
	BaseURL    string        // 厂商 API 基础 URL
	Timeout    time.Duration // HTTP 请求超时
	RetryCount int           // 重试次数
	RetryWait  time.Duration // 重试间隔
}

// DefaultICCEConfig 返回默认 ICCE S2S 配置
func DefaultICCEConfig() ICCEEndpoint {
	return ICCEEndpoint{
		BaseURL:    "https://api.huawei.com/carkey/v2",
		Timeout:    30 * time.Second,
		RetryCount: 2,
		RetryWait:  1 * time.Second,
	}
}

// ─── 请求/响应结构 ──────────────────────────────────────────

// ICCEBindRequest 密钥绑定请求
type ICCEBindRequest struct {
	VehicleID    string `json:"vehicle_id"`
	DeviceID     string `json:"device_id"`
	UserID       string `json:"user_id"`
	DevicePubKey string `json:"device_pubkey"` // base64 编码的设备公钥
	KeyType      string `json:"key_type"`       // "owner" / "friend" / "guest"
	AccessLevel  string `json:"access_level"`   // "full" / "limited"
	ValidFrom    int64  `json:"valid_from,omitempty"`
	ValidUntil   int64  `json:"valid_until,omitempty"`
}

// ICCEBindResponse 密钥绑定响应
type ICCEBindResponse struct {
	KeyID          string `json:"key_id"`
	VehiclePubKey  string `json:"vehicle_pubkey"`   // base64 编码的车端公钥
	SharedSecret   string `json:"shared_secret"`    // base64 编码的共享密钥
	Status         string `json:"status"`
	CreatedAt      int64  `json:"created_at"`
}

// ICCEUnbindRequest 密钥解绑请求
type ICCEUnbindRequest struct {
	KeyID string `json:"key_id"`
}

// ICCEShareRequest 密钥分享请求
type ICCEShareRequest struct {
	KeyID      string `json:"key_id"`
	FromUserID string `json:"from_user_id"`
}

// ICCEShareResponse 密钥分享响应
type ICCEShareResponse struct {
	ShareID   string `json:"share_id"`
	ShareCode string `json:"share_code"`
	ExpireAt  int64  `json:"expire_at,omitempty"`
}

// ICCEAcceptShareRequest 接受分享请求
type ICCEAcceptShareRequest struct {
	ShareCode string `json:"share_code"`
	DeviceID  string `json:"device_id"`
	UserID    string `json:"user_id"`
}

// ICCERevokeRequest 密钥撤销请求
type ICCERevokeRequest struct {
	KeyID  string `json:"key_id"`
	Reason string `json:"reason"`
}

// ICCEHealthResponse 健康检查响应
type ICCEHealthResponse struct {
	Status    string `json:"status"`
	Timestamp int64  `json:"timestamp"`
}

// ─── ICCE API 错误 ─────────────────────────────────────────

// ICCEAPIError ICCE API 错误响应
type ICCEAPIError struct {
	Code    int    `json:"code"`
	Message string `json:"message"`
	Details string `json:"details,omitempty"`
}

func (e *ICCEAPIError) Error() string {
	return "ICCE API error: " + e.Message
}
