# yuleDKCS 车云一体化数据库 ER 图

> **版本**: v1.0.0  
> **数据库**: PostgreSQL 15+  
> **支持协议**: CCC / ICCOA / ICCE

---

## 一、完整 ER 图

```mermaid
erDiagram
    %% 核心实体
    USERS ||--o{ VEHICLES : owns
    USERS ||--o{ DEVICES : has
    USERS ||--o{ VEHICLE_ALIASES : defines
    
    %% 车辆关系
    VEHICLES ||--o{ CCC_SLOTS : has
    VEHICLES ||--o{ ICCOA_KEYS : has
    VEHICLES ||--o{ ICCE_KEYS : has
    VEHICLES ||--o{ CERTIFICATES : owns
    VEHICLES ||--o{ VEHICLE_TELEMETRY : generates
    VEHICLES ||--o{ OPERATION_LOGS : records
    
    %% 设备关系
    DEVICES ||--o{ CCC_SLOTS : binds
    DEVICES ||--o{ ICCOA_KEYS : binds
    DEVICES ||--o{ ICCE_KEYS : binds
    DEVICES ||--o{ SECURE_ELEMENTS : contains
    DEVICES ||--o{ CERTIFICATES : has
    
    %% 安全芯片
    SECURE_ELEMENTS ||--o{ CERTIFICATES : stores
    
    %% CCC 体系
    CCC_SLOTS ||--o{ KEY_SHARES : shared_via
    CCC_SLOTS ||--o{ CERTIFICATES : authenticated_by
    CCC_SLOTS ||--o{ OPERATION_LOGS : generates
    
    %% ICCOA 体系
    ICCOA_KEYS ||--o{ KEY_SHARES : shared_via
    
    %% ICCE 体系
    ICCE_KEYS ||--o{ KEY_SHARES : shared_via
    
    %% 证书体系
    CERTIFICATES ||--o{ CERTIFICATES : parent_of
    
    %% 用户分享
    USERS ||--o{ KEY_SHARES : owns
    USERS ||--o{ KEY_SHARES : receives
    
    %% 消息队列
    VEHICLES ||--o{ MESSAGE_QUEUE : processes
    DEVICES ||--o{ MESSAGE_QUEUE : processes
    
    %% 实体定义
    USERS {
        uuid id PK "用户ID"
        string email UK "邮箱"
        string password_hash "密码哈希"
        string name "用户名"
        string phone "手机号"
        timestamp created_at "创建时间"
        timestamp updated_at "更新时间"
    }
    
    VEHICLES {
        string vin PK "VIN码(17位)"
        uuid user_id FK "车主ID"
        string vehicle_model "车型"
        string vehicle_brand "品牌"
        string license_plate UK "车牌号"
        date manufacture_date "生产日期"
        string color "颜色"
        string status "状态"
        string tbox_device_id UK "T-Box设备ID"
        boolean support_ccc "支持CCC"
        boolean support_iccoa "支持ICCOA"
        boolean support_icce "支持ICCE"
        timestamp last_online_at "最后在线"
        timestamp created_at "创建时间"
    }
    
    VEHICLE_ALIASES {
        uuid alias_id PK "别名ID"
        string vin FK "VIN码"
        uuid user_id FK "用户ID"
        string alias_name "别名"
    }
    
    DEVICES {
        uuid device_id PK "设备ID"
        uuid user_id FK "用户ID"
        string device_type "设备类型"
        string device_name "设备名"
        string device_model "型号"
        binary device_identifier UK "设备标识(16字节)"
        string se_type "SE类型"
        binary public_key "公钥"
        boolean support_ccc "支持CCC"
        boolean support_iccoa "支持ICCOA"
        boolean support_icce "支持ICCE"
        boolean is_active "是否激活"
        timestamp paired_at "配对时间"
    }
    
    SECURE_ELEMENTS {
        uuid se_id PK "SE ID"
        uuid device_id FK "设备ID"
        string se_type "SE类型"
        binary se_unique_id UK "SE唯一ID"
        integer scp03_key_version "SCP03版本"
        binary scp03_static_key "静态密钥"
        string attestation_status "认证状态"
        timestamp provisioned_at "预置时间"
    }
    
    CCC_SLOTS {
        uuid slot_id PK "槽位ID"
        string vin FK "VIN码"
        integer slot_identifier "槽位号(0-7)"
        binary key_identifier UK "钥匙标识(8字节)"
        binary device_identifier "设备标识"
        string slot_state "槽位状态"
        string key_type "钥匙类型"
        json authorized_actions "授权操作"
        timestamp valid_from "生效时间"
        timestamp valid_until "过期时间"
    }
    
    ICCOA_KEYS {
        uuid key_id PK "钥匙ID"
        string vin FK "VIN码"
        string phone_number "手机号"
        string iccoa_key_id UK "ICCOA钥匙ID"
        binary auth_key "认证密钥"
        binary encrypt_key "加密密钥"
        string device_token "设备令牌"
        integer permission_mask "权限掩码"
        timestamp valid_until "过期时间"
    }
    
    ICCE_KEYS {
        uuid key_id PK "钥匙ID"
        string vin FK "VIN码"
        binary icce_key_identifier UK "ICCE标识"
        binary vehicle_pk "车辆公钥"
        binary device_sk "设备私钥"
        integer vehicle_counter "车辆计数器"
        integer device_counter "设备计数器"
        timestamp valid_until "过期时间"
    }
    
    CERTIFICATES {
        uuid cert_id PK "证书ID"
        string vin FK "VIN码"
        uuid device_id FK "设备ID"
        string cert_type "证书类型"
        string protocol "协议来源"
        binary cert_data "证书数据"
        binary cert_hash UK "证书哈希"
        string serial_number UK "序列号"
        timestamp valid_from "生效时间"
        timestamp valid_until "过期时间"
        boolean is_custom "是否自定义"
        uuid parent_cert_id FK "父证书ID"
    }
    
    KEY_SHARES {
        uuid share_id PK "分享ID"
        uuid slot_id FK "CCC槽位"
        uuid iccoa_key_id FK "ICCOA钥匙"
        uuid owner_user_id FK "车主ID"
        uuid guest_user_id FK " guest ID"
        string key_type "钥匙类型"
        json authorized_actions "授权操作"
        string share_method "分享方式"
        string status "状态"
        timestamp valid_until "过期时间"
    }
    
    OPERATION_LOGS {
        uuid log_id PK "日志ID"
        string vin FK "VIN码"
        uuid user_id FK "用户ID"
        string operation_type "操作类型"
        string protocol "协议"
        string result "结果"
        timestamp created_at "创建时间"
    }
    
    VEHICLE_TELEMETRY {
        timestamp time PK "时间戳"
        string vin PK,FK "VIN码"
        float latitude "纬度"
        float longitude "经度"
        float speed "速度"
        json door_status "车门状态"
        integer battery_level "电量"
    }
    
    MESSAGE_QUEUE {
        uuid msg_id PK "消息ID"
        string vin FK "VIN码"
        uuid device_id FK "设备ID"
        string target_protocol "目标协议"
        binary payload "消息内容"
        integer priority "优先级"
        string status "状态"
        integer retry_count "重试次数"
    }
```

