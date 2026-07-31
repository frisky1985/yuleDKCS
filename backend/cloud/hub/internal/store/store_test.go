package store

import (
	"context"
	"os"
	"testing"
	"time"

	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/relay"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/service"
)

// testDSN 返回测试数据库连接串。
// 未设置 TEST_DATABASE_URL 时跳过集成测试（CI 或本地无 PG 时）。
func testDSN(t *testing.T) string {
	t.Helper()
	dsn := os.Getenv("TEST_DATABASE_URL")
	if dsn == "" {
		t.Skip("TEST_DATABASE_URL not set — skipping postgres integration test")
	}
	return dsn
}

func newTestStore(t *testing.T) *PostgresStore {
	t.Helper()
	ctx := context.Background()
	s, err := NewPostgresStore(ctx, testDSN(t))
	if err != nil {
		t.Fatalf("NewPostgresStore: %v", err)
	}
	t.Cleanup(s.Close)

	// 清理测试数据（测试库专用，直接清空表）
	if _, err := s.pool.Exec(ctx, `TRUNCATE keys, mailboxes`); err != nil {
		t.Fatalf("truncate test tables: %v", err)
	}
	return s
}

// ─── KeyStore ─────────────────────────────────────────────

func TestKeyStoreCRUD(t *testing.T) {
	ctx := context.Background()
	s := newTestStore(t)

	rec := &service.KeyRecord{
		KeyID:       "key-int-001",
		OwnerUserID: "user-int-001",
		VehicleID:   "VH-INT-001",
		Vendor:      "apple",
		Status:      "active",
		CreatedAt:   time.Now().UnixMilli(),
	}

	// Create
	if err := s.SetKey(ctx, rec); err != nil {
		t.Fatalf("SetKey create: %v", err)
	}

	// GetKeyRecord
	got, err := s.GetKeyRecord(ctx, rec.KeyID)
	if err != nil {
		t.Fatalf("GetKeyRecord: %v", err)
	}
	if got.VehicleID != rec.VehicleID || got.Status != "active" {
		t.Fatalf("unexpected record: %+v", got)
	}

	// GetKeyOwner / GetKeyStatus
	owner, err := s.GetKeyOwner(ctx, rec.KeyID)
	if err != nil || owner != "user-int-001" {
		t.Fatalf("GetKeyOwner: owner=%q err=%v", owner, err)
	}
	status, err := s.GetKeyStatus(ctx, rec.KeyID)
	if err != nil || status != "active" {
		t.Fatalf("GetKeyStatus: status=%q err=%v", status, err)
	}

	// SetKeyStatus (upsert status)
	if err := s.SetKeyStatus(ctx, rec.KeyID, "suspended"); err != nil {
		t.Fatalf("SetKeyStatus: %v", err)
	}
	status, _ = s.GetKeyStatus(ctx, rec.KeyID)
	if status != "suspended" {
		t.Fatalf("status after update = %q, want suspended", status)
	}

	// ListKeysByUser
	list, err := s.ListKeysByUser(ctx, "user-int-001")
	if err != nil {
		t.Fatalf("ListKeysByUser: %v", err)
	}
	if len(list) != 1 || list[0].KeyID != rec.KeyID {
		t.Fatalf("unexpected list: %+v", list)
	}

	// 覆盖更新 (UPSERT)
	rec.Status = "revoked"
	if err := s.SetKey(ctx, rec); err != nil {
		t.Fatalf("SetKey upsert: %v", err)
	}
	status, _ = s.GetKeyStatus(ctx, rec.KeyID)
	if status != "revoked" {
		t.Fatalf("status after upsert = %q, want revoked", status)
	}
}

func TestKeyStoreNotFound(t *testing.T) {
	ctx := context.Background()
	s := newTestStore(t)

	if _, err := s.GetKeyRecord(ctx, "nonexistent"); err == nil {
		t.Fatal("expected error for missing key")
	}
	if _, err := s.GetKeyOwner(ctx, "nonexistent"); err == nil {
		t.Fatal("expected error for missing key owner")
	}
	if err := s.SetKeyStatus(ctx, "nonexistent", "active"); err == nil {
		t.Fatal("expected error for missing key status update")
	}
}

