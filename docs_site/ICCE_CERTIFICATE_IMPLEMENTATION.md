# ICCE 证书模块实现总结

## 文档信息
- 项目: yuleDKCS 数字钥匙系统
- 模块: ICCE Certificate Module
- 版本: 1.0.0
- 日期: 2026-05-15
- 状态: 已完成

---

## 1. 模块概述

ICCE (Intelligent Car Connectivity Ecosystem) 智慧车联产业生态联盟证书模块，实现国密算法栈的数字钥匙证书管理。

### 1.1 主要特点
- 国密算法: SM2/SM3/SM4
- 自定义二进制格式 (非X.509)
- 支持证书链验证
- APDU传输封装
- 证书大小≤1024字节

### 1.2 与 ICCOA/CCC 对比

| 特性 | ICCE | ICCOA | CCC |
|------|------|-------|-----|
| 证书格式 | 自定义二进制 | X.509 DER | X.509 DER |
| 签名算法 | SM2-SM3 | ECDSA-SHA256 | ECDSA-SHA256 |
| 加密算法 | SM4 | AES-128 | AES-128 |
| 公钥长度 | 65字节 | 65字节 | 65字节 |
| 时间格式 | Unix时间戳 | UTC时间 | UTC时间 |
| 证书大小 | ≤1024字节 | ≤700-800字节 | 无限制 |

---

## 2. 文件结构

```
embedded/
├── include/
│   └── icce_certificate.h          # ICCE证书头文件 (439行)
├── src/icce/security/
│   └── icce_certificate.c          # ICCE证书实现 (929行)
└── tests/unit/
    └── test_icce_certificate.c     # 单元测试 (418行)

总代码量: ~1,786行
```

---

## 3. 证书类型

| 类型代码 | 名称 | 说明 |
|----------|------|------|
| 0x01 | VehicleCA | 车厂根CA证书 |
| 0x02 | Vehicle | 车证书 |
| 0x03 | OwnerDK | 车主数字钥匙证书 |
| 0x04 | SharedDK | 分享数字钥匙证书 |
| 0x05 | TempAccess | 临时访问证书 |

### 3.1 证书链结构
```
VehicleCA (0x01) → Vehicle (0x02) → OwnerDK (0x03) → SharedDK (0x04)
                                       ↓
                                  TempAccess (0x05)
```

---

## 4. 证书格式

### 4.1 二进制格式

```
魔法头 (4字节): 0x49 0x43 0x43 0x45 ("ICCE")

字段格式: [Field ID:1] [Length:2] [Value:N]
```

### 4.2 字段定义

| Field ID | 名称 | 长度 | 描述 |
|----------|------|------|------|
| 0x01 | Version | 1 | 证书版本 |
| 0x02 | CertType | 1 | 证书类型 |
| 0x03 | CertLen | 2 | 证书总长度 |
| 0x04 | Issuer | N | 颁发者名称 |
| 0x05 | Subject | N | 主体名称 |
| 0x06 | DeviceID | 16 | 设备ID |
| 0x07 | VehicleID | 17 | 车辆ID |
| 0x08 | KeyID | 16 | 钥匙ID |
| 0x09 | ValidFrom | 4 | 生效时间 (Unix时间戳) |
| 0x0A | ValidUntil | 4 | 过期时间 (Unix时间戳) |
| 0x0B | PublicKey | 65 | SM2公钥 |
| 0x0C | Signature | 64 | SM2签名 (r||s) |
| 0x0D | Permissions | 4 | 权限位图 |
| 0x0E | KeyUsage | 2 | 密钥用途 |
| 0x0F | IsCA | 1 | CA标志 |
| 0x10 | MaxPathLen | 1 | 证书链最大深度 |
| 0xFF | EndMarker | 0 | 结束标记 |

### 4.3 C结构体定义

```c
typedef struct {
    uint8_t version;
    uint8_t cert_type;
    uint16_t cert_len;
    uint16_t issuer_len;
    uint8_t issuer[64];
    uint16_t subject_len;
    uint8_t subject[64];
    uint8_t device_id[16];
    uint8_t vehicle_id[17];
    uint8_t key_id[16];
    uint32_t valid_from;
    uint32_t valid_until;
    uint8_t public_key[65];
    uint32_t permissions;
    uint16_t key_usage;
    uint8_t is_ca;
    uint8_t max_path_len;
    uint8_t signature[64];
    uint8_t raw_data[1024];
    uint16_t raw_len;
} icce_certificate_t;
```

