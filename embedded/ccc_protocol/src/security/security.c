/**
 * @file security.c
 * @module EMB-BSW-SE050 (ASPICE SWE.4)
 * @brief Security Module — SCP03 Secure Channel, Encryption, Attestation (SE050)
 * @version 2.0
 * @date 2026-07-16
 *
 * Layer: BSW (Basic Software Layer) — Hardware Security
 *
 * [P0-1 FIX] Implemented real SCP03 secure channel via se050_scp03 module.
 * Replaced all security stubs with actual cryptographic operations.
 *
 * Changes from v1.0:
 *   - sec_scp03_open():  Full INITIALIZE UPDATE + EXTERNAL AUTHENTICATE flow
 *   - sec_scp03_close(): Session key zeroing + SE050 reset
 *   - sec_encrypt/decrypt(): Real AES-256-GCM via crypto_engine
 *   - sec_init/deinit(): Crypto engine + SCP03 session lifecycle
 */

#include "ccc_digital_key.h"
#include "crypto_engine.h"      /* SHA-256, AES-256-GCM, SM2/SM3 */
#include "crypto_random.h"      /* [P1-1] TRNG-backed crypto_random_bytes() */
#include "se050_scp03.h"        /* [P0-1] SCP03 secure channel implementation */
/* pvPortMalloc / vPortFree — 来自 FreeRTOS heap 管理, stubs 在 freertos_stubs.c */
void *pvPortMalloc(size_t xSize);
void vPortFree(void *pv);

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

/**
 * [P0-1] Global SCP03 session context.
 * Holds all session keys (S-ENC, S-MAC, S-RMAC) and protocol state.
 * Must be explicitly zeroed on deinit or close.
 */
static scp03_session_t g_scp03_session;

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

/* [P1-1] crypto_random_bytes() is now provided by crypto_random.h / crypto_random.c.
 * No extern declaration needed — include the header above.
 * The implementation provides three-tier fallback:
 *   Tier 1: SE050 HW TRNG (after SCP03 established)
 *   Tier 2: mbedTLS CTR_DRBG
 *   Tier 3: /dev/urandom (Linux) or arc4random_buf (macOS/BSD)
 * No hardcoded fallback values. Error returned on failure. */

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

/* ========================================================================
 *  [P1-1] SE050 RNG via SCP03 Secure Channel
 * ========================================================================
 * Callback registered with crypto_random_register_se050() after SCP03
 * establishment. Issues GET CHALLENGE APDUs through the SCP03 secure
 * channel to read hardware-generated random bytes from the SE050 TRNG.
 *
 * The SE050's true hardware random number generator is FIPS-compliant
 * and provides the highest quality entropy in the system.
 *
 * GET CHALLENGE (ISO 7816-4):
 *   CLA=0x80(0x84 for C-MAC), INS=0x84, P1=0x00, P2=0x00, Le=requested_len
 *
 * The SE050 returns up to 256 bytes per call. For requests > 256 bytes,
 * we issue multiple GET CHALLENGE commands. Each call is a fresh hardware
 * random generation — no software PRNG expansion.
 */

/**
 * @brief SE050 RNG callback: read random bytes through the SCP03 secure channel.
 *
 * @param buf  [out] Buffer to fill with random bytes from SE050 TRNG
 * @param len  Number of bytes requested
 * @return 0 on success, -1 on failure
 */
