// Package main — yuleDKCS 生产批次管理 API (B3-B, 云端)
//
// 纯标准库实现 (无第三方依赖): net/http + encoding/json + 文件持久化。
// 数据模型与工厂侧 batch_manager.py (SQLite) 对齐, 哈希链算法一致,
// 工厂 A 端导出的 API 载荷可直接 POST 上报。
//
// 对接: MES/工厂工位配置域名后即可使用, 见 docs/mes-integration.md。
package main

import (
	"crypto/sha256"
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"os"
	"strings"
	"time"
)

// ---------------------------------------------------------------------------
//  数据模型 (与 batch_manager.py schema 对齐)
// ---------------------------------------------------------------------------

type Batch struct {
	ID              string   `json:"batch_id"`
	FirmwareVersion string   `json:"firmware_version"`
	PackageSHA256   string   `json:"package_sha256"`
	SigningKeyID    string   `json:"signing_key_id"`
	EncKeyID        string   `json:"enc_key_id"`
	PlannedDevices  []string `json:"planned_devices"`
	Status          string   `json:"status"`
	CreatedAt       string   `json:"created_at"`
}

type FlashRecord struct {
	DeviceID        string `json:"device_id"`
	FirmwareVersion string `json:"firmware_version"`
	PackageSHA256   string `json:"package_sha256"`
	Result          string `json:"result"` // PASSED | FAILED | DRY_RUN | ERROR
	Detail          string `json:"detail,omitempty"`
	FlashedAt       string `json:"flashed_at"`
	PrevHash        string `json:"prev_hash"`
	RecordHash      string `json:"record_hash"`
}

type BatchStats struct {
	Batch         string            `json:"batch"`
	Total         int               `json:"total"`
	Passed        int               `json:"passed"`
	YieldPct      float64           `json:"yield_pct"`
	ByResult      map[string]int    `json:"by_result"`
	FailedDevices []string          `json:"failed_devices"`
}

// 存储实现见 store.go (接口) / store_file.go (文件 JSON) / store_pg.go (PostgreSQL)

// ---------------------------------------------------------------------------
//  哈希链 (与 batch_manager.py record_hash 算法一致)
// ---------------------------------------------------------------------------

const genesisHash = "GENESIS"

func sha256hex(s string) string {
	sum := sha256.Sum256([]byte(s))
	return fmt.Sprintf("%x", sum)
}

func nowISO() string {
	return time.Now().UTC().Format(time.RFC3339)
}

func recordHash(prev, batchID, deviceID, result, flashedAt, version, sha string) string {
	payload := strings.Join([]string{prev, batchID, deviceID, result,
		flashedAt, version, sha}, "|")
	// sha256 十六进制
	return sha256hex(payload)
}

// ---------------------------------------------------------------------------
//  HTTP handler
// ---------------------------------------------------------------------------

type Server struct {
	store Store
	apiKey string
}

func writeJSON(w http.ResponseWriter, code int, v any) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.WriteHeader(code)
	_ = json.NewEncoder(w).Encode(v)
}

func writeErr(w http.ResponseWriter, code int, msg string) {
	writeJSON(w, code, map[string]string{"error": msg})
}

// 鉴权中间件: X-API-Key
func (s *Server) auth(next http.HandlerFunc) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		key := r.Header.Get("X-API-Key")
		if key == "" || key != s.apiKey {
			writeErr(w, http.StatusUnauthorized, "invalid or missing X-API-Key")
			return
		}
		next(w, r)
	}
}

