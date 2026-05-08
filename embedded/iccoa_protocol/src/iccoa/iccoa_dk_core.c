/**
 * @file iccoa_dk_core.c
 * @brief ICCOA Digital Key Core - Protocol Stack Entry Point
 * @version 1.0
 * @date 2026-05-08
 *
 * 功能:
 * - 协议栈初始化
 * - DK 3.0 / DK 4.0 版本选择
 * - BLE 数据分发
 * - 主循环处理
 */

#include "iccoa_digital_key.h"
#include <string.h>

/* ========================================================================
 *  Configuration
 * ======================================================================== */
#define ICCOA_DEFAULT_VERSION   4  /* 默认 DK 4.0 */

/* ========================================================================
 *  State
 * ======================================================================== */
typedef struct {
    uint8_t version;            /**< 当前协议版本 (3 or 4) */
    uint8_t initialized;
    uint8_t running;
} iccoa_ctx_t;

static iccoa_ctx_t g_ctx = {0};

/* ========================================================================
 *  BLE Data Dispatcher
 * ======================================================================== */

/**
 * @brief BLE 接收回调 - 分发到协议处理器
 */
static void ble_data_handler(const uint8_t *data, uint16_t len)
{
    if (!data || len < 2) return;

    /* 根据协议版本分发 */
    if (g_ctx.version == 3) {
        /* DK 3.0: SOP = 0xAA */
        if (data[0] == DK30_SOP) {
            iccoa_dk30_process(data, len);
        }
    }
    else if (g_ctx.version == 4) {
        /* DK 4.0: Magic = 0xICC0 */
        if (data[0] == 0xC0 && data[1] == 0x0C) {
            iccoa_dk40_process(data, len);
        }
    }
}

/* ========================================================================
 *  Public API
 * ======================================================================== */

int32_t iccoa_dk_init(void)
{
    if (g_ctx.initialized) return ICCOA_OK;

    int32_t ret;

    /* 初始化 BLE */
    ret = iccoa_ble_init();
    if (ret != ICCOA_OK) return ret;

    /* 初始化认证模块 */
    ret = iccoa_auth_init();
    if (ret != ICCOA_OK) return ret;

    /* 初始化车辆服务 */
    ret = iccoa_service_init();
    if (ret != ICCOA_OK) return ret;

    /* 初始化 DK 3.0 */
    ret = iccoa_dk30_init();
    if (ret != ICCOA_OK) return ret;

    /* 初始化 DK 4.0 */
    ret = iccoa_dk40_init();
    if (ret != ICCOA_OK) return ret;

    /* 注册 BLE 回调 */
    iccoa_ble_register_cb(ble_data_handler);

    /* 设置默认协议版本 */
    g_ctx.version = ICCOA_DEFAULT_VERSION;
    g_ctx.initialized = 1;
    g_ctx.running = 0;

    return ICCOA_OK;
}

int32_t iccoa_dk_deinit(void)
{
    if (!g_ctx.initialized) return ICCOA_OK;

    /* 停止 BLE */
    iccoa_ble_stop_adv();
    iccoa_ble_deinit();

    g_ctx.initialized = 0;
    g_ctx.running = 0;

    return ICCOA_OK;
}

int32_t iccoa_dk_run(void)
{
    if (!g_ctx.initialized) return ICCOA_ERR_NOT_INIT;
    if (g_ctx.running) return ICCOA_OK;

    /* 开始 BLE 广播 */
    int32_t ret = iccoa_ble_start_adv();
    if (ret != ICCOA_OK) return ret;

    g_ctx.running = 1;

    /* 主循环 (阻塞) */
    while (g_ctx.running) {
        /* 平台相关的主循环:
         * - 处理 BLE 事件
         * - 处理 UWB 测距
         * - 处理定时任务
         * - 低功耗休眠
         */
        extern void platform_main_loop_step(void);
        platform_main_loop_step();
    }

    return ICCOA_OK;
}

/**
 * @brief 设置协议版本
 */
int32_t iccoa_set_version(uint8_t version)
{
    if (version != 3 && version != 4) return ICCOA_ERR_PARAM;
    g_ctx.version = version;
    return ICCOA_OK;
}

/**
 * @brief 获取当前协议版本
 */
uint8_t iccoa_get_version(void)
{
    return g_ctx.version;
}
