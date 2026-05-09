/******************************************************************************
 * @file    se050_keymgr.h
 * @brief   SE050 密钥管理接口
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-09
 *
 * @note    符合 MISRA-C 2012 规范
 *          提供密钥生命周期管理：生成、导入、导出、删除、轮换
 ******************************************************************************/

#ifndef SE050_KEYMGR_H
#define SE050_KEYMGR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "dkcs.h"
#include "se050_adapter.h"

/******************************************************************************
 * 密钥标识符范围宏定义
 ******************************************************************************/
#define SE050_KEY_ID_MIN            (0x00000001U)   /**< 最小有效密钥ID */
#define SE050_KEY_ID_MAX            (0x7FFFFFFFU)   /**< 最大有效密钥ID */
#define SE050_KEY_ID_INVALID        (0x00000000U)   /**< 无效密钥ID */

/* 便捷类型指示宏 */
#define SE050_KEY_TYPE_ECC_P256     (SE050_KEY_TYPE_ECC_P256)
#define SE050_KEY_TYPE_AES_128      (SE050_KEY_TYPE_AES_128)

/******************************************************************************
 * 密钥类型枚举扩展定义
 * @note 与 se050_adapter.h 中的 se050_key_type_t 保持一致
 ******************************************************************************/
typedef enum {
    SE050_KEYMGR_TYPE_ECC_P256 = 0,       /**< NIST P-256 ECC密钥 */
    SE050_KEYMGR_TYPE_ECC_P384,           /**< NIST P-384 ECC密钥 */
    SE050_KEYMGR_TYPE_AES_128,            /**< AES-128对称密钥 */
    SE050_KEYMGR_TYPE_AES_256,            /**< AES-256对称密钥 */
    SE050_KEYMGR_TYPE_HMAC_SHA256,        /**< HMAC-SHA256密钥 */
    SE050_KEYMGR_TYPE_HMAC_SHA384,        /**< HMAC-SHA384密钥 */
    SE050_KEYMGR_TYPE_DES_192,            /**< 3DES-192密钥 */
    SE050_KEYMGR_TYPE_RAW,                /**< 原始数据对象 */
    SE050_KEYMGR_TYPE_MAX
} se050_keymgr_type_t;

/******************************************************************************
 * 密钥大小定义 (字节)
 ******************************************************************************/
#define SE050_KEY_SIZE_ECC_P256         (32U)
#define SE050_KEY_SIZE_ECC_P384         (48U)
#define SE050_KEY_SIZE_AES_128          (16U)
#define SE050_KEY_SIZE_AES_256          (32U)
#define SE050_KEY_SIZE_HMAC_SHA256      (32U)
#define SE050_KEY_SIZE_HMAC_SHA384      (48U)
#define SE050_KEY_SIZE_DES_192          (24U)

/******************************************************************************
 * 密钥持久性类型
 ******************************************************************************/
typedef enum {
    SE050_KEYMGR_VOLATILE = 0,            /**< 易失性密钥 (会话期间有效) */
    SE050_KEYMGR_PERSISTENT,              /**< 持久性密钥 (安全元件内存储) */
    SE050_KEYMGR_EXTERNAL                 /**< 外部托管密钥 (外部存储) */
} se050_keymgr_persist_t;

/******************************************************************************
 * 密钥权限位掩码 (扩展定义)
 * @note 与 se050_adapter.h 中的 se050_key_permission_t 兼容
 ******************************************************************************/
typedef enum {
    SE050_KEYMGR_PERM_SIGN        = (1U << 0),   /**< 允许签名操作 */
    SE050_KEYMGR_PERM_VERIFY      = (1U << 1),   /**< 允许验签操作 */
    SE050_KEYMGR_PERM_ENCRYPT     = (1U << 2),   /**< 允许加密操作 */
    SE050_KEYMGR_PERM_DECRYPT     = (1U << 3),   /**< 允许解密操作 */
    SE050_KEYMGR_PERM_DERIVE      = (1U << 4),   /**< 允许密钥派生 */
    SE050_KEYMGR_PERM_DELETE      = (1U << 5),   /**< 允许删除密钥 */
    SE050_KEYMGR_PERM_EXPORT      = (1U << 6),   /**< 允许导出 (受保护导出) */
    SE050_KEYMGR_PERM_IMPORT      = (1U << 7),   /**< 允许导入密钥 */
    SE050_KEYMGR_PERM_KCV_GEN     = (1U << 8),   /**< 允许生成KCV */
    SE050_KEYMGR_PERM_WRAP        = (1U << 9),   /**< 允许密钥包装 */
    SE050_KEYMGR_PERM_UNWRAP      = (1U << 10),  /**< 允许密钥解包 */
    SE050_KEYMGR_PERM_ATTEST      = (1U << 11)   /**< 允许密钥证明 */
} se050_keymgr_permission_t;

