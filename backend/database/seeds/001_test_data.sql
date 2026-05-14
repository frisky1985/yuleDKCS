-- 测试数据种子
-- yuleDKCS 本地开发测试数据

-- 清理现有数据
TRUNCATE TABLE key_usage_logs, key_shares, keys, vehicles, users CASCADE;

-- 添加测试用户
-- 密码: password123（bcrypt加密）
INSERT INTO users (id, username, email, phone, password, role, created_at) VALUES
(1, 'testuser', 'test@example.com', '13800138000', '$2a$10$N9qo8uLOickgx2ZMRZoMyeIjZAgcfl7p92ldGxad68LJZdL17lhWy', 'user', NOW()),
(2, 'admin', 'admin@example.com', '13900139000', '$2a$10$N9qo8uLOickgx2ZMRZoMyeIjZAgcfl7p92ldGxad68LJZdL17lhWy', 'admin', NOW()),
(3, 'friend1', 'friend1@example.com', '13700137000', '$2a$10$N9qo8uLOickgx2ZMRZoMyeIjZAgcfl7p92ldGxad68LJZdL17lhWy', 'user', NOW());

-- 添加测试车辆
INSERT INTO vehicles (id, vin, brand, model, year, color, plate_number, owner_id, status, created_at) VALUES
(1, 'LSVAG2180E2100001', '宝马', 'X5', 2023, '黑色', '京A12345', 1, 'active', NOW()),
(2, 'WBAJB3103JB100002', '奔驰', 'E300', 2022, '白色', '沪B67890', 2, 'active', NOW()),
(3, 'LFV3A28K8J3000003', '奥迪', 'A6L', 2023, '银灰色', '粤C11111', 1, 'active', NOW());

-- 添加数字钥匙
INSERT INTO keys (id, user_id, vehicle_id, type, status, protocol, key_identifier, name, description, permissions, usage_count, created_at) VALUES
(1, 1, 1, 'owner', 'active', 'CCC', 'KEY-CCC-001-BMW', '我的宝马X5', '主钥匙', '{"unlock": true, "lock": true, "start_engine": true, "open_trunk": true, "find_vehicle": true}'::jsonb, 15, NOW()),
(2, 1, 3, 'owner', 'active', 'ICCOA', 'KEY-ICCOA-002-AUDI', '我的奥迪A6L', '主钥匙', '{"unlock": true, "lock": true, "start_engine": true, "open_trunk": true}'::jsonb, 8, NOW()),
(3, 2, 2, 'owner', 'active', 'CCC', 'KEY-CCC-003-BENZ', '我的奔驰E300', '主钥匙', '{"unlock": true, "lock": true, "start_engine": true}'::jsonb, 22, NOW()),
(4, 1, 1, 'shared', 'active', 'CCC', 'KEY-CCC-004-SHARE', '分享给朋友的宝马', '朋友借用', '{"unlock": true, "lock": true, "start_engine": false}'::jsonb, 2, NOW());

-- 更新序列
SELECT setval('users_id_seq', 3, true);
SELECT setval('vehicles_id_seq', 3, true);
SELECT setval('keys_id_seq', 4, true);

-- 插入使用日志样例数据
INSERT INTO key_usage_logs (key_id, user_id, operation, status, device_info, location, created_at) VALUES
(1, 1, 'unlock', 'success', '{"device_type": "iPhone", "os_version": "iOS 17"}'::jsonb, '{"lat": 39.9042, "lng": 116.4074}'::jsonb, NOW() - INTERVAL '1 hour'),
(1, 1, 'start_engine', 'success', '{"device_type": "iPhone", "os_version": "iOS 17"}'::jsonb, '{"lat": 39.9042, "lng": 116.4074}'::jsonb, NOW() - INTERVAL '55 minutes'),
(1, 1, 'lock', 'success', '{"device_type": "iPhone", "os_version": "iOS 17"}'::jsonb, '{"lat": 39.9042, "lng": 116.4074}'::jsonb, NOW() - INTERVAL '30 minutes'),
(2, 1, 'unlock', 'success', '{"device_type": "Android", "os_version": "Android 14"}'::jsonb, '{"lat": 31.2304, "lng": 121.4737}'::jsonb, NOW() - INTERVAL '2 hours'),
(4, 3, 'unlock', 'success', '{"device_type": "iPhone", "os_version": "iOS 16"}'::jsonb, '{"lat": 39.9042, "lng": 116.4074}'::jsonb, NOW() - INTERVAL '1 day');
