package relay

import (
	"bytes"
	"context"
	"crypto/ecdsa"
	"crypto/x509"
	"encoding/json"
	"encoding/pem"
	"fmt"
	"io"
	"net/http"
	"time"

	"github.com/golang-jwt/jwt/v5"
	"golang.org/x/net/http2"
)

// ─── APNsPusher — iOS APNs HTTP/2 API ─────────────────────────

// APNsConfig APNs 推送配置
type APNsConfig struct {
	KeyID      string // APNs 认证密钥 ID（Apple Developer 生成）
	TeamID     string // Apple Developer Team ID
	BundleID   string // App Bundle ID，如 "com.yuletech.digitalkey"
	AuthKey    string // APNs 认证密钥文件路径 (.p8 文件)
	Production bool   // true = 生产环境, false = 开发沙箱
}

// APNsPusher 通过 Apple Push Notification Service (HTTP/2) 向 iOS 设备发送推送
type APNsPusher struct {
	config APNsConfig
	client *http.Client
}

func NewAPNsPusher(config APNsConfig) (*APNsPusher, error) {
	transport := &http.Transport{
		MaxIdleConns:    10,
		IdleConnTimeout: 30 * time.Second,
	}
	// 启用 HTTP/2（APNs 要求 HTTP/2）
	if err := http2.ConfigureTransport(transport); err != nil {
		return nil, fmt.Errorf("APNs HTTP/2 configure: %w", err)
	}

	client := &http.Client{
		Transport: transport,
		Timeout:   10 * time.Second,
	}

	return &APNsPusher{
		config: config,
		client: client,
	}, nil
}

// apnsPayload APNs 通知负载
type apnsPayload struct {
	Aps     apnsAps           `json:"aps"`
	Data    map[string]string `json:"data,omitempty"`
}

type apnsAps struct {
	Alert            apnsAlert `json:"alert"`
	ContentAvailable int       `json:"content-available,omitempty"`
	MutableContent   int       `json:"mutable-content,omitempty"`
}

type apnsAlert struct {
	Title string `json:"title"`
	Body  string `json:"body"`
}

// apnsResponse APNs 响应
type apnsResponse struct {
	Reason string `json:"reason"`
}

func (r *apnsResponse) Error() string {
	return fmt.Sprintf("APNs: %s", r.Reason)
}

// Notify 发送 APNs 推送通知
func (p *APNsPusher) Notify(ctx context.Context, msg PushMessage) error {
	if p.config.KeyID == "" || p.config.TeamID == "" {
		return fmt.Errorf("APNs: key_id and team_id required")
	}

	// 构建 payload
	payload := apnsPayload{
		Aps: apnsAps{
			Alert: apnsAlert{
				Title: msg.Title,
				Body:  msg.Body,
			},
			ContentAvailable: 1,
			MutableContent:   1,
		},
		Data: msg.Data,
	}
	if payload.Data == nil {
		payload.Data = map[string]string{
			"mailbox_id": msg.MailboxID,
			"sender":     "relay",
		}
	}

	body, err := json.Marshal(payload)
	if err != nil {
		return fmt.Errorf("APNs marshal: %w", err)
	}

	// 构建请求
	host := "api.push.apple.com"
	if !p.config.Production {
		host = "api.development.push.apple.com"
	}
	url := fmt.Sprintf("https://%s/3/device/%s", host, msg.Token)

	req, err := http.NewRequestWithContext(ctx, http.MethodPost, url, bytes.NewReader(body))
	if err != nil {
		return fmt.Errorf("APNs request: %w", err)
	}
	req.Header.Set("Content-Type", "application/json; charset=utf-8")
	req.Header.Set("apns-topic", p.config.BundleID)
	req.Header.Set("apns-push-type", "alert")
	req.Header.Set("apns-priority", "10")

	// 生成 JWT 认证令牌
	jwtToken, err := p.generateAuthToken()
	if err != nil {
		return fmt.Errorf("APNs JWT: %w", err)
	}
	req.Header.Set("authorization", "bearer "+jwtToken)

	resp, err := p.client.Do(req)
	if err != nil {
		return fmt.Errorf("APNs request failed: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode == 200 {
		return nil
	}

	// 解析错误
	respBody, _ := io.ReadAll(resp.Body)
	var apnsErr apnsResponse
	if err := json.Unmarshal(respBody, &apnsErr); err == nil && apnsErr.Reason != "" {
		return &apnsErr
	}
	return fmt.Errorf("APNs HTTP %d: %s", resp.StatusCode, string(respBody))
}

// generateAuthToken 生成 APNs JWT 认证令牌
// 使用 ES256 (ECDSA P-256) 签名，有效期 30 分钟
func (p *APNsPusher) generateAuthToken() (string, error) {
	now := time.Now()
	claims := &jwt.RegisteredClaims{
		Issuer:    p.config.TeamID,
		IssuedAt:  jwt.NewNumericDate(now),
		ExpiresAt: jwt.NewNumericDate(now.Add(30 * time.Minute)),
	}

	// 使用 golang-jwt 库生成 ES256 签名的 JWT
	token := jwt.NewWithClaims(jwt.SigningMethodES256, claims)
	token.Header["kid"] = p.config.KeyID

	// 解析 .p8 私钥文件
	keyBytes := []byte(p.config.AuthKey)
	// 如果 AuthKey 是文件路径，需在外部加载
	block, _ := pem.Decode(keyBytes)
	if block == nil {
		return "", fmt.Errorf("APNs: failed to decode .p8 key (not PEM format)")
	}

	privKey, err := x509.ParsePKCS8PrivateKey(block.Bytes)
	if err != nil {
		return "", fmt.Errorf("APNs: parse .p8 key: %w", err)
	}

	ecdsaKey, ok := privKey.(*ecdsa.PrivateKey)
	if !ok {
		return "", fmt.Errorf("APNs: .p8 key is not ECDSA P-256")
	}

	signedToken, err := token.SignedString(ecdsaKey)
	if err != nil {
		return "", fmt.Errorf("APNs: sign JWT: %w", err)
	}

	return signedToken, nil
}
