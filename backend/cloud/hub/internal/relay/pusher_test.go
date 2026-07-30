package relay

import (
	"context"
	"testing"

	"go.uber.org/zap"
	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/relay/v1"
)

// ─── PushNotifier 接口测试 ────────────────────────────────────

func TestNoopPusher_DoesNothing(t *testing.T) {
	p := &NoopPusher{}
	err := p.Notify(context.Background(), PushMessage{
		Title: "test",
		Body:  "body",
		Token: "device-token",
	})
	if err != nil {
		t.Errorf("NoopPusher should never error: %v", err)
	}
}

func TestMockPusher_RecordsMessages(t *testing.T) {
	p := NewMockPusher()

	msg1 := PushMessage{Title: "Test 1", Body: "Body 1", Token: "token-1"}
	msg2 := PushMessage{Title: "Test 2", Body: "Body 2", Token: "token-2"}

	if err := p.Notify(context.Background(), msg1); err != nil {
		t.Fatalf("Notify failed: %v", err)
	}
	if err := p.Notify(context.Background(), msg2); err != nil {
		t.Fatalf("Notify failed: %v", err)
	}

	msgs := p.Messages()
	if len(msgs) != 2 {
		t.Fatalf("expected 2 messages, got %d", len(msgs))
	}
	if msgs[0].Title != "Test 1" {
		t.Errorf("expected 'Test 1', got %s", msgs[0].Title)
	}
	if msgs[1].Token != "token-2" {
		t.Errorf("expected 'token-2', got %s", msgs[1].Token)
	}
}

func TestMockPusher_Reset(t *testing.T) {
	p := NewMockPusher()
	_ = p.Notify(context.Background(), PushMessage{Title: "temp"})
	if len(p.Messages()) != 1 {
		t.Fatal("expected 1 message before reset")
	}
	p.Reset()
	if len(p.Messages()) != 0 {
		t.Fatal("expected 0 messages after reset")
	}
}

// ─── MailboxController + PushNotifier 集成测试 ────────────────

func TestMailboxPush_ReceiverUpdateNotifiesSender(t *testing.T) {
	logger := zap.NewNop()
	mockPusher := NewMockPusher()
	ctrl := NewMailboxController(logger).WithNotifier(mockPusher)
	ctx := context.Background()

	// 发送方创建邮箱
	createReq := &pb.CreateMailboxRequest{
		Payload:          []byte(`{"key":"data"}`),
		SenderDeviceId:   "sender-phone",
		SenderVendor:     "apple",
		NotificationToken: "sender-apns-token",  // 发送方 push token
		DisplayInfo:      []byte(`{"brand":"BMW"}`),
		Config: &pb.MailboxConfig{
			AccessRights:      pb.AccessRights_READ_WRITE_DELETE,
			ExpirationSeconds: 3600,
		},
	}

	mb, err := ctrl.Create(ctx, createReq)
	if err != nil {
		t.Fatalf("Create failed: %v", err)
	}

	// 创建时不应有通知
	if len(mockPusher.Messages()) != 0 {
		t.Fatal("expected no notification on create")
	}

	// 接收方更新邮箱（KeySigning）
	updateReq := &pb.UpdateMailboxRequest{
		MailboxId:        mb.MailboxId,
		Payload:          []byte(`{"signed":"data"}`),
		SharingDataType:  2, // KeySigning
		UpdaterDeviceId:  "receiver-phone",
		NotificationToken: "receiver-fcm-token",
	}

	_, err = ctrl.Update(ctx, updateReq)
	if err != nil {
		t.Fatalf("Update failed: %v", err)
	}

	// 应发送通知给发送方
	msgs := mockPusher.Messages()
	if len(msgs) != 1 {
		t.Fatalf("expected 1 push notification, got %d", len(msgs))
	}
	if msgs[0].Token != "sender-apns-token" {
		t.Errorf("expected notification to sender, got token: %s", msgs[0].Token)
	}
	if msgs[0].Title != "数字钥匙分享已更新" {
		t.Errorf("unexpected title: %s", msgs[0].Title)
	}
}

