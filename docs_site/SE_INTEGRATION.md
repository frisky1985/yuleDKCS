# SE 整合指南

## 支持的安全元件

### 嵌入式 SE (eSE)

**支持芯片**:
- NXP JCOP 5/6/7
- Samsung S3D350A
- STMicroelectronics ST33
- 华大电子 CIU98

**特点**:
- 最高安全等级 (EAL5+)
- 独立硬件防护
- 成本较高

### SIM 卡 SE

**支持标准**:
- GlobalPlatform 2.3+
- UICC 接口

**特点**:
- 利用现有硬件
- 运营商控制
- 可移植性强

### TEE (可信执行环境)

**支持实现**:
- ARM TrustZone
- Qualcomm SPU
- Huawei iTrustee

**特点**:
- 软硬件结合
- 性能优越
- 安全等级略低

## SE 接口实现

### CCC SE 接口示例

```c
#include "ccc.h"

/* NXP JCOP 实现示例 */
static error_t nxp_jcop_init(void)
{
    /* 初始化 T=1 通信 */
    /* 选择 CCC Applet */
    return OK;
}

static error_t nxp_jcop_generate_key_pair(uint8_t *pub, uint8_t *priv)
{
    /* 使用 JCOP Crypto API 生成 P-256 密钥对 */
    /* 私钥保存在 SE 内部 */
    return OK;
}

static error_t nxp_jcop_sign(const uint8_t *data, size_t len,
                                   const uint8_t *priv_key,
                                   uint8_t *signature)
{
    /* 使用 SE 内部私钥进行签名 */
    /* 私钥参数通常是密钥引用 */
    return OK;
}

static ccc_se_interface_t nxp_jcop_interface = {
    .init = nxp_jcop_init,
    .deinit = nxp_jcop_deinit,
    .generate_key_pair = nxp_jcop_generate_key_pair,
    .sign = nxp_jcop_sign,
    .verify = nxp_jcop_verify,
    .derive_shared_secret = nxp_jcop_derive,
    .secure_store = nxp_jcop_store,
    .secure_read = nxp_jcop_read,
};
```

### ICCE SE 接口示例 (国产 SE)

```c
#include "icce.h"

/* 华大 CIU98 实现示例 */
static error_t ciu98_open_channel(uint8_t *atr, size_t *atr_len)
{
    /* 打开接触式卡通信通道 */
    return OK;
}

static error_t ciu98_generate_sm2_key(uint8_t *pub, uint8_t *priv)
{
    /* 使用国密算法生成 SM2 密钥对 */
    return OK;
}

static error_t ciu98_sm2_sign(const uint8_t *data, size_t len,
                                    uint8_t *signature)
{
    /* SM2 签名 */
    return OK;
}

static error_t ciu98_sm4_encrypt(const uint8_t *key, const uint8_t *iv,
                                       const uint8_t *plain, size_t len,
                                       uint8_t *cipher)
{
    /* SM4 CBC 加密 */
    return OK;
}

static icce_se_interface_t ciu98_interface = {
    .open_channel = ciu98_open_channel,
    .close_channel = ciu98_close_channel,
    .transmit = ciu98_transmit,
    .select_applet = ciu98_select_applet,
    .verify_pin = ciu98_verify_pin,
    .generate_sm2_key = ciu98_generate_sm2_key,
    .sm2_sign = ciu98_sm2_sign,
    .sm2_verify = ciu98_sm2_verify,
    .sm4_encrypt = ciu98_sm4_encrypt,
    .sm4_decrypt = ciu98_sm4_decrypt,
};
```

## 整合步骤

### 1. 硬件准备

- 确认 SE 芯片已焊接
- 确认天线/接口正确连接
- 测试基本通信

### 2. 软件配置

```c
/* 选择 SE 类型 */
session_config_t config = {
    .se_type = SE_ESE,  // 或 SE_TEE, SE_SIM
    // ...
};

/* 初始化 */
dkcs_init(&config);
```

### 3. 证书导入

```c
/* 导入设备证书到 SE */
error_t import_device_cert(const uint8_t *cert_data, size_t cert_len)
{
    /* 使用 SE 的安全存储功能 */
    return se_interface.secure_store(CERT_SLOT_DEVICE, cert_data, cert_len);
}
```

### 4. 测试验证

- 密钥生成测试
- 签名/验证测试
- 加解密测试
- 完整配对流程测试

## 故障排除

### 常见问题

| 问题 | 可能原因 | 解决方案 |
|-----|---------|---------|
| SE 无响应 | 通信线路问题 | 检查 I/O 连接 |
| 认证失败 | 证书错误 | 重新导入证书 |
| 签名失败 | 密钥不存在 | 检查密钥索引 |
| 性能低下 | 频率设置不当 | 调整 SPI/I2C 速率 |

### 调试方法

```c
/* 启用调试日志 */
#define DEBUG_LEVEL 3

/* 追踪 SE 命令 */
void trace_apdu(const uint8_t *apdu, size_t len)
{
    printf("APDU: ");
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", apdu[i]);
    }
    printf("\n");
}
```
