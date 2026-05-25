/**
 * @file CccDigitalKey.c
 * @brief CCC (Car Connectivity Consortium) 数字钥匙核心实现
 * 
 * 功能: CCC数字钥匙核心功能的实现，包括初始化、模式管理、会话管理
 * 符合CCC Digital Key规范 3.0
 * 
 * @note 此示例已整合到 yuleDKCS/embedded/examples/ccc_reference/
 *       同时引用 yuleDKCS ccc_core 协议栈和 yuleASR BSW 模块
 *
 * @author yuleASR Team / yuleDKCS Team
 * @version 2.0.0
 */

/*==================================================================================================
*                                       包含头文件
==================================================================================================*/
#include "CccDigitalKey.h"
#include "CccIntegration.h"  /* yuleDKCS+ yuleASR 集成说明 */
#include "Csm.h"             /* yuleASR CSM 加密服务管理 */
#include "Det.h"             /* yuleASR Det 开发错误追踪 */

/*
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │ yuleDKCS ccc_core 集成说明                                              │
 * │                                                                         │
 * │ 此文件中的 Ccc_Init() 对应 yuleDKCS 的 ccc_init() 函数:                  │
 * │   error_t ccc_init(const ccc_se_interface_t *se_interface);              │
 * │ 初始化时需要传入安全元件接口(SE050适配器)                                  │
 * │                                                                         │
 * │ Ccc_DeInit() 对应 yuleDKCS 的 ccc_deinit():                             │
 * │   error_t ccc_deinit(void);                                              │
 * │                                                                         │
 * │ Ccc_SignData() 对应 yuleDKCS 安全模块的 sec_sign():                     │
 * │   ccc_status_t sec_sign(const uint8_t *in, uint32_t len,                │
 * │                          uint8_t *out, uint32_t *out_len);              │
 * │                                                                         │
 * │ Ccc_VerifySignature() 对应 yuleDKCS 安全模块的 sec_verify():            │
 * │   verify_result_e sec_verify(const uint8_t *data, uint32_t len,         │
 * │                               const uint8_t *sig, uint32_t sig_len);    │
 * │                                                                         │
 * │ yuleASR BSW 集成说明:                                                   │
 * │ - Csm_Init() / Csm_DeInit(): 初始化/去初始化 CSM 加密服务              │
 * │ - Csm_KeyGenerate(): 通过CSM调用SE050硬件生成ECC密钥对                  │
 * │ - Det_ReportError(): 开发阶段错误追踪                                   │
 * │                                                                         │
 * │ 如需通过 yuleASR OS 调度,可用:                                          │
 * │   #include "Os.h"                                                        │
 * │   Os_CreateTask(TASK_CCC_PROCESS, CccProcessTask, prio);                 │
 * │                                                                         │
 * │ 如需通过 yuleASR NvM 持久化存储钥匙,可用:                                │
 * │   #include "NvM.h"                                                       │
 * │   NvM_WriteBlock(NVM_BLOCK_CCC_KEY, &keyData, sizeof(keyData));          │
 * └─────────────────────────────────────────────────────────────────────────┘
 */

/*==================================================================================================
*                                       宏定义
==================================================================================================*/
/**
 * @brief 检查指针是否为空
 */
#define CCC_CHECK_POINTER(ptr) \
    do { \
        if ((ptr) == NULL) { \
            if (CCC_DEV_ERROR_DETECT == STD_ON) { \
                Det_ReportError(CCC_MODULE_ID, 0, 0, CCC_E_PARAM_POINTER); \
            } \
            return CCC_E_PARAM_POINTER; \
        } \
    } while (0)

/**
 * @brief 检查模块是否已初始化
 */
#define CCC_CHECK_INITIALIZED() \
    do { \
        if (!g_CccRuntime.initialized) { \
            if (CCC_DEV_ERROR_DETECT == STD_ON) { \
                Det_ReportError(CCC_MODULE_ID, 0, 0, CCC_E_NOT_INITIALIZED); \
            } \
            return CCC_E_NOT_INITIALIZED; \
        } \
    } while (0)

/*==================================================================================================
*                                       全局变量
==================================================================================================*/
/**
 * @brief CCC运行时数据
 */
static Ccc_RuntimeDataType g_CccRuntime;

/**
 * @brief CCC配置数据
 */
static const Ccc_ConfigType* g_CccConfig = NULL;

/**
 * @brief 模块ID (用于Det)
 */
#define CCC_MODULE_ID                           0x80U

/*==================================================================================================
*                                       本地函数声明
==================================================================================================*/
static void Ccc_ClearRuntimeData(void);
static Ccc_ReturnType Ccc_ValidateConfig(const Ccc_ConfigType* config);
static void Ccc_UpdateSessionTimestamp(void);

/*==================================================================================================
*                                       公共API实现
==================================================================================================*/

/**
 * @brief 初始化CCC数字钥匙模块
 */
