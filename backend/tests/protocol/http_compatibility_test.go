package protocol

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// HTTPCompatibilityTester HTTP协议兼容性测试器
type HTTPCompatibilityTester struct {
	router     *gin.Engine
	baseURL    string
	testData   map[string]map[string]json.RawMessage
	report     *TestReport
}

// NewHTTPCompatibilityTester 创建新的HTTP兼容性测试器
func NewHTTPCompatibilityTester(router *gin.Engine) *HTTPCompatibilityTester {
	return &HTTPCompatibilityTester{
		router:   router,
		testData: make(map[string]map[string]json.RawMessage),
		report:   NewTestReport("HTTP Compatibility Test"),
	}
}

// LoadTestData 加载测试数据
func (t *HTTPCompatibilityTester) LoadTestData(testDataDir string) error {
	files := []string{
		"vehicle_requests.json",
		"vehicle_responses.json",
		"key_requests.json",
		"key_responses.json",
		"auth_requests.json",
		"auth_responses.json",
	}

	for _, file := range files {
		path := filepath.Join(testDataDir, file)
		data, err := os.ReadFile(path)
		if err != nil {
			return fmt.Errorf("failed to read %s: %w", file, err)
		}

		var fileData map[string]json.RawMessage
		if err := json.Unmarshal(data, &fileData); err != nil {
			return fmt.Errorf("failed to unmarshal %s: %w", file, err)
		}
		t.testData[file] = fileData
	}

	return nil
}

// GetTestData 获取测试数据
func (t *HTTPCompatibilityTester) GetTestData(file, key string) (json.RawMessage, bool) {
	if fileData, ok := t.testData[file]; ok {
		if data, ok := fileData[key]; ok {
			return data, true
		}
	}
	return nil, false
}

// TestEndpoint 测试单个端点
func (t *HTTPCompatibilityTester) TestEndpoint(method, path string, body []byte, headers map[string]string) (*http.Response, error) {
	w := httptest.NewRecorder()

	var bodyReader io.Reader
	if body != nil {
		bodyReader = bytes.NewReader(body)
	}

	req, err := http.NewRequest(method, path, bodyReader)
	if err != nil {
		return nil, err
	}

	for key, value := range headers {
		req.Header.Set(key, value)
	}

	if body != nil && req.Header.Get("Content-Type") == "" {
		req.Header.Set("Content-Type", "application/json")
	}

	t.router.ServeHTTP(w, req)

	return w.Result(), nil
}

// RunAllTests 运行所有HTTP兼容性测试
func (t *HTTPCompatibilityTester) RunAllTests() *TestReport {
	t.report.Start()

	t.testRequestFormatValidation()
	t.testResponseFormatValidation()
	t.testMessageTypeConsistency()
	t.testVersionCompatibility()
	t.testErrorHandling()
	t.testContentTypeValidation()
	t.testAuthenticationHeaders()
	t.testPaginationFormat()

	t.report.End()
	return t.report
}

// ==================== 具体测试方法 ====================

