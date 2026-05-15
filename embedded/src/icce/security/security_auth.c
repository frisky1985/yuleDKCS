/**
 * @file security_auth.c
 * @brief 安全认证模块实现
 * @version 1.0
 * @date 2026-05-05
 * 
 * 提供数字钥匙的安全认证功能
 */

#include "security_auth.h"
#include "crypto_engine.h"
#include "hsm_interface.h"
#include <string.h>
#include <stdlib.h>

/* 私有定义 */
#define MAX_SESSIONS            8
#define MAX_STORED_KEYS         32
#define NONCE_CACHE_SIZE        64
#define CHALLENGE_EXPIRY_MS     30000
#define SESSION_EXPIRY_MS       28800000  // 8小时

/* 会话上下文 */
typedef struct {
    session_info_t info;
    uint8_t in_use;
    uint32_t last_activity;
} session_context_t;

/* Nonce缓存项 */
typedef struct {
    uint8_t nonce[16];
    uint32_t timestamp;
    uint8_t used;
} nonce_cache_entry_t;

/* 安全管理器实例 */
typedef struct {
    bool initialized;
    session_context_t sessions[MAX_SESSIONS];
    nonce_cache_entry_t nonce_cache[NONCE_CACHE_SIZE];
    uint32_t nonce_cache_index;
    uint32_t challenge_counter;
    uint32_t session_counter;
} security_manager_t;

/* 全局实例 */
static security_manager_t g_security = {0};

/* 前向声明 */
static int32_t find_free_session_slot(void);
static session_context_t* find_session(uint32_t session_id);
static session_context_t* find_session_by_conn(uint16_t conn_handle);
static bool is_nonce_used(const uint8_t *nonce);
static void mark_nonce_used(const uint8_t *nonce);
static void cleanup_expired_sessions(void);
static void cleanup_expired_nonces(void);

/*============================================================================
 * 公开API实现
 *===========================================================================*/

security_result_t security_init(void)
{
    if (g_security.initialized) {
        return SEC_SUCCESS;
    }
    
    /* 初始化加密引擎 */
    if (crypto_engine_init() != CRYPTO_SUCCESS) {
        return SEC_ERR_HARDWARE_FAULT;
    }
    
    /* 初始化HSM */
    if (hsm_init() != HSM_SUCCESS) {
        return SEC_ERR_HARDWARE_FAULT;
    }
    
    /* 清空会话和Nonce缓存 */
    memset(g_security.sessions, 0, sizeof(g_security.sessions));
    memset(g_security.nonce_cache, 0, sizeof(g_security.nonce_cache));
    
    g_security.nonce_cache_index = 0;
    g_security.challenge_counter = 0;
    g_security.session_counter = 0;
    g_security.initialized = true;
    
    return SEC_SUCCESS;
}

security_result_t security_generate_challenge(auth_challenge_t *challenge)
{
    if (!g_security.initialized || !challenge) {
        return SEC_ERR_KEY_GENERATION_FAILED;
    }
    
    /* 生成随机Nonce */
    if (hsm_generate_random(challenge->nonce, sizeof(challenge->nonce)) != HSM_SUCCESS) {
        return SEC_ERR_KEY_GENERATION_FAILED;
    }
    
    /* 生成服务器随机数 */
    if (hsm_generate_random(challenge->server_random, 
                            sizeof(challenge->server_random)) != HSM_SUCCESS) {
        return SEC_ERR_KEY_GENERATION_FAILED;
    }
    
    /* 设置时间戳和过期时间 */
    challenge->timestamp = 0;  // TODO: 获取实际时间
    challenge->expiry = challenge->timestamp + CHALLENGE_EXPIRY_MS;
    
    /* 记录Nonce */
    mark_nonce_used(challenge->nonce);
    
    g_security.challenge_counter++;
    
    return SEC_SUCCESS;
}

