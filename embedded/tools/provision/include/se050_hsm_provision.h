/**
 * @file se050_hsm_provision.h
 * @brief SE050 产线 HSM 密钥派生工具接口（主机侧 / 注入工位）
 *
 * ## 定位
 *
 * 本模块定义产线注入工位与 HSM 之间的密钥派生契约：
 *
 *   HSM (MK)  ──derive──▶  K-ENC / K-MAC / K-RMAC  ──PUT KEY──▶  SE050
 *
 * 它**不**属于车端量产固件：不参与 `ccc_protocol` 固件构建，只链接到产线
 * 工具与单元测试。车端固件只接收派生结果，绝不持有 MK。
 *
 * ## 派生契约（与 docs/security/se050-provision-guide.md v1.1.0 一致）
 *
 *   input  = 0x01 || key_id || UID(18B) || key_version(1B)
 *   key_id = 0x01 K-ENC / 0x02 K-MAC / 0x03 K-RMAC
 *   K_i    = AES-CMAC(MK, input)          (输出 16 字节)
 *
 * 使用 CMAC 而非 ECB，输入长度不受 16 字节限制，且域标签提供算法隔离。
 *
 * ## 安全约束
 *
 * - MK 只在 HSM 内（生产）或进程内存（仅软件开发后端）。
 * - 软件后端必须通过 `hsm_config_t.allow_software = 1` 显式启用，防止误用。
 * - 派生结果不得写入日志、数据库或磁盘；产线数据库只保存指纹。
 * - 调用方必须在用完后调用 `hsm_provision_key_set_clear()`。
 */

#ifndef SE050_HSM_PROVISION_H
#define SE050_HSM_PROVISION_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** SE050 芯片唯一标识长度 */
#define HSM_PROV_UID_LEN        18U
/** SCP03 传输密钥长度（AES-128） */
#define HSM_PROV_KEY_LEN        16U
/** 指纹长度（SHA-256） */
#define HSM_PROV_FINGERPRINT_LEN 32U
/** MK 最大长度（AES-256） */
#define HSM_PROV_MK_MAX_LEN     32U
/** 允许注入的最小密钥版本（0 为出厂默认，禁止作为产线版本） */
#define HSM_PROV_KEY_VERSION_MIN 1U

/** 状态码 */
typedef enum {
    HSM_PROV_OK            =  0,
    HSM_PROV_ERR_PARAM     = -1,  /**< 参数非法（NULL/长度/全零 UID） */
    HSM_PROV_ERR_STATE     = -2,  /**< 上下文未初始化 */
    HSM_PROV_ERR_DENIED    = -3,  /**< 软件后端未显式授权 */
    HSM_PROV_ERR_UNAVAILABLE = -4,/**< 后端不可用（PKCS#11 未接入） */
    HSM_PROV_ERR_CRYPTO    = -5   /**< 派生/哈希内部失败 */
} hsm_prov_status_e;

/** HSM 后端类型 */
typedef enum {
    HSM_PROV_BACKEND_SOFTWARE = 0, /**< 仅开发/测试：MK 在进程内存 */
    HSM_PROV_BACKEND_PKCS11   = 1  /**< 生产：MK 不出 HSM（待接入） */
} hsm_prov_backend_e;

/** 派生结果：一组 SCP03 传输密钥 */
typedef struct {
    uint8_t k_enc[HSM_PROV_KEY_LEN];  /**< K-ENC */
    uint8_t k_mac[HSM_PROV_KEY_LEN];  /**< K-MAC */
    uint8_t k_rmac[HSM_PROV_KEY_LEN]; /**< K-RMAC */
    uint8_t key_version;              /**< 与派生输入一致的版本号 */
} hsm_prov_key_set_t;