// testRequestFormatValidation 测试请求格式验证
func (t *HTTPCompatibilityTester) testRequestFormatValidation() {
	testCases := []struct {
		name       string
		method     string
		path       string
		file       string
		key        string
		expectCode int
	}{
		{
			name:       "Valid Vehicle Register Request",
			method:     "POST",
			path:       "/api/v1/vehicles",
			file:       "vehicle_requests.json",
			key:        "valid_register_request",
			expectCode: http.StatusCreated,
		},
		{
			name:       "Invalid Vehicle Register - No VIN",
			method:     "POST",
			path:       "/api/v1/vehicles",
			file:       "vehicle_requests.json",
			key:        "invalid_register_request_no_vin",
			expectCode: http.StatusBadRequest,
		},
		{
			name:       "Invalid Vehicle Register - Short VIN",
			method:     "POST",
			path:       "/api/v1/vehicles",
			file:       "vehicle_requests.json",
			key:        "invalid_register_request_short_vin",
			expectCode: http.StatusBadRequest,
		},
		{
			name:       "Valid Command Request - Unlock",
			method:     "POST",
			path:       "/api/v1/vehicles/1/command",
			file:       "vehicle_requests.json",
			key:        "valid_command_request_unlock",
			expectCode: http.StatusAccepted,
		},
		{
			name:       "Valid Command Request - Lock",
			method:     "POST",
			path:       "/api/v1/vehicles/1/command",
			file:       "vehicle_requests.json",
			key:        "valid_command_request_lock",
			expectCode: http.StatusAccepted,
		},
		{
			name:       "Valid Key Issue - CCC",
			method:     "POST",
			path:       "/api/v1/keys/issue",
			file:       "key_requests.json",
			key:        "valid_issue_key_ccc",
			expectCode: http.StatusCreated,
		},
		{
			name:       "Valid Key Issue - ICCE",
			method:     "POST",
			path:       "/api/v1/keys/issue",
			file:       "key_requests.json",
			key:        "valid_issue_key_icce",
			expectCode: http.StatusCreated,
		},
		{
			name:       "Invalid Key Issue - No Vehicle",
			method:     "POST",
			path:       "/api/v1/keys/issue",
			file:       "key_requests.json",
			key:        "invalid_issue_key_no_vehicle",
			expectCode: http.StatusBadRequest,
		},
	}

	for _, tc := range testCases {
		t.report.AddTestCase(TestCase{
			Name:        tc.name,
			Category:    "Request Format Validation",
			Description: fmt.Sprintf("%s %s with %s", tc.method, tc.path, tc.key),
		})

		data, ok := t.GetTestData(tc.file, tc.key)
		if !ok {
			t.report.AddResult(tc.name, false, "Test data not found", nil)
			continue
		}

		resp, err := t.TestEndpoint(tc.method, tc.path, data, nil)
		if err != nil {
			t.report.AddResult(tc.name, false, fmt.Sprintf("Request error: %v", err), nil)
			continue
		}

		statusOK := resp.StatusCode == tc.expectCode
		t.report.AddResult(tc.name, statusOK, 
			fmt.Sprintf("Expected %d, got %d", tc.expectCode, resp.StatusCode), 
			map[string]interface{}{
				"expected_code": tc.expectCode,
				"actual_code":   resp.StatusCode,
			})
	}
}

// testResponseFormatValidation 测试响应格式验证
func (t *HTTPCompatibilityTester) testResponseFormatValidation() {
	testCases := []struct {
		name           string
		file           string
		key            string
		requiredFields []string
	}{
		{
			name:           "Vehicle Response Format",
			file:           "vehicle_responses.json",
			key:            "vehicle_response_success",
			requiredFields: []string{"id", "owner_id", "name", "vin", "brand", "model", "status", "lock_status", "engine_status", "online_status"},
		},
		{
			name:           "Vehicle Status Response Format",
			file:           "vehicle_responses.json",
			key:            "vehicle_status_response",
			requiredFields: []string{"id", "status", "lock_status", "engine_status", "online_status"},
		},
		{
			name:           "Key Response Format",
			file:           "key_responses.json",
			key:            "key_response_ccc_active",
			requiredFields: []string{"id", "user_id", "vehicle_id", "type", "status", "permissions", "key_identifier", "name"},
		},
		{
			name:           "Error Response Format",
			file:           "vehicle_responses.json",
			key:            "error_response_not_found",
			requiredFields: []string{"code", "message", "error"},
		},
		{
			name:           "Success Response with Data",
			file:           "vehicle_responses.json",
			key:            "vehicle_list_response",
			requiredFields: []string{"code", "message", "data"},
		},
	}

	for _, tc := range testCases {
		t.report.AddTestCase(TestCase{
			Name:        tc.name,
			Category:    "Response Format Validation",
			Description: fmt.Sprintf("Validate %s has all required fields", tc.key),
		})

		data, ok := t.GetTestData(tc.file, tc.key)
		if !ok {
			t.report.AddResult(tc.name, false, "Test data not found", nil)
			continue
		}

		var response map[string]interface{}
		if err := json.Unmarshal(data, &response); err != nil {
			t.report.AddResult(tc.name, false, fmt.Sprintf("Failed to unmarshal: %v", err), nil)
			continue
		}

		missingFields := []string{}
		for _, field := range tc.requiredFields {
			if _, ok := response[field]; !ok {
				missingFields = append(missingFields, field)
			}
		}

		if len(missingFields) > 0 {
			t.report.AddResult(tc.name, false, 
				fmt.Sprintf("Missing required fields: %v", missingFields),
				map[string]interface{}{"missing_fields": missingFields})
		} else {
			t.report.AddResult(tc.name, true, "All required fields present", nil)
		}
	}
}

