/**
 * @file main.c
 * @brief CCC数字钥匙应用示例程序
 * 
 * 功能: 演示CCC数字钥匙的完整使用流程
 * 
 * @note 此示例已整合到 yuleDKCS/embedded/examples/ccc_reference/
 *       展示 yuleDKCS ccc_core + yuleASR BSW 集成使用方法
 *
 * @author yuleASR Team / yuleDKCS Team
 * @version 2.0.0
 */

#include "CccDigitalKey.h"
#include "CccIntegration.h"  /* yuleDKCS + yuleASR 集成说明 */
#include <stdio.h>
#include <string.h>

/*
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │ yuleDKCS ccc_core + yuleDKCS 统一接口集成                                │
 * │                                                                         │
 * │ 此 main.c 展示面向读者的 API 调用流程。实际产品中可选用以下任一方式:       │
 * │                                                                         │
 * │ 方式 A: 使用 yuleDKCS 统一接口 (推荐)                                    │
 * │   #include "dkcs.h"                                                      │
 * │   dkcs_init(&config);                                                    │
 * │   dkcs_pairing_start(PROTOCOL_CCC, vin, callback, ctx);                  │
 * │   dkcs_vehicle_unlock(key_id, vin);                                      │
 * │   dkcs_deinit();                                                         │
 * │                                                                         │
 * │ 方式 B: 直接使用 CCC 协议栈接口                                          │
 * │   #include "ccc.h"                                                       │
 * │   ccc_init(&se_interface);                                               │
 * │   ccc_create_pairing_session(&config, &session);                         │
 * │   ccc_establish_session(session, pub_key, challenge);                    │
 * │   ccc_send_vehicle_command(session, CCC_CMD_VEHICLE_UNLOCK, ...);        │
 * │   ccc_deinit();                                                          │
 * │                                                                         │
 * │ 方式 C: 使用本示例的自定义实现 (教学/学习目的)                            │
 * │   本 main.c 即使用此方式,调用 Ccc_* 系列函数逐步演示流程                  │
 * │                                                                         │
 * │ yuleASR BSW 集成 (所有方式均可):                                        │
 * │   OS:   #include \"Os.h\" -> 任务/超时管理                               │
 * │   CSM:  #include \"Csm.h\" -> 加密服务 (已嵌入 CccDigitalKey.c 等)      │
 * │   NvM:  #include \"NvM.h\" -> 钥匙持久化                                 │
 * │   DCM:  #include \"Dcm.h\" -> 诊断安全访问                                │
 * │   EcuM: #include \"EcuM.h\" -> 唤醒源管理                               │
 * └─────────────────────────────────────────────────────────────────────────┘
 */

/*==================================================================================================
*                                       配置定义
==================================================================================================*/
/**
 * @brief CCC配置
 */
static const Ccc_ConfigType cccConfig = {
    .deviceId = {
        .deviceId = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                     0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10},
        .role = CCC_ROLE_VEHICLE,
        .protocolVersion = 0x0300
    },
    .role = CCC_ROLE_VEHICLE,
    .keyStorageId = 0x01,
    .certStorageId = 0x02,
    .csmKeyId = 0x01,
    .csmJobId = 0x01,
    .useSecureStorage = TRUE
};

/*==================================================================================================
*                                       本地函数声明
==================================================================================================*/
static void PrintReturnCode(Ccc_ReturnType result);
static void DemoPairing(void);
static void DemoAuthentication(void);
static void DemoSecureChannel(void);

/*==================================================================================================
*                                       主函数
==================================================================================================*/
/**
 * @brief 主函数
 */
int main(void)
{
    Ccc_ReturnType result;
    
    printf("========================================\n");
    printf("  CCC数字钥匙应用示例\n");
    printf("  YuleTech AutoSAR BSW Platform\n");
    printf("========================================\n\n");
    
    /* 初始化CCC模块 */
    printf("[1] 初始化CCC模块...\n");
    result = Ccc_Init(&cccConfig);
    printf("    结果: ");
    PrintReturnCode(result);
    
    if (result != CCC_E_OK) {
        printf("    初始化失败，退出程序\n");
        return -1;
    }
    
    printf("    当前模式: %d\n", Ccc_GetCurrentMode());
    
    /* 演示配对流程 */
    printf("\n[2] 配对流程演示...\n");
    DemoPairing();
    
    /* 演示认证流程 */
    printf("\n[3] 认证流程演示...\n");
    DemoAuthentication();
    
    /* 演示安全通信 */
    printf("\n[4] 安全通信演示...\n");
    DemoSecureChannel();
    
    /* 去初始化 */
    printf("\n[5] 去初始化CCC模块...\n");
    result = Ccc_DeInit();
    printf("    结果: ");
    PrintReturnCode(result);
    
    printf("\n========================================\n");
    printf("  演示完成\n");
    printf("========================================\n");
    
    return 0;
}

/*==================================================================================================
*                                       本地函数实现
==================================================================================================*/

/**
 * @brief 打印返回码
 */
