/**
 * @file iccoa_service.c
 * @brief ICCOA Vehicle Service Implementation
 * @version 1.0
 * @date 2026-05-08
 *
 * 车辆控制服务:
 * - 门锁控制 (解锁/上锁)
 * - 引擎控制 (启动/停止)
 * - 车窗控制
 * - 后备箱控制
 * - 空调控制
 * - 寻车功能
 */

#include "iccoa_digital_key.h"
#include <string.h>

/* ========================================================================
 *  Vehicle Control Interface (Platform Specific)
 * ======================================================================== */

/**
 * @brief 车辆控制 HAL 函数 (需平台实现)
 */
extern int hal_vehicle_lock_doors(uint8_t lock);
extern int hal_vehicle_start_engine(uint8_t start);
extern int hal_vehicle_control_window(uint8_t window_id, uint8_t direction);
extern int hal_vehicle_open_trunk(void);
extern int hal_vehicle_set_climate(uint8_t on, int8_t temp);
extern int hal_vehicle_horn(uint8_t times);
extern int hal_vehicle_flash_lights(uint8_t pattern);

/**
 * @brief 车辆状态读取 HAL 函数
 */
extern int hal_vehicle_get_door_status(uint8_t *status);
extern int hal_vehicle_get_window_status(uint8_t *status);
extern int hal_vehicle_get_engine_status(uint8_t *status);
extern int hal_vehicle_get_lock_status(uint8_t *status);
extern int hal_vehicle_get_battery_level(int8_t *level);
extern int hal_vehicle_get_interior_temp(int16_t *temp);
extern int hal_vehicle_get_alarm_status(uint8_t *status);

/* ========================================================================
 *  Service State
 * ======================================================================== */
static iccoa_vehicle_status_t g_vehicle_status;
static bool g_initialized = false;

/* ========================================================================
 *  Control Command Handlers
 * ======================================================================== */

/**
 * @brief 执行门锁控制
 */
static int32_t ctrl_door_lock(uint8_t lock)
{
    int ret = hal_vehicle_lock_doors(lock);
    if (ret == 0) {
        /* 更新状态 */
        hal_vehicle_get_lock_status(&g_vehicle_status.lock_status);
    }
    return (ret == 0) ? ICCOA_OK : ICCOA_ERR_HARDWARE;
}

/**
 * @brief 执行引擎控制
 */
static int32_t ctrl_engine(uint8_t start)
{
    /* 安全检查: 引擎启动需要特定条件 */
    if (start) {
        /* 检查刹车状态、挡位等 */
        /* TODO: 添加安全检查逻辑 */
    }

    int ret = hal_vehicle_start_engine(start);
    if (ret == 0) {
        hal_vehicle_get_engine_status(&g_vehicle_status.engine_status);
    }
    return (ret == 0) ? ICCOA_OK : ICCOA_ERR_HARDWARE;
}

/**
 * @brief 执行车窗控制
 */
static int32_t ctrl_window(uint8_t param)
{
    /* param: [window_id(4bits)] | [direction(4bits)] */
    uint8_t window_id = (param >> 4) & 0x0F;
    uint8_t direction = param & 0x0F;

    int ret = hal_vehicle_control_window(window_id, direction);
    if (ret == 0) {
        hal_vehicle_get_window_status(&g_vehicle_status.window_status);
    }
    return (ret == 0) ? ICCOA_OK : ICCOA_ERR_HARDWARE;
}

/**
 * @brief 执行后备箱控制
 */
static int32_t ctrl_trunk(void)
{
    int ret = hal_vehicle_open_trunk();
    return (ret == 0) ? ICCOA_OK : ICCOA_ERR_HARDWARE;
}

/**
 * @brief 执行空调控制
 */
static int32_t ctrl_climate(uint8_t on)
{
    int ret = hal_vehicle_set_climate(on, 22); /* 默认 22°C */
    return (ret == 0) ? ICCOA_OK : ICCOA_ERR_HARDWARE;
}

/**
 * @brief 执行寻车功能
 */
static int32_t ctrl_find(void)
{
    /* 闪烁灯光 + 鸣笛 */
    hal_vehicle_flash_lights(0x03); /* 双闪 */
    hal_vehicle_horn(3);

    return ICCOA_OK;
}

