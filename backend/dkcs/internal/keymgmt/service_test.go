package keymgmt

import (
	"context"
	"errors"
	"testing"

	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
	"github.com/google/uuid"
	"go.uber.org/zap"
	"google.golang.org/grpc"
)

// ---------------------------------------------------------------------------
// Mock HubTransportServiceClient
// ---------------------------------------------------------------------------

type mockHubClient struct {
	forwardToVendorFunc func(ctx context.Context, req *pb.ForwardRequest, opts ...grpc.CallOption) (*pb.ForwardResponse, error)
	vendorCallbackFunc  func(ctx context.Context, req *pb.CallbackRequest, opts ...grpc.CallOption) (*pb.CallbackResponse, error)
	healthCheckFunc     func(ctx context.Context, req *pb.HealthCheckRequest, opts ...grpc.CallOption) (*pb.HealthCheckResponse, error)
}

func (m *mockHubClient) ForwardToVendor(ctx context.Context, req *pb.ForwardRequest, opts ...grpc.CallOption) (*pb.ForwardResponse, error) {
	if m.forwardToVendorFunc != nil {
		return m.forwardToVendorFunc(ctx, req, opts...)
	}
	return &pb.ForwardResponse{}, nil
}

func (m *mockHubClient) VendorCallback(ctx context.Context, req *pb.CallbackRequest, opts ...grpc.CallOption) (*pb.CallbackResponse, error) {
	if m.vendorCallbackFunc != nil {
		return m.vendorCallbackFunc(ctx, req, opts...)
	}
	return &pb.CallbackResponse{}, nil
}

