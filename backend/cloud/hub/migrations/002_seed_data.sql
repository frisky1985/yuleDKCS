-- ─────────────────────────────────────────────────────────
-- yuleDKCS Seed Data v002
-- 插入初始租户、默认校准配置
-- ─────────────────────────────────────────────────────────

-- 默认 OEM 租户
INSERT INTO tenants (tenant_id, name, domain) VALUES
    ('00000000-0000-0000-0000-000000000001', 'Default OEM', 'default.oem')
ON CONFLICT (domain) DO NOTHING;

-- 预置手机型号校准参数 (参考 tune/profile.go)
INSERT INTO calibration_profiles (model_id, calib_type, params, avg_accuracy, sample_count) VALUES
    ('iPhone_15_Pro_Max', 'uwb_ranging',
     '{"tx_power": -20, "rx_sensitivity": -85, "antenna_delay": 0.5}',
     0.95, 1000),
    ('iPhone_15_Pro', 'uwb_ranging',
     '{"tx_power": -20, "rx_sensitivity": -84, "antenna_delay": 0.5}',
     0.93, 1000),
    ('Xiaomi_14_Ultra', 'uwb_ranging',
     '{"tx_power": -18, "rx_sensitivity": -82, "antenna_delay": 0.6}',
     0.91, 800),
    ('Samsung_Galaxy_S24_Ultra', 'uwb_ranging',
     '{"tx_power": -19, "rx_sensitivity": -83, "antenna_delay": 0.55}',
     0.92, 600),
    ('iPhone_15_Pro_Max', 'ble_rssi',
     '{"tx_power_1m": -59, "path_loss_exponent": 2.5, "reference_distance": 1.0}',
     0.88, 2000),
    ('Xiaomi_14_Ultra', 'ble_rssi',
     '{"tx_power_1m": -65, "path_loss_exponent": 2.7, "reference_distance": 1.0}',
     0.85, 1500)
ON CONFLICT (model_id, calib_type) DO NOTHING;

-- 标记种子数据已就绪
INSERT INTO schema_migrations (version) VALUES ('002_seed_data')
ON CONFLICT (version) DO NOTHING;
