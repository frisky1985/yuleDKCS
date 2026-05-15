/**
 * @file example.c
 * @brief Digital Key Unified Protocol - Usage Example
 */

#include "dk_unified.h"
#include <stdio.h>
#include <unistd.h>

/* ========================================================================
 *  回调函数
 * ======================================================================== */

static void on_connection_change(const dk_device_status_t *status, void *user_data)
{
    printf("[CONN] NFC: %d, BLE: %d, UWB: %d\n",
           status->conn.nfc_state,
           status->conn.ble_state,
           status->conn.uwb_state);
}

static void on_auth_change(const dk_auth_status_t *auth, void *user_data)
{
    printf("[AUTH] State: %d, Key ID: %02x%02x...\n",
           auth->state, auth->key_id[0], auth->key_id[1]);
}

static void on_zone_change(dk_zone_e zone, uint32_t distance_mm, void *user_data)
{
    const char *zone_names[] = {
        "LOCKED", "APPROACH", "UNLOCK", "ENTRY", "INSIDE", "UNKNOWN"
    };
    printf("[ZONE] %s (%d mm)\n", zone_names[zone], distance_mm);
    
    // 根据区域自动执行操作
    switch (zone) {
        case DK_ZONE_UNLOCK:
            printf("  -> Auto unlock\n");
            dk_vehicle_ctrl(DK_CTRL_UNLOCK, 0);
            break;
            
        case DK_ZONE_ENTRY:
            printf("  -> Welcome light\n");
            dk_vehicle_ctrl(DK_CTRL_LIGHTS, 1);
            break;
            
        case DK_ZONE_LOCKED:
            printf("  -> Auto lock\n");
            dk_vehicle_ctrl(DK_CTRL_LOCK, 0);
            break;
            
        default:
            break;
    }
}

/* ========================================================================
 *  主程序
 * ======================================================================== */

int main(int argc, char *argv[])
{
    printf("=== Digital Key Unified Protocol Demo ===\n\n");
    
    /* 1. 配置设备类型 */
    dk_device_type_t device = {0};
    
    // 设备标识
    memset(device.device_id, 0x01, 16);
    device.device_type = DK_DEVICE_VEHICLE_TCU;
    
    // 协议配置 (这里使用 CCC)
    device.protocol = DK_PROTOCOL_CCC;
    device.protocol_version = DK_VERSION_CCC_30;
    
    // 能力配置
    device.capabilities.capabilities = 
        DK_CAP_NFC | DK_CAP_BLE | DK_CAP_UWB | DK_CAP_SE | DK_CAP_LPCD;
    device.capabilities.max_keys = 8;
    device.capabilities.max_sessions = 4;
    device.capabilities.ble_mtu = 244;
    device.capabilities.uwb_max_range_cm = 1000;
    
    // 硬件信息
    memset(device.ble_mac, 0xAA, 6);
    memset(device.uwb_id, 0xBB, 8);
    memset(device.nfc_uid, 0xCC, 10);
    
    /* 2. 初始化 */
    printf("Initializing...\n");
    dk_status_t ret = dk_init(&device);
    if (ret != DK_OK) {
        printf("Init failed: %d\n", ret);
        return -1;
    }
    printf("Initialized OK\n\n");
    
    /* 3. 注册回调 */
    dk_register_conn_cb(on_connection_change, NULL);
    dk_register_auth_cb(on_auth_change, NULL);
    dk_register_zone_cb(on_zone_change, NULL);
    
    /* 4. 设置区域阈值 */
    dk_zone_set_threshold(1000, 500, 200, 50);
    printf("Zone thresholds: approach=10m, unlock=5m, entry=2m, inside=0.5m\n\n");
    
    /* 5. 开始监听 */
    printf("Starting listeners...\n");
    
    // NFC LPCD 监听
    ret = dk_nfc_start_listen();
    printf("  NFC LPCD: %s\n", ret == DK_OK ? "OK" : "FAIL");
    
    // BLE 广播
    ret = dk_ble_start_adv(DK_PROTOCOL_CCC);
    printf("  BLE Advertising: %s\n", ret == DK_OK ? "OK" : "FAIL");
    
    // UWB 测距
    uint32_t uwb_session = 0;
    ret = dk_uwb_start_ranging(&uwb_session);
    printf("  UWB Ranging (session %u): %s\n", uwb_session, ret == DK_OK ? "OK" : "FAIL");
    
    printf("\n=== Running (Ctrl+C to stop) ===\n\n");
    
    /* 6. 主循环 */
    int counter = 0;
    while (counter < 100) {
        // 驱动状态机
        dk_run();
        
        // 每 5 秒打印一次状态
        if (counter % 50 == 0) {
            dk_device_status_t status;
            if (dk_get_status(&status) == DK_OK) {
                printf("[%d] Auth: %d, Zone: %d\n", 
                       counter / 10,
                       status.auth.state,
                       status.location.zone);
            }
        }
        
        usleep(100000);  // 100ms
        counter++;
    }
    
    /* 7. 清理 */
    printf("\nShutting down...\n");
    
    dk_uwb_stop_ranging(uwb_session);
    dk_ble_stop_adv();
    dk_nfc_stop_listen();
    dk_deinit();
    
    printf("Done.\n");
    return 0;
}