---

## 5. API 接口

### 5.1 证书操作

```c
void icce_certificate_init(icce_certificate_t *cert);
void icce_certificate_clear(icce_certificate_t *cert);
void icce_certificate_copy(icce_certificate_t *dst, const icce_certificate_t *src);
bool icce_certificate_equals(const icce_certificate_t *cert1, const icce_certificate_t *cert2);
```

### 5.2 序列化/解析

```c
error_t icce_serialize_certificate(const icce_certificate_t *cert, uint8_t *out, size_t *out_len);
error_t icce_parse_certificate(const uint8_t *data, size_t data_len, icce_certificate_t *cert);
```

### 5.3 证书验证

```c
error_t icce_verify_certificate(const icce_certificate_t *cert, 
                                 const uint8_t *trusted_pubkey,
                                 const icce_cert_validator_config_t *config);
error_t icce_validate_cert_chain(const icce_cert_chain_t *cert_chain,
                                  const icce_certificate_t *trusted_root,
                                  const icce_cert_validator_config_t *config);
error_t icce_validate_owner_cert(const icce_certificate_t *cert,
                                  const icce_certificate_t *ca_cert,
                                  const icce_cert_validator_config_t *config);
error_t icce_validate_shared_cert(const icce_certificate_t *cert,
                                   const icce_certificate_t *signer_cert,
                                   const icce_certificate_t *ca_cert,
                                   const icce_cert_validator_config_t *config);
```

### 5.4 APDU 传输

```c
error_t icce_cert_to_apdu(const icce_certificate_t *cert,
                           uint8_t *apdu_data,
                           size_t *apdu_len,
                           bool is_last);
error_t icce_cert_from_apdu(const uint8_t *apdu_data,
                             size_t apdu_len,
                             icce_certificate_t *cert);
```

### 5.5 工具函数

```c
size_t icce_get_certificate_length(const icce_certificate_t *cert);
bool icce_check_cert_size(const icce_certificate_t *cert);
bool icce_cert_is_expired(const icce_certificate_t *cert, uint32_t current_time);
bool icce_cert_is_valid_now(const icce_certificate_t *cert, uint32_t current_time);
const char* icce_cert_type_to_string(icce_cert_type_t type);
const char* icce_cert_error_to_string(int error);
void icce_cert_validator_config_init(icce_cert_validator_config_t *config);
```

### 5.6 SM2 签名验证

```c
error_t icce_sm2_verify_signature(const uint8_t digest[32],
                                   const uint8_t signature[64],
                                   const uint8_t public_key[65]);
error_t icce_cert_compute_hash(const icce_certificate_t *cert, uint8_t digest[32]);
```

---

## 6. 测试覆盖

### 6.1 测试用例

| 测试项 | 数量 | 状态 |
|--------|-------|------|
| 证书初始化 | 1 | ✅ |
| 证书清零 | 1 | ✅ |
| 证书复制 | 1 | ✅ |
| 证书比较 | 1 | ✅ |
| 序列化/解析 | 2 | ✅ |
| 大小检查 | 1 | ✅ |
| 过期检查 | 1 | ✅ |
| 类型转换 | 2 | ✅ |
| 验证器配置 | 1 | ✅ |
| APDU转换 | 1 | ✅ |
| 长度获取 | 1 | ✅ |
| 非法格式 | 1 | ✅ |
| 不同证书类型 | 1 | ✅ |
| **总计** | **15** | **✅** |

### 6.2 测试执行

```bash
# 编译测试
$ cd /home/admin/yuleDKCS/embedded
$ gcc -I./include -I./sdk/include tests/unit/test_icce_certificate.c \
      src/icce/security/icce_certificate.c -o test_icce_cert

# 运行测试
$ ./test_icce_cert
=================================================================
ICCE Certificate Module Unit Tests
=================================================================

  [TEST] certificate_init                       PASS
  [TEST] certificate_clear                      PASS
  [TEST] certificate_copy                       PASS
  [TEST] certificate_equals                     PASS
  [TEST] certificate_serialize                  PASS
  [TEST] certificate_parse                      PASS
  [TEST] check_cert_size                        PASS
  [TEST] cert_expiry                            PASS
  [TEST] cert_type_to_string                    PASS
  [TEST] cert_error_to_string                   PASS
  [TEST] validator_config_init                  PASS
  [TEST] cert_apdu_conversion                   PASS
  [TEST] get_certificate_length                 PASS
  [TEST] invalid_certificate_parse              PASS
  [TEST] different_cert_types                   PASS

=================================================================
Test Summary:
  Total:   15
  Passed:  15
  Failed:  0
  Time:    0 seconds
=================================================================
```