func (m *mockHubClient) HealthCheck(ctx context.Context, req *pb.HealthCheckRequest, opts ...grpc.CallOption) (*pb.HealthCheckResponse, error) {
	if m.healthCheckFunc != nil {
		return m.healthCheckFunc(ctx, req, opts...)
	}
	return &pb.HealthCheckResponse{}, nil
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

func newTestService(hubClient pb.HubTransportServiceClient) *Service {
	logger, _ := zap.NewDevelopment()
	return NewService(hubClient, logger)
}

func testTraceID() string {
	return uuid.New().String()
}

// ---------------------------------------------------------------------------
// BindKey Tests
// ---------------------------------------------------------------------------

func TestKeyMgmt_BindKey_Success(t *testing.T) {
	hubClient := &mockHubClient{
		forwardToVendorFunc: func(_ context.Context, req *pb.ForwardRequest, _ ...grpc.CallOption) (*pb.ForwardResponse, error) {
			if req.Operation != "bind" {
				t.Errorf("Operation: want 'bind', got %q", req.Operation)
			}
			if req.Vendor != pb.PhoneVendor_APPLE {
				t.Errorf("Vendor: want APPLE, got %v", req.Vendor)
			}
			if req.Protocol != pb.Protocol_CCC_DK3 {
				t.Errorf("Protocol: want CCC_DK3, got %v", req.Protocol)
			}
			return &pb.ForwardResponse{}, nil
		},
	}

	svc := newTestService(hubClient)
	ctx := context.Background()

	resp, err := svc.BindKey(ctx, &pb.BindKeyRequest{
		VehicleId:  "vehicle-001",
		DeviceId:   "device-001",
		UserId:     "user-001",
		Vendor:     pb.PhoneVendor_APPLE,
		Protocol:   pb.Protocol_CCC_DK3,
		KeyType:    pb.KeyType_OWNER,
		TraceId:    testTraceID(),
	})

	if err != nil {
		t.Fatalf("BindKey failed: %v", err)
	}
	if resp.ErrorCode != "" {
		t.Errorf("unexpected error code: %s / %s", resp.ErrorCode, resp.ErrorMsg)
	}
	if resp.Key == nil {
		t.Fatal("BindKey response should include a Key")
	}
	if resp.Key.Status != pb.KeyStatus_ACTIVE {
		t.Errorf("Key status: want ACTIVE, got %v", resp.Key.Status)
	}
	if resp.Key.KeyId == "" {
		t.Error("KeyId should not be empty")
	}
}

func TestKeyMgmt_BindKey_HUBError(t *testing.T) {
	hubClient := &mockHubClient{
		forwardToVendorFunc: func(_ context.Context, req *pb.ForwardRequest, _ ...grpc.CallOption) (*pb.ForwardResponse, error) {
			return nil, errors.New("hub forward failed") // triggers HUB_ERROR path in BindKey
		},
	}

	svc := newTestService(hubClient)
	resp, err := svc.BindKey(context.Background(), &pb.BindKeyRequest{
		VehicleId: "vehicle-001",
		DeviceId:  "device-001",
		UserId:    "user-001",
		Vendor:    pb.PhoneVendor_APPLE,
		Protocol:  pb.Protocol_CCC_DK3,
		TraceId:   testTraceID(),
	})
	if err != nil {
		t.Fatalf("BindKey should not return gRPC error on HUB failure: %v", err)
	}
	if resp.ErrorCode != "HUB_ERROR" {
		t.Errorf("ErrorCode: want 'HUB_ERROR', got %q", resp.ErrorCode)
	}
}

func TestKeyMgmt_BindKey_MultipleVendors(t *testing.T) {
	vendors := []pb.PhoneVendor{
		pb.PhoneVendor_APPLE,
		pb.PhoneVendor_SAMSUNG,
		pb.PhoneVendor_XIAOMI,
		pb.PhoneVendor_OPPO,
		pb.PhoneVendor_VIVO,
		pb.PhoneVendor_HUAWEI,
	}

	for _, vendor := range vendors {
		t.Run(vendor.String(), func(t *testing.T) {
			hubClient := &mockHubClient{
				forwardToVendorFunc: func(_ context.Context, req *pb.ForwardRequest, _ ...grpc.CallOption) (*pb.ForwardResponse, error) {
					if req.Vendor != vendor {
						t.Errorf("Vendor: want %v, got %v", vendor, req.Vendor)
					}
					return &pb.ForwardResponse{}, nil
				},
			}
			svc := newTestService(hubClient)
			resp, err := svc.BindKey(context.Background(), &pb.BindKeyRequest{
				VehicleId: "vehicle-001",
				Vendor:    vendor,
				Protocol:  pb.Protocol_CCC_DK3,
				TraceId:   testTraceID(),
			})
			if err != nil {
				t.Fatalf("BindKey failed for %s: %v", vendor, err)
			}
			if resp.ErrorCode != "" {
				t.Errorf("unexpected error for %s: %s", vendor, resp.ErrorCode)
			}
		})
	}
}

func TestKeyMgmt_BindKey_MultipleProtocols(t *testing.T) {
	protocols := []pb.Protocol{
		pb.Protocol_CCC_DK3,
		pb.Protocol_ICCOA_DK30,
		pb.Protocol_ICCOA_DK40,
		pb.Protocol_ICCE,
	}

	for _, proto := range protocols {
		t.Run(proto.String(), func(t *testing.T) {
			hubClient := &mockHubClient{
				forwardToVendorFunc: func(_ context.Context, req *pb.ForwardRequest, _ ...grpc.CallOption) (*pb.ForwardResponse, error) {
					if req.Protocol != proto {
						t.Errorf("Protocol: want %v, got %v", proto, req.Protocol)
					}
					return &pb.ForwardResponse{}, nil
				},
			}
			svc := newTestService(hubClient)
			resp, err := svc.BindKey(context.Background(), &pb.BindKeyRequest{
				VehicleId: "vehicle-001",
				Vendor:    pb.PhoneVendor_APPLE,
				Protocol:  proto,
				TraceId:   testTraceID(),
			})
			if err != nil {
				t.Fatalf("BindKey failed for %s: %v", proto, err)
			}
			if resp.ErrorCode != "" {
				t.Errorf("unexpected error for %s: %s", proto, resp.ErrorCode)
			}
		})
	}
}

// ---------------------------------------------------------------------------
// UnbindKey Tests
// ---------------------------------------------------------------------------

func TestKeyMgmt_UnbindKey(t *testing.T) {
	svc := newTestService(&mockHubClient{})
	resp, err := svc.UnbindKey(context.Background(), &pb.UnbindKeyRequest{
		KeyId:   "key-001",
		TraceId: testTraceID(),
	})
	if err != nil {
		t.Fatalf("UnbindKey failed: %v", err)
	}
	_ = resp // stubbed — just verify no panic
}

// ---------------------------------------------------------------------------
// SuspendKey Tests
// ---------------------------------------------------------------------------

func TestKeyMgmt_SuspendKey(t *testing.T) {
	svc := newTestService(&mockHubClient{})
	resp, err := svc.SuspendKey(context.Background(), &pb.SuspendKeyRequest{
		KeyId:   "key-001",
		Reason:  "user reported lost phone",
		TraceId: testTraceID(),
	})
	if err != nil {
		t.Fatalf("SuspendKey failed: %v", err)
	}
	_ = resp
}

func TestKeyMgmt_SuspendKey_EmptyReason(t *testing.T) {
	svc := newTestService(&mockHubClient{})
	resp, err := svc.SuspendKey(context.Background(), &pb.SuspendKeyRequest{
		KeyId:   "key-002",
		TraceId: testTraceID(),
	})
	if err != nil {
		t.Fatalf("SuspendKey with empty reason failed: %v", err)
	}
	_ = resp
}

// ---------------------------------------------------------------------------
// ResumeKey Tests
// ---------------------------------------------------------------------------

func TestKeyMgmt_ResumeKey(t *testing.T) {
	svc := newTestService(&mockHubClient{})
	resp, err := svc.ResumeKey(context.Background(), &pb.ResumeKeyRequest{
		KeyId:   "key-001",
		TraceId: testTraceID(),
	})
	if err != nil {
		t.Fatalf("ResumeKey failed: %v", err)
	}
	_ = resp
}

// ---------------------------------------------------------------------------
// RevokeKey Tests
// ---------------------------------------------------------------------------

func TestKeyMgmt_RevokeKey(t *testing.T) {
	svc := newTestService(&mockHubClient{})
	resp, err := svc.RevokeKey(context.Background(), &pb.RevokeKeyRequest{
		KeyId:   "key-001",
		Reason:  "stolen vehicle",
		TraceId: testTraceID(),
	})
	if err != nil {
		t.Fatalf("RevokeKey failed: %v", err)
	}
	_ = resp
}

func TestKeyMgmt_RevokeKey_EmptyReason(t *testing.T) {
	svc := newTestService(&mockHubClient{})
	resp, err := svc.RevokeKey(context.Background(), &pb.RevokeKeyRequest{
		KeyId:   "key-002",
		TraceId: testTraceID(),
	})
	if err != nil {
		t.Fatalf("RevokeKey with empty reason failed: %v", err)
	}
	_ = resp
}

// ---------------------------------------------------------------------------
// RenewKey Tests
// ---------------------------------------------------------------------------

func TestKeyMgmt_RenewKey(t *testing.T) {
	svc := newTestService(&mockHubClient{})
	resp, err := svc.RenewKey(context.Background(), &pb.RenewKeyRequest{
		KeyId:      "key-001",
		ValidUntil: 1767225600, // some future timestamp
		TraceId:    testTraceID(),
	})
	if err != nil {
		t.Fatalf("RenewKey failed: %v", err)
	}
	_ = resp
}

// ---------------------------------------------------------------------------
// GetKey Tests
// ---------------------------------------------------------------------------

func TestKeyMgmt_GetKey(t *testing.T) {
	svc := newTestService(&mockHubClient{})
	resp, err := svc.GetKey(context.Background(), &pb.GetKeyRequest{
		KeyId: "key-001",
	})
	if err != nil {
		t.Fatalf("GetKey failed: %v", err)
	}
	_ = resp
}

// ---------------------------------------------------------------------------
// ListKeys Tests
// ---------------------------------------------------------------------------

func TestKeyMgmt_ListKeys_ByUser(t *testing.T) {
	svc := newTestService(&mockHubClient{})
	resp, err := svc.ListKeys(context.Background(), &pb.ListKeysRequest{
		UserId:   "user-001",
		PageSize: 20,
	})
	if err != nil {
		t.Fatalf("ListKeys failed: %v", err)
	}
	_ = resp
}

func TestKeyMgmt_ListKeys_ByVehicle(t *testing.T) {
	svc := newTestService(&mockHubClient{})
	resp, err := svc.ListKeys(context.Background(), &pb.ListKeysRequest{
		VehicleId: "vehicle-001",
		PageSize:  20,
	})
	if err != nil {
		t.Fatalf("ListKeys failed: %v", err)
	}
	_ = resp
}

// ---------------------------------------------------------------------------
// Hub Forward Verification Tests
// ---------------------------------------------------------------------------

func TestKeyMgmt_BindKey_ForwardRequestFields(t *testing.T) {
	var capturedReq *pb.ForwardRequest
	hubClient := &mockHubClient{
		forwardToVendorFunc: func(_ context.Context, req *pb.ForwardRequest, _ ...grpc.CallOption) (*pb.ForwardResponse, error) {
			capturedReq = req
			return &pb.ForwardResponse{}, nil
		},
	}

	svc := newTestService(hubClient)
	traceID := testTraceID()
	_, err := svc.BindKey(context.Background(), &pb.BindKeyRequest{
		VehicleId: "vehicle-001",
		DeviceId:  "device-001",
		UserId:    "user-001",
		Vendor:    pb.PhoneVendor_XIAOMI,
		Protocol:  pb.Protocol_ICCOA_DK40,
		KeyType:   pb.KeyType_FRIEND,
		TraceId:   traceID,
	})
	if err != nil {
		t.Fatalf("BindKey failed: %v", err)
	}

	if capturedReq == nil {
		t.Fatal("ForwardToVendor was never called")
	}
	if capturedReq.Operation != "bind" {
		t.Errorf("Operation: want 'bind', got %q", capturedReq.Operation)
	}
	if capturedReq.Vendor != pb.PhoneVendor_XIAOMI {
		t.Errorf("Vendor: want XIAOMI, got %v", capturedReq.Vendor)
	}
	if capturedReq.Protocol != pb.Protocol_ICCOA_DK40 {
		t.Errorf("Protocol: want ICCOA_DK40, got %v", capturedReq.Protocol)
	}
	if capturedReq.TraceId != traceID {
		t.Errorf("TraceId: want %s, got %s", traceID, capturedReq.TraceId)
	}
}
