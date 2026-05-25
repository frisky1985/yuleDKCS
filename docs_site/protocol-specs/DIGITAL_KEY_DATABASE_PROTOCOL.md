# yuleDKCS 数字钥匙数据库协议规范

> **版本**: v2.0  
> **状态**: 生产级规范  
> **适用**: Backend + Mobile + Embedded 三端协同  
> **经验来源**: 实际车载数字钥匙项目工程实践

---

## 1. 数据库设计哲学

### 1.1 核心设计原则

```
┌─────────────────────────────────────────────────────────────────┐
│                    数字钥匙数据库设计原则                          │
├─────────────────────────────────────────────────────────────────┤
│ 1. 安全性优先  - 所有敏感字段加密存储，密钥永不落地数据库            │
│ 2. 可追溯性   - 完整审计日志，支持全生命周期追踪                     │
│ 3. 多协议兼容 - 统一抽象层支持CCC/ICCOA/ICCE                       │
│ 4. 高并发支持 - 乐观锁+幂等设计，支持万级QPS                        │
│ 5. 软删除策略 - 数据保留90天，满足审计和恢复需求                     │
│ 6. 分区存储   - 热数据/冷数据分离，历史记录归档                      │
└─────────────────────────────────────────────────────────────────┘
```

### 1.2 表设计经验法则

| 经验项 | 规范 | 工程原因 |
|--------|------|----------|
| 主键类型 | `BIGINT UNSIGNED AUTO_INCREMENT` | 支持分布式ID，预留扩展空间 |
| 时间字段 | `DATETIME(3)` + 时区存储 | 精确到毫秒，跨时区场景 |
| 状态字段 | `TINYINT` + 状态表 | 节省空间，方便扩展 |
| 货币金额 | `DECIMAL(19,4)` | 避免浮点精度问题 |
| JSON字段 | MySQL 8.0+ `JSON` 类型 | 灵活存储扩展属性 |
| 软删除 | `deleted_at DATETIME` | 支持数据恢复和审计 |

---

## 2. 核心实体关系图 (ERD)

```
┌──────────────────────────────────────────────────────────────────────┐
│                        数字钥匙数据库 ER 图                           │
└──────────────────────────────────────────────────────────────────────┘

  ┌─────────────┐       ┌─────────────┐       ┌─────────────┐
  │   users     │───────│   keys      │───────│  vehicles   │
  │  (用户表)    │  1:N  │  (钥匙表)    │  N:1  │  (车辆表)    │
  └─────────────┘       └──────┬──────┘       └─────────────┘
         │                     │
         │              ┌──────┴──────┐
         │              │             │
  ┌──────┴──────┐  ┌────┴────┐   ┌────┴────┐
  │ user_devices│  │key_shares│   │key_logs │
  │(用户设备表) │  │(分享表) │   │(日志表) │
  └─────────────┘  └─────────┘   └─────────┘

  ┌─────────────┐       ┌─────────────┐       ┌─────────────┐
  │  pairings   │───────│  sessions   │───────│  commands   │
  │ (配对记录表) │       │ (会话表)    │       │ (命令表)    │
  └─────────────┘       └─────────────┘       └─────────────┘

  ┌─────────────┐       ┌─────────────┐
  │certificates │───────│  ota_updates│
  │ (证书表)    │       │ (OTA表)     │
  └─────────────┘       └─────────────┘
```

---

## 3. 核心表结构定义

### 3.1 用户表 (users)

