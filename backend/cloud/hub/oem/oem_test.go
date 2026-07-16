// Package oem — unit tests for OEM multi-tenant management
package oem

import (
	"context"
	"sync"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// ── InMemory TenantManager ────────────────────────────────────────────────

type InMemoryTenantManager struct {
	mu      sync.RWMutex
	tenants map[string]*Tenant
}

func NewInMemoryTenantManager() *InMemoryTenantManager {
	return &InMemoryTenantManager{
		tenants: make(map[string]*Tenant),
	}
}

func (m *InMemoryTenantManager) CreateTenant(_ context.Context, tenant *Tenant) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.tenants[tenant.TenantID] = tenant
	return nil
}

func (m *InMemoryTenantManager) GetTenant(_ context.Context, tenantID string) (*Tenant, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	t, ok := m.tenants[tenantID]
	if !ok {
		return nil, nil
	}
	return t, nil
}

func (m *InMemoryTenantManager) UpdateTenant(_ context.Context, tenant *Tenant) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.tenants[tenant.TenantID] = tenant
	return nil
}

func (m *InMemoryTenantManager) DeleteTenant(_ context.Context, tenantID string) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	delete(m.tenants, tenantID)
	return nil
}

func (m *InMemoryTenantManager) ListTenants(_ context.Context) ([]*Tenant, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	var result []*Tenant
	for _, t := range m.tenants {
		result = append(result, t)
	}
	return result, nil
}

func (m *InMemoryTenantManager) UpdateStatus(_ context.Context, tenantID string, status TenantStatus) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	t, ok := m.tenants[tenantID]
	if !ok {
		return nil
	}
	t.Status = status
	return nil
}

// ── InMemory ConfigManager ──────────────────────────────────────────────

type InMemoryConfigManager struct {
	mu       sync.RWMutex
	configs  map[string]*TenantConfig
	branding map[string]*BrandingConfig
}

func NewInMemoryConfigManager() *InMemoryConfigManager {
	return &InMemoryConfigManager{
		configs:  make(map[string]*TenantConfig),
		branding: make(map[string]*BrandingConfig),
	}
}

func (m *InMemoryConfigManager) GetConfig(_ context.Context, tenantID string) (*TenantConfig, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	c, ok := m.configs[tenantID]
	if !ok {
		return nil, nil
	}
	return c, nil
}

func (m *InMemoryConfigManager) UpdateConfig(_ context.Context, tenantID string, config *TenantConfig) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.configs[tenantID] = config
	return nil
}

func (m *InMemoryConfigManager) GetBranding(_ context.Context, tenantID string) (*BrandingConfig, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	b, ok := m.branding[tenantID]
	if !ok {
		return nil, nil
	}
	return b, nil
}

func (m *InMemoryConfigManager) UpdateBranding(_ context.Context, tenantID string, branding *BrandingConfig) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.branding[tenantID] = branding
	return nil
}

func (m *InMemoryConfigManager) IsFeatureEnabled(_ context.Context, tenantID string, feature string) (bool, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	cfg, ok := m.configs[tenantID]
	if !ok {
		return false, nil
	}
	return cfg.Features[feature], nil
}

func (m *InMemoryConfigManager) GetSupportedProtocols(_ context.Context, tenantID string) ([]string, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	cfg, ok := m.configs[tenantID]
	if !ok {
		return nil, nil
	}
	return cfg.SupportedProtocols, nil
}

// ── InMemory ScopeResolver ──────────────────────────────────────────────

type InMemoryScopeResolver struct {
	mu          sync.RWMutex
	userScope   map[string]*OEMScope
	deviceScope map[string]*OEMScope
	vehicleScope map[string]*OEMScope
}

func NewInMemoryScopeResolver() *InMemoryScopeResolver {
	return &InMemoryScopeResolver{
		userScope:    make(map[string]*OEMScope),
		deviceScope:  make(map[string]*OEMScope),
		vehicleScope: make(map[string]*OEMScope),
	}
}

func (r *InMemoryScopeResolver) ResolveByUser(_ context.Context, userID string) (*OEMScope, error) {
	r.mu.RLock()
	defer r.mu.RUnlock()
	return r.userScope[userID], nil
}

func (r *InMemoryScopeResolver) ResolveByDevice(_ context.Context, deviceID string) (*OEMScope, error) {
	r.mu.RLock()
	defer r.mu.RUnlock()
	return r.deviceScope[deviceID], nil
}

func (r *InMemoryScopeResolver) ResolveByVehicle(_ context.Context, vehicleID string) (*OEMScope, error) {
	r.mu.RLock()
	defer r.mu.RUnlock()
	return r.vehicleScope[vehicleID], nil
}

// ── Tests ─────────────────────────────────────────────────────────────────

