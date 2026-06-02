package gateway

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	"github.com/gin-gonic/gin"
)

// ── extractAuth ──

func TestExtractAuth_Success(t *testing.T) {
	g := newTestGateway()
	c, _ := gin.CreateTestContext(httptest.NewRecorder())
	c.Set("user_id", "user-1")
	c.Set("user_role", "admin")

	uid, role, err := g.extractAuth(c)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if uid != "user-1" {
		t.Fatalf("expected user-1, got %s", uid)
	}
	if role != "admin" {
		t.Fatalf("expected admin, got %s", role)
	}
}

func TestExtractAuth_SuccessNoRole(t *testing.T) {
	g := newTestGateway()
	c, _ := gin.CreateTestContext(httptest.NewRecorder())
	c.Set("user_id", "user-1")
	// user_role not set — should default to empty string without panic

	uid, role, err := g.extractAuth(c)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if uid != "user-1" {
		t.Fatalf("expected user-1, got %s", uid)
	}
	if role != "" {
		t.Fatalf("expected empty role, got %s", role)
	}
}

func TestExtractAuth_NoUserID(t *testing.T) {
	g := newTestGateway()
	c, _ := gin.CreateTestContext(httptest.NewRecorder())
	// Don't set user_id

	_, _, err := g.extractAuth(c)
	if err == nil {
		t.Fatal("expected error when user_id not set")
	}
	if err.Error() != "unauthenticated" {
		t.Fatalf("expected 'unauthenticated', got %v", err)
	}
}

func TestExtractAuth_WrongType(t *testing.T) {
	g := newTestGateway()
	c, _ := gin.CreateTestContext(httptest.NewRecorder())
	c.Set("user_id", 12345) // wrong type

	_, _, err := g.extractAuth(c)
	if err == nil {
		t.Fatal("expected error from type assertion failure")
	}
	if err.Error() != "invalid user_id type" {
		t.Fatalf("expected 'invalid user_id type', got %v", err)
	}
}

// ── helpers for device handler tests ──

func setupDeviceTestRouter(g *RESTGateway) *gin.Engine {
	r := gin.New()
	r.Use(gin.Recovery())

	// Auth-protected device routes
	v1 := r.Group("/api/v1")
	v1.Use(g.authMiddleware())
	{
		devices := v1.Group("/devices")
		{
			devices.POST("", g.registerDevice)
			devices.GET("", g.listDevices)
			devices.GET("/:deviceId", g.getDevice)
			devices.POST("/:deviceId/provision", g.provisionDevice)
			devices.POST("/:deviceId/revoke", g.revokeDevice)
			devices.DELETE("/:deviceId", g.deleteDevice)
		}
	}
	return r
}

func deviceAuthRequest(r *gin.Engine, method, path, body, token string) *httptest.ResponseRecorder {
	w := httptest.NewRecorder()
	req := httptest.NewRequest(method, path, strings.NewReader(body))
	req.Header.Set("Authorization", "Bearer "+token)
	if body != "" {
		req.Header.Set("Content-Type", "application/json")
	}
	r.ServeHTTP(w, req)
	return w
}

// ── registerDevice ──

func TestRegisterDevice_Success(t *testing.T) {
	g := newTestGateway()
	r := setupDeviceTestRouter(g)
	tok := issueTestToken(g, "user-1", "admin")

	body := `{
		"platform": "ios",
		"model": "iPhone 15 Pro",
		"os_version": "18.0",
		"app_version": "1.2.3",
		"ble": true,
		"uwb": true,
		"nfc": true,
		"secure_element": true
	}`
	w := deviceAuthRequest(r, "POST", "/api/v1/devices", body, tok)
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", w.Code, w.Body.String())
	}

	var resp map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &resp)
	if resp["device_id"] == nil {
		t.Fatal("expected device_id in response")
	}
	if resp["max_devices"].(float64) != 5 {
		t.Fatalf("expected max_devices 5, got %v", resp["max_devices"])
	}
}

func TestRegisterDevice_BadRequest(t *testing.T) {
	g := newTestGateway()
	r := setupDeviceTestRouter(g)
	tok := issueTestToken(g, "user-1", "admin")

	// Invalid JSON
	w := deviceAuthRequest(r, "POST", "/api/v1/devices", "{bad json", tok)
	if w.Code != http.StatusBadRequest {
		t.Fatalf("expected 400, got %d: %s", w.Code, w.Body.String())
	}
}

