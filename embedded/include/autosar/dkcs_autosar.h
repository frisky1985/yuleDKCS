/******************************************************************************
 * @file    dkcs_autosar.h
 * @brief   yuleDKCS AUTOSAR BSW 集成层主头文件
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-09
 *
 * @note    符合 AUTOSAR 4.4.0 规范
 *          提供 DKCS 与 AUTOSAR BSW 模块的集成接口
 ******************************************************************************/

#ifndef DKCS_AUTOSAR_H
#define DKCS_AUTOSAR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../dkcs.h"

/******************************************************************************
 * AUTOSAR 标准类型定义 (简化版)
 ******************************************************************************/
#ifndef _AUTOSAR_TYPES_DEFINED_
#define _AUTOSAR_TYPES_DEFINED_

typedef uint8_t Std_ReturnType;
#define E_OK        ((Std_ReturnType)0x00)
#define E_NOT_OK    ((Std_ReturnType)0x01)

typedef uint8_t Std_VersionInfoType[3];
#define STD_ACTIVE      0x01
#define STD_IDLE        0x00
#define STD_ON          0x01
#define STD_OFF         0x00
#define STD_HIGH        0x01
#define STD_LOW         0x00

typedef uint8_t Dio_LevelType;
typedef uint32_t Dio_PortLevelType;

#endif /* _AUTOSAR_TYPES_DEFINED_ */

/******************************************************************************
 * AUTOSAR 模块 ID 定义
 ******************************************************************************/
#define DKCS_MODULE_ID_CSM    0xC5  /**< Crypto Services Manager */
#define DKCS_MODULE_ID_DCM    0x19  /**< Diagnostic Communication Manager */
#define DKCS_MODULE_ID_E2E    0xC2  /**< End-to-End Protection */
#define DKCS_MODULE_ID_OS     0x01  /**< Operating System */

/******************************************************************************
 * 服务 ID 定义 (用于错误报告)
 ******************************************************************************/
#define DKCS_SID_INIT                   0x01
#define DKCS_SID_DEINIT                 0x02
#define DKCS_SID_KEY_GENERATE           0x03
#define DKCS_SID_KEY_IMPORT             0x04
#define DKCS_SID_KEY_EXPORT             0x05
#define DKCS_SID_ENCRYPT                0x06
#define DKCS_SID_DECRYPT                0x07
#define DKCS_SID_SIGN                   0x08
#define DKCS_SID_VERIFY                 0x09
#define DKCS_SID_RANDOM                 0x0A
#define DKCS_SID_KEY_SHARE              0x0B
#define DKCS_SID_KEY_REVOKE             0x0C
#define DKCS_SID_E2E_PROTECT            0x0D
#define DKCS_SID_E2E_CHECK              0x0E
#define DKCS_SID_TIMING_CHECK           0x0F
#define DKCS_SID_DCM_READ_DATA          0x10
#define DKCS_SID_DCM_WRITE_DATA         0x11
#define DKCS_SID_DCM_ROUTINE_CTRL       0x12

/******************************************************************************
 * 错误码定义 (AUTOSAR 标准格式)
 ******************************************************************************/
#define DKCS_E_NO_ERROR                 0x00
#define DKCS_E_PARAM_POINTER            0x01
#define DKCS_E_PARAM_HANDLE             0x02
#define DKCS_E_PARAM_LENGTH             0x03
#define DKCS_E_NOT_INITIALIZED          0x04
#define DKCS_E_ALREADY_INITIALIZED      0x05
#define DKCS_E_CRYPTO_FAILURE           0x06
#define DKCS_E_KEY_NOT_FOUND            0x07
#define DKCS_E_KEY_LOCKED               0x08
#define DKCS_E_BUFFER_OVERFLOW          0x09
#define DKCS_E_E2E_ERROR                0x0A
#define DKCS_E_TIMEOUT                  0x0B
#define DKCS_E_HARDWARE_FAILURE         0x0C

