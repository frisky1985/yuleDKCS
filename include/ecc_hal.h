/******************************************************************************
 * @file    ecc_hal.h
 * @brief   ECC 硬件加速抽象层 - 支持 P-256 曲线
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-09
 * @note    符合 FIPS 140-2 Level 2 要求
 ******************************************************************************/

#ifndef ECC_HAL_H
#define ECC_HAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "dkcs.h"

/******************************************************************************
 * 版本信息
 ******************************************************************************/
#define ECC_HAL_VERSION_MAJOR   1
#define ECC_HAL_VERSION_MINOR   0
#define ECC_HAL_VERSION_PATCH   0
#define ECC_HAL_VERSION_STRING  "1.0.0"

/******************************************************************************
 * ECC 曲线定义
 ******************************************************************************/
typedef enum {
    ECC_CURVE_P256 = 0,         /**< NIST P-256 / secp256r1 */
    ECC_CURVE_P384 = 1,         /**< NIST P-384 / secp384r1 (保留) */
    ECC_CURVE_P521 = 2,         /**< NIST P-521 / secp521r1 (保留) */
    ECC_CURVE_SM2 = 3,          /**< 国密 SM2 (ICCE 使用) */
    ECC_CURVE_MAX
} ecc_curve_t;

/******************************************************************************
 * ECC 后端类型
 ******************************************************************************/
typedef enum {
    ECC_BACKEND_SE050 = 0,      /**< NXP SE050 安全芯片 */
    ECC_BACKEND_CRYPTCELL = 1,  /**< ARM CryptoCell (如可用) */
    ECC_BACKEND_MBEDTLS = 2,    /**< 软件 mbedtls 实现 (后备) */
    ECC_BACKEND_TEE = 3,        /**< Trusted Execution Environment */
    ECC_BACKEND_MAX
} ecc_backend_t;

/******************************************************************************
 * 密钥句柄类型 - 不透明句柄，不直接暴露密钥
 ******************************************************************************/
typedef uint32_t ecc_key_handle_t;
#define ECC_INVALID_KEY_HANDLE  0xFFFFFFFFU

/******************************************************************************
 * 密钥属性标志 (FIPS 140-2 L2 要求)
 ******************************************************************************/
#define ECC_KEY_FLAG_NONE           0x0000U
#define ECC_KEY_FLAG_PERSISTENT     0x0001U  /**< 持久化存储 */
#define ECC_KEY_FLAG_EXPORTABLE     0x0002U  /**< 可导出 (不推荐) */
#define ECC_KEY_FLAG_SENSITIVE      0x0004U  /**< 敏感密钥 */
#define ECC_KEY_FLAG_USAGE_SIGN     0x0010U  /**< 可用于签名 */
#define ECC_KEY_FLAG_USAGE_VERIFY   0x0020U  /**< 可用于验签 */
#define ECC_KEY_FLAG_USAGE_ECDH     0x0040U  /**< 可用于 ECDH */
#define ECC_KEY_FLAG_HARDWARE       0x0100U  /**< 硬件安全密钥 */
#define ECC_KEY_FLAG_FIPS_MODE      0x0200U  /**< FIPS 模式生成 */

/******************************************************************************
 * 密钥信息结构 (零拷贝设计)
 ******************************************************************************/
typedef struct {
    ecc_key_handle_t handle;
    ecc_curve_t curve;
    uint16_t flags;
    uint16_t key_size;          /**< 密钥长度 (字节) */
    uint32_t generation_date;   /**< 生成时间戳 */
    uint8_t reserved[8];        /**< 保留字段 */
} ecc_key_info_t;

/******************************************************************************
 * ECDSA 签名结构
 ******************************************************************************/
typedef struct {
    uint8_t r[ECC_P256_KEY_LEN];    /**< 签名 R 分量 */
    uint8_t s[ECC_P256_KEY_LEN];    /**< 签名 S 分量 */
} ecdsa_signature_t;

/******************************************************************************
 * ECDH 共享密钥结构 (敏感数据 - 内存保护)
 ******************************************************************************/
typedef struct {
    uint8_t key[ECC_P256_KEY_LEN];  /**< 共享密钥 (32字节) */
    uint32_t validity;              /**< 有效时间戳 */
} ecdh_shared_secret_t;

/******************************************************************************
 * 硬件功能检测结构
 ******************************************************************************/
typedef struct {
    bool p256_supported;
    bool p384_supported;
    bool sm2_supported;
    bool ecdh_supported;
    bool ecdsa_sign_supported;
    bool ecdsa_verify_supported;
    bool hardware_protection;
    bool side_channel_resistant;
    uint32_t max_key_count;
    char backend_name[32];
} ecc_capabilities_t;

#ifndef ENABLE_THREAD_SAFETY
#define ENABLE_THREAD_SAFETY 1
#endif

