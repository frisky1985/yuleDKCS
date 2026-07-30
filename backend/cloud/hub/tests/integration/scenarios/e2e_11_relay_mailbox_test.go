// E2E-11: CCC Mailbox API — Relay Server 完整生命周期
//
// 场景描述:
//   CCC-TS-101 §11.3.4 — Mailbox API
//
//   测试 Relay Server 的 Mailbox 完整生命周期:
//   1. CreateMailbox — 发送方创建邮箱
//   2. ReadDisplayInformation — 接收方读取展示信息
//   3. UpdateMailbox — 接收方更新邮箱（KeySigning）
//   4. ReadSecureContent — 发送方读取加密内容
//   5. UpdateMailbox — 发送方最终更新（Import）
//   6. DeleteMailbox — 完成删除

package scenarios

import (
	"context"
	"net"
	"testing"
	"time"

	"go.uber.org/zap"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/test/bufconn"

	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/relay/v1"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/relay"
)

const bufSize = 1024 * 1024

func startRelayGRPCServer(t *testing.T) (pb.RelayServiceClient, func()) {
	t.Helper()

	logger := zap.NewNop()
	lis := bufconn.Listen(bufSize)

	srv := grpc.NewServer()
	relaySvc := relay.NewRelayService(logger)
	pb.RegisterRelayServiceServer(srv, relaySvc)

	go func() {
		if err := srv.Serve(lis); err != nil {
			t.Logf("relay gRPC server exited: %v", err)
		}
	}()

	conn, err := grpc.DialContext(context.Background(), "bufnet",
		grpc.WithContextDialer(func(ctx context.Context, s string) (net.Conn, error) {
			return lis.Dial()
		}),
		grpc.WithTransportCredentials(insecure.NewCredentials()),
	)
	if err != nil {
		t.Fatalf("failed to dial relay server: %v", err)
	}

	client := pb.NewRelayServiceClient(conn)
	cleanup := func() {
		conn.Close()
		srv.GracefulStop()
	}
	return client, cleanup
}

