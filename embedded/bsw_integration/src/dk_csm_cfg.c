/**
 * @file dk_csm_cfg.c
 * @brief yuleDKCS CSM 密钥/作业配置
 *
 * ICCE Digital Key 密钥配置:
 *   - 主密钥 (Key#1): ICCE OEM 主密钥存储, 用于派生
 *   - 会话密钥 (Key#2): BLE 加密会话
 *   - 存储密钥 (Key#3): NVRAM 加密保护
 *   - 诊断密钥 (Key#4): UDS 安全访问
 *   - 安全启动密钥 (Key#5): 固件签名验证
 *   - 通信密钥 (Key#6): UWB 安全测距
 *
 * 兼容: SE050 Secure Element, ATECC608B
 */

#include "Csm.h"
#include "Csm_Cfg.h"
#include "Csm_Types.h"

/* ============================================================================
 * 密钥元素配置
 * ============================================================================ */

/** @brief 主密钥元素 */
static const Csm_KeyElementConfigType Dk_MasterKey_Elements[] = {
    { CSM_KEY_ELEMENT_ID_SECRET, CSM_KEY_ELEMENT_TYPE_SECRET, 32U, TRUE,  TRUE,  FALSE },
    { CSM_KEY_ELEMENT_ID_PUBLIC, CSM_KEY_ELEMENT_TYPE_PUBLIC, 64U, TRUE,  TRUE,  FALSE },
    { CSM_KEY_ELEMENT_ID_IV,     CSM_KEY_ELEMENT_TYPE_IV,     12U, TRUE,  TRUE,  FALSE }
};

/** @brief 会话密钥元素 */
static const Csm_KeyElementConfigType Dk_SessionKey_Elements[] = {
    { CSM_KEY_ELEMENT_ID_SECRET, CSM_KEY_ELEMENT_TYPE_SECRET, 16U, FALSE, TRUE,  FALSE },
    { CSM_KEY_ELEMENT_ID_IV,     CSM_KEY_ELEMENT_TYPE_IV,     12U, TRUE,  TRUE,  FALSE }
};

/** @brief 存储密钥元素 */
static const Csm_KeyElementConfigType Dk_StorageKey_Elements[] = {
    { CSM_KEY_ELEMENT_ID_SECRET, CSM_KEY_ELEMENT_TYPE_SECRET, 16U, FALSE, TRUE,  FALSE },
    { CSM_KEY_ELEMENT_ID_IV,     CSM_KEY_ELEMENT_TYPE_IV,     16U, TRUE,  TRUE,  FALSE }
};

/** @brief 诊断密钥元素 */
static const Csm_KeyElementConfigType Dk_DiagKey_Elements[] = {
    { CSM_KEY_ELEMENT_ID_SECRET, CSM_KEY_ELEMENT_TYPE_SECRET, 16U, TRUE,  TRUE,  FALSE }
};

/** @brief 安全启动密钥元素 */
static const Csm_KeyElementConfigType Dk_SecureBoot_Elements[] = {
    { CSM_KEY_ELEMENT_ID_PUBLIC, CSM_KEY_ELEMENT_TYPE_PUBLIC,  64U, TRUE,  FALSE, FALSE },
    { CSM_KEY_ELEMENT_ID_PRIVATE,CSM_KEY_ELEMENT_TYPE_PRIVATE, 32U, FALSE, TRUE,  FALSE }
};

/** @brief 通信密钥元素 */
static const Csm_KeyElementConfigType Dk_CommKey_Elements[] = {
    { CSM_KEY_ELEMENT_ID_SECRET, CSM_KEY_ELEMENT_TYPE_SECRET, 16U, TRUE,  TRUE,  FALSE },
    { CSM_KEY_ELEMENT_ID_IV,     CSM_KEY_ELEMENT_TYPE_IV,     12U, TRUE,  TRUE,  FALSE }
};

/* ============================================================================
 * 密钥配置表
 * ============================================================================ */

