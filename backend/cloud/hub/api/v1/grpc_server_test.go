package v1

import (
	"testing"

	"google.golang.org/grpc"
)

// ─────────────────────────────────────────────────────────────
// gRPC Server descriptor tests — exercises generated code
// for Register* functions and service descriptors.
// ─────────────────────────────────────────────────────────────

func TestRegisterKeyManagementServiceServer(t *testing.T) {
	srv := grpc.NewServer()
	svc := &unimplementedKeyManagementServiceServer{}
	RegisterKeyManagementServiceServer(srv, svc)
	// Service should be registered
	info := srv.GetServiceInfo()
	if _, ok := info["digitalkey.hub.v1.KeyManagementService"]; !ok {
		t.Errorf("KeyManagementService should be registered")
	}
	_ = svc
}

func TestRegisterKeyShareServiceServer(t *testing.T) {
	srv := grpc.NewServer()
	svc := &unimplementedKeyShareServiceServer{}
	RegisterKeyShareServiceServer(srv, svc)
	info := srv.GetServiceInfo()
	if _, ok := info["digitalkey.hub.v1.KeyShareService"]; !ok {
		t.Errorf("KeyShareService should be registered")
	}
}

func TestRegisterVehicleControlServiceServer(t *testing.T) {
	srv := grpc.NewServer()
	svc := &unimplementedVehicleControlServiceServer{}
	RegisterVehicleControlServiceServer(srv, svc)
	info := srv.GetServiceInfo()
	if _, ok := info["digitalkey.hub.v1.VehicleControlService"]; !ok {
		t.Errorf("VehicleControlService should be registered")
	}
}

func TestRegisterHubTransportServiceServer(t *testing.T) {
	srv := grpc.NewServer()
	svc := &unimplementedHubTransportServiceServer{}
	RegisterHubTransportServiceServer(srv, svc)
	info := srv.GetServiceInfo()
	if _, ok := info["digitalkey.hub.v1.HubTransportService"]; !ok {
		t.Errorf("HubTransportService should be registered")
	}
}

// unimplemented service stubs for registration testing
type unimplementedKeyManagementServiceServer struct {
	UnimplementedKeyManagementServiceServer
}
type unimplementedKeyShareServiceServer struct {
	UnimplementedKeyShareServiceServer
}
type unimplementedVehicleControlServiceServer struct {
	UnimplementedVehicleControlServiceServer
}
type unimplementedHubTransportServiceServer struct {
	UnimplementedHubTransportServiceServer
}

// ─────────────────────────────────────────────────────────────
// Additional message Reset/String tests for coverage
// ─────────────────────────────────────────────────────────────

func TestEnumDescriptor(t *testing.T) {
	_ = Protocol(0).Descriptor()
	_ = PhoneVendor(0).Descriptor()
	_ = KeyType(0).Descriptor()
	_ = KeyStatus(0).Descriptor()
}

func TestEnumType(t *testing.T) {
	// Type() method returns enum descriptor type
	_ = Protocol(0).Type()
	_ = PhoneVendor(0).Type()
}
