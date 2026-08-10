package main

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
)

// ---------------------------------------------------------------------------
//  test helpers
// ---------------------------------------------------------------------------

func newTestServer(t *testing.T) *Server {
	t.Helper()
	store, err := NewStore("file", t.TempDir(), "")
	if err != nil {
		t.Fatal(err)
	}
	return &Server{store: store, apiKey: "test-key"}
}

func doReq(t *testing.T, s *Server, method, path, body, apiKey string) *httptest.ResponseRecorder {
	t.Helper()
	var reader *bytes.Reader
	if body == "" {
		reader = bytes.NewReader(nil)
	} else {
		reader = bytes.NewReader([]byte(body))
	}
	req := httptest.NewRequest(method, path, reader)
	if apiKey != "" {
		req.Header.Set("X-API-Key", apiKey)
	}
	mux := http.NewServeMux()
	mux.HandleFunc("POST /api/v1/batches", s.auth(s.handleCreateBatch))
	mux.HandleFunc("GET /api/v1/batches", s.auth(s.handleListBatches))
	mux.HandleFunc("GET /api/v1/batches/{id}", s.auth(s.handleGetBatch))
	mux.HandleFunc("GET /api/v1/batches/{id}/stats", s.auth(s.handleBatchStats))
	mux.HandleFunc("POST /api/v1/batches/{id}/records", s.auth(s.handleAddRecord))
	mux.HandleFunc("GET /api/v1/batches/{id}/records", s.auth(s.handleListRecords))
	mux.HandleFunc("GET /api/v1/devices/{id}", s.auth(s.handleDevice))
	rr := httptest.NewRecorder()
	mux.ServeHTTP(rr, req)
	return rr
}

func createTestBatch(t *testing.T, s *Server, id string) {
	t.Helper()
	body := `{"batch_id":"` + id + `","firmware_version":"2.1.0",` +
		`"package_sha256":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",` +
		`"signing_key_id":"dev","enc_key_id":"dev",` +
		`"planned_devices":["DK-0001","DK-0002"]}`
	rr := doReq(t, s, "POST", "/api/v1/batches", body, "test-key")
	if rr.Code != http.StatusCreated {
		t.Fatalf("create batch: code=%d body=%s", rr.Code, rr.Body.String())
	}
}

// ---------------------------------------------------------------------------
//  鉴权
// ---------------------------------------------------------------------------

func TestAuthRequired(t *testing.T) {
	s := newTestServer(t)
	rr := doReq(t, s, "GET", "/api/v1/batches", "", "")
	if rr.Code != http.StatusUnauthorized {
		t.Fatalf("want 401, got %d", rr.Code)
	}
	rr = doReq(t, s, "GET", "/api/v1/batches", "", "wrong-key")
	if rr.Code != http.StatusUnauthorized {
		t.Fatalf("want 401 for wrong key, got %d", rr.Code)
	}
	rr = doReq(t, s, "GET", "/api/v1/batches", "", "test-key")
	if rr.Code != http.StatusOK {
		t.Fatalf("want 200 with valid key, got %d", rr.Code)
	}
}

// ---------------------------------------------------------------------------
//  批次
// ---------------------------------------------------------------------------

func TestCreateAndGetBatch(t *testing.T) {
	s := newTestServer(t)
	createTestBatch(t, s, "B1")
	rr := doReq(t, s, "GET", "/api/v1/batches/B1", "", "test-key")
	if rr.Code != http.StatusOK {
		t.Fatalf("get batch: %d", rr.Code)
	}
	var b Batch
	if err := json.Unmarshal(rr.Body.Bytes(), &b); err != nil {
		t.Fatal(err)
	}
	if b.FirmwareVersion != "2.1.0" || b.Status != "active" {
		t.Fatalf("batch fields: %+v", b)
	}
	if len(b.PlannedDevices) != 2 {
		t.Fatalf("planned devices: %v", b.PlannedDevices)
	}
}

func TestCreateDuplicateBatch(t *testing.T) {
	s := newTestServer(t)
	createTestBatch(t, s, "B1")
	rr := doReq(t, s, "POST", "/api/v1/batches",
		`{"batch_id":"B1","firmware_version":"1.0"}`, "test-key")
	if rr.Code != http.StatusConflict {
		t.Fatalf("want 409, got %d", rr.Code)
	}
}

func TestGetMissingBatch(t *testing.T) {
	s := newTestServer(t)
	rr := doReq(t, s, "GET", "/api/v1/batches/NOPE", "", "test-key")
	if rr.Code != http.StatusNotFound {
		t.Fatalf("want 404, got %d", rr.Code)
	}
}

func TestListBatches(t *testing.T) {
	s := newTestServer(t)
	createTestBatch(t, s, "B1")
	createTestBatch(t, s, "B2")
	rr := doReq(t, s, "GET", "/api/v1/batches", "", "test-key")
	var resp struct {
		Total int `json:"total"`
	}
	if err := json.Unmarshal(rr.Body.Bytes(), &resp); err != nil {
		t.Fatal(err)
	}
	if resp.Total != 2 {
		t.Fatalf("want 2 batches, got %d", resp.Total)
	}
}