security_result_t security_verify_response(
    const auth_challenge_t *challenge,
    const auth_response_t *response,
    session_info_t *session)
{
    if (!g_security.initialized || !challenge || !response || !session) {
        return SEC_ERR_SIGNATURE_INVALID;
    }
    
    /* 检查挑战是否过期 */
    uint32_t current_time = 0;  // TODO
    if (current_time > challenge->expiry) {
        return SEC_ERR_CHALLENGE_EXPIRED;
    }
    
    /* 构建验证数据 */
    uint8_t verify_data[128];
    uint16_t verify_len = 0;
    
    memcpy(&verify_data[verify_len], challenge->nonce, sizeof(challenge->nonce));
    verify_len += sizeof(challenge->nonce);
    
    memcpy(&verify_data[verify_len], challenge->server_random, 
           sizeof(challenge->server_random));
    verify_len += sizeof(challenge->server_random);
    
    memcpy(&verify_data[verify_len], response->client_random, 
           sizeof(response->client_random));
    verify_len += sizeof(response->client_random);
    
    /* 从响应中提取公钥并验证签名 */
    crypto_key_t public_key = {
        .type = KEY_TYPE_ECC_P256_PUBLIC,
        .length = 64
    };
    memcpy(public_key.data, response->public_key, 64);
    
    if (security_verify_signature(&public_key, verify_data, verify_len,
                                  response->signature, 64) != SEC_SUCCESS) {
        return SEC_ERR_SIGNATURE_INVALID;
    }
    
    /* 创建会话 */
    int32_t slot = find_free_session_slot();
    if (slot < 0) {
        return SEC_ERR_ENCRYPTION_FAILED;
    }
    
    session_context_t *ctx = &g_security.sessions[slot];
    ctx->info.session_id = ++g_security.session_counter;
    ctx->info.creation_time = current_time;
    ctx->info.expiry_time = current_time + SESSION_EXPIRY_MS;
    ctx->info.is_encrypted = 1;
    ctx->in_use = 1;
    ctx->last_activity = current_time;
    
    /* 派生会话密钥 (ECDH) */
    // TODO: 实际的ECDH密钥协商
    
    memcpy(session, &ctx->info, sizeof(session_info_t));
    
    return SEC_SUCCESS;
}

security_result_t security_establish_session(
    uint16_t conn_handle,
    const uint8_t *public_key,
    session_info_t *session)
{
    if (!g_security.initialized || !public_key || !session) {
        return SEC_ERR_KEY_GENERATION_FAILED;
    }
    
    int32_t slot = find_free_session_slot();
    if (slot < 0) {
        return SEC_ERR_ENCRYPTION_FAILED;
    }
    
    session_context_t *ctx = &g_security.sessions[slot];
    
    /* 生成ECDH密钥对 */
    uint8_t my_private_key[32];
    uint8_t my_public_key[64];
    
    if (hsm_generate_ecdh_keypair(my_private_key, my_public_key) != HSM_SUCCESS) {
        return SEC_ERR_KEY_GENERATION_FAILED;
    }
    
    /* 计算共享密钥 */
    uint8_t shared_secret[32];
    if (hsm_ecdh_compute_shared(my_private_key, public_key, 
                                shared_secret) != HSM_SUCCESS) {
        return SEC_ERR_KEY_GENERATION_FAILED;
    }
    
    /* 派生会话密钥 */
    uint8_t key_material[48];
    if (crypto_kdf(shared_secret, 32, key_material, 48) != CRYPTO_SUCCESS) {
        return SEC_ERR_KEY_GENERATION_FAILED;
    }
    
    /* 设置会话 */
    uint32_t current_time = 0;  // TODO
    
    ctx->info.session_id = ++g_security.session_counter;
    ctx->info.conn_handle = conn_handle;
    memcpy(ctx->info.session_key, key_material, 32);
    memcpy(ctx->info.session_id_key, &key_material[32], 16);
    ctx->info.creation_time = current_time;
    ctx->info.expiry_time = current_time + SESSION_EXPIRY_MS;
    ctx->info.is_encrypted = 1;
    ctx->in_use = 1;
    ctx->last_activity = current_time;
    
    /* 清除敏感数据 */
    memset(my_private_key, 0, sizeof(my_private_key));
    memset(shared_secret, 0, sizeof(shared_secret));
    memset(key_material, 0, sizeof(key_material));
    
    memcpy(session, &ctx->info, sizeof(session_info_t));
    
    return SEC_SUCCESS;
}

