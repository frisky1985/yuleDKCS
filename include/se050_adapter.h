/******************************************************************************
 * @file    se050_adapter.h
 * @brief   NXP SE050 EdgeLock 安全元件适配器接口
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-09
 *
 * @note    符合 MISRA-C 2012 规范
 *          支持 CCC/ICCE/ICCOA 协议的加密操作
 ******************************************************************************/

#ifndef SE050_ADAPTER_H
#define SE050_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "dkcs.h"

/******************************************************************************
 * SE050 特定错误码
 ******************************************************************************/
#define SE050_OK                    (0x0000)
#define SE050_ERR_COMM              (0xF001)
#define SE050_ERR_PARAM             (0xF002)
#define SE050_ERR_MEMORY            (0xF003)
#define SE050_ERR_CRYPTO            (0xF004)
#define SE050_ERR_KEY_NOT_FOUND     (0xF005)
#define SE050_ERR_AUTH              (0xF006)
#define SE050_ERR_ACCESS_DENIED     (0xF007)
#define SE050_ERR_BUFFER_OVERFLOW   (0xF008)
#define SE050_ERR_NOT_INITIALIZED   (0xF009)
#define SE050_ERR_ALREADY_EXISTS    (0xF00A)

/******************************************************************************
 * SE050 密钥标识符范围
 ******************************************************************************/
#define SE050_MIN_KEY_ID            (0x00000001U)
#define SE050_MAX_KEY_ID            (0x7FFFFFFFU)
#define SE050_KEY_ID_INVALID        (0x00000000U)

/* DKCS 保留密钥 ID 范围 */
#define SE050_KEY_ID_DKCS_BASE      (0x7F000000U)
#define SE050_KEY_ID_ECC_PAIR_BASE  (0x7F000010U)
#define SE050_KEY_ID_AES_BASE       (0x7F000100U)
#define SE050_KEY_ID_HMAC_BASE      (0x7F000200U)
#define SE050_KEY_ID_CERT_BASE      (0x7F000300U)

/******************************************************************************
 * 密钥类型定义
 ******************************************************************************/
typedef enum {
    SE050_KEY_TYPE_ECC_P256 = 0,      /**< NIST P-256 曲线 */
    SE050_KEY_TYPE_ECC_P384,          /**< NIST P-384 曲线 */
    SE050_KEY_TYPE_AES_128,           /**< AES-128 */
    SE050_KEY_TYPE_AES_256,           /**< AES-256 */
    SE050_KEY_TYPE_HMAC_SHA256,       /**< HMAC-SHA256 */
    SE050_KEY_TYPE_HMAC_SHA384,       /**< HMAC-SHA384 */
    SE050_KEY_TYPE_RAW,               /**< 原始数据 */
    SE050_KEY_TYPE_MAX
} se050_key_type_t;

/******************************************************************************
 * 密钥策略/权限定义
 ******************************************************************************/
typedef enum {
    SE050_PERM_SIGN = (1U << 0),      /**< 允许签名 */
    SE050_PERM_VERIFY = (1U << 1),    /**< 允许验签 */
    SE050_PERM_ENCRYPT = (1U << 2),   /**< 允许加密 */
    SE050_PERM_DECRYPT = (1U << 3),   /**< 允许解密 */
    SE050_PERM_DERIVE = (1U << 4),    /**< 允许密钥派生 */
    SE050_PERM_DELETE = (1U << 5),    /**< 允许删除 */
    SE050_PERM_EXPORT = (1U << 6),    /**< 允许导出 (仅受保护) */
    SE050_PERM_IMPORT = (1U << 7)     /**< 允许导入 */
} se050_key_permission_t;

/******************************************************************************
 * 生命周期状态
 ******************************************************************************/
typedef enum {
    SE050_LC_INIT = 0,                /**< 初始状态 */
    SE050_LC_OP,                      /**< 操作状态 */
    SE050_LC_TP,                      /**< 可信配置状态 */
    SE050_LC_LOCKED                   /**< 锁定状态 */
} se050_lifecycle_t;