static void PrintReturnCode(Ccc_ReturnType result)
{
    switch (result) {
        case CCC_E_OK:
            printf("成功 (CCC_E_OK)\n");
            break;
        case CCC_E_NOT_INITIALIZED:
            printf("未初始化 (CCC_E_NOT_INITIALIZED)\n");
            break;
        case CCC_E_ALREADY_INITIALIZED:
            printf("已初始化 (CCC_E_ALREADY_INITIALIZED)\n");
            break;
        case CCC_E_PARAM_POINTER:
            printf("参数指针错误 (CCC_E_PARAM_POINTER)\n");
            break;
        case CCC_E_PARAM_LENGTH:
            printf("参数长度错误 (CCC_E_PARAM_LENGTH)\n");
            break;
        case CCC_E_CRYPTO_FAILURE:
            printf("加密操作失败 (CCC_E_CRYPTO_FAILURE)\n");
            break;
        case CCC_E_KEY_NOT_FOUND:
            printf("密钥未找到 (CCC_E_KEY_NOT_FOUND)\n");
            break;
        case CCC_E_KEY_INVALID:
            printf("密钥无效 (CCC_E_KEY_INVALID)\n");
            break;
        case CCC_E_CERT_INVALID:
            printf("证书无效 (CCC_E_CERT_INVALID)\n");
            break;
        case CCC_E_SIGNATURE_INVALID:
            printf("签名无效 (CCC_E_SIGNATURE_INVALID)\n");
            break;
        case CCC_E_AUTHENTICATION_FAILED:
            printf("认证失败 (CCC_E_AUTHENTICATION_FAILED)\n");
            break;
        case CCC_E_SESSION_NOT_ESTABLISHED:
            printf("会话未建立 (CCC_E_SESSION_NOT_ESTABLISHED)\n");
            break;
        case CCC_E_MESSAGE_INVALID:
            printf("消息无效 (CCC_E_MESSAGE_INVALID)\n");
            break;
        case CCC_E_REPLAY_DETECTED:
            printf("检测到重放攻击 (CCC_E_REPLAY_DETECTED)\n");
            break;
        default:
            printf("未知错误 (0x%02X)\n", result);
            break;
    }
}

/**
 * @brief 配对流程演示
 */
static void DemoPairing(void)
{
    Ccc_ReturnType result;
    uint8 localPublicKey[CCC_ECC_P256_PUBLIC_KEY_SIZE];
    uint32 publicKeyLength = sizeof(localPublicKey);
    Ccc_DeviceIdType mobileDevice;
    Ccc_CertificateType remoteCert;
    
    /* 准备移动设备信息 */
    mobileDevice.role = CCC_ROLE_MOBILE_DEVICE;
    mobileDevice.protocolVersion = 0x0300;
    for (int i = 0; i < CCC_DEVICE_ID_SIZE; i++) {
        mobileDevice.deviceId[i] = (uint8)(0xA0 + i);
    }
    
    /* 开始配对 */
    printf("    开始配对...\n");
    result = Ccc_PairingStart(&mobileDevice, localPublicKey, &publicKeyLength);
    printf("    结果: ");
    PrintReturnCode(result);
    
    if (result == CCC_E_OK) {
        printf("    生成的公钥长度: %u 字节\n", publicKeyLength);
        printf("    当前模式: 配对模式\n");
        
        /* 模拟完成配对 - 实际应用中需要从远程设备接收证书 */
        printf("    模拟接收远程证书...\n");
        
        /* 准备模拟证书 */
        remoteCert.valid = TRUE;
        remoteCert.certLength = 256;
        remoteCert.validFrom = 0;
        remoteCert.validUntil = 0xFFFFFFFF;
        for (int i = 0; i < CCC_DEVICE_ID_SIZE; i++) {
            remoteCert.subjectId[i] = mobileDevice.deviceId[i];
            remoteCert.issuerId[i] = (uint8)(0xC0 + i);
        }
        remoteCert.certificate[0] = 0x30;  /* X.509证书标识 */
        for (int i = 1; i < 256; i++) {
            remoteCert.certificate[i] = (uint8)(i & 0xFF);
        }
        
        /* 模拟远程公钥 */
        uint8 remotePublicKey[CCC_ECC_P256_PUBLIC_KEY_SIZE];
        remotePublicKey[0] = 0x04;  /* 未压缩格式 */
        for (int i = 1; i < CCC_ECC_P256_PUBLIC_KEY_SIZE; i++) {
            remotePublicKey[i] = (uint8)(0xBB + i);
        }
        
        printf("    完成配对...\n");
        result = Ccc_PairingComplete(remotePublicKey, sizeof(remotePublicKey), &remoteCert);
        printf("    结果: ");
        PrintReturnCode(result);
        
        if (result == CCC_E_OK) {
            printf("    配对成功，当前模式: 认证模式\n");
        }
    }
}

/**
 * @brief 认证流程演示
 */