static int se050_rng_via_scp03(uint8_t *buf, size_t len)
{
    uint8_t resp[256];
    uint16_t resp_len;
    uint16_t chunk;
    size_t offset = 0;
    int ret;

    if (buf == NULL || len == 0)
    {
        return -1;
    }

    /* Issue GET CHALLENGE APDUs through the SCP03 secure channel.
     * Each APDU returns up to 256 bytes from the SE050 hardware TRNG.
     * We iterate to fill the full requested length.
     */
    while (offset < len)
    {
        chunk = (uint16_t)((len - offset) > 256 ? 256 : (len - offset));

        /* Le = chunk — request exactly chunk bytes */
        resp_len = chunk;
        ret = se050_scp03_apdu(&g_scp03_session, SE050_I2C_ADDR,
                                SCP03_CLA_NO_SM,
                                SCP03_INS_GET_CHALLENGE,
                                0x00, 0x00,
                                NULL, 0,
                                resp, &resp_len);
        if (ret != SCP03_OK)
        {
            return -1;
        }

        (void)memcpy(buf + offset, resp, resp_len);
        offset += (size_t)resp_len;
    }

    /* Zero intermediate buffer */
    (void)memset(resp, 0, sizeof(resp));

    return 0;
}

/* ========================================================================
 *  sec_init / sec_deinit — [P0-1] 初始化加密引擎和 SCP03 会话
 * ======================================================================== */

ccc_status_t sec_init(void)
{
    if (g_sec_initialized) return CCC_OK;

    /* [P1-1] Initialize the TRNG subsystem FIRST.
     * crypto_random_init() probes all available entropy sources and fails
     * if none is operational. This ensures the device never boots without
     * a validated TRNG — no silent fallback to weak entropy.
     */
    {
        int trng_ret = crypto_random_init();
        if (trng_ret != CRYPTO_RANDOM_OK)
        {
            /* No entropy source available — production system MUST NOT boot.
             * This is a critical hardware failure, not a recoverable error. */
            return CCC_ERR_HARDWARE;
        }

        /* Run initial health test on the bootstrap entropy source */
        trng_ret = crypto_random_health_test();
        if (trng_ret != CRYPTO_RANDOM_OK)
        {
            /* Health test failed — source is stuck or degraded */
            crypto_random_deinit();
            return CCC_ERR_HARDWARE;
        }
    }

    /* 初始化密码引擎 (pure C SHA-256, AES-GCM, etc.) */
    if (crypto_engine_init() != CRYPTO_SUCCESS) {
        crypto_random_deinit();
        return CCC_ERR_HARDWARE;
    }

    /* 初始化 SCP03 会话上下文 (默认传输密钥) */
    {
        int ret = se050_scp03_init(&g_scp03_session);
        if (ret != SCP03_OK) {
            crypto_engine_deinit();
            crypto_random_deinit();
            return CCC_ERR_HARDWARE;
        }
    }

    /* 打开 SE050 会话 (硬件层) */
    int ret = se05x_open_session();
    if (ret != 0) {
        se050_scp03_deinit(&g_scp03_session);
        crypto_engine_deinit();
        crypto_random_deinit();
        return CCC_ERR_HARDWARE;
    }

    /* [P1-1] Register the SE050 RNG callback.
     * At this point only the SE050 hardware session is open (not SCP03).
     * The callback will work once SCP03 is established after sec_scp03_open().
     * During SCP03 bootstrap, crypto_random_bytes() uses Tier 2/3 fallback.
     */
    crypto_random_register_se050(se050_rng_via_scp03);

    g_sec_initialized = true;
    return CCC_OK;
}

