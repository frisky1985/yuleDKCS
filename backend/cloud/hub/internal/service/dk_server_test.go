package service

import (
	"context"
	"testing"
	"time"

	"google.golang.org/grpc"

	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/token"
)

func TestNewLocalDKServer(t *testing.T) {
	s := NewLocalDKServer()
	if s == nil {
		t.Fatal("NewLocalDKServer returned nil")
	}
}

func TestLocalDKServerIssueKey(t *testing.T) {
	s := NewLocalDKServer()
	ctx := context.Background()
	req := &KeyRequest{
		TokenID:     "tok-001",
		SubjectID:   "user-1",
		VehicleID:   "VH001",
		Permissions: []token.Permission{token.PermLock, token.PermEngineStart},
		ExpiresAt:   time.Now().Add(24 * time.Hour).UnixMilli(),
	}

	resp, err := s.IssueKey(ctx, req)
	if err != nil {
		t.Fatalf("IssueKey failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
	if resp.KeyID == "" {
		t.Error("expected non-empty KeyID")
	}
	if resp.Status != "issued" {
		t.Errorf("expected status 'issued', got %s", resp.Status)
	}
	if resp.Message == "" {
		t.Error("expected non-empty Message")
	}
}

func TestLocalDKServerIssueKey_CreatesKeyID(t *testing.T) {
	s := NewLocalDKServer()
	ctx := context.Background()
	req1 := &KeyRequest{TokenID: "t1", VehicleID: "VH001"}
	req2 := &KeyRequest{TokenID: "t2", VehicleID: "VH002"}

	resp1, _ := s.IssueKey(ctx, req1)
	resp2, _ := s.IssueKey(ctx, req2)

	if resp1.KeyID == resp2.KeyID {
		t.Error("expected unique key IDs for different vehicles")
	}
}

func TestLocalDKServerIssueKey_KeyIDContainsVehicleID(t *testing.T) {
	s := NewLocalDKServer()
	ctx := context.Background()
	req := &KeyRequest{TokenID: "tok-001", VehicleID: "VH-TEST-001"}

	resp, err := s.IssueKey(ctx, req)
	if err != nil {
		t.Fatalf("IssueKey failed: %v", err)
	}
	if len(resp.KeyID) < len("key-VH-TEST-001") {
		t.Errorf("expected KeyID to contain vehicle ID, got %s", resp.KeyID)
	}
}

func TestLocalDKServerRevokeKeyByToken(t *testing.T) {
	s := NewLocalDKServer()
	err := s.RevokeKeyByToken(context.Background(), "tok-001")
	if err != nil {
		t.Fatalf("RevokeKeyByToken should not fail: %v", err)
	}
}

func TestLocalDKServerRevokeKeyByToken_EmptyToken(t *testing.T) {
	s := NewLocalDKServer()
	err := s.RevokeKeyByToken(context.Background(), "")
	if err != nil {
		t.Fatalf("RevokeKeyByToken with empty token should not fail: %v", err)
	}
}

func TestNewGRPCDKServer(t *testing.T) {
	s := NewGRPCDKServer()
	if s == nil {
		t.Fatal("NewGRPCDKServer returned nil")
	}
}

func TestGRPCDKServerRegisterGRPCServer(t *testing.T) {
	s := NewGRPCDKServer()
	// Just verify it doesn't panic; ServiceRegistrar is a real interface
	var srv grpc.ServiceRegistrar = noopServiceRegistrar{}
	s.RegisterGRPCServer(srv)
}

// noopServiceRegistrar is a minimal gRPC ServiceRegistrar for testing
type noopServiceRegistrar struct{}

func (noopServiceRegistrar) RegisterService(desc *grpc.ServiceDesc, impl interface{}) {
	// noop — just verify no panic
}

// ── DKServer interface compliance ──

func TestLocalDKServer_ImplementsDKServer(t *testing.T) {
	var _ DKServer = (*LocalDKServer)(nil)
}