func TestRegisterDevice_NoAuth(t *testing.T) {
	g := newTestGateway()
	r := setupDeviceTestRouter(g)

	w := httptest.NewRecorder()
	req := httptest.NewRequest("POST", "/api/v1/devices", strings.NewReader(`{}`))
	req.Header.Set("Content-Type", "application/json")
	r.ServeHTTP(w, req)
	if w.Code != http.StatusUnauthorized {
		t.Fatalf("expected 401, got %d", w.Code)
	}
}

func TestRegisterDevice_MinimalFields(t *testing.T) {
	g := newTestGateway()
	r := setupDeviceTestRouter(g)
	tok := issueTestToken(g, "user-2", "user")

	// Only required fields
	body := `{"platform": "android", "model": "Pixel 8"}`
	w := deviceAuthRequest(r, "POST", "/api/v1/devices", body, tok)
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", w.Code, w.Body.String())
	}

	var resp map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &resp)
	if resp["device_id"] == nil {
		t.Fatal("expected device_id")
	}
}

// ── listDevices ──

func TestListDevices_Empty(t *testing.T) {
	g := newTestGateway()
	r := setupDeviceTestRouter(g)
	tok := issueTestToken(g, "user-empty", "user")

	w := deviceAuthRequest(r, "GET", "/api/v1/devices", "", tok)
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", w.Code, w.Body.String())
	}

	var resp map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &resp)
	devices, ok := resp["devices"].([]interface{})
	if !ok {
		t.Fatal("expected devices array")
	}
	if len(devices) != 0 {
		t.Fatalf("expected 0 devices, got %d", len(devices))
	}
}

func TestListDevices_WithDevice(t *testing.T) {
	g := newTestGateway()
	r := setupDeviceTestRouter(g)
	tok := issueTestToken(g, "user-list", "user")

	// Register one device first
	body := `{"platform": "ios", "model": "iPhone"}`
	deviceAuthRequest(r, "POST", "/api/v1/devices", body, tok)

	// Now list
	w := deviceAuthRequest(r, "GET", "/api/v1/devices", "", tok)
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", w.Code, w.Body.String())
	}

	var resp map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &resp)
	devices, ok := resp["devices"].([]interface{})
	if !ok {
		t.Fatal("expected devices array")
	}
	if len(devices) != 1 {
		t.Fatalf("expected 1 device, got %d", len(devices))
	}
}

// ── getDevice ──

func TestGetDevice_Success(t *testing.T) {
	g := newTestGateway()
	r := setupDeviceTestRouter(g)
	tok := issueTestToken(g, "user-get", "user")

	// Register a device
	body := `{"platform": "ios", "model": "iPhone 15", "ble": true}`
	regW := deviceAuthRequest(r, "POST", "/api/v1/devices", body, tok)
	var regResp map[string]interface{}
	json.Unmarshal(regW.Body.Bytes(), &regResp)
	deviceID := regResp["device_id"].(string)

	// Get it
	w := deviceAuthRequest(r, "GET", "/api/v1/devices/"+deviceID, "", tok)
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", w.Code, w.Body.String())
	}

	var getResp map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &getResp)
	if getResp["device_id"] != deviceID {
		t.Fatalf("expected device_id %s, got %v", deviceID, getResp["device_id"])
	}
	if getResp["ble"] != true {
		t.Error("expected ble=true")
	}
}

func TestGetDevice_NotFound(t *testing.T) {
	g := newTestGateway()
	r := setupDeviceTestRouter(g)
	tok := issueTestToken(g, "user-get", "user")

	w := deviceAuthRequest(r, "GET", "/api/v1/devices/nonexistent-device", "", tok)
	if w.Code != http.StatusNotFound {
		t.Fatalf("expected 404, got %d: %s", w.Code, w.Body.String())
	}
}