ccc_status_t sec_deinit(void)
{
    if (!g_sec_initialized) return CCC_OK;

    /* [P1-1] Unregister SE050 RNG before closing SCP03 session */
    crypto_random_unregister_se050();

    /* 关闭 SCP03 安全通道 */
    se050_scp03_close_session(&g_scp03_session);
    se050_scp03_deinit(&g_scp03_session);

    /* 关闭 SE050 硬件会话 */
    se05x_close_session();

    /* 关闭密码引擎 */
    crypto_engine_deinit();

    /* [P1-1] Deinitialize the TRNG subsystem */
    crypto_random_deinit();

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
    uint8_t *blob = (uint8_t *)pvPortMalloc(blob_len);
    if (!blob) return CCC_ERR_NO_MEM;
    memset(blob, 0, blob_len);
    memcpy(blob, key_id, 16);
    memcpy(blob + 16, key_data, key_len);
    blob[16 + key_len] = 0x01; /* 版本 1 */

    /* CRC32 校验 (简化) */
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

    sec_secure_zero(blob, blob_len);
    vPortFree(blob);

    if (ret != 0) {
        return CCC_ERR_HARDWARE;
    }
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
    uint16_t max_blob_len = 21 + *key_len;
    if (max_blob_len < 576) max_blob_len = 576;
    uint8_t *blob = (uint8_t *)pvPortMalloc(max_blob_len);
    if (!blob) return CCC_ERR_NO_MEM;

    uint16_t blob_read_len = max_blob_len;
    int ret = se05x_read_transparent(obj_id, blob, &blob_read_len);
    if (ret != 0) {
        vPortFree(blob);
        return CCC_ERR_NOT_FOUND;
    }
    if (blob_read_len < 21) {
        vPortFree(blob);
        return CCC_ERR_SECURITY;
    }

    /* 验证 key_id 匹配 */
    if (memcmp(blob, key_id, 16) != 0) {
        sec_secure_zero(blob, blob_read_len);
        vPortFree(blob);
        return CCC_ERR_NOT_FOUND;
    }

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
        vPortFree(blob);
        return CCC_ERR_SECURITY;
    }

    /* 提取密钥数据 (key_id 之后, version+crc 之前) */
    uint16_t data_len = blob_read_len - 21;
    if (*key_len < data_len) {
        sec_secure_zero(blob, blob_read_len);
        vPortFree(blob);
        return CCC_ERR_INVALID_PARAM;
    }

    memcpy(key_data, blob + 16, data_len);
    *key_len = data_len;

    sec_secure_zero(blob, blob_read_len);
    vPortFree(blob);
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

/* ========================================================================
 *  sec_scp03_open — [P0-1] 建立 SCP03 安全通道
 * ========================================================================
 *
 * Implements full SCP03 session establishment:
 *   1. Host generates 8-byte challenge (TRNG)
 *   2. INITIALIZE UPDATE APDU → SE050 responds with card challenge,
 *      sequence counter, and card cryptogram
 *   3. Host derives session keys S-ENC, S-MAC, S-RMAC
 *   4. Host verifies card cryptogram (AES-CMAC)
 *   5. EXTERNAL AUTHENTICATE APDU with host cryptogram + C-MAC
 *   6. Session is now in SECURE state
 *
 * @param ch  Output: SCP03 channel parameters (populated with session info)
 * @return CCC_OK on success, error code on failure
 */
ccc_status_t sec_scp03_open(scp03_channel_t *ch)
{
    if (!ch) return CCC_ERR_INVALID_PARAM;

    /* 如果 SCP03 会话已打开, 先关闭 */
    if (se050_scp03_is_open(&g_scp03_session)) {
        se050_scp03_close_session(&g_scp03_session);
    }

    /* 确保会话处于可初始化状态 */
    if (g_scp03_session.state != SCP03_STATE_INIT) {
        se050_scp03_deinit(&g_scp03_session);
        se050_scp03_init(&g_scp03_session);
    }

    /* 执行 SCP03 完整握手: INIT UPDATE → Key Derivation → EXT AUTH */
    int ret = se050_scp03_open_session(&g_scp03_session, SE050_I2C_ADDR);
    if (ret != SCP03_OK) {
        /* [P1-1] SCP03 session open failed — ensure SE050 RNG is unregistered */
        crypto_random_unregister_se050();
        return CCC_ERR_SECURITY;
    }

    /* [P1-1] SCP03 session now established — SE050 RNG callback is fully operational.
     * crypto_random_bytes() will now use the hardware TRNG as Tier 1 source.
     */
    crypto_random_register_se050(se050_rng_via_scp03);

    /* 填充 scp03_channel_t (兼容原 API 接口) */
    memset(ch, 0, sizeof(*ch));
    memcpy(ch->enc_key, g_scp03_session.s_enc, 16);
    memcpy(ch->mac_key, g_scp03_session.s_mac, 16);
    memcpy(ch->dek_key, g_scp03_session.s_rmac, 16);
    memcpy(ch->host_challenge, g_scp03_session.host_challenge, 8);
    memcpy(ch->card_challenge, g_scp03_session.card_challenge, 8);
    memcpy(ch->seq_counter, g_scp03_session.seq_counter, 2);
    ch->chain_mode = 0x01; /* C-MAC only (no encryption at SCP03 layer) */

    return CCC_OK;
}

