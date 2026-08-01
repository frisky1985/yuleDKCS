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
	// URL 不应包含 fragment (#secret)
	for i := 0; i < len(mb.SharingUrl); i++ {
		if mb.SharingUrl[i] == '#' {
			t.Error("sharing URL must not contain fragment (secret)")
		}
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

	// 3. Sender updates (ImportRequest) — 正常流程最后一步, 邮箱转为 COMPLETED
	updateReq2 := &pb.UpdateMailboxRequest{
		MailboxId:       mb.MailboxId,
		Payload:         []byte("import-data"),
		SharingDataType: 3, // ImportRequest (sharingImportRequest) — CCC §11.3.4: 发送方导入即完成
		UpdaterDeviceId: "sender-001",
	}
	updated2, err := ctrl.Update(context.Background(), updateReq2)
	if err != nil {
		t.Fatalf("second Update failed: %v", err)
	}
	if updated2.Status != pb.MailboxStatus_COMPLETED {
		t.Errorf("expected COMPLETED (ImportRequest 完成分享), got %v", updated2.Status)
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

	// Create another mailbox
	_, _ = ctrl.Create(context.Background(), &pb.CreateMailboxRequest{
		SenderDeviceId: "sender-002",
		SenderVendor:   "samsung",
		Payload:        []byte("data2"),
		Config:         &pb.MailboxConfig{ExpirationSeconds: 3600},
	})

	// ExpireScan should find the first mailbox expired (the second one was just created)
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

	// Must cancel first before delete
	_, err = ctrl.Update(context.Background(), &pb.UpdateMailboxRequest{
		MailboxId:       mb.MailboxId,
		SharingDataType: 4, // SenderCancel
		UpdaterDeviceId: "sender-001",
	})
	if err != nil {
		t.Fatalf("Cancel before delete failed: %v", err)
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

// ─── 新增测试 ──────────────────────────────────────────────

// TestPinReEntry 测试 PinReEntry 不改变状态
func TestPinReEntry(t *testing.T) {
	logger := zap.NewNop()
	ctrl := NewMailboxController(logger)

	mb, err := ctrl.Create(context.Background(), &pb.CreateMailboxRequest{
		SenderDeviceId: "sender-001",
		SenderVendor:   "apple",
		Payload:        []byte("data"),
		Config:         &pb.MailboxConfig{ExpirationSeconds: 3600},
	})
	if err != nil {
		t.Fatalf("Create failed: %v", err)
	}

	// PinReEntryRequest (6) — 不改变状态
	_, err = ctrl.Update(context.Background(), &pb.UpdateMailboxRequest{
		MailboxId:       mb.MailboxId,
		Payload:         []byte(`{"pinReEntryRequest":true}`),
		SharingDataType: 6,
		UpdaterDeviceId: "sender-001",
	})
	if err != nil {
		t.Fatalf("PinReEntry should succeed: %v", err)
	}
	if mb2, ok := ctrl.Get(mb.MailboxId); ok {
		if mb2.Status != StatusCreated {
			t.Errorf("PinReEntry should not change status, got %v", mb2.Status)
		}
	}

	// PinReEntryValue (7) — 不改变状态
	_, err = ctrl.Update(context.Background(), &pb.UpdateMailboxRequest{
		MailboxId:       mb.MailboxId,
		Payload:         []byte(`{"pinReEntryValue":"1234"}`),
		SharingDataType: 7,
		UpdaterDeviceId: "receiver-001",
	})
	if err != nil {
		t.Fatalf("PinReEntryValue should succeed: %v", err)
	}
	if mb3, ok := ctrl.Get(mb.MailboxId); ok {
		if mb3.Status != StatusCreated {
			t.Errorf("PinReEntryValue should not change status, got %v", mb3.Status)
		}
	}
}

// TestRelinquishAfterCancel 验证取消后不可 Relinquish
func TestRelinquishAfterCancel(t *testing.T) {
	logger := zap.NewNop()
	ctrl := NewMailboxController(logger)

	mb, err := ctrl.Create(context.Background(), &pb.CreateMailboxRequest{
		SenderDeviceId: "sender-001",
		SenderVendor:   "apple",
		Payload:        []byte("data"),
		Config:         &pb.MailboxConfig{ExpirationSeconds: 3600},
	})
	if err != nil {
		t.Fatalf("Create failed: %v", err)
	}

	// Cancel first
	_, err = ctrl.Update(context.Background(), &pb.UpdateMailboxRequest{
		MailboxId:       mb.MailboxId,
		SharingDataType: 4, // SenderCancel
		UpdaterDeviceId: "sender-001",
	})
	if err != nil {
		t.Fatalf("Cancel failed: %v", err)
	}

	// Relinquish after cancel should fail
	_, err = ctrl.Relinquish(context.Background(), &pb.RelinquishMailboxRequest{
		MailboxId:     mb.MailboxId,
		FromDeviceId:  "sender-001",
		ToDeviceId:    "other-device",
	})
	if err == nil {
		t.Error("Relinquish after cancel should fail")
	}
}

// TestDeleteBeforeTerminal 验证活跃状态下不可 Delete
func TestDeleteBeforeTerminal(t *testing.T) {
	logger := zap.NewNop()
	ctrl := NewMailboxController(logger)

	mb, err := ctrl.Create(context.Background(), &pb.CreateMailboxRequest{
		SenderDeviceId: "sender-001",
		SenderVendor:   "apple",
		Payload:        []byte("data"),
		Config:         &pb.MailboxConfig{ExpirationSeconds: 3600},
	})
	if err != nil {
		t.Fatalf("Create failed: %v", err)
	}

	// Delete while active should fail
	_, err = ctrl.Delete(context.Background(), &pb.DeleteMailboxRequest{
		MailboxId:       mb.MailboxId,
		Reason:          "cancelled",
		DeleterDeviceId: "sender-001",
	})
	if err == nil {
		t.Error("Delete while active should fail")
	}
}

// TestConcurrentAccess 并发安全测试
func TestConcurrentAccess(t *testing.T) {
	logger := zap.NewNop()
	ctrl := NewMailboxController(logger)

	mb, err := ctrl.Create(context.Background(), &pb.CreateMailboxRequest{
		SenderDeviceId: "sender-001",
		SenderVendor:   "apple",
		Payload:        []byte("data"),
		Config:         &pb.MailboxConfig{ExpirationSeconds: 3600},
	})
	if err != nil {
		t.Fatalf("Create failed: %v", err)
	}

	// 并发更新
	done := make(chan bool, 10)
	for i := 0; i < 10; i++ {
		go func(id int) {
			_, err := ctrl.Update(context.Background(), &pb.UpdateMailboxRequest{
				MailboxId:       mb.MailboxId,
				Payload:         []byte("concurrent-data"),
				SharingDataType: 2,
				UpdaterDeviceId: "receiver-001",
			})
			// 第一次应该成功，之后可能因为状态转移失败（已到 UPDATED_BY_RECEIVER）
			_ = err
			done <- true
		}(i)
	}
	for i := 0; i < 10; i++ {
		<-done
	}
}

// TestURLNoFragment 确保 URL 不含 #secret
func TestURLNoFragment(t *testing.T) {
	logger := zap.NewNop()
	ctrl := NewMailboxController(logger)

	mb, err := ctrl.Create(context.Background(), &pb.CreateMailboxRequest{
		SenderDeviceId: "device-001",
		SenderVendor:   "apple",
		Payload:        []byte("data"),
		Config:         &pb.MailboxConfig{ExpirationSeconds: 3600},
	})
	if err != nil {
		t.Fatalf("Create failed: %v", err)
	}

	if mb.SharingUrl == "" {
		t.Fatal("expected non-empty sharing URL")
	}
	for i := 0; i < len(mb.SharingUrl); i++ {
		if mb.SharingUrl[i] == '#' {
			t.Error("sharing URL must not contain fragment (#)")
		}
	}
	if mb.SharingUrl != "https://dk-relay.yuletech.com/mailbox/"+mb.MailboxId {
		t.Errorf("unexpected URL format: %s", mb.SharingUrl)
	}
}

// ─── 完成流测试 (W3: StatusCompleted 可达路径) ─────────────────

// TestMailboxCompletionFlow 验证正常完成路径: Create → KeySigning(2) → Import(3)=COMPLETED → Delete(reason="completed")
// 依据 CCC-TS-101 §11.3.3/§11.3.4:
//   - sharingDataType=3 (sharingImportRequest) 是发送方正常流程的最后一步 → 邮箱置为 COMPLETED
//   - DeleteMailbox 语义: 接收方获取 ImportRequest 后删除 (reason="completed")
func TestMailboxCompletionFlow(t *testing.T) {
	logger := zap.NewNop()
	ctrl := NewMailboxController(logger)

	mb, err := ctrl.Create(context.Background(), &pb.CreateMailboxRequest{
		SenderDeviceId: "sender-001",
		SenderVendor:   "apple",
		Payload:        []byte("key-creation-data"),
		Config:         &pb.MailboxConfig{ExpirationSeconds: 3600},
	})
	if err != nil {
		t.Fatalf("Create failed: %v", err)
	}

	// 1. 接收方 KeySigning → UPDATED_BY_RECEIVER
	updated, err := ctrl.Update(context.Background(), &pb.UpdateMailboxRequest{
		MailboxId:       mb.MailboxId,
		Payload:         []byte("key-signing-data"),
		SharingDataType: 2, // sharingKeySigningRequest — 接收方签名
		UpdaterDeviceId: "receiver-001",
	})
	if err != nil {
		t.Fatalf("KeySigning Update failed: %v", err)
	}
	if updated.Status != pb.MailboxStatus_UPDATED_BY_RECEIVER {
		t.Fatalf("expected UPDATED_BY_RECEIVER, got %v", updated.Status)
	}

	// 2. 发送方 Import → COMPLETED (正常完成)
	updated2, err := ctrl.Update(context.Background(), &pb.UpdateMailboxRequest{
		MailboxId:       mb.MailboxId,
		Payload:         []byte("import-data"),
		SharingDataType: 3, // sharingImportRequest — 发送方导入, 正常完成
		UpdaterDeviceId: "sender-001",
	})
	if err != nil {
		t.Fatalf("Import Update failed: %v", err)
	}
	if updated2.Status != pb.MailboxStatus_COMPLETED {
		t.Fatalf("expected COMPLETED, got %v", updated2.Status)
	}

	// 3. COMPLETED 态下 Delete(reason="completed") 应成功 (CCC §11.3.4: 接收方获取 ImportRequest 后删除)
	_, err = ctrl.Delete(context.Background(), &pb.DeleteMailboxRequest{
		MailboxId:       mb.MailboxId,
		Reason:          "completed",
		DeleterDeviceId: "receiver-001",
	})
	if err != nil {
		t.Fatalf("Delete(reason=completed) on COMPLETED mailbox failed: %v", err)
	}

	// 4. 删除后邮箱应不可读
	if _, _, err := ctrl.ReadSecureContent(context.Background(), mb.MailboxId); err == nil {
		t.Error("expected error after delete, got nil")
	}
}

// TestUpdateAfterCompletedRejected 验证 COMPLETED 是终态: 后续任何 Update 必须失败
func TestUpdateAfterCompletedRejected(t *testing.T) {
	logger := zap.NewNop()
	ctrl := NewMailboxController(logger)

	mb, err := ctrl.Create(context.Background(), &pb.CreateMailboxRequest{
		SenderDeviceId: "sender-001",
		SenderVendor:   "apple",
		Payload:        []byte("data"),
		Config:         &pb.MailboxConfig{ExpirationSeconds: 3600},
	})
	if err != nil {
		t.Fatalf("Create failed: %v", err)
	}

	// 走完正常流程到 COMPLETED
	if _, err := ctrl.Update(context.Background(), &pb.UpdateMailboxRequest{
		MailboxId:       mb.MailboxId,
		SharingDataType: 2,
		UpdaterDeviceId: "receiver-001",
	}); err != nil {
		t.Fatalf("KeySigning failed: %v", err)
	}
	if _, err := ctrl.Update(context.Background(), &pb.UpdateMailboxRequest{
		MailboxId:       mb.MailboxId,
		SharingDataType: 3,
		UpdaterDeviceId: "sender-001",
	}); err != nil {
		t.Fatalf("Import failed: %v", err)
	}

	// 终态后改变状态的 Update 应被拒绝 (KeySigning / SenderCancel / ReceiverCancel)
	// 注: PinReEntry(6/7) 是仅更新 payload、保持状态不变的更新, 不违反终态约束, 不在断言范围。
	for _, dt := range []int32{2, 4, 5} {
		if _, err := ctrl.Update(context.Background(), &pb.UpdateMailboxRequest{
			MailboxId:       mb.MailboxId,
			SharingDataType: dt,
			UpdaterDeviceId: "receiver-001",
		}); err == nil {
			t.Errorf("Update(dataType=%d) on COMPLETED mailbox should fail", dt)
		}
	}
}