/**
 * @brief PKCS#11 后端适配层（由调用方注入，避免依赖 pkcs11.h / dlopen）。
 *
 * 厂商适配器内部可用 C_SignInit(CKM_AES_CMAC) + C_Sign 实现 @ref aes_cmac，
 * 保证 MK 永不出 HSM。本模块只持有指针，不负责会话生命周期。
 */
typedef struct {
    void *session;   /* 调用方持有的 HSM 会话，不透明 */
    int (*aes_cmac)(void *session, const char *key_label,
                    const uint8_t *data, size_t len, uint8_t out[16]);
    /* 返回 0 成功，非 0 失败 */
} hsm_prov_pkcs11_ops_t;

/** 初始化配置 */
typedef struct {
    hsm_prov_backend_e backend;
    const uint8_t     *mk;             /**< 仅 SOFTWARE 后端使用 */
    size_t             mk_len;         /**< 16 / 24 / 32；生产推荐 32 */
    uint8_t            allow_software; /**< SOFTWARE 后端必须置 1 */
    const hsm_prov_pkcs11_ops_t *pkcs11; /**< 仅 PKCS11 后端使用；不得为 NULL */
} hsm_prov_config_t;

/**
 * @brief 调用方持有的上下文。
 *
 * 公开结构以便无动态分配；调用方不得拷贝或打印本结构。
 * 字段只由本模块写入。
 */
typedef struct {
    hsm_prov_backend_e backend;
    uint8_t            mk[HSM_PROV_MK_MAX_LEN];
    uint8_t            mk_len;
    uintptr_t          session; /**< 预留给 PKCS#11 会话句柄 */
    const hsm_prov_pkcs11_ops_t *pkcs11; /**< PKCS11 后端 vtable（不拥有会话） */
    uint8_t            ready;
} hsm_prov_ctx_t;

/**
 * @brief 初始化上下文。
 *
 * - SOFTWARE：要求 @p cfg->allow_software == 1，mk_len ∈ {16,24,32}。
 * - PKCS11：要求 @p cfg->pkcs11 及其 aes_cmac/session 均非 NULL；
 *   不要求 mk，MK 由 HSM 内部持有。
 *
 * @return HSM_PROV_OK 或错误码。失败时上下文保持未初始化。
 */
hsm_prov_status_e hsm_provision_init(hsm_prov_ctx_t *ctx,
                                     const hsm_prov_config_t *cfg);

/**
 * @brief 清除并销毁上下文（安全清零 MK 与句柄）。
 */
void hsm_provision_deinit(hsm_prov_ctx_t *ctx);

/**
 * @brief 从 UID 派生一组 SCP03 传输密钥。
 *
 * @param ctx          已初始化上下文
 * @param uid          18 字节芯片 UID，禁止全零
 * @param key_version  目标密钥版本，必须 >= HSM_PROV_KEY_VERSION_MIN
 * @param out          输出密钥组；失败时被清零
 *
 * @return HSM_PROV_OK 或错误码。
 */
hsm_prov_status_e hsm_provision_derive_transport(
    const hsm_prov_ctx_t *ctx,
    const uint8_t uid[HSM_PROV_UID_LEN],
    uint8_t key_version,
    hsm_prov_key_set_t *out);

/**
 * @brief 计算密钥组指纹，供产线数据库登记。
 *
 *   fingerprint = SHA-256("YULE-DK-TRANSPORT-V1" || UID || K-ENC || K-MAC || K-RMAC)
 *
 * 指纹不写入密钥明文，仅用于核对“同一组密钥是否已注入”。
 */
hsm_prov_status_e hsm_provision_fingerprint(
    const uint8_t uid[HSM_PROV_UID_LEN],
    const hsm_prov_key_set_t *set,
    uint8_t out[HSM_PROV_FINGERPRINT_LEN]);

/**
 * @brief 安全清零密钥组。
 */
void hsm_provision_key_set_clear(hsm_prov_key_set_t *set);

#ifdef __cplusplus
}
#endif

#endif /* SE050_HSM_PROVISION_H */
