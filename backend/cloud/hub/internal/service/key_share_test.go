package service

import (
	"context"
	"testing"

	"go.uber.org/zap"

	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/adapter"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/relay"
)

func TestNewKeyShareService(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	mbc := relay.NewMailboxController(logger)
	s := NewKeyShareService(reg, mbc, logger)
	if s == nil {
		t.Fatal("NewKeyShareService returned nil")
	}
}

func TestKeyShareService_CreateShare_NoAdapter(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	mbc := relay.NewMailboxController(logger)
	s := NewKeyShareService(reg, mbc, logger)

	req := &pb.CreateShareRequest{
		KeyId:      "key-001",
		FromUserId: "user-1",
		ToVendor:   pb.PhoneVendor_VENDOR_UNSPECIFIED,
	}
	resp, err := s.CreateShare(context.Background(), req)
	if err != nil {
		t.Fatalf("CreateShare failed: %v", err)
	}
	if resp.ErrorCode != "ADAPTER_NOT_FOUND" {
		t.Errorf("expected ADAPTER_NOT_FOUND, got %s", resp.ErrorCode)
	}
}

func TestKeyShareService_CreateShare_Success(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	reg.Register("XIAOMI", "iccoa_dk40", adapter.NewICCOAAdapter("XIAOMI", logger))
	mbc := relay.NewMailboxController(logger)
	s := NewKeyShareService(reg, mbc, logger)

	req := &pb.CreateShareRequest{
		KeyId:      "key-001",
		FromUserId: "user-1",
		ToVendor:   pb.PhoneVendor_XIAOMI,
	}
	resp, err := s.CreateShare(context.Background(), req)
	if err != nil {
		t.Fatalf("CreateShare failed: %v", err)
	}
	if resp.ShareId == "" {
		t.Error("expected non-empty ShareId")
	}
}

func TestKeyShareService_AcceptShare_NoAdapter(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	mbc := relay.NewMailboxController(logger)
	s := NewKeyShareService(reg, mbc, logger)

	resp, err := s.AcceptShare(context.Background(), &pb.AcceptShareRequest{
		Vendor: pb.PhoneVendor_VENDOR_UNSPECIFIED,
	})
	if err != nil {
		t.Fatalf("AcceptShare failed: %v", err)
	}
	if resp.ErrorCode != "ADAPTER_NOT_FOUND" {
		t.Errorf("expected ADAPTER_NOT_FOUND, got %s", resp.ErrorCode)
	}
}

func TestKeyShareService_AcceptShare_Success(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	reg.Register("XIAOMI", "iccoa_dk40", adapter.NewICCOAAdapter("XIAOMI", logger))
	mbc := relay.NewMailboxController(logger)
	s := NewKeyShareService(reg, mbc, logger)

	resp, err := s.AcceptShare(context.Background(), &pb.AcceptShareRequest{
		Vendor: pb.PhoneVendor_XIAOMI,
	})
	if err != nil {
		t.Fatalf("AcceptShare failed: %v", err)
	}
	if resp.ErrorCode != "" {
		t.Errorf("expected no error, got %s", resp.ErrorCode)
	}
}

func TestKeyShareService_CancelShare(t *testing.T) {
	logger := zap.NewNop()
	mbc := relay.NewMailboxController(logger)
	s := NewKeyShareService(adapter.NewRegistry(logger), mbc, logger)
	resp, err := s.CancelShare(context.Background(), &pb.CancelShareRequest{ShareId: "share-001"})
	if err != nil {
		t.Fatalf("CancelShare failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}

func TestKeyShareService_GetShare(t *testing.T) {
	logger := zap.NewNop()
	mbc := relay.NewMailboxController(logger)
	s := NewKeyShareService(adapter.NewRegistry(logger), mbc, logger)
	resp, err := s.GetShare(context.Background(), &pb.GetShareRequest{ShareId: "share-001"})
	if err != nil {
		t.Fatalf("GetShare failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}