static void DemoAuthentication(void)
{
    Ccc_ReturnType result;
    uint8 challenge[CCC_CHALLENGE_SIZE];
    uint32 challengeLength = sizeof(challenge);
    uint8 localSignature[128];
    uint32 localSigLength = sizeof(localSignature);
    
    /* 模拟远程挑战和签名 */
    uint8 remoteChallenge[CCC_CHALLENGE_SIZE];
    uint8 remoteSignature[64];
    
    /* 开始认证 */
    printf("    开始认证...\n");
    result = Ccc_AuthenticationStart(challenge, &challengeLength);
    printf("    结果: ");
    PrintReturnCode(result);
    
    if (result == CCC_E_OK) {
        printf("    生成的挑战值长度: %u 字节\n", challengeLength);
        
        /* 模拟接收远程挑战和签名 */
        printf("    模拟接收远程挑战和签名...\n");
        for (int i = 0; i < CCC_CHALLENGE_SIZE; i++) {
            remoteChallenge[i] = (uint8)(0xCC + i);
            remoteSignature[i] = (uint8)(0xDD + i);
        }
        for (int i = CCC_CHALLENGE_SIZE; i < 64; i++) {
            remoteSignature[i] = (uint8)(0xEE + i);
        }
        
        /* 完成认证 */
        printf("    完成认证...\n");
        result = Ccc_AuthenticationComplete(
            remoteChallenge,
            remoteSignature,
            64,
            localSignature,
            &localSigLength
        );
        printf("    结果: ");
        PrintReturnCode(result);
        
        if (result == CCC_E_OK) {
            printf("    认证成功，当前模式: 操作模式\n");
        }
    }
}

/**
 * @brief 安全通信演示
 */
static void DemoSecureChannel(void)
{
    Ccc_ReturnType result;
    uint8 remotePublicKey[CCC_ECC_P256_PUBLIC_KEY_SIZE];
    
    /* 准备远程公钥 */
    remotePublicKey[0] = 0x04;
    for (int i = 1; i < CCC_ECC_P256_PUBLIC_KEY_SIZE; i++) {
        remotePublicKey[i] = (uint8)(0xFF + i);
    }
    
    /* 建立会话 */
    printf("    建立安全会话...\n");
    result = Ccc_SessionEstablish(TRUE, remotePublicKey, sizeof(remotePublicKey));
    printf("    结果: ");
    PrintReturnCode(result);
    
    if (result == CCC_E_OK) {
        Ccc_SessionStateType sessionState;
        Ccc_GetSessionState(&sessionState);
        printf("    会话状态: %d (激活)\n", sessionState);
        
        /* 加密消息 */
        printf("\n    加密消息测试...\n");
        uint8 plaintext[] = "Unlock Door Command";
        uint8 ciphertext[256];
        uint32 ciphertextLength = sizeof(ciphertext);
        uint8 authTag[CCC_AES_TAG_SIZE];
        
        result = Ccc_EncryptMessage(
            plaintext,
            sizeof(plaintext),
            ciphertext,
            &ciphertextLength,
            authTag
        );
        printf("    加密结果: ");
        PrintReturnCode(result);
        
        if (result == CCC_E_OK) {
            printf("    原文: %s\n", plaintext);
            printf("    密文长度: %u 字节\n", ciphertextLength);
            printf("    认证标签: ");
            for (int i = 0; i < 8; i++) {
                printf("%02X ", authTag[i]);
            }
            printf("...\n");
            
            /* 解密消息 */
            printf("\n    解密消息测试...\n");
            uint8 decrypted[256];
            uint32 decryptedLength = sizeof(decrypted);
            
            result = Ccc_DecryptMessage(
                ciphertext,
                ciphertextLength,
                authTag,
                decrypted,
                &decryptedLength
            );
            printf("    解密结果: ");
            PrintReturnCode(result);
            
            if (result == CCC_E_OK) {
                printf("    明文: %s\n", decrypted);
            }
        }
        
        /* 安全消息包测试 */
        printf("\n    安全消息包测试...\n");
        Ccc_SecureMessageType message;
        uint8 command[] = {0x01, 0x02, 0x03, 0x04};  /* 模拟命令 */
        
        result = Ccc_CreateSecureMessage(
            CCC_MSG_SECURE_MESSAGE,
            command,
            sizeof(command),
            &message
        );
        printf("    创建消息结果: ");
        PrintReturnCode(result);
        
        if (result == CCC_E_OK) {
            printf("    消息类型: 0x%04X\n", message.header.messageType);
            printf("    载荷长度: %u 字节\n", message.header.payloadLength);
            
            /* 解析消息 */
            uint8 receivedPayload[256];
            uint32 receivedPayloadLength = sizeof(receivedPayload);
            Ccc_MessageType msgType;
            
            result = Ccc_ParseSecureMessage(
                &message,
                receivedPayload,
                &receivedPayloadLength,
                &msgType
            );
            printf("    解析消息结果: ");
            PrintReturnCode(result);
            
            if (result == CCC_E_OK) {
                printf("    接收的命令: ");
                for (uint32 i = 0; i < receivedPayloadLength; i++) {
                    printf("%02X ", receivedPayload[i]);
                }
                printf("\n");
            }
        }
        
        /* 关闭会话 */
        printf("\n    关闭安全会话...\n");
        result = Ccc_SessionClose();
        printf("    结果: ");
        PrintReturnCode(result);
    }
}