func TestGetDevice_OtherUserDevice(t *testing.T) {
	g := newTestGateway()
	r := setupDeviceTestRouter(g)
	tok1 := issueTestToken(g, "user-owner", "user")
	tok2 := issueTestToken(g, "user-other", "user")

	// Register device as user-owner
	body := `{"platform": "android", "model": "Samsung"}`
	regW := deviceAuthRequest(r, "POST", "/api/v1/devices", body, tok1)
	var regResp map[string]interface{}
	json.Unmarshal(regW.Body.Bytes(), &regResp)
	deviceID := regResp["device_id"].(string)

	// Try to get as user-other (forbidden)
	w := deviceAuthRequest(r, "GET", "/api/v1/devices/"+deviceID, "", tok2)
	if w.Code != http.StatusForbidden {
		t.Fatalf("expected 403, got %d: %s", w.Code, w.Body.String())
	}
}

// ── provisionDevice ──

func TestProvisionDevice_Success(t *testing.T) {
	g := newTestGateway()
	r := setupDeviceTestRouter(g)
	tok := issueTestToken(g, "user-provision", "user")
	vehicleID := "VH-PROV-001"

	// Register device first
	body := `{"platform": "ios", "model": "iPhone", "nfc": true, "ble": true}`
	regW := deviceAuthRequest(r, "POST", "/api/v1/devices", body, tok)
	var regResp map[string]interface{}
	json.Unmarshal(regW.Body.Bytes(), &regResp)
	deviceID := regResp["device_id"].(string)

	// Provision
	provBody := `{"vehicle_id": "` + vehicleID + `"}`
	provW := deviceAuthRequest(r, "POST", "/api/v1/devices/"+deviceID+"/provision", provBody, tok)
	if provW.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", provW.Code, provW.Body.String())
	}

	var provResp map[string]interface{}
	json.Unmarshal(provW.Body.Bytes(), &provResp)
	if provResp["key_id"] == nil {
		t.Fatal("expected key_id in response")
	}
	if provResp["vehicle_id"] != vehicleID {
		t.Fatalf("expected vehicle_id %s, got %v", vehicleID, provResp["vehicle_id"])
	}
}

func TestProvisionDevice_MissingVehicleID(t *testing.T) {
	g := newTestGateway()
	r := setupDeviceTestRouter(g)
	tok := issueTestToken(g, "user-provision", "user")

	// Register device first
	body := `{"platform": "ios", "model": "iPhone"}`
	regW := deviceAuthRequest(r, "POST", "/api/v1/devices", body, tok)
	var regResp map[string]interface{}
	json.Unmarshal(regW.Body.Bytes(), &regResp)
	deviceID := regResp["device_id"].(string)

	// Provision without vehicle_id
	provW := deviceAuthRequest(r, "POST", "/api/v1/devices/"+deviceID+"/provision", `{}`, tok)
	if provW.Code != http.StatusBadRequest {
		t.Fatalf("expected 400, got %d: %s", provW.Code, provW.Body.String())
	}
}

func TestProvisionDevice_DeviceNotFound(t *testing.T) {
	g := newTestGateway()
	r := setupDeviceTestRouter(g)
	tok := issueTestToken(g, "user-provision", "user")

	provW := deviceAuthRequest(r, "POST", "/api/v1/devices/nonexistent/provision",
		`{"vehicle_id": "VH-001"}`, tok)
	if provW.Code != http.StatusNotFound {
		t.Fatalf("expected 404, got %d: %s", provW.Code, provW.Body.String())
	}
}

func TestProvisionDevice_OtherUserDevice(t *testing.T) {
	g := newTestGateway()
	r := setupDeviceTestRouter(g)
	tok1 := issueTestToken(g, "user-owner", "user")
	tok2 := issueTestToken(g, "user-other", "user")

	// Register device as user-owner
	regW := deviceAuthRequest(r, "POST", "/api/v1/devices", `{"platform": "android"}`, tok1)
	var regResp map[string]interface{}
	json.Unmarshal(regW.Body.Bytes(), &regResp)
	deviceID := regResp["device_id"].(string)

	// Try to provision as other user
	provW := deviceAuthRequest(r, "POST", "/api/v1/devices/"+deviceID+"/provision",
		`{"vehicle_id": "VH-001"}`, tok2)
	if provW.Code != http.StatusForbidden {
		t.Fatalf("expected 403, got %d: %s", provW.Code, provW.Body.String())
	}
}