/**
 * @brief 执行鸣笛
 */
static int32_t ctrl_horn(uint8_t times)
{
    int ret = hal_vehicle_horn(times);
    return (ret == 0) ? ICCOA_OK : ICCOA_ERR_HARDWARE;
}

/* ========================================================================
 *  Public API
 * ======================================================================== */

int32_t iccoa_service_init(void)
{
    if (g_initialized) return ICCOA_OK;

    memset(&g_vehicle_status, 0, sizeof(g_vehicle_status));

    /* 初始化车辆状态 */
    hal_vehicle_get_door_status(&g_vehicle_status.door_status);
    hal_vehicle_get_window_status(&g_vehicle_status.window_status);
    hal_vehicle_get_engine_status(&g_vehicle_status.engine_status);
    hal_vehicle_get_lock_status(&g_vehicle_status.lock_status);
    hal_vehicle_get_battery_level(&g_vehicle_status.battery_level);
    hal_vehicle_get_interior_temp(&g_vehicle_status.interior_temp);
    hal_vehicle_get_alarm_status(&g_vehicle_status.alarm_status);

    g_initialized = true;
    return ICCOA_OK;
}

int32_t iccoa_ctrl_execute(iccoa_ctrl_cmd_e cmd, uint8_t param)
{
    if (!g_initialized) return ICCOA_ERR_NOT_INIT;

    switch (cmd) {
    case CTRL_LOCK:
        return ctrl_door_lock(1);

    case CTRL_UNLOCK:
        return ctrl_door_lock(0);

    case CTRL_ENGINE_ON:
        return ctrl_engine(1);

    case CTRL_ENGINE_OFF:
        return ctrl_engine(0);

    case CTRL_TRUNK_OPEN:
        return ctrl_trunk();

    case CTRL_WINDOW_UP:
        return ctrl_window((param << 4) | 0x01);

    case CTRL_WINDOW_DOWN:
        return ctrl_window((param << 4) | 0x02);

    case CTRL_CLIMATE_ON:
        return ctrl_climate(1);

    case CTRL_CLIMATE_OFF:
        return ctrl_climate(0);

    case CTRL_FIND:
        return ctrl_find();

    case CTRL_HORN:
        return ctrl_horn(param);

    default:
        return ICCOA_ERR_PARAM;
    }
}

int32_t iccoa_service_get_status(iccoa_vehicle_status_t *status)
{
    if (!status) return ICCOA_ERR_PARAM;
    if (!g_initialized) return ICCOA_ERR_NOT_INIT;

    /* 刷新状态 */
    hal_vehicle_get_door_status(&g_vehicle_status.door_status);
    hal_vehicle_get_window_status(&g_vehicle_status.window_status);
    hal_vehicle_get_engine_status(&g_vehicle_status.engine_status);
    hal_vehicle_get_lock_status(&g_vehicle_status.lock_status);
    hal_vehicle_get_battery_level(&g_vehicle_status.battery_level);
    hal_vehicle_get_interior_temp(&g_vehicle_status.interior_temp);
    hal_vehicle_get_alarm_status(&g_vehicle_status.alarm_status);

    memcpy(status, &g_vehicle_status, sizeof(iccoa_vehicle_status_t));
    return ICCOA_OK;
}

/**
 * @brief 更新车辆状态 (由外部事件调用)
 */
void iccoa_service_update_status(void)
{
    if (!g_initialized) return;

    /* 读取最新状态 */
    hal_vehicle_get_door_status(&g_vehicle_status.door_status);
    hal_vehicle_get_window_status(&g_vehicle_status.window_status);
    hal_vehicle_get_engine_status(&g_vehicle_status.engine_status);
    hal_vehicle_get_lock_status(&g_vehicle_status.lock_status);
    hal_vehicle_get_battery_level(&g_vehicle_status.battery_level);
    hal_vehicle_get_interior_temp(&g_vehicle_status.interior_temp);
    hal_vehicle_get_alarm_status(&g_vehicle_status.alarm_status);

    /* TODO: 触发状态通知到已连接的手机 */
}
