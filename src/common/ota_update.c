/******************************************************************************
 * @file    ota_update.c
 * @brief   OTA 安全升级支持
 ******************************************************************************/

#include "dkcs.h"
#include "se050_adapter.h"
#include <string.h>

#define OTA_MAGIC_HEADER    0x4F544131u  /* "OTA1" */
#define OTA_MAX_IMAGE_SIZE  (1024 * 1024)  /* 1MB */

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t crc32;
    uint8_t  signature[64];  /* ECDSA P-256 */
    uint8_t  reserved[16];
} ota_header_t;

typedef enum {
    OTA_STATE_IDLE = 0,
    OTA_STATE_DOWNLOADING,
    OTA_STATE_VERIFYING,
    OTA_STATE_FLASHING,
    OTA_STATE_COMPLETE,
    OTA_STATE_ERROR
} ota_state_t;

static struct {
    ota_state_t state;
    ota_header_t header;
    uint8_t *image_buffer;
    size_t received_size;
    uint32_t expected_crc;
} g_ota_ctx;

/**
 * @brief 初始化 OTA 更新
 * @param image_buffer 固件缓冲区 (静态分配)
 * @param buffer_size 缓冲区大小
 */
int ota_init(uint8_t *image_buffer, size_t buffer_size) {
    if (!image_buffer || buffer_size < OTA_MAX_IMAGE_SIZE) {
        return DKCS_ERROR_INVALID_PARAM;
    }
    
    memset(&g_ota_ctx, 0, sizeof(g_ota_ctx));
    g_ota_ctx.image_buffer = image_buffer;
    g_ota_ctx.state = OTA_STATE_IDLE;
    
    return DKCS_SUCCESS;
}

/**
 * @brief 开始接收 OTA 固件
 * @param version 目标版本号
 * @param size 固件大小
 */
int ota_start(uint32_t version, size_t size) {
    if (g_ota_ctx.state != OTA_STATE_IDLE) {
        return DKCS_ERROR_INVALID_STATE;
    }
    
    if (size > OTA_MAX_IMAGE_SIZE) {
        return DKCS_ERROR_TOO_LARGE;
    }
    
    g_ota_ctx.header.version = version;
    g_ota_ctx.header.size = size;
    g_ota_ctx.received_size = 0;
    g_ota_ctx.state = OTA_STATE_DOWNLOADING;
    
    return DKCS_SUCCESS;
}

/**
 * @brief 接收固件数据块
 * @param data 数据
 * @param offset 偏移
 * @param len 长度
 */
int ota_receive_chunk(const uint8_t *data, size_t offset, size_t len) {
    if (g_ota_ctx.state != OTA_STATE_DOWNLOADING) {
        return DKCS_ERROR_INVALID_STATE;
    }
    
    if (offset + len > g_ota_ctx.header.size) {
        return DKCS_ERROR_INVALID_PARAM;
    }
    
    memcpy(&g_ota_ctx.image_buffer[offset], data, len);
    g_ota_ctx.received_size += len;
    
    return DKCS_SUCCESS;
}

/**
 * @brief 验证固件签名
 * @param public_key 公钥 (SE050 中的密钥句柄)
 */
int ota_verify_signature(se050_key_id_t public_key) {
    if (g_ota_ctx.state != OTA_STATE_DOWNLOADING) {
        return DKCS_ERROR_INVALID_STATE;
    }
    
    g_ota_ctx.state = OTA_STATE_VERIFYING;
    
    /* 读取固件头部 */
    memcpy(&g_ota_ctx.header, g_ota_ctx.image_buffer, sizeof(ota_header_t));
    
    /* 验证魔法头部 */
    if (g_ota_ctx.header.magic != OTA_MAGIC_HEADER) {
        g_ota_ctx.state = OTA_STATE_ERROR;
        return DKCS_ERROR_INVALID_FORMAT;
    }
    
    /* 验证签名 (使用 SE050) */
    uint8_t hash[32];
    se050_hash_sha256(g_ota_ctx.image_buffer + sizeof(ota_header_t),
                      g_ota_ctx.header.size - sizeof(ota_header_t),
                      hash);
    
    int ret = se050_ecdsa_verify(public_key, hash, sizeof(hash),
                                  g_ota_ctx.header.signature, 64);
    if (ret != SE050_SUCCESS) {
        g_ota_ctx.state = OTA_STATE_ERROR;
        return DKCS_ERROR_VERIFICATION;
    }
    
    return DKCS_SUCCESS;
}

/**
 * @brief 应用固件更新
 * @param flash_write_cb Flash 写入回调
 */
int ota_apply(int (*flash_write_cb)(uint32_t addr, const uint8_t *data, size_t len)) {
    if (g_ota_ctx.state != OTA_STATE_VERIFYING) {
        return DKCS_ERROR_INVALID_STATE;
    }
    
    g_ota_ctx.state = OTA_STATE_FLASHING;
    
    /* 写入 Flash (通过回调以保持硬件无关) */
    uint32_t addr = 0x08000000;  /* 示例: Flash 起始地址 */
    size_t offset = sizeof(ota_header_t);
    
    while (offset < g_ota_ctx.header.size) {
        size_t chunk = 256;  /* Flash 页大小 */
        if (chunk > g_ota_ctx.header.size - offset) {
            chunk = g_ota_ctx.header.size - offset;
        }
        
        int ret = flash_write_cb(addr + offset - sizeof(ota_header_t),
                                  &g_ota_ctx.image_buffer[offset], chunk);
        if (ret != 0) {
            g_ota_ctx.state = OTA_STATE_ERROR;
            return DKCS_ERROR_FLASH;
        }
        
        offset += chunk;
    }
    
    g_ota_ctx.state = OTA_STATE_COMPLETE;
    return DKCS_SUCCESS;
}

/**
 * @brief 完成 OTA 更新
 */
int ota_complete(void) {
    if (g_ota_ctx.state == OTA_STATE_COMPLETE) {
        memset(&g_ota_ctx, 0, sizeof(g_ota_ctx));
        return DKCS_SUCCESS;
    }
    return DKCS_ERROR_INVALID_STATE;
}
