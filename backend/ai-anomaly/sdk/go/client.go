package anomaly

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"time"
)

const (
	DefaultBaseURL   = "http://localhost:8000"
	DefaultTimeout   = 30 * time.Second
	DefaultMaxRetries = 3
)

// Client AI异常检测客户端
type Client struct {
	baseURL    string
	httpClient *http.Client
	apiKey     string
	maxRetries int
}

// ClientOption 客户端配置选项
type ClientOption func(*Client)

// WithBaseURL 设置基础URL
func WithBaseURL(url string) ClientOption {
	return func(c *Client) {
		c.baseURL = url
	}
}

// WithHTTPClient 设置自定义HTTP客户端
func WithHTTPClient(client *http.Client) ClientOption {
	return func(c *Client) {
		c.httpClient = client
	}
}

// WithAPIKey 设置API Key
func WithAPIKey(key string) ClientOption {
	return func(c *Client) {
		c.apiKey = key
	}
}

// WithMaxRetries 设置最大重试次数
func WithMaxRetries(retries int) ClientOption {
	return func(c *Client) {
		c.maxRetries = retries
	}
}

// NewClient 创建新的AI异常检测客户端
func NewClient(opts ...ClientOption) *Client {
	c := &Client{
		baseURL:    DefaultBaseURL,
		httpClient: &http.Client{Timeout: DefaultTimeout},
		maxRetries: DefaultMaxRetries,
	}
	
	for _, opt := range opts {
		opt(c)
	}
	
	return c
}

// doRequest 执行HTTP请求 (带重试机制)
func (c *Client) doRequest(ctx context.Context, method, path string, body interface{}) (*http.Response, error) {
	var lastErr error
	
	for attempt := 0; attempt <= c.maxRetries; attempt++ {
		if attempt > 0 {
			// 指数退避: 2^attempt 秒
			backoff := time.Duration(1<<uint(attempt-1)) * time.Second
			select {
			case <-time.After(backoff):
			case <-ctx.Done():
				return nil, ctx.Err()
			}
		}
		
		reqURL, err := url.JoinPath(c.baseURL, path)
		if err != nil {
			return nil, fmt.Errorf("invalid URL: %w", err)
		}
		
		var bodyReader io.Reader
		if body != nil {
			jsonBody, err := json.Marshal(body)
			if err != nil {
				return nil, fmt.Errorf("marshal body failed: %w", err)
			}
			bodyReader = bytes.NewReader(jsonBody)
		}
		
		req, err := http.NewRequestWithContext(ctx, method, reqURL, bodyReader)
		if err != nil {
			return nil, fmt.Errorf("create request failed: %w", err)
		}
		
		req.Header.Set("Content-Type", "application/json")
		if c.apiKey != "" {
			req.Header.Set("X-API-Key", c.apiKey)
		}
		
		resp, err := c.httpClient.Do(req)
		if err != nil {
			lastErr = err
			continue // 重试
		}
		
		if resp.StatusCode >= 500 {
			// 服务器错误，可重试
			resp.Body.Close()
			lastErr = fmt.Errorf("server error: %d", resp.StatusCode)
			continue
		}
		
		return resp, nil
	}
	
	return nil, fmt.Errorf("max retries exceeded: %w", lastErr)
}

// DetectKey 检测钥匙异常
func (c *Client) DetectKey(ctx context.Context, req *KeyDetectionRequest) (*DetectionResponse, error) {
	resp, err := c.doRequest(ctx, "POST", "/api/v1/detect/key", req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	
	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("unexpected status: %d", resp.StatusCode)
	}
	
	var result DetectionResponse
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return nil, fmt.Errorf("decode response failed: %w", err)
	}
	
	return &result, nil
}

// DetectVehicle 检测车辆异常
func (c *Client) DetectVehicle(ctx context.Context, req *VehicleDetectionRequest) (*DetectionResponse, error) {
	resp, err := c.doRequest(ctx, "POST", "/api/v1/detect/vehicle", req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	
	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("unexpected status: %d", resp.StatusCode)
	}
	
	var result DetectionResponse
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return nil, fmt.Errorf("decode response failed: %w", err)
	}
	
	return &result, nil
}

// DetectUserBehavior 检测用户行为异常
func (c *Client) DetectUserBehavior(ctx context.Context, req *UserBehaviorDetectionRequest) (*DetectionResponse, error) {
	resp, err := c.doRequest(ctx, "POST", "/api/v1/detect/user", req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	
	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("unexpected status: %d", resp.StatusCode)
	}
	
	var result DetectionResponse
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return nil, fmt.Errorf("decode response failed: %w", err)
	}
	
	return &result, nil
}

// DetectBatch 批量检测
func (c *Client) DetectBatch(ctx context.Context, req *BatchDetectionRequest) (*BatchDetectionResponse, error) {
	resp, err := c.doRequest(ctx, "POST", "/api/v1/detect/batch", req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	
	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("unexpected status: %d", resp.StatusCode)
	}
	
	var result BatchDetectionResponse
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return nil, fmt.Errorf("decode response failed: %w", err)
	}
	
	return &result, nil
}

// GetAnomalies 获取异常记录列表
func (c *Client) GetAnomalies(ctx context.Context, anomalyType string, params *AnomalyQueryParams) (*AnomalyListResponse, error) {
	path := fmt.Sprintf("/api/v1/anomalies/%s", anomalyType)
	
	if params != nil {
		query := url.Values{}
		if params.Limit > 0 {
			query.Set("limit", fmt.Sprintf("%d", params.Limit))
		}
		if params.Offset > 0 {
			query.Set("offset", fmt.Sprintf("%d", params.Offset))
		}
		if params.StartTime != "" {
			query.Set("start_time", params.StartTime)
		}
		if params.EndTime != "" {
			query.Set("end_time", params.EndTime)
		}
		if params.Severity != "" {
			query.Set("severity", params.Severity)
		}
		path = path + "?" + query.Encode()
	}
	
	resp, err := c.doRequest(ctx, "GET", path, nil)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	
	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("unexpected status: %d", resp.StatusCode)
	}
	
	var result AnomalyListResponse
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return nil, fmt.Errorf("decode response failed: %w", err)
	}
	
	return &result, nil
}

// GetStatsOverview 获取统计概览
func (c *Client) GetStatsOverview(ctx context.Context) (*StatsOverviewResponse, error) {
	resp, err := c.doRequest(ctx, "GET", "/api/v1/stats/overview", nil)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	
	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("unexpected status: %d", resp.StatusCode)
	}
	
	var result StatsOverviewResponse
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return nil, fmt.Errorf("decode response failed: %w", err)
	}
	
	return &result, nil
}

// TrainModels 训练模型
func (c *Client) TrainModels(ctx context.Context, req *TrainRequest) (*TrainResponse, error) {
	resp, err := c.doRequest(ctx, "POST", "/api/v1/models/train", req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	
	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("unexpected status: %d", resp.StatusCode)
	}
	
	var result TrainResponse
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return nil, fmt.Errorf("decode response failed: %w", err)
	}
	
	return &result, nil
}

// Health 健康检查
func (c *Client) Health(ctx context.Context) (*HealthResponse, error) {
	resp, err := c.doRequest(ctx, "GET", "/api/v1/health", nil)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	
	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("unexpected status: %d", resp.StatusCode)
	}
	
	var result HealthResponse
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return nil, fmt.Errorf("decode response failed: %w", err)
	}
	
	return &result, nil
}
