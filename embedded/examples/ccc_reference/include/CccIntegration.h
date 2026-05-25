/**
 * @file CccIntegration.h
 * @brief yuleDKCS + yuleASR 集成接口说明
 *
 * 本文件说明 CCC Reference 示例与 yuleDKCS ccc_core 协议栈
 * 以及 yuleASR AutoSAR BSW 模块的集成方式。
 *
 * @author yuleDKCS Team
 * @version 1.0.0
 */

#ifndef CCC_INTEGRATION_H
#define CCC_INTEGRATION_H

#ifdef __cplusplus
extern "C" {
#endif

/*==================================================================================================
 * yuleDKCS ccc_core 协议栈集成
 *
 * yuleDKCS 在 embedded/src/ccc/ 中提供了完整的 CCC Digital Key R3 协议栈实现。
 * 本示例展示如何从零使用这些协议栈函数构建数字钥匙应用。
 *
 * 关键头文件:
 *   - #include "ccc.h"                         /* CCC 协议栈主头文件 */
 *   - #include "ccc/ccc_digital_key.h"          /* CCC 数字钥匙硬件抽象层 */
 *
 * 核心 API 与示例对照:
 *
 *   示例函数                   | ccc_core 等价函数              | 说明
 *   ---------------------------|--------------------------------|---------------------------
 *   Ccc_Init()                | ccc_init()                     | 初始化协议栈
 *   Ccc_PairingStart()        | ccc_create_pairing_session()   | 创建配对会话
 *   Ccc_PairingComplete()     | ccc_start_pairing()            | 执行配对
 *   Ccc_SessionEstablish()    | ccc_establish_session()        | 建立安全会话
 *   Ccc_AuthenticationStart() | ccc_generate_challenge()       | 生成挑战
 *   Ccc_AuthenticationComplete() | (挑战-响应流程的组合)       | 身份认证
 *   Ccc_EncryptMessage()      | ccc_encrypt_message()          | 加密消息
 *   Ccc_DecryptMessage()      | ccc_decrypt_message()          | 解密消息
 *   Ccc_VerifyCertificate()   | ccc_verify_certificate()       | 验证证书
 *
 * 硬件模块映射 (ccc_digital_key.h):
 *   - NFC 传输: nfc_st25r501_* (ST ST25R501 读卡器)
 *   - BLE 传输: ble_kw47a_*    (NXP KW47A 蓝牙)
 *   - UWB 测距: uwb_ncj29d6_*  (NXP NCJ29D6)
 *   - 安全元件: sec_*          (NXP SE050)
 *
 * 使用 yuleDKCS 构建应用的推荐流程:
 *
 *   Step 1: dkcs_init(&config)              -- 初始化 DKCS 统一接口
 *   Step 2: ccc_init(&se_interface)          -- 初始化 CCC 协议栈
 *   Step 3: ccc_create_pairing_session()     -- 创建配对会话
 *   Step 4: ccc_start_pairing()              -- 开始配对
 *   Step 5: ccc_establish_session()          -- 建立安全会话
 *   Step 6: ccc_encrypt_message() / ccc_decrypt_message() -- 安全通信
 *   Step 7: dkcs_deinit() / ccc_deinit()     -- 清理
 *=================================================================================================*/

/*==================================================================================================
 * yuleASR BSW 模块集成
 *
 * 此示例原用于 yuleASR AutoSAR BSW 平台。迁移到 yuleDKCS 后，
 * 以下 BSW 服务仍可通过包含 yuleASR 头文件调用:
 *
 * 1. OS (操作系统) - SchM_* / Os_*
 *    使用场景: 任务调度、中断管理、时间片轮转
 *    集成示例:
 *       #include "Os.h"
 *       // 创建CCC处理任务
 *       Os_CreateTask(TASK_CCC_PROCESS, CccProcessTask, 10);
 *       // 使用定时器触发会话超时检查
 *       Os_SetTimer(TIMER_CCC_SESSION_TIMEOUT, CCC_SESSION_TIMEOUT_MS,
 *                   CccSessionTimeoutCallback);
 *
 * 2. CSM (加密服务管理) - Csm_*
 *    使用场景: 密钥生成、签名、加解密、哈希、MAC
 *    集成示例:
 *       #include "Csm.h"
 *       // 使用CSM进行ECDH密钥协商
 *       Csm_KeyExchangeCalcSecret(jobId, remotePubKey, keyLen);
 *       // 使用CSM进行AES-GCM加解密
 *       Csm_Encrypt(jobId, CSM_OPERATIONMODE_SINGLECALL, plain, len, cipher, &outLen);
 *
 * 3. NvM (非易失性存储器) - NvM_*
 *    使用场景: 密钥持久化存储、证书存储、配置保存
 *    集成示例:
 *       #include "NvM.h"
 *       // 写入数字钥匙数据到NVM
 *       NvM_WriteBlock(NVM_BLOCK_CCC_KEY, keyData, sizeof(ccc_digital_key_t));
 *       // 读取已存储的钥匙
 *       NvM_ReadBlock(NVM_BLOCK_CCC_KEY, keyData, sizeof(ccc_digital_key_t));
 *
 * 4. DCM (诊断通信管理) - Dcm_*
 *    使用场景: 诊断服务、钥匙状态查询、OTA更新
 *    集成示例:
 *       #include "Dcm.h"
 *       // 处理诊断请求 (读取钥匙信息)
 *       Dcm_ProcessRequest(DCM_SID_READ_DATA_BY_ID, DID_CCC_KEY_INFO, ...);
 *
 * 5. Det (开发错误追踪) - Det_*
 *    使用场景: 运行时错误检测与报告
 *    已在 CccDigitalKey.c 中使用。
 *
 * 6. EcuM (ECU状态管理) - EcuM_*
 *    使用场景: 唤醒管理、运行状态切换
 *    集成示例:
 *       #include "EcuM.h"
 *       EcuM_SelectWakeupSource(ECUM_WAKEUP_SOURCE_NFC);
 *       EcuM_RequestRunLevel(ECUM_RUN_LEVEL_CCC);
 *=================================================================================================*/

/*==================================================================================================
 * 推荐编译定义
 *
 * 使用 yuleDKCS + yuleASR 集成编译时建议启用以下定义:
 *
 *   -DCCC_USE_CSM=STD_ON          /* 启用 CSM 加密服务 (默认) */
 *   -DCCC_DEV_ERROR_DETECT=STD_ON /* 启用开发错误检测 (默认) */
 *   -DCCC_VERSION_INFO_API=STD_ON  /* 启用版本信息 API (默认) */
 *   -DENABLE_CCC                   /* 启用 yuleDKCS CCC 协议栈 */
 *
 * 头文件路径:
 *   - yuleDKCS/embedded/include/           (DKCS 公共头文件)
 *   - yuleASR/src/bsw/csm/                 (CSM 头文件)
 *   - yuleASR/src/bsw/det/                 (Det 头文件)
 *   - yuleASR/src/bsw/os/                  (OS 头文件)
 *   - yuleASR/src/bsw/nvm/                 (NvM 头文件)
 *   - yuleASR/src/bsw/dcm/                 (DCM 头文件)
 *   - yuleASR/src/bsw/ecum/                (EcuM 头文件)
 *=================================================================================================*/

#ifdef __cplusplus
}
#endif

#endif /* CCC_INTEGRATION_H */