static const Csm_KeyConfigType Dk_KeyConfigs[] = {
    {
        .keyId         = CSM_KEY_ID_MASTER,
        .allowedUsage  = CSM_KEY_USAGE_DERIVE | CSM_KEY_USAGE_ENCRYPT | CSM_KEY_USAGE_MAC_GENERATE,
        .elements      = Dk_MasterKey_Elements,
        .numElements   = (uint8)(sizeof(Dk_MasterKey_Elements) / sizeof(Dk_MasterKey_Elements[0])),
        .cryptoKeyType = 0U
    },
    {
        .keyId         = CSM_KEY_ID_SESSION,
        .allowedUsage  = CSM_KEY_USAGE_ENCRYPT | CSM_KEY_USAGE_DECRYPT | CSM_KEY_USAGE_MAC_GENERATE | CSM_KEY_USAGE_MAC_VERIFY,
        .elements      = Dk_SessionKey_Elements,
        .numElements   = (uint8)(sizeof(Dk_SessionKey_Elements) / sizeof(Dk_SessionKey_Elements[0])),
        .cryptoKeyType = 0U
    },
    {
        .keyId         = CSM_KEY_ID_STORAGE,
        .allowedUsage  = CSM_KEY_USAGE_ENCRYPT | CSM_KEY_USAGE_DECRYPT | CSM_KEY_USAGE_MAC_GENERATE,
        .elements      = Dk_StorageKey_Elements,
        .numElements   = (uint8)(sizeof(Dk_StorageKey_Elements) / sizeof(Dk_StorageKey_Elements[0])),
        .cryptoKeyType = 0U
    },
    {
        .keyId         = CSM_KEY_ID_DIAGNOSTIC,
        .allowedUsage  = CSM_KEY_USAGE_ENCRYPT | CSM_KEY_USAGE_DECRYPT,
        .elements      = Dk_DiagKey_Elements,
        .numElements   = (uint8)(sizeof(Dk_DiagKey_Elements) / sizeof(Dk_DiagKey_Elements[0])),
        .cryptoKeyType = 0U
    },
    {
        .keyId         = CSM_KEY_ID_SECURE_BOOT,
        .allowedUsage  = CSM_KEY_USAGE_VERIFY | CSM_KEY_USAGE_SIGN,
        .elements      = Dk_SecureBoot_Elements,
        .numElements   = (uint8)(sizeof(Dk_SecureBoot_Elements) / sizeof(Dk_SecureBoot_Elements[0])),
        .cryptoKeyType = 0U
    },
    {
        .keyId         = CSM_KEY_ID_COMMUNICATION,
        .allowedUsage  = CSM_KEY_USAGE_ENCRYPT | CSM_KEY_USAGE_DECRYPT | CSM_KEY_USAGE_MAC_GENERATE | CSM_KEY_USAGE_MAC_VERIFY,
        .elements      = Dk_CommKey_Elements,
        .numElements   = (uint8)(sizeof(Dk_CommKey_Elements) / sizeof(Dk_CommKey_Elements[0])),
        .cryptoKeyType = 0U
    }
};

/* ============================================================================
 * 作业配置表
 * ============================================================================ */

