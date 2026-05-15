# CCC 配对证书 X.509 序列化实现

## 概述

本模块实现了 CCC Digital Key R3 配对过程中使用的 X.509 证书序列化和解析功能，解决了证书长度返回 0 的问题。

## 文件说明

| 文件 | 说明 |
|------|------|
| `ccc_certificate.c` | X.509 证书序列化/解析核心实现 |
| `ccc_certificate.h` | 头文件，定义所有公开接口 |

## 实现的功能

### 1. 证书序列化

```c
error_t ccc_serialize_certificate(const ccc_certificate_t *cert, uint8_t *out, size_t *out_len);
```

将 CCC 证书结构序列化为标准 X.509 DER 格式，包含:

- **版本号**: X.509 v3 (Context-Specific [0])
- **序列号**: 16字节唯一标识
- **签名算法**: ECDSA with SHA-256 (OID: 1.2.840.10045.4.3.2)
- **颁发者**: 使用 subject_id 构建的 X.500 Name
- **有效期**: UTCTime 格式 (YYMMDDHHMMSSZ)
- **主体**: 使用 subject_id 构建的 X.500 Name
- **主体公钥信息**: EC P-256 公钥 (未压缩格式)
- **扩展字段**:
  - Subject Key Identifier
  - Key Usage (digitalSignature)
- **签名**: ECDSA 签名 (r || s, 各 32 字节)

### 2. 证书解析

```c
error_t ccc_parse_certificate(const uint8_t *data, size_t data_len, ccc_certificate_t *cert);
```

从 X.509 DER 格式解析证书，提取以下字段:

- 序列号
- 颁发者 ID
- 主体 ID
- 有效期范围
- 公钥
- 签名

### 3. 证书验证

```c
error_t ccc_verify_certificate(const ccc_certificate_t *cert, const uint8_t *trusted_pubkey);
```

验证证书的有效性:

- 证书有效期检查
- 签名验证
- 公钥格式验证

### 4. 证书链验证

```c
error_t ccc_validate_cert_chain(const ccc_cert_chain_t *cert_chain, const ccc_certificate_t *trusted_root);
```

从根证书开始逐级验证证书链。

### 5. 证书长度计算

```c
size_t ccc_get_certificate_length(const ccc_certificate_t *cert);
```

计算证书序列化后的 DER 长度 (用于动态分配缓冲区)。

### 6. 自签名证书生成 (用于测试)

```c
error_t ccc_generate_self_signed_certificate(
    ccc_cert_type_t cert_type,
    const uint8_t *subject_id,
    const uint8_t *key_pair,
    uint32_t validity_days,
    ccc_certificate_t *cert
);
```

生成用于测试的自签名证书。

## 技术细节

### ASN.1 DER 编码

实现了完整的 ASN.1 DER 编码支持:

| ASN.1 类型 | 功能 |
|----------|------|
| INTEGER | 大整数编码，支持前导零处理 |
| BIT STRING | 位字符串，包含未使用位数 |
| OCTET STRING | 八进制字符串 |
| OBJECT IDENTIFIER | OID 编码 |
| SEQUENCE | 序列结构 |
| UTCTime | UTC 时间格式 |
| Context-Specific | 上下文特定标签 |

### 签名算法

- **Algorithm**: ECDSA with SHA-256
- **OID**: 1.2.840.10045.4.3.2
- **Curve**: P-256 (prime256v1)
- **Signature Format**: r || s (各 32 字节)

### 证书大小

典型的 CCC 证书序列化后约 **321 字节** (DER 格式)。

## 修复的问题

### 原问题
- X.509 证书长度返回 0
- 配对过程失败

### 解决方案
1. 实现完整的 X.509 DER 序列化
2. 实现完整的 ASN.1 编码
3. 支持标准证书结构
4. 集成到配对流程

## 使用示例

```c
/* 创建证书 */
ccc_certificate_t cert;
memset(&cert, 0, sizeof(cert));
cert.type = CCC_CERT_TYPE_DEVICE;
memcpy(cert.serial_number, device_id, 16);
memcpy(cert.subject_id, device_id, 16);
memcpy(cert.issuer_id, ca_id, 16);
cert.valid_from = time(NULL);
cert.valid_until = cert.valid_from + 365 * 24 * 3600;
cert.public_key[0] = 0x04;  /* 未压缩格式 */
memcpy(&cert.public_key[1], pub_key_x, 32);
memcpy(&cert.public_key[33], pub_key_y, 32);
memcpy(cert.signature, signature, 64);

/* 序列化 */
uint8_t der_buf[1024];
size_t der_len = sizeof(der_buf);
error_t ret = ccc_serialize_certificate(&cert, der_buf, &der_len);
if (ret == OK && der_len > 0) {
    printf("证书长度: %zu bytes\n", der_len);
}

/* 解析 */
ccc_certificate_t parsed;
ret = ccc_parse_certificate(der_buf, der_len, &parsed);

/* 验证 */
ret = ccc_verify_certificate(&parsed, trusted_root_pubkey);
```

## 测试

运行测试套件:

```bash
cd /home/admin/yuleDKCS
gcc -I include -I embedded/sdk/src/protocol -I src \
    tests/test_ccc_certificate.c \
    embedded/sdk/src/protocol/ccc_certificate.c \
    -o test_ccc_crypto -std=c99
./test_ccc_crypto
```

测试结果:
- 证书序列化: ✓ 通过
- 证书解析: ✓ 通过
- 长度验证: ✓ 通过 (长度 > 0)
- 自签名证书: ✓ 通过
- 证书链: ✓ 通过

## 集成到配对流程

已更新 `ccc_core.c`，在配对请求和确认消息中使用实际的 X.509 证书:

1. `ccc_start_pairing()`: 包含设备证书
2. `ccc_complete_pairing()`: 包含证书链

## 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| 1.0.0 | 2026-05-09 | 初始实现，完成 X.509 DER 序列化/解析 |
