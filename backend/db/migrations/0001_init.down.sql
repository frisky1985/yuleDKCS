-- ============================================================
-- dkcs PostgreSQL 初始迁移回滚 (0001_init.down)
-- ============================================================

DROP TRIGGER IF EXISTS trigger_vehicles_updated_at ON vehicles;
DROP FUNCTION IF EXISTS update_updated_at();

DROP TABLE IF EXISTS events;
DROP TABLE IF EXISTS digital_keys;
DROP TABLE IF EXISTS vehicles;
