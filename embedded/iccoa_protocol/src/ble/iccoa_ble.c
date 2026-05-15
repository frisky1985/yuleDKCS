/**
 * @file iccoa_ble.c
 * @brief ICCOA BLE Service Implementation - GATT Server for Digital Key
 * @version 1.0
 * @date 2026-05-08
 *
 * BLE 服务特性:
 * - ICCOA Service UUID: 0xFEF5
 * - Data Characteristic: 读写/通知
 * - Control Characteristic: 写入
 * - 最大 MTU: 247
 * - 连接参数优化: 低功耗
 */

#include "iccoa_digital_key.h"
#include <string.h>

/* ========================================================================
 *  BLE Constants
 * ======================================================================== */
#define ICCOA_SVC_UUID              0xFEF5
#define ICCOA_CHAR_DATA_UUID        0xFEF6
#define ICCOA_CHAR_CTRL_UUID        0xFEF7
#define ICCOA_CHAR_UWB_UUID         0xFEF8

#define BLE_MAX_MTU                 247
#define BLE_MIN_CONN_INTERVAL_MS    30
#define BLE_MAX_CONN_INTERVAL_MS    100
#define BLE_SLAVE_LATENCY           4
#define BLE supervision_timeout_ms  400

#define BLE_ADV_INTERVAL_MIN_MS     100
#define BLE_ADV_INTERVAL_MAX_MS     200

/* ========================================================================
 *  Types
 * ======================================================================== */

/**
 * @brief BLE 连接状态
 */
typedef enum {
    BLE_STATE_IDLE = 0,
    BLE_STATE_ADVERTISING,
    BLE_STATE_CONNECTED,
    BLE_STATE_ENCRYPTING,
    BLE_STATE_READY
} ble_state_e;

/**
 * @brief BLE 上下文
 */
typedef struct {
    ble_state_e state;
    uint16_t conn_handle;
    uint16_t mtu;
    uint8_t  peer_addr[6];
    uint8_t  bonded;
    uint8_t  encrypted;

    /* GATT 句柄 */
    uint16_t svc_handle;
    uint16_t data_char_handle;
    uint16_t ctrl_char_handle;
    uint16_t uwb_char_handle;
    uint16_t data_ccc_handle;

    /* 接收回调 */
    iccoa_ble_recv_cb_t recv_cb;

    /* 发送缓冲 */
    uint8_t tx_buf[BLE_MAX_MTU];
    uint16_t tx_len;
    uint8_t  tx_busy;
} ble_ctx_t;

/* ========================================================================
 *  Globals
 * ======================================================================== */
static ble_ctx_t g_ble = {0};
static const uint8_t ADV_DATA[] = {
    /* Flags */
    0x02, 0x01, 0x06,
    /* Complete Local Name */
    0x0A, 0x09, 'I', 'C', 'C', 'O', 'A', '-', 'D', 'K',
    /* 16-bit Service UUIDs */
    0x03, 0x03, 0xF5, 0xFE,
    /* Appearance (Key) */
    0x03, 0x19, 0x81, 0x09,
    /* TX Power Level */
    0x02, 0x0A, 0x00
};

static const uint8_t SCAN_RSP_DATA[] = {
    /* Complete Local Name */
    0x0A, 0x09, 'I', 'C', 'C', 'O', 'A', '-', 'D', 'K'
};

/* ========================================================================
 *  External HAL Functions (Platform Specific)
 * ======================================================================== */
extern int hal_ble_init(void);
extern int hal_ble_deinit(void);
extern int hal_ble_start_advertising(const uint8_t *adv_data, uint8_t adv_len,
                                      const uint8_t *scan_rsp, uint8_t scan_len,
                                      uint16_t interval_min_ms, uint16_t interval_max_ms);
extern int hal_ble_stop_advertising(void);
extern int hal_ble_send_notification(uint16_t conn_handle, uint16_t char_handle,
                                      const uint8_t *data, uint16_t len);
extern int hal_ble_disconnect(uint16_t conn_handle);
extern int hal_ble_request_encryption(uint16_t conn_handle);
extern int hal_ble_update_connection(uint16_t conn_handle,
                                      uint16_t min_interval, uint16_t max_interval,
                                      uint16_t latency, uint16_t timeout);

/* ========================================================================
 *  GATT Service Setup
 * ======================================================================== */