func (s *Server) handleCreateBatch(w http.ResponseWriter, r *http.Request) {
	var req struct {
		ID              string   `json:"batch_id"`
		FirmwareVersion string   `json:"firmware_version"`
		PackageSHA256   string   `json:"package_sha256"`
		SigningKeyID    string   `json:"signing_key_id"`
		EncKeyID        string   `json:"enc_key_id"`
		PlannedDevices  []string `json:"planned_devices"`
	}
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeErr(w, http.StatusBadRequest, "invalid JSON: "+err.Error())
		return
	}
	if req.ID == "" || req.FirmwareVersion == "" {
		writeErr(w, http.StatusBadRequest, "batch_id and firmware_version required")
		return
	}
	if req.SigningKeyID == "" {
		req.SigningKeyID = "dev"
	}
	if req.EncKeyID == "" {
		req.EncKeyID = "dev"
	}
	b := Batch{
		ID: req.ID, FirmwareVersion: req.FirmwareVersion,
		PackageSHA256: req.PackageSHA256, SigningKeyID: req.SigningKeyID,
		EncKeyID: req.EncKeyID, PlannedDevices: req.PlannedDevices,
		Status: "active", CreatedAt: nowISO(),
	}
	if err := s.store.CreateBatch(b); err != nil {
		writeErr(w, http.StatusConflict, err.Error())
		return
	}
	writeJSON(w, http.StatusCreated, b)
}

func (s *Server) handleListBatches(w http.ResponseWriter, r *http.Request) {
	batches, err := s.store.ListBatches()
	if err != nil {
		writeErr(w, http.StatusInternalServerError, err.Error())
		return
	}
	writeJSON(w, http.StatusOK, map[string]any{"batches": batches, "total": len(batches)})
}

func (s *Server) handleGetBatch(w http.ResponseWriter, r *http.Request) {
	id := strings.TrimPrefix(r.URL.Path, "/api/v1/batches/")
	id = strings.TrimSuffix(id, "/stats")
	id = strings.TrimSuffix(id, "/records")
	b, err := s.store.GetBatch(id)
	if err != nil {
		writeErr(w, http.StatusInternalServerError, err.Error())
		return
	}
	if b == nil {
		writeErr(w, http.StatusNotFound, "batch not found: "+id)
		return
	}
	writeJSON(w, http.StatusOK, b)
}

func (s *Server) handleBatchStats(w http.ResponseWriter, r *http.Request) {
	id := strings.TrimPrefix(r.URL.Path, "/api/v1/batches/")
	id = strings.TrimSuffix(id, "/stats")
	records, err := s.store.ListRecords(id)
	if err != nil {
		writeErr(w, http.StatusInternalServerError, err.Error())
		return
	}
	stats := BatchStats{Batch: id, ByResult: map[string]int{}, FailedDevices: []string{}}
	seen := map[string]bool{}
	for _, rec := range records {
		stats.Total++
		stats.ByResult[rec.Result]++
		if rec.Result == "PASSED" {
			stats.Passed++
		} else if !seen[rec.DeviceID] {
			seen[rec.DeviceID] = true
			stats.FailedDevices = append(stats.FailedDevices, rec.DeviceID)
		}
	}
	if stats.Total > 0 {
		stats.YieldPct = float64(stats.Passed) / float64(stats.Total) * 100
	}
	writeJSON(w, http.StatusOK, stats)
}

func (s *Server) handleAddRecord(w http.ResponseWriter, r *http.Request) {
	id := strings.TrimPrefix(r.URL.Path, "/api/v1/batches/")
	id = strings.TrimSuffix(id, "/records")
	b, err := s.store.GetBatch(id)
	if err != nil {
		writeErr(w, http.StatusInternalServerError, err.Error())
		return
	}
	if b == nil {
		writeErr(w, http.StatusNotFound, "batch not found: "+id)
		return
	}
	var req struct {
		DeviceID  string `json:"device_id"`
		Result    string `json:"result"`
		Detail    string `json:"detail"`
		Version   string `json:"firmware_version"`
		Package   string `json:"package_sha256"`
		FlashedAt string `json:"flashed_at"`
	}
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeErr(w, http.StatusBadRequest, "invalid JSON: "+err.Error())
		return
	}
	switch req.Result {
	case "PASSED", "FAILED", "DRY_RUN", "ERROR":
	default:
		writeErr(w, http.StatusBadRequest, "invalid result: "+req.Result)
		return
	}
	if req.DeviceID == "" {
		writeErr(w, http.StatusBadRequest, "device_id required")
		return
	}
	if req.Version == "" {
		req.Version = b.FirmwareVersion
	}
	if req.Package == "" {
		req.Package = b.PackageSHA256
	}
	if req.FlashedAt == "" {
		req.FlashedAt = nowISO()
	}
	records, err := s.store.ListRecords(id)
	if err != nil {
		writeErr(w, http.StatusInternalServerError, err.Error())
		return
	}
	prev := genesisHash
	if len(records) > 0 {
		prev = records[len(records)-1].RecordHash
	}
	rec := FlashRecord{
		DeviceID: req.DeviceID, FirmwareVersion: req.Version,
		PackageSHA256: req.Package, Result: req.Result, Detail: req.Detail,
		FlashedAt: req.FlashedAt, PrevHash: prev,
		RecordHash: recordHash(prev, id, req.DeviceID, req.Result,
			req.FlashedAt, req.Version, req.Package),
	}
	if err := s.store.AppendRecord(id, rec); err != nil {
		writeErr(w, http.StatusInternalServerError, err.Error())
		return
	}
	writeJSON(w, http.StatusCreated, rec)
}

