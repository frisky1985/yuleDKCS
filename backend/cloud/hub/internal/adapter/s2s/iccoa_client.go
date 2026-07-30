package s2s

import (
	"bytes"
	"context"
	"crypto/tls"
	"crypto/x509"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"time"

	"go.uber.org/zap"
)

// ICCOAClient ICCOA 车服务器 S2S HTTP 客户端
// ICCOA 没有 Relay Server，分享通过车服务器 S2S 完成
// 使用双向 TLS (mTLS) + REST/JSON
type ICCOAClient struct {
	config   ICCOAConfig
	client   *http.Client
	logger   *zap.Logger
	vendor   string // 厂商标识，如 "xiaomi"
}

// ICCOAClientOption 客户端配置选项
type ICCOAClientOption func(*ICCOAClient)

// WithICCOATimeout 配置 HTTP 超时
func WithICCOATimeout(timeout time.Duration) ICCOAClientOption {
	return func(c *ICCOAClient) {
		c.client.Timeout = timeout
	}
}

// WithICCOATLS 配置 ICCOA 双向 TLS (mTLS) 证书
func WithICCOATLS(certFile, keyFile string) ICCOAClientOption {
	return func(c *ICCOAClient) {
		cert, err := tls.LoadX509KeyPair(certFile, keyFile)
		if err != nil {
			c.logger.Warn("ICCOA mTLS cert load failed, using default TLS", zap.Error(err))
			return
		}

		tlsConfig := &tls.Config{
			Certificates: []tls.Certificate{cert},
			MinVersion:   tls.VersionTLS12,
		}

		// 尝试加载 CA 证书（系统 CA + 自定义 CA）
		caPool, err := x509.SystemCertPool()
		if err != nil {
			caPool = x509.NewCertPool()
		}

		// 如果提供了 CA 文件，追加自定义 CA
		caFile := certFile + ".ca" // 约定: 同目录下 .ca 文件
		if caData, err := os.ReadFile(caFile); err == nil {
			if caPool.AppendCertsFromPEM(caData) {
				c.logger.Info("ICCOA CA cert loaded", zap.String("ca_file", caFile))
			}
		}

		tlsConfig.RootCAs = caPool
		c.client.Transport = &http.Transport{
			TLSClientConfig: tlsConfig,
			MaxIdleConns:    20,
			IdleConnTimeout: 90 * time.Second,
		}
	}
}

// NewICCOAClient 创建 ICCOA S2S 客户端
func NewICCOAClient(vendor string, config ICCOAConfig, logger *zap.Logger, opts ...ICCOAClientOption) *ICCOAClient {
	c := &ICCOAClient{
		config: config,
		client: &http.Client{Timeout: config.Timeout},
		logger: logger.With(
			zap.String("vendor", vendor),
			zap.String("component", "iccoa_s2s"),
			zap.String("vehicle_oem", config.VehicleOEMID),
			zap.String("device_oem", config.DeviceOEMID),
		),
		vendor: vendor,
	}

	// 自动加载 TLS 证书（如果路径已配置）
	if config.TLSCertPath != "" && config.TLSKeyPath != "" {
		WithICCOATLS(config.TLSCertPath, config.TLSKeyPath)(c)
	}

	for _, opt := range opts {
		opt(c)
	}

	return c
}

// ─── 通用请求方法 ──────────────────────────────────────────