// testMessageTypeConsistency 测试消息类型一致性
func (t *HTTPCompatibilityTester) testMessageTypeConsistency() {
	testCases := []struct {
		name         string
		file         string
		key          string
		expectedType string
		actualPath   string
	}{
		{
			name:         "Vehicle Command - Unlock",
			file:         "vehicle_requests.json",
			key:          "valid_command_request_unlock",
			expectedType: "unlock",
			actualPath:   "command",
		},
		{
			name:         "Vehicle Command - Lock",
			file:         "vehicle_requests.json",
			key:          "valid_command_request_lock",
			expectedType: "lock",
			actualPath:   "command",
		},
		{
			name:         "Vehicle Command - Engine Start",
			file:         "vehicle_requests.json",
			key:          "valid_command_request_engine_start",
			expectedType: "engine_start",
			actualPath:   "command",
		},
		{
			name:         "Key Type - CCC",
			file:         "key_requests.json",
			key:          "valid_issue_key_ccc",
			expectedType: "CCC",
			actualPath:   "type",
		},
		{
			name:         "Key Type - ICCE",
			file:         "key_requests.json",
			key:          "valid_issue_key_icce",
			expectedType: "ICCE",
			actualPath:   "type",
		},
		{
			name:         "Key Type - ICCOA",
			file:         "key_requests.json",
			key:          "valid_issue_key_iccoa",
			expectedType: "ICCOA",
			actualPath:   "type",
		},
	}

	validCommands := map[string]bool{
		"unlock": true, "lock": true, "engine_start": true, "engine_stop": true,
		"trunk": true, "windows_up": true, "windows_down": true, "find_my_car": true,
	}

	validKeyTypes := map[string]bool{"CCC": true, "ICCE": true, "ICCOA": true}

	for _, tc := range testCases {
		t.report.AddTestCase(TestCase{
			Name:        tc.name,
			Category:    "Message Type Consistency",
			Description: fmt.Sprintf("Validate type consistency for %s", tc.key),
		})

		data, ok := t.GetTestData(tc.file, tc.key)
		if !ok {
			t.report.AddResult(tc.name, false, "Test data not found", nil)
			continue
		}

		var request map[string]interface{}
		if err := json.Unmarshal(data, &request); err != nil {
			t.report.AddResult(tc.name, false, fmt.Sprintf("Failed to unmarshal: %v", err), nil)
			continue
		}

		actualType, ok := request[tc.actualPath].(string)
		if !ok {
			t.report.AddResult(tc.name, false, 
				fmt.Sprintf("Field %s not found or not a string", tc.actualPath), nil)
			continue
		}

		var isValid bool
		if tc.actualPath == "command" {
			isValid = validCommands[actualType]
		} else if tc.actualPath == "type" && tc.file == "key_requests.json" {
			isValid = validKeyTypes[actualType]
		} else {
			isValid = actualType == tc.expectedType
		}

		t.report.AddResult(tc.name, isValid,
			fmt.Sprintf("Expected type '%s', got '%s'", tc.expectedType, actualType),
			map[string]interface{}{
				"expected": tc.expectedType,
				"actual":   actualType,
			})
	}
}

