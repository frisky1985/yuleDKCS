/**
 * @file icce_security.c
 * @brief ICCE Security Module - Bind/Auth/Session Verification via SE050
 */

#include "icce_digital_key.h"

#define MAX_BOUND_DEVICES  8

typedef struct {
    uint8_t device_pubkey[64];  /* ECDH P-256 public key */
    uint8_t device_id[16];
    uint8_t key_slot;           /* SE050 key slot */
    bool    active;
} icce_bound_device_t;

static icce_bound_device_t g_devices[MAX_BOUND_DEVICES];
static uint8_t g_device_count = 0;

int32_t icce_security_init(void)
{
    memset(g_devices, 0, sizeof(g_devices));
    g_device_count = 0;
    return ICCE_OK;
}

int32_t icce_security_bind(const uint8_t *device_pubkey, uint16_t len)
{
    if (!device_pubkey || len != 64) return ICCE_ERR_PARAM;
    if (g_device_count >= MAX_BOUND_DEVICES) return ICCE_ERR_NO_MEM;

    /* Store device public key in SE050 */
    /* Generate shared ECDH secret */
    /* Assign key slot */
    icce_bound_device_t *dev = &g_devices[g_device_count];
    memcpy(dev->device_pubkey, device_pubkey, 64);
    dev->key_slot = g_device_count + 1;
    dev->active = true;

    /* TODO: SE050 key import */
    /* TODO: Verify device certificate chain */

    g_device_count++;
    return ICCE_OK;
}

int32_t icce_security_auth(const uint8_t *challenge, uint16_t chal_len,
                           const uint8_t *signature, uint16_t sig_len)
{
    if (!challenge || !signature) return ICCE_ERR_PARAM;

    /* Verify ECDSA signature against stored device public keys */
    /* challenge: 16 bytes random from vehicle */
    /* signature: 64 bytes (r || s) P-256 */

    for (uint8_t i = 0; i < g_device_count; i++) {
        if (!g_devices[i].active) continue;

        /* TODO: SE050 ECDSA verify */
        /* if (verify succeeds) return ICCE_OK; */
    }

    return ICCE_ERR_SECURITY;
}

int32_t icce_security_verify_session(uint16_t session_id)
{
    (void)session_id;
    /* Check if UWB session is authenticated */
    /* Verify session token matches bound device */
    return ICCE_OK;
}
