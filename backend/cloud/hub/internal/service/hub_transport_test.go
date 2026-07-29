package service

import (
	"context"
	"testing"

	"go.uber.org/zap"

	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/adapter"
)

func TestNewHubTransportService(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewHubTransportService(reg, logger)
	if s == nil {
		t.Fatal("NewHubTransportService returned nil")
	}
}

func TestHubTransportService_ForwardToVendor_NoAdapter(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewHubTransportService(reg, logger)

	resp, err := s.ForwardToVendor(context.Background(), &pb.ForwardRequest{
		Vendor:    pb.PhoneVendor_VENDOR_UNSPECIFIED,
		Protocol:  pb.Protocol_PROTOCOL_UNSPECIFIED,
		Operation: "bind",
	})
	if err != nil {
		t.Fatalf("ForwardToVendor failed: %v", err)
	}
	if resp.ErrorCode != "ADAPTER_NOT_FOUND" {
		t.Errorf("expected ADAPTER_NOT_FOUND, got %s", resp.ErrorCode)
	}
}

func TestHubTransportService_ForwardToVendor_UnsupportedOperation(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	reg.Register("XIAOMI", "ICCOA_DK40", adapter.NewICCOAAdapter("XIAOMI", logger))
	s := NewHubTransportService(reg, logger)

	resp, err := s.ForwardToVendor(context.Background(), &pb.ForwardRequest{
		Vendor:    pb.PhoneVendor_XIAOMI,
		Protocol:  pb.Protocol_ICCOA_DK40,
		Operation: "unsupported",
	})
	if err != nil {
		t.Fatalf("ForwardToVendor failed: %v", err)
	}
	if resp.ErrorCode != "UNSUPPORTED_OPERATION" {
		t.Errorf("expected UNSUPPORTED_OPERATION, got %s", resp.ErrorCode)
	}
}

func TestHubTransportService_ForwardToVendor_Success(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	reg.Register("XIAOMI", "ICCOA_DK40", adapter.NewICCOAAdapter("XIAOMI", logger))
	s := NewHubTransportService(reg, logger)

	operations := []string{"bind", "unbind", "share", "notify"}
	for _, op := range operations {
		resp, err := s.ForwardToVendor(context.Background(), &pb.ForwardRequest{
			Vendor:    pb.PhoneVendor_XIAOMI,
			Protocol:  pb.Protocol_ICCOA_DK40,
			Operation: op,
		})
		if err != nil {
			t.Fatalf("ForwardToVendor(%s) failed: %v", op, err)
		}
		if resp == nil {
			t.Fatalf("ForwardToVendor(%s) returned nil", op)
		}
	}
}

func TestHubTransportService_VendorCallback(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewHubTransportService(reg, logger)

	resp, err := s.VendorCallback(context.Background(), &pb.CallbackRequest{
		Vendor:    pb.PhoneVendor_XIAOMI,
		Operation: "status_report",
	})
	if err != nil {
		t.Fatalf("VendorCallback failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
}

func TestHubTransportService_HealthCheck_AllHealthy(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	reg.Register("XIAOMI", "ICCOA_DK40", adapter.NewICCOAAdapter("XIAOMI", logger))
	reg.Register("apple", "ccc_dk3", adapter.NewCCCAdapter("apple", logger))
	s := NewHubTransportService(reg, logger)

	resp, err := s.HealthCheck(context.Background(), &pb.HealthCheckRequest{})
	if err != nil {
		t.Fatalf("HealthCheck failed: %v", err)
	}
	if !resp.Healthy {
		t.Error("expected healthy")
	}
	if len(resp.Adapters) != 2 {
		t.Errorf("expected 2 adapters, got %d", len(resp.Adapters))
	}
}

func TestHubTransportService_HealthCheck_Empty(t *testing.T) {
	logger := zap.NewNop()
	reg := adapter.NewRegistry(logger)
	s := NewHubTransportService(reg, logger)

	resp, err := s.HealthCheck(context.Background(), &pb.HealthCheckRequest{})
	if err != nil {
		t.Fatalf("HealthCheck failed: %v", err)
	}
	if !resp.Healthy {
		t.Error("expected healthy when no adapters")
	}
	if len(resp.Adapters) != 0 {
		t.Errorf("expected 0 adapters, got %d", len(resp.Adapters))
	}
}