func TestMailboxPush_SenderImportNotifiesReceiver(t *testing.T) {
	logger := zap.NewNop()
	mockPusher := NewMockPusher()
	ctrl := NewMailboxController(logger).WithNotifier(mockPusher)
	ctx := context.Background()

	// 发送方创建邮箱
	createReq := &pb.CreateMailboxRequest{
		Payload:          []byte(`{"key":"data"}`),
		SenderDeviceId:   "sender-phone",
		SenderVendor:     "apple",
		NotificationToken: "sender-apns-token",
	}
	mb, err := ctrl.Create(ctx, createReq)
	if err != nil {
		t.Fatalf("Create failed: %v", err)
	}

	// 接收方更新（KeySigning）— 带上自己的 token
	_, err = ctrl.Update(ctx, &pb.UpdateMailboxRequest{
		MailboxId:        mb.MailboxId,
		Payload:          []byte(`{"signed":"data"}`),
		SharingDataType:  2,
		NotificationToken: "receiver-fcm-token",
	})
	if err != nil {
		t.Fatalf("First update failed: %v", err)
	}

	// 发送方 Import
	_, err = ctrl.Update(ctx, &pb.UpdateMailboxRequest{
		MailboxId:       mb.MailboxId,
		Payload:         []byte(`{"import":"success"}`),
		SharingDataType: 3, // Import
		UpdaterDeviceId: "sender-phone",
	})
	if err != nil {
		t.Fatalf("Import update failed: %v", err)
	}

	// 第二次更新应通知接收方（共 2 条通知）
	msgs := mockPusher.Messages()
	if len(msgs) != 2 {
		t.Fatalf("expected 2 notifications, got %d", len(msgs))
	}
	// 第2条应发给接收方
	if msgs[1].Token != "receiver-fcm-token" {
		t.Errorf("expected notification to receiver, got: %s", msgs[1].Token)
	}
	if msgs[1].Title != "钥匙导入成功 🎉" {
		t.Errorf("unexpected title: %s", msgs[1].Title)
	}
}

func TestMailboxPush_NoTokenNoNotification(t *testing.T) {
	logger := zap.NewNop()
	mockPusher := NewMockPusher()
	ctrl := NewMailboxController(logger).WithNotifier(mockPusher)
	ctx := context.Background()

	// 创建时不带 notification token
	mb, err := ctrl.Create(ctx, &pb.CreateMailboxRequest{
		Payload:        []byte(`{"key":"data"}`),
		SenderDeviceId: "sender-phone",
		SenderVendor:   "apple",
	})
	if err != nil {
		t.Fatalf("Create failed: %v", err)
	}

	// 接收方更新也不带 token
	_, err = ctrl.Update(ctx, &pb.UpdateMailboxRequest{
		MailboxId:        mb.MailboxId,
		Payload:          []byte(`{"signed":"data"}`),
		SharingDataType:  2,
		UpdaterDeviceId:  "receiver-phone",
	})
	if err != nil {
		t.Fatalf("Update failed: %v", err)
	}

	// 不应有任何通知
	if len(mockPusher.Messages()) != 0 {
		t.Fatal("expected no notifications when tokens are empty")
	}
}

func TestMailboxPush_CancelNotifiesOtherParty(t *testing.T) {
	logger := zap.NewNop()
	mockPusher := NewMockPusher()
	ctrl := NewMailboxController(logger).WithNotifier(mockPusher)
	ctx := context.Background()

	// 发送方创建（有 token）
	mb, err := ctrl.Create(ctx, &pb.CreateMailboxRequest{
		Payload:          []byte(`{"key":"data"}`),
		SenderDeviceId:   "sender-phone",
		SenderVendor:     "apple",
		NotificationToken: "sender-apns-token",
	})
	if err != nil {
		t.Fatalf("Create failed: %v", err)
	}

	// 接收方更新（带上自己的 token）
	_, err = ctrl.Update(ctx, &pb.UpdateMailboxRequest{
		MailboxId:        mb.MailboxId,
		Payload:          []byte(`{"signed":"data"}`),
		SharingDataType:  2,
		NotificationToken: "receiver-fcm-token",
	})
	if err != nil {
		t.Fatalf("First update failed: %v", err)
	}

	mockPusher.Reset() // 清掉前两条通知

	// 发送方取消
	_, err = ctrl.Update(ctx, &pb.UpdateMailboxRequest{
		MailboxId:       mb.MailboxId,
		SharingDataType: 4, // SenderCancel
	})
	if err != nil {
		t.Fatalf("Cancel failed: %v", err)
	}

	// 应通知接收方
	msgs := mockPusher.Messages()
	if len(msgs) != 1 {
		t.Fatalf("expected 1 cancellation notification, got %d", len(msgs))
	}
	if msgs[0].Token != "receiver-fcm-token" {
		t.Errorf("expected notification to receiver, got: %s", msgs[0].Token)
	}
	if msgs[0].Title != "钥匙分享已取消" {
		t.Errorf("unexpected title: %s", msgs[0].Title)
	}
}