```sql
CREATE TABLE `users` (
  `id` BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  `username` VARCHAR(50) NOT NULL COMMENT '用户名',
  `email` VARCHAR(100) NOT NULL COMMENT '邮箱',
  `phone` VARCHAR(20) DEFAULT NULL COMMENT '手机号',
  
  -- 密码安全：使用 bcrypt，永不存储明文
  `password_hash` VARCHAR(255) NOT NULL COMMENT '密码哈希',
  `password_changed_at` DATETIME(3) DEFAULT NULL COMMENT '密码修改时间',
  
  -- 用户类型：owner(车主), user(普通用户), admin(管理员)
  `role` TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '角色:1-user,2-owner,9-admin',
  
  -- 账户状态：0-禁用, 1-正常, 2-待验证, 3-锁定
  `status` TINYINT UNSIGNED NOT NULL DEFAULT 2 COMMENT '状态',
  `locked_until` DATETIME DEFAULT NULL COMMENT '锁定截止时间',
  `failed_login_attempts` TINYINT UNSIGNED DEFAULT 0 COMMENT '连续登录失败次数',
  
  -- 安全令牌
  `mfa_enabled` BOOLEAN DEFAULT FALSE COMMENT '是否启用多因素认证',
  `mfa_secret` VARCHAR(255) DEFAULT NULL COMMENT 'MFA密钥(加密存储)',
  
  -- 审计字段
  `last_login_at` DATETIME(3) DEFAULT NULL COMMENT '最后登录时间',
  `last_login_ip` VARCHAR(45) DEFAULT NULL COMMENT '最后登录IP',
  `created_at` DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  `updated_at` DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  `deleted_at` DATETIME(3) DEFAULT NULL COMMENT '软删除时间',
  
  -- 约束
  UNIQUE KEY `uk_username` (`username`),
  UNIQUE KEY `uk_email` (`email`),
  UNIQUE KEY `uk_phone` (`phone`),
  KEY `idx_status_created` (`status`, `created_at`),
  KEY `idx_deleted_at` (`deleted_at`)
  
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
  COMMENT='用户表';
```

**工程经验**:
- `phone` 使用 `VARCHAR(20)` 而非 `BIGINT`：支持国际号码格式（如 +86-138xxxx）
- `last_login_ip` 使用 `VARCHAR(45)`：支持 IPv6 地址
- `failed_login_attempts` 配合 `locked_until` 实现账户锁定机制
- 软删除通过 `deleted_at` 实现，而非 `is_deleted` 标记

---

### 3.2 车辆表 (vehicles)

```sql
CREATE TABLE `vehicles` (
  `id` BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  `owner_id` BIGINT UNSIGNED NOT NULL COMMENT '车主用户ID',
  
  -- 车辆识别
  `vin` VARCHAR(17) NOT NULL COMMENT '车辆识别号(Vehicle Identification Number)',
  `name` VARCHAR(100) DEFAULT NULL COMMENT '车辆昵称',
  `brand` VARCHAR(50) DEFAULT NULL COMMENT '品牌',
  `model` VARCHAR(50) DEFAULT NULL COMMENT '型号',
  `year` YEAR DEFAULT NULL COMMENT '年份',
  `color` VARCHAR(30) DEFAULT NULL COMMENT '颜色',
  `plate_number` VARCHAR(20) DEFAULT NULL COMMENT '车牌号',
  
  -- 硬件标识（用于BLE/UWB配对）
  `ble_mac` VARCHAR(17) DEFAULT NULL COMMENT 'BLE MAC地址(AA:BB:CC:DD:EE:FF)',
  `uwb_id` VARCHAR(64) DEFAULT NULL COMMENT 'UWB设备标识',
  `ecu_serial` VARCHAR(100) DEFAULT NULL COMMENT 'ECU序列号',
  
  -- 协议支持标识
  `support_ccc` BOOLEAN DEFAULT FALSE COMMENT '是否支持CCC协议',
  `support_iccoa` BOOLEAN DEFAULT FALSE COMMENT '是否支持ICCOA协议',
  `support_icce` BOOLEAN DEFAULT FALSE COMMENT '是否支持ICCE协议',
  `preferred_protocol` TINYINT DEFAULT 1 COMMENT '首选协议:1-CCC,2-ICCOA,3-ICCE',
  
  -- 车辆状态
  `status` TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '状态:0-未激活,1-正常,2-维修中,9-报废',
  `lock_status` TINYINT DEFAULT 1 COMMENT '锁定状态:0-未知,1-锁定,2-解锁',
  `engine_status` TINYINT DEFAULT 0 COMMENT '引擎状态:0-关闭,1-启动中,2-运行中',
  `online_status` TINYINT DEFAULT 0 COMMENT '在线状态:0-离线,1-在线,2-休眠',
  
  -- 位置信息（加密存储敏感位置）
  `last_location` POINT DEFAULT NULL COMMENT '最后位置(WGS84坐标系)',
  `location_updated_at` DATETIME(3) DEFAULT NULL,
  `location_accuracy` FLOAT DEFAULT NULL COMMENT '定位精度(米)',
  
  -- 软件版本
  `software_version` VARCHAR(50) DEFAULT NULL COMMENT '当前软件版本',
  `hardware_version` VARCHAR(50) DEFAULT NULL COMMENT '硬件版本',
  `last_seen_at` DATETIME(3) DEFAULT NULL COMMENT '最后通信时间',
  
  -- 审计字段
  `created_at` DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  `updated_at` DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  `deleted_at` DATETIME(3) DEFAULT NULL,
  
  -- 约束
  UNIQUE KEY `uk_vin` (`vin`),
  UNIQUE KEY `uk_ble_mac` (`ble_mac`),
  KEY `idx_owner_id` (`owner_id`),
  KEY `idx_status` (`status`),
  KEY `idx_online_status` (`online_status`),
  KEY `idx_last_seen` (`last_seen_at`),
  SPATIAL KEY `idx_location` (`last_location`),
  
  FOREIGN KEY (`owner_id`) REFERENCES `users`(`id`) ON DELETE CASCADE
  
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
  COMMENT='车辆表';
```

