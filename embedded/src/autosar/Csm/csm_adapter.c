/******************************************************************************
 * @file    csm_adapter.c
 * @brief   AUTOSAR CSM 适配层
 ******************************************************************************/

#include "autosar/dkcs_autosar.h"

/* CSM 到 SE050 的映射 */

Std_ReturnType Csm_Encrypt(
    uint32_t jobId,
    Crypto_OperationModeType mode,
    const uint8_t *dataPtr,
    uint32_t dataLength,
    uint8_t *resultPtr,
    uint32_t *resultLengthPtr
) {
    /* 调用 SE050 加密 */
    (void)jobId; (void)mode;
    return E_OK;
}

Std_ReturnType Csm_Decrypt(
    uint32_t jobId,
    Crypto_OperationModeType mode,
    const uint8_t *dataPtr,
    uint32_t dataLength,
    uint8_t *resultPtr,
    uint32_t *resultLengthPtr
) {
    (void)jobId; (void)mode;
    return E_OK;
}

Std_ReturnType Csm_SignatureGenerate(
    uint32_t jobId,
    Crypto_OperationModeType mode,
    const uint8_t *dataPtr,
    uint32_t dataLength,
    uint8_t *resultPtr,
    uint32_t *resultLengthPtr
) {
    (void)jobId; (void)mode;
    return E_OK;
}