/**
 * @brief 初始化 GATT 服务
 */
static int32_t ble_setup_gatt(void)
{
    /* 外部 GATT 注册函数 */
    extern int hal_ble_add_service(uint16_t uuid, uint16_t *svc_handle);
    extern int hal_ble_add_characteristic(uint16_t svc_handle, uint16_t uuid,
                                           uint8_t props, uint8_t perms,
                                           uint16_t *char_handle, uint16_t *ccc_handle);

    int ret;

    /* 添加 ICCOA 服务 */
    ret = hal_ble_add_service(ICCOA_SVC_UUID, &g_ble.svc_handle);
    if (ret != 0) return ICCOA_ERR_HARDWARE;

    /* 数据特性: Read | Write | Notify */
    ret = hal_ble_add_characteristic(g_ble.svc_handle, ICCOA_CHAR_DATA_UUID,
                                      0x1A,  /* READ | WRITE | NOTIFY */
                                      0x01,  /* READABLE */
                                      &g_ble.data_char_handle,
                                      &g_ble.data_ccc_handle);
    if (ret != 0) return ICCOA_ERR_HARDWARE;

    /* 控制特性: Write */
    ret = hal_ble_add_characteristic(g_ble.svc_handle, ICCOA_CHAR_CTRL_UUID,
                                      0x08,  /* WRITE */
                                      0x01,
                                      &g_ble.ctrl_char_handle, NULL);
    if (ret != 0) return ICCOA_ERR_HARDWARE;

    /* UWB 测距特性: Read | Notify */
    ret = hal_ble_add_characteristic(g_ble.svc_handle, ICCOA_CHAR_UWB_UUID,
                                      0x12,  /* READ | NOTIFY */
                                      0x01,
                                      &g_ble.uwb_char_handle, NULL);
    if (ret != 0) return ICCOA_ERR_HARDWARE;

    return ICCOA_OK;
}

/* ========================================================================
 *  BLE Event Handlers (Callbacks from HAL)
 * ======================================================================== */

/**
 * @brief 连接事件回调
 */
void hal_ble_on_connect(uint16_t conn_handle, const uint8_t *peer_addr)
{
    g_ble.state = BLE_STATE_CONNECTED;
    g_ble.conn_handle = conn_handle;
    g_ble.mtu = 23; /* 默认 MTU */
    memcpy(g_ble.peer_addr, peer_addr, 6);
    g_ble.bonded = 0;
    g_ble.encrypted = 0;

    /* 更新连接参数以降低功耗 */
    hal_ble_update_connection(conn_handle,
                               BLE_MIN_CONN_INTERVAL_MS,
                               BLE_MAX_CONN_INTERVAL_MS,
                               BLE_SLAVE_LATENCY,
                               BLE_SUPERVISION_TIMEOUT_MS);
}

/**
 * @brief 断开事件回调
 */
void hal_ble_on_disconnect(uint16_t conn_handle)
{
    (void)conn_handle;

    g_ble.state = BLE_STATE_IDLE;
    g_ble.conn_handle = 0;
    g_ble.mtu = 0;
    memset(g_ble.peer_addr, 0, 6);
    g_ble.bonded = 0;
    g_ble.encrypted = 0;
    g_ble.tx_busy = 0;

    /* 自动重新开始广播 */
    iccoa_ble_start_adv();
}

/**
 * @brief MTU 交换完成回调
 */
void hal_ble_on_mtu_exchanged(uint16_t conn_handle, uint16_t mtu)
{
    (void)conn_handle;
    g_ble.mtu = mtu;
}

/**
 * @brief 加密完成回调
 */
void hal_ble_on_encryption_complete(uint16_t conn_handle, uint8_t success)
{
    (void)conn_handle;
    if (success) {
        g_ble.encrypted = 1;
        g_ble.state = BLE_STATE_READY;
    }
}

/**
 * @brief 配对完成回调
 */
void hal_ble_on_bonding_complete(uint16_t conn_handle, uint8_t success)
{
    (void)conn_handle;
    if (success) {
        g_ble.bonded = 1;
        /* 配对成功后请求加密 */
        hal_ble_request_encryption(g_ble.conn_handle);
    }
}

/**
 * @brief GATT 写入回调
 */
