#ifndef YULEDKCS_H
#define YULEDKCS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * YuleDKCS FFI Header
 * Swift 与 C 库的桥接头文件
 * ============================================================================ */

/* 版本信息 */
#define YDKCS_VERSION_MAJOR 1
#define YDKCS_VERSION_MINOR 0
#define YDKCS_VERSION_PATCH 0

/* 算法类型 */
typedef enum {
    YDKCS_ALGO_AES256_GCM = 0,
    YDKCS_ALGO_CHACHA20_POLY1305 = 1
} ydkcs_algorithm_t;

/* 错误码 */
typedef enum {
    YDKCS_OK = 0,
    YDKCS_ERROR_INVALID_PARAM = -1,
    YDKCS_ERROR_MEMORY = -2,
    YDKCS_ERROR_CRYPTO = -3,
    YDKCS_ERROR_KEY_NOT_FOUND = -4,
    YDKCS_ERROR_INVALID_KEY = -5,
    YDKCS_ERROR_SIGNATURE = -6,
    YDKCS_ERROR_DERIVATION = -7,
    YDKCS_ERROR_UNKNOWN = -99
} ydkcs_error_t;

/* 钥匙生成结果结构体 */
typedef struct {
    int32_t success;                    /* 成功标志: 1=成功, 0=失败 */
    const uint8_t *key_data;            /* 钥匙数据指针 */
    size_t key_len;                     /* 钥匙数据长度 */
    const char *error_msg;              /* 错误信息（失败时有效） */
} ydkcs_key_result_t;

/* 加密/解密结果结构体 */
typedef struct {
    int32_t success;                    /* 成功标志: 1=成功, 0=失败 */
    const uint8_t *data;                /* 数据指针 */
    size_t data_len;                    /* 数据长度 */
    const char *error_msg;              /* 错误信息（失败时有效） */
} ydkcs_crypto_result_t;

/* BLE 连接状态 */
typedef enum {
    YDKCS_BLE_DISCONNECTED = 0,
    YDKCS_BLE_CONNECTING = 1,
    YDKCS_BLE_CONNECTED = 2,
    YDKCS_BLE_AUTHENTICATED = 3
} ydkcs_ble_state_t;

/* 命令类型 */
typedef enum {
    YDKCS_CMD_UNLOCK = 0x01,
    YDKCS_CMD_LOCK = 0x02,
    YDKCS_CMD_START_ENGINE = 0x03,
    YDKCS_CMD_STOP_ENGINE = 0x04,
    YDKCS_CMD_OPEN_TRUNK = 0x05,
    YDKCS_CMD_CLOSE_TRUNK = 0x06,
    YDKCS_CMD_OPEN_WINDOWS = 0x07,
    YDKCS_CMD_CLOSE_WINDOWS = 0x08,
    YDKCS_CMD_HONK = 0x09,
    YDKCS_CMD_FLASH_LIGHTS = 0x0A
} ydkcs_command_t;

/* ============================================================================
 * 生命周期管理
 * ============================================================================ */

/**
 * 初始化 YuleDKCS 库
 * 必须在调用其他函数之前调用
 */
void ydkcs_init(void);

/**
 * 清理 YuleDKCS 库资源
 * 程序结束时调用
 */
void ydkcs_cleanup(void);

/**
 * 获取库版本字符串
 * @return 版本号字符串（静态内存，无需释放）
 */
const char* ydkcs_version(void);

/* ============================================================================
 * 钥匙管理
 * ============================================================================ */

/**
 * 生成新的数字钥匙
 * @param vehicle_id 车辆唯一标识符（UTF-8 字符串）
 * @param result 输出结果结构体
 */
void ydkcs_generate_key(const char *vehicle_id, ydkcs_key_result_t *result);

/**
 * 释放钥匙生成结果占用的内存
 * @param result 结果结构体指针
 */
void ydkcs_free_key_result(ydkcs_key_result_t *result);

/**
 * 验证钥匙数据的有效性
 * @param key_data 钥匙数据
 * @param key_len 钥匙数据长度
 * @return 1=有效, 0=无效
 */
int32_t ydkcs_validate_key(const uint8_t *key_data, size_t key_len);

/**
 * 派生会话密钥
 * @param master_key 主密钥数据
 * @param master_key_len 主密钥长度
 * @param context 上下文信息（用于派生）
 * @param result 输出结果结构体
 */
void ydkcs_derive_key(const uint8_t *master_key, size_t master_key_len,
                      const char *context, ydkcs_key_result_t *result);

/* ============================================================================
 * 加密操作
 * ============================================================================ */

/**
 * 加密数据
 * @param plaintext 明文数据
 * @param plain_len 明文长度
 * @param key 加密密钥
 * @param key_len 密钥长度
 * @param algorithm 加密算法
 * @param result 输出结果结构体
 */
void ydkcs_encrypt(const uint8_t *plaintext, size_t plain_len,
                   const uint8_t *key, size_t key_len,
                   ydkcs_algorithm_t algorithm,
                   ydkcs_crypto_result_t *result);

