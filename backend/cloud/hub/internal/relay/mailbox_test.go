package relay

import (
	"context"
	"testing"
	"time"

	"go.uber.org/zap"
	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/relay/v1"
)

func TestCreateMailbox(t *testing.T) {
	logger := zap.NewNop()
	ctrl := NewMailboxController(logger)

	req := &pb.CreateMailboxRequest{
		SenderDeviceId:    "device-001",
		SenderVendor:      "apple",
		NotificationToken: "apns-token-xxx",
		DisplayInfo:       []byte(`{"brand":"Tesla","model":"Model 3"}`),
		Payload:           []byte(`{"format":"digitalwallet.carkey.ccc","content":{"genericSharingData":{}}}`),
		Config: &pb.MailboxConfig{
			AccessRights:      pb.AccessRights_READ_WRITE_DELETE,
			ExpirationSeconds: 3600,
		},
	}

	mb, err := ctrl.Create(context.Background(), req)
	if err != nil {
		t.Fatalf("Create failed: %v", err)
	}
	if mb.MailboxId == "" {
		t.Error("expected non-empty mailbox_id")
	}
	if mb.SenderVendor != "apple" {
		t.Errorf("expected apple, got %s", mb.SenderVendor)
	}
	if mb.Status != pb.MailboxStatus_CREATED {
		t.Errorf("expected CREATED, got %v", mb.Status)
	}
	if mb.SharingUrl == "" {
		t.Error("expected non-empty sharing URL")
	}
	if mb.ExpiresAt <= mb.CreatedAt {
		t.Error("expires_at should be after created_at")
	}
}

func TestCreateAndUpdateMailbox(t *testing.T) {
	logger := zap.NewNop()
	ctrl := NewMailboxController(logger)

	// 1. Create
	createReq := &pb.CreateMailboxRequest{
		SenderDeviceId: "sender-001",
		SenderVendor:   "apple",
		Payload:        []byte("key-creation-data"),
		Config:         &pb.MailboxConfig{ExpirationSeconds: 3600},
	}
	mb, err := ctrl.Create(context.Background(), createReq)
	if err != nil {
		t.Fatalf("Create failed: %v", err)
	}

	// 2. Receiver updates (KeySigningRequest)
	updateReq := &pb.UpdateMailboxRequest{
		MailboxId:       mb.MailboxId,
		Payload:         []byte("key-signing-data"),
		SharingDataType: 2, // KeySigningRequest
		UpdaterDeviceId: "receiver-001",
	}
	updated, err := ctrl.Update(context.Background(), updateReq)
	if err != nil {
		t.Fatalf("Update failed: %v", err)
	}
	if updated.Status != pb.MailboxStatus_UPDATED_BY_RECEIVER {
		t.Errorf("expected UPDATED_BY_RECEIVER, got %v", updated.Status)
	}

	// 3. Sender updates (ImportRequest)
	updateReq2 := &pb.UpdateMailboxRequest{
		MailboxId:       mb.MailboxId,
		Payload:         []byte("import-data"),
		SharingDataType: 3, // ImportRequest
		UpdaterDeviceId: "sender-001",
	}
	updated2, err := ctrl.Update(context.Background(), updateReq2)
	if err != nil {
		t.Fatalf("second Update failed: %v", err)
	}
	if updated2.Status != pb.MailboxStatus_UPDATED_BY_SENDER {
		t.Errorf("expected UPDATED_BY_SENDER, got %v", updated2.Status)
	}

	// 4. Read secure content
	payload, version, err := ctrl.ReadSecureContent(context.Background(), mb.MailboxId)
	if err != nil {
		t.Fatalf("ReadSecureContent failed: %v", err)
	}
	if string(payload) != "import-data" {
		t.Errorf("expected import-data, got %s", string(payload))
	}
	if version != 3 {
		t.Errorf("expected version 3, got %d", version)
	}
}

func TestMailboxExpiry(t *testing.T) {
	logger := zap.NewNop()
	ctrl := NewMailboxController(logger)
	now := time.Now()

	ctrl.now = func() time.Time { return now }

	createReq := &pb.CreateMailboxRequest{
		SenderDeviceId: "sender-001",
		SenderVendor:   "apple",
		Payload:        []byte("data"),
		Config:         &pb.MailboxConfig{ExpirationSeconds: 3600},
	}
	_, err := ctrl.Create(context.Background(), createReq)
	if err != nil {
		t.Fatalf("Create failed: %v", err)
	}

	// Advance time by 2 hours
	ctrl.now = func() time.Time { return now.Add(2 * time.Hour) }

	// 读取已过期，所以 ExpireScan 不再重复标记
	// 直接创建另一个邮箱来测试 ExpireScan
	mb, _ := ctrl.Create(context.Background(), &pb.CreateMailboxRequest{
		SenderDeviceId: "sender-002",
		SenderVendor:   "samsung",
		Payload:        []byte("data2"),
		Config:         &pb.MailboxConfig{ExpirationSeconds: 3600},
	})
	_ = mb

	// 过期扫描应找到 mb2（读取过 mb 已过期）
	expired := ctrl.ExpireScan()
	if expired != 1 {
		t.Errorf("expected 1 expired, got %d", expired)
	}
}