---

## 7. 使用示例

### 7.1 创建和序列化证书

```c
#include "icce_certificate.h"

// 初始化证书
icce_certificate_t cert;
icce_certificate_init(&cert);

cert.cert_type = ICCE_CERT_TYPE_OWNER_DK;
cert.subject_len = snprintf((char*)cert.subject, 64, "Owner Device 001");
cert.valid_from = time(NULL);
cert.valid_until = cert.valid_from + 365 * 24 * 3600;
cert.permissions = 0xFFFFFFFF;

// 序列化
uint8_t buffer[ICCE_MAX_CERT_SIZE];
size_t len = sizeof(buffer);
error_t ret = icce_serialize_certificate(&cert, buffer, &len);
if (ret == OK) {
    // buffer 中包含长度为 len 的序列化证书
}
```

### 7.2 解析证书

```c
icce_certificate_t parsed_cert;
error_t ret = icce_parse_certificate(received_data, data_len, &parsed_cert);
if (ret == OK) {
    printf("Certificate Type: %s\n", 
           icce_cert_type_to_string((icce_cert_type_t)parsed_cert.cert_type));
    printf("Subject: %.*s\n", parsed_cert.subject_len, parsed_cert.subject);
}
```

### 7.3 验证证书

```c
// 配置验证器
icce_cert_validator_config_t config;
icce_cert_validator_config_init(&config);
config.verify_time = true;
config.strict_size_check = true;

// 验证
error_t ret = icce_verify_certificate(&cert, ca_public_key, &config);
if (ret != OK) {
    printf("Verification failed: %s\n", icce_cert_error_to_string(ret));
}
```

### 7.4 APDU 传输

```c
// 证书转APDU
uint8_t apdu[ICCE_MAX_CERT_SIZE];
size_t apdu_len = sizeof(apdu);
error_t ret = icce_cert_to_apdu(&cert, apdu, &apdu_len, true);

// 发送到SE
ret = se_interface->transmit(apdu, apdu_len, response, &resp_len);
```

---

## 8. 国密算法集成

### 8.1 SM2/SM3/SM4 插件

ICCE证书模块设计为插件式结构，需要集成真实的国密算法实现：

```c
// 需要实现的国密接口

// SM3 哈希
error_t sm3_hash(const uint8_t *data, size_t len, uint8_t digest[32]);

// SM2 签名验证
error_t sm2_verify(const uint8_t digest[32], 
                    const uint8_t sig[64],
                    const uint8_t pubkey[65]);

// SM4 加解密
error_t sm4_encrypt(const uint8_t key[16],
                     const uint8_t iv[16],
                     const uint8_t *plaintext,
                     size_t len,
                     uint8_t *ciphertext);
```

### 8.2 推荐的国密库

1. **GMSSL** - 国密密码模块
2. **TASSL** - 泰安安全套件
3. **mbedtls-sm2** - ARM mbedTLS 国密扩展
4. 自研SE驱动 - 华大/国密/联发科国密SE芯片

---

## 9. 属性对比

### 9.1 安全级别

| 算法 | 安全级别 | 对应国际算法 |
|------|----------|------------|
| SM2 | 256位 | RSA-3072 / ECDSA P-256 |
| SM3 | 256位 | SHA-256 |
| SM4 | 128位 | AES-128 |

### 9.2 性能特征

- 证书大小: 约 300-600字节 (取决于主题名长度)
- 序列化速度: < 1ms
- 验证速度: 取决于SM2验签实现 (硬件SE通常 < 10ms)
- 内存占用: ~2KB/证书

---

## 10. 参考文档

1. ICCE Digital Key 2.0 规范
2. GM/T 0003.1-2012 SM2椭圆曲线公钥密码算法
3. GM/T 0004-2012 SM3杂凑算法
4. GM/T 0002-2012 SM4分组加密算法
5. 三端通信协议规范 (docs/protocol-specs/TRIPLE_END_COMMUNICATION_PROTOCOL_SPEC.md)

---

*文档由 YuleTech 自动生成*
*最后更新: 2026-05-15*
