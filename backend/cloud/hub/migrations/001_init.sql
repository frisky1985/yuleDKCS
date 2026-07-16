-- ─────────────────────────────────────────────────────────
-- yuleDKCS Schema v001 — 初始建表
-- 包含：tenants, key_records, devices, security_events,
--       test_runs, calibration_profiles
-- ─────────────────────────────────────────────────────────

-- 租户表
CREATE TABLE IF NOT EXISTS tenants (
    tenant_id   UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name        VARCHAR(255) NOT NULL,
    domain      VARCHAR(255) UNIQUE,
    config      JSONB DEFAULT '{}',
    created_at  TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at  TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- 密钥记录表（OMS 生命周期 tracking）
CREATE TABLE IF NOT EXISTS key_records (
    key_id       VARCHAR(64) PRIMARY KEY,
    tenant_id    UUID REFERENCES tenants(tenant_id),
    device_id    VARCHAR(128),
    owner_id     VARCHAR(128) NOT NULL,
    state        VARCHAR(32) NOT NULL DEFAULT 'created',
    metadata     JSONB DEFAULT '{}',
    created_at   TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    activated_at TIMESTAMPTZ,
    revoked_at   TIMESTAMPTZ
);

-- 设备表
CREATE TABLE IF NOT EXISTS devices (
    device_id    VARCHAR(128) PRIMARY KEY,
    tenant_id    UUID REFERENCES tenants(tenant_id),
    model        VARCHAR(128),
    os_version   VARCHAR(64),
    capabilities JSONB DEFAULT '[]',
    status       VARCHAR(32) DEFAULT 'pending',
    created_at   TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    last_seen    TIMESTAMPTZ
);

-- 安全事件表
CREATE TABLE IF NOT EXISTS security_events (
    event_id    UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    tenant_id   UUID REFERENCES tenants(tenant_id),
    event_type  VARCHAR(64) NOT NULL,
    severity    VARCHAR(16) NOT NULL,
    device_id   VARCHAR(128),
    description TEXT,
    raw_data    JSONB DEFAULT '{}',
    created_at  TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- 测试运行表
CREATE TABLE IF NOT EXISTS test_runs (
    run_id       UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    tenant_id    UUID REFERENCES tenants(tenant_id),
    device_id    VARCHAR(128),
    status       VARCHAR(32) DEFAULT 'pending',
    total_cases  INTEGER DEFAULT 0,
    passed       INTEGER DEFAULT 0,
    failed       INTEGER DEFAULT 0,
    report       JSONB DEFAULT '{}',
    created_at   TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    completed_at TIMESTAMPTZ
);

-- 校准参数表
CREATE TABLE IF NOT EXISTS calibration_profiles (
    model_id     VARCHAR(128) NOT NULL,
    calib_type   VARCHAR(32) NOT NULL,
    params       JSONB DEFAULT '{}',
    avg_accuracy FLOAT DEFAULT 0,
    sample_count INTEGER DEFAULT 0,
    updated_at   TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    PRIMARY KEY (model_id, calib_type)
);

-- ── Indexes ──
CREATE INDEX IF NOT EXISTS idx_key_records_owner  ON key_records(owner_id);
CREATE INDEX IF NOT EXISTS idx_key_records_state  ON key_records(state);
CREATE INDEX IF NOT EXISTS idx_devices_tenant     ON devices(tenant_id);
CREATE INDEX IF NOT EXISTS idx_security_events_type     ON security_events(event_type);
CREATE INDEX IF NOT EXISTS idx_security_events_severity ON security_events(severity);
CREATE INDEX IF NOT EXISTS idx_test_runs_status   ON test_runs(status);
CREATE INDEX IF NOT EXISTS idx_key_records_device ON key_records(device_id);
CREATE INDEX IF NOT EXISTS idx_security_events_device   ON security_events(device_id);
CREATE INDEX IF NOT EXISTS idx_test_runs_device   ON test_runs(device_id);