// TestE2E11_RelayMailboxLifecycle 测试 Mailbox 完整生命周期
func TestE2E11_RelayMailboxLifecycle(t *testing.T) {
	client, cleanup := startRelayGRPCServer(t)
	defer cleanup()

	ctx := context.Background()

	// ── Step 1: CreateMailbox — 发送方创建邮箱 ──
	var mailboxID string
	var secretURL string
	t.Run("E2E-11-01: CreateMailbox", func(t *testing.T) {
		req := &pb.CreateMailboxRequest{
			Payload:          []byte(`{"encryptedKey":"AES256_BASE64_DATA"}`),
			DisplayInfo:      []byte(`{"brand":"BMW","model":"i7","year":2026}`),
			SenderDeviceId:   "iphone-15-pro-001",
			SenderVendor:     "apple",
			NotificationToken: "apns-device-token-xxx",
			Config: &pb.MailboxConfig{
				AccessRights:      pb.AccessRights_READ_WRITE_DELETE,
				ExpirationSeconds: 86400, // 24h
				MaxUpdates:        10,
			},
		}

		resp, err := client.CreateMailbox(ctx, req)
		if err != nil {
			t.Fatalf("CreateMailbox failed: %v", err)
		}
		if resp.ErrorCode != "" {
			t.Fatalf("CreateMailbox error: %s: %s", resp.ErrorCode, resp.ErrorMsg)
		}
		if resp.MailboxId == "" {
			t.Fatal("expected non-empty mailbox_id")
		}
		if resp.SharingUrl == "" {
			t.Fatal("expected non-empty sharing_url")
		}
		if resp.ExpiresAt == 0 {
			t.Fatal("expected non-zero expires_at")
		}

		mailboxID = resp.MailboxId
		secretURL = resp.SharingUrl
		t.Logf("Created mailbox: %s", mailboxID)
		t.Logf("Sharing URL: %s", secretURL)
	})

	// ── Step 2: ReadDisplayInformation — 接收方读取展示信息 ──
	t.Run("E2E-11-02: ReadDisplayInformation", func(t *testing.T) {
		req := &pb.ReadDisplayInformationRequest{
			MailboxId: mailboxID,
			DeviceId:  "galaxy-s25-001",
		}

		resp, err := client.ReadDisplayInformation(ctx, req)
		if err != nil {
			t.Fatalf("ReadDisplayInformation failed: %v", err)
		}
		if resp.ErrorCode != "" {
			t.Fatalf("ReadDisplayInformation error: %s", resp.ErrorCode)
		}
		if len(resp.DisplayInfo) == 0 {
			t.Fatal("expected non-empty display_info")
		}
		if string(resp.DisplayInfo) != `{"brand":"BMW","model":"i7","year":2026}` {
			t.Errorf("unexpected display_info: %s", string(resp.DisplayInfo))
		}
		t.Logf("Display info: %s", string(resp.DisplayInfo))
	})

	// ── Step 3: UpdateMailbox — 接收方更新（KeySigning） ──
	t.Run("E2E-11-03: UpdateMailbox (receiver - KeySigning)", func(t *testing.T) {
		req := &pb.UpdateMailboxRequest{
			MailboxId:        mailboxID,
			Payload:          []byte(`{"signedKey":"ECDSA_SIG_DATA"}`),
			SharingDataType:  2, // KeySigning
			UpdaterDeviceId:  "galaxy-s25-001",
			TraceId:          "trace-receiver-key-signing",
		}

		resp, err := client.UpdateMailbox(ctx, req)
		if err != nil {
			t.Fatalf("UpdateMailbox failed: %v", err)
		}
		if resp.ErrorCode != "" {
			t.Fatalf("UpdateMailbox error: %s", resp.ErrorCode)
		}
		if resp.Status != pb.MailboxStatus_UPDATED_BY_RECEIVER {
			t.Errorf("expected UPDATED_BY_RECEIVER, got %v", resp.Status)
		}
		t.Logf("Update status: %v, version: %d", resp.Status, resp.Version)
	})

	// ── Step 4: ReadSecureContent — 发送方读取加密内容 ──
	t.Run("E2E-11-04: ReadSecureContent", func(t *testing.T) {
		req := &pb.ReadSecureContentRequest{
			MailboxId: mailboxID,
			DeviceId:  "iphone-15-pro-001",
		}

		resp, err := client.ReadSecureContent(ctx, req)
		if err != nil {
			t.Fatalf("ReadSecureContent failed: %v", err)
		}
		if resp.ErrorCode != "" {
			t.Fatalf("ReadSecureContent error: %s", resp.ErrorCode)
		}
		if len(resp.Payload) == 0 {
			t.Fatal("expected non-empty payload")
		}
		t.Logf("Secure content: %s", string(resp.Payload))
	})

	// ── Step 5: UpdateMailbox — 发送方最终更新（Import） ──
	t.Run("E2E-11-05: UpdateMailbox (sender - Import)", func(t *testing.T) {
		req := &pb.UpdateMailboxRequest{
			MailboxId:        mailboxID,
			Payload:          []byte(`{"importResult":"SUCCESS"}`),
			SharingDataType:  3, // Import
			UpdaterDeviceId:  "iphone-15-pro-001",
			TraceId:          "trace-sender-import",
		}

		resp, err := client.UpdateMailbox(ctx, req)
		if err != nil {
			t.Fatalf("UpdateMailbox failed: %v", err)
		}
		if resp.ErrorCode != "" {
			t.Fatalf("UpdateMailbox error: %s", resp.ErrorCode)
		}
		if resp.Status != pb.MailboxStatus_UPDATED_BY_SENDER {
			t.Errorf("expected UPDATED_BY_SENDER, got %v", resp.Status)
		}
		t.Logf("Update status: %v, version: %d", resp.Status, resp.Version)
	})

	// ── Step 6: DeleteMailbox — 完成删除 ──
	t.Run("E2E-11-06: DeleteMailbox (completed)", func(t *testing.T) {
		req := &pb.DeleteMailboxRequest{
			MailboxId:       mailboxID,
			Reason:          "completed",
			DeleterDeviceId: "iphone-15-pro-001",
		}

		resp, err := client.DeleteMailbox(ctx, req)
		if err != nil {
			t.Fatalf("DeleteMailbox failed: %v", err)
		}
		if !resp.Success {
			t.Fatal("expected success")
		}
		t.Log("Mailbox deleted successfully")
	})

	// ── Step 7: 删除后读取 → 应失败 ──
	t.Run("E2E-11-07: Read after delete (should fail)", func(t *testing.T) {
		req := &pb.ReadSecureContentRequest{
			MailboxId: mailboxID,
			DeviceId:  "iphone-15-pro-001",
		}

		resp, err := client.ReadSecureContent(ctx, req)
		if err != nil {
			t.Fatalf("ReadSecureContent failed: %v", err)
		}
		if resp.ErrorCode == "" {
			t.Fatal("expected error after delete, got success")
		}
		t.Logf("Expected error: %s: %s", resp.ErrorCode, resp.ErrorMsg)
	})
}

// TestE2E12_RelayMailboxExpiry 测试 Mailbox TTL 过期清理
func TestE2E12_RelayMailboxExpiry(t *testing.T) {
	client, cleanup := startRelayGRPCServer(t)
	defer cleanup()

	ctx := context.Background()

	// 创建短 TTL 的 mailbox (1s)
	req := &pb.CreateMailboxRequest{
		Payload:         []byte(`{"test":"expiry"}`),
		SenderDeviceId:  "device-expiry-test",
		SenderVendor:    "apple",
		Config: &pb.MailboxConfig{
			AccessRights:      pb.AccessRights_READ_WRITE_DELETE,
			ExpirationSeconds: 1, // 1秒过期
		},
	}

	resp, err := client.CreateMailbox(ctx, req)
	if err != nil {
		t.Fatalf("CreateMailbox failed: %v", err)
	}
	if resp.ErrorCode != "" {
		t.Fatalf("CreateMailbox error: %s", resp.ErrorCode)
	}
	mailboxID := resp.MailboxId

	// 等待过期
	time.Sleep(1500 * time.Millisecond)

	// 过期后读取 → 应失败
	readResp, err := client.ReadSecureContent(ctx, &pb.ReadSecureContentRequest{
		MailboxId: mailboxID,
		DeviceId:  "device-expiry-test",
	})
	if err != nil {
		t.Fatalf("ReadSecureContent failed: %v", err)
	}
	if readResp.ErrorCode == "" {
		t.Fatal("expected error after expiry")
	}
	t.Logf("Expiry verified: %s: %s", readResp.ErrorCode, readResp.ErrorMsg)
}
