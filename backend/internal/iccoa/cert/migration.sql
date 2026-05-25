-- ICCOA DK 4.0 证书管理数据库迁移脚本
-- 符合 ICCOA/T 002-2024 标准

-- 创建证书表
CREATE TABLE IF NOT EXISTS iccoa_certificates (
    id VARCHAR(36) PRIMARY KEY COMMENT '证书唯一标识符(UUID)',
    type TINYINT NOT NULL COMMENT '证书类型: 1=CA, 2=车, 3=车主钥匙, 4=中间分享, 5=好友钥匙, 11=好友V2',
    mode TINYINT NOT NULL DEFAULT 0 COMMENT '证书模式: 0=CA模式, 1=非CA模式',
    vehicle_oem_id VARCHAR(10) COMMENT '车企唯一标识符(2字节十六进制)',
    vehicle_id VARCHAR(50) COMMENT '车辆唯一标识符(16字节十六进制)',
    key_id VARCHAR(50) COMMENT '钥匙唯一标识符(16字节十六进制)',
    user_id BIGINT UNSIGNED COMMENT '用户ID(用于车主/好友钥匙)',
    serial_number VARCHAR(100) NOT NULL COMMENT '证书序列号',
    subject VARCHAR(500) NOT NULL COMMENT '证书主题(Subject)',
    issuer VARCHAR(500) NOT NULL COMMENT '证书颁发者(Issuer)',
    not_before DATETIME NOT NULL COMMENT '有效期开始时间',
    not_after DATETIME NOT NULL COMMENT '有效期结束时间',
    der_data TEXT NOT NULL COMMENT 'DER 格式证书数据(Base64编码)',
    pem_data TEXT COMMENT 'PEM 格式证书数据',
    parent_cert_id VARCHAR(36) COMMENT '父证书ID(用于构建证书链)',
    status VARCHAR(20) NOT NULL DEFAULT 'active' COMMENT '证书状态: active, revoked, expired',
    revoked_at DATETIME COMMENT '撤销时间',
    revoke_reason VARCHAR(200) COMMENT '撤销原因',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
    
    INDEX idx_type (type),
    INDEX idx_vehicle_oem_id (vehicle_oem_id),
    INDEX idx_vehicle_id (vehicle_id),
    INDEX idx_key_id (key_id),
    INDEX idx_user_id (user_id),
    INDEX idx_status (status),
    INDEX idx_not_after (not_after),
    UNIQUE KEY uk_serial_number (serial_number)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='ICCOA 证书表';

-- 创建证书链表
CREATE TABLE IF NOT EXISTS iccoa_cert_chains (
    id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY COMMENT '自增ID',
    entity_id VARCHAR(100) NOT NULL COMMENT '实体标识(车辆ID/钥匙ID等)',
    chain_type TINYINT NOT NULL DEFAULT 0 COMMENT '链类型: 0=车, 1=车主, 2=好友',
    chain_data JSON NOT NULL COMMENT '证书链数据(JSON格式)',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
    
    UNIQUE KEY uk_entity_id (entity_id),
    INDEX idx_chain_type (chain_type)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='ICCOA 证书链表';

-- 创建证书撤销列表(CRL)表
CREATE TABLE IF NOT EXISTS iccoa_cert_revocations (
    id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY COMMENT '自增ID',
    cert_id VARCHAR(36) NOT NULL COMMENT '被撤销的证书ID',
    serial_number VARCHAR(100) NOT NULL COMMENT '证书序列号',
    issuer VARCHAR(500) NOT NULL COMMENT '颁发者',
    revoked_at DATETIME NOT NULL COMMENT '撤销时间',
    reason VARCHAR(200) COMMENT '撤销原因',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    
    INDEX idx_cert_id (cert_id),
    INDEX idx_serial_number (serial_number),
    INDEX idx_issuer (issuer),
    INDEX idx_revoked_at (revoked_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='ICCOA 证书撤销列表';

-- 创建证书请求(CSR)表
CREATE TABLE IF NOT EXISTS iccoa_cert_signing_requests (
    id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY COMMENT '自增ID',
    csr_id VARCHAR(36) NOT NULL COMMENT 'CSR 唯一标识符',
    type TINYINT NOT NULL COMMENT '证书类型',
    vehicle_oem_id VARCHAR(10) COMMENT '车企ID',
    vehicle_id VARCHAR(50) COMMENT '车辆ID',
    key_id VARCHAR(50) COMMENT '钥匙ID',
    user_id BIGINT UNSIGNED COMMENT '用户ID',
    subject VARCHAR(500) NOT NULL COMMENT '证书主题',
    public_key TEXT NOT NULL COMMENT '公钥(PEM格式)',
    csr_data TEXT NOT NULL COMMENT 'CSR 数据(PEM格式)',
    status VARCHAR(20) NOT NULL DEFAULT 'pending' COMMENT '状态: pending, signed, rejected',
    cert_id VARCHAR(36) COMMENT '签发后的证书ID',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    
    UNIQUE KEY uk_csr_id (csr_id),
    INDEX idx_type (type),
    INDEX idx_vehicle_id (vehicle_id),
    INDEX idx_key_id (key_id),
    INDEX idx_user_id (user_id),
    INDEX idx_status (status)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='ICCOA 证书签名请求表';

-- 创建密钥存储表(安全存储参考)
CREATE TABLE IF NOT EXISTS iccoa_key_pairs (
    id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY COMMENT '自增ID',
    key_id VARCHAR(50) NOT NULL COMMENT '密钥标识符',
    key_type VARCHAR(20) NOT NULL COMMENT '密钥类型: ca, vehicle, owner, mid_share, friend',
    cert_id VARCHAR(36) COMMENT '关联证书ID',
    public_key TEXT NOT NULL COMMENT '公钥(PEM格式)',
    private_key_encrypted TEXT NOT NULL COMMENT '加密的私钥',
    key_encryption_algorithm VARCHAR(50) DEFAULT 'AES-256-GCM' COMMENT '密钥加密算法',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
    
    UNIQUE KEY uk_key_id (key_id),
    INDEX idx_cert_id (cert_id),
    INDEX idx_key_type (key_type)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='ICCOA 密钥存储表';

-- 创建触发器: 自动将撤销的证书添加到撤销列表
DELIMITER //
CREATE TRIGGER IF NOT EXISTS trg_cert_revocation
AFTER UPDATE ON iccoa_certificates
FOR EACH ROW
BEGIN
    IF OLD.status != 'revoked' AND NEW.status = 'revoked' THEN
        INSERT INTO iccoa_cert_revocations (cert_id, serial_number, issuer, revoked_at, reason)
        VALUES (NEW.id, NEW.serial_number, NEW.issuer, NEW.revoked_at, NEW.revoke_reason);
    END IF;
END//
DELIMITER ;

-- 创建触发器: 更新 updated_at 字段
DELIMITER //
CREATE TRIGGER IF NOT EXISTS trg_cert_updated_at
BEFORE UPDATE ON iccoa_certificates
FOR EACH ROW
BEGIN
    SET NEW.updated_at = CURRENT_TIMESTAMP;
END//
DELIMITER ;

-- 初始化数据: 添加索引优化
ALTER TABLE iccoa_certificates 
ADD INDEX IF NOT EXISTS idx_vehicle_oem_type (vehicle_oem_id, type),
ADD INDEX IF NOT EXISTS idx_user_type (user_id, type);

-- 添加表注释
ALTER TABLE iccoa_certificates COMMENT = 'ICCOA DK 4.0 证书表 - 符合 ICCOA/T 002-2024 标准';
ALTER TABLE iccoa_cert_chains COMMENT = 'ICCOA DK 4.0 证书链表 - 存储证书链关系';
ALTER TABLE iccoa_cert_revocations COMMENT = 'ICCOA DK 4.0 证书撤销列表(CRL)';
ALTER TABLE iccoa_cert_signing_requests COMMENT = 'ICCOA DK 4.0 证书签名请求(CSR)';
ALTER TABLE iccoa_key_pairs COMMENT = 'ICCOA DK 4.0 密钥存储表(安全存储参考)';
