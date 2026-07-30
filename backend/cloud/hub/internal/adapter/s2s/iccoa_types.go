package s2s

import "time"

// ─── ICCOA S2S 基础类型 ────────────────────────────────────
// ICCOA (Intelligent Car Connectivity Open Alliance) §13.3
// 对接小米钱包/OPPO钱包/vivo钱包数字钥匙服务
// 无 Relay Server，分享通过车服务器 S2S 完成

// ICCOAConfig ICCOA 车服务器 API 端点配置
type ICCOAConfig struct {
	VendorName   string        // xiaomi, oppo, vivo
	BaseURL      string        // 车服务器 base URL
	VehicleOEMID string        // 车企唯一标识
	DeviceOEMID  string        // 设备厂商唯一标识
	TLSCertPath  string        // 双向 TLS 客户端证书路径
	TLSKeyPath   string        // TLS 客户端私钥路径
	Timeout      time.Duration // HTTP 请求超时
	RetryCount   int           // 重试次数
	RetryWait    time.Duration // 重试间隔
}

// NewDefaultICCOAConfig 返回默认 ICCOA S2S 配置
func NewDefaultICCOAConfig(vendorName, baseURL, vehicleOEMID, deviceOEMID string) ICCOAConfig {
	return ICCOAConfig{
		VendorName:   vendorName,
		BaseURL:      baseURL,
		VehicleOEMID: vehicleOEMID,
		DeviceOEMID:  deviceOEMID,
		Timeout:      30 * time.Second,
		RetryCount:   2,
		RetryWait:    1 * time.Second,
	}
}

// ─── 共享密钥 (SharedKey) §13.3 ─────────────────────────────

// ICCOASharedKey ICCOA 共享密钥结构
type ICCOASharedKey struct {
	KeyID             string `json:"keyId"`
	FriendlyName      string `json:"friendlyName,omitempty"`
	DeviceType        string `json:"deviceType,omitempty"`
	SharedDkCert      string `json:"sharedDkCert,omitempty"` // base64 DER
	StartDate         int64  `json:"startDate,omitempty"`
	EndDate           int64  `json:"endDate,omitempty"`
	KeyStatus         string `json:"keyStatus,omitempty"` // ACTIVE / DISABLED / REVOKED
}

// ICCOAVehicleProfile 车辆配置信息
type ICCOAVehicleProfile struct {
	VehicleModel      string   `json:"vehicleModel"`
	RkeFunctions      []string `json:"rkeFunctions,omitempty"`
	RemoteFunctions   []string `json:"remoteFunctions,omitempty"`
	KeyAccessProfiles []string `json:"keyAccessProfiles,omitempty"`
}

// ICCOADeviceInfo 设备信息
type ICCOADeviceInfo struct {
	DeviceModel string `json:"deviceModel"`
	DeviceBrand string `json:"deviceBrand"`
	DeviceType  string `json:"deviceType"`
}

// ICCOAKeyProprietaryData 密钥厂商自定义数据
type ICCOAKeyProprietaryData struct {
	VendorSpecific []byte `json:"vendorSpecific,omitempty"`
}

// ─── 请求/响应结构 (对应 §13.5 API) ─────────────────────────

// ICCOAGenSessionRequest 生成分享 sessionId
type ICCOAGenSessionRequest struct {
	KeyID        string `json:"keyId"`
	FromUserID   string `json:"fromUserId"`
	ToUserID     string `json:"toUserId,omitempty"`
	ValidFrom    int64  `json:"validFrom,omitempty"`
	ValidUntil   int64  `json:"validUntil,omitempty"`
	DeviceOEMID  string `json:"deviceOemId,omitempty"`
	VehicleOEMID string `json:"vehicleOemId,omitempty"`
}

// ICCOAGenSessionResponse 生成分享 sessionId 响应
type ICCOAGenSessionResponse struct {
	SessionID string `json:"sessionId"`
	ShareCode string `json:"shareCode,omitempty"` // 6位分享码
	ExpireAt  int64  `json:"expireAt,omitempty"`
}

// ICCOAGetMidCsrRequest 请求中间分享证书 CSR (CA模式)
type ICCOAGetMidCsrRequest struct {
	SessionID string `json:"sessionId"`
	Csr       string `json:"csr,omitempty"` // PKCS#10 CSR, base64
}

// ICCOAGetMidCsrResponse 中间分享证书 CSR 响应
type ICCOAGetMidCsrResponse struct {
	MidCsr string `json:"midCsr"` // 中间证书 CSR, base64
}