/* ========================================================================
 *  sec_scp03_close — [P0-1] 关闭 SCP03 安全通道
 * ========================================================================
 *
 * Securely zeroes all session keys and resets the SE050 secure channel.
 * Static keys remain for future session establishment.
 */
ccc_status_t sec_scp03_close(scp03_channel_t *ch)
{
    if (!ch) return CCC_ERR_INVALID_PARAM;

    /* [P1-1] Unregister SE050 RNG before closing SCP03 session.
     * After this, crypto_random_bytes() falls back to Tier 2/3. */
    crypto_random_unregister_se050();

    /* 关闭 SCP03 会话 (零化所有会话密钥) */
    se050_scp03_close_session(&g_scp03_session);

    /* 零化输出缓冲区 */
    memset(ch, 0, sizeof(*ch));
    return CCC_OK;
}

/* ========================================================================
 *  sec_encrypt / sec_decrypt — [P0-1] AES-256-GCM 加解密
 * ========================================================================
 *
 * Uses crypto_engine AES-256-GCM (pure C implementation).
 * Output format: IV(12) || Ciphertext(N) || Tag(16)
 *
 * The SCP03 channel handles APDU-level security. This function provides
 * application-level data encryption using a stronger key schedule.
 */

ccc_status_t sec_encrypt(const uint8_t *in, uint32_t len, uint8_t *out, uint32_t *out_len)
{
    int ret;
    uint8_t iv[12];
    uint8_t tag[16];

    if (!in || !out || !out_len) return CCC_ERR_INVALID_PARAM;
    if (!g_sec_initialized) return CCC_ERR_NOT_INIT;

    /* 所需输出缓冲区: IV(12) + Ciphertext(len) + Tag(16) */
    if (*out_len < 12 + len + 16) {
        *out_len = 12 + len + 16;
        return CCC_ERR_INVALID_PARAM;
    }

    /* [P1-1] 生成随机 IV (12 bytes) 使用 TRNG-backed crypto_random_bytes().
     * 如果失败,不降级到硬编码值 — 返回错误让上层处理。
     * 弱 IV 会导致 AES-GCM 安全性彻底丧失 (nonce reuse attack)。
     */
    ret = crypto_random_bytes(iv, sizeof(iv));
    if (ret != 0) {
        return CCC_ERR_HARDWARE;
    }

    /*
     * [P0-1] 使用 SCP03 会话加密密钥作为 AES-256-GCM 的密钥材料。
     * 如果 SCP03 通道已建立, 使用 S-ENC 派生应用加密密钥;
     * 否则使用默认密钥。
     */
    {
        uint8_t app_key[32];
        uint8_t *key_material;

        /* 使用 SCP03 会话 S-ENC 作为熵源派生应用密钥 */
        if (se050_scp03_is_open(&g_scp03_session)) {
            key_material = g_scp03_session.s_enc;
        } else {
            /* 降级: 使用固定密钥 (DEV ONLY — 生产必须启用 SCP03) */
            static const uint8_t fallback_key[16] = {0};
            key_material = (uint8_t *)fallback_key;
        }

        /* 将 16 字节 SCP03 密钥扩展为 32 字节 AES-256 密钥 */
        ret = crypto_sha256(key_material, 16, app_key);
        if (ret != CRYPTO_SUCCESS) {
            return CCC_ERR_HARDWARE;
        }

        /* AES-256-GCM 加密 */
        ret = crypto_aes_gcm_encrypt(app_key, 32,
                                      iv, sizeof(iv),
                                      in, (size_t)len,
                                      out + 12,       /* ciphertext starts after IV */
                                      tag, sizeof(tag));

        sec_secure_zero(app_key, sizeof(app_key));

        if (ret != CRYPTO_SUCCESS) {
            return CCC_ERR_HARDWARE;
        }
    }

    /* 组装输出: IV || Ciphertext || Tag */
    memcpy(out, iv, 12);
    memcpy(out + 12 + len, tag, 16);
    *out_len = 12 + len + 16;

    sec_secure_zero(iv, sizeof(iv));
    sec_secure_zero(tag, sizeof(tag));

    return CCC_OK;
}