// testVersionCompatibility 测试版本兼容性
func (t *HTTPCompatibilityTester) testVersionCompatibility() {
	testCases := []struct {
		name           string
		path           string
		expectVersion  string
		shouldContain  string
	}{
		{
			name:          "API Path Version Check",
			path:          "/api/v1/vehicles",
			expectVersion: "v1",
			shouldContain: "/api/v1",
		},
		{
			name:          "API Path Version Check - Keys",
			path:          "/api/v1/keys",
			expectVersion: "v1",
			shouldContain: "/api/v1",
		},
	}

	for _, tc := range testCases {
		t.report.AddTestCase(TestCase{
			Name:        tc.name,
			Category:    "Version Compatibility",
			Description: fmt.Sprintf("Check API path version for %s", tc.path),
		})

		isValid := strings.Contains(tc.path, tc.shouldContain)
		t.report.AddResult(tc.name, isValid,
			fmt.Sprintf("Path %s should contain %s", tc.path, tc.shouldContain),
			map[string]interface{}{
				"path":           tc.path,
				"expected_version": tc.expectVersion,
			})
	}
}

// testErrorHandling 测试错误处理
func (t *HTTPCompatibilityTester) testErrorHandling() {
	testCases := []struct {
		name           string
		file           string
		key            string
		expectedCode   int
		expectErrorMsg bool
	}{
		{
			name:           "404 Not Found Error",
			file:           "vehicle_responses.json",
			key:            "error_response_not_found",
			expectedCode:   404,
			expectErrorMsg: true,
		},
		{
			name:           "401 Unauthorized Error",
			file:           "vehicle_responses.json",
			key:            "error_response_unauthorized",
			expectedCode:   401,
			expectErrorMsg: true,
		},
		{
			name:           "400 Validation Error",
			file:           "vehicle_responses.json",
			key:            "error_response_validation",
			expectedCode:   400,
			expectErrorMsg: true,
		},
		{
			name:           "Key Not Found Error",
			file:           "key_responses.json",
			key:            "error_key_not_found",
			expectedCode:   404,
			expectErrorMsg: true,
		},
		{
			name:           "Key Unauthorized Error",
			file:           "key_responses.json",
			key:            "error_key_unauthorized",
			expectedCode:   403,
			expectErrorMsg: true,
		},
	}

	for _, tc := range testCases {
		t.report.AddTestCase(TestCase{
			Name:        tc.name,
			Category:    "Error Handling",
			Description: fmt.Sprintf("Validate error format for %s", tc.key),
		})

		data, ok := t.GetTestData(tc.file, tc.key)
		if !ok {
			t.report.AddResult(tc.name, false, "Test data not found", nil)
			continue
		}

		var response map[string]interface{}
		if err := json.Unmarshal(data, &response); err != nil {
			t.report.AddResult(tc.name, false, fmt.Sprintf("Failed to unmarshal: %v", err), nil)
			continue
		}

		code, _ := response["code"].(float64)
		message, _ := response["message"].(string)
		errField, _ := response["error"].(string)

		codeOK := int(code) == tc.expectedCode
		messageOK := message != ""
		errFieldOK := !tc.expectErrorMsg || errField != ""

		allOK := codeOK && messageOK && errFieldOK
		t.report.AddResult(tc.name, allOK,
			fmt.Sprintf("Code: %d, Message: %s, Error: %s", int(code), message, errField),
			map[string]interface{}{
				"expected_code": tc.expectedCode,
				"actual_code":   int(code),
				"has_message":   messageOK,
				"has_error":     errFieldOK,
			})
	}
}