/******************************************************************************
 * 会话配置
 ******************************************************************************/
typedef struct {
    uint32_t i2c_bus;                 /**< I2C 总线号 */
    uint8_t i2c_address;              /**< SE050 I2C 地址 (通常为 0x48) */
    uint32_t timeout_ms;              /**< 通信超时 (毫秒) */
    bool use_fips_mode;               /**< FIPS 模式使能 */
    bool enable_session_resume;       /**< 会话恢复使能 */
} se050_config_t;

/******************************************************************************
 * 密钥属性结构
 ******************************************************************************/
typedef struct {
    uint32_t key_id;
    se050_key_type_t type;
    uint16_t permissions;
    uint32_t access_counter;
    bool persistent;                  /**< 是否持久化存储 */
    uint32_t valid_from;              /**< 生效时间戳 */
    uint32_t valid_until;             /**< 过期时间戳 */
} se050_key_attr_t;

/******************************************************************************
 * 密钥信息结构
 ******************************************************************************/
typedef struct {
    uint32_t key_id;
    se050_key_type_t type;
    uint8_t public_key[ECC_P256_PUB_KEY_LEN];
    size_t pub_key_len;
    uint16_t permissions;
    bool exists;
} se050_key_info_t;

/******************************************************************************
 * 加密上下文结构
 ******************************************************************************/
typedef struct {
    uint32_t session_id;
    uint32_t key_id;
    uint8_t iv[AES_IV_LEN];
    size_t iv_len;
    uint8_t aad[32];
    size_t aad_len;
} se050_crypto_ctx_t;

/******************************************************************************
 * 初始化与反初始化
 ******************************************************************************/

/**
 * @brief 初始化 SE050 适配器
 * @param[in] config SE050 配置参数
 * @return OK 成功，其他错误码
 */
error_t se050_init(const se050_config_t *config);

/**
 * @brief 反初始化 SE050 适配器
 * @return OK 成功，其他错误码
 */
error_t se050_deinit(void);

/**
 * @brief 检查 SE050 是否已初始化
 * @return true 已初始化，false 未初始化
 */
bool se050_is_initialized(void);

/**
 * @brief 获取 SE050 芯片信息
 * @param[out] version 版本字符串缓冲区
 * @param[in,out] version_len 缓冲区长度/实际长度
 * @param[out] uid 唯一标识符缓冲区
 * @param[in,out] uid_len 缓冲区长度/实际长度
 * @return OK 成功，其他错误码
 */
error_t se050_get_info(
    uint8_t *version,
    size_t *version_len,
    uint8_t *uid,
    size_t *uid_len
);

/******************************************************************************
 * 密钥管理接口
 ******************************************************************************/

/**
 * @brief 生成 ECC 密钥对
 * @param[in] key_id 密钥标识符
 * @param[in] type 密钥类型 (仅支持 ECC_P256/ECC_P384)
 * @param[in] permissions 密钥权限
 * @param[in] persistent 是否持久化存储
 * @return OK 成功，其他错误码
 */
error_t se050_generate_ecc_keypair(
    uint32_t key_id,
    se050_key_type_t type,
    uint16_t permissions,
    bool persistent
);

/**
 * @brief 生成 AES 密钥
 * @param[in] key_id 密钥标识符
 * @param[in] type 密钥类型 (AES_128/AES_256)
 * @param[in] permissions 密钥权限
 * @param[in] persistent 是否持久化存储
 * @return OK 成功，其他错误码
 */
error_t se050_generate_aes_key(
    uint32_t key_id,
    se050_key_type_t type,
    uint16_t permissions,
    bool persistent
);

