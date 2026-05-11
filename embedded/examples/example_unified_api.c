/******************************************************************************
 * @file    example_unified_api.c
 * @brief   Yule DKCS 统一 API 使用示例
 * 
 * 示例演示如何使用统一的 API 接口操作不同协议
 ******************************************************************************/

#include <stdio.h>
#include <string.h>
#include "dkcs.h"

void auth_callback(error_t result, const key_info_t *key_info, void *user_ctx)
{
    (void)user_ctx;
    
    printf("[Callback] Authentication result: %d\n", result);
    if (result == OK && key_info != NULL) {
        printf("[Callback] Key status: %s\n", 
               key_info->status == KEY_STATUS_ACTIVE ? "Active" : "Inactive");
    }
}

void demonstrate_protocol(protocol_type_t protocol, const char *name)
{
    printf("\n--- Testing %s Protocol ---\n", name);
    
    /* 初始化 */
    session_config_t config;
    memset(&config, 0, sizeof(config));
    config.protocol = protocol;
    config.conn_type = CONN_BLE;
    config.se_type = SE_ESE;
    config.timeout_ms = 30000;
    
    error_t ret = dkcs_init(&config);
    if (ret != OK) {
        printf("Init failed: %d\n", ret);
        return;
    }
    printf("Initialized %s\n", name);
    
    /* 配对 */
    uint8_t vin[17] = "LSVNV2182E2100001";
    printf("Starting pairing...\n");
    ret = dkcs_pairing_start(protocol, vin, auth_callback, NULL);
    if (ret != OK) {
        printf("Pairing failed: %d\n", ret);
    }
    
    /* 解锁 */
    uint8_t key_id[16] = {0x01, 0x02, 0x03, 0x04};
    printf("Unlocking vehicle...\n");
    ret = dkcs_vehicle_unlock(key_id, vin);
    if (ret == OK) {
        printf("Unlock successful!\n");
    } else {
        printf("Unlock failed: %d\n", ret);
    }
    
    /* 上锁 */
    printf("Locking vehicle...\n");
    ret = dkcs_vehicle_lock(key_id, vin);
    if (ret == OK) {
        printf("Lock successful!\n");
    }
    
    /* 启动发动机 */
    printf("Starting engine...\n");
    ret = dkcs_engine_start(key_id, vin);
    if (ret == OK) {
        printf("Engine started!\n");
    }
    
    /* 反初始化 */
    dkcs_deinit();
    printf("Deinitialized\n");
}

int main(void)
{
    printf("=== Yule DKCS Unified API Example ===\n");
    printf("Version: %s\n\n", dkcs_get_version());
    
    /* 演示各协议 */
#ifdef ENABLE_CCC
    demonstrate_protocol(PROTOCOL_CCC, "CCC Digital Key R3");
#endif

#ifdef ENABLE_ICCE
    demonstrate_protocol(PROTOCOL_ICCE, "ICCE (智慧车联)");
#endif

#ifdef ENABLE_ICCOA
    demonstrate_protocol(PROTOCOL_ICCOA, "ICCOA (开放联盟)");
#endif
    
    printf("\n=== All Protocol Tests Completed ===\n");
    return 0;
}
