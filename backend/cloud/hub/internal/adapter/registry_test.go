package adapter

import (
	"context"
	"testing"

	"go.uber.org/zap"
)

// ── Registry: case-insensitive matching (KNI-002/003) ──

func TestRegistryGet_CaseInsensitive(t *testing.T) {
	logger := zap.NewNop()
	r := NewRegistry(logger)

	// Register with lowercase vendor/protocol
	adapter := NewICCOAAdapter("xiaomi", logger)
	r.Register("xiaomi", "iccoa_dk40", adapter)

	tests := []struct {
		name     string
		vendor   string
		protocol string
		wantOK   bool
	}{
		{"exact same case", "xiaomi", "iccoa_dk40", true},
		{"vendor uppercase", "XIAOMI", "iccoa_dk40", true},
		{"protocol uppercase", "xiaomi", "ICCOA_DK40", true},
		{"both uppercase", "XIAOMI", "ICCOA_DK40", true},
		{"mixed case vendor", "XiaoMi", "iccoa_dk40", true},
		{"mixed case protocol", "xiaomi", "Iccoa_Dk40", true},
		{"wrong vendor", "nonexistent", "iccoa_dk40", false},
		{"wrong protocol", "xiaomi", "ccc_dk3", false},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, ok := r.Get(tt.vendor, tt.protocol)
			if ok != tt.wantOK {
				t.Errorf("Get(%q, %q) ok=%v, want %v", tt.vendor, tt.protocol, ok, tt.wantOK)
			}
			if tt.wantOK && got == nil {
				t.Error("expected non-nil adapter when ok=true")
			}
		})
	}
}

func TestRegistryGetByVendor_CaseInsensitive(t *testing.T) {
	logger := zap.NewNop()
	r := NewRegistry(logger)

	r.Register("oppo", "iccoa_dk40", NewICCOAAdapter("oppo", logger))
	r.Register("apple", "ccc_dk3", NewCCCAdapter("apple", logger))

	tests := []struct {
		name   string
		vendor string
		wantOK bool
	}{
		{"lowercase", "oppo", true},
		{"uppercase", "OPPO", true},
		{"mixed case", "OpPo", true},
		{"apple lowercase", "apple", true},
		{"apple uppercase", "APPLE", true},
		{"nonexistent", "nokia", false},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, ok := r.GetByVendor(tt.vendor)
			if ok != tt.wantOK {
				t.Errorf("GetByVendor(%q) ok=%v, want %v", tt.vendor, ok, tt.wantOK)
			}
			if tt.wantOK && got == nil {
				t.Error("expected non-nil adapter when ok=true")
			}
		})
	}
}

func TestRegistryRegister_OverwriteExisting(t *testing.T) {
	logger := zap.NewNop()
	r := NewRegistry(logger)

	r.Register("xiaomi", "iccoa_dk40", NewICCOAAdapter("xiaomi_v1", logger))
	r.Register("XIAOMI", "ICCOA_DK40", NewICCOAAdapter("xiaomi_v2", logger))

	// Should overwrite — key is normalized to lowercase
	a, ok := r.Get("xiaomi", "iccoa_dk40")
	if !ok {
		t.Fatal("expected adapter after overwrite")
	}
	if a.Vendor() != "xiaomi_v2" {
		t.Errorf("expected overwritten adapter (xiaomi_v2), got %q", a.Vendor())
	}

	// Verify only one entry in registry
	count := 0
	r.mu.RLock()
	for k := range r.adapters {
		if k == "xiaomi:iccoa_dk40" {
			count++
		}
	}
	r.mu.RUnlock()
	if count != 1 {
		t.Errorf("expected exactly 1 entry for xiaomi:iccoa_dk40, found %d", count)
	}
}

func TestRegistryGetByVendor_MultipleAdapters(t *testing.T) {
	logger := zap.NewNop()
	r := NewRegistry(logger)

	// Register with same vendor string as Vendor() returns (case preserved)
	r.Register("samsung", "ccc_dk3", NewCCCAdapter("samsung", logger))
	r.Register("OPPO", "iccoa_dk40", NewICCOAAdapter("oppo", logger))

	// GetByVendor should find the samsung adapter (case-insensitive lookup)
	a, ok := r.GetByVendor("SAMSUNG")
	if !ok {
		t.Fatal("expected to find adapter for SAMSUNG")
	}
	if a.Vendor() != "samsung" {
		t.Errorf("expected vendor 'samsung', got %q", a.Vendor())
	}

	// Also find oppo (registered with uppercase, but Vendor() returns lowercase)
	a2, ok2 := r.GetByVendor("oppo")
	if !ok2 {
		t.Fatal("expected to find adapter for oppo")
	}
	if a2.Vendor() != "oppo" {
		t.Errorf("expected vendor 'oppo', got %q", a2.Vendor())
	}
}