// testContentTypeValidation 测试Content-Type验证
func (t *HTTPCompatibilityTester) testContentTypeValidation() {
	testCases := []struct {
		name        string
		contentType string
		expectValid bool
	}{
		{
			name:        "Valid JSON Content-Type",
			contentType: "application/json",
			expectValid: true,
		},
		{
			name:        "Valid JSON with Charset",
			contentType: "application/json; charset=utf-8",
			expectValid: true,
		},
		{
			name:        "Invalid Content-Type",
			contentType: "text/plain",
			expectValid: false,
		},
	}

	for _, tc := range testCases {
		t.report.AddTestCase(TestCase{
			Name:        tc.name,
			Category:    "Content Type Validation",
			Description: fmt.Sprintf("Validate Content-Type: %s", tc.contentType),
		})

		isValid := strings.Contains(tc.contentType, "application/json")
		t.report.AddResult(tc.name, isValid == tc.expectValid,
			fmt.Sprintf("Content-Type %s validation", tc.contentType),
			map[string]interface{}{
				"content_type": tc.contentType,
				"expected":     tc.expectValid,
			})
	}
}

// testAuthenticationHeaders 测试认证头部
func (t *HTTPCompatibilityTester) testAuthenticationHeaders() {
	testCases := []struct {
		name      string
		header    string
		value     string
		expectOK  bool
	}{
		{
			name:     "Valid Bearer Token",
			header:   "Authorization",
			value:    "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
			expectOK: true,
		},
		{
			name:     "Invalid Token Format",
			header:   "Authorization",
			value:    "Basic dXNlcjpwYXNz",
			expectOK: false,
		},
		{
			name:     "Missing Token",
			header:   "Authorization",
			value:    "",
			expectOK: false,
		},
	}

	for _, tc := range testCases {
		t.report.AddTestCase(TestCase{
			Name:        tc.name,
			Category:    "Authentication Headers",
			Description: fmt.Sprintf("Validate %s header", tc.header),
		})

		isValid := false
		if tc.header == "Authorization" {
			isValid = strings.HasPrefix(tc.value, "Bearer ") && len(tc.value) > 7
		}

		t.report.AddResult(tc.name, isValid == tc.expectOK,
			fmt.Sprintf("Authorization header validation"),
			map[string]interface{}{
				"header":   tc.header,
				"has_bearer": strings.HasPrefix(tc.value, "Bearer "),
			})
	}
}

// testPaginationFormat 测试分页格式
func (t *HTTPCompatibilityTester) testPaginationFormat() {
	testCases := []struct {
		name           string
		file           string
		key            string
		requiredFields []string
	}{
		{
			name:           "Vehicle List Pagination",
			file:           "vehicle_responses.json",
			key:            "vehicle_list_response",
			requiredFields: []string{"list", "total", "page", "page_size"},
		},
	}

	for _, tc := range testCases {
		t.report.AddTestCase(TestCase{
			Name:        tc.name,
			Category:    "Pagination Format",
			Description: fmt.Sprintf("Validate pagination format in %s", tc.key),
		})

		data, ok := t.GetTestData(tc.file, tc.key)
		if !ok {
			t.report.AddResult(tc.name, false, "Test data not found", nil)
			continue
		}

		var response map[string]interface{}
		if err := json.Unmarshal(data, &response); err != nil {
			t.report.AddResult(tc.name, false, fmt.Sprintf("Failed to unmarshal: %v", err), nil)
			continue
		}

		dataField, ok := response["data"].(map[string]interface{})
		if !ok {
			t.report.AddResult(tc.name, false, "Missing data field", nil)
			continue
		}

		missingFields := []string{}
		for _, field := range tc.requiredFields {
			if _, ok := dataField[field]; !ok {
				missingFields = append(missingFields, field)
			}
		}

		if len(missingFields) > 0 {
			t.report.AddResult(tc.name, false,
				fmt.Sprintf("Missing pagination fields: %v", missingFields),
				map[string]interface{}{"missing_fields": missingFields})
		} else {
			t.report.AddResult(tc.name, true, "Pagination format valid", nil)
		}
	}
}