/******************************************************************************
 * CSM (Crypto Services Manager) 集成配置
 ******************************************************************************/
typedef enum {
    DKCS_CSM_JOB_KEY_EXCHANGE = 0,
    DKCS_CSM_JOB_SIGNATURE,
    DKCS_CSM_JOB_ENCRYPTION,
    DKCS_CSM_JOB_DECRYPTION,
    DKCS_CSM_JOB_RANDOM,
    DKCS_CSM_JOB_HASH,
    DKCS_CSM_JOB_MAC,
    DKCS_CSM_JOB_MAX
} dkcs_csm_job_type_t;

typedef uint32_t Dkcs_CsmJobIdType;

typedef struct {
    Dkcs_CsmJobIdType jobId;
    dkcs_csm_job_type_t jobType;
    uint32_t keyId;
    uint32_t algorithm;
    uint32_t mode;
} dkcs_csm_job_cfg_t;

/******************************************************************************
 * E2E (End-to-End Protection) 配置
 ******************************************************************************/
typedef enum {
    DKCS_E2E_PROFILE_1 = 1,   /**< CRC8 + SeqCounter */
    DKCS_E2E_PROFILE_2 = 2,   /**< CRC8 + SeqCounter + DataID */
    DKCS_E2E_PROFILE_4 = 4,   /**< CRC32 + SeqCounter */
    DKCS_E2E_PROFILE_7 = 7,   /**< CRC32 + SeqCounter + DataID */
    DKCS_E2E_PROFILE_11 = 11  /**< Custom for UWB ranging */
} dkcs_e2e_profile_t;

typedef struct {
    dkcs_e2e_profile_t profile;
    uint16_t dataId;
    uint16_t dataLength;
    uint8_t maxDeltaCounter;
    uint8_t maxErrorCounter;
    uint32_t syncCounterInit;
} dkcs_e2e_config_t;

typedef struct {
    uint16_t sequenceCounter;
    uint8_t crc;
    uint32_t crc32;
    uint8_t dataIdNibble;
    uint8_t status;
} dkcs_e2e_header_t;

typedef struct {
    uint32_t aliveCounter;
    uint32_t lostMessages;
    uint32_t duplicateMessages;
    uint32_t sequenceErrors;
} dkcs_e2e_status_t;

/******************************************************************************
 * DCM (Diagnostic Communication Manager) 配置
 ******************************************************************************/
typedef enum {
    DKCS_DID_KEY_STATUS = 0xF100,
    DKCS_DID_KEY_INFO = 0xF101,
    DKCS_DID_SE_STATUS = 0xF102,
    DKCS_DID_SESSION_INFO = 0xF103,
    DKCS_DID_RANGING_DATA = 0xF104,
    DKCS_DID_SECURITY_EVENT = 0xF105,
    DKCS_DID_PROTOCOL_VERSION = 0xF106,
    DKCS_DID_FIRMWARE_VERSION = 0xF107
} dkcs_did_type_t;

typedef enum {
    DKCS_ROUTINE_ERASE_ALL_KEYS = 0xFF00,
    DKCS_ROUTINE_VERIFY_KEY_INTEGRITY = 0xFF01,
    DKCS_ROUTINE_RESET_SECURITY_MODULE = 0xFF02,
    DKCS_ROUTINE_SELF_TEST = 0xFF03,
    DKCS_ROUTINE_UPDATE_FIRMWARE = 0xFF04
} dkcs_routine_type_t;

typedef struct {
    uint16_t did;
    uint8_t securityLevel;
    uint8_t sessionType;
    Std_ReturnType (*readFunc)(uint8_t *data, uint16_t *length);
    Std_ReturnType (*writeFunc)(const uint8_t *data, uint16_t length);
} dkcs_dcm_did_cfg_t;

/******************************************************************************
 * OS 定时保护配置
 ******************************************************************************/