/**
 * @brief 导入外部密钥到 SE050
 * @param[in] key_id 密钥标识符
 * @param[in] key_data 密钥数据
 * @param[in] key_len 密钥长度
 * @param[in] type 密钥类型
 * @param[in] permissions 密钥权限
 * @param[in] persistent 是否持久化存储
 * @return OK 成功，其他错误码
 */
error_t se050_import_key(
    uint32_t key_id,
    const uint8_t *key_data,
    size_t key_len,
    se050_key_type_t type,
    uint16_t permissions,
    bool persistent
);

/**
 * @brief 导出公钥
 * @param[in] key_id 密钥标识符
 * @param[out] pub_key 公钥缓冲区
 * @param[in,out] pub_key_len 缓冲区长度/实际长度
 * @return OK 成功，其他错误码
 */
error_t se050_export_public_key(
    uint32_t key_id,
    uint8_t *pub_key,
    size_t *pub_key_len
);

/**
 * @brief 删除密钥
 * @param[in] key_id 密钥标识符
 * @return OK 成功，其他错误码
 */
error_t se050_delete_key(uint32_t key_id);

/**
 * @brief 检查密钥是否存在
 * @param[in] key_id 密钥标识符
 * @param[out] exists 是否存在
 * @param[out] key_info 密钥信息 (可为 NULL)
 * @return OK 成功，其他错误码
 */
error_t se050_key_exists(
    uint32_t key_id,
    bool *exists,
    se050_key_info_t *key_info
);

/**
 * @brief 获取密钥属性
 * @param[in] key_id 密钥标识符
 * @param[out] attr 密钥属性结构
 * @return OK 成功，其他错误码
 */
error_t se050_get_key_attr(uint32_t key_id, se050_key_attr_t *attr);

/******************************************************************************
 * ECC 加密操作接口
 ******************************************************************************/

/**
 * @brief ECDH 密钥协商
 * @param[in] key_id 本地私钥标识符
 * @param[in] peer_pub_key 对端公钥
 * @param[in] peer_pub_key_len 对端公钥长度
 * @param[out] shared_secret 共享密钥输出
 * @param[in,out] shared_secret_len 缓冲区长度/实际长度
 * @return OK 成功，其他错误码
 */
error_t se050_ecdh(
    uint32_t key_id,
    const uint8_t *peer_pub_key,
    size_t peer_pub_key_len,
    uint8_t *shared_secret,
    size_t *shared_secret_len
);

/**
 * @brief ECDSA 签名
 * @param[in] key_id 签名私钥标识符
 * @param[in] digest 消息摘要
 * @param[in] digest_len 摘要长度
 * @param[out] signature 签名输出 (R|S 格式)
 * @param[in,out] sig_len 缓冲区长度/实际长度
 * @return OK 成功，其他错误码
 */
error_t se050_ecdsa_sign(
    uint32_t key_id,
    const uint8_t *digest,
    size_t digest_len,
    uint8_t *signature,
    size_t *sig_len
);

/**
 * @brief ECDSA 验签
 * @param[in] key_id 验签公钥标识符
 * @param[in] digest 消息摘要
 * @param[in] digest_len 摘要长度
 * @param[in] signature 签名数据 (R|S 格式)
 * @param[in] sig_len 签名长度
 * @param[out] result 验签结果 (true=验证通过)
 * @return OK 成功，其他错误码
 */
error_t se050_ecdsa_verify(
    uint32_t key_id,
    const uint8_t *digest,
    size_t digest_len,
    const uint8_t *signature,
    size_t sig_len,
    bool *result
);

/******************************************************************************
 * AES 加密操作接口
 ******************************************************************************/

/**
 * @brief AES-GCM 加密
 * @param[in] key_id AES 密钥标识符
 * @param[in] plaintext 明文数据
 * @param[in] plaintext_len 明文长度
 * @param[in] iv 初始化向量
 * @param[in] iv_len IV 长度
 * @param[in] aad 附加认证数据 (可为 NULL)
 * @param[in] aad_len AAD 长度
 * @param[out] ciphertext 密文输出
 * @param[in] ciphertext_len 密文缓冲区长度
 * @param[out] tag 认证标签输出
 * @param[in] tag_len 标签长度
 * @return OK 成功，其他错误码
 */