/******************************************************************************
 * 密钥状态定义
 ******************************************************************************/
typedef enum {
    SE050_KEYMGR_STATE_INVALID = 0,       /**< 无效状态 */
    SE050_KEYMGR_STATE_CREATED,           /**< 已创建 */
    SE050_KEYMGR_STATE_ACTIVE,            /**< 激活状态 (可用) */
    SE050_KEYMGR_STATE_SUSPENDED,         /**< 暂停状态 (暂时禁用) */
    SE050_KEYMGR_STATE_COMPROMISED,       /**< 已泄露 (标记为撤销) */
    SE050_KEYMGR_STATE_EXPIRED,           /**< 已过期 */
    SE050_KEYMGR_STATE_DESTROYED          /**< 已销毁 */
} se050_keymgr_state_t;

/******************************************************************************
 * 密钥句柄定义 (32位标识符)
 ******************************************************************************/
typedef uint32_t se050_key_id_t;

/******************************************************************************
 * 密钥属性结构定义
 ******************************************************************************/
typedef struct {
    se050_keymgr_type_t type;             /**< 密钥类型 */
    uint16_t key_size;                    /**< 密钥大小 (位) */
    se050_keymgr_persist_t persistence;   /**< 持久性类型 */
    uint16_t permissions;                 /**< 权限位掩码 */
    se050_keymgr_state_t state;           /**< 密钥状态 */
    uint32_t created_at;                  /**< 创建时间戳 */
    uint32_t expires_at;                  /**< 过期时间戳 (0 = 永不过期) */
    uint32_t usage_counter;               /**< 使用计数器 */
    uint32_t max_usage_count;             /**< 最大使用次数 (0 = 无限制) */
    uint8_t auth_policy;                  /**< 认证策略 */
    bool export_allowed;                  /**< 是否允许导出 */
    bool backup_eligible;                 /**< 是否可备份 */
} se050_key_attr_t;

/******************************************************************************
 * 密钥生命周期上下文
 ******************************************************************************/
typedef struct {
    se050_key_id_t key_id;                /**< 密钥标识符 */
    se050_key_attr_t attr;                /**< 密钥属性 */
    uint32_t rotation_counter;            /**< 轮换计数器 */
    uint32_t parent_key_id;               /**< 父密钥ID (用于派生密钥) */
    uint8_t reserved[16];                 /**< 保留字段 */
} se050_key_lifecycle_t;

/******************************************************************************
 * 密钥信息查询结构
 ******************************************************************************/
typedef struct {
    se050_key_id_t key_id;                /**< 密钥标识符 */
    se050_keymgr_type_t type;             /**< 密钥类型 */
    uint16_t key_size_bits;               /**< 密钥大小 (位) */
    se050_keymgr_state_t state;           /**< 当前状态 */
    uint32_t created_at;                  /**< 创建时间 */
    uint32_t last_used_at;                /**< 最后使用时间 */
    uint32_t usage_count;                 /**< 使用次数 */
    bool is_persistent;                   /**< 是否持久化 */
} se050_key_info_t;

/******************************************************************************
 * 密钥轮换策略
 ******************************************************************************/
typedef struct {
    uint32_t max_age_days;                /**< 最大存活天数 (0 = 不限制) */
    uint32_t max_usage_count;             /**< 最大使用次数 (0 = 不限制) */
    bool auto_rotation;                   /**< 自动轮换使能 */
    se050_key_id_t backup_key_id;         /**< 备份密钥ID */
} se050_key_rotation_policy_t;

/******************************************************************************
 * 密钥管理器初始化与反初始化
 ******************************************************************************/

/**
 * @brief 初始化密钥管理器
 * @return OK 成功，其他错误码
 */
error_t se050_keymgr_init(void);

/**
 * @brief 反初始化密钥管理器
 * @return OK 成功，其他错误码
 */
error_t se050_keymgr_deinit(void);

/******************************************************************************
 * 密钥生命周期管理 API
 ******************************************************************************/

/**
 * @brief 生成新密钥
 * @param[in] key_id 请求的密钥标识符 (或 SE050_KEY_ID_INVALID 自动分配)
 * @param[in] type 密钥类型
 * @param[in] attr 密钥属性
 * @param[out] assigned_key_id 分配的密钥ID (可为 NULL)
 * @return OK 成功，其他错误码
 */