**工程经验**:
- `vin` 必须唯一且不可修改，使用 `VARCHAR(17)` 符合ISO标准
- `last_location` 使用 MySQL 8.0 的 `POINT` 类型支持空间查询
- 协议支持使用布尔标志而非枚举，支持多协议同时启用
- `last_seen_at` 用于判断车辆在线状态和心跳超时

---

### 3.3 数字钥匙表 (keys) ★核心表

```sql
CREATE TABLE `keys` (
  `id` BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  `key_uuid` CHAR(36) NOT NULL COMMENT '钥匙UUID(v4)，三端统一标识',
  
  -- 关联关系
  `user_id` BIGINT UNSIGNED NOT NULL COMMENT '持有者用户ID',
  `vehicle_id` BIGINT UNSIGNED NOT NULL COMMENT '关联车辆ID',
  `issuer_id` BIGINT UNSIGNED NOT NULL COMMENT '发行者用户ID（车主）',
  
  -- 钥匙类型
  `key_type` TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT 
    '类型:1-车主钥匙,2-家庭共享,3-临时分享,4-代客泊车,5-维修模式',
  
  -- 协议相关
  `protocol` TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '协议:1-CCC,2-ICCOA,3-ICCE',
  `protocol_version` VARCHAR(10) DEFAULT '2.0' COMMENT '协议版本',
  
  -- 钥匙标识（业务层展示）
  `name` VARCHAR(100) DEFAULT NULL COMMENT '钥匙名称（用户自定义）',
  `key_identifier` VARCHAR(64) DEFAULT NULL COMMENT '协议层钥匙标识',
  
  -- 安全关键字段（加密存储）
  `key_slot_id` INT UNSIGNED DEFAULT NULL COMMENT '安全芯片密钥槽位ID',
  `encryption_key_id` VARCHAR(100) DEFAULT NULL COMMENT 'KMS密钥ID',
  `key_data_ref` VARCHAR(255) DEFAULT NULL COMMENT '密钥数据引用(HSM/KMS)',
  
  -- 权限配置（JSON格式，支持灵活扩展）
  `permissions` JSON NOT NULL COMMENT '权限配置: {"unlock":true,"lock":true,...}',
  
  -- 时效控制
  `valid_from` DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) COMMENT '生效时间',
  `valid_until` DATETIME(3) DEFAULT NULL COMMENT '过期时间（NULL表示永久）',
  `auto_expire_action` TINYINT DEFAULT 1 COMMENT '过期动作:1-失效,2-提醒,3-自动续期',
  
  -- 使用限制
  `max_daily_uses` INT UNSIGNED DEFAULT NULL COMMENT '每日最大使用次数',
  `today_use_count` INT UNSIGNED DEFAULT 0 COMMENT '今日使用次数',
  `use_count_reset_at` DATETIME DEFAULT NULL COMMENT '计数重置时间',
  `total_use_count` BIGINT UNSIGNED DEFAULT 0 COMMENT '累计使用次数',
  
  -- 状态机
  `status` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 
    '状态:0-待激活,1-正常,2-暂停,3-过期,4-撤销,5-已用完',
  `activation_status` TINYINT UNSIGNED DEFAULT 0 COMMENT '激活状态:0-未激活,1-已激活,2-激活中',
  
  -- 配对信息
  `paired_device_id` BIGINT UNSIGNED DEFAULT NULL COMMENT '配对设备ID',
  `paired_at` DATETIME(3) DEFAULT NULL COMMENT '配对时间',
  `last_used_at` DATETIME(3) DEFAULT NULL COMMENT '最后使用时间',
  `last_used_location` POINT DEFAULT NULL COMMENT '最后使用位置',
  
  -- 撤销信息
  `revoked_at` DATETIME(3) DEFAULT NULL COMMENT '撤销时间',
  `revoked_by` BIGINT UNSIGNED DEFAULT NULL COMMENT '撤销操作人ID',
  `revoke_reason` VARCHAR(255) DEFAULT NULL COMMENT '撤销原因',
  
  -- 审计字段
  `created_at` DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  `updated_at` DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  `deleted_at` DATETIME(3) DEFAULT NULL,
  `version` INT UNSIGNED NOT NULL DEFAULT 1 COMMENT '乐观锁版本号',
  
  -- 约束
  UNIQUE KEY `uk_key_uuid` (`key_uuid`),
  KEY `idx_user_id` (`user_id`),
  KEY `idx_vehicle_id` (`vehicle_id`),
  KEY `idx_issuer_id` (`issuer_id`),
  KEY `idx_status` (`status`),
  KEY `idx_valid_time` (`valid_from`, `valid_until`),
  KEY `idx_protocol` (`protocol`, `protocol_version`),
  KEY `idx_key_type` (`key_type`),
  
  FOREIGN KEY (`user_id`) REFERENCES `users`(`id`) ON DELETE CASCADE,
  FOREIGN KEY (`vehicle_id`) REFERENCES `vehicles`(`id`) ON DELETE CASCADE,
  FOREIGN KEY (`issuer_id`) REFERENCES `users`(`id`) ON DELETE CASCADE
  
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
  COMMENT='数字钥匙表';
```