func TestRegistryGet_MultipleVendors(t *testing.T) {
	logger := zap.NewNop()
	r := NewRegistry(logger)

	r.Register("xiaomi", "iccoa_dk40", NewICCOAAdapter("xiaomi", logger))
	r.Register("oppo", "iccoa_dk40", NewICCOAAdapter("oppo", logger))
	r.Register("apple", "ccc_dk3", NewCCCAdapter("apple", logger))

	// Each should be independently retrievable
	for _, vendor := range []string{"XIAOMI", "OPPO", "APPLE"} {
		a, ok := r.GetByVendor(vendor)
		if !ok {
			t.Errorf("GetByVendor(%q) failed", vendor)
			continue
		}
		if a.Vendor() != vendor {
			// Vendor() returns the original case used in New*Adapter
			expected := map[string]string{"XIAOMI": "xiaomi", "OPPO": "oppo", "APPLE": "apple"}[vendor]
			if a.Vendor() != expected {
				t.Errorf("GetByVendor(%q) returned adapter with vendor %q, expected %q", vendor, a.Vendor(), expected)
			}
		}
	}
}

// ── AutoDetectProtocol case-insensitivity (KNI-002/003) ──
// Note: AutoDetectProtocol is in the unified package, so we test it via
// the registry's interaction pattern that mimics its use.

func TestRegistryIntegration_AutoDetectProtocolPattern(t *testing.T) {
	logger := zap.NewNop()
	r := NewRegistry(logger)

	// Simulate the AutoDetectProtocol pattern — vendors registered lowercase,
	// lookup with protobuf uppercase
	r.Register("xiaomi", "iccoa_dk40", NewICCOAAdapter("xiaomi", logger))

	// Simulate protobuf String() output which is UPPERCASE
	vendorFromProto := "XIAOMI"

	// GetByVendor is what AutoDetectProtocol's result feeds into
	// After our fix, AutoDetectProtocol does strings.ToLower()
	a, ok := r.GetByVendor(vendorFromProto)
	if !ok {
		t.Fatal("AutoDetectProtocol pattern: GetByVendor should find adapter with uppercase vendor")
	}
	if a.Vendor() != "xiaomi" {
		t.Errorf("expected vendor xiaomi, got %q", a.Vendor())
	}

	// Also test the new adapter.Registry Get path used by UnifiedKeyService.BindKey
	protoFromProto := "ICCOA_DK40"
	a2, ok2 := r.Get(vendorFromProto, protoFromProto)
	if !ok2 {
		t.Fatal("UnifiedKeyService pattern: Get should find adapter with uppercase vendor+protocol")
	}
	if a2.Vendor() != "xiaomi" {
		t.Errorf("expected vendor xiaomi, got %q", a2.Vendor())
	}
}

// ── Registry edge cases ──

func TestRegistryGet_EmptyRegistry(t *testing.T) {
	logger := zap.NewNop()
	r := NewRegistry(logger)

	_, ok := r.Get("any", "any")
	if ok {
		t.Error("expected not found from empty registry")
	}
}

func TestRegistryGetByVendor_EmptyRegistry(t *testing.T) {
	logger := zap.NewNop()
	r := NewRegistry(logger)

	_, ok := r.GetByVendor("any")
	if ok {
		t.Error("expected not found from empty registry")
	}
}

func TestRegistryNew_NilLogger(t *testing.T) {
	r := NewRegistry(nil)
	if r == nil {
		t.Fatal("NewRegistry with nil logger should return non-nil registry")
	}
	if r.adapters == nil {
		t.Error("expected non-nil adapters map")
	}
}

// Verify that ListStatus returns empty slice (not nil) on empty registry
func TestRegistryListStatus_EmptyDoesNotPanic(t *testing.T) {
	logger := zap.NewNop()
	r := NewRegistry(logger)

	statuses := r.ListStatus(context.Background())
	if statuses == nil {
		t.Error("expected non-nil slice, got nil")
	}
	if len(statuses) != 0 {
		t.Errorf("expected 0 statuses, got %d", len(statuses))
	}
}
