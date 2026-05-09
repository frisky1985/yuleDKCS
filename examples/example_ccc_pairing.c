/******************************************************************************
 * @file    example_ccc_pairing.c
 * @brief   CCC 配对示例
 * 
 * 运行流程:
 * 1. 初始化 CCC 协议栈
 * 2. 创建配对会话
 * 3. 发送配对请求
 * 4. 处理配对响应
 * 5. 完成配对
 ******************************************************************************/

#include <stdio.h>
#include <string.h>
#include "ccc.h"

/* 示例 VIN */
static const uint8_t EXAMPLE_VIN[17] = "LSVNV2182E2100001";

/* 模拟 SE 实现 */
static int example_se_init(void)
{
    printf("[SE] 安全元件初始化\n");
    return 0;
}

static int example_se_deinit(void)
{
    printf("[SE] 安全元件反初始化\n");
    return 0;
}

static int example_gen_key(uint8_t *pub, uint8_t *priv)
{
    /* 模拟生成 P-256 密钥对 */
    memset(pub, 0x04, 1);  /* 未压缩格式标识 */
    memset(pub + 1, 0xAA, 32);  /* X 坐标 */
    memset(pub + 33, 0xBB, 32); /* Y 坐标 */
    memset(priv, 0xCC, 32);
    printf("[SE] 生成 ECDH 密钥对\n");
    return 0;
}

static int example_sign(const uint8_t *data, size_t len, 
                        const uint8_t *priv, uint8_t *sig)
{
    (void)data; (void)len; (void)priv;
    memset(sig, 0xDD, 64);
    printf("[SE] ECDSA 签名\n");
    return 0;
}

static int example_verify(const uint8_t *data, size_t len,
                          const uint8_t *pub, const uint8_t *sig)
{
    (void)data; (void)len; (void)pub; (void)sig;
    printf("[SE] ECDSA 验证签名\n");
    return 1;  /* 验证通过 */
}

static int example_derive(const uint8_t *priv, const uint8_t *pub, uint8_t *secret)
{
    (void)priv; (void)pub;
    memset(secret, 0xEE, 32);
    printf("[SE] ECDH 共享秘钥计算\n");
    return 0;
}

int main(void)
{
    printf("=== CCC Digital Key Pairing Example ===\n\n");
    
    /* 配置 SE 接口 */
    ccc_se_interface_t se = {
        .init = example_se_init,
        .deinit = example_se_deinit,
        .generate_key_pair = example_gen_key,
        .sign = example_sign,
        .verify = example_verify,
        .derive_shared_secret = example_derive,
    };
    
    /* 1. 初始化 CCC */
    printf("1. Initializing CCC stack...\n");
    error_t ret = ccc_init(&se);
    if (ret != OK) {
        printf("Failed to init CCC: %d\n", ret);
        return 1;
    }
    printf("   OK\n\n");
    
    /* 2. 创建配对配置 */
    printf("2. Creating pairing configuration...\n");
    ccc_pairing_config_t config;
    memset(&config, 0, sizeof(config));
    memcpy(config.vehicle_vin, EXAMPLE_VIN, 17);
    config.requested_role = KEY_ROLE_OWNER;
    config.requested_permissions = PERM_UNLOCK | PERM_LOCK | 
                                    PERM_ENGINE_START | PERM_SHARE;
    printf("   VIN: %s\n", EXAMPLE_VIN);
    printf("   Role: Owner\n");
    printf("   OK\n\n");
    
    /* 3. 创建配对会话 */
    printf("3. Creating pairing session...\n");
    ccc_session_context_t *session = NULL;
    ret = ccc_create_pairing_session(&config, &session);
    if (ret != OK) {
        printf("Failed to create session: %d\n", ret);
        ccc_deinit();
        return 1;
    }
    printf("   Session created\n\n");
    
    /* 4. 开始配对 */
    printf("4. Starting pairing process...\n");
    uint8_t request[512];
    size_t request_len = sizeof(request);
    ret = ccc_start_pairing(session, request, &request_len);
    if (ret != OK) {
        printf("Failed to start pairing: %d\n", ret);
        ccc_destroy_session(session);
        ccc_deinit();
        return 1;
    }
    printf("   Pairing request generated (%zu bytes)\n", request_len);
    printf("   Ephemeral public key and challenge sent\n\n");
    
    /* 5. 模拟车辆响应 */
    printf("5. Processing vehicle response...\n");
    uint8_t response[512];
    size_t response_len = 1 + 65 + 32 + 64;  /* Version + PubKey + Challenge + Sig */
    response[0] = 0x30;  /* Version 3.0 */
    memset(response + 1, 0x11, 65);  /* Vehicle public key */
    memset(response + 66, 0x22, 32); /* Vehicle challenge */
    memset(response + 98, 0x33, 64); /* Signature */
    
    ret = ccc_process_pairing_response(session, response, response_len);
    if (ret != OK) {
        printf("Failed to process response: %d\n", ret);
        ccc_destroy_session(session);
        ccc_deinit();
        return 1;
    }
    printf("   Vehicle response processed\n");
    printf("   Shared secret derived\n\n");
    
    /* 6. 完成配对 */
    printf("6. Completing pairing...\n");
    uint8_t confirmation[512];
    size_t confirmation_len = sizeof(confirmation);
    ret = ccc_complete_pairing(session, confirmation, &confirmation_len);
    if (ret != OK) {
        printf("Failed to complete pairing: %d\n", ret);
        ccc_destroy_session(session);
        ccc_deinit();
        return 1;
    }
    printf("   Pairing completed successfully!\n");
    printf("   Confirmation sent (%zu bytes)\n\n", confirmation_len);
    
    /* 7. 建立安全会话 */
    printf("7. Establishing secure session...\n");
    ret = ccc_establish_session(session, response + 1, response + 66);
    if (ret != OK) {
        printf("Failed to establish session: %d\n", ret);
        ccc_destroy_session(session);
        ccc_deinit();
        return 1;
    }
    printf("   Secure session established\n");
    printf("   Session keys derived\n\n");
    
    /* 清理 */
    printf("8. Cleanup...\n");
    ccc_destroy_session(session);
    ccc_deinit();
    printf("   Done\n\n");
    
    printf("=== Pairing Example Completed Successfully ===\n");
    return 0;
}