typedef struct {
    uint32_t executionBudget;     /**< 最大执行时间 (us) */
    uint32_t timeFrame;           /**< 时间窗口 (us) */
    uint32_t interArrivalTime;    /**< 最小到达间隔 (us) */
    uint32_t watchdogThreshold;   /**< 看门狗阈值 (us) */
} dkcs_os_timing_budget_t;

typedef enum {
    DKCS_TASK_KEY_EXCHANGE = 0,
    DKCS_TASK_CRYPTO_OPS,
    DKCS_TASK_RANGING,
    DKCS_TASK_DIAGNOSTIC,
    DKCS_TASK_EVENT_HANDLER,
    DKCS_TASK_MAX
} dkcs_task_id_t;

typedef struct {
    dkcs_task_id_t taskId;
    dkcs_os_timing_budget_t budget;
    void (*hookFunc)(dkcs_task_id_t taskId, uint32_t violationType);
} dkcs_os_timing_cfg_t;

/******************************************************************************
 * CSM 接口函数声明
 ******************************************************************************/

/**
 * @brief 初始化 CSM 适配器
 * @return E_OK 成功，E_NOT_OK 失败
 */
Std_ReturnType Dkcs_Csm_Init(void);

/**
 * @brief 反初始化 CSM 适配器
 * @return E_OK 成功，E_NOT_OK 失败
 */
Std_ReturnType Dkcs_Csm_DeInit(void);

/**
 * @brief 执行密钥交换作业
 * @param[in] jobId 作业 ID
 * @param[in] peerPubKey 对端公钥
 * @param[in] peerPubKeyLen 公钥长度
 * @param[out] sharedSecret 共享密钥
 * @param[out] secretLen 共享密钥长度
 * @return E_OK 成功，E_NOT_OK 失败
 */
Std_ReturnType Dkcs_Csm_KeyExchange(
    Dkcs_CsmJobIdType jobId,
    const uint8_t *peerPubKey,
    uint32_t peerPubKeyLen,
    uint8_t *sharedSecret,
    uint32_t *secretLen
);

/**
 * @brief 执行签名作业
 * @param[in] jobId 作业 ID
 * @param[in] data 待签名数据
 * @param[in] dataLen 数据长度
 * @param[out] signature 签名输出
 * @param[out] sigLen 签名长度
 * @return E_OK 成功，E_NOT_OK 失败
 */
Std_ReturnType Dkcs_Csm_SignatureGenerate(
    Dkcs_CsmJobIdType jobId,
    const uint8_t *data,
    uint32_t dataLen,
    uint8_t *signature,
    uint32_t *sigLen
);

/**
 * @brief 执行验签作业
 * @param[in] jobId 作业 ID
 * @param[in] data 原始数据
 * @param[in] dataLen 数据长度
 * @param[in] signature 签名
 * @param[in] sigLen 签名长度
 * @param[out] verifyResult 验签结果
 * @return E_OK 成功，E_NOT_OK 失败
 */
Std_ReturnType Dkcs_Csm_SignatureVerify(
    Dkcs_CsmJobIdType jobId,
    const uint8_t *data,
    uint32_t dataLen,
    const uint8_t *signature,
    uint32_t sigLen,
    uint8_t *verifyResult
);

/**
 * @brief 执行加密作业
 * @param[in] jobId 作业 ID
 * @param[in] plaintext 明文
 * @param[in] plainLen 明文长度
 * @param[out] ciphertext 密文
 * @param[out] cipherLen 密文长度
 * @return E_OK 成功，E_NOT_OK 失败
 */
Std_ReturnType Dkcs_Csm_Encrypt(
    Dkcs_CsmJobIdType jobId,
    const uint8_t *plaintext,
    uint32_t plainLen,
    uint8_t *ciphertext,
    uint32_t *cipherLen
);

/**
 * @brief 执行解密作业
 * @param[in] jobId 作业 ID
 * @param[in] ciphertext 密文
 * @param[in] cipherLen 密文长度
 * @param[out] plaintext 明文
 * @param[out] plainLen 明文长度
 * @return E_OK 成功，E_NOT_OK 失败
 */
