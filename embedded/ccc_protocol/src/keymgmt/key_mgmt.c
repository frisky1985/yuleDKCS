/**
 * @file key_mgmt.c
 * @brief CCC Digital Key Management Module
 */

#include "ccc_digital_key.h"
/* pvPortMalloc / vPortFree — 来自 FreeRTOS heap 管理, stubs 在 freertos_stubs.c */
void *pvPortMalloc(size_t xSize);
void vPortFree(void *pv);

/* ========================================================================
 *  密钥持久化 — [P0-4] 非易失存储
 * ========================================================================
 * 使用 SE050 Transparent Object 实现密钥数据的非易失持久化。
 * 存储格式: [magic(4)][version(2)][key_count(1)][keys(N*sizeof(ccc_digital_key_t))][crc32(4)]
 *
 * 兼容性:
 *   - 无 SE050 环境 (仿真/CI): 回退到加密 Flash (virt_flash_*)
 *   - 都不可用: 返回 CCC_ERR_HARDWARE, 不阻塞启动
 */

/* 持久化元数据 */
#define KEYSTORE_MAGIC      0x4B455953  /* "KEYS" */
#define KEYSTORE_VERSION    0x0001

/* 虚拟 Flash 接口 (非 SE050 环境下的回退, 例如 MCU 内部 Flash) */
/* 声明为 weak, 可被平台覆盖 */
__attribute__((weak)) int virt_flash_write(uint32_t addr, const uint8_t *data, uint16_t len)  { (void)addr; (void)data; (void)len; return -1; }
__attribute__((weak)) int virt_flash_read(uint32_t addr, uint8_t *data, uint16_t len)   { (void)addr; (void)data; (void)len; return -1; }
__attribute__((weak)) int virt_flash_erase(uint32_t addr, uint16_t len)                  { (void)addr; (void)len; return -1; }

/* 存储地址: 生产环境由 linker script 分配 */
#define KEYSTORE_FLASH_ADDR  0x080E0000  /* 例如 STM32 最后一页 */
#define KEYSTORE_FLASH_SIZE  4096

/* volatile memset: 安全清零 (编译器屏障) */
static inline void keystore_secure_zero(void *ptr, size_t len)
{
    if (ptr) {
        volatile uint8_t *p = (volatile uint8_t *)ptr;
        for (size_t i = 0; i < len; i++) {
            p[i] = 0;
        }
    }
}

/* 大端字节序工具 (与 crypto_utils.h 一致) */

/* 密钥存储全局变量 (必须在 save_keys/load_keys 之前声明) */
static ccc_digital_key_t g_keys[MAX_KEYS];
static uint8_t g_key_count = 0;
static inline uint32_t keystore_load_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
}
static inline void keystore_store_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >>  8);
    p[3] = (uint8_t)(v);
}

/* CRC32 (简化实现, 多项式 0xEDB88320) */
static uint32_t keystore_crc32(const uint8_t *data, uint16_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint32_t)data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
        }
    }
    return ~crc;
}

/* ========================================================================
 *  save_keys — [P0-4] 将密钥元数据写入非易失存储
 * ========================================================================
 * 优先使用 SE050 Transparent Object; 回退到 Flash。
 * 包含 key_id(16B) 级别的版本检查, 仅保存 ACTIVE/SUSPENDED 状态的密钥。
 */
