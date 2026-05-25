# CCC/ICCOA/ICCE 协议对比分析

> **版本**: v1.0.0  
> **分析日期**: 2026-05-11  
> **目标**: 统一数据库设计，支持多协议共存

---

## 一、协议概览

| 特性 | **CCC** | **ICCOA** | **ICCE** |
|------|---------|-----------|----------|
| **全称** | Car Connectivity Consortium | 智惠车联开放联盟 | 智惠车联产业生态联盟 |
| **总部** | 美国 | 中国 | 中国 |
| **定位** | 全球标准 | 国产化替代 | 国产化替代 |
| **核心技术** | BLE/NFC + UWB | BLE | BLE/NFC |
| **安全芯片** | 强制SE | TEE/可选SE | TEE/可选SE |
| **认证方式** | PKI证书链 | 密钡+令牌 | 密钡+认证 |
| **市场占有率** | 高端车型 | 中国品牌 | 新兴品牌 |

---

## 二、数据库字段对比

### 2.1 车辆标识

| 字段 | CCC | ICCOA | ICCE | 数据库设计 |
|------|-----|-------|------|-----------|
| **VIN** | ✅ 必需 | ✅ 必需 | ✅ 必需 | `vehicles.vin` (PK) |
| **车辆证书** | ✅ X.509 | ❌ 无 | ❌ 无 | `certificates` (CCC专用) |
| **车厂CA** | ✅ 预置 | ❌ 无 | ❌ 无 | `certificates` (oem类型) |

### 2.2 钥匙标识

| 字段 | CCC | ICCOA | ICCE | 数据库设计 |
|------|-----|-------|------|-----------|
| **Slot ID** | ✅ 0-7 | ❌ 无 | ❌ 无 | `ccc_slots.slot_identifier` |
| **Key ID** | ✅ 8字节 | ❌ 无 | ✅ ICCE标识 | `ccc_slots.key_identifier` |
| **手机号** | ❌ 无 | ✅ 必需 | ❌ 无 | `iccoa_keys.phone_number` |
| **Device ID** | ✅ 16字节 | ✅ Device Token | ✅ Device ID | `devices.device_identifier` |

### 2.3 密钥管理

| 特性 | CCC | ICCOA | ICCE | 数据库设计 |
|------|-----|-------|------|-----------|
| **密钥类型** | 对称密钥 | 对称密钥 | 对称密钥 | `ccc_slots`/单独表 |
| **密钥位置** | SE芯片 | TEE/服务端 | TEE/服务端 | `secure_elements.se_type` |
| **授权动作** | JSON配置 | 位掩码 | 位掩码 | `authorized_actions` |
| **计数器** | 会话级 | 无 | 有 (防重放) | `icce_keys.device_counter` |

---

## 三、关键差异分析

### 3.1 CCC vs ICCOA

```
CCC优势:
✓ 标准化PKI体系
✓ 强制安全芯片(SE)
✓ 支持UWB精确定位
✓ 全球互认

ICCOA优势:
✓ 不依赖外国PKI
✓ TEE即可(成本低)
✓ 简化部署流程
✓ 本土化服务支持

关键差异:
- 认证: CCC=证书链, ICCOA=密钥+令牌
- 槽位: CCC=8个硬件槽位, ICCOA=软件级无限
- 安全: CCC=SE芯片, ICCOA=TEE模拟
```

### 3.2 CCC vs ICCE

```
共同点:
• 都采用BLE通信
• 都支持密钥分享
• 都有计数器防重放

主要差异:
- 协议栈: CCC=BLE+UWB, ICCE=BLE主要
- PKI依赖: CCC=强制, ICCE=可选
- SE要求: CCC=必须, ICCE=推荐
```

### 3.3 ICCOA vs ICCE

| 对比项 | ICCOA | ICCE |
|---------|-------|------|
| 定位 | 华为/小米生态 | 广汽/上汽/吉利生态 |
| 服务端 | 必需 | 可选本地通信 |
| 授权方式 | 手机号绑定 | 设备绑定 |
| 分享限制 | 时间/次数 | 地理围栏+时间 |

---

## 四、数据库共用设计

### 4.1 表结构设计

```
┌─────────────────┐
│    vehicles    │  ← 三协议共用
├─────────────────┤
│ vin (PK)      │
│ support_ccc   │  ✓ 标记支持的协议
│ support_iccoa │
│ support_icce  │
└─────────────────┘
       │
       │─────────────┐
       ▼            ▼
┌─────────────────┐  ┌─────────────────┐
│   ccc_slots   │  │  iccoa_keys  │  ┌─────────────────┐
├─────────────────┤  ├─────────────────┤  │   icce_keys   │
│ slot_id       │  │ key_id      │  ├─────────────────┤
│ slot_ident    │  │ phone_no    │  │ key_id        │
│ key_ident     │  │ device_token│  │ icce_ident    │
│ device_ident  │  │ auth_key    │  │ vehicle_pk    │
└─────────────────┘  └─────────────────┘  │ device_sk     │
                              └─────────────────┘
```

### 4.2 证书统一表