func TestInMemoryTenantManager(t *testing.T) {
	ctx := context.Background()
	m := NewInMemoryTenantManager()

	tenant := &Tenant{
		TenantID: "nio",
		Name:     "NIO",
		Domain:   "nio.com",
		Status:   TenantActive,
	}

	t.Run("create and get", func(t *testing.T) {
		err := m.CreateTenant(ctx, tenant)
		require.NoError(t, err)

		t2, err := m.GetTenant(ctx, "nio")
		require.NoError(t, err)
		assert.Equal(t, "NIO", t2.Name)
	})

	t.Run("update", func(t *testing.T) {
		tenant.Name = "NIO Inc."
		err := m.UpdateTenant(ctx, tenant)
		assert.NoError(t, err)

		t2, _ := m.GetTenant(ctx, "nio")
		assert.Equal(t, "NIO Inc.", t2.Name)
	})

	t.Run("list", func(t *testing.T) {
		_ = m.CreateTenant(ctx, &Tenant{TenantID: "byd", Name: "BYD"})
		list, err := m.ListTenants(ctx)
		assert.NoError(t, err)
		assert.Len(t, list, 2)
	})

	t.Run("update status", func(t *testing.T) {
		err := m.UpdateStatus(ctx, "nio", TenantInactive)
		assert.NoError(t, err)

		t2, _ := m.GetTenant(ctx, "nio")
		assert.Equal(t, TenantInactive, t2.Status)
	})

	t.Run("delete", func(t *testing.T) {
		err := m.DeleteTenant(ctx, "byd")
		assert.NoError(t, err)

		list, _ := m.ListTenants(ctx)
		assert.Len(t, list, 1)
	})

	t.Run("get non-existent returns nil", func(t *testing.T) {
		t2, err := m.GetTenant(ctx, "nonexistent")
		assert.NoError(t, err)
		assert.Nil(t, t2)
	})
}

func TestInMemoryConfigManager(t *testing.T) {
	ctx := context.Background()
	m := NewInMemoryConfigManager()

	config := &TenantConfig{
		MaxDevicesPerUser:  5,
		MaxKeysPerVehicle:  10,
		SupportedProtocols: []string{"ICCE", "CCC"},
		Features:           map[string]bool{"remote_control": true},
	}

	t.Run("update and get config", func(t *testing.T) {
		err := m.UpdateConfig(ctx, "nio", config)
		assert.NoError(t, err)

		c, err := m.GetConfig(ctx, "nio")
		require.NoError(t, err)
		assert.Equal(t, 5, c.MaxDevicesPerUser)
	})

	t.Run("get config non-existent", func(t *testing.T) {
		c, err := m.GetConfig(ctx, "nonexistent")
		assert.NoError(t, err)
		assert.Nil(t, c)
	})

	t.Run("branding", func(t *testing.T) {
		b := &BrandingConfig{BrandName: "NIO", AppName: "NIO App"}
		err := m.UpdateBranding(ctx, "nio", b)
		assert.NoError(t, err)

		b2, err := m.GetBranding(ctx, "nio")
		require.NoError(t, err)
		assert.Equal(t, "NIO App", b2.AppName)
	})

	t.Run("feature enabled", func(t *testing.T) {
		enabled, err := m.IsFeatureEnabled(ctx, "nio", "remote_control")
		assert.NoError(t, err)
		assert.True(t, enabled)
	})

	t.Run("feature disabled", func(t *testing.T) {
		enabled, err := m.IsFeatureEnabled(ctx, "nio", "uwb_ranging")
		assert.NoError(t, err)
		assert.False(t, enabled)
	})

	t.Run("supported protocols", func(t *testing.T) {
		protos, err := m.GetSupportedProtocols(ctx, "nio")
		assert.NoError(t, err)
		assert.Contains(t, protos, "ICCE")
		assert.Contains(t, protos, "CCC")
	})
}

func TestInMemoryScopeResolver(t *testing.T) {
	ctx := context.Background()
	r := NewInMemoryScopeResolver()

	// We need a way to set up scopes for resolving. Let's use the internal maps.
	scope := &OEMScope{TenantID: "nio", Region: "CN"}

	t.Run("resolve by user", func(t *testing.T) {
		r.mu.Lock()
		r.userScope["user-nio"] = scope
		r.mu.Unlock()

		s, err := r.ResolveByUser(ctx, "user-nio")
		assert.NoError(t, err)
		require.NotNil(t, s)
		assert.Equal(t, "nio", s.TenantID)
	})

	t.Run("resolve non-existent returns nil", func(t *testing.T) {
		s, err := r.ResolveByUser(ctx, "unknown")
		assert.NoError(t, err)
		assert.Nil(t, s)
	})
}

func TestTenantStatusConstants(t *testing.T) {
	assert.Equal(t, TenantStatus("active"), TenantActive)
	assert.Equal(t, TenantStatus("inactive"), TenantInactive)
	assert.Equal(t, TenantStatus("disabled"), TenantDisabled)
	assert.Equal(t, TenantStatus("trial"), TenantTrial)
}

func TestOEMScopeFields(t *testing.T) {
	scope := OEMScope{TenantID: "byd", Region: "CN", Brand: "BYD"}
	assert.Equal(t, "byd", scope.TenantID)
	assert.Equal(t, "CN", scope.Region)
	assert.Equal(t, "BYD", scope.Brand)
}