// ─── MailboxStore ─────────────────────────────────────────

func TestMailboxStoreCRUD(t *testing.T) {
	ctx := context.Background()
	s := newTestStore(t)

	now := time.Now()
	mb := &relay.Mailbox{
		ID:             "mb-int-001",
		Status:         relay.StatusCreated,
		SenderDeviceID: "iphone-001",
		SenderVendor:   "apple",
		DisplayInfo:    []byte(`{"brand":"BMW"}`),
		Payload:        []byte(`{"encrypted":"data"}`),
		SharingURL:     "https://dk-relay.yuletech.com/mailbox/mb-int-001",
		CreatedAt:      now,
		ExpiresAt:      now.Add(24 * time.Hour),
		UpdatedAt:      now,
		Version:        1,
		UpdateCount:    1,
		MaxUpdates:     10,
	}

	// Create
	if err := s.Create(ctx, mb); err != nil {
		t.Fatalf("Create: %v", err)
	}

	// Get
	got, err := s.Get(ctx, mb.ID)
	if err != nil {
		t.Fatalf("Get: %v", err)
	}
	if got.SenderVendor != "apple" || got.Version != 1 {
		t.Fatalf("unexpected mailbox: %+v", got)
	}

	// Update
	got.Status = relay.StatusUpdatedByReceiver
	got.Version = 2
	got.ReceiverDeviceID = "samsung-002"
	if err := s.Update(ctx, got); err != nil {
		t.Fatalf("Update: %v", err)
	}
	re, err := s.Get(ctx, mb.ID)
	if err != nil {
		t.Fatalf("Get after update: %v", err)
	}
	if re.Status != relay.StatusUpdatedByReceiver || re.Version != 2 {
		t.Fatalf("update not persisted: %+v", re)
	}

	// ListExpired
	expiredMB := &relay.Mailbox{
		ID:        "mb-int-expired",
		Status:    relay.StatusCreated,
		CreatedAt: now.Add(-48 * time.Hour),
		ExpiresAt: now.Add(-24 * time.Hour), // 已过期
		UpdatedAt: now.Add(-48 * time.Hour),
		Version:   1,
	}
	if err := s.Create(ctx, expiredMB); err != nil {
		t.Fatalf("Create expired: %v", err)
	}

	expired, err := s.ListExpired(ctx, now)
	if err != nil {
		t.Fatalf("ListExpired: %v", err)
	}
	found := false
	for _, e := range expired {
		if e.ID == "mb-int-expired" {
			found = true
		}
	}
	if !found {
		t.Fatalf("expired mailbox not found in ListExpired: %+v", expired)
	}

	// Delete
	if err := s.Delete(ctx, mb.ID); err != nil {
		t.Fatalf("Delete: %v", err)
	}
	if _, err := s.Get(ctx, mb.ID); err != relay.ErrMailboxNotFound {
		t.Fatalf("Get after delete: got %v, want ErrMailboxNotFound", err)
	}
}

func TestMailboxStoreNotFound(t *testing.T) {
	ctx := context.Background()
	s := newTestStore(t)

	if _, err := s.Get(ctx, "nonexistent"); err != relay.ErrMailboxNotFound {
		t.Fatalf("Get: got %v, want ErrMailboxNotFound", err)
	}
	if err := s.Update(ctx, &relay.Mailbox{ID: "nonexistent"}); err != relay.ErrMailboxNotFound {
		t.Fatalf("Update: got %v, want ErrMailboxNotFound", err)
	}
	if err := s.Delete(ctx, "nonexistent"); err != relay.ErrMailboxNotFound {
		t.Fatalf("Delete: got %v, want ErrMailboxNotFound", err)
	}
}
