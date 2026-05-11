/******************************************************************************
 * @file    nvm_key_storage.h
 * @brief   NVM (非易失性内存) 钥匙持久化存储模块
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-09
 * 
 * @note    实现数字钥匙的安全持久化存储
 *          - 支持 CCC/ICCE/ICCOA 三种协议
 *          - AES-256-GCM 加密保护
 *          - HKDF 密钥派生
 *          - CRC32 完整性校验
 *          - 双份防回滚机制
 ******************************************************************************/

#ifndef NVM_KEY_STORAGE_H
#define NVM_KEY_STORAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "dkcs.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/******************************************************************************
 * NVM 版本和魔数定义
 ******************************************************************************/
#define NVM_MAGIC_NUMBER            0x4E564D4B  /**< "NVMK" */
#define NVM_VERSION_MAJOR           1
#define NVM_VERSION_MINOR           0
#define NVM_VERSION_PATCH           0

/******************************************************************************
 * NVM 固定长度定义
 ******************************************************************************/
#define NVM_MAX_KEY_RECORDS         32          /**< 最大钥匙记录数 */
#define NVM_KEY_DATA_MAX_LEN        2048        /**< 加密钥匙数据最大长度 */
#define NVM_DEVICE_KEY_LEN          32          /**< 设备主密钥长度 */
#define NVM_ENCRYPT_KEY_LEN         32          /**< AES-256 加密密钥长度 */
#define NVM_HKDF_SALT_LEN           32          /**< HKDF Salt 长度 */
#define NVM_IV_LEN                  12          /**< GCM IV 长度 */
#define NVM_TAG_LEN                 16          /**< GCM Tag 长度 */
#define NVM_KEY_LABEL_LEN           32          /**< 钥匙标签长度 */
#define NVM_SIGNATURE_LEN           32          /**< 记录签名长度 */

/******************************************************************************
 * NVM 存储区域定义
 ******************************************************************************/
#define NVM_SLOT_SIZE               4096        /**< 每个存储槽大小 */
#define NVM_PRIMARY_SLOT_OFFSET     0           /**< 主存储区偏移 */
#define NVM_BACKUP_SLOT_OFFSET      4096        /**< 备份存储区偏移 */
#define NVM_HEADER_SIZE             256         /**< 存储头部大小 */

/******************************************************************************
 * NVM 错误码定义
 ******************************************************************************/
typedef enum {
    NVM_OK = 0,
    NVM_ERROR_INVALID_PARAM = -200,
    NVM_ERROR_NOT_INITIALIZED = -201,
    NVM_ERROR_ALREADY_INITIALIZED = -202,
    NVM_ERROR_NO_SPACE = -203,
    NVM_ERROR_KEY_NOT_FOUND = -204,
    NVM_ERROR_KEY_EXISTS = -205,
    NVM_ERROR_CRYPTO_FAILURE = -206,
    NVM_ERROR_HAL_FAILURE = -207,
    NVM_ERROR_CRC_MISMATCH = -208,
    NVM_ERROR_MAGIC_MISMATCH = -209,
    NVM_ERROR_VERSION_MISMATCH = -210,
    NVM_ERROR_DATA_CORRUPTED = -211,
    NVM_ERROR_ROLLBACK_DETECTED = -212,
    NVM_ERROR_AUTHENTICATION_FAILED = -213,
    NVM_ERROR_DECRYPTION_FAILED = -214,
    NVM_ERROR_ENCRYPTION_FAILED = -215,
} nvm_error_t;

/******************************************************************************
 * 钥匙记录状态
 ******************************************************************************/
typedef enum {
    NVM_KEY_STATE_EMPTY = 0,        /**< 空槽位 */
    NVM_KEY_STATE_ACTIVE = 1,       /**< 活跃状态 */
    NVM_KEY_STATE_REVOKED = 2,      /**< 已撤销 */
    NVM_KEY_STATE_EXPIRED = 3,      /**< 已过期 */
    NVM_KEY_STATE_CORRUPTED = 4,    /**< 数据损坏 */
} nvm_key_state_t;

/******************************************************************************
 * NVM 钥匙记录结构 (存储格式)
 ******************************************************************************/
