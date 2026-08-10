// store_file.go — 文件 JSON 存储实现 (默认, 零依赖)
package main

import (
	"encoding/json"
	"os"
	"path/filepath"
)

type FileStore struct {
	dir string
}

func NewFileStore(dir string) (*FileStore, error) {
	if err := os.MkdirAll(dir, 0o755); err != nil {
		return nil, err
	}
	return &FileStore{dir: dir}, nil
}

func (s *FileStore) batchesPath() string { return filepath.Join(s.dir, "batches.json") }
func (s *FileStore) recordsPath(id string) string {
	return filepath.Join(s.dir, "records", id+".json")
}

func (s *FileStore) ListBatches() ([]Batch, error) {
	data, err := os.ReadFile(s.batchesPath())
	if err != nil {
		if os.IsNotExist(err) {
			return []Batch{}, nil
		}
		return nil, err
	}
	var batches []Batch
	if err := json.Unmarshal(data, &batches); err != nil {
		return nil, err
	}
	return batches, nil
}

func (s *FileStore) saveBatches(batches []Batch) error {
	data, err := json.MarshalIndent(batches, "", "  ")
	if err != nil {
		return err
	}
	return os.WriteFile(s.batchesPath(), data, 0o644)
}

func (s *FileStore) GetBatch(id string) (*Batch, error) {
	batches, err := s.ListBatches()
	if err != nil {
		return nil, err
	}
	for i := range batches {
		if batches[i].ID == id {
			return &batches[i], nil
		}
	}
	return nil, nil
}

func (s *FileStore) CreateBatch(b Batch) error {
	batches, err := s.ListBatches()
	if err != nil {
		return err
	}
	for _, x := range batches {
		if x.ID == b.ID {
			return errBatchExists(b.ID)
		}
	}
	batches = append(batches, b)
	return s.saveBatches(batches)
}

func (s *FileStore) ListRecords(batchID string) ([]FlashRecord, error) {
	data, err := os.ReadFile(s.recordsPath(batchID))
	if err != nil {
		if os.IsNotExist(err) {
			return []FlashRecord{}, nil
		}
		return nil, err
	}
	var records []FlashRecord
	if err := json.Unmarshal(data, &records); err != nil {
		return nil, err
	}
	return records, nil
}

func (s *FileStore) AppendRecord(batchID string, r FlashRecord) error {
	records, err := s.ListRecords(batchID)
	if err != nil {
		return err
	}
	records = append(records, r)
	if err := os.MkdirAll(filepath.Dir(s.recordsPath(batchID)), 0o755); err != nil {
		return err
	}
	data, err := json.MarshalIndent(records, "", "  ")
	if err != nil {
		return err
	}
	return os.WriteFile(s.recordsPath(batchID), data, 0o644)
}