**工程经验 - 关键设计决策**:

#### 1. 密钥存储策略
```
❌ 错误做法: 将私钥直接存储在数据库中
✅ 正确做法: 
   - 敏感密钥存储在HSM（硬件安全模块）或KMS（密钥管理服务）
   - 数据库只存储密钥引用(key_data_ref)和槽位ID(key_slot_id)
   - 所有密钥操作通过API调用HSM/KMS完成
```

#### 2. 权限设计（RBAC + ABAC）
```json
{
  "unlock": true,
  "lock": true,
  "start_engine": true,
  "stop_engine": true,
  "open_trunk": false,
  "close_trunk": false,
  "open_windows": false,
  "close_windows": false,
  "remote_start": false,
  "location_access": true,
  "speed_limit": 120,
  "geofence": {
    "enabled": true,
    "center": {"lat": 39.9042, "lng": 116.4074},
    "radius": 5000
  }
}
```

#### 3. 状态机设计
```
                    ┌──────────┐
         ┌─────────│  待激活   │◄────────┐
         │         │(pending)  │         │
         │         └────┬─────┘         │
    激活失败│              │激活成功         │撤销
         │              ▼              │
         │         ┌──────────┐         │
         └────────►│   正常    │─────────┘
                   │(active)  │
                   └────┬─────┘
                        │
        ┌───────────────┼───────────────┐
        │               │               │
        ▼               ▼               ▼
   ┌─────────┐    ┌─────────┐    ┌─────────┐
   │  暂停   │    │  过期   │    │  撤销   │
   │(paused) │    │(expired)│    │(revoked)│
   └────┬────┘    └────┬────┘    └────┬────┘
        │               │               │
        └───────────────┴───────────────┘
                          │
                          ▼
                    ┌──────────┐
                    │  已用完   │
                    │(depleted)│
                    └──────────┘
```

#### 4. 使用限制实现
- `max_daily_uses` + `today_use_count`：实现每日限额
- `use_count_reset_at`：记录上次重置时间，用于日终重置逻辑
- 建议配合 Redis 做实时计数，数据库做最终一致性

---

### 3.4 钥匙分享表 (key_shares)