security_result_t security_encrypt(
    const session_info_t *session,
    const uint8_t *plaintext,
    uint16_t plaintext_len,
    uint8_t *ciphertext,
    uint16_t *ciphertext_len)
{
    if (!g_security.initialized || !session || !plaintext || 
        !ciphertext || !ciphertext_len) {
        return SEC_ERR_INVALID_PARAM;
    }
    
    /* AES-256-GCM加密 */
    uint8_t iv[12];
    uint8_t tag[16];
    
    if (hsm_generate_random(iv, sizeof(iv)) != HSM_SUCCESS) {
        return SEC_ERR_ENCRYPTION_FAILED;
    }
    
    if (crypto_aes_gcm_encrypt(session->session_key, 32,
                               iv, sizeof(iv),
                               plaintext, plaintext_len,
                               ciphertext,
                               tag, sizeof(tag)) != CRYPTO_SUCCESS) {
        return SEC_ERR_ENCRYPTION_FAILED;
    }
    
    /* 构建输出: IV + Ciphertext + Tag */
    uint16_t total_len = sizeof(iv) + plaintext_len + sizeof(tag);
    
    if (*ciphertext_len < total_len) {
        return SEC_ERR_BUFFER_OVERFLOW;
    }
    
    memmove(&ciphertext[sizeof(iv)], ciphertext, plaintext_len);
    memcpy(ciphertext, iv, sizeof(iv));
    memcpy(&ciphertext[sizeof(iv) + plaintext_len], tag, sizeof(tag));
    
    *ciphertext_len = total_len;
    
    return SEC_SUCCESS;
}

security_result_t security_decrypt(
    const session_info_t *session,
    const uint8_t *ciphertext,
    uint16_t ciphertext_len,
    uint8_t *plaintext,
    uint16_t *plaintext_len)
{
    if (!g_security.initialized || !session || !ciphertext || 
        !plaintext || !plaintext_len) {
        return SEC_ERR_INVALID_PARAM;
    }
    
    /* 解析输入: IV + Ciphertext + Tag */
    const uint8_t *iv = ciphertext;
    const uint8_t *enc_data = &ciphertext[12];
    const uint8_t *tag = &ciphertext[ciphertext_len - 16];
    
    uint16_t enc_len = ciphertext_len - 12 - 16;
    
    if (*plaintext_len < enc_len) {
        return SEC_ERR_BUFFER_OVERFLOW;
    }
    
    /* AES-256-GCM解密 */
    if (crypto_aes_gcm_decrypt(session->session_key, 32,
                               iv, 12,
                               enc_data, enc_len,
                               plaintext,
                               tag, 16) != CRYPTO_SUCCESS) {
        return SEC_ERR_DECRYPTION_FAILED;
    }
    
    *plaintext_len = enc_len;
    
    return SEC_SUCCESS;
}

security_result_t security_sign(
    const crypto_key_t *private_key,
    const uint8_t *data,
    uint16_t data_len,
    uint8_t *signature,
    uint16_t *sig_len)
{
    if (!g_security.initialized || !private_key || !data || 
        !signature || !sig_len) {
        return SEC_ERR_INVALID_PARAM;
    }
    
    /* 计算哈希 */
    uint8_t hash[32];
    if (crypto_sha256(data, data_len, hash) != CRYPTO_SUCCESS) {
        return SEC_ERR_ENCRYPTION_FAILED;
    }
    
    /* ECDSA签名 */
    if (hsm_ecdsa_sign(private_key->data, hash, signature, sig_len) != HSM_SUCCESS) {
        return SEC_ERR_ENCRYPTION_FAILED;
    }
    
    return SEC_SUCCESS;
}

security_result_t security_verify_signature(
    const crypto_key_t *public_key,
    const uint8_t *data,
    uint16_t data_len,
    const uint8_t *signature,
    uint16_t sig_len)
{
    if (!g_security.initialized || !public_key || !data || !signature) {
        return SEC_ERR_INVALID_PARAM;
    }
    
    /* 计算哈希 */
    uint8_t hash[32];
    if (crypto_sha256(data, data_len, hash) != CRYPTO_SUCCESS) {
        return SEC_ERR_SIGNATURE_INVALID;
    }
    
    /* ECDSA验证 */
    if (hsm_ecdsa_verify(public_key->data, hash, signature, sig_len) != HSM_SUCCESS) {
        return SEC_ERR_SIGNATURE_INVALID;
    }
    
    return SEC_SUCCESS;
}

