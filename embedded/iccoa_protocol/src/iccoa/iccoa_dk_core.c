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

/* Include logger for audit event recording */
#include "../../../ccc_protocol/src/logger/dk_logger.h"

/* ========================================================================
 *  Configuration
 * ======================================================================== */
#define ICCOA_DEFAULT_VERSION   4  /* 默认 DK 4.0 */

/* ========================================================================
 *  Security: DK4.0→DK3.0 降级保护
 * ======================================================================== */
/* 当 no_downgrade 置位时，禁止从 DK4.0 (HMAC) 降级到 DK3.0 (XOR)，
 * 并记录审计事件。 */

/* ========================================================================
 *  State
 * ======================================================================== */
typedef struct {
    uint8_t version;            /**< 当前协议版本 (3 or 4) */
    uint8_t initialized;
    uint8_t running;
    uint8_t no_downgrade;       /**< [V-16] 禁止从 DK4.0 降级到 DK3.0 */
    uint8_t downgrade_attempted; /**< [V-16] 是否检测到降级尝试 */
} iccoa_ctx_t;

static iccoa_ctx_t g_ctx = {0};

/* ========================================================================
 *  BLE Data Dispatcher
 * ======================================================================== */

/**
 * @brief BLE 接收回调 - 分发到协议处理器
 *
 * @note [V-16] 当 no_downgrade 置位且当前为 DK4.0 时，
 *       检测到 DK3.0 帧视为降级攻击尝试，记录审计事件并丢弃。
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
        /* [V-16] 检测降级攻击: 在 DK4.0 模式收到 DK3.0 帧 */
        if (data[0] == DK30_SOP) {
            if (g_ctx.no_downgrade) {
                g_ctx.downgrade_attempted = 1;
                DK_LOG_SEC_WARN("降级攻击检测: DK4.0 模式下收到 DK3.0 帧, 已丢弃");
                return;
            }
        }

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
    /* [V-16] 默认启用 DK4.0→DK3.0 降级保护 */
    g_ctx.no_downgrade = 1;
    g_ctx.downgrade_attempted = 0;
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

#ifndef TEST_MODE
    /* 主循环 (阻塞) — 在 TEST_MODE 下跳过，防止主机测试环境挂死 */
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
#endif /* TEST_MODE */

    return ICCOA_OK;
}

/**
 * @brief 设置协议版本
 *
 * @note [V-16] 当 no_downgrade 置位时，禁止从 DK4.0 (HMAC)
 *       降级到 DK3.0 (XOR)。记录审计事件并返回错误。
 */
int32_t iccoa_set_version(uint8_t version)
{
    if (version != 3 && version != 4) return ICCOA_ERR_PARAM;

    /* [V-16] 检查降级安全策略 */
    if (g_ctx.no_downgrade && g_ctx.version == 4 && version == 3) {
        g_ctx.downgrade_attempted = 1;
        DK_LOG_SEC_WARN("降级攻击阻止: DK4.0→DK3.0 被 no_downgrade 策略拒绝");
        return ICCOA_ERR_SECURITY;
    }

    g_ctx.version = version;
    return ICCOA_OK;
}

/**
 * @brief 设置/清除 no_downgrade 标志
 * @param enable 1=启用降级保护, 0=关闭
 *
 * @note [V-16] 启用后阻止从 DK4.0 (HMAC) 降级到 DK3.0 (XOR)。
 *       默认在 DK4.0 模式下启用。
 */
void iccoa_set_no_downgrade(uint8_t enable)
{
    g_ctx.no_downgrade = enable;
    if (enable) {
        DK_LOG_SEC_INFO("降级保护已启用 (DK4.0→DK3.0)");
    } else {
        DK_LOG_SEC_WARN("降级保护已禁用");
    }
}

/**
 * @brief 查询是否检测到降级尝试
 * @return 1=检测到降级攻击, 0=未检测
 */
uint8_t iccoa_is_downgrade_attempted(void)
{
    return g_ctx.downgrade_attempted;
}

/**
 * @brief 清除降级尝试标志
 */
void iccoa_clear_downgrade_flag(void)
{
    g_ctx.downgrade_attempted = 0;
}

/**
 * @brief 获取当前协议版本
 */
uint8_t iccoa_get_version(void)
{
    return g_ctx.version;
}