```sql
-- 统一证书表，通过protocol字段区分来源
CREATE TABLE certificates (
    cert_id UUID PRIMARY KEY,
    protocol VARCHAR(10) CHECK (protocol IN ('CCC', 'ICCOA', 'ICCE')),
    cert_type VARCHAR(30),
    cert_data BYTEA,
    -- ...
);
```

### 4.3 设备统一表

```sql
-- 设备表支持多协议
CREATE TABLE devices (
    device_id UUID PRIMARY KEY,
    device_type VARCHAR(20), -- ios/android/watch/card/fob
    
    -- 协议支持标记
    support_ccc BOOLEAN DEFAULT FALSE,
    support_iccoa BOOLEAN DEFAULT FALSE,
    support_icce BOOLEAN DEFAULT FALSE,
    
    -- 协议特定字段
    device_identifier BYTEA,      -- CCC/ICCE
    iccoa_device_token TEXT,      -- ICCOA特有
    
    -- ...
);
```

---

## 五、缺失字段补充

### 5.1 ICCOA特有字段 (已补充)

| 字段 | 表 | 说明 |
|------|-----|------|
| phone_number | iccoa_keys | 手机号绑定 |
| iccoa_key_id | iccoa_keys | ICCOA钥匙标识 |
| device_token | iccoa_keys | 推送通道Token |
| auth_key | iccoa_keys | 认证密钥 |
| encrypt_key | iccoa_keys | 加密密钥 |
| permission_mask | iccoa_keys | 位掩码权限 |

### 5.2 ICCE特有字段 (已补充)

| 字段 | 表 | 说明 |
|------|-----|------|
| icce_key_identifier | icce_keys | ICCE钥匙标识 |
| vehicle_pk | icce_keys | 车辆公钥 |
| device_sk | icce_keys | 设备私钥 |
| session_key | icce_keys | 会话密钥 |
| vehicle_counter | icce_keys | 车端计数器 |
| device_counter | icce_keys | 设备计数器 |

---

## 六、共用与差异设计建议

### 6.1 应统一设计

| 功能 | 建议 | 理由 |
|------|-------|-------|
| 用户管理 | 统一表 | 不因协议变化 |
| 车辆基础信息 | 统一表 | VIN是通用标识 |
| 设备管理 | 统一表 + 协议标记 | 一设备可支持多协议 |
| 钥匙管理 | 分表设计 | 协议特性差异大 |
| 证书管理 | 统一表 + 协议标记 | 不同协议证书格式不同 |
| 操作日志 | 统一表 + 协议标记 | 统一审计入口 |

### 6.2 协议选择策略

```
建议实施优先级:

1. 高端进口车型 → CCC (完善的PKI和UWB支持)
2. 国产车型 → ICCOA (本土化服务)
3. 成本敏感型 → ICCE (低成本部署)
4. 全系支持 → 三协议兼容 (数据库已支持)
```

---

## 七、BER-TLV映射差异

### 7.1 协议特定TAG

| TAG | CCC | ICCOA | ICCE | 数据库映射 |
|-----|-----|-------|------|-----------|
| 0x41 (VIN) | ✅ | ✅ | ✅ | vehicles.vin |
| 0x46 (SLOT_ID) | ✅ | ❌ | ❌ | ccc_slots.slot_identifier |
| 0x47 (KEY_ID) | ✅ | ❌ | ✅ | ccc_slots.key_identifier / icce_keys.icce_key_identifier |
| 0x48 (DEVICE_ID) | ✅ | ✅ | ✅ | devices.device_identifier |
| 0x49 (CERT) | ✅ | ❌ | ✅ | certificates.cert_data |
| 0x4A (OEM_ID) | ✅ | ❌ | ✅ | certificates.oem_id |
| 0x4B (PHONE) | ❌ | ✅ | ❌ | iccoa_keys.phone_number |
| 0x4C (PERMISSION) | ✅ JSON | ✅ 位掩 | ✅ 位掩 | authorized_actions / permission_mask |
| 0x4D (COUNTER) | ❌ | ❌ | ✅ | icce_keys.device_counter |

### 7.2 建议的统一TAG分配

```yaml
# 建议在BER-TLV中增加协议标识字段
protocol_identifier:
  tag: 0x7F  # 私有TAG
  values:
    0x01: CCC
    0x02: ICCOA
    0x03: ICCE
```

---

## 八、总结与建议

### 8.1 已覆盖的字段

✦ 完整支持:
- CCC标准所有字段
- ICCOA特有字段
- ICCE特有字段
- 跨协议共用字段

### 8.2 建议补充

| 项目 | 优先级 | 说明 |
|------|--------|------|
| 协议自动检测 | 高 | 根据车型VIN前缀自动选择协议 |
| 协议间钥匙迁移 | 中 | CCC↔ICCOA钥匙数据转换 |
| 统一审计日志 | 高 | 所有协议操作统一记录 |

### 8.3 数据库优化建议

```sql
-- 为多协议查询添加复合索引
CREATE INDEX idx_vehicles_protocol ON vehicles(support_ccc, support_iccoa, support_icce);

-- 协议特定查询优化
CREATE INDEX idx_certs_protocol_type ON certificates(protocol, cert_type);
```

---

*文档版本: v1.0.0*  
*参考标准*:
- CCC Digital Key Specification v3.0
- ICCOA Digital Key Specification v1.0
- ICCE Digital Key Specification v1.0