typedef struct {
    /* 记录头 */
    uint32_t magic;                 /**< 魔数 NVM_MAGIC_NUMBER */
    uint16_t version_major;         /**< 主版本 */
    uint16_t version_minor;         /**< 次版本 */
    uint16_t version_patch;         /**< 补丁版本 */
    uint16_t record_size;           /**< 记录总大小 */
    
    /* 钥匙标识 */
    uint8_t key_id[KEY_ID_LEN];     /**< 钥匙唯一ID */
    uint8_t device_id[DEVICE_ID_LEN]; /**< 设备ID */
    protocol_type_t protocol;       /**< 协议类型 CCC/ICCE/ICCOA */
    uint8_t key_role;               /**< 钥匙角色 */
    
    /* 元数据 */
    uint32_t created_time;          /**< 创建时间戳 */
    uint32_t expire_time;           /**< 过期时间戳 */
    uint32_t last_used;             /**< 最后使用时间 */
    uint32_t use_count;             /**< 使用计数 */
    uint16_t permissions;           /**< 权限位图 */
    uint16_t reserved;              /**< 保留 */
    
    /* 标签和描述 */
    uint8_t label[NVM_KEY_LABEL_LEN]; /**< 用户标签 */
    uint8_t vin[VIN_LEN];           /**< 车辆识别号 */
    
    /* 加密参数 */
    uint8_t salt[NVM_HKDF_SALT_LEN]; /**< HKDF Salt */
    uint8_t iv[NVM_IV_LEN];         /**< AES-GCM IV */
    uint8_t tag[NVM_TAG_LEN];       /**< AES-GCM Tag */
    
    /* 加密数据 */
    uint16_t encrypted_data_len;    /**< 加密数据长度 */
    uint16_t padding;               /**< 对齐填充 */
    uint8_t encrypted_data[NVM_KEY_DATA_MAX_LEN]; /**< 加密钥匙材料 */
    
    /* 完整性校验 */
    uint32_t crc32;                 /**< CRC32 校验值 */
    uint8_t signature[NVM_SIGNATURE_LEN]; /**< HMAC-SHA256 签名 */
} nvm_key_record_t;

/******************************************************************************
 * NVM 存储头部结构
 ******************************************************************************/
typedef struct {
    uint32_t magic;                 /**< 魔数 */
    uint16_t version_major;         /**< 版本 */
    uint16_t version_minor;
    uint16_t version_patch;
    uint16_t header_size;           /**< 头部大小 */
    
    uint32_t sequence_number;       /**< 序列号 (防回滚) */
    uint32_t write_count;           /**< 写入计数 */
    uint32_t record_count;          /**< 有效记录数 */
    uint32_t reserved_count;        /**< 保留槽位数 */
    
    uint32_t header_crc32;          /**< 头部 CRC32 */
    uint8_t header_signature[NVM_SIGNATURE_LEN]; /**< 头部签名 */
} nvm_storage_header_t;

/******************************************************************************
 * NVM 配置结构
 ******************************************************************************/
typedef struct {
    uint32_t storage_base_addr;     /**< 存储基地址 */
    uint32_t storage_size;          /**< 总存储大小 */
    uint32_t max_records;           /**< 最大记录数 */
    bool use_backup_slot;           /**< 启用双份备份 */
    bool verify_on_read;            /**< 读取时验证 */
    bool auto_compact;              /**< 自动整理碎片 */
} nvm_config_t;

/******************************************************************************
 * NVM HAL (硬件抽象层) 接口
 ******************************************************************************/

/**
 * @brief HAL 读取函数类型
 * @param addr 读取地址 (相对于存储基地址)
 * @param buffer 数据缓冲区
 * @param len 读取长度
 * @return 成功返回 NVM_OK，失败返回错误码
 */
typedef int32_t (*nvm_hal_read_func_t)(uint32_t addr, uint8_t *buffer, uint32_t len);

/**
 * @brief HAL 写入函数类型
 * @param addr 写入地址 (相对于存储基地址)
 * @param buffer 数据缓冲区
 * @param len 写入长度
 * @return 成功返回 NVM_OK，失败返回错误码
 */
typedef int32_t (*nvm_hal_write_func_t)(uint32_t addr, const uint8_t *buffer, uint32_t len);

/**
 * @brief HAL 擦除函数类型
 * @param addr 擦除地址 (相对于存储基地址)
 * @param len 擦除长度
 * @return 成功返回 NVM_OK，失败返回错误码
 */
typedef int32_t (*nvm_hal_erase_func_t)(uint32_t addr, uint32_t len);

/**
 * @brief HAL 接口结构
 */
typedef struct {
    nvm_hal_read_func_t read;       /**< 读取函数 */
    nvm_hal_write_func_t write;     /**< 写入函数 */
    nvm_hal_erase_func_t erase;     /**< 擦除函数 */
} nvm_hal_interface_t;

/******************************************************************************
 * NVM 枚举回调函数
 ******************************************************************************/

/**
 * @brief 钥匙枚举回调函数类型
 * @param record 钥匙记录指针
 * @param user_data 用户数据
 * @return 继续枚举返回 true，停止返回 false
 */
typedef bool (*nvm_enum_callback_t)(const nvm_key_record_t *record, void *user_data);

/******************************************************************************
 * NVM API 函数声明
 ******************************************************************************/

/**
 * @brief 初始化 NVM 模块
 * @param config NVM 配置指针
 * @param hal HAL 接口指针
 * @param device_master_key 设备主密钥 (32字节)
 * @return 成功返回 NVM_OK，失败返回错误码
 */
int32_t nvm_init(const nvm_config_t *config, const nvm_hal_interface_t *hal,
                 const uint8_t *device_master_key);

