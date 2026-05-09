package testutils

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/gin-gonic/gin"
)

// HTTPTestHelper HTTP 测试辅助工具
type HTTPTestHelper struct {
	Router *gin.Engine
}

// NewHTTPTestHelper 创建新的 HTTP 测试辅助工具
func NewHTTPTestHelper() *HTTPTestHelper {
	gin.SetMode(gin.TestMode)
	return &HTTPTestHelper{
		Router: gin.New(),
	}
}

// Request 发送 HTTP 请求
func (h *HTTPTestHelper) Request(method, path string, body interface{}, headers map[string]string) *httptest.ResponseRecorder {
	var bodyBytes []byte
	if body != nil {
		var err error
		bodyBytes, err = json.Marshal(body)
		if err != nil {
			panic(err)
		}
	}

	req := httptest.NewRequest(method, path, bytes.NewBuffer(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	
	for key, value := range headers {
		req.Header.Set(key, value)
	}

	w := httptest.NewRecorder()
	h.Router.ServeHTTP(w, req)
	return w
}

// Get 发送 GET 请求
func (h *HTTPTestHelper) Get(path string, headers map[string]string) *httptest.ResponseRecorder {
	return h.Request(http.MethodGet, path, nil, headers)
}

// Post 发送 POST 请求
func (h *HTTPTestHelper) Post(path string, body interface{}, headers map[string]string) *httptest.ResponseRecorder {
	return h.Request(http.MethodPost, path, body, headers)
}

// Put 发送 PUT 请求
func (h *HTTPTestHelper) Put(path string, body interface{}, headers map[string]string) *httptest.ResponseRecorder {
	return h.Request(http.MethodPut, path, body, headers)
}

// Delete 发送 DELETE 请求
func (h *HTTPTestHelper) Delete(path string, headers map[string]string) *httptest.ResponseRecorder {
	return h.Request(http.MethodDelete, path, nil, headers)
}

// Patch 发送 PATCH 请求
func (h *HTTPTestHelper) Patch(path string, body interface{}, headers map[string]string) *httptest.ResponseRecorder {
	return h.Request(http.MethodPatch, path, body, headers)
}

// AssertStatus 断言 HTTP 状态码
func AssertStatus(t *testing.T, w *httptest.ResponseRecorder, expected int) {
	if w.Code != expected {
		t.Errorf("期望状态码 %d, 实际 %d", expected, w.Code)
	}
}

// AssertJSON 断言 JSON 响应
func AssertJSON(t *testing.T, w *httptest.ResponseRecorder, v interface{}) {
	contentType := w.Header().Get("Content-Type")
	if contentType != "application/json; charset=utf-8" {
		t.Errorf("期望 Content-Type 为 application/json; charset=utf-8, 实际为 %s", contentType)
	}

	if err := json.Unmarshal(w.Body.Bytes(), v); err != nil {
		t.Errorf("解析 JSON 响应失败: %v", err)
	}
}

// ParseJSONResponse 解析 JSON 响应到 map
func ParseJSONResponse(t *testing.T, w *httptest.ResponseRecorder) map[string]interface{} {
	var result map[string]interface{}
	if err := json.Unmarshal(w.Body.Bytes(), &result); err != nil {
		t.Fatalf("解析 JSON 响应失败: %v", err)
	}
	return result
}

// JSONRequest 创建 JSON 请求
func JSONRequest(method, path string, body interface{}) *http.Request {
	var bodyBytes []byte
	if body != nil {
		var err error
		bodyBytes, err = json.Marshal(body)
		if err != nil {
			panic(err)
		}
	}

	req := httptest.NewRequest(method, path, bytes.NewBuffer(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	return req
}

// SetAuthHeader 设置认证头
func SetAuthHeader(req *http.Request, token string) {
	req.Header.Set("Authorization", "Bearer "+token)
}

// GetAuthToken 从响应中获取 token
func GetAuthToken(t *testing.T, w *httptest.ResponseRecorder) string {
	var result map[string]interface{}
	if err := json.Unmarshal(w.Body.Bytes(), &result); err != nil {
		t.Fatalf("解析响应失败: %v", err)
	}
	
	token, ok := result["token"].(string)
	if !ok {
		t.Fatal("响应中没有 token")
	}
	return token
}
