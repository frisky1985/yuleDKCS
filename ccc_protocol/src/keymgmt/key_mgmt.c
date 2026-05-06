/**
 * @file key_mgmt.c
 * @brief CCC Digital Key Management Module
 */

#include "ccc_digital_key.h"

static ccc_digital_key_t g_keys[MAX_KEYS];
static uint8_t g_key_count = 0;

ccc_status_t key_mgmt_init(void)
{
    memset(g_keys, 0, sizeof(g_keys));
    g_key_count = 0;
    /* TODO: Load persisted keys from SE050/Flash */
    return CCC_OK;
}

ccc_status_t key_mgmt_deinit(void)
{
    /* TODO: Persist keys */
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
    memcpy(&g_keys[slot], key, sizeof(ccc_digital_key_t));
    g_keys[slot].state = KEY_STATE_ACTIVE;
    g_key_count++;

    /* TODO: Store key material in SE050 */
    /* sec_store_key(key->se_key_id, key_data); */

    return CCC_OK;
}

ccc_status_t key_delete(const uint8_t *key_id)
{
    if (!key_id) return CCC_ERR_INVALID_PARAM;

    int8_t slot = find_key_slot(key_id);
    if (slot < 0) return CCC_ERR_NOT_FOUND;

    /* TODO: Delete key from SE050 */

    memset(&g_keys[slot], 0, sizeof(ccc_digital_key_t));
    g_key_count--;

    return CCC_OK;
}

ccc_status_t key_get(const uint8_t *key_id, ccc_digital_key_t *key)
{
    if (!key_id || !key) return CCC_ERR_INVALID_PARAM;

    int8_t slot = find_key_slot(key_id);
    if (slot < 0) return CCC_ERR_NOT_FOUND;

    memcpy(key, &g_keys[slot], sizeof(ccc_digital_key_t));
    return CCC_OK;
}

ccc_status_t key_list(ccc_digital_key_t *keys, uint8_t *count)
{
    if (!keys || !count) return CCC_ERR_INVALID_PARAM;

    uint8_t idx = 0;
    for (uint8_t i = 0; i < MAX_KEYS && idx < *count; i++) {
        if (g_keys[i].state != KEY_STATE_INACTIVE) {
            memcpy(&keys[idx], &g_keys[i], sizeof(ccc_digital_key_t));
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
