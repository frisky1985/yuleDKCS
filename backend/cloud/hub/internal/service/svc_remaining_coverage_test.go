package service

import (
	"context"
	"testing"

	"go.uber.org/zap"
	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/adapter"
)

// ─────────────────────────────────────────────────────────────
// KeyShareService tests
// ─────────────────────────────────────────────────────────────

func TestNewKeyShareService(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	defer logger.Sync()

	reg := adapter.NewRegistry(logger)
	s := NewKeyShareService(reg, logger)
	if s == nil {
		t.Fatal("NewKeyShareService returned nil")
	}
}

func TestKeyShareService_CancelShare(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	defer logger.Sync()

	reg := adapter.NewRegistry(logger)
	s := NewKeyShareService(reg, logger)
	ctx := context.Background()

	resp, err := s.CancelShare(ctx, &pb.CancelShareRequest{
		ShareId: "share-001",
	})
	if err != nil {
		t.Fatalf("CancelShare: %v", err)
	}
	if resp.ErrorCode != "" {
		t.Errorf("ErrorCode should be empty on success")
	}
}

func TestKeyShareService_GetShare(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	defer logger.Sync()

	s := NewKeyShareService(adapter.NewRegistry(logger), logger)
	ctx := context.Background()

	resp, err := s.GetShare(ctx, &pb.GetShareRequest{
		ShareId: "share-002",
	})
	if err != nil {
		t.Fatalf("GetShare: %v", err)
	}
	_ = resp
}

func TestKeyShareService_CreateShare_NoAdapter(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	defer logger.Sync()

	reg := adapter.NewRegistry(logger)
	s := NewKeyShareService(reg, logger)
	ctx := context.Background()

	resp, err := s.CreateShare(ctx, &pb.CreateShareRequest{
		KeyId:      "k-001",
		FromUserId: "u-001",
		ToVendor:   pb.PhoneVendor_OPPO,
	})
	if err != nil {
		t.Fatalf("CreateShare: %v", err)
	}
	if resp.ErrorCode != "ADAPTER_NOT_FOUND" {
		t.Errorf("Expected ADAPTER_NOT_FOUND, got %s", resp.ErrorCode)
	}
}

func TestKeyShareService_AcceptShare_NoAdapter(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	defer logger.Sync()

	s := NewKeyShareService(adapter.NewRegistry(logger), logger)
	ctx := context.Background()

	resp, err := s.AcceptShare(ctx, &pb.AcceptShareRequest{
		ShareCode: "123456",
		Vendor:    pb.PhoneVendor_APPLE,
	})
	if err != nil {
		t.Fatalf("AcceptShare: %v", err)
	}
	if resp.ErrorCode != "ADAPTER_NOT_FOUND" {
		t.Errorf("Expected ADAPTER_NOT_FOUND, got %s", resp.ErrorCode)
	}
}

// ─────────────────────────────────────────────────────────────
// HubTransportService tests
// ─────────────────────────────────────────────────────────────

func TestNewHubTransportService(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	defer logger.Sync()

	reg := adapter.NewRegistry(logger)
	s := NewHubTransportService(reg, logger)
	if s == nil {
		t.Fatal("NewHubTransportService returned nil")
	}
}

func TestHubTransportService_ForwardNoAdapter(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	defer logger.Sync()

	reg := adapter.NewRegistry(logger)
	s := NewHubTransportService(reg, logger)
	ctx := context.Background()

	resp, err := s.ForwardToVendor(ctx, &pb.ForwardRequest{
		Vendor:   pb.PhoneVendor_APPLE,
		Protocol: pb.Protocol_CCC_DK3,
		Operation: "bind",
	})
	if err != nil {
		t.Fatalf("ForwardToVendor: %v", err)
	}
	if resp.ErrorCode != "ADAPTER_NOT_FOUND" {
		t.Errorf("Expected ADAPTER_NOT_FOUND, got %s", resp.ErrorCode)
	}
}

func TestHubTransportService_ForwardNoOperation(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	defer logger.Sync()

	s := NewHubTransportService(adapter.NewRegistry(logger), logger)
	ctx := context.Background()

	resp, err := s.ForwardToVendor(ctx, &pb.ForwardRequest{
		Vendor:   pb.PhoneVendor_APPLE,
		Protocol: pb.Protocol_CCC_DK3,
		Operation: "bind",
	})
	if err != nil {
		t.Fatalf("ForwardToVendor: %v", err)
	}
	_ = resp
}

func TestHubTransportService_VendorCallback(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	defer logger.Sync()

	reg := adapter.NewRegistry(logger)
	s := NewHubTransportService(reg, logger)
	ctx := context.Background()

	resp, err := s.VendorCallback(ctx, &pb.CallbackRequest{
		Vendor:     pb.PhoneVendor_APPLE,
		Protocol:   pb.Protocol_CCC_DK3,
		Operation:  "notify",
		CallbackId: "cb-001",
	})
	if err != nil {
		t.Fatalf("VendorCallback: %v", err)
	}
	_ = resp
}

func TestHubTransportService_HealthCheck(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	defer logger.Sync()

	reg := adapter.NewRegistry(logger)
	s := NewHubTransportService(reg, logger)
	ctx := context.Background()

	resp, err := s.HealthCheck(ctx, &pb.HealthCheckRequest{})
	if err != nil {
		t.Fatalf("HealthCheck: %v", err)
	}
	// HealthCheck succeeds with zero-value (no error, Healthy defaults)
	_ = resp
}

// ─────────────────────────────────────────────────────────────
// UnifiedKeyService basic construction
// ─────────────────────────────────────────────────────────────

func TestNewUnifiedKeyService(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	defer logger.Sync()

	reg := adapter.NewRegistry(logger)
	s := NewUnifiedKeyService(reg, logger)
	if s == nil {
		t.Fatal("NewUnifiedKeyService returned nil")
	}
}
