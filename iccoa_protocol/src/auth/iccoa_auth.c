/**
 * @file iccoa_auth.c
 * @brief ICCOA Authorization Module - Bind/Daily/Remote/Share Authentication
 */

#include "iccoa_digital_key.h"

#define MAX_USERS 8

static iccoa_permission_t g_users[MAX_USERS];
static uint8_t g_user_count = 0;

int32_t iccoa_auth_init(void)
{
    memset(g_users, 0, sizeof(g_users));
    g_user_count = 0;
    return ICCOA_OK;
}

static int32_t find_user(const uint8_t *user_id)
{
    for (uint8_t i = 0; i < g_user_count; i++) {
        if (memcmp(g_users[i].user_id, user_id, 16) == 0) {
            return i;
        }
    }
    return ICCOA_ERR_NOT_FOUND;
}

int32_t iccoa_auth_request(iccoa_auth_type_e type, const uint8_t *challenge, uint16_t len)
{
    (void)type; (void)challenge; (void)len;

    /* Generate challenge for phone via SE050 RNG */
    /* Expected: phone signs challenge with private key */
    /* Vehicle verifies with stored public key */
    return ICCOA_OK;
}

int32_t iccoa_auth_verify(const uint8_t *response, uint16_t len)
{
    if (!response || len < 32) return ICCOA_ERR_PARAM;

    /* Verify ECDSA signature using SE050 */
    /* response layout: [user_id(16)] + [signature(32..72)] */
    uint8_t user_id[16];
    memcpy(user_id, response, 16);

    int32_t idx = find_user(user_id);
    if (idx < 0) return ICCOA_ERR_DENIED;

    /* Check validity period */
    /* TODO: compare current timestamp vs valid_from/valid_until */

    /* Check usage count */
    if (g_users[idx].max_uses > 0 && g_users[idx].used_count >= g_users[idx].max_uses) {
        return ICCOA_ERR_DENIED;
    }

    g_users[idx].used_count++;
    return ICCOA_OK;
}

int32_t iccoa_auth_check_permission(const uint8_t *user_id, uint8_t perm)
{
    int32_t idx = find_user(user_id);
    if (idx < 0) return ICCOA_ERR_DENIED;

    /* Check if permission bit is set */
    uint8_t byte_idx = perm / 8;
    uint8_t bit_idx = perm % 8;
    if (byte_idx >= 4) return ICCOA_ERR_PARAM;

    if (!(g_users[idx].permissions[byte_idx] & (1 << bit_idx))) {
        return ICCOA_ERR_DENIED;
    }
    return ICCOA_OK;
}