```sql
CREATE TABLE `key_shares` (
  `id` BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  `share_uuid` CHAR(36) NOT NULL COMMENT '分享UUID',
  
  -- 关联关系
  `key_id` BIGINT UNSIGNED NOT NULL COMMENT '源钥匙ID',
  `shared_by` BIGINT UNSIGNED NOT NULL COMMENT '分享人ID',
  `shared_to` BIGINT UNSIGNED DEFAULT NULL COMMENT '接收人ID（注册用户）',
  `shared_to_email` VARCHAR(100) DEFAULT NULL COMMENT '接收人邮箱（非注册用户）',
  `shared_to_phone` VARCHAR(20) DEFAULT NULL COMMENT '接收人手机号',
  
  -- 分享类型
  `share_type` TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT 
    '类型:1-注册用户分享,2-链接分享,3-二维码分享',
  
  -- 权限继承（继承原钥匙，可进一步限制）
  `permissions` JSON NOT NULL COMMENT '分享时的权限配置（子集）',
  
  -- 时效控制
  `valid_from` DATETIME(3) NOT NULL,
  `valid_until` DATETIME(3) NOT NULL COMMENT '必须设置过期时间',
  `max_uses` INT UNSIGNED DEFAULT NULL COMMENT '最大使用次数',
  `used_count` INT UNSIGNED DEFAULT 0 COMMENT '已使用次数',
  
  -- 分享链接专用
  `share_token` VARCHAR(255) DEFAULT NULL COMMENT '分享令牌（加密）',
  `share_url` VARCHAR(500) DEFAULT NULL COMMENT '分享链接',
  `require_approval` BOOLEAN DEFAULT FALSE COMMENT '是否需要审批',
  `approved_by` BIGINT UNSIGNED DEFAULT NULL COMMENT '审批人ID',
  `approved_at` DATETIME(3) DEFAULT NULL,
  
  -- 状态
  `status` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 
    '状态:0-待接受,1-已接受,2-已拒绝,3-已过期,4-已撤销,5-已用完',
  `accepted_at` DATETIME(3) DEFAULT NULL,
  
  -- 审计
  `created_at` DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  `updated_at` DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  `revoked_at` DATETIME(3) DEFAULT NULL,
  `revoked_by` BIGINT UNSIGNED DEFAULT NULL,
  `revoke_reason` VARCHAR(255) DEFAULT NULL,
  `deleted_at` DATETIME(3) DEFAULT NULL,
  
  -- 约束
  UNIQUE KEY `uk_share_uuid` (`share_uuid`),
  UNIQUE KEY `uk_share_token` (`share_token`),
  KEY `idx_key_id` (`key_id`),
  KEY `idx_shared_by` (`shared_by`),
  KEY `idx_shared_to` (`shared_to`),
  KEY `idx_status` (`status`),
  KEY `idx_valid_time` (`valid_from`, `valid_until`),
  
  FOREIGN KEY (`key_id`) REFERENCES `keys`(`id`) ON DELETE CASCADE,
  FOREIGN KEY (`shared_by`) REFERENCES `users`(`id`) ON DELETE CASCADE,
  FOREIGN KEY (`shared_to`) REFERENCES `users`(`id`) ON DELETE SET NULL
  
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
  COMMENT='钥匙分享表';
```

**工程经验**:
- 分享权限必须是原钥匙权限的**子集**，防止权限提升
- 必须设置过期时间（安全合规要求）
- 支持注册用户分享和临时链接分享两种模式

---

### 3.5 钥匙使用日志表 (key_logs)