/******************************************************************************
 * ECC HAL 初始化配置
 ******************************************************************************/
typedef struct {
    ecc_backend_t preferred_backend;
    bool force_software_fallback;
    bool enable_fips_mode;
    bool secure_memory_clear;
    void *se050_i2c_handle;     /**< SE050 I2C 句柄 (如适用) */
    void *tee_session;          /**< TEE 会话 (如适用) */
} ecc_hal_config_t;

/******************************************************************************
 * API 函数声明
 ******************************************************************************/

/**
 * @brief 初始化 ECC HAL 层
 * @param config 配置参数
 * @return OK 或错误码
 */
error_t ecc_hal_init(const ecc_hal_config_t *config);

/**
 * @brief 反初始化 ECC HAL 层
 * @return OK 或错误码
 */
error_t ecc_hal_deinit(void);

/**
 * @brief 获取 ECC HAL 版本
 * @return 版本字符串
 */
const char* ecc_hal_get_version(void);

/**
 * @brief 获取硬件能力信息
 * @param caps 输出能力结构
 * @return OK 或错误码
 */
error_t ecc_hal_get_capabilities(ecc_capabilities_t *caps);

/**
 * @brief 获取当前使用的后端类型
 * @return 当前后端类型
 */
ecc_backend_t ecc_hal_get_current_backend(void);

/**
 * @brief 获取后端详细信息
 * @param name 输出后端名称缓冲区
 * @param name_len 名称缓冲区长度
 * @param features 输出特性位图
 * @param hardware_accelerated 输出是否硬件加速
 * @return OK 或错误码
 */
error_t ecc_hal_get_backend_info(char *name, size_t name_len,
                                 uint32_t *features,
                                 bool *hardware_accelerated);

/******************************************************************************
 * 密钥管理 API (FIPS 140-2 L2 密钥句柄抽象)
 ******************************************************************************/

/**
 * @brief 生成 ECC 密钥对 (硬件安全模块内生成)
 * @param curve 椭圆曲线类型
 * @param flags 密钥属性标志
 * @param handle 输出密钥句柄
 * @return OK 或错误码
 * @note 私钥不会离开安全硬件
 */
error_t ecc_hal_generate_keypair(
    ecc_curve_t curve,
    uint16_t flags,
    ecc_key_handle_t *handle
);

/**
 * @brief 导入公钥 (仅公钥可导入)
 * @param curve 椭圆曲线类型
 * @param public_key 公钥数据 (65字节未压缩格式: 0x04 + X + Y)
 * @param key_len 公钥长度
 * @param handle 输出密钥句柄
 * @return OK 或错误码
 */
error_t ecc_hal_import_public_key(
    ecc_curve_t curve,
    const uint8_t *public_key,
    size_t key_len,
    ecc_key_handle_t *handle
);

/**
 * @brief 导出公钥
 * @param handle 密钥句柄
 * @param public_key 输出缓冲区 (65字节)
 * @param key_len 输出长度
 * @return OK 或错误码
 */
error_t ecc_hal_export_public_key(
    ecc_key_handle_t handle,
    uint8_t *public_key,
    size_t *key_len
);

/**
 * @brief 获取密钥信息
 * @param handle 密钥句柄
 * @param info 输出密钥信息
 * @return OK 或错误码
 */
error_t ecc_hal_get_key_info(
    ecc_key_handle_t handle,
    ecc_key_info_t *info
);

/**
 * @brief 删除密钥
 * @param handle 密钥句柄
 * @return OK 或错误码
 * @note 会执行安全擦除
 */
error_t ecc_hal_delete_key(ecc_key_handle_t handle);

/**
 * @brief 检查密钥句柄有效性
 * @param handle 密钥句柄
 * @return true 有效, false 无效
 */
bool ecc_hal_is_valid_key_handle(ecc_key_handle_t handle);

/******************************************************************************
 * ECDH 密钥协商 API
 ******************************************************************************/

/**
 * @brief 执行 ECDH 密钥协商 (零拷贝设计)
 * @param private_key_handle 本地私钥句柄
 * @param public_key_handle 远端公钥句柄
 * @param shared_secret 输出共享密钥 (敏感数据)
 * @return OK 或错误码
 * @note 共享密钥在受保护内存中生成
 */
error_t ecc_hal_ecdh_compute_shared_secret(
    ecc_key_handle_t private_key_handle,
    ecc_key_handle_t public_key_handle,
    ecdh_shared_secret_t *shared_secret
);

/**
 * @brief 使用原始公钥执行 ECDH (用于临时协商)
 * @param private_key_handle 本地私钥句柄
 * @param public_key 远端公钥数据 (65字节)
 * @param pub_key_len 公钥长度
 * @param shared_secret 输出共享密钥
 * @return OK 或错误码
 */
error_t ecc_hal_ecdh_compute_with_raw_pubkey(
    ecc_key_handle_t private_key_handle,
    const uint8_t *public_key,
    size_t pub_key_len,
    ecdh_shared_secret_t *shared_secret
);

