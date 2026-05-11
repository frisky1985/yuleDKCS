# yuleDKCS 数据库设计报告

> **项目名称**: yuleDKCS (Digital Key Compliance System)  
> **项目说明**: 数字钥匙合规检测系统  
> **数据库**: PostgreSQL 15+  
> **设计版本**: v1.0.0  
> **设计日期**: 2026-05-11

---

## 目录

1. [概述](#一概述)
2. [支持的数字钥匙协议](#二支持的数字钥匙协议)
3. [数据库架构总览](#三数据库架构总览)
4. [核心表设计说明](#四核心表设计说明)
5. [业务流程](#五业务流程)
6. [性能优化策略](#六性能优化策略)
7. [安全设计](#七安全设计)
8. [部署指南](#八部署指南)
9. [附录](#九附录)

---

## 一、概述

### 1.1 项目背景

yuleDKCS (数字钥匙合规检测系统) 是一个面向汽车数字钥匙的自动化合规检测平台。系统支持对符合CCC、ICCOA、ICCE等标准的数字钥匙进行全面的合规性测试，包括：

- 加密算法验证
- 通信协议一致性检查
- 安全机制测试
- 证书链验证
- 性能指标测试

### 1.2 数据库统计

| 指标 | 数值 |
|------|------|
| 总表数量 | 15 张 |
| 核心业务表 | 8 张 |
| 分区表 | 2 张 |
| 视图 | 3 个 |
| 索引 | 20+ 个 |
| 触发器 | 4 个 |

---

## 二、支持的数字钥匙协议

### 2.1 CCC (Car Connectivity Consortium)

**协议版本**: Digital Key 3.0

**合规维度**:
- [x] SE安全元认证 (Secure Element)
- [x] 证书链验证 (X.509)
- [x] 密钥协商协议 (Key Agreement)
- [x] 授权协议 (Authorization Protocol)
- [x] 扇区命令 (Ranging/Unlock/Lock)
- [x] 数据加密 (AES-GCM)
- [x] 时间同步 (Time Synchronization)
- [x] 证书撤销 (Certificate Revocation)

**通信接口支持**: BLE 5.0+ / NFC / UWB

### 2.2 ICCOA (智慧车联生态联盟)

**协议版本**: 数字钥匙规范 v2.0

**合规维度**:
- [x] 国产SE验证
- [x] 证书管理系统
- [x] 车机互联通信
- [x] 安全启动流程
- [x] 远程控制协议

**通信接口支持**: BLE

### 2.3 ICCE (智慧车联与电动车)

**协议版本**: 数字钥匙标准 v1.0

**合规维度**:
- [x] SE认证
- [x] 证书体系
- [x] 车机协议
- [x] 充电控制协议

**通信接口支持**: BLE

---

## 三、数据库架构总览

### 3.1 模块划分

```
┌──────────────────────────────────────────────────────────┐
│                  yuleDKCS 数据库架构                        │
├──────────────────────────────────────────────────────────┤
│                                                              │
│  ┌─────────────────────────────────────────────────────────┐          │
│  │        第一部分: 系统管理模块 (3表)                   │          │
│  ├─────────────────────────────────────────────────────────┤          │
│  │  • users              - 用户管理                        │          │
│  │  • system_configs     - 系统配置                        │          │
│  │  • compliance_modules - 合规模块管理                    │          │
│  └─────────────────────────────────────────────────────────┘          │
│                                                              │
│  ┌─────────────────────────────────────────────────────────┐          │
│  │        第二部分: 合规规则库 (3表)                       │          │
│  ├─────────────────────────────────────────────────────────┤          │
│  │  • compliance_rules   - 合规规则定义 (核心)            │          │
│  │  • rule_test_points   - 规则测试点                     │          │
│  │  • test_suites        - 测试套件                     │          │
│  └─────────────────────────────────────────────────────────┘          │
│                                                              │
│  ┌─────────────────────────────────────────────────────────┐          │
│  │        第三部分: 测试执行 (4表)                       │          │
│  ├─────────────────────────────────────────────────────────┤          │
│  │  • test_targets       - 测试对象 (DUT)                │          │
│  │  • target_certificates- 证书管理                     │          │
│  │  • test_tasks         - 测试任务实例                 │          │
│  │  • test_results       - 测试结果 (分区表)          │          │
│  └─────────────────────────────────────────────────────────┘          │
│                                                              │
│  ┌─────────────────────────────────────────────────────────┐          │
│  │        第四部分: 数据采集 (2表)                       │          │
│  ├─────────────────────────────────────────────────────────┤          │
│  │  • packet_captures    - 抓包文件记录                   │          │
│  │  • communication_flows- 通信流水线                   │          │
│  └─────────────────────────────────────────────────────────┘          │
│                                                              │
│  ┌─────────────────────────────────────────────────────────┐          │
│  │        第五部分: 报告生成 (2表)                       │          │
│  ├─────────────────────────────────────────────────────────┤          │
│  │  • compliance_reports - 合规报告                     │          │
│  │  • report_sections    - 报告章节                     │          │
│  └─────────────────────────────────────────────────────────┘          │
│                                                              │
│  ┌─────────────────────────────────────────────────────────┐          │
│  │        第六部分: 审计日志 (1表)                       │          │
│  ├─────────────────────────────────────────────────────────┤          │
│  │  • audit_logs         - 操作审计日志 (分区表)        │          │
│  └─────────────────────────────────────────────────────────┘          │
│                                                              │
└─────────────────────────────────────────────────────────┘
```

### 3.2 表结构概览

| 模块 | 表名 | 说明 | 分区 |
|------|------|------|------|
| 系统管理 | users | 用户账号管理 | - |
| 系统管理 | system_configs | 系统配置参数 | - |
| 系统管理 | compliance_modules | 合规模块定义 | - |
| 合规规则 | compliance_rules | 合规规则定义 | - |
| 合规规则 | rule_test_points | 规则测试点映射 | - |
| 合规规则 | test_suites | 测试套件定义 | - |
| 测试执行 | test_targets | 测试对象 (DUT) | - |
| 测试执行 | target_certificates | 证书管理 | - |
| 测试执行 | test_tasks | 测试任务实例 | - |
| 测试执行 | test_results | 测试结果 | ✅ 挈月分区 |
| 数据采集 | packet_captures | 抓包文件记录 | - |
| 数据采集 | communication_flows | 通信流水线 | - |
| 报告生成 | compliance_reports | 合规报告 | - |
| 报告生成 | report_sections | 报告章节 | - |
| 审计日志 | audit_logs | 操作审计日志 | ✅ 挈月分区 |

---

## 四、核心表设计说明

### 4.1 compliance_rules (合规规则表)

**表说明**: 存储合规检测的核心规则定义，支持多协议版本。

**字段定义**:

| 字段名 | 数据类型 | 约束 | 默认值 | 说明 |
|--------|----------|------|--------|------|
| rule_id | UUID | PK | gen_random_uuid() | 规则唯一标识 |
| rule_code | VARCHAR(50) | UK, NOT NULL | - | 规则编号，如 "CCC-DK3-SEC-001" |
| rule_name | VARCHAR(200) | NOT NULL | - | 规则名称 |
| protocol | VARCHAR(20) | NOT NULL | - | 归属协议: CCC/ICCOA/ICCE |
| specification_version | VARCHAR(20) | NOT NULL | - | 规范版本，如 "3.0" |
| category | VARCHAR(50) | NOT NULL | - | 规则分类 |
| severity | VARCHAR(20) | NOT NULL | - | 强制性: mandatory/recommended/optional |
| description | TEXT | NOT NULL | - | 规则详细描述 |
| specification_reference | TEXT | - | - | 规范文档引用 |
| validation_logic | JSONB | NOT NULL | - | 验证逻辑配置 |
| expected_result | JSONB | - | - | 期望结果 |
| is_active | BOOLEAN | - | TRUE | 是否启用 |
| effective_date | DATE | - | - | 生效日期 |
| expiration_date | DATE | - | - | 失效日期 |
| version | INTEGER | - | 1 | 规则版本 |
| parent_rule_id | UUID | FK | - | 父规则(版本演进) |
| created_by | UUID | FK | - | 创建人 |
| created_at | TIMESTAMPTZ | - | NOW() | 创建时间 |
| updated_at | TIMESTAMPTZ | - | NOW() | 更新时间 |

**索引**: 
```sql
CREATE INDEX idx_rules_protocol ON compliance_rules(protocol);
CREATE INDEX idx_rules_category ON compliance_rules(category);
CREATE INDEX idx_rules_severity ON compliance_rules(severity);
CREATE INDEX idx_rules_active ON compliance_rules(is_active);
```

### 4.2 test_targets (测试对象表)

**表说明**: 存储被测试设备的基本信息，包括手机、车载终端、SE芯片等。

**字段定义**:

| 字段名 | 数据类型 | 约束 | 默认值 | 说明 |
|--------|----------|------|--------|------|
| target_id | UUID | PK | gen_random_uuid() | 对象ID |
| target_name | VARCHAR(100) | NOT NULL | - | 设备名称 |
| target_type | VARCHAR(30) | NOT NULL | - | 设备类型 |
| supported_protocols | JSONB | NOT NULL | [] | 支持的协议列表 |
| manufacturer | VARCHAR(100) | - | - | 制造商 |
| model | VARCHAR(100) | - | - | 型号 |
| hardware_version | VARCHAR(50) | - | - | 硬件版本 |
| firmware_version | VARCHAR(50) | - | - | 固件版本 |
| serial_number | VARCHAR(100) | - | - | 序列号 |
| vin | VARCHAR(17) | - | - | 车辆识别码 |
| device_id | BYTEA | - | - | 设备ID |
| se_id | BYTEA | - | - | SE识别码 |
| status | VARCHAR(20) | - | 'active' | 状态 |
| owner_user_id | UUID | FK | - | 负责人 |
| created_at | TIMESTAMPTZ | - | NOW() | 创建时间 |

**设备类型 (target_type)**:
- `mobile_device` - 手机设备
- `vehicle_ecu` - 车载ECU
- `key_fob` - 物理钥匙
- `se_chip` - SE芯片
- `backend_server` - 后端服务器

### 4.3 target_certificates (证书管理表)

**表说明**: 存储测试对象的X.509证书，支持自动解析和验证。

**字段定义**:

| 字段名 | 数据类型 | 约束 | 说明 |
|--------|----------|------|------|
| cert_id | UUID | PK | 证书ID |
| target_id | UUID | FK, NOT NULL | 关联对象 |
| cert_type | VARCHAR(30) | NOT NULL | 证书类型 |
| cert_format | VARCHAR(20) | DEFAULT 'x509_der' | 证书格式 |
| cert_data | BYTEA | NOT NULL | 证书原始数据 |
| cert_hash | BYTEA | - | 证书哈希 |
| serial_number | VARCHAR(64) | - | 证书序列号 |
| issuer_dn | TEXT | - | 颁发者DN |
| subject_dn | TEXT | - | 主题DN |
| subject_key_id | BYTEA | - | Subject Key Identifier |
| valid_from | TIMESTAMPTZ | - | 有效起始时间 |
| valid_until | TIMESTAMPTZ | - | 有效截止时间 |
| validation_status | VARCHAR(20) | DEFAULT 'pending' | 验证状态 |
| validation_details | JSONB | - | 验证详情 |
| uploaded_by | UUID | FK | 上传人 |
| uploaded_at | TIMESTAMPTZ | DEFAULT NOW() | 上传时间 |

**证书类型 (cert_type)**:
- `root` - 根证书
- `intermediate` - 中间证书
- `oem` - OEM证书
- `vehicle` - 车辆证书
- `device` - 设备证书
- `se` - SE证书
- `custom` - 自定义证书

### 4.4 test_tasks (测试任务表)

**表说明**: 记录每次测试执行的实例，支持状态机和进度追踪。

**字段定义**:

| 字段名 | 数据类型 | 约束 | 默认值 | 说明 |
|--------|----------|------|--------|------|
| task_id | UUID | PK | gen_random_uuid() | 任务ID |
| task_name | VARCHAR(200) | NOT NULL | - | 任务名称 |
| task_code | VARCHAR(50) | UK, NOT NULL | - | 任务编码 |
| suite_id | UUID | FK | - | 关联套件 |
| target_id | UUID | FK, NOT NULL | - | 测试对象 |
| executor_id | UUID | FK | - | 执行人 |
| target_protocol | VARCHAR(20) | NOT NULL | - | 目标协议 |
| specification_version | VARCHAR(20) | - | - | 规范版本 |
| status | VARCHAR(30) | - | 'pending' | 任务状态 |
| total_rules | INTEGER | - | 0 | 总规则数 |
| completed_rules | INTEGER | - | 0 | 已完成数 |
| passed_rules | INTEGER | - | 0 | 通过数 |
| failed_rules | INTEGER | - | 0 | 失败数 |
| scheduled_at | TIMESTAMPTZ | - | - | 计划执行时间 |
| started_at | TIMESTAMPTZ | - | - | 实际开始时间 |
| completed_at | TIMESTAMPTZ | - | - | 完成时间 |
| estimated_duration | INTEGER | - | - | 预估耗时(秒) |
| actual_duration | INTEGER | - | - | 实际耗时(秒) |
| config | JSONB | - | {} | 测试配置 |
| environment_info | JSONB | - | - | 环境信息 |
| overall_result | VARCHAR(20) | - | - | 总体结论 |
| summary_report | JSONB | - | - | 汇总报告 |
| created_at | TIMESTAMPTZ | - | NOW() | 创建时间 |
| updated_at | TIMESTAMPTZ | - | NOW() | 更新时间 |

**任务状态 (status)**:
- `pending` - 待执行
- `preparing` - 准备中
- `running` - 执行中
- `paused` - 已暂停
- `completed` - 已完成
- `failed` - 执行失败
- `cancelled` - 已取消

### 4.5 test_results (测试结果表) - 分区表

**表说明**: 存储每条规则的详细测试结果，采用按月分区策略。

**分区策略**: `PARTITION BY RANGE (created_at)`

**字段定义**:

| 字段名 | 数据类型 | 约束 | 说明 |
|--------|----------|------|------|
| result_id | UUID | PK | 结果ID |
| task_id | UUID | FK, NOT NULL | 关联任务 |
| rule_id | UUID | FK, NOT NULL | 关联规则 |
| test_point_id | UUID | FK | 关联测试点 |
| execution_sequence | INTEGER | NOT NULL | 执行顺序 |
| executed_by | UUID | FK | 执行人 |
| result_status | VARCHAR(20) | NOT NULL | 结果状态 |
| result_details | JSONB | - | 详细结果 |
| expected_value | JSONB | - | 期望值 |
| actual_value | JSONB | - | 实际值 |
| deviation_description | TEXT | - | 偏差说明 |
| evidence_files | JSONB | DEFAULT [] | 证据文件 |
| log_data | TEXT | - | 日志数据 |
| packet_captures | JSONB | - | 抓包文件 |
| reviewed_by | UUID | FK | 审核人 |
| reviewed_at | TIMESTAMPTZ | - | 审核时间 |
| review_comment | TEXT | - | 审核评论 |
| execution_time_ms | INTEGER | - | 执行耗时(毫秒) |
| memory_usage_mb | INTEGER | - | 内存使用(MB) |
| created_at | TIMESTAMPTZ | - | 创建时间 |

**结果状态 (result_status)**:
- `pass` - 通过
- `fail` - 失败
- `warning` - 警告
- `skip` - 跳过
- `error` - 执行错误
- `inconclusive` - 无法判定

### 4.6 packet_captures (抓包记录表)

**表说明**: 存储实时通信抓包会话的元数据。

**字段定义**:

| 字段名 | 数据类型 | 约束 | 说明 |
|--------|----------|------|------|
| capture_id | UUID | PK | 抓包ID |
| task_id | UUID | FK | 关联任务 |
| result_id | UUID | FK | 关联结果 |
| capture_name | VARCHAR(100) | - | 抓包名称 |
| protocol_layer | VARCHAR(30) | - | 协议层: ble/nfc/uwb |
| capture_start_at | TIMESTAMPTZ | - | 开始时间 |
| capture_end_at | TIMESTAMPTZ | - | 结束时间 |
| duration_ms | INTEGER | - | 持续时间(毫秒) |
| file_path | VARCHAR(500) | - | pcap文件路径 |
| file_format | VARCHAR(20) | DEFAULT 'pcapng' | 文件格式 |
| file_size_bytes | INTEGER | - | 文件大小 |
| packet_count | INTEGER | - | 包数量 |
| metadata | JSONB | - | 通信参数 |
| captured_by | UUID | FK | 抓包人 |
| captured_at | TIMESTAMPTZ | DEFAULT NOW() | 抓包时间 |

### 4.7 communication_flows (通信流水线表)

**表说明**: 解析抓包内容，存储通信流的解析结果。

**字段定义**:

| 字段名 | 数据类型 | 约束 | 说明 |
|--------|----------|------|------|
| flow_id | UUID | PK | 流水线ID |
| capture_id | UUID | FK | 关联抓包 |
| flow_sequence | INTEGER | - | 流水线序号 |
| timestamp | TIMESTAMPTZ | - | 时间戳 |
| source_type | VARCHAR(20) | - | 源端类型 |
| source_id | VARCHAR(100) | - | 源端ID |
| destination_type | VARCHAR(20) | - | 目标端类型 |
| destination_id | VARCHAR(100) | - | 目标端ID |
| protocol | VARCHAR(20) | - | 协议 |
| message_type | VARCHAR(50) | - | 消息类型 |
| command_code | VARCHAR(20) | - | 命令码 |
| payload_size | INTEGER | - | 载荷大小 |
| payload_hash | BYTEA | - | 载荷哈希 |
| parsed_data | JSONB | - | 解析数据 |
| response_time_ms | INTEGER | - | 响应时间 |
| result_status | VARCHAR(20) | - | 响应状态 |

**端点类型 (source_type/destination_type)**:
- `mobile` - 手机
- `vehicle` - 车辆
- `se` - SE安全元
- `server` - 服务器
- `relay` - 中继节点

### 4.8 compliance_reports (合规报告表)

**表说明**: 存储生成的合规报告信息。

**字段定义**:

| 字段名 | 数据类型 | 约束 | 默认值 | 说明 |
|--------|----------|------|--------|------|
| report_id | UUID | PK | gen_random_uuid() | 报告ID |
| report_title | VARCHAR(200) | NOT NULL | - | 报告标题 |
| report_code | VARCHAR(50) | UK, NOT NULL | - | 报告编码 |
| report_type | VARCHAR(30) | - | - | 报告类型 |
| task_id | UUID | FK | - | 关联任务 |
| target_id | UUID | FK | - | 关联对象 |
| protocol | VARCHAR(20) | - | - | 协议 |
| specification_version | VARCHAR(20) | - | - | 版本 |
| overall_compliance | VARCHAR(20) | - | - | 总体合规结论 |
| compliance_percentage | DECIMAL(5,2) | - | - | 合规百分比 |
| total_rules_tested | INTEGER | - | - | 测试规则数 |
| passed_rules | INTEGER | - | - | 通过数 |
| failed_rules | INTEGER | - | - | 失败数 |
| warning_rules | INTEGER | - | - | 警告数 |
| report_file_path | VARCHAR(500) | - | - | 报告文件路径 |
| report_format | VARCHAR(20) | DEFAULT 'pdf' | - | 格式 |
| file_size_bytes | INTEGER | - | - | 文件大小 |
| generated_by | UUID | FK | - | 生成人 |
| generated_at | TIMESTAMPTZ | DEFAULT NOW() | - | 生成时间 |
| digital_signature | BYTEA | - | - | 数字签名 |
| signature_valid_until | TIMESTAMPTZ | - | - | 签名有效期 |
| approved_by | UUID | FK | - | 审批人 |
| approved_at | TIMESTAMPTZ | - | - | 审批时间 |
| approval_status | VARCHAR(20) | DEFAULT 'pending' | - | 审批状态 |
| is_archived | BOOLEAN | DEFAULT FALSE | - | 是否归档 |
| archived_at | TIMESTAMPTZ | - | - | 归档时间 |

### 4.9 audit_logs (审计日志表) - 分区表

**表说明**: 记录系统所有操作，支持审计追溯。

**分区策略**: `PARTITION BY RANGE (executed_at)`

**字段定义**:

| 字段名 | 数据类型 | 约束 | 说明 |
|--------|----------|------|------|
| log_id | UUID | PK | 日志ID |
| action_type | VARCHAR(50) | NOT NULL | 操作类型 |
| action_target | VARCHAR(50) | - | 操作对象类型 |
| target_id | UUID | - | 对象ID |
| user_id | UUID | FK | 执行用户 |
| user_role | VARCHAR(20) | - | 用户角色 |
| action_details | JSONB | - | 操作详情 |
| old_values | JSONB | - | 旧值 |
| new_values | JSONB | - | 新值 |
| ip_address | INET | - | IP地址 |
| user_agent | TEXT | - | User Agent |
| session_id | VARCHAR(100) | - | 会话ID |
| executed_at | TIMESTAMPTZ | DEFAULT NOW() | 执行时间 |
| execution_time_ms | INTEGER | - | 执行耗时 |

---

## 五、业务流程

### 5.1 测试完整流程

```
┌───────────────────────────────────────────────────────────────────┐
│                   yuleDKCS 测试完整流程                        │
├───────────────────────────────────────────────────────────────────┤
│                                                              │
│   阶段1: 准备阶段                                          │
│   ├─────────────────────────────────────────────────────────────┤        │
│   │                                                        │        │
│   │  [用户登录]                                            │        │
│   │       ↓                                                │        │
│   │  [配置测试对象]                                        │        │
│   │       │  → INSERT INTO test_targets                     │        │
│   │       ↓                                                │        │
│   │  [上传证书]                                            │        │
│   │       │  → INSERT INTO target_certificates              │        │
│   │       ↓                                                │        │
│   │  [选择测试套件]                                        │        │
│   │       │  → 从 test_suites 选择                      │        │
│   │                                                        │        │
│   阶段2: 执行阶段                                          │        │
│   ├─────────────────────────────────────────────────────────────┤        │
│   │                                                        │        │
│   │  [创建测试任务]                                        │        │
│   │       │  → INSERT INTO test_tasks (status='pending')   │        │
│   │       ↓                                                │        │
│   │  [启动抓包] ────────────────────────────────────────→        │
│   │       ↓                                                │        │
│   │  [遍历执行规则]                                        │        │
│   │       │  → SELECT FROM compliance_rules                │        │
│   │       ↓                                                │        │
│   │  [执行测试点]                                          │        │
│   │       │  → SELECT FROM rule_test_points               │        │
│   │       ↓                                                │        │
│   │  [记录结果] ────────────────────────────→ INSERT INTO test_results     │
│   │       ↓                                                │        │
│   │  [更新任务进度]                                        │        │
│   │       │  → UPDATE test_tasks                           │        │
│   │                                                        │        │
│   阶段3: 报告阶段                                          │        │
│   ├─────────────────────────────────────────────────────────────┤        │
│   │                                                        │        │
│   │  [汇总统计]                                            │        │
│   │       │  → SELECT FROM test_results                   │        │
│   │       ↓                                                │        │
│   │  [生成报告]                                            │        │
│   │       │  → INSERT INTO compliance_reports              │        │
│   │       ↓                                                │        │
│   │  [保存抓包] ←───────────────────────────────────────┘        │
│   │       ↓                                                │        │
│   │  [审批发布]                                            │        │
│   │       ↓                                                │        │
│   │  [归档存储]                                            │        │
│   │                                                        │        │
└───────────────────────────────────────────────────────────────────┘
```

### 5.2 关键业务查询

**查询任务进度**:
```sql
SELECT 
    t.task_id,
    t.task_name,
    t.status,
    t.total_rules,
    t.completed_rules,
    ROUND((t.completed_rules::NUMERIC / NULLIF(t.total_rules, 0)) * 100, 2) as progress_pct
FROM test_tasks t
WHERE t.task_id = ?;
```

**查询最新测试结果**:
```sql
SELECT DISTINCT ON (r.rule_id)
    r.rule_code,
    r.rule_name,
    r.severity,
    res.result_status,
    res.created_at
FROM test_results res
JOIN compliance_rules r ON res.rule_id = r.rule_id
WHERE res.task_id = ?
ORDER BY r.rule_id, res.created_at DESC;
```

**查询合规统计**:
```sql
SELECT 
    result_status,
    COUNT(*) as count,
    ROUND(COUNT(*)::NUMERIC / SUM(COUNT(*)) OVER() * 100, 2) as percentage
FROM test_results
WHERE task_id = ?
GROUP BY result_status;
```

---

## 六、性能优化策略

### 6.1 分区表设计

**分区表**: test_results, audit_logs

**分区策略**: 按月分区 (RANGE)

**创建语句**:
```sql
-- 创建分区表 (按月)
CREATE TABLE test_results_2024_01 PARTITION OF test_results
    FOR VALUES FROM ('2024-01-01') TO ('2024-02-01');

-- 自动创建新分区脚本
CREATE OR REPLACE FUNCTION create_monthly_partition()
RETURNS void AS $$
DECLARE
    partition_date DATE;
    partition_name TEXT;
    start_date DATE;
    end_date DATE;
BEGIN
    partition_date := DATE_TRUNC('month', NOW() + INTERVAL '1 month');
    partition_name := 'test_results_' || TO_CHAR(partition_date, 'YYYY_MM');
    start_date := partition_date;
    end_date := partition_date + INTERVAL '1 month';
    
    EXECUTE format('CREATE TABLE IF NOT EXISTS %I PARTITION OF test_results FOR VALUES FROM (%L) TO (%L)',
                   partition_name, start_date, end_date);
END;
$$ LANGUAGE plpgsql;
```

### 6.2 索引优化

**核心索引**:
```sql
-- compliance_rules 索引
CREATE INDEX idx_rules_protocol ON compliance_rules(protocol);
CREATE INDEX idx_rules_category ON compliance_rules(category);
CREATE INDEX idx_rules_severity ON compliance_rules(severity);
CREATE INDEX idx_rules_active ON compliance_rules(is_active);

-- test_results 索引 (分区表)
CREATE INDEX idx_results_task ON test_results(task_id);
CREATE INDEX idx_results_rule ON test_results(rule_id);
CREATE INDEX idx_results_status ON test_results(result_status);
CREATE INDEX idx_results_created ON test_results(created_at DESC);

-- JSONB 索引 (GIN)
CREATE INDEX idx_targets_protocols ON test_targets USING GIN(supported_protocols);
CREATE INDEX idx_suites_protocols ON test_suites USING GIN(target_protocols);
```

### 6.3 查询优化

**视图优化**:
```sql
-- 任务进度视图 (已预计算)
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

-- 最新结果视图 (使用 DISTINCT ON 优化)
CREATE VIEW latest_results AS
SELECT DISTINCT ON (task_id, rule_id)
    r.*,
    c.rule_code,
    c.rule_name,
    c.severity
FROM test_results r
JOIN compliance_rules c ON r.rule_id = c.rule_id
ORDER BY task_id, rule_id, r.created_at DESC;
```

---

## 七、安全设计

### 7.1 数据安全

**敏感字段加密**:
- 证书数据 (cert_data) 使用 BYTEA 存储
- 密码哈希化存储 (password_hash)
- SE识别码加密存储

### 7.2 访问控制

**角色权限**:
- `admin` - 系统管理员，完全权限
- `tester` - 测试工程师，执行测试权限
- `auditor` - 审计员，查看报告权限
- `viewer` - 只读用户

### 7.3 审计追溯

**audit_logs 表记录**:
- 所有数据变更操作
- 用户登录/登出
- 测试执行记录
- 报告生成/下载

---

## 八、部署指南

### 8.1 环壁要求

- **PostgreSQL**: 15+
- **扩展**: uuid-ossp, pgcrypto
- **存储**: 根据抓包数据量预估 (100GB+)
- **内存**: 8GB+

### 8.2 初始化步骤

```bash
# 1. 创建数据库
createdb dkcs_db

# 2. 创建用户
createuser -P dkcs_user

# 3. 赋予权限
psql -c "GRANT ALL PRIVILEGES ON DATABASE dkcs_db TO dkcs_user;"

# 4. 执行Schema
psql -U dkcs_user -d dkcs_db -f schema_v1.0.0.sql
```

### 8.3 配置建议

**postgresql.conf**:
```ini
# 分区表性能
enable_partition_pruning = on

# 并行查询
max_parallel_workers_per_gather = 4

# 索引维护
autovacuum = on
```

---

## 九、附录

### 附录A: 文件列表

| 文件名 | 说明 |
|--------|------|
| schema_v1.0.0.sql | 完整数据库Schema脚本 |
| ER_DIAGRAM.md | ER图和关系说明 |
| schema_report.html | HTML格式报告 (可视化) |
| DESIGN_REPORT.md | 本设计报告 |

### 附录B: 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| v1.0.0 | 2026-05-11 | 初始版本，支持CCC/ICCOA/ICCE三大协议 |

### 附录C: 联系信息

**项目**: yuleDKCS  
**作者**: Hermes Agent  
**日期**: 2026-05-11

---

*本文档使用 Markdown 格式，可用任意 Markdown 阅读器打开*