```sql
CREATE TABLE `key_logs` (
  `id` BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  `log_uuid` CHAR(36) NOT NULL COMMENT '日志UUID',
  
  -- 关联关系
  `key_id` BIGINT UNSIGNED NOT NULL COMMENT '钥匙ID',
  `user_id` BIGINT UNSIGNED NOT NULL COMMENT '操作用户ID',
  `vehicle_id` BIGINT UNSIGNED NOT NULL COMMENT '车辆ID',
  `share_id` BIGINT UNSIGNED DEFAULT NULL COMMENT '分享记录ID（如果是通过分享使用）',
  
  -- 操作信息
  `action` TINYINT UNSIGNED NOT NULL COMMENT 
    '操作:1-解锁,2-上锁,3-启动引擎,4-关闭引擎,5-开后备箱,6-开空调,...',
  `action_name` VARCHAR(50) DEFAULT NULL COMMENT '操作名称（冗余存储便于查询）',
  `action_result` TINYINT NOT NULL DEFAULT 0 COMMENT '结果:0-失败,1-成功,2-超时,3-取消',
  `failure_reason` VARCHAR(255) DEFAULT NULL COMMENT '失败原因',
  
  -- 设备信息
  `device_id` BIGINT UNSIGNED DEFAULT NULL COMMENT '设备ID',
  `device_type` TINYINT DEFAULT NULL COMMENT '设备类型:1-iOS,2-Android,3-车机',
  `device_model` VARCHAR(50) DEFAULT NULL COMMENT '设备型号',
  `os_version` VARCHAR(30) DEFAULT NULL COMMENT '操作系统版本',
  `app_version` VARCHAR(30) DEFAULT NULL COMMENT 'App版本',
  
  -- 位置信息
  `location` POINT DEFAULT NULL COMMENT '操作位置',
  `location_accuracy` FLOAT DEFAULT NULL,
  `vehicle_location` POINT DEFAULT NULL COMMENT '车辆当时位置',
  `distance_to_vehicle` FLOAT DEFAULT NULL COMMENT '与车辆距离(米)',
  
  -- 网络信息
  `ip_address` VARCHAR(45) DEFAULT NULL COMMENT 'IP地址',
  `connection_type` VARCHAR(20) DEFAULT NULL COMMENT '连接类型:BLET/UWB/4G/5G/WiFi',
  `rssi` INT DEFAULT NULL COMMENT 'BLE信号强度',
  `latency_ms` INT DEFAULT NULL COMMENT '通信延迟(ms)',
  
  -- 协议信息
  `protocol` TINYINT UNSIGNED DEFAULT NULL COMMENT '使用的协议',
  `session_id` VARCHAR(100) DEFAULT NULL COMMENT '会话ID',
  `command_id` VARCHAR(100) DEFAULT NULL COMMENT '命令ID（用于追踪）',
  
  -- 元数据（扩展信息）
  `metadata` JSON DEFAULT NULL COMMENT '扩展元数据',
  
  -- 时间戳
  `performed_at` DATETIME(3) NOT NULL COMMENT '操作执行时间',
  `created_at` DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  
  -- 约束
  UNIQUE KEY `uk_log_uuid` (`log_uuid`),
  KEY `idx_key_id` (`key_id`),
  KEY `idx_user_id` (`user_id`),
  KEY `idx_vehicle_id` (`vehicle_id`),
  KEY `idx_action` (`action`),
  KEY `idx_action_result` (`action_result`),
  KEY `idx_performed_at` (`performed_at`),
  KEY `idx_created_at` (`created_at`),
  SPATIAL KEY `idx_location` (`location`)
  
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
  COMMENT='钥匙使用日志表'
  PARTITION BY RANGE (YEAR(performed_at)) (
    PARTITION p2023 VALUES LESS THAN (2024),
    PARTITION p2024 VALUES LESS THAN (2025),
    PARTITION p2025 VALUES LESS THAN (2026),
    PARTITION pfuture VALUES LESS THAN MAXVALUE
  );
```

**工程经验 - 日志表设计要点**:

#### 1. 分区策略
- 按 `performed_at` 年分区，便于归档和清理历史数据
- 冷数据（超过1年）可迁移到归档表或对象存储

#### 2. 字段冗余设计
- `action_name` 冗余存储，避免联表查询
- `user_id` 和 `key_id` 同时存储，支持多种查询维度

#### 3. 地理位置分析
- 存储 `location` 和 `vehicle_location`，支持轨迹分析
- 计算 `distance_to_vehicle`，用于异常检测（如远程开锁时距离过远）

#### 4. 性能优化
- 日志写入使用**异步队列**（如 Kafka + Consumer），不阻塞主流程
- 查询频繁的最近7天数据保留在热存储，历史数据归档

---

### 3.6 设备表 (user_devices)

```sql
CREATE TABLE `user_devices` (
  `id` BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  `device_uuid` CHAR(36) NOT NULL COMMENT '设备UUID',
  
  `user_id` BIGINT UNSIGNED NOT NULL COMMENT '用户ID',
  `device_name` VARCHAR(100) DEFAULT NULL COMMENT '设备名称',
  
  -- 设备标识
  `device_type` TINYINT UNSIGNED NOT NULL COMMENT '类型:1-iPhone,2-iPad,3-Android,4-AndroidTablet,5-Watch',
  `device_model` VARCHAR(50) DEFAULT NULL COMMENT '设备型号',
  `os_version` VARCHAR(30) DEFAULT NULL COMMENT '系统版本',
  
  -- 安全标识
  `device_token` VARCHAR(255) DEFAULT NULL COMMENT '设备推送Token',
  `public_key` TEXT DEFAULT NULL COMMENT '设备公钥（用于端到端加密）',
  `trusted` BOOLEAN DEFAULT FALSE COMMENT '是否受信任设备',
  `trust_established_at` DATETIME(3) DEFAULT NULL COMMENT '信任建立时间',
  
  -- BLE/UWB标识
  `ble_mac` VARCHAR(17) DEFAULT NULL COMMENT '设备BLE MAC',
  `uwb_capable` BOOLEAN DEFAULT FALSE COMMENT '是否支持UWB',
  `uwb_id` VARCHAR(64) DEFAULT NULL COMMENT 'UWB标识',
  
  -- 状态
  `status` TINYINT UNSIGNED DEFAULT 1 COMMENT '状态:0-禁用,1-正常,2-丢失,3-注销',
  `last_active_at` DATETIME(3) DEFAULT NULL,
  
  `created_at` DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  `updated_at` DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  `deleted_at` DATETIME(3) DEFAULT NULL,
  
  UNIQUE KEY `uk_device_uuid` (`device_uuid`),
  KEY `idx_user_id` (`user_id`),
  KEY `idx_ble_mac` (`ble_mac`),
  KEY `idx_status` (`status`)
  
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
  COMMENT='用户设备表';
```

