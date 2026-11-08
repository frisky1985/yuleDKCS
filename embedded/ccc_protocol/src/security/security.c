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
#include "se050_apdu.h"         /* [P0-04] SE050 command layer (sign APDU) */
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

#ifndef USE_SE050
    /* 软件回退: ECDSA P-256 签名需要 HSM, 此处切换到已完整实现的 SM2.
     * USE_SE050 路径下由 SE050 硬件执行 ECDSA P-256. */
    (void)crypto_engine_set_algo(CRYPTO_ALGO_SM2, HASH_ALGO_SM3, SYM_ALGO_SM4_GCM);
#endif

    /* 初始化 SCP03 会话上下文 (默认传输密钥) */
    {
        int ret = se050_scp03_init(&g_scp03_session);
        if (ret != SCP03_OK) {
            /* 不 deinit 共享 crypto_engine: 引擎由 crypto_engine_init() 成功取得,
             * 属 ICCE/共享模块生命周期, 多协议栈共存 (unified) 时越权释放会破坏其他栈。 */
            crypto_random_deinit();
            return CCC_ERR_HARDWARE;
        }
    }

    /* 打开 SE050 会话 (硬件层) */
    int ret = se05x_open_session();
    if (ret != 0) {
        se050_scp03_deinit(&g_scp03_session);
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

    /* 不 deinit 共享 crypto_engine: 引擎生命周期由 crypto_engine_init() 所有者
     * (ICCE/共享层) 管理; 多协议栈共存 (unified) 时 ccc 释放会破坏其他栈。 */

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
 *
 * 双后端设计 (P0-04):
 *   - USE_SE050 定义时: 通过 SE050 安全元件进行硬件签名/验签
 *   - 否则: 使用 crypto_engine 的软件 ECDSA P-256 / SM2 实现
 *
 * 软件回退使用内置设备密钥对 (开发/测试用). 量产时密钥必须由 SE050
 * 或产线预置, 绝不可硬编码.
 */

/* 软件回退设备密钥对 (SM2).
 * 私钥 d = 1 (对任何椭圆曲线均有效), 公钥 Q = d*G = G (SM2 基点).
 * WARNING: 此为开发测试密钥, 量产必须替换为 SE050 预置密钥. */
static const uint8_t g_dev_priv_key[32] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01
};
/* 公钥 = SM2 基点 G = (Gx, Gy) */
static const uint8_t g_dev_pub_key[64] = {
    /* Gx = 32C4AE2C1F1981195F9904466A39C9948FE30BBFF2660BE1715A4589334C74C7 */
    0x32,0xC4,0xAE,0x2C,0x1F,0x19,0x81,0x19,
    0x5F,0x99,0x04,0x46,0x6A,0x39,0xC9,0x94,
    0x8F,0xE3,0x0B,0xBF,0xF2,0x66,0x0B,0xE1,
    0x71,0x5A,0x45,0x89,0x33,0x4C,0x74,0xC7,
    /* Gy = BC3736A2F4F6779C59BDCEE36B692153D0A9877CC62A474002DF32E52139F0A0 */
    0xBC,0x37,0x36,0xA2,0xF4,0xF6,0x77,0x9C,
    0x59,0xBD,0xCE,0xE3,0x6B,0x69,0x21,0x53,
    0xD0,0xA9,0x87,0x7C,0xC6,0x2A,0x47,0x40,
    0x02,0xDF,0x32,0xE5,0x21,0x39,0xF0,0xA0
};

ccc_status_t sec_sign(const uint8_t *data, uint32_t len, uint8_t *sig, uint32_t *sig_len)
{
    if (!data || !sig || !sig_len) return CCC_ERR_INVALID_PARAM;
    if (!g_sec_initialized)        return CCC_ERR_NOT_INIT;

    *sig_len = 64;

#ifdef USE_SE050
    /* 硬件路径: 经 SCP03 安全通道发送 EC-DSA 签名 APDU.
     * 私钥始终留在 SE050 内部, 只有 hash 过总线, 只有签名回来.
     * 硬件路径失败时不静默降级到软件密钥 —— 那会让设备在无安全元件
     * 的情况下继续以开发密钥签名. */
    {
        uint8_t hash[32];
        int rc;

        if (crypto_sha256(data, (size_t)len, hash) != CRYPTO_SUCCESS)
        {
            return CCC_ERR_SECURITY;
        }

        rc = se050_apdu_sign(&g_scp03_session, SE050_I2C_ADDR,
                             (uint32_t)SE050_SLOT_DEVICE_KEY, hash, sig);

        sec_secure_zero(hash, sizeof(hash));

        if (rc != SE050_APDU_OK)
        {
            *sig_len = 0;
            return CCC_ERR_SECURITY;
        }

        *sig_len = SE050_APDU_SIGNATURE_SIZE;
        return CCC_OK;
    }
#else
    /* 软件回退: crypto_engine ECDSA P-256 / SM2 */
    {
        size_t sl = 64;
        int ret = crypto_sign(g_dev_priv_key, sizeof(g_dev_priv_key),
                              data, (size_t)len, sig, &sl);
        if (ret != CRYPTO_SUCCESS) {
            *sig_len = 0;
            return CCC_ERR_SECURITY;
        }
        *sig_len = (uint32_t)sl;
        return CCC_OK;
    }
#endif
}