func (s *Server) handleListRecords(w http.ResponseWriter, r *http.Request) {
	id := strings.TrimPrefix(r.URL.Path, "/api/v1/batches/")
	id = strings.TrimSuffix(id, "/records")
	records, err := s.store.ListRecords(id)
	if err != nil {
		writeErr(w, http.StatusInternalServerError, err.Error())
		return
	}
	writeJSON(w, http.StatusOK, map[string]any{"batch": id, "records": records,
		"total": len(records)})
}

func (s *Server) handleDevice(w http.ResponseWriter, r *http.Request) {
	dev := strings.TrimPrefix(r.URL.Path, "/api/v1/devices/")
	batches, err := s.store.ListBatches()
	if err != nil {
		writeErr(w, http.StatusInternalServerError, err.Error())
		return
	}
	// 扫描所有批次记录, 汇总设备状态
	type devState struct {
		DeviceID string `json:"device_id"`
		BatchID  string `json:"batch_id"`
		Status   string `json:"status"`
		LastFlash string `json:"last_flash_result"`
	}
	for _, b := range batches {
		records, err := s.store.ListRecords(b.ID)
		if err != nil {
			continue
		}
		for _, rec := range records {
			if rec.DeviceID == dev {
				st := "PENDING"
				if rec.Result == "PASSED" {
					st = "FLASHED"
				}
				writeJSON(w, http.StatusOK, devState{
					DeviceID: dev, BatchID: b.ID, Status: st,
					LastFlash: rec.Result,
				})
				return
			}
		}
	}
	writeErr(w, http.StatusNotFound, "device not found: "+dev)
}

func (s *Server) handleHealth(w http.ResponseWriter, r *http.Request) {
	writeJSON(w, http.StatusOK, map[string]string{"status": "ok"})
}

// ---------------------------------------------------------------------------
//  main
// ---------------------------------------------------------------------------

func main() {
	port := getenv("BATCH_API_PORT", "8080")
	dataDir := getenv("BATCH_API_DATA_DIR", "./data")
	apiKey := getenv("BATCH_API_KEY", "dev-key-change-me")

	store, err := storeFromEnv()
	if err != nil {
		log.Fatalf("store init: %v", err)
	}
	s := &Server{store: store, apiKey: apiKey}

	mux := http.NewServeMux()
	mux.HandleFunc("GET /healthz", s.handleHealth)
	mux.HandleFunc("POST /api/v1/batches", s.auth(s.handleCreateBatch))
	mux.HandleFunc("GET /api/v1/batches", s.auth(s.handleListBatches))
	mux.HandleFunc("GET /api/v1/batches/{id}", s.auth(s.handleGetBatch))
	mux.HandleFunc("GET /api/v1/batches/{id}/stats", s.auth(s.handleBatchStats))
	mux.HandleFunc("POST /api/v1/batches/{id}/records", s.auth(s.handleAddRecord))
	mux.HandleFunc("GET /api/v1/batches/{id}/records", s.auth(s.handleListRecords))
	mux.HandleFunc("GET /api/v1/devices/{id}", s.auth(s.handleDevice))

	log.Printf("batch-api listening on :%s (data=%s)", port, dataDir)
	if err := http.ListenAndServe(":"+port, mux); err != nil {
		log.Fatal(err)
	}
}

func getenv(k, def string) string {
	if v := os.Getenv(k); v != "" {
		return v
	}
	return def
}