func TestProvisionDevice_AlreadyProvisioned(t *testing.T) {
	g := newTestGateway()
	r := setupDeviceTestRouter(g)
	tok := issueTestToken(g, "user-provision2", "user")
	vehicleID := "VH-DUP-001"

	// Register device
	regW := deviceAuthRequest(r, "POST", "/api/v1/devices", `{"platform": "ios","model":"iPhone"}`, tok)
	var regResp map[string]interface{}
	json.Unmarshal(regW.Body.Bytes(), &regResp)
	deviceID := regResp["device_id"].(string)

	// Provision first time - success
	provW1 := deviceAuthRequest(r, "POST", "/api/v1/devices/"+deviceID+"/provision",
		`{"vehicle_id": "`+vehicleID+`"}`, tok)
	if provW1.Code != http.StatusOK {
		t.Fatalf("first provision expected 200, got %d", provW1.Code)
	}

	// Provision second time — should re-return existing active binding
	provW2 := deviceAuthRequest(r, "POST", "/api/v1/devices/"+deviceID+"/provision",
		`{"vehicle_id": "`+vehicleID+`"}`, tok)
	if provW2.Code != http.StatusOK {
		t.Fatalf("second provision expected 200, got %d: %s", provW2.Code, provW2.Body.String())
	}
}

// ── revokeDevice ──

func TestRevokeDevice_Success(t *testing.T) {
	g := newTestGateway()
	r := setupDeviceTestRouter(g)
	tok := issueTestToken(g, "user-revoke", "user")

	// Register device
	regW := deviceAuthRequest(r, "POST", "/api/v1/devices", `{"platform": "ios"}`, tok)
	var regResp map[string]interface{}
	json.Unmarshal(regW.Body.Bytes(), &regResp)
	deviceID := regResp["device_id"].(string)

	// Revoke
	w := deviceAuthRequest(r, "POST", "/api/v1/devices/"+deviceID+"/revoke", "", tok)
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", w.Code, w.Body.String())
	}

	var revResp map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &revResp)
	if revResp["status"] != "revoked" {
		t.Fatalf("expected revoked status, got %v", revResp["status"])
	}
	if revResp["keys_revoked"].(float64) != 0 {
		t.Fatalf("expected 0 keys_revoked, got %v", revResp["keys_revoked"])
	}
}

func TestRevokeDevice_WithKeys(t *testing.T) {
	g := newTestGateway()
	r := setupDeviceTestRouter(g)
	tok := issueTestToken(g, "user-revoke2", "user")

	// Register + provision
	regW := deviceAuthRequest(r, "POST", "/api/v1/devices", `{"platform": "android","nfc":true}`, tok)
	var regResp map[string]interface{}
	json.Unmarshal(regW.Body.Bytes(), &regResp)
	deviceID := regResp["device_id"].(string)

	deviceAuthRequest(r, "POST", "/api/v1/devices/"+deviceID+"/provision",
		`{"vehicle_id": "VH-REV-001"}`, tok)

	// Revoke — should have 1 key revoked
	w := deviceAuthRequest(r, "POST", "/api/v1/devices/"+deviceID+"/revoke", "", tok)
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", w.Code, w.Body.String())
	}
	var revResp map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &revResp)
	if revResp["keys_revoked"].(float64) != 1 {
		t.Fatalf("expected 1 key revoked, got %v", revResp["keys_revoked"])
	}
}

// ── deleteDevice ──

func TestDeleteDevice_Success(t *testing.T) {
	g := newTestGateway()
	r := setupDeviceTestRouter(g)
	tok := issueTestToken(g, "user-delete", "user")

	// Register device
	regW := deviceAuthRequest(r, "POST", "/api/v1/devices", `{"platform": "ios"}`, tok)
	var regResp map[string]interface{}
	json.Unmarshal(regW.Body.Bytes(), &regResp)
	deviceID := regResp["device_id"].(string)

	// Delete
	w := deviceAuthRequest(r, "DELETE", "/api/v1/devices/"+deviceID, "", tok)
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", w.Code, w.Body.String())
	}

	var delResp map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &delResp)
	if delResp["status"] != "deleted" {
		t.Fatalf("expected 'deleted', got %v", delResp["status"])
	}

	// Verify device is gone
	getW := deviceAuthRequest(r, "GET", "/api/v1/devices/"+deviceID, "", tok)
	if getW.Code != http.StatusNotFound {
		t.Fatalf("expected 404 for deleted device, got %d", getW.Code)
	}
}