/**
 * @brief 反初始化 NVM 模块
 * @return 成功返回 NVM_OK，失败返回错误码
 */
int32_t nvm_deinit(void);

/**
 * @brief 存储钥匙
 * @param key_id 钥匙ID
 * @param protocol 协议类型
 * @param key_data 明文钥匙数据
 * @param key_data_len 钥匙数据长度
 * @param permissions 权限位图
 * @param expire_time 过期时间戳 (0 表示永不过期)
 * @param label 钥匙标签 (可为 NULL)
 * @return 成功返回 NVM_OK，失败返回错误码
 */
int32_t nvm_key_store(const uint8_t *key_id, protocol_type_t protocol,
                      const uint8_t *key_data, uint16_t key_data_len,
                      uint16_t permissions, uint32_t expire_time,
                      const uint8_t *label);

/**
 * @brief 加载钥匙
 * @param key_id 钥匙ID
 * @param key_data 输出缓冲区
 * @param key_data_len 缓冲区大小 (输入), 实际长度 (输出)
 * @param protocol 输出协议类型 (可为 NULL)
 * @param permissions 输出权限 (可为 NULL)
 * @return 成功返回 NVM_OK，失败返回错误码
 */
int32_t nvm_key_load(const uint8_t *key_id, uint8_t *key_data,
                     uint16_t *key_data_len, protocol_type_t *protocol,
                     uint16_t *permissions);

/**
 * @brief 删除钥匙 (安全删除)
 * @param key_id 钥匙ID
 * @return 成功返回 NVM_OK，失败返回错误码
 */
int32_t nvm_key_delete(const uint8_t *key_id);

/**
 * @brief 检查钥匙是否存在
 * @param key_id 钥匙ID
 * @return 存在返回 NVM_OK，不存在返回 NVM_ERROR_KEY_NOT_FOUND
 */
int32_t nvm_key_exists(const uint8_t *key_id);

/**
 * @brief 获取钥匙信息 (不解密)
 * @param key_id 钥匙ID
 * @param record 输出记录结构
 * @return 成功返回 NVM_OK，失败返回错误码
 */
int32_t nvm_key_get_info(const uint8_t *key_id, nvm_key_record_t *record);

/**
 * @brief 更新钥匙权限
 * @param key_id 钥匙ID
 * @param new_permissions 新权限
 * @return 成功返回 NVM_OK，失败返回错误码
 */
int32_t nvm_key_update_permissions(const uint8_t *key_id, uint16_t new_permissions);

/**
 * @brief 更新钥匙过期时间
 * @param key_id 钥匙ID
 * @param new_expire_time 新过期时间
 * @return 成功返回 NVM_OK，失败返回错误码
 */
int32_t nvm_key_update_expire_time(const uint8_t *key_id, uint32_t new_expire_time);

/**
 * @brief 枚举所有钥匙
 * @param callback 回调函数
 * @param user_data 用户数据
 * @return 成功返回 NVM_OK，失败返回错误码
 */
int32_t nvm_key_enumerate(nvm_enum_callback_t callback, void *user_data);

/**
 * @brief 获取钥匙数量
 * @param active_count 活跃钥匙数 (可为 NULL)
 * @param total_count 总记录数 (可为 NULL)
 * @return 成功返回 NVM_OK，失败返回错误码
 */
int32_t nvm_key_get_count(uint32_t *active_count, uint32_t *total_count);

/**
 * @brief 安全擦除所有钥匙数据
 * @return 成功返回 NVM_OK，失败返回错误码
 */
int32_t nvm_factory_reset(void);

/******************************************************************************
 * NVM 维护 API
 ******************************************************************************/

/**
 * @brief 整理存储碎片 (压缩无效记录)
 * @return 成功返回 NVM_OK，失败返回错误码
 */
int32_t nvm_compact(void);

/**
 * @brief 验证存储完整性
 * @param repair 是否自动修复
 * @return 成功返回 NVM_OK，发现错误返回错误码
 */
int32_t nvm_verify_integrity(bool repair);

/**
 * @brief 同步主备存储区
 * @return 成功返回 NVM_OK，失败返回错误码
 */
int32_t nvm_sync_backup(void);

/**
 * @brief 获取存储统计信息
 * @param used_space 已用空间
 * @param free_space 可用空间
 * @param write_cycles 写入次数
 * @return 成功返回 NVM_OK，失败返回错误码
 */
int32_t nvm_get_stats(uint32_t *used_space, uint32_t *free_space, uint32_t *write_cycles);

/******************************************************************************
 * NVM 工具函数
 ******************************************************************************/

/**
 * @brief 计算 CRC32
 * @param data 数据指针
 * @param len 数据长度
 * @return CRC32 值
 */
uint32_t nvm_crc32(const uint8_t *data, uint32_t len);

/**
 * @brief 安全内存清零
 * @param ptr 内存指针
 * @param len 长度
 */
void nvm_secure_zero(void *ptr, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* NVM_KEY_STORAGE_H */
