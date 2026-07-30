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

// ICCEClient ICCE 厂商 S2S HTTP 客户端
// 与 ICCOA 架构相同：Hub 直连车厂 OEM S2S 端点，无 Relay Server
// 使用双向 TLS (mTLS) + REST/JSON
type ICCEClient struct {
	config   ICCEEndpoint
	client   *http.Client
	logger   *zap.Logger
	vendorID string // 厂商标识，如 "huawei"
}

// ICCEClientOption 客户端配置选项
type ICCEClientOption func(*ICCEClient)

// WithTLSCert 配置双向 TLS (mTLS) 证书
func WithTLSCert(certFile, keyFile, caFile string) ICCEClientOption {
	return func(c *ICCEClient) {
		cert, err := tls.LoadX509KeyPair(certFile, keyFile)
		if err != nil {
			c.logger.Warn("ICCE mTLS cert load failed, using default TLS", zap.Error(err))
			return
		}

		tlsConfig := &tls.Config{
			Certificates: []tls.Certificate{cert},
			MinVersion:   tls.VersionTLS12,
		}

		if caFile != "" {
			caCert, err := os.ReadFile(caFile)
			if err != nil {
				c.logger.Warn("ICCE CA cert load failed", zap.Error(err))
			} else {
				caPool := x509.NewCertPool()
				if caPool.AppendCertsFromPEM(caCert) {
					tlsConfig.RootCAs = caPool
				}
			}
		}

		c.client.Transport = &http.Transport{
			TLSClientConfig: tlsConfig,
			MaxIdleConns:    20,
			IdleConnTimeout: 90 * time.Second,
		}
	}
}

// WithTimeout 配置 HTTP 超时
func WithTimeout(timeout time.Duration) ICCEClientOption {
	return func(c *ICCEClient) {
		c.client.Timeout = timeout
	}
}

// NewICCEClient 创建 ICCE S2S 客户端
func NewICCEClient(vendorID string, endpoint ICCEEndpoint, logger *zap.Logger, opts ...ICCEClientOption) *ICCEClient {
	c := &ICCEClient{
		config:   endpoint,
		client:   &http.Client{Timeout: endpoint.Timeout},
		logger:   logger.With(zap.String("vendor", vendorID), zap.String("component", "icce_s2s")),
		vendorID: vendorID,
	}

	for _, opt := range opts {
		opt(c)
	}

	return c
}

// ─── 通用请求方法 ──────────────────────────────────────────

// doRequest 发送 ICCE S2S HTTP 请求
func (c *ICCEClient) doRequest(ctx context.Context, method, path string, reqBody, respBody interface{}) error {
	url := c.config.BaseURL + path

	var bodyReader io.Reader
	if reqBody != nil {
		data, err := json.Marshal(reqBody)
		if err != nil {
			return fmt.Errorf("ICCE marshal request: %w", err)
		}
		bodyReader = bytes.NewReader(data)
	}

	req, err := http.NewRequestWithContext(ctx, method, url, bodyReader)
	if err != nil {
		return fmt.Errorf("ICCE create request: %w", err)
	}

	// ICCE 通用请求头
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Accept", "application/json")
	req.Header.Set("X-Vendor-ID", c.vendorID)
	req.Header.Set("X-Request-Timestamp", fmt.Sprintf("%d", time.Now().UnixMilli()))
	req.Header.Set("User-Agent", "YuleDKCS-Hub/1.0 ICCE-S2S/1.0")

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
			lastErr = fmt.Errorf("ICCE request attempt %d: %w", attempt+1, err)
			continue
		}

		respBodyBytes, err := io.ReadAll(resp.Body)
		resp.Body.Close()
		if err != nil {
			lastErr = fmt.Errorf("ICCE read response: %w", err)
			continue
		}

		if resp.StatusCode >= 200 && resp.StatusCode < 300 {
			if respBody != nil && len(respBodyBytes) > 0 {
				if err := json.Unmarshal(respBodyBytes, respBody); err != nil {
					return fmt.Errorf("ICCE unmarshal response: %w", err)
				}
			}
			return nil
		}

		// 解析 ICCE API 错误
		var apiErr ICCEAPIError
		if err := json.Unmarshal(respBodyBytes, &apiErr); err == nil && apiErr.Code != 0 {
			// 4xx 错误不重试
			if resp.StatusCode >= 400 && resp.StatusCode < 500 {
				return &apiErr
			}
			lastErr = &apiErr
		} else {
			lastErr = fmt.Errorf("ICCE HTTP %d: %s", resp.StatusCode, string(respBodyBytes))
		}

		// 5xx 错误可重试
		if resp.StatusCode < 500 {
			break
		}
	}

	return fmt.Errorf("ICCE request failed after %d retries: %w", c.config.RetryCount, lastErr)
}

// ─── S2S API 方法 ──────────────────────────────────────────

// BindKey 密钥绑定
func (c *ICCEClient) BindKey(ctx context.Context, req *ICCEBindRequest) (*ICCEBindResponse, error) {
	c.logger.Info("S2S BindKey",
		zap.String("vehicle_id", req.VehicleID),
		zap.String("user_id", req.UserID),
	)

	var resp ICCEBindResponse
	if err := c.doRequest(ctx, http.MethodPost, "/bind", req, &resp); err != nil {
		return nil, err
	}
	return &resp, nil
}

// UnbindKey 密钥解绑
func (c *ICCEClient) UnbindKey(ctx context.Context, keyID string) error {
	c.logger.Info("S2S UnbindKey", zap.String("key_id", keyID))

	req := &ICCEUnbindRequest{KeyID: keyID}
	return c.doRequest(ctx, http.MethodPost, "/unbind", req, nil)
}

// RevokeKey 密钥撤销
func (c *ICCEClient) RevokeKey(ctx context.Context, keyID, reason string) error {
	c.logger.Info("S2S RevokeKey", zap.String("key_id", keyID), zap.String("reason", reason))

	req := &ICCERevokeRequest{KeyID: keyID, Reason: reason}
	return c.doRequest(ctx, http.MethodPost, "/revoke", req, nil)
}

// ShareKey 密钥分享
func (c *ICCEClient) ShareKey(ctx context.Context, req *ICCEShareRequest) (*ICCEShareResponse, error) {
	c.logger.Info("S2S ShareKey", zap.String("key_id", req.KeyID))

	var resp ICCEShareResponse
	if err := c.doRequest(ctx, http.MethodPost, "/share", req, &resp); err != nil {
		return nil, err
	}
	return &resp, nil
}

// AcceptShare 接受分享
func (c *ICCEClient) AcceptShare(ctx context.Context, req *ICCEAcceptShareRequest) (*ICCEBindResponse, error) {
	c.logger.Info("S2S AcceptShare",
		zap.String("share_code", req.ShareCode),
		zap.String("user_id", req.UserID),
	)

	var resp ICCEBindResponse
	if err := c.doRequest(ctx, http.MethodPost, "/share/accept", req, &resp); err != nil {
		return nil, err
	}
	return &resp, nil
}

// HealthCheck 厂商健康检查
func (c *ICCEClient) HealthCheck(ctx context.Context) (*ICCEHealthResponse, error) {
	var resp ICCEHealthResponse
	if err := c.doRequest(ctx, http.MethodGet, "/health", nil, &resp); err != nil {
		return nil, err
	}
	return &resp, nil
}