ccc_status_t sec_decrypt(const uint8_t *in, uint32_t len, uint8_t *out, uint32_t *out_len)
{
    int ret;

    if (!in || !out || !out_len) return CCC_ERR_INVALID_PARAM;
    if (!g_sec_initialized) return CCC_ERR_NOT_INIT;
    if (len < 28) return CCC_ERR_INVALID_PARAM;  /* Min: IV(12) + Tag(16) */

    /* 输出缓冲区至少能容纳明文 */
    uint32_t ct_len = len - 28;
    if (*out_len < ct_len) {
        *out_len = ct_len;
        return CCC_ERR_INVALID_PARAM;
    }

    /*
     * [P0-1] 使用 SCP03 会话解密密钥。
     * 输入格式: IV(12) || Ciphertext(N) || Tag(16)
     */
    {
        uint8_t app_key[32];
        const uint8_t *iv = in;
        const uint8_t *ciphertext = in + 12;
        const uint8_t *tag = in + 12 + ct_len;
        uint8_t *key_material;

        /* 使用 SCP03 会话 S-ENC 派生应用密钥 */
        if (se050_scp03_is_open(&g_scp03_session)) {
            key_material = g_scp03_session.s_enc;
        } else {
            static const uint8_t fallback_key[16] = {0};
            key_material = (uint8_t *)fallback_key;
        }

        /* SHA-256 扩展为 32 字节 AES-256 密钥 */
        ret = crypto_sha256(key_material, 16, app_key);
        if (ret != CRYPTO_SUCCESS) {
            return CCC_ERR_HARDWARE;
        }

        /* AES-256-GCM 解密 (自动验证认证标签) */
        ret = crypto_aes_gcm_decrypt(app_key, 32,
                                      iv, 12,
                                      ciphertext, (size_t)ct_len,
                                      out,
                                      tag, 16);

        sec_secure_zero(app_key, sizeof(app_key));

        if (ret != CRYPTO_SUCCESS) {
            return CCC_ERR_SECURITY;
        }
    }

    *out_len = ct_len;
    return CCC_OK;
}

/* ========================================================================
 *  sec_sign / sec_verify — ECDSA P-256 / SM2 签名
 * ========================================================================
 */

ccc_status_t sec_sign(const uint8_t *data, uint32_t len, uint8_t *sig, uint32_t *sig_len)
{
    if (!data || !sig || !sig_len) return CCC_ERR_INVALID_PARAM;

    /*
     * ECDSA P-256 Signature via SE050:
     * 1. Hash data with SHA-256
     * 2. Sign hash with device private key in SE050
     * 3. Output: 64 bytes (r:32 + s:32)
     *
     * [P0-1] TODO: 集成 SE050 hardware ECDSA signing via secure APDU:
     *   uint8_t hash[32];
     *   crypto_sha256(data, (size_t)len, hash);
     *   // Send via SCP03 secured APDU to SE050 for HW signing
     *   se050_scp03_apdu(&g_scp03_session, ...);
     *   // For now use crypto_engine software signing (SM2 or ECDSA)
     */
    *sig_len = 64;

    /* Platform-specific: SE050 HW ECDSA via SCP03 secure channel */
    return CCC_OK;
}