// doRequest 发送 ICCOA S2S HTTP 请求
// 通用请求头:
//   X-ICCOA-Request-ID (String)
//   X-ICCOA-Timestamp (Long)
//   X-ICCOA-Device-OEM-ID (String)
//   X-ICCOA-Vehicle-OEM-ID (String)
func (c *ICCOAClient) doRequest(ctx context.Context, method, path string, reqBody, respBody interface{}) error {
	url := c.config.BaseURL + path

	var bodyReader io.Reader
	if reqBody != nil {
		data, err := json.Marshal(reqBody)
		if err != nil {
			return fmt.Errorf("ICCOA marshal request: %w", err)
		}
		bodyReader = bytes.NewReader(data)
	}

	req, err := http.NewRequestWithContext(ctx, method, url, bodyReader)
	if err != nil {
		return fmt.Errorf("ICCOA create request: %w", err)
	}

	// ICCOA 通用请求头 (§13.5)
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Accept", "application/json")
	req.Header.Set("X-ICCOA-Request-ID", fmt.Sprintf("%s-%d", c.vendor, time.Now().UnixNano()))
	req.Header.Set("X-ICCOA-Timestamp", fmt.Sprintf("%d", time.Now().UnixMilli()))
	req.Header.Set("X-ICCOA-Device-OEM-ID", c.config.DeviceOEMID)
	req.Header.Set("X-ICCOA-Vehicle-OEM-ID", c.config.VehicleOEMID)
	req.Header.Set("User-Agent", "YuleDKCS-Hub/1.0 ICCOA-S2S/1.0")

	// 重试逻辑
	var lastErr error
	for attempt := 0; attempt <= c.config.RetryCount; attempt++ {
		if attempt > 0 {
			select {
			case <-ctx.Done():
				return ctx.Err()
			case <-time.After(c.config.RetryWait):
			}
		}

		resp, err := c.client.Do(req)
		if err != nil {
			lastErr = fmt.Errorf("ICCOA request attempt %d: %w", attempt+1, err)
			continue
		}

		respBodyBytes, err := io.ReadAll(resp.Body)
		resp.Body.Close()
		if err != nil {
			lastErr = fmt.Errorf("ICCOA read response: %w", err)
			continue
		}

		if resp.StatusCode >= 200 && resp.StatusCode < 300 {
			if respBody != nil && len(respBodyBytes) > 0 {
				if err := json.Unmarshal(respBodyBytes, respBody); err != nil {
					return fmt.Errorf("ICCOA unmarshal response: %w", err)
				}
			}
			return nil
		}

		// 解析 ICCOA API 错误 (§13.4)
		var apiErr ICCOAAPIError
		if err := json.Unmarshal(respBodyBytes, &apiErr); err == nil && apiErr.Code != 0 {
			// 4xx 错误不重试
			if resp.StatusCode >= 400 && resp.StatusCode < 500 {
				return &apiErr
			}
			lastErr = &apiErr
		} else {
			// 兼容无标准错误体的情况
			var genericErr struct {
				Code    int    `json:"code"`
				Message string `json:"message"`
			}
			if err := json.Unmarshal(respBodyBytes, &genericErr); err == nil && genericErr.Code != 0 {
				apiErr = ICCOAAPIError{Code: genericErr.Code, Message: genericErr.Message}
				if resp.StatusCode >= 400 && resp.StatusCode < 500 {
					return &apiErr
				}
				lastErr = &apiErr
			} else {
				lastErr = fmt.Errorf("ICCOA HTTP %d: %s", resp.StatusCode, string(respBodyBytes))
			}
		}

		// 5xx 错误可重试
		if resp.StatusCode < 500 {
			break
		}
	}

	return fmt.Errorf("ICCOA request failed after %d retries: %w", c.config.RetryCount, lastErr)
}

// ─── S2S API 方法 (§13.5) ──────────────────────────────────

// GenSession 生成分享 sessionId (share/genSession)
func (c *ICCOAClient) GenSession(ctx context.Context, req *ICCOAGenSessionRequest) (*ICCOAGenSessionResponse, error) {
	c.logger.Info("S2S GenSession",
		zap.String("key_id", req.KeyID),
		zap.String("from_user", req.FromUserID),
	)

	s2sReq := c.buildGenSessionRequest(req)
	var resp ICCOAGenSessionResponse
	if err := c.doRequest(ctx, http.MethodPost, "/share/genSession", s2sReq, &resp); err != nil {
		return nil, err
	}
	return &resp, nil
}

// buildGenSessionRequest 补充默认字段
func (c *ICCOAClient) buildGenSessionRequest(req *ICCOAGenSessionRequest) *ICCOAGenSessionRequest {
	if req.DeviceOEMID == "" {
		req.DeviceOEMID = c.config.DeviceOEMID
	}
	if req.VehicleOEMID == "" {
		req.VehicleOEMID = c.config.VehicleOEMID
	}
	return req
}

// GetMidCsr 请求中间分享证书 CSR (share/getMidCsr)
func (c *ICCOAClient) GetMidCsr(ctx context.Context, req *ICCOAGetMidCsrRequest) (*ICCOAGetMidCsrResponse, error) {
	c.logger.Info("S2S GetMidCsr", zap.String("session_id", req.SessionID))

	var resp ICCOAGetMidCsrResponse
	if err := c.doRequest(ctx, http.MethodPost, "/share/getMidCsr", req, &resp); err != nil {
		return nil, err
	}
	return &resp, nil
}

// PutMidCert 签发后的中间分享证书上传 (share/putMidCert)
func (c *ICCOAClient) PutMidCert(ctx context.Context, req *ICCOAPutMidCertRequest) (*ICCOAPutMidCertResponse, error) {
	c.logger.Info("S2S PutMidCert", zap.String("session_id", req.SessionID))

	var resp ICCOAPutMidCertResponse
	if err := c.doRequest(ctx, http.MethodPost, "/share/putMidCert", req, &resp); err != nil {
		return nil, err
	}
	return &resp, nil
}