func TestMailboxCancel(t *testing.T) {
	logger := zap.NewNop()
	ctrl := NewMailboxController(logger)

	createReq := &pb.CreateMailboxRequest{
		SenderDeviceId: "sender-001",
		SenderVendor:   "apple",
		Payload:        []byte("data"),
		Config:         &pb.MailboxConfig{ExpirationSeconds: 3600},
	}
	mb, err := ctrl.Create(context.Background(), createReq)
	if err != nil {
		t.Fatalf("Create failed: %v", err)
	}

	// Cancel by sender
	_, err = ctrl.Update(context.Background(), &pb.UpdateMailboxRequest{
		MailboxId:       mb.MailboxId,
		SharingDataType: 4, // SenderCancel
		UpdaterDeviceId: "sender-001",
	})
	if err != nil {
		t.Errorf("cancel should succeed, got: %v", err)
	}

	// Verify cancelled
	mb2, ok := ctrl.Get(mb.MailboxId)
	if !ok {
		t.Fatal("mailbox should exist")
	}
	if mb2.Status != StatusCancelled {
		t.Errorf("expected CANCELLED, got %v", mb2.Status)
	}
}

func TestReadDisplayInfo(t *testing.T) {
	logger := zap.NewNop()
	ctrl := NewMailboxController(logger)

	req := &pb.CreateMailboxRequest{
		SenderDeviceId: "device-001",
		SenderVendor:   "apple",
		DisplayInfo:    []byte(`{"brand":"Tesla","model":"Model Y","year":"2026"}`),
		Payload:        []byte("data"),
		Config:         &pb.MailboxConfig{ExpirationSeconds: 3600},
	}
	mb, err := ctrl.Create(context.Background(), req)
	if err != nil {
		t.Fatalf("Create failed: %v", err)
	}

	info, version, err := ctrl.ReadDisplayInfo(context.Background(), mb.MailboxId)
	if err != nil {
		t.Fatal(err)
	}
	if string(info) != `{"brand":"Tesla","model":"Model Y","year":"2026"}` {
		t.Errorf("unexpected display info: %s", string(info))
	}
	if version != 1 {
		t.Errorf("expected version 1, got %d", version)
	}
}

func TestRelinquishMailbox(t *testing.T) {
	logger := zap.NewNop()
	ctrl := NewMailboxController(logger)

	req := &pb.CreateMailboxRequest{
		SenderDeviceId: "phone-001",
		SenderVendor:   "apple",
		Payload:        []byte("data"),
		Config:         &pb.MailboxConfig{ExpirationSeconds: 3600},
	}
	mb, err := ctrl.Create(context.Background(), req)
	if err != nil {
		t.Fatalf("Create failed: %v", err)
	}

	rel, err := ctrl.Relinquish(context.Background(), &pb.RelinquishMailboxRequest{
		MailboxId:     mb.MailboxId,
		FromDeviceId:  "phone-001",
		ToDeviceId:    "tablet-001",
	})
	if err != nil {
		t.Fatal(err)
	}
	if rel.ReceiverDeviceId != "tablet-001" {
		t.Errorf("expected tablet-001, got %s", rel.ReceiverDeviceId)
	}
}

func TestDeleteMailbox(t *testing.T) {
	logger := zap.NewNop()
	ctrl := NewMailboxController(logger)

	req := &pb.CreateMailboxRequest{
		SenderDeviceId: "sender-001",
		SenderVendor:   "apple",
		Payload:        []byte("data"),
		Config:         &pb.MailboxConfig{ExpirationSeconds: 3600},
	}
	mb, err := ctrl.Create(context.Background(), req)
	if err != nil {
		t.Fatalf("Create failed: %v", err)
	}

	_, err = ctrl.Delete(context.Background(), &pb.DeleteMailboxRequest{
		MailboxId:       mb.MailboxId,
		Reason:          "completed",
		DeleterDeviceId: "receiver-001",
	})
	if err != nil {
		t.Fatal(err)
	}

	// Should not be found after delete
	_, _, err = ctrl.ReadSecureContent(context.Background(), mb.MailboxId)
	if err == nil {
		t.Error("expected error after delete, got nil")
	}
}