// ICCOAPutMidCertRequest 签发后的中间分享证书 (CA模式)
type ICCOAPutMidCertRequest struct {
	SessionID string `json:"sessionId"`
	MidCert   string `json:"midCert"` // 中间证书, base64 DER
}

// ICCOAPutMidCertResponse 中间证书上传响应
type ICCOAPutMidCertResponse struct {
	Status string `json:"status"`
}

// ICCOASignRequest 签发好友钥匙
type ICCOASignRequest struct {
	SessionID    string `json:"sessionId"`
	DevicePubKey string `json:"devicePubKey,omitempty"` // 设备公钥, base64
	DkCert       string `json:"dkCert,omitempty"`       // 数字钥匙证书, base64
}

// ICCOASignResponse 签发好友钥匙响应
type ICCOASignResponse struct {
	KeyID    string `json:"keyId"`
	DkCert   string `json:"dkCert"` // 签发的钥匙证书, base64 DER
}

// ICCOACancelShareRequest 撤销分享
type ICCOACancelShareRequest struct {
	KeyID     string `json:"keyId"`
	SessionID string `json:"sessionId,omitempty"`
	Reason    string `json:"reason,omitempty"`
}

// ICCOATrackKeyRequest 注册钥匙 (trackKey)
type ICCOATrackKeyRequest struct {
	KeyID        string `json:"keyId"`
	VehicleID    string `json:"vehicleId"`
	DeviceID     string `json:"deviceId"`
	UserID       string `json:"userId"`
	KeyType      string `json:"keyType"`      // "owner" / "friend" / "guest"
	AccessLevel  string `json:"accessLevel"`  // "full" / "limited"
	DevicePubKey string `json:"devicePubKey,omitempty"` // base64
	ValidFrom    int64  `json:"validFrom,omitempty"`
	ValidUntil   int64  `json:"validUntil,omitempty"`
}

// ICCOATrackKeyResponse 注册钥匙响应
type ICCOATrackKeyResponse struct {
	KeyID         string `json:"keyId"`
	VehiclePubKey string `json:"vehiclePubKey,omitempty"` // base64
	Status        string `json:"status"`
}

// ICCOAManageKeyRequest 钥匙状态管理
type ICCOAManageKeyRequest struct {
	KeyID  string `json:"keyId"`
	Action string `json:"action"` // "enable" / "disable" / "revoke"
	Reason string `json:"reason,omitempty"`
}

// ICCOAManageKeyResponse 钥匙状态管理响应
type ICCOAManageKeyResponse struct {
	Status string `json:"status"`
}

// ICCOANotifyKeyEventRequest 钥匙事件通知
type ICCOANotifyKeyEventRequest struct {
	KeyID     string `json:"keyId"`
	EventType string `json:"eventType"` // "bind" / "unbind" / "share" / "revoke"
	UserID    string `json:"userId,omitempty"`
	Timestamp int64  `json:"timestamp"`
}

// ICCOAGetVehicleProfileRequest 获取钥匙配置信息
type ICCOAGetVehicleProfileRequest struct {
	VehicleID string `json:"vehicleId"`
}

// ICCOAGetVehicleProfileResponse 钥匙配置信息响应
type ICCOAGetVehicleProfileResponse struct {
	Profile ICCOAVehicleProfile `json:"profile"`
}

// ICCOAHealthResponse 健康检查响应
type ICCOAHealthResponse struct {
	Status    string `json:"status"`
	Timestamp int64  `json:"timestamp"`
}

// ICCOAGetSarRequest 获取分享证明请求 (非CA模式)
type ICCOAGetSarRequest struct {
	SessionID string `json:"sessionId"`
}

// ICCOAGetSarResponse 获取分享证明请求响应
type ICCOAGetSarResponse struct {
	Sar string `json:"sar"` // 分享证明请求, base64
}

// ICCOAPutSharingAttestationRequest 签发分享证明 (非CA模式)
type ICCOAPutSharingAttestationRequest struct {
	SessionID         string `json:"sessionId"`
	SharingAttestation string `json:"sharingAttestation"` // 分享证明, base64
}

// ICCOAPutSharingAttestationResponse 签发分享证明响应
type ICCOAPutSharingAttestationResponse struct {
	Status string `json:"status"`
}

// ─── ICCOA API 错误 (§13.4) ─────────────────────────────────
// 错误码范围: 40000~50001

// ICCOAAPIError ICCOA API 错误响应
type ICCOAAPIError struct {
	Code      int    `json:"code"`
	Message   string `json:"message"`
	RequestID string `json:"requestId,omitempty"`
}

func (e *ICCOAAPIError) Error() string {
	return "ICCOA API error: " + e.Message
}