void hal_ble_on_write(uint16_t conn_handle, uint16_t char_handle,
                       const uint8_t *data, uint16_t len)
{
    (void)conn_handle;

    /* 数据特性写入 */
    if (char_handle == g_ble.data_char_handle) {
        if (g_ble.recv_cb) {
            g_ble.recv_cb(data, len);
        }
    }
    /* 控制特性写入 */
    else if (char_handle == g_ble.ctrl_char_handle) {
        /* 控制命令处理 */
        if (len >= 1) {
            /* cmd = data[0], params = data[1..] */
            /* TODO: 分发到控制模块 */
        }
    }
    /* CCC 写入 */
    else if (char_handle == g_ble.data_ccc_handle) {
        /* 通知使能/禁用 */
        uint16_t ccc = *(uint16_t *)data;
        (void)ccc; /* TODO: 保存 CCC 状态 */
    }
}

/**
 * @brief GATT 读取回调
 */
void hal_ble_on_read(uint16_t conn_handle, uint16_t char_handle,
                      uint8_t *data, uint16_t *len)
{
    (void)conn_handle;

    if (char_handle == g_ble.data_char_handle) {
        /* 返回车辆状态 */
        iccoa_vehicle_status_t status;
        iccoa_service_get_status(&status);
        memcpy(data, &status, sizeof(status));
        *len = sizeof(status);
    }
    else if (char_handle == g_ble.uwb_char_handle) {
        /* 返回 UWB 测距数据 */
        /* TODO: 填充当前测距结果 */
        *len = 4;
    }
}

/* ========================================================================
 *  Public API
 * ======================================================================== */

int32_t iccoa_ble_init(void)
{
    memset(&g_ble, 0, sizeof(g_ble));

    int ret = hal_ble_init();
    if (ret != 0) return ICCOA_ERR_HARDWARE;

    ret = ble_setup_gatt();
    if (ret != ICCOA_OK) return ret;

    g_ble.state = BLE_STATE_IDLE;
    return ICCOA_OK;
}

int32_t iccoa_ble_deinit(void)
{
    if (g_ble.state == BLE_STATE_CONNECTED) {
        hal_ble_disconnect(g_ble.conn_handle);
    }
    hal_ble_deinit();
    memset(&g_ble, 0, sizeof(g_ble));
    return ICCOA_OK;
}

int32_t iccoa_ble_start_adv(void)
{
    if (g_ble.state == BLE_STATE_CONNECTED) {
        return ICCOA_ERR_BUSY;
    }

    int ret = hal_ble_start_advertising(
        ADV_DATA, sizeof(ADV_DATA),
        SCAN_RSP_DATA, sizeof(SCAN_RSP_DATA),
        BLE_ADV_INTERVAL_MIN_MS, BLE_ADV_INTERVAL_MAX_MS
    );

    if (ret != 0) return ICCOA_ERR_HARDWARE;

    g_ble.state = BLE_STATE_ADVERTISING;
    return ICCOA_OK;
}

int32_t iccoa_ble_stop_adv(void)
{
    if (g_ble.state != BLE_STATE_ADVERTISING) {
        return ICCOA_OK;
    }

    int ret = hal_ble_stop_advertising();
    if (ret != 0) return ICCOA_ERR_HARDWARE;

    g_ble.state = BLE_STATE_IDLE;
    return ICCOA_OK;
}

int32_t iccoa_ble_send(const uint8_t *data, uint16_t len)
{
    if (!data || len == 0) return ICCOA_ERR_PARAM;
    if (g_ble.state != BLE_STATE_READY && g_ble.state != BLE_STATE_CONNECTED) {
        return ICCOA_ERR_NOT_INIT;
    }
    if (len > g_ble.mtu - 3) return ICCOA_ERR_PARAM; /* ATT 头 3 字节 */
    if (g_ble.tx_busy) return ICCOA_ERR_BUSY;

    g_ble.tx_busy = 1;
    int ret = hal_ble_send_notification(g_ble.conn_handle,
                                         g_ble.data_char_handle,
                                         data, len);
    g_ble.tx_busy = 0;

    return (ret == 0) ? ICCOA_OK : ICCOA_ERR_HARDWARE;
}

int32_t iccoa_ble_register_cb(iccoa_ble_recv_cb_t cb)
{
    g_ble.recv_cb = cb;
    return ICCOA_OK;
}