func TestDeleteDevice_OtherUserDevice(t *testing.T) {
	g := newTestGateway()
	r := setupDeviceTestRouter(g)
	tok1 := issueTestToken(g, "user-owner", "user")
	tok2 := issueTestToken(g, "user-other", "user")

	regW := deviceAuthRequest(r, "POST", "/api/v1/devices", `{"platform": "ios"}`, tok1)
	var regResp map[string]interface{}
	json.Unmarshal(regW.Body.Bytes(), &regResp)
	deviceID := regResp["device_id"].(string)

	w := deviceAuthRequest(r, "DELETE", "/api/v1/devices/"+deviceID, "", tok2)
	// Delete will try to get device first, which succeeds, but ownership check fails
	// DeviceService checks ownership and returns error -> handler returns 500
	if w.Code != http.StatusInternalServerError {
		t.Fatalf("expected 500 (device not owned), got %d: %s", w.Code, w.Body.String())
	}
}

func TestDeleteDevice_NotFound(t *testing.T) {
	g := newTestGateway()
	r := setupDeviceTestRouter(g)
	tok := issueTestToken(g, "user-delete", "user")

	w := deviceAuthRequest(r, "DELETE", "/api/v1/devices/nonexistent", "", tok)
	// DeviceService.DeleteDevice checks existence, returns error -> 500
	// (the gateway handler doesn't distinguish not-found vs other errors)
	if w.Code != http.StatusInternalServerError {
		t.Fatalf("expected 500, got %d: %s", w.Code, w.Body.String())
	}
}

// ── Direct handler tests (no auth middleware) ──
// These tests cover the extractAuth() error branches in each device handler

func TestRegisterDevice_DirectExtractAuthError(t *testing.T) {
	g := newTestGateway()
	w := httptest.NewRecorder()
	c, _ := gin.CreateTestContext(w)
	c.Request = httptest.NewRequest("POST", "/api/v1/devices", strings.NewReader(`{}`))
	c.Request.Header.Set("Content-Type", "application/json")

	g.registerDevice(c)
	if w.Code != http.StatusUnauthorized {
		t.Fatalf("expected 401, got %d: %s", w.Code, w.Body.String())
	}
	var resp map[string]string
	json.Unmarshal(w.Body.Bytes(), &resp)
	if resp["error"] != "AUTH_FAILED" {
		t.Fatalf("expected AUTH_FAILED, got %s", resp["error"])
	}
}

func TestListDevices_DirectExtractAuthError(t *testing.T) {
	g := newTestGateway()
	w := httptest.NewRecorder()
	c, _ := gin.CreateTestContext(w)
	c.Request = httptest.NewRequest("GET", "/api/v1/devices", nil)

	g.listDevices(c)
	if w.Code != http.StatusUnauthorized {
		t.Fatalf("expected 401, got %d: %s", w.Code, w.Body.String())
	}
	var resp map[string]string
	json.Unmarshal(w.Body.Bytes(), &resp)
	if resp["error"] != "AUTH_FAILED" {
		t.Fatalf("expected AUTH_FAILED, got %s", resp["error"])
	}
}

func TestGetDevice_DirectExtractAuthError(t *testing.T) {
	g := newTestGateway()
	w := httptest.NewRecorder()
	c, _ := gin.CreateTestContext(w)
	c.Request = httptest.NewRequest("GET", "/api/v1/devices/dev-1", nil)
	c.Params = []gin.Param{{Key: "deviceId", Value: "dev-1"}}

	g.getDevice(c)
	if w.Code != http.StatusUnauthorized {
		t.Fatalf("expected 401, got %d: %s", w.Code, w.Body.String())
	}
}

func TestProvisionDevice_DirectExtractAuthError(t *testing.T) {
	g := newTestGateway()
	w := httptest.NewRecorder()
	c, _ := gin.CreateTestContext(w)
	c.Request = httptest.NewRequest("POST", "/api/v1/devices/d-1/provision", strings.NewReader(`{"vehicle_id": "VH-001"}`))
	c.Request.Header.Set("Content-Type", "application/json")
	c.Params = []gin.Param{{Key: "deviceId", Value: "d-1"}}

	g.provisionDevice(c)
	if w.Code != http.StatusUnauthorized {
		t.Fatalf("expected 401, got %d: %s", w.Code, w.Body.String())
	}
}