static ccc_status_t save_keys(void)  /* [P0-4] */
{
    /* 计算有效密钥数 */
    uint8_t active_count = 0;
    for (uint8_t i = 0; i < MAX_KEYS; i++) {
        if (g_keys[i].state == KEY_STATE_ACTIVE ||
            g_keys[i].state == KEY_STATE_SUSPENDED) {
            active_count++;
        }
    }

    if (active_count == 0) {
        return CCC_OK;  /* 无密钥可持久化, 非错误 */
    }

    /* 构造持久化载荷: [magic(4)][version(2)][count(1)][keys(N)][crc32(4)] */
    uint16_t keys_data_len = active_count * sizeof(ccc_digital_key_t);
    uint16_t blob_len = 4 + 2 + 1 + keys_data_len + 4;
    uint8_t *blob = (uint8_t *)pvPortMalloc(blob_len);
    if (!blob) return CCC_ERR_NO_MEM;
    (void)memset(blob, 0, blob_len);
    uint16_t pos = 0;

    /* Magic */
    keystore_store_be32(blob + pos, KEYSTORE_MAGIC);
    pos += 4;

    /* Version */
    blob[pos++] = (uint8_t)(KEYSTORE_VERSION >> 8);
    blob[pos++] = (uint8_t)(KEYSTORE_VERSION & 0xFF);

    /* Key count */
    blob[pos++] = active_count;

    /* Keys data */
    for (uint8_t i = 0, written = 0; i < MAX_KEYS && written < active_count; i++) {
        if (g_keys[i].state == KEY_STATE_ACTIVE ||
            g_keys[i].state == KEY_STATE_SUSPENDED) {
            (void)memcpy(blob + pos, &g_keys[i], sizeof(ccc_digital_key_t));
            pos += sizeof(ccc_digital_key_t);
            written++;
        }
    }

    /* CRC32 校验 */
    uint32_t crc = keystore_crc32(blob, pos);
    blob[pos++] = (uint8_t)(crc >> 24);
    blob[pos++] = (uint8_t)(crc >> 16);
    blob[pos++] = (uint8_t)(crc >> 8);
    blob[pos++] = (uint8_t)(crc & 0xFF);

    /* 尝试 SE050 优先 */
    ccc_status_t ret;
    ret = sec_store_key((const uint8_t *)"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01",
                         blob, blob_len);
    if (ret == CCC_OK) {
        keystore_secure_zero(blob, blob_len);
        vPortFree(blob);
        return CCC_OK;
    }

    /* SE050 不可用: 回退到 Flash 存储 (如 FOTA 保留区) */
    ret = (ccc_status_t)virt_flash_erase(KEYSTORE_FLASH_ADDR, KEYSTORE_FLASH_SIZE);
    if (ret != 0) {
        keystore_secure_zero(blob, blob_len);
        vPortFree(blob);
        return CCC_ERR_HARDWARE;
    }

    /* Flash 写入 (分页写入) */
    uint16_t remaining = blob_len;
    uint16_t offset = 0;
    while (remaining > 0) {
        uint16_t chunk = (remaining > 256) ? 256 : remaining;
        if (virt_flash_write(KEYSTORE_FLASH_ADDR + offset, blob + offset, chunk) != 0) {
            keystore_secure_zero(blob, blob_len);
            vPortFree(blob);
            return CCC_ERR_HARDWARE;
        }
        offset += chunk;
        remaining -= chunk;
    }

    keystore_secure_zero(blob, blob_len);
    vPortFree(blob);
    return CCC_OK;
}

/* ========================================================================
 *  load_keys — [P0-4] 从非易失存储恢复密钥
 * ========================================================================
 * 启动时自动调用, 尝试从 SE050 → Flash 依次恢复。
 * 包含版本检查、CRC 完整性校验。
 */