/**
 * 解密数据
 * @param ciphertext 密文数据（包含 nonce 和 tag）
 * @param cipher_len 密文长度
 * @param key 解密密钥
 * @param key_len 密钥长度
 * @param algorithm 加密算法
 * @param result 输出结果结构体
 */
void ydkcs_decrypt(const uint8_t *ciphertext, size_t cipher_len,
                   const uint8_t *key, size_t key_len,
                   ydkcs_algorithm_t algorithm,
                   ydkcs_crypto_result_t *result);

/**
 * 使用 AES-GCM 加密
 * @param plaintext 明文数据
 * @param plain_len 明文长度
 * @param key 加密密钥（32字节）
 * @param key_len 密钥长度
 * @param nonce 随机数（12字节）
 * @param nonce_len 随机数长度
 * @param result 输出结果结构体
 */
void ydkcs_encrypt_aes_gcm(const uint8_t *plaintext, size_t plain_len,
                           const uint8_t *key, size_t key_len,
                           const uint8_t *nonce, size_t nonce_len,
                           ydkcs_crypto_result_t *result);

/**
 * 使用 AES-GCM 解密
 * @param ciphertext 密文数据（包含 tag）
 * @param cipher_len 密文长度
 * @param key 解密密钥（32字节）
 * @param key_len 密钥长度
 * @param nonce 随机数（12字节）
 * @param nonce_len 随机数长度
 * @param result 输出结果结构体
 */
void ydkcs_decrypt_aes_gcm(const uint8_t *ciphertext, size_t cipher_len,
                           const uint8_t *key, size_t key_len,
                           const uint8_t *nonce, size_t nonce_len,
                           ydkcs_crypto_result_t *result);

/**
 * 释放加密结果占用的内存
 * @param result 结果结构体指针
 */
void ydkcs_free_crypto_result(ydkcs_crypto_result_t *result);

/* ============================================================================
 * 签名验证
 * ============================================================================ */

/**
 * 验证钥匙签名
 * @param key_data 钥匙数据
 * @param key_len 钥匙数据长度
 * @param signature 签名数据
 * @param sig_len 签名长度
 * @param public_key 公钥数据
 * @param pub_len 公钥长度
 * @return 1=验证通过, 0=验证失败
 */
int32_t ydkcs_verify_signature(const uint8_t *key_data, size_t key_len,
                               const uint8_t *signature, size_t sig_len,
                               const uint8_t *public_key, size_t pub_len);

/**
 * 计算数据签名
 * @param data 输入数据
 * @param data_len 数据长度
 * @param private_key 私钥数据
 * @param key_len 私钥长度
 * @param result 输出结果结构体
 */
void ydkcs_sign_data(const uint8_t *data, size_t data_len,
                     const uint8_t *private_key, size_t key_len,
                     ydkcs_crypto_result_t *result);

/* ============================================================================
 * BLE 通信
 * ============================================================================ */

/**
 * 准备 BLE 命令数据包
 * @param command 命令类型
 * @param key_id 钥匙标识符
 * @param sequence 序列号
 * @param payload 附加数据（可为 NULL）
 * @param payload_len 附加数据长度
 * @param result 输出结果结构体
 */
void ydkcs_prepare_ble_packet(ydkcs_command_t command,
                              const char *key_id,
                              uint32_t sequence,
                              const uint8_t *payload, size_t payload_len,
                              ydkcs_crypto_result_t *result);

/**
 * 验证 BLE 响应数据包
 * @param response 响应数据
 * @param resp_len 响应长度
 * @param key_id 钥匙标识符
 * @param expected_seq 期望的序列号
 * @return 1=验证通过, 0=验证失败
 */
int32_t ydkcs_verify_ble_response(const uint8_t *response, size_t resp_len,
                                  const char *key_id,
                                  uint32_t expected_seq);

/* ============================================================================
 * 工具函数
 * ============================================================================ */

/**
 * 生成安全随机数
 * @param buffer 输出缓冲区
 * @param len 需要的字节数
 * @return 0=成功, 其他=失败
 */
int32_t ydkcs_random_bytes(uint8_t *buffer, size_t len);

/**
 * 计算 SHA-256 哈希
 * @param data 输入数据
 * @param data_len 数据长度
 * @param hash 输出哈希（32字节）
 */
void ydkcs_sha256(const uint8_t *data, size_t data_len, uint8_t *hash);

/**
 * 计算 HMAC-SHA256
 * @param data 输入数据
 * @param data_len 数据长度
 * @param key 密钥
 * @param key_len 密钥长度
 * @param hmac 输出 HMAC（32字节）
 */
void ydkcs_hmac_sha256(const uint8_t *data, size_t data_len,
                       const uint8_t *key, size_t key_len,
                       uint8_t *hmac);

/**
 * 安全内存清零
 * @param ptr 内存指针
 * @param len 字节数
 */
void ydkcs_secure_zero(void *ptr, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* YULEDKCS_H */