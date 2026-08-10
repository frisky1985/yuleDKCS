// store.go — 存储层抽象 (可插拔: file / postgres)
//
// 切换配置 (环境变量):
//   BATCH_API_STORE=file|postgres   (默认 file)
//   BATCH_API_PG_DSN=postgres://user:pass@host:5432/db
//
// 数据模型与工厂侧 batch_manager.py (SQLite) 对齐。
package main

import (
	"fmt"
)

// Store 存储接口 — handler 只依赖此接口, 存储引擎可替换。
type Store interface {
	ListBatches() ([]Batch, error)
	GetBatch(id string) (*Batch, error)
	CreateBatch(b Batch) error
	ListRecords(batchID string) ([]FlashRecord, error)
	AppendRecord(batchID string, r FlashRecord) error
}

// NewStore 按配置创建存储实现。
func NewStore(kind, dataDir, pgDSN string) (Store, error) {
	switch kind {
	case "file", "":
		return NewFileStore(dataDir)
	case "postgres", "pg":
		return NewPGStore(pgDSN)
	default:
		return nil, fmt.Errorf("unknown store kind: %s (file|postgres)", kind)
	}
}

// errBatchExists 统一错误 (file/pg 实现共用)
func errBatchExists(id string) error {
	return fmt.Errorf("batch already exists: %s", id)
}

// storeFromEnv 从环境变量创建存储。
func storeFromEnv() (Store, error) {
	kind := getenv("BATCH_API_STORE", "file")
	dataDir := getenv("BATCH_API_DATA_DIR", "./data")
	pgDSN := getenv("BATCH_API_PG_DSN", "")
	if (kind == "postgres" || kind == "pg") && pgDSN == "" {
		return nil, fmt.Errorf("BATCH_API_PG_DSN required for store=%s", kind)
	}
	return NewStore(kind, dataDir, pgDSN)
}
