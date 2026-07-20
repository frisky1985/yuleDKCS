-- ============================================================
-- Digital Key Cloud Service - Database Schema
-- MySQL 8.0+ / InnoDB / utf8mb4
-- 设计要点: 高并发读写、分库分表预留、软删除、审计追踪
-- ============================================================

SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

-- ============================================================
-- 1. 用户 & 设备管理
-- ============================================================

CREATE TABLE `dk_user` (
    `id`              BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `user_id`         VARCHAR(64)  NOT NULL COMMENT '用户唯一ID (手机厂商提供的open_id)',
    `phone_vendor`    VARCHAR(32)  NOT NULL COMMENT '手机厂商: apple/samsung/xiaomi/oppo/vivo/huawei',
    `phone_model`     VARCHAR(64)  DEFAULT NULL COMMENT '手机型号',
    `os_version`      VARCHAR(32)  DEFAULT NULL COMMENT '操作系统版本',
    `app_version`     VARCHAR(32)  DEFAULT NULL COMMENT 'App版本',
    `status`          TINYINT      NOT NULL DEFAULT 1 COMMENT '1=正常 2=冻结 3=注销',
    `created_at`      DATETIME(3)  NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    `updated_at`      DATETIME(3)  NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
    `deleted_at`      DATETIME(3)  DEFAULT NULL COMMENT '软删除时间',
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_user_vendor` (`user_id`, `phone_vendor`),
    KEY `idx_phone_vendor` (`phone_vendor`),
    KEY `idx_status` (`status`),
    KEY `idx_created_at` (`created_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='数字钥匙用户表';

CREATE TABLE `dk_device` (
    `id`              BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `device_id`       VARCHAR(128) NOT NULL COMMENT '设备唯一标识 (SE serial / device fingerprint)',
    `user_id`         BIGINT UNSIGNED NOT NULL COMMENT '关联用户ID',
    `device_type`     VARCHAR(32)  NOT NULL COMMENT '设备类型: phone/wearable/key_fob',
    `se_type`         VARCHAR(32)  DEFAULT NULL COMMENT '安全元件类型: apple_se/samsung_knox/ccc_se',
    `se_public_key`   BLOB         DEFAULT NULL COMMENT 'SE公钥 (DER格式, AES-256-GCM加密存储)',
    `ble_mac`         VARCHAR(17)  DEFAULT NULL COMMENT 'BLE MAC地址',
    `uwb_capability`  TINYINT      NOT NULL DEFAULT 0 COMMENT 'UWB能力: 0=无 1=FiRa 2=CCC 3=ICCOA',
    `nfc_capability`  TINYINT      NOT NULL DEFAULT 0 COMMENT 'NFC能力: 0=无 1=TypeA 2=TypeB 3=TypeF',
    `status`          TINYINT      NOT NULL DEFAULT 1 COMMENT '1=已绑定 2=已解绑 3=挂失 4=暂停',
    `last_active_at`  DATETIME(3)  DEFAULT NULL COMMENT '最后活跃时间',
    `created_at`      DATETIME(3)  NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    `updated_at`      DATETIME(3)  NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
    `deleted_at`      DATETIME(3)  DEFAULT NULL,
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_device_id` (`device_id`),
    KEY `idx_user_id` (`user_id`),
    KEY `idx_status` (`status`),
    KEY `idx_device_type` (`device_type`),
    KEY `idx_last_active` (`last_active_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='用户设备表';

-- ============================================================
-- 2. 车辆管理
-- ============================================================

CREATE TABLE `dk_vehicle` (
    `id`              BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `vin`             VARCHAR(17)  NOT NULL COMMENT '车辆VIN码',
    `brand`           VARCHAR(32)  NOT NULL COMMENT '品牌',
    `model`           VARCHAR(64)  NOT NULL COMMENT '车型',
    `year`            SMALLINT     NOT NULL COMMENT '年份',
    `tsp_id`          VARCHAR(64)  DEFAULT NULL COMMENT 'TSP系统车辆ID',
    `tsp_endpoint`    VARCHAR(256) DEFAULT NULL COMMENT 'TSP推送端点',
    `tcu_id`          VARCHAR(64)  DEFAULT NULL COMMENT 'TCU设备ID',
    `protocol_support`VARCHAR(128) NOT NULL DEFAULT 'ccc,iccoa' COMMENT '支持的协议: ccc,iccoa,icce',
    `hw_version`      VARCHAR(64)  DEFAULT NULL COMMENT '硬件版本 (BLE/UWB/NFC芯片型号)',
    `fw_version`      VARCHAR(64)  DEFAULT NULL COMMENT '固件版本',
    `max_keys`        TINYINT      NOT NULL DEFAULT 8 COMMENT '最大绑定密钥数',
    `status`          TINYINT      NOT NULL DEFAULT 1 COMMENT '1=在线 2=离线 3=维修中 4=报废',
    `last_heartbeat`  DATETIME(3)  DEFAULT NULL COMMENT '最后心跳时间',
    `created_at`      DATETIME(3)  NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    `updated_at`      DATETIME(3)  NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
    `deleted_at`      DATETIME(3)  DEFAULT NULL,
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_vin` (`vin`),
    KEY `idx_tsp_id` (`tsp_id`),
    KEY `idx_brand_model` (`brand`, `model`),
    KEY `idx_status` (`status`),
    KEY `idx_last_heartbeat` (`last_heartbeat`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='车辆信息表';

-- ============================================================
-- 3. 数字密钥管理 (核心表)
-- ============================================================

CREATE TABLE `dk_digital_key` (
    `id`              BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `key_id`          VARCHAR(64)  NOT NULL COMMENT '密钥唯一ID (UUID v7)',
    `vehicle_id`      BIGINT UNSIGNED NOT NULL COMMENT '关联车辆ID',
    `device_id`       BIGINT UNSIGNED NOT NULL COMMENT '绑定设备ID',
    `user_id`         BIGINT UNSIGNED NOT NULL COMMENT '关联用户ID',
    `key_type`        TINYINT      NOT NULL COMMENT '1=Owner(车主) 2=Friend(朋友) 3=Service(服务) 4=Temporary(临时)',
    `protocol`        VARCHAR(16)  NOT NULL COMMENT '协议: ccc_dk3/iccoa_dk30/iccoa_dk40/icce',
    `access_level`    SMALLINT     NOT NULL DEFAULT 0 COMMENT '权限位掩码: bit0=闭锁 bit1=解锁 bit2=引擎 bit3=后备箱 bit4=车窗 bit5=空调 bit6=寻车 bit7=座椅',
    `distance_limit`  INT          DEFAULT NULL COMMENT '距离限制(mm), NULL=无限制',
    `time_restriction`JSON         DEFAULT NULL COMMENT '时间限制 {"weekdays":[1,2,3,4,5],"start":"08:00","end":"18:00"}',
    `max_uses`        INT          DEFAULT NULL COMMENT '最大使用次数, NULL=无限制',
    `used_count`      INT          NOT NULL DEFAULT 0 COMMENT '已使用次数',
    `key_version`     INT          NOT NULL DEFAULT 1 COMMENT '密钥版本号',
    `vehicle_pubkey`  BLOB         DEFAULT NULL COMMENT '车端公钥 (AES-256-GCM加密)',
    `vehicle_key_ref` VARCHAR(128) DEFAULT NULL COMMENT 'HSM中车端密钥引用',
    `device_pubkey`   BLOB         DEFAULT NULL COMMENT '设备端公钥 (AES-256-GCM加密)',
    `shared_secret`   BLOB         DEFAULT NULL COMMENT 'ECDH共享密钥 (HSM加密)',
    `valid_from`      DATETIME(3)  NOT NULL COMMENT '生效时间',
    `valid_until`     DATETIME(3)  NOT NULL COMMENT '过期时间',
    `status`          TINYINT      NOT NULL DEFAULT 1 COMMENT '1=Active 2=Suspended 3=Revoked 4=Expired',
    `revoked_at`      DATETIME(3)  DEFAULT NULL COMMENT '撤销时间',
    `revoke_reason`   VARCHAR(256) DEFAULT NULL COMMENT '撤销原因',
    `created_at`      DATETIME(3)  NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    `updated_at`      DATETIME(3)  NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
    `deleted_at`      DATETIME(3)  DEFAULT NULL,
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_key_id` (`key_id`),
    KEY `idx_vehicle_id` (`vehicle_id`),
    KEY `idx_device_id` (`device_id`),
    KEY `idx_user_id` (`user_id`),
    KEY `idx_key_type` (`key_type`),
    KEY `idx_protocol` (`protocol`),
    KEY `idx_status` (`status`),
    KEY `idx_valid_period` (`valid_from`, `valid_until`),
    KEY `idx_vehicle_status` (`vehicle_id`, `status`),
    KEY `idx_created_at` (`created_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='数字密钥表 (核心)';

-- ============================================================
-- 4. 密钥分享
-- ============================================================

CREATE TABLE `dk_key_share` (
    `id`              BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `share_id`        VARCHAR(64)  NOT NULL COMMENT '分享ID (UUID v7)',
    `key_id`          VARCHAR(64)  NOT NULL COMMENT '源密钥ID',
    `from_user_id`    BIGINT UNSIGNED NOT NULL COMMENT '分享发起人',
    `to_user_id`      BIGINT UNSIGNED DEFAULT NULL COMMENT '接收人 (NULL=通过分享码领取)',
    `to_phone_vendor` VARCHAR(32)  DEFAULT NULL COMMENT '接收人手机厂商',
    `share_code`      VARCHAR(32)  DEFAULT NULL COMMENT '分享码 (6位数字, AES加密)',
    `access_level`    SMALLINT     NOT NULL COMMENT '分享权限位掩码',
    `valid_from`      DATETIME(3)  NOT NULL,
    `valid_until`     DATETIME(3)  NOT NULL,
    `max_uses`        INT          DEFAULT NULL,
    `status`          TINYINT      NOT NULL DEFAULT 1 COMMENT '1=Pending 2=Accepted 3=Rejected 4=Cancelled 5=Expired',
    `accepted_at`     DATETIME(3)  DEFAULT NULL,
    `cancelled_at`    DATETIME(3)  DEFAULT NULL,
    `created_at`      DATETIME(3)  NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    `updated_at`      DATETIME(3)  NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_share_id` (`share_id`),
    UNIQUE KEY `uk_share_code` (`share_code`),
    KEY `idx_key_id` (`key_id`),
    KEY `idx_from_user` (`from_user_id`),
    KEY `idx_to_user` (`to_user_id`),
    KEY `idx_status` (`status`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='密钥分享记录表';

-- ============================================================
-- 5. 车辆状态 & 控制指令
-- ============================================================

CREATE TABLE `dk_vehicle_status` (
    `id`              BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `vehicle_id`      BIGINT UNSIGNED NOT NULL,
    `lock_status`     TINYINT      NOT NULL DEFAULT 0 COMMENT '0=解锁 1=闭锁',
    `engine_status`   TINYINT      NOT NULL DEFAULT 0 COMMENT '0=熄火 1=运行',
    `door_status`     TINYINT      NOT NULL DEFAULT 0 COMMENT '位掩码: bit0-3=四门 bit4=后备箱',
    `window_status`   TINYINT      NOT NULL DEFAULT 0 COMMENT '位掩码: bit0-3=四窗',
    `battery_pct`     TINYINT      DEFAULT NULL COMMENT '电量百分比',
    `interior_temp`   SMALLINT     DEFAULT NULL COMMENT '车内温度(0.1°C)',
    `odometer_km`     INT          DEFAULT NULL COMMENT '里程(km)',
    `alarm_status`    TINYINT      NOT NULL DEFAULT 0 COMMENT '0=无报警',
    `latitude`        DECIMAL(9,6) DEFAULT NULL COMMENT '纬度',
    `longitude`       DECIMAL(9,6) DEFAULT NULL COMMENT '经度',
    `reported_at`     DATETIME(3)  NOT NULL COMMENT '车端上报时间',
    `created_at`      DATETIME(3)  NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    PRIMARY KEY (`id`),
    KEY `idx_vehicle_reported` (`vehicle_id`, `reported_at` DESC),
    KEY `idx_vehicle_id` (`vehicle_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='车辆状态快照表 (按vehicle_id分表)';

CREATE TABLE `dk_control_command` (
    `id`              BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `cmd_id`          VARCHAR(64)  NOT NULL COMMENT '指令ID (UUID v7)',
    `vehicle_id`      BIGINT UNSIGNED NOT NULL,
    `user_id`         BIGINT UNSIGNED NOT NULL,
    `device_id`       BIGINT UNSIGNED NOT NULL,
    `key_id`          VARCHAR(64)  NOT NULL COMMENT '使用的密钥ID',
    `action`          VARCHAR(32)  NOT NULL COMMENT '动作: unlock/lock/engine_on/engine_off/trunk/window/climate/find/horn',
    `params`          JSON         DEFAULT NULL COMMENT '动作参数 {"window":"front_left","direction":"down"}',
    `source`          TINYINT      NOT NULL COMMENT '1=NFC 2=BLE 3=UWB 4=Remote(云端) 5=Edge(边缘)',
    `status`          TINYINT      NOT NULL DEFAULT 1 COMMENT '1=Pending 2=Sent 3=Delivered 4=Executed 5=Failed 6=Timeout',
    `result_code`     SMALLINT     DEFAULT NULL COMMENT '车端返回结果码',
    `sent_at`         DATETIME(3)  DEFAULT NULL,
    `delivered_at`    DATETIME(3)  DEFAULT NULL,
    `executed_at`     DATETIME(3)  DEFAULT NULL,
    `created_at`      DATETIME(3)  NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_cmd_id` (`cmd_id`),
    KEY `idx_vehicle_status` (`vehicle_id`, `status`),
    KEY `idx_user_id` (`user_id`),
    KEY `idx_created_at` (`created_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='控制指令记录表';

-- ============================================================
-- 6. HUB协议适配
-- ============================================================

CREATE TABLE `dk_hub_adapter` (
    `id`              BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `adapter_id`      VARCHAR(64)  NOT NULL COMMENT '适配器ID',
    `vendor`          VARCHAR(32)  NOT NULL COMMENT '手机厂商: apple/samsung/xiaomi/oppo/vivo/huawei',
    `protocol`        VARCHAR(16)  NOT NULL COMMENT '协议: ccc_dk3/iccoa_dk30/iccoa_dk40/icce',
    `api_endpoint`    VARCHAR(256) NOT NULL COMMENT '厂商API端点',
    `api_version`     VARCHAR(16)  NOT NULL COMMENT '厂商API版本',
    `auth_type`       VARCHAR(32)  NOT NULL COMMENT '认证方式: oauth2/jwt/mtls/apikey',
    `auth_config`     BLOB         DEFAULT NULL COMMENT '认证配置 (加密存储)',
    `rate_limit`      INT          NOT NULL DEFAULT 1000 COMMENT '速率限制 (次/分)',
    `timeout_ms`      INT          NOT NULL DEFAULT 5000 COMMENT '超时时间(ms)',
    `retry_policy`    JSON         DEFAULT NULL COMMENT '重试策略 {"max_retries":3,"backoff_ms":1000}',
    `circuit_breaker` JSON         DEFAULT NULL COMMENT '熔断配置 {"threshold":5,"reset_ms":30000}',
    `status`          TINYINT      NOT NULL DEFAULT 1 COMMENT '1=Active 2=Disabled 3=Maintenance',
    `last_healthcheck`DATETIME(3)  DEFAULT NULL,
    `created_at`      DATETIME(3)  NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    `updated_at`      DATETIME(3)  NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_adapter_vendor_proto` (`vendor`, `protocol`),
    KEY `idx_vendor` (`vendor`),
    KEY `idx_protocol` (`protocol`),
    KEY `idx_status` (`status`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='HUB协议适配器配置表';

CREATE TABLE `dk_hub_route` (
    `id`              BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `route_id`        VARCHAR(64)  NOT NULL COMMENT '路由ID',
    `vendor`          VARCHAR(32)  NOT NULL COMMENT '手机厂商',
    `operation`       VARCHAR(64)  NOT NULL COMMENT '操作: bind/unbind/auth/share/revoke/notify',
    `adapter_id`      BIGINT UNSIGNED NOT NULL COMMENT '适配器ID',
    `priority`        TINYINT      NOT NULL DEFAULT 128 COMMENT '路由优先级 0-255',
    `enabled`         TINYINT      NOT NULL DEFAULT 1,
    `created_at`      DATETIME(3)  NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_route` (`vendor`, `operation`),
    KEY `idx_adapter` (`adapter_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='HUB路由规则表';

-- ============================================================
-- 7. 审计日志 (不可篡改)
-- ============================================================

CREATE TABLE `dk_audit_log` (
    `id`              BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `trace_id`        VARCHAR(64)  NOT NULL COMMENT '链路追踪ID',
    `span_id`         VARCHAR(64)  NOT NULL COMMENT '当前Span ID',
    `operation`       VARCHAR(64)  NOT NULL COMMENT '操作类型',
    `user_id`         BIGINT UNSIGNED DEFAULT NULL,
    `device_id`       BIGINT UNSIGNED DEFAULT NULL,
    `vehicle_id`      BIGINT UNSIGNED DEFAULT NULL,
    `key_id`          VARCHAR(64)  DEFAULT NULL,
    `protocol`        VARCHAR(16)  DEFAULT NULL,
    `vendor`          VARCHAR(32)  DEFAULT NULL,
    `request_hash`    VARCHAR(128) DEFAULT NULL COMMENT '请求摘要 (SHA256)',
    `response_hash`   VARCHAR(128) DEFAULT NULL COMMENT '响应摘要 (SHA256)',
    `result`          TINYINT      NOT NULL COMMENT '1=Success 2=Failure 3=Error',
    `error_code`      VARCHAR(32)  DEFAULT NULL,
    `error_msg`       VARCHAR(512) DEFAULT NULL,
    `source_ip`       VARCHAR(45)  DEFAULT NULL COMMENT '来源IP',
    `user_agent`      VARCHAR(256) DEFAULT NULL,
    `latency_ms`      INT          DEFAULT NULL COMMENT '处理耗时(ms)',
    `created_at`      DATETIME(3)  NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    PRIMARY KEY (`id`),
    KEY `idx_trace_id` (`trace_id`),
    KEY `idx_operation` (`operation`),
    KEY `idx_user_id` (`user_id`),
    KEY `idx_vehicle_id` (`vehicle_id`),
    KEY `idx_key_id` (`key_id`),
    KEY `idx_created_at` (`created_at`),
    KEY `idx_result` (`result`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='审计日志表 (只增不改)';

-- ============================================================
-- 8. OTA & 配置
-- ============================================================

CREATE TABLE `dk_ota_campaign` (
    `id`              BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `campaign_id`     VARCHAR(64)  NOT NULL COMMENT 'OTA活动ID',
    `target_type`     TINYINT      NOT NULL COMMENT '1=全量 2=按车型 3=按VIN列表 4=按固件版本',
    `target_filter`   JSON         DEFAULT NULL COMMENT '筛选条件',
    `fw_version`      VARCHAR(64)  NOT NULL COMMENT '目标固件版本',
    `fw_url`          VARCHAR(512) NOT NULL COMMENT '固件下载URL',
    `fw_hash`         VARCHAR(128) NOT NULL COMMENT '固件SHA256校验',
    `fw_size`         INT          NOT NULL COMMENT '固件大小(bytes)',
    `rollout_pct`     TINYINT      NOT NULL DEFAULT 100 COMMENT '灰度百分比',
    `status`          TINYINT      NOT NULL DEFAULT 1 COMMENT '1=Draft 2=Running 3=Paused 4=Completed 5=Rollback',
    `created_by`      BIGINT UNSIGNED DEFAULT NULL,
    `created_at`      DATETIME(3)  NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    `updated_at`      DATETIME(3)  NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_campaign_id` (`campaign_id`),
    KEY `idx_status` (`status`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='OTA升级活动表';

CREATE TABLE `dk_ota_progress` (
    `id`              BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `campaign_id`     VARCHAR(64)  NOT NULL,
    `vehicle_id`      BIGINT UNSIGNED NOT NULL,
    `current_version` VARCHAR(64)  DEFAULT NULL,
    `target_version`  VARCHAR(64)  NOT NULL,
    `status`          TINYINT      NOT NULL DEFAULT 1 COMMENT '1=Pending 2=Downloading 3=Installing 4=Success 5=Failed 6=Rollback',
    `started_at`      DATETIME(3)  DEFAULT NULL,
    `completed_at`    DATETIME(3)  DEFAULT NULL,
    `error_code`      VARCHAR(32)  DEFAULT NULL,
    `created_at`      DATETIME(3)  NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    PRIMARY KEY (`id`),
    KEY `idx_campaign_vehicle` (`campaign_id`, `vehicle_id`),
    KEY `idx_vehicle_id` (`vehicle_id`),
    KEY `idx_status` (`status`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='OTA升级进度表';

-- ============================================================
-- 9. 分库分表策略 (预留)
-- ============================================================
-- 按 vehicle_id 分表的热点表: dk_vehicle_status, dk_control_command, dk_audit_log
-- 分表规则: table_{vehicle_id % 16}
-- 跨分片查询通过 HUB 统一路由

SET FOREIGN_KEY_CHECKS = 1;