// ---------------------------------------------------------------------------
//  烧录记录 + 哈希链
// ---------------------------------------------------------------------------

func TestAddRecordAndChain(t *testing.T) {
	s := newTestServer(t)
	createTestBatch(t, s, "B1")

	rr := doReq(t, s, "POST", "/api/v1/batches/B1/records",
		`{"device_id":"DK-0001","result":"PASSED"}`, "test-key")
	if rr.Code != http.StatusCreated {
		t.Fatalf("add record: %d %s", rr.Code, rr.Body.String())
	}
	var rec1 FlashRecord
	json.Unmarshal(rr.Body.Bytes(), &rec1)
	if rec1.PrevHash != genesisHash {
		t.Fatalf("first record prev_hash should be GENESIS, got %s", rec1.PrevHash)
	}

	rr = doReq(t, s, "POST", "/api/v1/batches/B1/records",
		`{"device_id":"DK-0002","result":"FAILED","detail":"verifybin mismatch"}`, "test-key")
	var rec2 FlashRecord
	json.Unmarshal(rr.Body.Bytes(), &rec2)
	if rec2.PrevHash != rec1.RecordHash {
		t.Fatalf("chain broken: rec2.prev=%s rec1.hash=%s", rec2.PrevHash, rec1.RecordHash)
	}

	// 与 Python 侧算法一致性验证: 同输入应同哈希
	expect := recordHash(rec1.RecordHash, "B1", "DK-0002", "FAILED",
		rec2.FlashedAt, "2.1.0", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")
	if rec2.RecordHash != expect {
		t.Fatalf("record hash mismatch: %s != %s", rec2.RecordHash, expect)
	}
}

func TestInvalidResultRejected(t *testing.T) {
	s := newTestServer(t)
	createTestBatch(t, s, "B1")
	rr := doReq(t, s, "POST", "/api/v1/batches/B1/records",
		`{"device_id":"DK-0001","result":"BOGUS"}`, "test-key")
	if rr.Code != http.StatusBadRequest {
		t.Fatalf("want 400, got %d", rr.Code)
	}
}

func TestRecordMissingBatch(t *testing.T) {
	s := newTestServer(t)
	rr := doReq(t, s, "POST", "/api/v1/batches/NOPE/records",
		`{"device_id":"DK-0001","result":"PASSED"}`, "test-key")
	if rr.Code != http.StatusNotFound {
		t.Fatalf("want 404, got %d", rr.Code)
	}
}

// ---------------------------------------------------------------------------
//  统计 / 设备
// ---------------------------------------------------------------------------

func TestBatchStats(t *testing.T) {
	s := newTestServer(t)
	createTestBatch(t, s, "B1")
	for _, dev := range []string{"DK-0001", "DK-0002", "DK-0003"} {
		res := "PASSED"
		if dev == "DK-0003" {
			res = "FAILED"
		}
		doReq(t, s, "POST", "/api/v1/batches/B1/records",
			`{"device_id":"`+dev+`","result":"`+res+`"}`, "test-key")
	}
	rr := doReq(t, s, "GET", "/api/v1/batches/B1/stats", "", "test-key")
	var st BatchStats
	json.Unmarshal(rr.Body.Bytes(), &st)
	if st.Total != 3 || st.Passed != 2 {
		t.Fatalf("stats: %+v", st)
	}
	if st.YieldPct != 66.66666666666666 && st.YieldPct != 66.7 {
		t.Fatalf("yield: %v", st.YieldPct)
	}
	if len(st.FailedDevices) != 1 || st.FailedDevices[0] != "DK-0003" {
		t.Fatalf("failed devices: %v", st.FailedDevices)
	}
}

func TestDeviceQuery(t *testing.T) {
	s := newTestServer(t)
	createTestBatch(t, s, "B1")
	doReq(t, s, "POST", "/api/v1/batches/B1/records",
		`{"device_id":"DK-0001","result":"PASSED"}`, "test-key")
	rr := doReq(t, s, "GET", "/api/v1/devices/DK-0001", "", "test-key")
	if rr.Code != http.StatusOK {
		t.Fatalf("device: %d", rr.Code)
	}
	if !strings.Contains(rr.Body.String(), "FLASHED") {
		t.Fatalf("device status: %s", rr.Body.String())
	}
	rr = doReq(t, s, "GET", "/api/v1/devices/UNKNOWN", "", "test-key")
	if rr.Code != http.StatusNotFound {
		t.Fatalf("want 404, got %d", rr.Code)
	}
}

func TestHealthz(t *testing.T) {
	s := newTestServer(t)
	req := httptest.NewRequest("GET", "/healthz", nil)
	rr := httptest.NewRecorder()
	s.handleHealth(rr, req)
	if rr.Code != http.StatusOK {
		t.Fatalf("healthz: %d", rr.Code)
	}
}