error_t se050_aes_gcm_encrypt(
    uint32_t key_id,
    const uint8_t *plaintext,
    size_t plaintext_len,
    const uint8_t *iv,
    size_t iv_len,
    const uint8_t *aad,
    size_t aad_len,
    uint8_t *ciphertext,
    size_t ciphertext_len,
    uint8_t *tag,
    size_t tag_len
);

/**
 * @brief AES-GCM 解密
 * @param[in] key_id AES 密钥标识符
 * @param[in] ciphertext 密文数据
 * @param[in] ciphertext_len 密文长度
 * @param[in] iv 初始化向量
 * @param[in] iv_len IV 长度
 * @param[in] aad 附加认证数据 (可为 NULL)
 * @param[in] aad_len AAD 长度
 * @param[in] tag 认证标签
 * @param[in] tag_len 标签长度
 * @param[out] plaintext 明文输出
 * @param[in] plaintext_len 明文缓冲区长度
 * @return OK 成功，其他错误码
 */
error_t se050_aes_gcm_decrypt(
    uint32_t key_id,
    const uint8_t *ciphertext,
    size_t ciphertext_len,
    const uint8_t *iv,
    size_t iv_len,
    const uint8_t *aad,
    size_t aad_len,
    const uint8_t *tag,
    size_t tag_len,
    uint8_t *plaintext,
    size_t plaintext_len
);

/**
 * @brief AES-CCM 加密
 * @param[in] key_id AES 密钥标识符
 * @param[in] plaintext 明文数据
 * @param[in] plaintext_len 明文长度
 * @param[in] iv 初始化向量
 * @param[in] iv_len IV 长度
 * @param[in] aad 附加认证数据 (可为 NULL)
 * @param[in] aad_len AAD 长度
 * @param[in] tag_len 标签长度 (4, 6, 8, 10, 12, 14, 16)
 * @param[out] ciphertext 密文输出
 * @param[in] ciphertext_len 密文缓冲区长度
 * @param[out] tag 认证标签输出
 * @return OK 成功，其他错误码
 */
error_t se050_aes_ccm_encrypt(
    uint32_t key_id,
    const uint8_t *plaintext,
    size_t plaintext_len,
    const uint8_t *iv,
    size_t iv_len,
    const uint8_t *aad,
    size_t aad_len,
    size_t tag_len,
    uint8_t *ciphertext,
    size_t ciphertext_len,
    uint8_t *tag
);

/**
 * @brief AES-CCM 解密
 * @param[in] key_id AES 密钥标识符
 * @param[in] ciphertext 密文数据
 * @param[in] ciphertext_len 密文长度
 * @param[in] iv 初始化向量
 * @param[in] iv_len IV 长度
 * @param[in] aad 附加认证数据 (可为 NULL)
    * @param[in] aad_len AAD 长度
 * @param[in] tag 认证标签
 * @param[in] tag_len 标签长度
 * @param[out] plaintext 明文输出
 * @param[in] plaintext_len 明文缓冲区长度
 * @return OK 成功，其他错误码
 */
error_t se050_aes_ccm_decrypt(
    uint32_t key_id,
    const uint8_t *ciphertext,
    size_t ciphertext_len,
    const uint8_t *iv,
    size_t iv_len,
    const uint8_t *aad,
    size_t aad_len,
    const uint8_t *tag,
    size_t tag_len,
    uint8_t *plaintext,
    size_t plaintext_len
);

/******************************************************************************
 * HMAC 和哈希操作接口
 ******************************************************************************/

