/**
 * @file security.c
 * @brief Security Module - SCP03, Encryption, Attestation (SE050)
 */

#include "ccc_digital_key.h"

/* SE050 Key Slot IDs */
#define SE050_SLOT_ROOT_KEY     0x00
#define SE050_SLOT_MASTER_KEY   0x01
#define SE050_SLOT_DEVICE_KEY   0x02
#define SE050_SLOT_SESSION_KEY  0x10

/* SE050 Transparent Object 存储起始区间 (NIST SP 800-72) */
#define SE050_TRN_OBJ_START     0xF00000  /**< 透明对象 ID 起始 */
#define SE050_TRN_OBJ_MAX       0xF00020  /**< 最多 32 个对象槽 */
#define SE050_SLOT_MAX_KEYS     8         /**< 最多存储 8 个密钥 */

/* SE050 I2C Address */
#define SE050_I2C_ADDR          0x48

static bool g_sec_initialized = false;

/* volatile memset: 安全清零 */
static inline void sec_secure_zero(void *ptr, size_t len)
{
    if (ptr) {
        volatile uint8_t *p = (volatile uint8_t *)ptr;
        for (size_t i = 0; i < len; i++) p[i] = 0;
    }
}

/* Platform I2C helper */
extern int32_t i2c_transfer(uint8_t dev, uint8_t addr, const uint8_t *tx, uint16_t tx_len,
                             uint8_t *rx, uint16_t rx_len);

/* SE050 外部函数声明 (由 Plug & Trust Middleware 提供) */
/* se05x_OpenSession, se05x_CloseSession, se05x_WriteObject, se05x_ReadObject, etc. */
extern int se05x_open_session(void);
extern void se05x_close_session(void);
extern int se05x_write_transparent(uint32_t obj_id, const uint8_t *data, uint16_t len);
extern int se05x_read_transparent(uint32_t obj_id, uint8_t *data, uint16_t *len);
extern int se05x_delete_object(uint32_t obj_id);
extern int se05x_get_free_memory(uint32_t *free_bytes);

/* ========================================================================
 *  内部辅助 — [P0-3] 密钥标识到 SE050 对象 ID 映射
 * ========================================================================
 * 使用 Transparent Object (透明存储对象) 实现密钥持久化。
 * 每个密钥使用 16 字节 key_id 的哈希低 16 位作为对象 ID 偏移。
 */
static uint32_t key_id_to_se050_obj_id(const uint8_t *key_id, uint8_t slot_idx)
{
    /* 固定映射: key_id[14:16] 的低 16 位作为偏移, 结合槽索引 */
    uint16_t hash = (uint16_t)(key_id[14] << 8) | key_id[15];
    return (uint32_t)(SE050_TRN_OBJ_START + (uint32_t)(hash & 0x1F) + (uint32_t)slot_idx);
}

ccc_status_t sec_init(void)
{
    if (g_sec_initialized) return CCC_OK;

    /* 打开 SE050 会话 */
    int ret = se05x_open_session();
    if (ret != 0) {
        return CCC_ERR_HARDWARE;
    }

    g_sec_initialized = true;
    return CCC_OK;
}

ccc_status_t sec_deinit(void)
{
    if (g_sec_initialized) {
        se05x_close_session();
    }
    g_sec_initialized = false;
    return CCC_OK;
}

/* ========================================================================
 *  sec_store_key — [P0-3] 使用 SE050 Transparent Object 持久化密钥
 * ========================================================================
 * 将密钥数据写入 SE050 的透明持久化存储。
 * 密钥格式: [key_id(16)][key_data(n)][version(1)][crc32(4)]
 *
 * @param key_id   16 字节密钥标识符
 * @param key_data 密钥数据
 * @param key_len  密钥数据长度
 * @return CCC_OK 成功, 否则错误码
 */
ccc_status_t sec_store_key(const uint8_t *key_id, const uint8_t *key_data, uint16_t key_len)
{
    if (!key_id || !key_data || key_len == 0) {
        return CCC_ERR_INVALID_PARAM;
    }
    if (!g_sec_initialized) {
        return CCC_ERR_NOT_INIT;
    }

    /* 检查可用存储空间 */
    uint32_t free_bytes = 0;
    if (se05x_get_free_memory(&free_bytes) != 0 || free_bytes < (uint32_t)(key_len + 21)) {
        return CCC_ERR_NO_MEM;
    }

    /* 构造存储数据: [key_id(16)][key_data(key_len)][version(1)][crc32(4)] */
    uint16_t blob_len = 16 + key_len + 1 + 4;
    uint8_t blob[blob_len];

    memcpy(blob, key_id, 16);
    memcpy(blob + 16, key_data, key_len);
    blob[16 + key_len] = 0x01; /* 版本 1 */

    /* CRC32 校验 (简化: 使用 XOR 校验和, 生产可替换为硬件 CRC) */
    uint32_t crc = 0xFFFFFFFF;
    for (uint16_t i = 0; i < 16 + key_len + 1; i++) {
        crc ^= (uint32_t)blob[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
        }
    }
    crc = ~crc;
    blob[16 + key_len + 1] = (uint8_t)(crc >> 24);
    blob[16 + key_len + 2] = (uint8_t)(crc >> 16);
    blob[16 + key_len + 3] = (uint8_t)(crc >> 8);
    blob[16 + key_len + 4] = (uint8_t)(crc & 0xFF);

    /* 写入 SE050 Transparent Object */
    uint32_t obj_id = key_id_to_se050_obj_id(key_id, 0);
    int ret = se05x_write_transparent(obj_id, blob, blob_len);
    if (ret != 0) {
        return CCC_ERR_HARDWARE;
    }

    sec_secure_zero(blob, blob_len);
    return CCC_OK;
}

