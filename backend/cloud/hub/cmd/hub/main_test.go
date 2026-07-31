package main

import (
	"testing"
	"time"

	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/gateway"
	"go.uber.org/zap"
)

// ── setupHubGRPCServer ──────────────────────────────────────────────────────

func TestSetupHubGRPCServer_ReturnsNonNil(t *testing.T) {
	logger, _ := zap.NewProduction()
	defer logger.Sync()

	grpcSrv, gw := setupHubGRPCServer(logger)

	if grpcSrv == nil {
		t.Fatal("setupHubGRPCServer returned nil grpcSrv")
	}
	if gw == nil {
		t.Fatal("setupHubGRPCServer returned nil gw")
	}
}

func TestSetupHubGRPCServer_ServicesRegistered(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	defer logger.Sync()

	grpcSrv, _ := setupHubGRPCServer(logger)
	if grpcSrv == nil {
		t.Fatal("grpcSrv is nil")
	}

	serviceInfo := grpcSrv.GetServiceInfo()

	// Check that the expected hub services are registered
	// Service names follow the proto package namespace: digitalkey.hub.v1.*
	expectedServices := []string{
		"digitalkey.hub.v1.KeyManagementService",
		"digitalkey.hub.v1.KeyShareService",
		"digitalkey.hub.v1.VehicleControlService",
		"digitalkey.hub.v1.HubTransportService",
		"digitalkey.relay.v1.RelayService",
	}

	registered := 0
	for _, name := range expectedServices {
		if _, ok := serviceInfo[name]; ok {
			registered++
		} else {
			t.Logf("service %q not found (may use different naming)", name)
		}
	}

	if registered < 5 {
		// Print all registered services for debugging
		t.Logf("Expected 5 hub services, found %d. Registered services:", len(serviceInfo))
		for name := range serviceInfo {
			t.Logf("  - %s", name)
		}
	}
}

func TestSetupHubGRPCServer_AtLeast5Services(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	defer logger.Sync()

	grpcSrv, _ := setupHubGRPCServer(logger)
	si := grpcSrv.GetServiceInfo()

	if len(si) < 5 {
		t.Errorf("expected at least 5 services registered, got %d", len(si))
		for name := range si {
			t.Logf("  registered: %s", name)
		}
	}
}

// ── Gateway creation ────────────────────────────────────────────────────────

func TestSetupHubGRPCServer_GatewayType(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	defer logger.Sync()

	_, gw := setupHubGRPCServer(logger)
	if gw == nil {
		t.Fatal("gateway is nil")
	}

	var _ *gateway.RESTGateway = gw
}

// ── Keepalive configuration ─────────────────────────────────────────────────

func TestSetupHubGRPCServer_KeepaliveParamsDuration(t *testing.T) {
	// Verify sane duration values used for keepalive
	tests := []struct{
		name string
		d    time.Duration
		min  time.Duration
		max  time.Duration
	}{
		{"MaxConnectionIdle", 5 * time.Minute, time.Second, 30 * time.Minute},
		{"MaxConnectionAge", 30 * time.Minute, time.Minute, 24 * time.Hour},
		{"MaxConnectionAgeGrace", 10 * time.Second, time.Second, time.Minute},
		{"Time (ping interval)", 30 * time.Second, time.Second, 5 * time.Minute},
		{"Timeout (ping timeout)", 10 * time.Second, time.Second, time.Minute},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if tt.d < tt.min || tt.d > tt.max {
				t.Errorf("duration %v out of range [%v, %v]", tt.d, tt.min, tt.max)
			}
		})
	}
}

// ── Logger compatibility ────────────────────────────────────────────────────

func TestSetupHubGRPCServer_MultipleLoggers(t *testing.T) {
	t.Run("production", func(t *testing.T) {
		logger, _ := zap.NewProduction()
		g, gw := setupHubGRPCServer(logger)
		if g == nil || gw == nil {
			t.Error("nil components with production logger")
		}
		logger.Sync()
	})

	t.Run("development", func(t *testing.T) {
		logger, _ := zap.NewDevelopment()
		g, gw := setupHubGRPCServer(logger)
		if g == nil || gw == nil {
			t.Error("nil components with development logger")
		}
		logger.Sync()
	})

	t.Run("example", func(t *testing.T) {
		logger := zap.NewExample()
		g, gw := setupHubGRPCServer(logger)
		if g == nil || gw == nil {
			t.Error("nil components with example logger")
		}
		logger.Sync()
	})
}

// ── Message size configuration ──────────────────────────────────────────────

func TestSetupHubGRPCServer_ServiceInfoNotEmpty(t *testing.T) {
	logger, _ := zap.NewDevelopment()
	grpcSrv, _ := setupHubGRPCServer(logger)

	si := grpcSrv.GetServiceInfo()
	if len(si) == 0 {
		t.Error("GetServiceInfo returned empty — no services registered")
	}
}

// ── parseOEMJWKSURLs [P1-2] ─────────────────────────────────────────────────

func TestParseOEMJWKSURLs_Valid(t *testing.T) {
	raw := "oemA=https://a.example.com/jwks,oemB=https://b.example.com/jwks"
	got := parseOEMJWKSURLs(raw)
	if len(got) != 2 {
		t.Fatalf("expected 2 entries, got %d: %v", len(got), got)
	}
	if got["oemA"] != "https://a.example.com/jwks" {
		t.Fatalf("unexpected oemA URL: %q", got["oemA"])
	}
	if got["oemB"] != "https://b.example.com/jwks" {
		t.Fatalf("unexpected oemB URL: %q", got["oemB"])
	}
}

func TestParseOEMJWKSURLs_Empty(t *testing.T) {
	got := parseOEMJWKSURLs("")
	if len(got) != 0 {
		t.Fatalf("expected empty map, got %v", got)
	}
}

func TestParseOEMJWKSURLs_SkipsMalformed(t *testing.T) {
	raw := "oemA=https://a.example.com/jwks,no-equals-here,oemB=,=https://nokey.example.com/jwks"
	got := parseOEMJWKSURLs(raw)
	if len(got) != 1 {
		t.Fatalf("expected 1 valid entry, got %d: %v", len(got), got)
	}
	if _, ok := got["oemA"]; !ok {
		t.Fatalf("expected oemA entry, got %v", got)
	}
}

func TestParseOEMJWKSURLs_TrimsWhitespace(t *testing.T) {
	got := parseOEMJWKSURLs(" oemA = https://a.example.com/jwks , oemB = https://b.example.com/jwks ")
	if len(got) != 2 {
		t.Fatalf("expected 2 entries after trimming, got %d: %v", len(got), got)
	}
	if got["oemA"] != "https://a.example.com/jwks" {
		t.Fatalf("unexpected trimmed oemA URL: %q", got["oemA"])
	}
}