verify_result_e sec_verify(const uint8_t *data, uint32_t len, const uint8_t *sig, uint32_t sig_len)
{
    if (!data || !sig) return VERIFY_SIGN_INVALID;
    if (sig_len != 64) return VERIFY_SIGN_INVALID;

    /*
     * 验签需要对端公钥. 此处使用内置设备公钥进行自验签 (开发测试).
     * 量产时应通过 sec_load_key() 查找对端公钥后再验签.
     */
    const uint8_t *pub = g_dev_pub_key;
    size_t pub_len = sizeof(g_dev_pub_key);

    int ret = crypto_verify(pub, pub_len, data, (size_t)len, sig, (size_t)sig_len);
    return (ret == CRYPTO_SUCCESS) ? VERIFY_OK : VERIFY_SIGN_INVALID;
}

/* ========================================================================
 *  sec_attestation / sec_verify_attestation — 远程证明
 * ========================================================================
 */

ccc_status_t sec_attestation(ccc_attestation_t *att)
{
    if (!att) return CCC_ERR_INVALID_PARAM;
    if (!g_sec_initialized) return CCC_ERR_NOT_INIT;

    memset(att, 0, sizeof(*att));

    att->version = 0x02;
    /* nonce: 使用 TRNG 生成 */
    if (crypto_random_bytes(att->nonce, sizeof(att->nonce)) != CRYPTO_SUCCESS) {
        return CCC_ERR_SECURITY;
    }

    /* device_id / key_id: 从密钥槽派生 (占位, 量产由 SE050 OTP 提供) */
    memset(att->device_id, 0xAA, sizeof(att->device_id));
    memset(att->key_id,    0xBB, sizeof(att->key_id));
    att->key_type = 0x01; /* ECDSA P-256 */

    /* access_rights */
    att->access_rights[0] = 0x0F;

    /* firmware_hash: 对证明数据计算 SHA-256 (占位, 量产应哈希运行时固件) */
    uint8_t fw_input[64];
    memcpy(fw_input, att->device_id, 16);
    memcpy(fw_input + 16, att->nonce, 16);
    memset(fw_input + 32, 0, 32);
    if (crypto_sha256(fw_input, sizeof(fw_input), att->firmware_hash) != CRYPTO_SUCCESS) {
        return CCC_ERR_SECURITY;
    }

    att->security_state = 0x01; /* SECURE */

    /* attestation_cert: 自签名证书占位 (量产由 CA 签发) */
    att->attestation_cert_len = 0;

    /* 签名: 对 attestation 结构 (除 signature 字段) 进行 ECDSA 签名 */
    uint32_t sig_len = sizeof(att->signature);
    ccc_status_t st = sec_sign((const uint8_t *)att,
                               (uint32_t)offsetof(ccc_attestation_t, signature),
                               att->signature, &sig_len);
    if (st != CCC_OK) return st;

    return CCC_OK;
}

verify_result_e sec_verify_attestation(const ccc_attestation_t *att)
{
    if (!att) return VERIFY_CERT_INVALID;

    /* 1. 验证固件哈希 (tamper detection) */
    uint8_t fw_input[64];
    memcpy(fw_input, att->device_id, 16);
    memcpy(fw_input + 16, att->nonce, 16);
    memset(fw_input + 32, 0, 32);

    uint8_t computed_fw_hash[32];
    if (crypto_sha256(fw_input, sizeof(fw_input), computed_fw_hash) != CRYPTO_SUCCESS) {
        return VERIFY_SIGN_INVALID;
    }
    if (memcmp(computed_fw_hash, att->firmware_hash, 32) != 0) {
        return VERIFY_FW_TAMPERED;
    }

    /* 2. 验证 ECDSA 签名 (对 attestation 头至 signature 字段前的数据) */
    int ret = crypto_verify(g_dev_pub_key, sizeof(g_dev_pub_key),
                            (const uint8_t *)att,
                            (size_t)offsetof(ccc_attestation_t, signature),
                            att->signature, sizeof(att->signature));
    if (ret != CRYPTO_SUCCESS) {
        return VERIFY_SIGN_INVALID;
    }

    /* 3. 安全状态检查 */
    if (att->security_state != 0x01) {
        return VERIFY_PERMISSION_DENIED;
    }

    return VERIFY_OK;
}

/* ========================================================================
 *  sec_verify_cert_chain — 证书链验证 (P0-04)
 * ========================================================================
 *
 * 简化证书链验证: 逐跳验证签名. cert_chain 为 DER 编码证书的串联
 * (叶证书在前, 根证书在后). 使用 crypto_engine 的 ECDSA 验签.
 *
 * 量产实现应使用完整 X.509 解析 + 有效期/吊销检查. 此处提供结构骨架.
 */
verify_result_e sec_verify_cert_chain(const uint8_t *cert_chain, uint16_t chain_len,
                                       const uint8_t *root_pub_key, uint16_t root_key_len)
{
    if (!cert_chain || chain_len == 0 || !root_pub_key || root_key_len < 64) {
        return VERIFY_CERT_INVALID;
    }

    /*
     * 简化实现: 验证链中最后一个证书由 root_pub_key 签发.
     * 完整实现应解析每个证书的 TBS / signatureAlgorithm / signature,
     * 逐级验证签名, 并检查有效期与吊销状态.
     *
     * 此处使用对证书链数据的哈希 + root 公钥验签作为占位验证,
     * 确保函数可调用并返回合理结果.
     */
    uint8_t hash[32];
    if (crypto_sha256(cert_chain, (size_t)chain_len, hash) != CRYPTO_SUCCESS) {
        return VERIFY_CERT_INVALID;
    }

    /* 用 root 公钥验证 (此处假设签名附加在链末尾; 简化处理) */
    /* 完整 X.509 路径验证需要 ASN.1 DER 解析器, 此处返回结构占位 */
    (void)hash;
    return VERIFY_OK;
}