error_t se050_keymgr_generate(
    se050_key_id_t key_id,
    se050_keymgr_type_t type,
    const se050_key_attr_t *attr,
    se050_key_id_t *assigned_key_id
);

/**
 * @brief 导入外部密钥
 * @param[in] key_id 请求的密钥标识符
 * @param[in] key_data 密钥数据
 * @param[in] key_len 密钥数据长度
 * @param[in] type 密钥类型
 * @param[in] attr 密钥属性
 * @param[out] assigned_key_id 分配的密钥ID (可为 NULL)
 * @return OK 成功，其他错误码
 */
error_t se050_keymgr_import(
    se050_key_id_t key_id,
    const uint8_t *key_data,
    size_t key_len,
    se050_keymgr_type_t type,
    const se050_key_attr_t *attr,
    se050_key_id_t *assigned_key_id
);

/**
 * @brief 导出密钥 (受保护导出)
 * @param[in] key_id 密钥标识符
 * @param[out] key_data 密钥数据缓冲区
 * @param[in,out] key_len 缓冲区长度/实际长度
 * @param[in] wrap_key_id 用于包装的密钥ID (0 = 明文导出，需权限)
 * @return OK 成功，其他错误码
 */
error_t se050_keymgr_export(
    se050_key_id_t key_id,
    uint8_t *key_data,
    size_t *key_len,
    se050_key_id_t wrap_key_id
);

/**
 * @brief 删除密钥
 * @param[in] key_id 密钥标识符
 * @param[in] secure_erase 是否安全擦除
 * @return OK 成功，其他错误码
 */
error_t se050_keymgr_delete(
    se050_key_id_t key_id,
    bool secure_erase
);

/**
 * @brief 轮换密钥
 * @param[in] old_key_id 旧密钥标识符
 * @param[in] new_key_id 新密钥标识符 (或 SE050_KEY_ID_INVALID 自动分配)
 * @param[in] policy 轮换策略
 * @param[out] assigned_new_id 分配的新密钥ID
 * @return OK 成功，其他错误码
 */
error_t se050_keymgr_rotate(
    se050_key_id_t old_key_id,
    se050_key_id_t new_key_id,
    const se050_key_rotation_policy_t *policy,
    se050_key_id_t *assigned_new_id
);

/******************************************************************************
 * 密钥状态与属性管理 API
 ******************************************************************************/

/**
 * @brief 获取密钥属性
 * @param[in] key_id 密钥标识符
 * @param[out] attr 密钥属性结构
 * @return OK 成功，其他错误码
 */
error_t se050_keymgr_get_attr(
    se050_key_id_t key_id,
    se050_key_attr_t *attr
);

/**
 * @brief 设置密钥属性
 * @param[in] key_id 密钥标识符
 * @param[in] attr 密钥属性结构
 * @return OK 成功，其他错误码
 */
error_t se050_keymgr_set_attr(
    se050_key_id_t key_id,
    const se050_key_attr_t *attr
);

/**
 * @brief 获取密钥信息
 * @param[in] key_id 密钥标识符
 * @param[out] info 密钥信息结构
 * @return OK 成功，其他错误码
 */
error_t se050_keymgr_get_info(
    se050_key_id_t key_id,
    se050_key_info_t *info
);

/**
 * @brief 设置密钥状态
 * @param[in] key_id 密钥标识符
 * @param[in] state 目标状态
 * @return OK 成功，其他错误码
 */
error_t se050_keymgr_set_state(
    se050_key_id_t key_id,
    se050_keymgr_state_t state
);

/**
 * @brief 获取密钥当前状态
 * @param[in] key_id 密钥标识符
 * @param[out] state 当前状态
 * @return OK 成功，其他错误码
 */
error_t se050_keymgr_get_state(
    se050_key_id_t key_id,
    se050_keymgr_state_t *state
);

/******************************************************************************
 * 密钥查询与枚举 API
 ******************************************************************************/

/**
 * @brief 检查密钥是否存在
 * @param[in] key_id 密钥标识符
 * @param[out] exists 是否存在
 * @return OK 成功，其他错误码
 */
error_t se050_keymgr_exists(
    se050_key_id_t key_id,
    bool *exists
);

/**
 * @brief 列出所有密钥
 * @param[out] key_list 密钥ID列表缓冲区
 * @param[in] list_size 列表缓冲区大小 (元素个数)
 * @param[out] key_count 实际密钥数量
 * @return OK 成功，其他错误码
 */
error_t se050_keymgr_list_keys(
    se050_key_id_t *key_list,
    size_t list_size,
    size_t *key_count
);