---

### 3.7 配对记录表 (pairings)

```sql
CREATE TABLE `pairings` (
  `id` BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  `pairing_uuid` CHAR(36) NOT NULL COMMENT '配对UUID',
  
  `key_id` BIGINT UNSIGNED NOT NULL COMMENT '钥匙ID',
  `device_id` BIGINT UNSIGNED NOT NULL COMMENT '设备ID',
  `vehicle_id` BIGINT UNSIGNED NOT NULL COMMENT '车辆ID',
  
  -- 配对信息
  `protocol` TINYINT UNSIGNED NOT NULL COMMENT '协议类型',
  `pairing_method` TINYINT DEFAULT 1 COMMENT '配对方式:1-APP,2-钥匙,3-云端',
  `pairing_data` JSON NOT NULL COMMENT '配对数据（协议相关）',
  
  -- 安全信息
  `session_keys` JSON DEFAULT NULL COMMENT '会话密钥（加密存储）',
  `counter` BIGINT UNSIGNED DEFAULT 0 COMMENT '滚动计数器',
  `last_counter` BIGINT UNSIGNED DEFAULT 0 COMMENT '上次计数器值',
  
  -- 状态
  `status` TINYINT UNSIGNED DEFAULT 1 COMMENT '状态:0-未激活,1-已配对,2-已解绑',
  `paired_at` DATETIME(3) NOT NULL,
  `unpaired_at` DATETIME(3) DEFAULT NULL,
  `unpair_reason` VARCHAR(255) DEFAULT NULL,
  
  `created_at` DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  `updated_at` DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  
  UNIQUE KEY `uk_pairing_uuid` (`pairing_uuid`),
  KEY `idx_key_id` (`key_id`),
  KEY `idx_device_id` (`device_id`),
  KEY `idx_vehicle_id` (`vehicle_id`),
  KEY `idx_status` (`status`)
  
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
  COMMENT='设备配对记录表';
```

---

## 4. 索引优化策略

### 4.1 索引设计原则

```sql
-- 1. 高频查询场景
-- 场景：用户查看自己的钥匙列表
-- 优化：复合索引覆盖查询
CREATE INDEX `idx_keys_user_status` ON `keys` (`user_id`, `status`, `valid_until`);

-- 2. 时间范围查询
-- 场景：查询某段时间内的使用日志
-- 优化：分区键 + 索引
CREATE INDEX `idx_logs_vehicle_time` ON `key_logs` (`vehicle_id`, `performed_at`);

-- 3. 空间查询
-- 场景：查找车辆附近的可用钥匙
-- 优化：空间索引（MySQL 8.0）
CREATE SPATIAL INDEX `idx_vehicle_location` ON `vehicles` (`last_location`);

-- 4. JSON字段查询
-- 场景：查询有特定权限的钥匙
-- 优化：虚拟列 + 索引（MySQL 8.0.13+）
ALTER TABLE `keys` 
  ADD COLUMN `perm_unlock` AS (JSON_UNQUOTE(JSON_EXTRACT(`permissions`, '$.unlock'))) VIRTUAL,
  ADD INDEX `idx_perm_unlock` (`perm_unlock`);
```

### 4.2 查询优化示例

```sql
-- 反例：低效的模糊查询
SELECT * FROM keys WHERE name LIKE '%宝马%';

-- 正例：使用全文索引
ALTER TABLE `keys` ADD FULLTEXT INDEX `ft_name` (`name`);
SELECT * FROM keys WHERE MATCH(name) AGAINST('宝马');
```