---

## 二、核心关系说明

### 2.1 用户-车辆关系

```
users ||--o{ vehicles : owns
```
- 一个用户可以拥有多辆车
- 每辆车有唯一VIN作为主键
- 车辆支持多种数字钥匙协议

### 2.2 车辆-钥匙关系

```
vehicles ||--o{ ccc_slots : has
vehicles ||--o{ iccoa_keys : has
vehicles ||--o{ icce_keys : has
```
- 一辆车可同时拥有CCC/ICCOA/ICCE钥匙
- CCC: 最多8个槽位(slot 0-7)
- ICCOA/ICCE: 理论上无限，实际受限于策略

### 2.3 设备-钥匙绑定

```
devices ||--o{ ccc_slots : binds
devices ||--o{ iccoa_keys : binds
devices ||--o{ icce_keys : binds
```
- 一个设备可绑定多把钥匙(不同车辆)
- 通过 device_identifier 关联

### 2.4 证书链关系

```
certificates ||--o{ certificates : parent_of
```
- 自引用关系构建证书链
- parent_cert_id 指向父证书

---

## 三、协议特定关系

### 3.1 CCC 关系图

```mermaid
erDiagram
    VEHICLES ||--o{ CCC_SLOTS : "最多8个槽位"
    DEVICES ||--o{ CCC_SLOTS : "绑定设备"
    CCC_SLOTS ||--o{ KEY_SHARES : "钥匙分享"
    CCC_SLOTS ||--o{ CERTIFICATES : "证书认证"
    SECURE_ELEMENTS ||--o{ CCC_SLOTS : "SE芯片存储"
    
    CCC_SLOTS {
        uuid slot_id PK
        string vin FK
        int slot_identifier "0-7"
        binary key_identifier
        string slot_state
    }
```