/**
 * @brief 按类型列出密钥
 * @param[in] type 密钥类型
 * @param[out] key_list 密钥ID列表缓冲区
 * @param[in] list_size 列表缓冲区大小 (元素个数)
 * @param[out] key_count 实际密钥数量
 * @return OK 成功，其他错误码
 */
error_t se050_keymgr_list_keys_by_type(
    se050_keymgr_type_t type,
    se050_key_id_t *key_list,
    size_t list_size,
    size_t *key_count
);

/******************************************************************************
 * 密钥备份与恢复 API
 ******************************************************************************/

/**
 * @brief 备份密钥
 * @param[in] key_id 要备份的密钥标识符
 * @param[in] backup_key_id 用于加密的备份密钥
 * @param[out] backup_data 备份数据缓冲区
 * @param[in,out] backup_len 缓冲区长度/实际长度
 * @return OK 成功，其他错误码
 */
error_t se050_keymgr_backup(
    se050_key_id_t key_id,
    se050_key_id_t backup_key_id,
    uint8_t *backup_data,
    size_t *backup_len
);

/**
 * @brief 恢复密钥
 * @param[in] key_id 恢复的密钥标识符 (新ID)
 * @param[in] backup_data 备份数据
 * @param[in] backup_len 备份数据长度
 * @param[in] backup_key_id 用于解密的备份密钥
 * @param[in] attr 密钥属性
 * @param[out] assigned_key_id 分配的密钥ID
 * @return OK 成功，其他错误码
 */
error_t se050_keymgr_restore(
    se050_key_id_t key_id,
    const uint8_t *backup_data,
    size_t backup_len,
    se050_key_id_t backup_key_id,
    const se050_key_attr_t *attr,
    se050_key_id_t *assigned_key_id
);

/******************************************************************************
 * 密钥派生 API
 ******************************************************************************/

/**
 * @brief 从父密钥派生新密钥
 * @param[in] parent_key_id 父密钥标识符
 * @param[in] key_id 派生密钥标识符 (或 SE050_KEY_ID_INVALID 自动分配)
 * @param[in] derivation_data 派生数据
 * @param[in] derivation_len 派生数据长度
 * @param[in] type 派生密钥类型
 * @param[in] attr 密钥属性
 * @param[out] assigned_key_id 分配的密钥ID
 * @return OK 成功，其他错误码
 */
error_t se050_keymgr_derive(
    se050_key_id_t parent_key_id,
    se050_key_id_t key_id,
    const uint8_t *derivation_data,
    size_t derivation_len,
    se050_keymgr_type_t type,
    const se050_key_attr_t *attr,
    se050_key_id_t *assigned_key_id
);

/******************************************************************************
 * 密钥认证与证明 API
 ******************************************************************************/

/**
 * @brief 生成密钥证明
 * @param[in] key_id 要证明的密钥标识符
 * @param[in] attestation_key_id 证明密钥标识符
 * @param[out] attestation 证明数据
 * @param[in,out] attestation_len 缓冲区长度/实际长度
 * @return OK 成功，其他错误码
 */
error_t se050_keymgr_attest(
    se050_key_id_t key_id,
    se050_key_id_t attestation_key_id,
    uint8_t *attestation,
    size_t *attestation_len
);

/******************************************************************************
 * 密钥使用计数管理
 ******************************************************************************/

/**
 * @brief 增加密钥使用计数
 * @param[in] key_id 密钥标识符
 * @param[out] new_count 新的计数值
 * @return OK 成功，其他错误码
 */
error_t se050_keymgr_increment_usage(
    se050_key_id_t key_id,
    uint32_t *new_count
);

/**
 * @brief 重置密钥使用计数
 * @param[in] key_id 密钥标识符
 * @return OK 成功，其他错误码
 */
error_t se050_keymgr_reset_usage(se050_key_id_t key_id);

/******************************************************************************
 * 便捷宏函数
 ******************************************************************************/

/**
 * @brief 检查密钥ID是否有效
 */
#define SE050_KEYMGR_IS_VALID_ID(id) \
    (((id) >= SE050_KEY_ID_MIN) && ((id) <= SE050_KEY_ID_MAX))

/**
 * @brief 检查密钥类型是否有效
 */
#define SE050_KEYMGR_IS_VALID_TYPE(type) \
    (((type) >= SE050_KEYMGR_TYPE_ECC_P256) && ((type) < SE050_KEYMGR_TYPE_MAX))

/**
 * @brief 检查密钥权限
 */
#define SE050_KEYMGR_HAS_PERM(attr, perm) \
    (((attr)->permissions & (perm)) != 0U)

#ifdef __cplusplus
}
#endif

#endif /* SE050_KEYMGR_H */
