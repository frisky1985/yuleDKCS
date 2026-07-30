package relay

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"time"
)

// ─── FCMPusher — Android FCM v1 HTTP API ──────────────────────

// FCMConfig FCM 推送配置
type FCMConfig struct {
	ProjectID      string // Firebase 项目 ID
	CredentialsJSON string // 服务账号 JSON 文件路径（可选，默认用 ADC）
}

// FCMPusher 通过 Firebase Cloud Messaging v1 API 向 Android 设备发送推送
type FCMPusher struct {
	config  FCMConfig
	client  *http.Client
}

func NewFCMPusher(config FCMConfig) *FCMPusher {
	return &FCMPusher{
		config: config,
		client: &http.Client{Timeout: 10 * time.Second},
	}
}

// fcmMessage FCM v1 API 请求体
type fcmMessage struct {
	Message struct {
		Token        string            `json:"token"`
		Notification *fcmNotification  `json:"notification,omitempty"`
		Data         map[string]string `json:"data,omitempty"`
	} `json:"message"`
}

type fcmNotification struct {
	Title string `json:"title"`
	Body  string `json:"body"`
}

// fcmErrorBody FCM v1 API 错误响应体
type fcmErrorBody struct {
	Code    int    `json:"code"`
	Message string `json:"message"`
	Status  string `json:"status"`
}

// fcmError FCM v1 API 错误响应
type fcmError struct {
	Err fcmErrorBody `json:"error"`
}

func (f *fcmError) Error() string {
	return fmt.Sprintf("FCM error [%d]: %s (%s)", f.Err.Code, f.Err.Message, f.Err.Status)
}

// Notify 发送 FCM 推送通知
func (p *FCMPusher) Notify(ctx context.Context, msg PushMessage) error {
	if p.config.ProjectID == "" {
		return fmt.Errorf("FCM: project_id not configured")
	}

	body := fcmMessage{}
	body.Message.Token = msg.Token
	body.Message.Notification = &fcmNotification{
		Title: msg.Title,
		Body:  msg.Body,
	}
	// 透传数据
	if len(msg.Data) > 0 {
		body.Message.Data = msg.Data
	} else {
		body.Message.Data = map[string]string{
			"mailbox_id": msg.MailboxID,
			"sender":     "relay",
		}
	}

	payload, err := json.Marshal(body)
	if err != nil {
		return fmt.Errorf("FCM marshal: %w", err)
	}

	url := fmt.Sprintf("https://fcm.googleapis.com/v1/projects/%s/messages:send", p.config.ProjectID)
	req, err := http.NewRequestWithContext(ctx, http.MethodPost, url, bytes.NewReader(payload))
	if err != nil {
		return fmt.Errorf("FCM request: %w", err)
	}
	req.Header.Set("Content-Type", "application/json")

	// 认证：使用 Google Application Default Credentials
	// 生产环境通过 GOOGLE_APPLICATION_CREDENTIALS 环境变量或 GCP 元数据服务获取
	token, err := p.getAccessToken(ctx)
	if err != nil {
		return fmt.Errorf("FCM auth: %w", err)
	}
	req.Header.Set("Authorization", "Bearer "+token)

	resp, err := p.client.Do(req)
	if err != nil {
		return fmt.Errorf("FCM request failed: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode >= 200 && resp.StatusCode < 300 {
		return nil
	}

	respBody, _ := io.ReadAll(resp.Body)
	var fcmErr fcmError
	if err := json.Unmarshal(respBody, &fcmErr); err == nil && fcmErr.Err.Message != "" {
		return &fcmErr
	}
	return fmt.Errorf("FCM HTTP %d: %s", resp.StatusCode, string(respBody))
}

// getAccessToken 获取 Google OAuth2 访问令牌
// 使用 Application Default Credentials（ADC）
// 如果配置了 CredentialsJSON，则从文件读取服务账号
func (p *FCMPusher) getAccessToken(ctx context.Context) (string, error) {
	// 注意：生产部署时通过 GOOGLE_APPLICATION_CREDENTIALS 环境变量指定服务账号 JSON 路径
	// 或直接在 GCP/GKE 上运行（元数据服务自动提供令牌）
	// 这里使用标准的 GCP 凭证获取方式
	tokenURL := "http://metadata.google.internal/computeMetadata/v1/instance/service-accounts/default/token?scopes=https://www.googleapis.com/auth/firebase.messaging"

	req, err := http.NewRequestWithContext(ctx, http.MethodGet, tokenURL, nil)
	if err != nil {
		return "", fmt.Errorf("token request: %w", err)
	}
	req.Header.Set("Metadata-Flavor", "Google")

	resp, err := p.client.Do(req)
	if err != nil {
		return "", fmt.Errorf("metadata token failed (use ADC or set GOOGLE_APPLICATION_CREDENTIALS): %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != 200 {
		return "", fmt.Errorf("metadata token HTTP %d", resp.StatusCode)
	}

	var tokenResp struct {
		AccessToken string `json:"access_token"`
		ExpiresIn   int    `json:"expires_in"`
	}
	if err := json.NewDecoder(resp.Body).Decode(&tokenResp); err != nil {
		return "", fmt.Errorf("token decode: %w", err)
	}

	return tokenResp.AccessToken, nil
}