Std_ReturnType Dkcs_Csm_Decrypt(
    Dkcs_CsmJobIdType jobId,
    const uint8_t *ciphertext,
    uint32_t cipherLen,
    uint8_t *plaintext,
    uint32_t *plainLen
);

/**
 * @brief 生成随机数
 * @param[out] random 随机数缓冲区
 * @param[in] len 请求长度
 * @return E_OK 成功，E_NOT_OK 失败
 */
Std_ReturnType Dkcs_Csm_RandomGenerate(
    uint8_t *random,
    uint32_t len
);

/******************************************************************************
 * E2E 接口函数声明
 ******************************************************************************/

/**
 * @brief 初始化 E2E 保护模块
 * @param[in] config E2E 配置
 * @return E_OK 成功，E_NOT_OK 失败
 */
Std_ReturnType Dkcs_E2E_Init(const dkcs_e2e_config_t *config);

/**
 * @brief 保护数据 (添加 E2E 头)
 * @param[in] dataId 数据 ID
 * @param[in,out] data 数据缓冲区 (输入/输出)
 * @param[in,out] length 数据长度 (包含 E2E 头)
 * @return E_OK 成功，E_NOT_OK 失败
 */
Std_ReturnType Dkcs_E2E_Protect(
    uint16_t dataId,
    uint8_t *data,
    uint16_t *length
);

/**
 * @brief 检查受保护数据
 * @param[in] dataId 数据 ID
 * @param[in] data 数据缓冲区
 * @param[in] length 数据长度
 * @param[out] status E2E 状态
 * @return E_OK 成功，E_NOT_OK 失败
 */
Std_ReturnType Dkcs_E2E_Check(
    uint16_t dataId,
    const uint8_t *data,
    uint16_t length,
    dkcs_e2e_status_t *status
);

/**
 * @brief 初始化 E2E 序列计数器
 * @param[in] dataId 数据 ID
 * @param[in] initialValue 初始值
 * @return E_OK 成功，E_NOT_OK 失败
 */
Std_ReturnType Dkcs_E2E_InitCounter(
    uint16_t dataId,
    uint16_t initialValue
);

/**
 * @brief 获取 E2E 状态
 * @param[in] dataId 数据 ID
 * @param[out] status 状态结构
 * @return E_OK 成功，E_NOT_OK 失败
 */
Std_ReturnType Dkcs_E2E_GetStatus(
    uint16_t dataId,
    dkcs_e2e_status_t *status
);

/******************************************************************************
 * DCM 接口函数声明
 ******************************************************************************/

/**
 * @brief 初始化 DCM 接口
 * @return E_OK 成功，E_NOT_OK 失败
 */
Std_ReturnType Dkcs_Dcm_Init(void);

/**
 * @brief 读取 DID 数据
 * @param[in] did 数据标识符
 * @param[out] data 数据缓冲区
 * @param[out] length 数据长度
 * @return E_OK 成功，E_NOT_OK 失败
 */
Std_ReturnType Dkcs_Dcm_ReadDataByIdentifier(
    uint16_t did,
    uint8_t *data,
    uint16_t *length
);

/**
 * @brief 写入 DID 数据
 * @param[in] did 数据标识符
 * @param[in] data 数据缓冲区
 * @param[in] length 数据长度
 * @return E_OK 成功，E_NOT_OK 失败
 */
Std_ReturnType Dkcs_Dcm_WriteDataByIdentifier(
    uint16_t did,
    const uint8_t *data,
    uint16_t length
);

/**
 * @brief 执行诊断例程
 * @param[in] routineId 例程 ID
 * @param[in] controlType 控制类型 (启动/停止/请求结果)
 * @param[in] requestData 请求数据
 * @param[in] requestLen 请求数据长度
 * @param[out] responseData 响应数据
 * @param[out] responseLen 响应数据长度
 * @return E_OK 成功，E_NOT_OK 失败
 */