Ccc_ReturnType Ccc_Init(const Ccc_ConfigType* config)
{
    Ccc_ReturnType result;
    Std_ReturnType csmResult;
    
    /* 检查参数 */
    CCC_CHECK_POINTER(config);
    
    /* 检查是否已初始化 */
    if (g_CccRuntime.initialized) {
#if (CCC_DEV_ERROR_DETECT == STD_ON)
        Det_ReportError(CCC_MODULE_ID, 0, CCC_API_INIT, CCC_E_ALREADY_INITIALIZED);
#endif
        return CCC_E_ALREADY_INITIALIZED;
    }
    
    /* 验证配置 */
    result = Ccc_ValidateConfig(config);
    if (result != CCC_E_OK) {
        return result;
    }
    
    /* 初始化CSM服务 */
#if (CCC_USE_CSM == STD_ON)
    csmResult = Csm_Init(NULL);
    if (csmResult != E_OK) {
        return CCC_E_CRYPTO_FAILURE;
    }
#endif
    
    /* 清空运行时数据 */
    Ccc_ClearRuntimeData();
    
    /* 保存配置 */
    g_CccConfig = config;
    
    /* 设置初始状态 */
    g_CccRuntime.currentMode = CCC_MODE_UNINITIALIZED;
    g_CccRuntime.session.state = CCC_SESSION_STATE_INACTIVE;
    g_CccRuntime.sessionCounter = 0U;
    g_CccRuntime.initialized = TRUE;
    
    /* 复制设备ID到配对数据 */
    g_CccRuntime.pairingData.localDevice = config->deviceId;
    
    return CCC_E_OK;
}

/**
 * @brief 去初始化CCC数字钥匙模块
 */
Ccc_ReturnType Ccc_DeInit(void)
{
    /* 检查是否已初始化 */
    if (!g_CccRuntime.initialized) {
#if (CCC_DEV_ERROR_DETECT == STD_ON)
        Det_ReportError(CCC_MODULE_ID, 0, CCC_API_DEINIT, CCC_E_NOT_INITIALIZED);
#endif
        return CCC_E_NOT_INITIALIZED;
    }
    
    /* 关闭会话 */
    if (g_CccRuntime.session.state != CCC_SESSION_STATE_INACTIVE) {
        (void)Ccc_SessionClose();
    }
    
    /* 去初始化CSM服务 */
#if (CCC_USE_CSM == STD_ON)
    (void)Csm_DeInit();
#endif
    
    /* 清空数据 */
    Ccc_ClearRuntimeData();
    g_CccConfig = NULL;
    
    return CCC_E_OK;
}

/**
 * @brief 获取版本信息
 */
#if (CCC_VERSION_INFO_API == STD_ON)
void Ccc_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    if (versioninfo != NULL) {
        versioninfo->vendorID = CCC_VENDOR_ID;
        versioninfo->moduleID = CCC_MODULE_ID;
        versioninfo->sw_major_version = CCC_SW_MAJOR_VERSION;
        versioninfo->sw_minor_version = CCC_SW_MINOR_VERSION;
        versioninfo->sw_patch_version = CCC_SW_PATCH_VERSION;
    }
}
#endif

/**
 * @brief 获取当前模式
 */
Ccc_ModeType Ccc_GetCurrentMode(void)
{
    if (!g_CccRuntime.initialized) {
        return CCC_MODE_UNINITIALIZED;
    }
    return g_CccRuntime.currentMode;
}

/**
 * @brief 获取会话状态
 */
Ccc_ReturnType Ccc_GetSessionState(Ccc_SessionStateType* sessionState)
{
    CCC_CHECK_POINTER(sessionState);
    CCC_CHECK_INITIALIZED();
    
    *sessionState = g_CccRuntime.session.state;
    return CCC_E_OK;
}

/**
 * @brief 对数据进行签名
 */
Ccc_ReturnType Ccc_SignData(
    const uint8* data,
    uint32 dataLength,
    uint8* signature,
    uint32* signatureLength
)
{
    Std_ReturnType result;
    
    CCC_CHECK_POINTER(data);
    CCC_CHECK_POINTER(signature);
    CCC_CHECK_POINTER(signatureLength);
    CCC_CHECK_INITIALIZED();
    
#if (CCC_USE_CSM == STD_ON)
    /* 设置作业密钥 */
    result = Csm_JobKeySetUp(g_CccConfig->csmJobId, g_CccConfig->csmKeyId);
    if (result != E_OK) {
        return CCC_E_KEY_NOT_FOUND;
    }
    
    /* 生成签名 */
    result = Csm_SignatureGenerate(
        g_CccConfig->csmJobId,
        CSM_OPERATIONMODE_SINGLECALL,
        data,
        dataLength,
        signature,
        signatureLength
    );
    
    if (result != E_OK) {
        return CCC_E_CRYPTO_FAILURE;
    }
#else
    /* 模拟实现 - 实际应用中应使用真实的加密服务 */
    (void)memcpy(signature, data, (dataLength < *signatureLength) ? dataLength : *signatureLength);
    *signatureLength = (dataLength < 64U) ? dataLength : 64U;
#endif
    
    return CCC_E_OK;
}