### 3.2 ICCOA 关系图

```mermaid
erDiagram
    VEHICLES ||--o{ ICCOA_KEYS : "手机钥匙"
    DEVICES ||--o{ ICCOA_KEYS : "设备绑定"
    ICCOA_KEYS ||--o{ KEY_SHARES : "授权分享"
    
    ICCOA_KEYS {
        uuid key_id PK
        string vin FK
        string phone_number
        string iccoa_key_id UK
        binary auth_key
        int permission_mask
    }
```

### 3.3 ICCE 关系图

```mermaid
erDiagram
    VEHICLES ||--o{ ICCE_KEYS : "车端公钥"
    DEVICES ||--o{ ICCE_KEYS : "设备私钥"
    ICCE_KEYS ||--o{ KEY_SHARES : "分享授权"
    
    ICCE_KEYS {
        uuid key_id PK
        string vin FK
        binary vehicle_pk
        binary device_sk
        int vehicle_counter
        int device_counter
    }
```

---

## 四、数据流关系

### 4.1 钥匙分享流程

```mermaid
sequenceDiagram
    participant Owner as 车主(users)
    participant Vehicle as 车辆(vehicles)
    participant Slot as 槽位(ccc_slots)
    participant Share as 分享记录(key_shares)
    participant Guest as 被授权人(users)
    
    Owner->>Vehicle: 选择车辆
    Vehicle->>Slot: 查询可用槽位
    Slot->>Share: 创建分享记录
    Share->>Guest: 发送授权
    Guest->>Slot: 激活钥匙
    Slot->>Vehicle: 解锁/启动
```

### 4.2 证书验证流程

```mermaid
sequenceDiagram
    participant Device as 设备(devices)
    participant SE as 安全芯片(secure_elements)
    participant Cert as 证书(certificates)
    participant Vehicle as 车辆(vehicles)
    
    Device->>SE: 请求签名
    SE->>Cert: 获取证书链
    Cert->>Cert: 验证链完整性
    Cert->>Vehicle: 发送证书
    Vehicle->>Vehicle: 验证根证书
    Vehicle->>Device: 认证成功
```

---

## 五、表统计信息

| 表名 | 类型 | 主键 | 外键数 | 说明 |
|------|------|------|--------|------|
| users | 核心 | uuid | 0 | 用户基础 |
| vehicles | 核心 | varchar(17) | 1 | 车辆信息 |
| devices | 核心 | uuid | 1 | 设备管理 |
| ccc_slots | 协议 | uuid | 2 | CCC槽位 |
| iccoa_keys | 协议 | uuid | 1 | ICCOA钥匙 |
| icce_keys | 协议 | uuid | 1 | ICCE钥匙 |
| certificates | 安全 | uuid | 4 | 证书存储 |
| key_shares | 业务 | uuid | 4 | 分享记录 |
| operation_logs | 审计 | uuid | 2 | 操作日志 |
| vehicle_telemetry | 时序 | time+vin | 1 | 遥测数据 |

---

## 六、索引策略

### 6.1 主键索引
所有表均使用主键索引

### 6.2 唯一索引
- vehicles: vin, license_plate, tbox_device_id
- devices: device_identifier
- ccc_slots: key_identifier
- certificates: cert_hash, serial_number

### 6.3 外键索引
- 所有 _id 结尾的外键字段均有索引
- 复合索引支持多条件查询

### 6.4 查询优化索引
```sql
-- 车辆查询优化
CREATE INDEX idx_vehicles_user_status ON vehicles(user_id, status);

-- 槽位查询优化
CREATE INDEX idx_ccc_slots_vin_state ON ccc_slots(vin, slot_state);

-- 日志查询优化
CREATE INDEX idx_logs_vin_time ON operation_logs(vin, created_at DESC);
```

---

*文档版本: v1.0.0*  
*生成时间: 2026-05-11*