Std_ReturnType Dkcs_Dcm_RoutineControl(
    uint16_t routineId,
    uint8_t controlType,
    const uint8_t *requestData,
    uint16_t requestLen,
    uint8_t *responseData,
    uint16_t *responseLen
);

/**
 * @brief 报告安全事件到 DCM
 * @param[in] eventId 事件 ID
 * @param[in] eventData 事件数据
 * @param[in] dataLen 数据长度
 * @return E_OK 成功，E_NOT_OK 失败
 */
Std_ReturnType Dkcs_Dcm_ReportSecurityEvent(
    uint32_t eventId,
    const uint8_t *eventData,
    uint8_t dataLen
);

/******************************************************************************
 * OS 定时保护接口函数声明
 ******************************************************************************/

/**
 * @brief 初始化 OS 定时保护
 * @param[in] timingConfigs 定时配置数组
 * @param[in] configCount 配置数量
 * @return E_OK 成功，E_NOT_OK 失败
 */
Std_ReturnType Dkcs_Os_TimingProtectionInit(
    const dkcs_os_timing_cfg_t *timingConfigs,
    uint8_t configCount
);

/**
 * @brief 启动任务执行时间监控
 * @param[in] taskId 任务 ID
 * @return E_OK 成功，E_NOT_OK 失败
 */
Std_ReturnType Dkcs_Os_StartTimingProtection(dkcs_task_id_t taskId);

/**
 * @brief 停止任务执行时间监控
 * @param[in] taskId 任务 ID
 * @return E_OK 成功，E_NOT_OK 失败
 */
Std_ReturnType Dkcs_Os_StopTimingProtection(dkcs_task_id_t taskId);

/**
 * @brief 检查任务执行时间
 * @param[in] taskId 任务 ID
 * @param[in] executionTime 执行时间 (us)
 * @return E_OK 在预算内，E_NOT_OK 超时
 */
Std_ReturnType Dkcs_Os_CheckExecutionTime(
    dkcs_task_id_t taskId,
    uint32_t executionTime
);

/**
 * @brief 检查任务到达率
 * @param[in] taskId 任务 ID
 * @param[in] arrivalInterval 到达间隔 (us)
 * @return E_OK 符合要求，E_NOT_OK 过于频繁
 */
Std_ReturnType Dkcs_Os_CheckInterArrivalTime(
    dkcs_task_id_t taskId,
    uint32_t arrivalInterval
);

/**
 * @brief 喂看门狗
 * @param[in] taskId 任务 ID
 * @return E_OK 成功，E_NOT_OK 失败
 */
Std_ReturnType Dkcs_Os_FeedWatchdog(dkcs_task_id_t taskId);

/**
 * @brief 注册定时违规回调
 * @param[in] hookFunc 回调函数
 * @return E_OK 成功，E_NOT_OK 失败
 */
Std_ReturnType Dkcs_Os_RegisterTimingHook(
    void (*hookFunc)(dkcs_task_id_t taskId, uint32_t violationType)
);

/******************************************************************************
 * 错误报告接口 (用于 DET - Default Error Tracer)
 ******************************************************************************/
/**
 * @brief 报告开发错误
 * @param[in] moduleId 模块 ID
 * @param[in] instanceId 实例 ID
 * @param[in] apiId API ID
 * @param[in] errorId 错误 ID
 */
void Dkcs_ReportError(
    uint16_t moduleId,
    uint8_t instanceId,
    uint8_t apiId,
    uint8_t errorId
);

/******************************************************************************
 * 版本信息
 ******************************************************************************/
/**
 * @brief 获取 AUTOSAR 集成层版本信息
 * @param[out] version 版本信息结构
 */
void Dkcs_Autosar_GetVersionInfo(Std_VersionInfoType *version);

#ifdef __cplusplus
}
#endif

#endif /* DKCS_AUTOSAR_H */