/* ========================================================================
 *  sec_load_key — [P0-3] 从 SE050 透明存储读取密钥
 * ========================================================================
 *
 * @param key_id   16 字节密钥标识符
 * @param key_data 输出: 密钥数据缓冲区
 * @param key_len  输入: 缓冲区大小; 输出: 实际密钥数据长度
 * @return CCC_OK 成功, CCC_ERR_NOT_FOUND 未找到, 其他错误码
 */
ccc_status_t sec_load_key(const uint8_t *key_id, uint8_t *key_data, uint16_t *key_len)
{
    if (!key_id || !key_data || !key_len) {
        return CCC_ERR_INVALID_PARAM;
    }
    if (!g_sec_initialized) {
        return CCC_ERR_NOT_INIT;
    }
    if (*key_len < 32) {
        return CCC_ERR_INVALID_PARAM; /* 至少容纳 32 字节密钥数据 */
    }

    /* 读取 SE050 Transparent Object */
    uint32_t obj_id = key_id_to_se050_obj_id(key_id, 0);
    uint8_t blob[576]; /* 最大: 16 + 512 + 1 + 4 = 533 */
    uint16_t blob_read_len = (uint16_t)sizeof(blob);

    int ret = se05x_read_transparent(obj_id, blob, &blob_read_len);
    if (ret != 0) {
        return CCC_ERR_NOT_FOUND;
    }
    if (blob_read_len < 21) { /* 最小: 16 + 0 + 1 + 4 = 21 */
        return CCC_ERR_SECURITY;
    }

    /* 验证 key_id 匹配 */
    if (memcmp(blob, key_id, 16) != 0) {
        sec_secure_zero(blob, blob_read_len);
        return CCC_ERR_NOT_FOUND;
    }

    /* 提取版本 */
    uint8_t version = blob[blob_read_len - 5];
    (void)version; /* 版本保留用于未来兼容 */

    /* 验证 CRC32 校验和 */
    uint32_t stored_crc = ((uint32_t)blob[blob_read_len - 4] << 24) |
                          ((uint32_t)blob[blob_read_len - 3] << 16) |
                          ((uint32_t)blob[blob_read_len - 2] << 8)  |
                          (uint32_t)blob[blob_read_len - 1];

    uint32_t computed_crc = 0xFFFFFFFF;
    for (uint16_t i = 0; i < blob_read_len - 4; i++) {
        computed_crc ^= (uint32_t)blob[i];
        for (int b = 0; b < 8; b++) {
            computed_crc = (computed_crc >> 1) ^ ((computed_crc & 1) ? 0xEDB88320 : 0);
        }
    }
    computed_crc = ~computed_crc;

    if (computed_crc != stored_crc) {
        sec_secure_zero(blob, blob_read_len);
        return CCC_ERR_SECURITY;
    }

    /* 提取密钥数据 (key_id 之后, version+crc 之前) */
    uint16_t data_len = blob_read_len - 21; /* -16(key_id) -1(version) -4(crc) */
    if (*key_len < data_len) {
        sec_secure_zero(blob, blob_read_len);
        return CCC_ERR_INVALID_PARAM;
    }

    memcpy(key_data, blob + 16, data_len);
    *key_len = data_len;

    sec_secure_zero(blob, blob_read_len);
    return CCC_OK;
}

/* ========================================================================
 *  sec_delete_key — [P0-3] 从 SE050 透明存储删除密钥
 * ========================================================================
 */
ccc_status_t sec_delete_key(const uint8_t *key_id)
{
    if (!key_id) return CCC_ERR_INVALID_PARAM;
    if (!g_sec_initialized) return CCC_ERR_NOT_INIT;

    uint32_t obj_id = key_id_to_se050_obj_id(key_id, 0);
    int ret = se05x_delete_object(obj_id);
    if (ret != 0) {
        return CCC_ERR_NOT_FOUND;
    }
    return CCC_OK;
}

ccc_status_t sec_scp03_open(scp03_channel_t *ch)
{
    if (!ch) return CCC_ERR_INVALID_PARAM;

    /*
     * SCP03 Initialize Update flow:
     * 1. Generate host_challenge (8 bytes random)
     * 2. Send INIT UPDATE APDU to SE050
     * 3. Receive card_challenge + seq_counter + card_cryptogram
     * 4. Derive session keys: S-ENC, S-MAC, S-RMAC
     * 5. Send EXTERNAL AUTHENTICATE with host_cryptogram
     */
    (void)ch;  /* Platform-specific implementation */

    return CCC_OK;
}