/**
 * @brief 验证签名
 */
Ccc_ReturnType Ccc_VerifySignature(
    const uint8* data,
    uint32 dataLength,
    const uint8* signature,
    uint32 signatureLength,
    const uint8* publicKey
)
{
    Std_ReturnType result;
    boolean verifyResult = FALSE;
    
    CCC_CHECK_POINTER(data);
    CCC_CHECK_POINTER(signature);
    CCC_CHECK_INITIALIZED();
    
    /* 如果提供了公钥，需要设置到CSM (这里省略详细实现) */
    (void)publicKey;
    
#if (CCC_USE_CSM == STD_ON)
    /* 验证签名 */
    result = Csm_SignatureVerify(
        g_CccConfig->csmJobId,
        CSM_OPERATIONMODE_SINGLECALL,
        data,
        dataLength,
        signature,
        signatureLength,
        &verifyResult
    );
    
    if (result != E_OK) {
        return CCC_E_CRYPTO_FAILURE;
    }
    
    if (!verifyResult) {
        return CCC_E_SIGNATURE_INVALID;
    }
#else
    /* 模拟实现 */
    (void)memcpy(&verifyResult, &signatureLength, sizeof(boolean));
#endif
    
    return CCC_E_OK;
}

/**
 * @brief 生成随机数
 */
Ccc_ReturnType Ccc_GenerateRandom(uint8* randomData, uint32 length)
{
    Std_ReturnType result;
    
    CCC_CHECK_POINTER(randomData);
    CCC_CHECK_INITIALIZED();
    
    if (length == 0U || length > 256U) {
        return CCC_E_PARAM_LENGTH;
    }
    
#if (CCC_USE_CSM == STD_ON)
    result = Csm_RandomGenerate(g_CccConfig->csmJobId, randomData, length);
    if (result != E_OK) {
        return CCC_E_CRYPTO_FAILURE;
    }
#else
    /* 模拟实现 - 实际应用中应使用真实的随机数生成器 */
    for (uint32 i = 0U; i < length; i++) {
        randomData[i] = (uint8)(i * 7 + 0xAB);
    }
#endif
    
    return CCC_E_OK;
}

/**
 * @brief 计算哈希值
 */
Ccc_ReturnType Ccc_CalculateHash(
    const uint8* data,
    uint32 dataLength,
    uint8* hash,
    uint32* hashLength
)
{
    Std_ReturnType result;
    
    CCC_CHECK_POINTER(data);
    CCC_CHECK_POINTER(hash);
    CCC_CHECK_POINTER(hashLength);
    CCC_CHECK_INITIALIZED();
    
#if (CCC_USE_CSM == STD_ON)
    result = Csm_Hash(
        g_CccConfig->csmJobId,
        CSM_OPERATIONMODE_SINGLECALL,
        data,
        dataLength,
        hash,
        hashLength
    );
    
    if (result != E_OK) {
        return CCC_E_CRYPTO_FAILURE;
    }
#else
    /* 模拟实现 */
    if (*hashLength >= 32U) {
        for (uint32 i = 0U; i < 32U; i++) {
            hash[i] = (uint8)(i ^ 0xAA);
        }
        *hashLength = 32U;
    }
#endif
    
    return CCC_E_OK;
}

/*==================================================================================================
*                                       本地函数实现
==================================================================================================*/

/**
 * @brief 清空运行时数据
 */
static void Ccc_ClearRuntimeData(void)
{
    /* 使用volatile避免优化 */
    volatile uint8* ptr = (volatile uint8*)&g_CccRuntime;
    
    for (uint32 i = 0U; i < sizeof(Ccc_RuntimeDataType); i++) {
        ptr[i] = 0U;
    }
}

/**
 * @brief 验证配置
 */
static Ccc_ReturnType Ccc_ValidateConfig(const Ccc_ConfigType* config)
{
    /* 检查设备ID */
    if (config->deviceId.deviceId[0] == 0U) {
        return CCC_E_PARAM_POINTER;
    }
    
    /* 检查角色 */
    if (config->role != CCC_ROLE_VEHICLE && 
        config->role != CCC_ROLE_MOBILE_DEVICE && 
        config->role != CCC_ROLE_SERVER) {
        return CCC_E_PARAM_MODE;
    }
    
    return CCC_E_OK;
}

/**
 * @brief 更新会话时间戳
 */
static void Ccc_UpdateSessionTimestamp(void)
{
    /* 在实际应用中，这里应该使用真实的时间源 */
    static uint32 timestamp = 0U;
    g_CccRuntime.session.timestamp = timestamp++;
}

/*==================================================================================================
*                                       外部变量定义
==================================================================================================*/
/**
 * @brief 获取运行时数据指针 (供其他模块使用)
 */
Ccc_RuntimeDataType* Ccc_GetRuntimeData(void)
{
    return &g_CccRuntime;
}

/**
 * @brief 获取配置数据指针 (供其他模块使用)
 */
const Ccc_ConfigType* Ccc_GetConfig(void)
{
    return g_CccConfig;
}