/**
 * @brief HMAC 计算
 * @param[in] key_id HMAC 密钥标识符
 * @param[in] data 输入数据
 * @param[in] data_len 数据长度
 * @param[out] hmac 输出 HMAC
 * @param[in,out] hmac_len 缓冲区长度/实际长度
 * @return OK 成功，其他错误码
 */
error_t se050_hmac(
    uint32_t key_id,
    const uint8_t *data,
    size_t data_len,
    uint8_t *hmac,
    size_t *hmac_len
);

/******************************************************************************
 * 随机数生成接口
 ******************************************************************************/

/**
 * @brief 生成随机数
 * @param[out] random 随机数缓冲区
 * @param[in] len 请求长度
 * @return OK 成功，其他错误码
 */
error_t se050_generate_random(uint8_t *random, size_t len);

/******************************************************************************
 * 安全计数器和防回滚
 ******************************************************************************/

/**
 * @brief 增加安全计数器
 * @param[in] counter_id 计数器 ID
 * @param[out] new_value 新计数值
 * @return OK 成功，其他错误码
 */
error_t se050_increment_counter(uint32_t counter_id, uint32_t *new_value);

/**
 * @brief 读取安全计数器
 * @param[in] counter_id 计数器 ID
 * @param[out] value 当前计数值
 * @return OK 成功，其他错误码
 */
error_t se050_read_counter(uint32_t counter_id, uint32_t *value);

/******************************************************************************
 * 与 DKCS 协议集成接口
 ******************************************************************************/

/**
 * @brief 为 DKCS 协议初始化密钥槽
 * @param[in] key_type 密钥类型
 * @param[out] key_id 分配的密钥 ID
 * @return OK 成功，其他错误码
 */
error_t se050_dkcs_allocate_key_slot(se050_key_type_t key_type, uint32_t *key_id);

/**
 * @brief 执行 DKCS 协议需要的 ECDH 密钥派生
 * @param[in] local_key_id 本地密钥 ID
 * @param[in] peer_pub_key 对端公钥
 * @param[in] peer_pub_key_len 对端公钥长度
 * @param[in] context_info 上下文信息
 * @param[in] context_info_len 上下文信息长度
 * @param[out] derived_key 派生密钥
 * @param[in] derived_key_len 派生密钥长度
 * @return OK 成功，其他错误码
 */
error_t se050_dkcs_derive_key(
    uint32_t local_key_id,
    const uint8_t *peer_pub_key,
    size_t peer_pub_key_len,
    const uint8_t *context_info,
    size_t context_info_len,
    uint8_t *derived_key,
    size_t derived_key_len
);

/**
 * @brief 执行 DKCS 协议的认证签名
 * @param[in] key_id 签名密钥 ID
 * @param[in] auth_data 认证数据
 * @param[in] auth_data_len 认证数据长度
 * @param[out] signature 签名输出
 * @param[in,out] sig_len 签名长度
 * @return OK 成功，其他错误码
 */
error_t se050_dkcs_auth_sign(
    uint32_t key_id,
    const uint8_t *auth_data,
    size_t auth_data_len,
    uint8_t *signature,
    size_t *sig_len
);

/**
 * @brief 备份密钥到安全存储
 * @param[in] key_id 要备份的密钥 ID
 * @param[out] backup_data 备份数据
 * @param[in,out] backup_data_len 备份数据长度
 * @return OK 成功，其他错误码
 */
error_t se050_backup_key(
    uint32_t key_id,
    uint8_t *backup_data,
    size_t *backup_data_len
);

/**
 * @brief 从备份恢复密钥
 * @param[in] key_id 目标密钥 ID
 * @param[in] backup_data 备份数据
 * @param[in] backup_data_len 备份数据长度
 * @return OK 成功，其他错误码
 */
error_t se050_restore_key(
    uint32_t key_id,
    const uint8_t *backup_data,
    size_t backup_data_len
);

#ifdef __cplusplus
}
#endif

#endif /* SE050_ADAPTER_H */