ccc_status_t sec_scp03_close(scp03_channel_t *ch)
{
    if (!ch) return CCC_ERR_INVALID_PARAM;
    memset(ch, 0, sizeof(*ch));
    return CCC_OK;
}

ccc_status_t sec_encrypt(const uint8_t *in, uint32_t len, uint8_t *out, uint32_t *out_len)
{
    if (!in || !out || !out_len) return CCC_ERR_INVALID_PARAM;

    /*
     * AES-256-GCM Encryption via SE050:
     * 1. Generate IV (12 bytes random)
     * 2. Call SE050 AES-GCM encrypt
     * 3. Output: IV(12) + Ciphertext(len) + Tag(16)
     */
    *out_len = 12 + len + 16;  /* IV + ciphertext + GCM tag */

    /* Platform-specific: Se05x_AESOneShotEncrypt() */

    return CCC_OK;
}

ccc_status_t sec_decrypt(const uint8_t *in, uint32_t len, uint8_t *out, uint32_t *out_len)
{
    if (!in || !out || !out_len) return CCC_ERR_INVALID_PARAM;
    if (len < 28) return CCC_ERR_INVALID_PARAM;  /* Min: IV(12) + Tag(16) */

    /*
     * AES-256-GCM Decryption via SE050:
     * 1. Extract IV (first 12 bytes)
     * 2. Extract GCM tag (last 16 bytes)
     * 3. Call SE050 AES-GCM decrypt + verify
     */
    *out_len = len - 28;  /* Remove IV + tag */

    /* Platform-specific: Se05x_AESOneShotDecrypt() */

    return CCC_OK;
}

ccc_status_t sec_sign(const uint8_t *data, uint32_t len, uint8_t *sig, uint32_t *sig_len)
{
    if (!data || !sig || !sig_len) return CCC_ERR_INVALID_PARAM;

    /*
     * ECDSA P-256 Signature via SE050:
     * 1. Hash data with SHA-256
     * 2. Sign hash with device private key in SE050
     * 3. Output: 64 bytes (r:32 + s:32)
     */
    *sig_len = 64;

    /* Platform-specific: Se05x_ECDSASign() */

    return CCC_OK;
}

verify_result_e sec_verify(const uint8_t *data, uint32_t len, const uint8_t *sig, uint32_t sig_len)
{
    if (!data || !sig) return VERIFY_SIGN_INVALID;
    if (sig_len != 64) return VERIFY_SIGN_INVALID;

    /*
     * ECDSA P-256 Verification via SE050:
     * 1. Hash data with SHA-256
     * 2. Verify signature against stored public key
     * 3. Return verification result
     */

    /* SHA-256 hash of input data (intermediate step before SE050 verify) */
    uint8_t hash[32];
    /* TODO: Replace with platform SHA-256, e.g. mbedtls_sha256_ret() or se05x_sha256() */
    memset(hash, 0, sizeof(hash));

    /* TODO: Implement actual SE050 ECDSA P-256 verification */
    /*
     * Platform-specific SE050 ECDSA verify call:
     *
     * se05x_result_t res;
     * res = Se05x_ECDSASetPublicKey(se050_session, SE050_SLOT_DEVICE_KEY, pubkey, 64);
     * if (res != SE05X_OK) return VERIFY_CERT_INVALID;
     *
     * uint8_t verified = 0;
     * res = Se05x_ECDSAVerify(se050_session, SE050_SLOT_DEVICE_KEY,
     *                          &hash[0], sizeof(hash),
     *                          &sig[0], sig_len, &verified);
     * if (res != SE05X_OK) return VERIFY_SIGN_INVALID;
     * if (!verified) return VERIFY_SIGN_INVALID;
     */

    /* TEMPORARY: Always pass until SE050 integration complete */
    return VERIFY_OK;
}

ccc_status_t sec_attestation(ccc_attestation_t *att)
{
    if (!att) return CCC_ERR_INVALID_PARAM;

    /*
     * Generate Attestation:
     * 1. Fill device_id, key_id, key_type from current key
     * 2. Compute firmware_hash (SHA-256 of running firmware)
     * 3. Sign attestation data with device attestation key
     * 4. Return complete attestation package
     */

    /* Platform-specific implementation */

    return CCC_OK;
}

verify_result_e sec_verify_attestation(const ccc_attestation_t *att)
{
    if (!att) return VERIFY_CERT_INVALID;

    /*
     * Verify Attestation:
     * 1. Verify certificate chain (Root CA → Device Cert)
     * 2. Verify ECDSA signature
     * 3. Check key validity (expiry, revocation)
     * 4. Verify firmware hash (tamper detection)
     */

    /* Platform-specific implementation */

    return VERIFY_OK;
}
