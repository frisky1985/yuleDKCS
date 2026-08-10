// store_pg.go — PostgreSQL 存储实现
//
// 使用 github.com/jackc/pgx/v5/stdlib (database/sql 兼容驱动)。
// schema 与工厂侧 batch_manager.py (SQLite) / FileStore 对齐。
package main

import (
	"database/sql"
	"encoding/json"
	"fmt"

	_ "github.com/jackc/pgx/v5/stdlib"
)

const pgSchema = `
CREATE TABLE IF NOT EXISTS batches (
    id               TEXT PRIMARY KEY,
    firmware_version TEXT NOT NULL,
    package_sha256   TEXT NOT NULL,
    signing_key_id   TEXT NOT NULL,
    enc_key_id       TEXT NOT NULL,
    planned_devices  JSONB,
    status           TEXT NOT NULL DEFAULT 'active',
    created_at       TIMESTAMPTZ NOT NULL
);
CREATE TABLE IF NOT EXISTS flash_records (
    id               BIGSERIAL PRIMARY KEY,
    batch_id         TEXT NOT NULL REFERENCES batches(id),
    device_id        TEXT NOT NULL,
    firmware_version TEXT NOT NULL,
    package_sha256   TEXT NOT NULL,
    result           TEXT NOT NULL,
    detail           TEXT,
    flashed_at       TIMESTAMPTZ NOT NULL,
    prev_hash        TEXT NOT NULL,
    record_hash      TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_records_batch ON flash_records(batch_id);
`

type PGStore struct {
	db *sql.DB
}

func NewPGStore(dsn string) (*PGStore, error) {
	db, err := sql.Open("pgx", dsn)
	if err != nil {
		return nil, fmt.Errorf("pg open: %w", err)
	}
	if err := db.Ping(); err != nil {
		db.Close()
		return nil, fmt.Errorf("pg ping: %w", err)
	}
	if _, err := db.Exec(pgSchema); err != nil {
		db.Close()
		return nil, fmt.Errorf("pg schema: %w", err)
	}
	return &PGStore{db: db}, nil
}

func (s *PGStore) Close() error { return s.db.Close() }

func (s *PGStore) ListBatches() ([]Batch, error) {
	rows, err := s.db.Query(
		`SELECT id, firmware_version, package_sha256, signing_key_id, enc_key_id,
		        planned_devices, status, created_at
		 FROM batches ORDER BY created_at DESC`)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	batches := []Batch{}
	for rows.Next() {
		var b Batch
		var devices []byte
		var created string
		if err := rows.Scan(&b.ID, &b.FirmwareVersion, &b.PackageSHA256,
			&b.SigningKeyID, &b.EncKeyID, &devices, &b.Status, &created); err != nil {
			return nil, err
		}
		b.CreatedAt = created
		if len(devices) > 0 {
			_ = json.Unmarshal(devices, &b.PlannedDevices)
		}
		batches = append(batches, b)
	}
	return batches, rows.Err()
}

func (s *PGStore) GetBatch(id string) (*Batch, error) {
	row := s.db.QueryRow(
		`SELECT id, firmware_version, package_sha256, signing_key_id, enc_key_id,
		        planned_devices, status, created_at
		 FROM batches WHERE id = $1`, id)
	var b Batch
	var devices []byte
	var created string
	err := row.Scan(&b.ID, &b.FirmwareVersion, &b.PackageSHA256,
		&b.SigningKeyID, &b.EncKeyID, &devices, &b.Status, &created)
	if err == sql.ErrNoRows {
		return nil, nil
	}
	if err != nil {
		return nil, err
	}
	b.CreatedAt = created
	if len(devices) > 0 {
		_ = json.Unmarshal(devices, &b.PlannedDevices)
	}
	return &b, nil
}

func (s *PGStore) CreateBatch(b Batch) error {
	devices, _ := json.Marshal(b.PlannedDevices)
	_, err := s.db.Exec(
		`INSERT INTO batches (id, firmware_version, package_sha256,
		                      signing_key_id, enc_key_id, planned_devices,
		                      status, created_at)
		 VALUES ($1,$2,$3,$4,$5,$6,$7,$8)`,
		b.ID, b.FirmwareVersion, b.PackageSHA256, b.SigningKeyID, b.EncKeyID,
		string(devices), b.Status, b.CreatedAt)
	if err != nil && isUniqueViolation(err) {
		return errBatchExists(b.ID)
	}
	return err
}

func (s *PGStore) ListRecords(batchID string) ([]FlashRecord, error) {
	rows, err := s.db.Query(
		`SELECT device_id, firmware_version, package_sha256, result, detail,
		        flashed_at, prev_hash, record_hash
		 FROM flash_records WHERE batch_id = $1 ORDER BY id`, batchID)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	records := []FlashRecord{}
	for rows.Next() {
		var r FlashRecord
		var flashed string
		var detail sql.NullString
		if err := rows.Scan(&r.DeviceID, &r.FirmwareVersion, &r.PackageSHA256,
			&r.Result, &detail, &flashed, &r.PrevHash, &r.RecordHash); err != nil {
			return nil, err
		}
		r.Detail = detail.String
		r.FlashedAt = flashed
		records = append(records, r)
	}
	return records, rows.Err()
}

func (s *PGStore) AppendRecord(batchID string, r FlashRecord) error {
	_, err := s.db.Exec(
		`INSERT INTO flash_records (batch_id, device_id, firmware_version,
		                           package_sha256, result, detail, flashed_at,
		                           prev_hash, record_hash)
		 VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9)`,
		batchID, r.DeviceID, r.FirmwareVersion, r.PackageSHA256, r.Result,
		r.Detail, r.FlashedAt, r.PrevHash, r.RecordHash)
	return err
}

// 辅助: 唯一约束冲突检测 (postgres 23505)
func isUniqueViolation(err error) bool {
	type sqlErr interface{ SQLState() string }
	if se, ok := err.(sqlErr); ok {
		return se.SQLState() == "23505"
	}
	return false
}

var _ Store = (*PGStore)(nil)
var _ Store = (*FileStore)(nil)