static const Csm_JobConfigType Dk_JobConfigs[] = {
    {
        .jobId        = CSM_JOB_ID_HASH_DEFAULT,
        .serviceType  = CSM_SERVICE_HASH,
        .priority     = CSM_JOB_PRIORITY_NORMAL,
        .keyId        = CSM_KEY_ID_NONE,
        .algorithm    = {
            .family    = CSM_ALGOFAM_SHA2_256,
            .mode      = CSM_ALGOMODE_NOT_SET,
            .classType = CSM_ALGOCLASS_HASH,
            .keyLength = 0U
        },
        .asynchronous = FALSE,
        .callbackId   = 0U
    },
    {
        .jobId        = CSM_JOB_ID_ENCRYPT_DEFAULT,
        .serviceType  = CSM_SERVICE_ENCRYPT,
        .priority     = CSM_JOB_PRIORITY_HIGH,
        .keyId        = CSM_KEY_ID_SESSION,
        .algorithm    = {
            .family    = CSM_ALGOFAM_AES,
            .mode      = CSM_ALGOMODE_GCM,
            .classType = CSM_ALGOCLASS_CIPHER,
            .keyLength = 128U
        },
        .asynchronous = FALSE,
        .callbackId   = 0U
    },
    {
        .jobId        = CSM_JOB_ID_DECRYPT_DEFAULT,
        .serviceType  = CSM_SERVICE_DECRYPT,
        .priority     = CSM_JOB_PRIORITY_HIGH,
        .keyId        = CSM_KEY_ID_SESSION,
        .algorithm    = {
            .family    = CSM_ALGOFAM_AES,
            .mode      = CSM_ALGOMODE_GCM,
            .classType = CSM_ALGOCLASS_CIPHER,
            .keyLength = 128U
        },
        .asynchronous = FALSE,
        .callbackId   = 0U
    },
    {
        .jobId        = CSM_JOB_ID_MAC_GENERATE_DEFAULT,
        .serviceType  = CSM_SERVICE_MAC_GENERATE,
        .priority     = CSM_JOB_PRIORITY_NORMAL,
        .keyId        = CSM_KEY_ID_SESSION,
        .algorithm    = {
            .family    = CSM_ALGOFAM_HMAC,
            .mode      = CSM_ALGOMODE_NOT_SET,
            .classType = CSM_ALGOCLASS_MAC,
            .keyLength = 256U,
            .secondaryFamily = (const void*)CSM_ALGOFAM_SHA2_256
        },
        .asynchronous = FALSE,
        .callbackId   = 0U
    },
    {
        .jobId        = CSM_JOB_ID_MAC_VERIFY_DEFAULT,
        .serviceType  = CSM_SERVICE_MAC_VERIFY,
        .priority     = CSM_JOB_PRIORITY_NORMAL,
        .keyId        = CSM_KEY_ID_SESSION,
        .algorithm    = {
            .family    = CSM_ALGOFAM_HMAC,
            .mode      = CSM_ALGOMODE_NOT_SET,
            .classType = CSM_ALGOCLASS_MAC,
            .keyLength = 256U,
            .secondaryFamily = (const void*)CSM_ALGOFAM_SHA2_256
        },
        .asynchronous = FALSE,
        .callbackId   = 0U
    },
    {
        .jobId        = CSM_JOB_ID_SIGN_DEFAULT,
        .serviceType  = CSM_SERVICE_SIGNATURE_GENERATE,
        .priority     = CSM_JOB_PRIORITY_HIGH,
        .keyId        = CSM_KEY_ID_SECURE_BOOT,
        .algorithm    = {
            .family    = CSM_ALGOFAM_ECDSA,
            .mode      = CSM_ALGOMODE_NOT_SET,
            .classType = CSM_ALGOCLASS_SIGNATURE,
            .keyLength = 256U
        },
        .asynchronous = FALSE,
        .callbackId   = 0U
    },
    {
        .jobId        = CSM_JOB_ID_VERIFY_DEFAULT,
        .serviceType  = CSM_SERVICE_SIGNATURE_VERIFY,
        .priority     = CSM_JOB_PRIORITY_HIGH,
        .keyId        = CSM_KEY_ID_SECURE_BOOT,
        .algorithm    = {
            .family    = CSM_ALGOFAM_ECDSA,
            .mode      = CSM_ALGOMODE_NOT_SET,
            .classType = CSM_ALGOCLASS_SIGNATURE,
            .keyLength = 256U
        },
        .asynchronous = FALSE,
        .callbackId   = 0U
    },
    {
        .jobId        = CSM_JOB_ID_RANDOM_DEFAULT,
        .serviceType  = CSM_SERVICE_RANDOM_GENERATE,
        .priority     = CSM_JOB_PRIORITY_IMMEDIATE,
        .keyId        = CSM_KEY_ID_NONE,
        .algorithm    = {
            .family    = CSM_ALGOFAM_DRBG,
            .mode      = CSM_ALGOMODE_NOT_SET,
            .classType = CSM_ALGOCLASS_RANDOM,
            .keyLength = 256U
        },
        .asynchronous = FALSE,
        .callbackId   = 0U
    }
};

/* ============================================================================
 * CSM 全局配置实例
 * ============================================================================ */
const Csm_ConfigType Csm_Config = {
    .keys                  = Dk_KeyConfigs,
    .numKeys               = (uint8)(sizeof(Dk_KeyConfigs) / sizeof(Dk_KeyConfigs[0])),
    .jobs                  = Dk_JobConfigs,
    .numJobs               = (uint8)(sizeof(Dk_JobConfigs) / sizeof(Dk_JobConfigs[0])),
    .useAsyncMode          = TRUE,
    .queueProcessingPeriod = 10U,
    .devErrorDetect        = TRUE
};
