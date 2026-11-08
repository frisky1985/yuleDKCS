/**
 * @file se050_apdu.c
 * @module EMB-BSW-SE050-APDU (ASPICE SWE.4)
 * @brief SE050 command layer — ECDSA signature generation over SCP03.
 *
 * See se050_apdu.h for the APDU layout and layering.
 */

#include "se050_apdu.h"

#include <string.h>

int se050_apdu_sign(scp03_session_t *session, uint8_t i2c_addr, uint32_t key_id,
                    const uint8_t hash[SE050_APDU_HASH_SIZE],
                    uint8_t signature[SE050_APDU_SIGNATURE_SIZE])
{
    uint8_t  cmd[SE050_APDU_SIGN_INPUT_SIZE];
    uint8_t  resp[SE050_APDU_SIGNATURE_SIZE];
    uint16_t resp_len = (uint16_t)sizeof(resp);
    int      rc;

    if (session == NULL || hash == NULL || signature == NULL)
    {
        return SE050_APDU_ERR_PARAM;
    }

    if (!se050_scp03_is_open(session))
    {
        return SE050_APDU_ERR_CHANNEL;
    }

    /* Data field: key object id (big-endian) || hash */
    cmd[0] = (uint8_t)(key_id >> 24);
    cmd[1] = (uint8_t)(key_id >> 16);
    cmd[2] = (uint8_t)(key_id >>  8);
    cmd[3] = (uint8_t)(key_id);
    (void)memcpy(&cmd[4], hash, SE050_APDU_HASH_SIZE);

    rc = se050_scp03_apdu_enc(session, i2c_addr,
                              SE050_APDU_CLA, SE050_APDU_INS_ECDSA_SIGN,
                              0x00, 0x00,
                              cmd, (uint16_t)sizeof(cmd),
                              resp, &resp_len);

    se050_scp03_secure_zero(cmd, sizeof(cmd));

    if (rc == SCP03_ERR_SW)
    {
        se050_scp03_secure_zero(resp, sizeof(resp));
        return SE050_APDU_ERR_SW;
    }
    if (rc == SCP03_ERR_CMAC)
    {
        se050_scp03_secure_zero(resp, sizeof(resp));
        return SE050_APDU_ERR_INTEGRITY;
    }
    if (rc != SCP03_OK)
    {
        se050_scp03_secure_zero(resp, sizeof(resp));
        return SE050_APDU_ERR_TRANSPORT;
    }

    if (resp_len != SE050_APDU_SIGNATURE_SIZE)
    {
        se050_scp03_secure_zero(resp, sizeof(resp));
        return SE050_APDU_ERR_RESPONSE;
    }

    (void)memcpy(signature, resp, SE050_APDU_SIGNATURE_SIZE);
    se050_scp03_secure_zero(resp, sizeof(resp));

    return SE050_APDU_OK;
}
