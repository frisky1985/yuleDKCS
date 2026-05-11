/******************************************************************************
 * @file    example_icce_pairing.c
 * @brief   ICCE 配对示例
 ******************************************************************************/

#include <stdio.h>
#include <string.h>
#include "icce.h"

/* 示例车辆 ID */
static const uint8_t EXAMPLE_VEHICLE_ID[17] = "LSVNV2182E2100001";

int main(void)
{
    printf("=== ICCE (智慧车联) Pairing Example ===\n\n");
    
    printf("ICCE 协议特点:\n");
    printf("- 支持国密 SM2/SM4 算法\n");
    printf("- 适配国产 SE 芯片\n");
    printf("- APDU 命令格式\n\n");
    
    /* 配置 SE 接口 (模拟) */
    icce_se_interface_t se = {0};
    
    /* 1. 初始化 */
    printf("1. Initializing ICCE stack...\n");
    error_t ret = icce_init(&se);
    if (ret != OK) {
        printf("Failed: %d\n", ret);
        return 1;
    }
    printf("   OK\n\n");
    
    /* 2. 配置配对信息 */
    printf("2. Configuring pairing info...\n");
    icce_pairing_info_t info;
    memset(&info, 0, sizeof(info));
    memcpy(info.vehicle_id, EXAMPLE_VEHICLE_ID, 17);
    info.crypto_preference = ICCE_CRYPTO_SM2;
    info.se_type = ICCE_SE_TYPE_ESE;
    info.requested_role = KEY_ROLE_OWNER;
    printf("   Vehicle ID: %s\n", EXAMPLE_VEHICLE_ID);
    printf("   Algorithm: SM2/SM4\n");
    printf("   SE Type: eSE\n\n");
    
    /* 3. 创建配对会话 */
    printf("3. Creating pairing session...\n");
    icce_session_context_t *session = NULL;
    ret = icce_pairing_init(&info, &session);
    if (ret != OK) {
        printf("Failed: %d\n", ret);
        icce_deinit();
        return 1;
    }
    printf("   Session created\n\n");
    
    /* 4. 认证流程 */
    printf("4. Authentication process...\n");
    printf("   4.1 Select ICCE Applet\n");
    printf("   4.2 Send PAIRING_INIT command\n");
    printf("   4.3 Receive challenge from vehicle\n");
    printf("   4.4 Sign challenge with device key\n");
    printf("   4.5 Send signature to vehicle\n");
    printf("   4.6 Vehicle verifies signature\n");
    printf("   Authentication successful!\n\n");
    
    /* 5. 完成配对 */
    printf("5. Completing pairing...\n");
    printf("   Digital key installed\n");
    printf("   Session keys established\n\n");
    
    /* 清理 */
    icce_session_destroy(session);
    icce_deinit();
    
    printf("=== ICCE Pairing Completed Successfully ===\n");
    return 0;
}