---

## 5. 数据生命周期管理

### 5.1 自动清理策略

```sql
-- 1. 软删除数据清理（90天后物理删除）
-- 定期执行（建议每天凌晨）
DELETE FROM keys 
WHERE deleted_at IS NOT NULL 
  AND deleted_at < DATE_SUB(NOW(), INTERVAL 90 DAY);

-- 2. 日志数据归档（1年后迁移）
-- 步骤1：创建归档表
CREATE TABLE key_logs_archive LIKE key_logs;

-- 步骤2：迁移历史数据
INSERT INTO key_logs_archive 
SELECT * FROM key_logs 
WHERE performed_at < DATE_SUB(NOW(), INTERVAL 1 YEAR);

-- 步骤3：删除原表数据
DELETE FROM key_logs 
WHERE performed_at < DATE_SUB(NOW(), INTERVAL 1 YEAR);
```

### 5.2 数据保留政策

| 数据类型 | 保留时间 | 说明 |
|----------|----------|------|
| 用户数据 | 永久 | 账户存续期间保留 |
| 钥匙数据 | 撤销后90天 | 满足审计需求 |
| 使用日志 | 2年 | 之后归档到冷存储 |
| 配对记录 | 解绑后1年 | 用于安全审计 |
| 分享记录 | 过期后90天 | 满足合规要求 |

---

## 6. 安全设计规范

### 6.1 敏感数据处理

```sql
-- 1. 字段级加密示例
-- 使用MySQL的AES_ENCRYPT进行字段加密
ALTER TABLE `users` ADD COLUMN `phone_encrypted` VARBINARY(255);
UPDATE `users` SET 
  `phone_encrypted` = AES_ENCRYPT(`phone`, @encryption_key),
  `phone` = NULL; -- 删除明文

-- 2. 透明数据加密（TDE）
-- 表空间级别加密（需要MySQL Enterprise或Percona）
CREATE TABLE `keys` (...) 
ENCRYPTION='Y';
```

### 6.2 访问控制

```sql
-- 最小权限原则
-- 创建应用专用账户
CREATE USER 'dkcs_app'@'10.0.%' IDENTIFIED BY 'strong_password';

-- 只授予必要权限
GRANT SELECT, INSERT, UPDATE ON `yuledkcs`.`keys` TO 'dkcs_app'@'10.0.%';
GRANT SELECT, INSERT ON `yuledkcs`.`key_logs` TO 'dkcs_app'@'10.0.%';
-- 不允许DELETE，软删除通过UPDATE实现
REVOKE DELETE ON `yuledkcs`.* FROM 'dkcs_app'@'10.0.%';
```

---

## 7. 附录：常用查询模板

### 7.1 业务查询

```sql
-- 查询用户的有效钥匙
SELECT k.*, v.vin, v.name as vehicle_name
FROM keys k
JOIN vehicles v ON k.vehicle_id = v.id
WHERE k.user_id = ?
  AND k.status = 1
  AND k.valid_until > NOW()
  AND k.deleted_at IS NULL;

-- 查询车辆最近24小时的使用统计
SELECT 
  action,
  COUNT(*) as count,
  SUM(CASE WHEN action_result = 1 THEN 1 ELSE 0 END) as success_count
FROM key_logs
WHERE vehicle_id = ?
  AND performed_at > DATE_SUB(NOW(), INTERVAL 24 HOUR)
GROUP BY action;

-- 查找过期的钥匙并自动更新状态
UPDATE keys 
SET status = 3, updated_at = NOW()
WHERE status = 1 
  AND valid_until < NOW()
  AND valid_until IS NOT NULL;
```

### 7.2 运维查询

```sql
-- 检查表大小
SELECT 
  table_name,
  ROUND(data_length / 1024 / 1024, 2) AS 'data_size(MB)',
  ROUND(index_length / 1024 / 1024, 2) AS 'index_size(MB)'
FROM information_schema.TABLES
WHERE table_schema = 'yuledkcs'
ORDER BY data_length DESC;

-- 慢查询分析
SELECT * FROM mysql.slow_log 
WHERE start_time > DATE_SUB(NOW(), INTERVAL 1 DAY)
ORDER BY query_time DESC
LIMIT 20;
```

---

**文档维护**: yuleDKCS 工程团队  
**最后更新**: 2026-05-15  
**下次评审**: 2026-08-15