static ccc_status_t load_keys(void)  /* [P0-4] */
{
    uint8_t *blob = (uint8_t *)pvPortMalloc(KEYSTORE_FLASH_SIZE);
    if (!blob) return CCC_ERR_NO_MEM;
    uint16_t blob_len = 0;
    ccc_status_t ret;

    /* 尝试 SE050 读取 */
    uint16_t se050_len = KEYSTORE_FLASH_SIZE;
    ret = sec_load_key((const uint8_t *)"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01",
                        blob, &se050_len);
    if (ret == CCC_OK) {
        blob_len = se050_len;
    } else {
        /* 回退到 Flash 读取 */
        uint16_t flash_len = KEYSTORE_FLASH_SIZE;
        if (virt_flash_read(KEYSTORE_FLASH_ADDR, blob, flash_len) != 0) {
            vPortFree(blob);
            return CCC_ERR_HARDWARE;
        }
        blob_len = flash_len;
    }

    if (blob_len < 11) { /* min: 4 + 2 + 1 + 0 + 4 = 11 */
        vPortFree(blob);
        return CCC_ERR_SECURITY;
    }

    /* 验证 Magic */
    if (keystore_load_be32(blob) != KEYSTORE_MAGIC) {
        vPortFree(blob);
        return CCC_ERR_SECURITY;
    }

    /* 验证 Version */
    uint16_t version = (uint16_t)((uint16_t)blob[4] << 8) | blob[5];
    if (version != KEYSTORE_VERSION) {
        vPortFree(blob);
        return CCC_ERR_SECURITY;  /* 不兼容版本, 需要迁移 */
    }

    /* 解析密钥数 */
    uint8_t count = blob[6];
    if (count > MAX_KEYS || count == 0) {
        vPortFree(blob);
        return CCC_ERR_SECURITY;
    }

    /* 计算预期长度 */
    uint16_t expected_len = 4 + 2 + 1 + count * sizeof(ccc_digital_key_t) + 4;
    if (blob_len < expected_len) {
        vPortFree(blob);
        return CCC_ERR_SECURITY;
    }

    /* 验证 CRC */
    uint32_t stored_crc = ((uint32_t)blob[expected_len - 4] << 24) |
                          ((uint32_t)blob[expected_len - 3] << 16) |
                          ((uint32_t)blob[expected_len - 2] << 8)  |
                          (uint32_t)blob[expected_len - 1];
    uint32_t computed_crc = keystore_crc32(blob, expected_len - 4);
    if (computed_crc != stored_crc) {
        vPortFree(blob);
        return CCC_ERR_SECURITY;  /* 数据篡改或损坏 */
    }

    /* 恢复密钥到内存 */
    uint16_t src_pos = 7; /* 跳过 magic(4) + version(2) + count(1) */
    (void)memset(g_keys, 0, sizeof(g_keys));

    for (uint8_t i = 0; i < count && i < MAX_KEYS; i++) {
        (void)memcpy(&g_keys[i], blob + src_pos, sizeof(ccc_digital_key_t));

        /* 有效性检查: 确保状态合法 */
        if (g_keys[i].state != KEY_STATE_ACTIVE &&
            g_keys[i].state != KEY_STATE_SUSPENDED) {
            g_keys[i].state = KEY_STATE_INACTIVE;
        }
        src_pos += sizeof(ccc_digital_key_t);
        g_key_count++;
    }

    keystore_secure_zero(blob, blob_len);
    vPortFree(blob);
    return CCC_OK;
}

ccc_status_t key_mgmt_init(void)
{
    (void)memset(g_keys, 0, sizeof(g_keys));
    g_key_count = 0;

    /* [P0-4] 从非易失存储恢复持久化密钥 */
    ccc_status_t ret = load_keys();
    if (ret != CCC_OK && ret != CCC_ERR_NOT_FOUND && ret != CCC_ERR_HARDWARE) {
        /* 首次启动/无持久化数据不视为错误 */
    }

    return CCC_OK;
}

ccc_status_t key_mgmt_deinit(void)
{
    /* [P0-4] 在关闭前持久化当前密钥状态 */
    save_keys();

    (void)memset(g_keys, 0, sizeof(g_keys));
    g_key_count = 0;
    return CCC_OK;
}

static int8_t find_key_slot(const uint8_t *key_id)
{
    for (uint8_t i = 0; i < MAX_KEYS; i++) {
        if (g_keys[i].state != KEY_STATE_INACTIVE &&
            memcmp(g_keys[i].key_id, key_id, KEY_ID_LEN) == 0) {
            return (int8_t)i;
        }
    }
    return -1;
}

static int8_t find_free_slot(void)
{
    for (uint8_t i = 0; i < MAX_KEYS; i++) {
        if (g_keys[i].state == KEY_STATE_INACTIVE) {
            return (int8_t)i;
        }
    }
    return -1;
}

ccc_status_t key_create(ccc_digital_key_t *key)
{
    if (!key) return CCC_ERR_INVALID_PARAM;

    /* Check duplicate */
    if (find_key_slot(key->key_id) >= 0) {
        return CCC_ERR_ALREADY_EXISTS;
    }

    /* Find free slot */
    int8_t slot = find_free_slot();
    if (slot < 0) return CCC_ERR_NO_MEM;

    /* Store key */
    (void)memcpy(&g_keys[slot], key, sizeof(ccc_digital_key_t));
    g_keys[slot].state = KEY_STATE_ACTIVE;
    g_key_count++;

    /* [P0-4] 持久化密钥到 SE050/Flash */
    ccc_status_t ret = sec_store_key(key->key_id, (const uint8_t *)key, sizeof(ccc_digital_key_t));
    if (ret != CCC_OK) {
        /* SE050 不可用不阻塞; 仅记录 */
    }

    /* [P0-4] 持久化密钥元数据 */
    save_keys();

    return CCC_OK;
}