func TestRevokeDevice_DirectExtractAuthError(t *testing.T) {
	g := newTestGateway()
	w := httptest.NewRecorder()
	c, _ := gin.CreateTestContext(w)
	c.Request = httptest.NewRequest("POST", "/api/v1/devices/d-1/revoke", nil)
	c.Params = []gin.Param{{Key: "deviceId", Value: "d-1"}}

	g.revokeDevice(c)
	if w.Code != http.StatusUnauthorized {
		t.Fatalf("expected 401, got %d: %s", w.Code, w.Body.String())
	}
}

func TestDeleteDevice_DirectExtractAuthError(t *testing.T) {
	g := newTestGateway()
	w := httptest.NewRecorder()
	c, _ := gin.CreateTestContext(w)
	c.Request = httptest.NewRequest("DELETE", "/api/v1/devices/d-1", nil)
	c.Params = []gin.Param{{Key: "deviceId", Value: "d-1"}}

	g.deleteDevice(c)
	if w.Code != http.StatusUnauthorized {
		t.Fatalf("expected 401, got %d: %s", w.Code, w.Body.String())
	}
}

// ── Device Service error paths ──

func TestRegisterDevice_LimitReached(t *testing.T) {
	g := newTestGateway()
	r := setupDeviceTestRouter(g)
	tok := issueTestToken(g, "user-limit", "user")

	// Register 5 devices (max limit)
	for i := 0; i < 5; i++ {
		body := `{"platform": "ios", "model": "iPhone"}`
		w := deviceAuthRequest(r, "POST", "/api/v1/devices", body, tok)
		if w.Code != http.StatusOK {
			t.Fatalf("device %d expected 200, got %d: %s", i+1, w.Code, w.Body.String())
		}
	}

	// 6th device should fail with 409
	body := `{"platform": "android", "model": "Pixel"}`
	w := deviceAuthRequest(r, "POST", "/api/v1/devices", body, tok)
	if w.Code != http.StatusConflict {
		t.Fatalf("expected 409 (limit), got %d: %s", w.Code, w.Body.String())
	}
	var resp map[string]string
	json.Unmarshal(w.Body.Bytes(), &resp)
	if resp["error"] != "REGISTER_FAILED" {
		t.Fatalf("expected REGISTER_FAILED, got %s", resp["error"])
	}
}

// ── ProvisionDevice UWB capability branch ──

func TestProvisionDevice_WithUWB(t *testing.T) {
	g := newTestGateway()
	r := setupDeviceTestRouter(g)
	tok := issueTestToken(g, "user-uwb", "user")

	// Register with UWB capability
	body := `{"platform": "ios", "model": "iPhone", "uwb": true}`
	regW := deviceAuthRequest(r, "POST", "/api/v1/devices", body, tok)
	var regResp map[string]interface{}
	json.Unmarshal(regW.Body.Bytes(), &regResp)
	deviceID := regResp["device_id"].(string)

	provW := deviceAuthRequest(r, "POST", "/api/v1/devices/"+deviceID+"/provision",
		`{"vehicle_id": "VH-UWB-001"}`, tok)
	if provW.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", provW.Code, provW.Body.String())
	}
}

func TestProvisionDevice_ProvisionKeyError(t *testing.T) {
	g := newTestGateway()
	r := setupDeviceTestRouter(g)
	tok := issueTestToken(g, "user-prov-err", "user")

	// Register device
	regW := deviceAuthRequest(r, "POST", "/api/v1/devices", `{"platform": "ios"}`, tok)
	var regResp map[string]interface{}
	json.Unmarshal(regW.Body.Bytes(), &regResp)
	deviceID := regResp["device_id"].(string)

	// Delete device first so provisioning it again fails
	delW := deviceAuthRequest(r, "DELETE", "/api/v1/devices/"+deviceID, "", tok)
	if delW.Code != http.StatusOK {
		t.Fatalf("delete expected 200, got %d", delW.Code)
	}

	// Try to provision deleted device via the handler
	// Register a new device to get a different ID that the service doesn't know
	provW := deviceAuthRequest(r, "POST", "/api/v1/devices/"+deviceID+"/provision",
		`{"vehicle_id": "VH-ERR"}`, tok)
	if provW.Code != http.StatusNotFound {
		t.Fatalf("expected 404 for deleted device, got %d: %s", provW.Code, provW.Body.String())
	}
}