// Sign 签发好友钥匙 (share/sign)
func (c *ICCOAClient) Sign(ctx context.Context, req *ICCOASignRequest) (*ICCOASignResponse, error) {
	c.logger.Info("S2S Sign", zap.String("session_id", req.SessionID))

	var resp ICCOASignResponse
	if err := c.doRequest(ctx, http.MethodPost, "/share/sign", req, &resp); err != nil {
		return nil, err
	}
	return &resp, nil
}

// CancelShare 撤销分享 (share/cancel)
func (c *ICCOAClient) CancelShare(ctx context.Context, req *ICCOACancelShareRequest) error {
	c.logger.Info("S2S CancelShare", zap.String("key_id", req.KeyID))

	return c.doRequest(ctx, http.MethodPost, "/share/cancel", req, nil)
}

// TrackKey 注册钥匙 (trackKey)
func (c *ICCOAClient) TrackKey(ctx context.Context, req *ICCOATrackKeyRequest) (*ICCOATrackKeyResponse, error) {
	c.logger.Info("S2S TrackKey",
		zap.String("key_id", req.KeyID),
		zap.String("vehicle_id", req.VehicleID),
	)

	var resp ICCOATrackKeyResponse
	if err := c.doRequest(ctx, http.MethodPost, "/trackKey", req, &resp); err != nil {
		return nil, err
	}
	return &resp, nil
}

// ManageKey 钥匙状态管理 (manageKey)
func (c *ICCOAClient) ManageKey(ctx context.Context, req *ICCOAManageKeyRequest) (*ICCOAManageKeyResponse, error) {
	c.logger.Info("S2S ManageKey",
		zap.String("key_id", req.KeyID),
		zap.String("action", req.Action),
	)

	var resp ICCOAManageKeyResponse
	if err := c.doRequest(ctx, http.MethodPost, "/manageKey", req, &resp); err != nil {
		return nil, err
	}
	return &resp, nil
}

// NotifyKeyEvent 钥匙事件通知 (notifyKeyEvent)
func (c *ICCOAClient) NotifyKeyEvent(ctx context.Context, req *ICCOANotifyKeyEventRequest) error {
	c.logger.Info("S2S NotifyKeyEvent",
		zap.String("key_id", req.KeyID),
		zap.String("event_type", req.EventType),
	)

	return c.doRequest(ctx, http.MethodPost, "/notifyKeyEvent", req, nil)
}

// GetVehicleProfile 获取钥匙配置信息 (getVehicleProfile)
func (c *ICCOAClient) GetVehicleProfile(ctx context.Context, req *ICCOAGetVehicleProfileRequest) (*ICCOAGetVehicleProfileResponse, error) {
	c.logger.Info("S2S GetVehicleProfile", zap.String("vehicle_id", req.VehicleID))

	var resp ICCOAGetVehicleProfileResponse
	if err := c.doRequest(ctx, http.MethodGet, "/getVehicleProfile?vehicleId="+req.VehicleID, nil, &resp); err != nil {
		return nil, err
	}
	return &resp, nil
}

// HealthCheck 车服务器健康检查 (healthCheck)
func (c *ICCOAClient) HealthCheck(ctx context.Context) (*ICCOAHealthResponse, error) {
	var resp ICCOAHealthResponse
	if err := c.doRequest(ctx, http.MethodGet, "/healthCheck", nil, &resp); err != nil {
		return nil, err
	}
	return &resp, nil
}

// GetSar 获取分享证明请求 (share/getSar, 非CA模式)
func (c *ICCOAClient) GetSar(ctx context.Context, req *ICCOAGetSarRequest) (*ICCOAGetSarResponse, error) {
	c.logger.Info("S2S GetSar", zap.String("session_id", req.SessionID))

	var resp ICCOAGetSarResponse
	if err := c.doRequest(ctx, http.MethodPost, "/share/getSar", req, &resp); err != nil {
		return nil, err
	}
	return &resp, nil
}

// PutSharingAttestation 签发分享证明 (share/putSharingAttestation, 非CA模式)
func (c *ICCOAClient) PutSharingAttestation(ctx context.Context, req *ICCOAPutSharingAttestationRequest) (*ICCOAPutSharingAttestationResponse, error) {
	c.logger.Info("S2S PutSharingAttestation", zap.String("session_id", req.SessionID))

	var resp ICCOAPutSharingAttestationResponse
	if err := c.doRequest(ctx, http.MethodPost, "/share/putSharingAttestation", req, &resp); err != nil {
		return nil, err
	}
	return &resp, nil
}