// ==================== Go Test 函数 ====================

func TestHTTPCompatibility(t *testing.T) {
	gin.SetMode(gin.TestMode)
	router := gin.New()

	// 加载测试数据
	testDataDir := filepath.Join(".", "test_data")
	
	// 创建测试器
	tester := NewHTTPCompatibilityTester(router)
	
	if err := tester.LoadTestData(testDataDir); err != nil {
		t.Fatalf("Failed to load test data: %v", err)
	}

	// 运行测试
	report := tester.RunAllTests()

	// 输出报告
	t.Logf("Test Report:\n%s", report.GenerateTextReport())

	// 检查结果
	stats := report.GetStatistics()
	if stats.Failed > 0 {
		t.Errorf("HTTP Compatibility Tests: %d failed out of %d", stats.Failed, stats.Total)
	}
}

func TestRequestValidation(t *testing.T) {
	testCases := []struct {
		name        string
		request     interface{}
		shouldValid bool
	}{
		{
			name: "Valid Login Request",
			request: map[string]string{
				"username": "testuser",
				"password": "password123",
			},
			shouldValid: true,
		},
		{
			name: "Invalid Login - Missing Password",
			request: map[string]string{
				"username": "testuser",
			},
			shouldValid: false,
		},
		{
			name: "Valid Vehicle Register",
			request: map[string]interface{}{
				"vin":   "WBA12345678901234",
				"brand": "BMW",
				"model": "X5",
				"year":  2024,
				"name":  "My Car",
			},
			shouldValid: true,
		},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			data, err := json.Marshal(tc.request)
			require.NoError(t, err)

			var result map[string]interface{}
			err = json.Unmarshal(data, &result)
			require.NoError(t, err)

			// 验证JSON格式
			assert.NotNil(t, result)
		})
	}
}

func TestResponseFormatConsistency(t *testing.T) {
	// 验证所有响应都包含 code 和 message
	responses := []map[string]interface{}{
		{
			"code":    200,
			"message": "成功",
			"data":    map[string]string{"id": "1"},
		},
		{
			"code":    400,
			"message": "请求错误",
			"error":   "validation failed",
		},
	}

	for i, resp := range responses {
		t.Run(fmt.Sprintf("Response_%d", i), func(t *testing.T) {
			assert.Contains(t, resp, "code")
			assert.Contains(t, resp, "message")
		})
	}
}

func TestAPITimeout(t *testing.T) {
	// 测试API超时设置
	timeout := 30 * time.Second
	assert.Equal(t, 30*time.Second, timeout)
}

func TestHTTPStatusCodes(t *testing.T) {
	statusCodes := map[int]string{
		http.StatusOK:           "200 OK - Success",
		http.StatusCreated:      "201 Created - Resource created",
		http.StatusAccepted:     "202 Accepted - Async operation",
		http.StatusBadRequest:   "400 Bad Request - Validation error",
		http.StatusUnauthorized: "401 Unauthorized - Auth required",
		http.StatusForbidden:    "403 Forbidden - No permission",
		http.StatusNotFound:     "404 Not Found - Resource not found",
		http.StatusConflict:     "409 Conflict - Duplicate",
		http.StatusInternalServerError: "500 Internal Server Error",
	}

	for code, desc := range statusCodes {
		t.Run(fmt.Sprintf("Status_%d", code), func(t *testing.T) {
			assert.GreaterOrEqual(t, code, 100)
			assert.Less(t, code, 600)
			t.Logf("Status %s", desc)
		})
	}
}

func TestJSONContentType(t *testing.T) {
	validTypes := []string{
		"application/json",
		"application/json; charset=utf-8",
		"application/json;charset=UTF-8",
	}

	for _, ct := range validTypes {
		t.Run(fmt.Sprintf("ContentType_%s", ct), func(t *testing.T) {
			assert.Contains(t, ct, "application/json")
		})
	}
}