/******************************************************************************
 * ECDSA 签名/验签 API
 ******************************************************************************/

/**
 * @brief ECDSA 签名
 * @param private_key_handle 签名私钥句柄
 * @param digest 消息摘要 (32字节)
 * @param digest_len 摘要长度
 * @param signature 输出签名
 * @return OK 或错误码
 */
error_t ecc_hal_ecdsa_sign(
    ecc_key_handle_t private_key_handle,
    const uint8_t *digest,
    size_t digest_len,
    ecdsa_signature_t *signature
);

/**
 * @brief ECDSA 验签
 * @param public_key_handle 验签公钥句柄
 * @param digest 消息摘要
 * @param digest_len 摘要长度
 * @param signature 签名数据
 * @return OK 或错误码
 */
error_t ecc_hal_ecdsa_verify(
    ecc_key_handle_t public_key_handle,
    const uint8_t *digest,
    size_t digest_len,
    const ecdsa_signature_t *signature
);

/**
 * @brief 使用原始公钥验签 (用于证书验证等场景)
 * @param public_key 公钥数据
 * @param pub_key_len 公钥长度
 * @param digest 消息摘要
 * @param digest_len 摘要长度
 * @param signature 签名数据
 * @return OK 或错误码
 */
error_t ecc_hal_ecdsa_verify_raw(
    const uint8_t *public_key,
    size_t pub_key_len,
    const uint8_t *digest,
    size_t digest_len,
    const ecdsa_signature_t *signature
);

/******************************************************************************
 * 便捷 API 函数
 ******************************************************************************/

/**
 * @brief ECDH 密钥协商 (便捷函数)
 * @param private_key_handle 本地私钥句柄
 * @param peer_public_key 对端公钥数据
 * @param peer_pub_len 对端公钥长度
 * @param shared_secret 输出共享密钥
 * @return OK 或错误码
 */
error_t ecc_hal_ecdh(ecc_key_handle_t private_key_handle,
                     const uint8_t *peer_public_key,
                     size_t peer_pub_len,
                     ecdh_shared_secret_t *shared_secret);

/**
 * @brief ECDSA 签名 (便捷函数)
 * @param private_key_handle 签名私钥句柄
 * @param digest 消息摘要
 * @param digest_len 摘要长度
 * @param signature 输出签名
 * @return OK 或错误码
 */
error_t ecc_hal_sign(ecc_key_handle_t private_key_handle,
                     const uint8_t *digest,
                     size_t digest_len,
                     ecdsa_signature_t *signature);

/**
 * @brief ECDSA 验签 (便捷函数)
 * @param public_key_handle 验签公钥句柄
 * @param digest 消息摘要
 * @param digest_len 摘要长度
 * @param signature 签名数据
 * @return OK 或错误码
 */
error_t ecc_hal_verify(ecc_key_handle_t public_key_handle,
                       const uint8_t *digest,
                       size_t digest_len,
                       const ecdsa_signature_t *signature);

/******************************************************************************
 * 工具函数
 ******************************************************************************/

/**
 * @brief 安全内存清零 (防优化)
 * @param ptr 内存指针
 * @param len 长度
 */
void ecc_hal_secure_zero(void *ptr, size_t len);

/**
 * @brief 验证公钥格式
 * @param curve 曲线类型
 * @param public_key 公钥数据
 * @param key_len 长度
 * @return true 有效, false 无效
 */
bool ecc_hal_validate_public_key(
    ecc_curve_t curve,
    const uint8_t *public_key,
    size_t key_len
);

/**
 * @brief 获取曲线密钥长度
 * @param curve 曲线类型
 * @return 密钥长度 (字节), 0 表示不支持
 */
size_t ecc_hal_get_key_length(ecc_curve_t curve);

/**
 * @brief 获取曲线公钥长度
 * @param curve 曲线类型
 * @return 公钥长度 (字节), 0 表示不支持
 */
size_t ecc_hal_get_public_key_length(ecc_curve_t curve);

/******************************************************************************
 * 错误码扩展
 ******************************************************************************/
#define ERROR_ECC_INVALID_CURVE         -100
#define ERROR_ECC_INVALID_KEY_HANDLE    -101
#define ERROR_ECC_KEY_NOT_FOUND         -102
#define ERROR_ECC_KEY_ALREADY_EXISTS    -103
#define ERROR_ECC_KEY_EXPORT_RESTRICTED -104
#define ERROR_ECC_KEY_USAGE_RESTRICTED  -105
#define ERROR_ECC_HARDWARE_FAILURE      -106
#define ERROR_ECC_SIGNATURE_INVALID     -107
#define ERROR_ECC_SELFTEST_FAILED       -108

#ifdef __cplusplus
}
#endif

#endif /* ECC_HAL_H */
