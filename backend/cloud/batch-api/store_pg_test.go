//go:build pg_test

// store_pg_test.go — PostgreSQL 存储集成测试
//
// 需要外部 PostgreSQL:
//   BATCH_TEST_PG_DSN=postgres://user:pass@host:5432/db go test -tags pg_test -run TestPG ./...
//
// 覆盖: schema 初始化 / 批次 CRUD / 记录追加与哈希链 / 唯一冲突 / JSONB 设备列表。
package main

import (
	"os"
	"testing"
)

func pgDSN(t *testing.T) string {
	t.Helper()
	dsn := os.Getenv("BATCH_TEST_PG_DSN")
	if dsn == "" {
		t.Skip("BATCH_TEST_PG_DSN 未设置, 跳过 PG 集成测试")
	}
	return dsn
}

func TestPGStoreFull(t *testing.T) {
	s, err := NewPGStore(pgDSN(t))
	if err != nil {
		t.Fatal(err)
	}
	defer s.Close()

	// 清理 (幂等)
	_, _ = s.db.Exec("TRUNCATE flash_records, batches CASCADE")

	// 创建批次 (含 JSONB 设备列表)
	b := Batch{
		ID: "B-PG-01", FirmwareVersion: "2.1.0",
		PackageSHA256: "aa" + t.Name(), SigningKeyID: "dev", EncKeyID: "dev",
		PlannedDevices: []string{"DK-0001", "DK-0002"},
		Status: "active", CreatedAt: nowISO(),
	}
	if err := s.CreateBatch(b); err != nil {
		t.Fatal(err)
	}
	got, err := s.GetBatch("B-PG-01")
	if err != nil {
		t.Fatal(err)
	}
	if got.FirmwareVersion != "2.1.0" || len(got.PlannedDevices) != 2 {
		t.Fatalf("batch roundtrip: %+v", got)
	}

	// 重复创建 → 唯一冲突
	if err := s.CreateBatch(b); err == nil {
		t.Fatal("want unique violation error")
	} else if err.Error() != "batch already exists: B-PG-01" {
		t.Fatalf("unexpected error: %v", err)
	}

	// 记录 + 哈希链
	r1 := FlashRecord{DeviceID: "DK-0001", FirmwareVersion: "2.1.0",
		PackageSHA256: "aa", Result: "PASSED", FlashedAt: nowISO(),
		PrevHash: genesisHash,
		RecordHash: recordHash(genesisHash, "B-PG-01", "DK-0001", "PASSED",
			nowISO(), "2.1.0", "aa")}
	r1.RecordHash = recordHash(genesisHash, "B-PG-01", "DK-0001", "PASSED",
		r1.FlashedAt, "2.1.0", "aa")
	if err := s.AppendRecord("B-PG-01", r1); err != nil {
		t.Fatal(err)
	}
	r2 := FlashRecord{DeviceID: "DK-0002", FirmwareVersion: "2.1.0",
		PackageSHA256: "aa", Result: "FAILED", Detail: "verifybin mismatch",
		FlashedAt: nowISO(), PrevHash: r1.RecordHash}
	r2.RecordHash = recordHash(r1.RecordHash, "B-PG-01", "DK-0002", "FAILED",
		r2.FlashedAt, "2.1.0", "aa")
	if err := s.AppendRecord("B-PG-01", r2); err != nil {
		t.Fatal(err)
	}

	records, err := s.ListRecords("B-PG-01")
	if err != nil {
		t.Fatal(err)
	}
	if len(records) != 2 {
		t.Fatalf("want 2 records, got %d", len(records))
	}
	// 哈希链连续性
	if records[1].PrevHash != records[0].RecordHash {
		t.Fatal("PG 哈希链断裂")
	}
	if records[1].Result != "FAILED" || records[1].Detail != "verifybin mismatch" {
		t.Fatalf("record fields: %+v", records[1])
	}

	// 列表
	batches, err := s.ListBatches()
	if err != nil {
		t.Fatal(err)
	}
	found := false
	for _, x := range batches {
		if x.ID == "B-PG-01" {
			found = true
		}
	}
	if !found {
		t.Fatal("batch not in list")
	}
}

func TestPGStoreNewStoreFactory(t *testing.T) {
	s, err := NewStore("postgres", "", pgDSN(t))
	if err != nil {
		t.Fatal(err)
	}
	defer s.(*PGStore).Close()
	if _, err := s.ListBatches(); err != nil {
		t.Fatal(err)
	}
}