security_result_t security_store_key(uint32_t key_id, const crypto_key_t *key)
{
    if (!g_security.initialized || !key) {
        return SEC_ERR_INVALID_PARAM;
    }
    
    /* 存储到HSM安全存储 */
    hsm_key_handle_t handle;
    
    if (hsm_store_key(key_id, key->data, key->length, &handle) != HSM_SUCCESS) {
        return SEC_ERR_KEY_GENERATION_FAILED;
    }
    
    return SEC_SUCCESS;
}

security_result_t security_load_key(uint32_t key_id, crypto_key_t *key)
{
    if (!g_security.initialized || !key) {
        return SEC_ERR_INVALID_PARAM;
    }
    
    /* 从HSM加载密钥 */
    if (hsm_load_key(key_id, key->data, &key->length) != HSM_SUCCESS) {
        return SEC_ERR_KEY_NOT_FOUND;
    }
    
    key->key_id = key_id;
    
    return SEC_SUCCESS;
}

security_result_t security_destroy_session(uint32_t session_id)
{
    if (!g_security.initialized) {
        return SEC_ERR_INVALID_PARAM;
    }
    
    session_context_t *ctx = find_session(session_id);
    if (!ctx) {
        return SEC_ERR_INVALID_PARAM;
    }
    
    /* 清除会话密钥 */
    memset(&ctx->info, 0, sizeof(session_info_t));
    ctx->in_use = 0;
    
    return SEC_SUCCESS;
}

/*============================================================================
 * 私有函数实现
 *===========================================================================*/

static int32_t find_free_session_slot(void)
{
    cleanup_expired_sessions();
    
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (!g_security.sessions[i].in_use) {
            return i;
        }
    }
    return -1;
}

static session_context_t* find_session(uint32_t session_id)
{
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (g_security.sessions[i].in_use &&
            g_security.sessions[i].info.session_id == session_id) {
            return &g_security.sessions[i];
        }
    }
    return NULL;
}

static session_context_t* find_session_by_conn(uint16_t conn_handle)
{
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (g_security.sessions[i].in_use &&
            g_security.sessions[i].info.conn_handle == conn_handle) {
            return &g_security.sessions[i];
        }
    }
    return NULL;
}

static bool is_nonce_used(const uint8_t *nonce)
{
    cleanup_expired_nonces();
    
    for (int i = 0; i < NONCE_CACHE_SIZE; i++) {
        if (g_security.nonce_cache[i].used &&
            memcmp(g_security.nonce_cache[i].nonce, nonce, 16) == 0) {
            return true;
        }
    }
    return false;
}

static void mark_nonce_used(const uint8_t *nonce)
{
    nonce_cache_entry_t *entry = &g_security.nonce_cache[g_security.nonce_cache_index];
    
    memcpy(entry->nonce, nonce, 16);
    entry->timestamp = 0;  // TODO
    entry->used = 1;
    
    g_security.nonce_cache_index = (g_security.nonce_cache_index + 1) % NONCE_CACHE_SIZE;
}

static void cleanup_expired_sessions(void)
{
    uint32_t current_time = 0;  // TODO
    
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (g_security.sessions[i].in_use &&
            g_security.sessions[i].info.expiry_time < current_time) {
            security_destroy_session(g_security.sessions[i].info.session_id);
        }
    }
}

static void cleanup_expired_nonces(void)
{
    uint32_t current_time = 0;  // TODO
    uint32_t max_age = 60000;  // 60秒
    
    for (int i = 0; i < NONCE_CACHE_SIZE; i++) {
        if (g_security.nonce_cache[i].used &&
            (current_time - g_security.nonce_cache[i].timestamp) > max_age) {
            g_security.nonce_cache[i].used = 0;
        }
    }
}

/*============================================================================
 * End of file
 *===========================================================================*/
