/******************************************************************************
 * @file    example_iccoa_control.c
 * @brief   ICCOA 车辆控制示例
 ******************************************************************************/

#include <stdio.h>
#include <string.h>
#include "iccoa.h"

void print_vehicle_command(iccoa_command_t cmd)
{
    switch (cmd) {
        case ICCOA_CMD_VEHICLE_UNLOCK: printf("VEHICLE_UNLOCK"); break;
        case ICCOA_CMD_VEHICLE_LOCK: printf("VEHICLE_LOCK"); break;
        case ICCOA_CMD_ENGINE_START: printf("ENGINE_START"); break;
        case ICCOA_CMD_ENGINE_STOP: printf("ENGINE_STOP"); break;
        case ICCOA_CMD_TRUNK_OPEN: printf("TRUNK_OPEN"); break;
        case ICCOA_CMD_CLIMATE_ON: printf("CLIMATE_ON"); break;
        default: printf("UNKNOWN(0x%02X)", cmd);
    }
}

int main(void)
{
    printf("=== ICCOA Vehicle Control Example ===\n\n");
    
    printf("ICCOA 协议特点:\n");
    printf("- TLV 消息格式\n");
    printf("- 支持 BLE/NFC 双模式\n");
    printf("- 与小米、OPPO、vivo、一加等厂商兼容\n\n");
    
    /* 1. 初始化 */
    printf("1. Initializing ICCOA stack...\n");
    iccoa_se_interface_t se = {0};
    error_t ret = iccoa_init(&se);
    if (ret != OK) {
        printf("Failed: %d\n", ret);
        return 1;
    }
    printf("   OK\n\n");
    
    /* 2. 配置设备 */
    printf("2. Configuring device...\n");
    iccoa_pairing_config_t config;
    memset(&config, 0, sizeof(config));
    config.enable_ble = true;
    config.enable_nfc = true;
    printf("   BLE: enabled\n");
    printf("   NFC: enabled\n\n");
    
    /* 3. 配对 */
    printf("3. Starting pairing...\n");
    iccoa_session_context_t *session = NULL;
    ret = iccoa_pairing_start(&config, &session);
    if (ret != OK) {
        printf("Failed: %d\n", ret);
        iccoa_deinit();
        return 1;
    }
    printf("   Pairing started\n\n");
    
    /* 4. 建立会话 */
    printf("4. Establishing session...\n");
    uint8_t key_id[ICCOA_KEY_ID_LEN] = {0x01, 0x02, 0x03, 0x04};
    ret = iccoa_session_create(session, key_id);
    if (ret != OK) {
        printf("Failed: %d\n", ret);
        iccoa_session_destroy(session);
        iccoa_deinit();
        return 1;
    }
    printf("   Session established\n\n");
    
    /* 5. 车辆控制 */
    printf("5. Vehicle control commands:\n\n");
    
    iccoa_command_t commands[] = {
        ICCOA_CMD_VEHICLE_UNLOCK,
        ICCOA_CMD_ENGINE_START,
        ICCOA_CMD_CLIMATE_ON,
        ICCOA_CMD_ENGINE_STOP,
        ICCOA_CMD_VEHICLE_LOCK,
    };
    
    for (int i = 0; i < 5; i++) {
        printf("   Command %d: ", i + 1);
        print_vehicle_command(commands[i]);
        printf("\n");
        
        uint8_t result = 0;
        ret = iccoa_vehicle_control(session, commands[i], NULL, 0, &result);
        if (ret == OK) {
            printf("   Result: SUCCESS\n\n");
        } else {
            printf("   Result: FAILED (%d)\n\n", ret);
        }
    }
    
    /* 6. 获取状态 */
    printf("6. Querying vehicle status...\n");
    iccoa_vehicle_status_t status;
    ret = iccoa_get_vehicle_status(session, &status);
    if (ret == OK) {
        printf("   Lock status: %s\n", status.lock_status ? "LOCKED" : "UNLOCKED");
        printf("   Engine: %s\n", status.engine_status ? "RUNNING" : "STOPPED");
        printf("   Fuel/Battery: %d%%\n", status.fuel_level);
    }
    printf("\n");
    
    /* 清理 */
    iccoa_session_destroy(session);
    iccoa_deinit();
    
    printf("=== ICCOA Control Example Completed ===\n");
    return 0;
}