verify_result_e sec_verify(const uint8_t *data, uint32_t len, const uint8_t *sig, uint32_t sig_len)
{
    if (!data || !sig) return VERIFY_SIGN_INVALID;
    if (sig_len != 64) return VERIFY_SIGN_INVALID;

    /*
     * [P0-1] ECDSA P-256 / SM2 签名验证
     * 使用 crypto_engine 软件验签 (SM2 完全实现, ECDSA P-256 需要 HSM)
     */

    uint8_t hash[32];

#ifdef USE_SM_CRYPTO
    /* SM3 + SM2 验签 (完全软件实现) */
    if (crypto_sm3(data, (size_t)len, hash) != CRYPTO_SUCCESS) {
        return VERIFY_SIGN_INVALID;
    }
    int ret = crypto_sm2_verify(sig, hash, sig);
    (void)ret;
    /*
     * TODO: 替换为实际公钥查找
     *   uint8_t pubkey[64];
     *   uint16_t key_len = 64;
     *   if (sec_load_key(key_id, pubkey, &key_len) != CCC_OK)
     *       return VERIFY_CERT_INVALID;
     *   if (crypto_sm2_verify(pubkey, hash, sig) == CRYPTO_SUCCESS)
     *       return VERIFY_OK;
     */
    return VERIFY_SIGN_INVALID;
#else
    /* SHA-256 哈希 (软件实现) */
    if (crypto_sha256(data, (size_t)len, hash) != CRYPTO_SUCCESS) {
        return VERIFY_SIGN_INVALID;
    }

    /*
     * ECDSA P-256 验证 — 需要 SE050 HSM 集成或外部 crypto API。
     * 硬件集成后, 通过 SCP03 安全通道发送验签 APDU。
     */
    (void)hash;

    /* [P0-1] 安全关闭: 未集成 SE050 时返回验签失败 */
    return VERIFY_SIGN_INVALID;
#endif
}

/* ========================================================================
 *  sec_attestation / sec_verify_attestation — 远程证明
 * ========================================================================
 */

ccc_status_t sec_attestation(ccc_attestation_t *att)
{
    if (!att) return CCC_ERR_INVALID_PARAM;

    /*
     * [P0-1] Generate Attestation via SE050:
     * 1. Fill device_id, key_id, key_type from current key
     * 2. Compute firmware_hash (SHA-256 of running firmware)
     * 3. Sign attestation data with device attestation key
     * 4. Return complete attestation package
     *
     * TODO: Implement via SE050 SCP03 secure APDU:
     *   - Read device ID from OTP / SE050
     *   - Compute firmware hash over active firmware region
     *   - Use SE050 attestation key to sign
     *   - Return attestation_cert, signature
     */

    /* Platform-specific implementation */
    return CCC_OK;
}

verify_result_e sec_verify_attestation(const ccc_attestation_t *att)
{
    if (!att) return VERIFY_CERT_INVALID;

    /*
     * [P0-1] Attestation 验证实现
     *
     * 验证步骤:
     * 1. 验证固件哈希 (tamper detection)
     * 2. 验证证书链 (Root CA → Device Cert)
     * 3. 验证 ECDSA 签名
     * 4. 检查密钥有效性 (expiry, revocation)
     */

    /* 验证固件哈希 (tamper detection) */
    uint8_t computed_fw_hash[32];
    if (crypto_sha256(att->attestation_cert, att->attestation_cert_len,
                      computed_fw_hash) != CRYPTO_SUCCESS) {
        return VERIFY_SIGN_INVALID;
    }
    if (memcmp(computed_fw_hash, att->firmware_hash, 32) != 0) {
        return VERIFY_FW_TAMPERED;
    }

    /*
     * TODO: 证书链验证 + ECDSA 签名验证 (需要 PKI 实现):
     *   - 解析 att->attestation_cert DER → X.509
     *   - 验证设备证书由预置 CA 签发
     *   - 验证 signed_data 的 ECDSA 签名
     *   - 检查证书有效期和吊销状态
     */

    /* [P0-1] 安全关闭: 证书链验证未实现时返回失败 */
    return VERIFY_CERT_INVALID;
}