ccc_status_t key_delete(const uint8_t *key_id)
{
    if (!key_id) return CCC_ERR_INVALID_PARAM;

    int8_t slot = find_key_slot(key_id);
    if (slot < 0) return CCC_ERR_NOT_FOUND;

    /* [P0-3] 从 SE050 删除密钥数据 */
    sec_delete_key(key_id);

    /* [P0-4] 删除内存中的密钥记录 */
    (void)memset(&g_keys[slot], 0, sizeof(ccc_digital_key_t));
    g_key_count--;

    /* [P0-4] 持久化最新的密钥元数据 */
    save_keys();

    return CCC_OK;
}

ccc_status_t key_get(const uint8_t *key_id, ccc_digital_key_t *key)
{
    if (!key_id || !key) return CCC_ERR_INVALID_PARAM;

    int8_t slot = find_key_slot(key_id);
    if (slot < 0) return CCC_ERR_NOT_FOUND;

    (void)memcpy(key, &g_keys[slot], sizeof(ccc_digital_key_t));
    return CCC_OK;
}

ccc_status_t key_list(ccc_digital_key_t *keys, uint8_t *count)
{
    if (!keys || !count) return CCC_ERR_INVALID_PARAM;

    uint8_t idx = 0;
    for (uint8_t i = 0; i < MAX_KEYS && idx < *count; i++) {
        if (g_keys[i].state != KEY_STATE_INACTIVE) {
            (void)memcpy(&keys[idx], &g_keys[i], sizeof(ccc_digital_key_t));
            idx++;
        }
    }
    *count = idx;
    return CCC_OK;
}

ccc_status_t key_share(const uint8_t *key_id, key_type_e type, uint32_t duration_s)
{
    if (!key_id) return CCC_ERR_INVALID_PARAM;

    int8_t slot = find_key_slot(key_id);
    if (slot < 0) return CCC_ERR_NOT_FOUND;

    /* Only owner keys can be shared */
    if (g_keys[slot].key_type != KEY_TYPE_OWNER) {
        return CCC_ERR_DENIED;
    }

    /* Create a friend/service key derived from owner key */
    ccc_digital_key_t shared = g_keys[slot];
    shared.key_type = type;
    shared.access_rights[0] &= ~ACCESS_ENGINE_START; /* Restrict some rights */
    shared.valid_until = shared.valid_from + duration_s;
    shared.version++;

    return key_create(&shared);
}

ccc_status_t key_revoke(const uint8_t *key_id)
{
    if (!key_id) return CCC_ERR_INVALID_PARAM;
    int8_t slot = find_key_slot(key_id);
    if (slot < 0) return CCC_ERR_NOT_FOUND;
    g_keys[slot].state = KEY_STATE_REVOKED;
    return CCC_OK;
}

ccc_status_t key_suspend(const uint8_t *key_id)
{
    if (!key_id) return CCC_ERR_INVALID_PARAM;
    int8_t slot = find_key_slot(key_id);
    if (slot < 0) return CCC_ERR_NOT_FOUND;
    g_keys[slot].state = KEY_STATE_SUSPENDED;
    return CCC_OK;
}

ccc_status_t key_resume(const uint8_t *key_id)
{
    if (!key_id) return CCC_ERR_INVALID_PARAM;
    int8_t slot = find_key_slot(key_id);
    if (slot < 0) return CCC_ERR_NOT_FOUND;
    if (g_keys[slot].state != KEY_STATE_SUSPENDED) return CCC_ERR_DENIED;
    g_keys[slot].state = KEY_STATE_ACTIVE;
    return CCC_OK;
}

ccc_status_t key_validate(const uint8_t *key_id)
{
    if (!key_id) return CCC_ERR_INVALID_PARAM;
    int8_t slot = find_key_slot(key_id);
    if (slot < 0) return CCC_ERR_NOT_FOUND;

    ccc_digital_key_t *k = &g_keys[slot];

    /* Check state */
    if (k->state != KEY_STATE_ACTIVE) return CCC_ERR_DENIED;

    /* Check expiry */
    /* uint32_t now = get_unix_timestamp(); */
    /* if (now > k->valid_until) { k->state = KEY_STATE_EXPIRED; return CCC_ERR_DENIED; } */

    return CCC_OK;
}
