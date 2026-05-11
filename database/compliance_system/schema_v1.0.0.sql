-- ============================================================
-- yuleDKCS - Digital Key Compliance System Database Schema v1.0.0
-- 数字钥匙合规检测系统数据库设计
-- 支持: CCC / ICCOA / ICCE 协议合规性测试
-- 数据库: PostgreSQL 15+
-- ============================================================

CREATE EXTENSION IF NOT EXISTS "uuid-ossp";
CREATE EXTENSION IF NOT EXISTS "pgcrypto";

-- ============================================================
-- 第一部分: 系统管理
-- ============================================================

CREATE TABLE users (
    user_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    username VARCHAR(50) UNIQUE NOT NULL,
    email VARCHAR(254) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    full_name VARCHAR(100),
    role VARCHAR(20) DEFAULT 'tester' CHECK (role IN ('admin', 'tester', 'auditor', 'viewer')),
    organization VARCHAR(100),
    department VARCHAR(50),
    is_active BOOLEAN DEFAULT TRUE,
    last_login_at TIMESTAMPTZ,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE INDEX idx_users_email ON users(email);
CREATE INDEX idx_users_role ON users(role);
CREATE INDEX idx_users_org ON users(organization);

-- 系统配置表
CREATE TABLE system_configs (
    config_key VARCHAR(100) PRIMARY KEY,
    config_value TEXT NOT NULL,
    config_type VARCHAR(20) DEFAULT 'string' CHECK (config_type IN ('string', 'number', 'boolean', 'json')),
    description TEXT,
    updated_by UUID REFERENCES users(user_id),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);

-- 插件/模块管理
CREATE TABLE compliance_modules (
    module_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    module_name VARCHAR(100) NOT NULL,
    module_code VARCHAR(50) UNIQUE NOT NULL,
    protocol VARCHAR(20) NOT NULL CHECK (protocol IN ('CCC', 'ICCOA', 'ICCE', 'COMMON')),
    module_type VARCHAR(30) CHECK (module_type IN ('security', 'performance', 'functional', 'interoperability')),
    version VARCHAR(20) NOT NULL,
    specification_version VARCHAR(20),
    description TEXT,
    is_active BOOLEAN DEFAULT TRUE,
    execution_order INTEGER DEFAULT 0,
    dependencies JSONB DEFAULT '[]'::jsonb,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE INDEX idx_modules_protocol ON compliance_modules(protocol);
CREATE INDEX idx_modules_type ON compliance_modules(module_type);
CREATE INDEX idx_modules_active ON compliance_modules(is_active);

-- ============================================================
-- 第二部分: 测试对象管理 (DUT - Device Under Test)
-- ============================================================

CREATE TABLE test_targets (
    target_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    
    -- 基本信息
    target_name VARCHAR(100) NOT NULL,
    target_type VARCHAR(30) NOT NULL CHECK (target_type IN ('mobile_device', 'vehicle_ecu', 'key_fob', 'se_chip', 'backend_server')),
    
    -- 协议支持
    supported_protocols JSONB NOT NULL DEFAULT '[]'::jsonb,
    
    -- 厂商信息
    manufacturer VARCHAR(100),
    model VARCHAR(100),
    hardware_version VARCHAR(50),
    firmware_version VARCHAR(50),
    software_version VARCHAR(50),
    
    -- 识别信息
    serial_number VARCHAR(100),
    vin VARCHAR(17),
    device_id BYTEA,
    se_id BYTEA,
    
    -- 状态
    status VARCHAR(20) DEFAULT 'active' CHECK (status IN ('active', 'inactive', 'testing', 'retired')),
    
    -- 归档
    owner_user_id UUID REFERENCES users(user_id),
    project_name VARCHAR(100),
    description TEXT,
    
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE INDEX idx_targets_type ON test_targets(target_type);
CREATE INDEX idx_targets_protocols ON test_targets USING GIN(supported_protocols);
CREATE INDEX idx_targets_status ON test_targets(status);
CREATE INDEX idx_targets_owner ON test_targets(owner_user_id);
CREATE INDEX idx_targets_vin ON test_targets(vin);

-- 测试对象证书管理
CREATE TABLE target_certificates (
    cert_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    target_id UUID NOT NULL REFERENCES test_targets(target_id) ON DELETE CASCADE,
    
    cert_type VARCHAR(30) NOT NULL CHECK (cert_type IN ('root', 'intermediate', 'oem', 'vehicle', 'device', 'se', 'custom')),
    cert_format VARCHAR(20) DEFAULT 'x509_der' CHECK (cert_format IN ('x509_der', 'x509_pem', 'raw')),
    cert_data BYTEA NOT NULL,
    cert_hash BYTEA,
    
    -- 解析字段
    serial_number VARCHAR(64),
    issuer_dn TEXT,
    subject_dn TEXT,
    subject_key_id BYTEA,
    valid_from TIMESTAMPTZ,
    valid_until TIMESTAMPTZ,
    
    -- 验证结果
    validation_status VARCHAR(20) DEFAULT 'pending' CHECK (validation_status IN ('pending', 'valid', 'invalid', 'expired', 'revoked')),
    validation_details JSONB,
    
    uploaded_by UUID REFERENCES users(user_id),
    uploaded_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE INDEX idx_target_certs_target ON target_certificates(target_id);
CREATE INDEX idx_target_certs_type ON target_certificates(cert_type);
CREATE INDEX idx_target_certs_status ON target_certificates(validation_status);

-- ============================================================
-- 第三部分: 合规规则库 (核心)
-- ============================================================

CREATE TABLE compliance_rules (
    rule_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    
    -- 规则标识
    rule_code VARCHAR(50) UNIQUE NOT NULL,
    rule_name VARCHAR(200) NOT NULL,
    
    -- 协议归属
    protocol VARCHAR(20) NOT NULL CHECK (protocol IN ('CCC', 'ICCOA', 'ICCE', 'COMMON')),
    specification_version VARCHAR(20) NOT NULL,
    
    -- 类别
    category VARCHAR(50) NOT NULL CHECK (category IN (
        'cryptography', 'protocol', 'security', 'performance', 
        'interoperability', 'certificate', 'key_management', 'communication'
    )),
    
    -- 强制性
    severity VARCHAR(20) NOT NULL CHECK (severity IN ('mandatory', 'recommended', 'optional')),
    
    -- 规则内容
    description TEXT NOT NULL,
    specification_reference TEXT, -- 规范文档引用 (如 "CCC DK 3.0 Section 4.2.1")
    
    -- 验证逻辑
    validation_logic JSONB NOT NULL, -- 验证逻辑定义
    expected_result JSONB, -- 期望结果
    
    -- 状态
    is_active BOOLEAN DEFAULT TRUE,
    effective_date DATE,
    expiration_date DATE,
    
    -- 版本控制
    version INTEGER DEFAULT 1,
    parent_rule_id UUID REFERENCES compliance_rules(rule_id), -- 规则演进关系
    
    created_by UUID REFERENCES users(user_id),
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE INDEX idx_rules_protocol ON compliance_rules(protocol);
CREATE INDEX idx_rules_category ON compliance_rules(category);
CREATE INDEX idx_rules_severity ON compliance_rules(severity);
CREATE INDEX idx_rules_active ON compliance_rules(is_active);
CREATE INDEX idx_rules_spec ON compliance_rules(specification_version);

-- 规则测试点映射 (多对多)
CREATE TABLE rule_test_points (
    mapping_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    rule_id UUID NOT NULL REFERENCES compliance_rules(rule_id) ON DELETE CASCADE,
    test_point_code VARCHAR(50) NOT NULL,
    test_point_name VARCHAR(200) NOT NULL,
    test_type VARCHAR(30) CHECK (test_type IN ('automated', 'manual', 'semi_automated')),
    test_data_requirements JSONB,
    execution_script TEXT, -- 自动化测试脚本路径
    priority INTEGER DEFAULT 100,
    UNIQUE(rule_id, test_point_code)
);

CREATE INDEX idx_rule_points_rule ON rule_test_points(rule_id);
CREATE INDEX idx_rule_points_type ON rule_test_points(test_type);

-- ============================================================
-- 第四部分: 测试套件/任务管理
-- ============================================================

CREATE TABLE test_suites (
    suite_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    suite_name VARCHAR(100) NOT NULL,
    suite_code VARCHAR(50) UNIQUE NOT NULL,
    
    -- 测试范围
    target_protocols JSONB NOT NULL, -- ["CCC", "ICCOA"]
    target_categories JSONB, -- ["security", "cryptography"]
    target_severities JSONB, -- ["mandatory", "recommended"]
    
    -- 测试对象类型
    applicable_target_types JSONB DEFAULT '["mobile_device", "vehicle_ecu"]'::jsonb,
    
    -- 规则包含
    included_rules JSONB DEFAULT '[]'::jsonb, -- 明确包含的规则ID列表
    excluded_rules JSONB DEFAULT '[]'::jsonb, -- 排除的规则
    
    description TEXT,
    is_template BOOLEAN DEFAULT FALSE, -- 是否为模板
    is_active BOOLEAN DEFAULT TRUE,
    
    created_by UUID REFERENCES users(user_id),
    created_at TIMESTAMPTZ DEFAULT NOW()
);

-- 测试任务 (执行实例)
CREATE TABLE test_tasks (
    task_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    
    -- 任务标识
    task_name VARCHAR(200) NOT NULL,
    task_code VARCHAR(50) UNIQUE NOT NULL,
    
    -- 关聓信息
    suite_id UUID REFERENCES test_suites(suite_id),
    target_id UUID NOT NULL REFERENCES test_targets(target_id),
    executor_id UUID REFERENCES users(user_id),
    
    -- 协议信息
    target_protocol VARCHAR(20) NOT NULL,
    specification_version VARCHAR(20),
    
    -- 状态
    status VARCHAR(30) DEFAULT 'pending' CHECK (status IN (
        'pending', 'preparing', 'running', 'paused', 'completed', 'failed', 'cancelled'
    )),
    
    -- 进度
    total_rules INTEGER DEFAULT 0,
    completed_rules INTEGER DEFAULT 0,
    passed_rules INTEGER DEFAULT 0,
    failed_rules INTEGER DEFAULT 0,
    warning_rules INTEGER DEFAULT 0,
    
    -- 时间
    scheduled_at TIMESTAMPTZ,
    started_at TIMESTAMPTZ,
    completed_at TIMESTAMPTZ,
    estimated_duration INTEGER, -- 预估耗时(秒)
    actual_duration INTEGER, -- 实际耗时(秒)
    
    -- 配置
    config JSONB DEFAULT '{}'::jsonb, -- 测试配置参数
    environment_info JSONB, -- 测试环境信息
    
    -- 结果
    overall_result VARCHAR(20) CHECK (overall_result IN ('pass', 'fail', 'warning', 'inconclusive')),
    summary_report JSONB,
    
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE INDEX idx_tasks_suite ON test_tasks(suite_id);
CREATE INDEX idx_tasks_target ON test_tasks(target_id);
CREATE INDEX idx_tasks_executor ON test_tasks(executor_id);
CREATE INDEX idx_tasks_status ON test_tasks(status);
CREATE INDEX idx_tasks_protocol ON test_tasks(target_protocol);
CREATE INDEX idx_tasks_created ON test_tasks(created_at DESC);

-- ============================================================
-- 第五部分: 测试结果详情 (核心)
-- ============================================================

CREATE TABLE test_results (
    result_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    
    -- 关聓信息
    task_id UUID NOT NULL REFERENCES test_tasks(task_id) ON DELETE CASCADE,
    rule_id UUID NOT NULL REFERENCES compliance_rules(rule_id),
    test_point_id UUID REFERENCES rule_test_points(mapping_id),
    
    -- 测试执行信息
    execution_sequence INTEGER NOT NULL, -- 执行顺序
    executed_by UUID REFERENCES users(user_id), -- 执行人，自动化为null
    
    -- 结果
    result_status VARCHAR(20) NOT NULL CHECK (result_status IN ('pass', 'fail', 'warning', 'skip', 'error', 'inconclusive')),
    result_details JSONB, -- 详细结果数据
    
    -- 实际值 vs 期望值
    expected_value JSONB,
    actual_value JSONB,
    deviation_description TEXT,
    
    -- 证据
    evidence_files JSONB DEFAULT '[]'::jsonb, -- 证据文件路径列表
    log_data TEXT, -- 日志数据
    packet_captures JSONB, -- 抓包文件
    
    -- 审核
    reviewed_by UUID REFERENCES users(user_id),
    reviewed_at TIMESTAMPTZ,
    review_comment TEXT,
    
    -- 性能数据
    execution_time_ms INTEGER, -- 执行耗时
    memory_usage_mb INTEGER,
    
    created_at TIMESTAMPTZ DEFAULT NOW()
) PARTITION BY RANGE (created_at);

-- 创建分区表 (按月)
CREATE TABLE test_results_2024_01 PARTITION OF test_results
    FOR VALUES FROM ('2024-01-01') TO ('2024-02-01');
CREATE TABLE test_results_2024_02 PARTITION OF test_results
    FOR VALUES FROM ('2024-02-01') TO ('2024-03-01');
CREATE TABLE test_results_2024_03 PARTITION OF test_results
    FOR VALUES FROM ('2024-03-01') TO ('2024-04-01');

CREATE INDEX idx_results_task ON test_results(task_id);
CREATE INDEX idx_results_rule ON test_results(rule_id);
CREATE INDEX idx_results_status ON test_results(result_status);
CREATE INDEX idx_results_created ON test_results(created_at DESC);

-- ============================================================
-- 第六部分: 合规报告生成
-- ============================================================

CREATE TABLE compliance_reports (
    report_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    
    -- 报告基本信息
    report_title VARCHAR(200) NOT NULL,
    report_code VARCHAR(50) UNIQUE NOT NULL,
    report_type VARCHAR(30) CHECK (report_type IN ('full', 'summary', 'detailed', 'certificate')),
    
    -- 关联任务
    task_id UUID REFERENCES test_tasks(task_id),
    
    -- 报告范围
    target_id UUID REFERENCES test_targets(target_id),
    protocol VARCHAR(20),
    specification_version VARCHAR(20),
    
    -- 结论
    overall_compliance VARCHAR(20) CHECK (overall_compliance IN ('compliant', 'non_compliant', 'partially_compliant', 'inconclusive')),
    compliance_percentage DECIMAL(5,2), -- 合规百分比
    
    -- 统计
    total_rules_tested INTEGER,
    passed_rules INTEGER,
    failed_rules INTEGER,
    warning_rules INTEGER,
    
    -- 报告文件
    report_file_path VARCHAR(500),
    report_format VARCHAR(20) DEFAULT 'pdf' CHECK (report_format IN ('pdf', 'html', 'xml', 'json')),
    file_size_bytes INTEGER,
    
    -- 签名/验证
    generated_by UUID REFERENCES users(user_id),
    generated_at TIMESTAMPTZ DEFAULT NOW(),
    digital_signature BYTEA, -- 数字签名
    signature_valid_until TIMESTAMPTZ,
    
    -- 审批
    approved_by UUID REFERENCES users(user_id),
    approved_at TIMESTAMPTZ,
    approval_status VARCHAR(20) DEFAULT 'pending' CHECK (approval_status IN ('pending', 'approved', 'rejected')),
    
    -- 归档
    is_archived BOOLEAN DEFAULT FALSE,
    archived_at TIMESTAMPTZ
);

CREATE INDEX idx_reports_task ON compliance_reports(task_id);
CREATE INDEX idx_reports_target ON compliance_reports(target_id);
CREATE INDEX idx_reports_compliance ON compliance_reports(overall_compliance);
CREATE INDEX idx_reports_generated ON compliance_reports(generated_at DESC);

-- 报告章节详情
CREATE TABLE report_sections (
    section_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    report_id UUID NOT NULL REFERENCES compliance_reports(report_id) ON DELETE CASCADE,
    
    section_order INTEGER NOT NULL,
    section_title VARCHAR(200) NOT NULL,
    section_type VARCHAR(30) CHECK (section_type IN ('summary', 'detailed_results', 'findings', 'recommendations', 'appendix')),
    
    content TEXT,
    included_rules JSONB, -- 包含的规则ID
    section_data JSONB, -- 结构化数据
    
    created_at TIMESTAMPTZ DEFAULT NOW()
);

-- ============================================================
-- 第七部分: 实时测试数据 (通信抓包)
-- ============================================================

CREATE TABLE packet_captures (
    capture_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    
    -- 关联信息
    task_id UUID REFERENCES test_tasks(task_id),
    result_id UUID REFERENCES test_results(result_id),
    
    -- 抓包信息
    capture_name VARCHAR(100),
    protocol_layer VARCHAR(30), -- ble/nfc/uwb/mqtt/https
    
    -- 时间戳
    capture_start_at TIMESTAMPTZ,
    capture_end_at TIMESTAMPTZ,
    duration_ms INTEGER,
    
    -- 文件信息
    file_path VARCHAR(500),
    file_format VARCHAR(20) DEFAULT 'pcapng', -- pcap/pcapng/json
    file_size_bytes INTEGER,
    packet_count INTEGER,
    
    -- 元数据
    metadata JSONB, -- 通信参数、频率等
    
    captured_by UUID REFERENCES users(user_id),
    captured_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE INDEX idx_captures_task ON packet_captures(task_id);
CREATE INDEX idx_captures_result ON packet_captures(result_id);

-- 通信流水线
CREATE TABLE communication_flows (
    flow_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    capture_id UUID REFERENCES packet_captures(capture_id),
    
    flow_sequence INTEGER,
    timestamp TIMESTAMPTZ,
    
    -- 端点信息
    source_type VARCHAR(20) CHECK (source_type IN ('mobile', 'vehicle', 'se', 'server', 'relay')),
    source_id VARCHAR(100),
    destination_type VARCHAR(20),
    destination_id VARCHAR(100),
    
    -- 协议信息
    protocol VARCHAR(20),
    message_type VARCHAR(50),
    command_code VARCHAR(20),
    
    -- 数据
    payload_size INTEGER,
    payload_hash BYTEA,
    parsed_data JSONB, -- 解析后的数据
    
    -- 结果
    response_time_ms INTEGER,
    result_status VARCHAR(20) -- success/timeout/error
);

-- ============================================================
-- 第八部分: 审计日志
-- ============================================================

CREATE TABLE audit_logs (
    log_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    
    -- 操作信息
    action_type VARCHAR(50) NOT NULL, -- create/update/delete/execute/login
    action_target VARCHAR(50), -- 操作对象类型
    target_id UUID, -- 对象ID
    
    -- 执行人
    user_id UUID REFERENCES users(user_id),
    user_role VARCHAR(20),
    
    -- 详情
    action_details JSONB,
    old_values JSONB,
    new_values JSONB,
    
    -- 上下文
    ip_address INET,
    user_agent TEXT,
    session_id VARCHAR(100),
    
    -- 时间
    executed_at TIMESTAMPTZ DEFAULT NOW(),
    execution_time_ms INTEGER
) PARTITION BY RANGE (executed_at);

-- 分区表
CREATE TABLE audit_logs_2024_01 PARTITION OF audit_logs
    FOR VALUES FROM ('2024-01-01') TO ('2024-02-01');

CREATE INDEX idx_audit_user ON audit_logs(user_id);
CREATE INDEX idx_audit_action ON audit_logs(action_type);
CREATE INDEX idx_audit_target ON audit_logs(action_target);
CREATE INDEX idx_audit_time ON audit_logs(executed_at DESC);

-- ============================================================
-- 触发器函数
-- ============================================================

CREATE OR REPLACE FUNCTION update_updated_at_column()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ language 'plpgsql';

-- 应用触发器
CREATE TRIGGER update_users_updated_at BEFORE UPDATE ON users
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

CREATE TRIGGER update_test_targets_updated_at BEFORE UPDATE ON test_targets
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

CREATE TRIGGER update_compliance_rules_updated_at BEFORE UPDATE ON compliance_rules
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

CREATE TRIGGER update_test_tasks_updated_at BEFORE UPDATE ON test_tasks
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

-- 任务状态变更触发器
CREATE OR REPLACE FUNCTION update_task_progress()
RETURNS TRIGGER AS $$
BEGIN
    IF NEW.status = 'completed' THEN
        NEW.completed_at = NOW();
        IF NEW.started_at IS NOT NULL THEN
            NEW.actual_duration = EXTRACT(EPOCH FROM (NEW.completed_at - NEW.started_at))::INTEGER;
        END IF;
    ELSIF NEW.status = 'running' AND OLD.status != 'running' THEN
        NEW.started_at = NOW();
    END IF;
    RETURN NEW;
END;
$$ language 'plpgsql';

CREATE TRIGGER task_status_change BEFORE UPDATE ON test_tasks
    FOR EACH ROW EXECUTE FUNCTION update_task_progress();

-- ============================================================
-- 视图
-- ============================================================

-- 合规概览视图
CREATE VIEW compliance_overview AS
SELECT 
    c.protocol,
    c.specification_version,
    COUNT(*) as total_rules,
    COUNT(*) FILTER (WHERE c.severity = 'mandatory') as mandatory_rules,
    COUNT(*) FILTER (WHERE c.severity = 'recommended') as recommended_rules,
    COUNT(*) FILTER (WHERE c.is_active = TRUE) as active_rules
FROM compliance_rules c
GROUP BY c.protocol, c.specification_version;

-- 测试任务概览
CREATE VIEW task_overview AS
SELECT 
    t.task_id,
    t.task_name,
    t.status,
    t.target_protocol,
    tt.target_name,
    u.username as executor,
    t.overall_result,
    t.completed_rules || '/' || t.total_rules as progress,
    CASE 
        WHEN t.total_rules > 0 THEN 
            ROUND((t.completed_rules::NUMERIC / t.total_rules) * 100, 2)
        ELSE 0
    END as progress_percentage
FROM test_tasks t
JOIN test_targets tt ON t.target_id = tt.target_id
LEFT JOIN users u ON t.executor_id = u.user_id;

-- 最新测试结果视图
CREATE VIEW latest_results AS
SELECT DISTINCT ON (task_id, rule_id)
    r.*,
    c.rule_code,
    c.rule_name,
    c.severity
FROM test_results r
JOIN compliance_rules c ON r.rule_id = c.rule_id
ORDER BY task_id, rule_id, r.created_at DESC;

-- Schema版本
CREATE TABLE schema_version (
    version VARCHAR(20) PRIMARY KEY,
    applied_at TIMESTAMPTZ DEFAULT NOW(),
    description TEXT
);

INSERT INTO schema_version (version, description) VALUES 
    ('1.0.0', 'Initial DKCS schema for CCC/ICCOA/ICCE compliance testing');

-- 初始化数据
INSERT INTO system_configs (config_key, config_value, config_type, description) VALUES
    ('max_concurrent_tasks', '5', 'number', '最大并发测试任务数'),
    ('default_test_timeout', '300', 'number', '默认测试超时时间(秒)'),
    ('packet_capture_enabled', 'true', 'boolean', '是否启用抓包功能'),
    ('supported_protocols', '["CCC", "ICCOA", "ICCE"]', 'json', '支持的协议列表');
