/******************************************************************************
 * @file    uwb_driver.c
 * @brief   UWB 芯片驱动抽象层实现
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-09
 *
 * @note    支持多种 UWB 芯片的驱动抽象层
 *          符合 CCC Digital Key R3 UWB 规范
 ******************************************************************************/

#include <string.h>
#include <stdio.h>
#include "uwb_hal.h"

/******************************************************************************
 * 内部状态和宏定义
 ******************************************************************************/

#define MAX_UWB_DRIVERS         4
#define MAX_DRIVER_NAME_LEN     32

/* UWB 信道频率定义 (MHz) */
#define UWB_CHANNEL_5_FREQ      6489
#define UWB_CHANNEL_6_FREQ      6989
#define UWB_CHANNEL_8_FREQ      7488
#define UWB_CHANNEL_9_FREQ      7987

/* 测距超时默认值 (ms) */
#define DEFAULT_RANGING_TIMEOUT_MS      100
#define DEFAULT_RANGING_INTERVAL_MS     100

/******************************************************************************
 * 全局状态
 ******************************************************************************/

typedef struct {
    uwb_chip_driver_t   *drivers[MAX_UWB_DRIVERS];
    uint8_t             driver_count;
    uwb_chip_driver_t   *default_driver;
    bool                initialized;
    uint32_t            total_ranging_count;
    uint32_t            failed_ranging_count;
} uwb_driver_manager_t;

static uwb_driver_manager_t g_driver_mgr = {
    .driver_count = 0,
    .default_driver = NULL,
    .initialized = false,
    .total_ranging_count = 0,
    .failed_ranging_count = 0
};

/******************************************************************************
 * 内部函数声明
 ******************************************************************************/
static int uwb_driver_validate_config(const uwb_hal_config_t *config);
static int uwb_driver_wait_for_state(uwb_chip_driver_t *driver, uwb_chip_state_t state, uint32_t timeout_ms);
static float uwb_calculate_distance(uint32_t timestamp_tx, uint32_t timestamp_rx, uint32_t resp_delay);

/******************************************************************************
 * 驱动抽象层 API 实现
 ******************************************************************************/

/**
 * @brief 初始化 UWB 驱动管理器
 */
int uwb_driver_init(void)
{
    if (g_driver_mgr.initialized) {
        return UWB_HAL_ERROR_ALREADY_INITIALIZED;
    }

    memset(&g_driver_mgr, 0, sizeof(g_driver_mgr));
    g_driver_mgr.initialized = true;

    return UWB_HAL_OK;
}

/**
 * @brief 反初始化 UWB 驱动管理器
 */
int uwb_driver_deinit(void)
{
    if (!g_driver_mgr.initialized) {
        return UWB_HAL_ERROR_NOT_INITIALIZED;
    }

    /* 停止所有测距并注销驱动 */
    for (uint8_t i = 0; i < g_driver_mgr.driver_count; i++) {
        uwb_chip_driver_t *driver = g_driver_mgr.drivers[i];
        if (driver != NULL && driver->initialized) {
            if (driver->ops.stop_ranging != NULL) {
                driver->ops.stop_ranging(driver);
            }
            if (driver->ops.deinit != NULL) {
                driver->ops.deinit(driver);
            }
        }
    }

    memset(&g_driver_mgr, 0, sizeof(g_driver_mgr));
    return UWB_HAL_OK;
}

/**
 * @brief 注册 UWB 芯片驱动
 */
int uwb_driver_register_chip(uwb_chip_driver_t *driver)
{
    if (!g_driver_mgr.initialized) {
        return UWB_HAL_ERROR_NOT_INITIALIZED;
    }

    if (driver == NULL) {
        return UWB_HAL_ERROR_INVALID_PARAM;
    }

    if (g_driver_mgr.driver_count >= MAX_UWB_DRIVERS) {
        return UWB_HAL_ERROR_NO_MEMORY;
    }

    /* 检查是否已注册 */
    for (uint8_t i = 0; i < g_driver_mgr.driver_count; i++) {
        if (g_driver_mgr.drivers[i] == driver) {
            return UWB_HAL_ERROR_ALREADY_INITIALIZED;
        }
    }

    /* 初始化驱动结构 */
    driver->initialized = false;
    driver->state = UWB_CHIP_STATE_UNINIT;

    g_driver_mgr.drivers[g_driver_mgr.driver_count++] = driver;

    /* 如果是第一个驱动，设为默认 */
    if (g_driver_mgr.driver_count == 1) {
        g_driver_mgr.default_driver = driver;
    }

    return UWB_HAL_OK;
}

/**
 * @brief 注销 UWB 芯片驱动
 */
int uwb_driver_unregister_chip(uwb_chip_driver_t *driver)
{
    if (!g_driver_mgr.initialized) {
        return UWB_HAL_ERROR_NOT_INITIALIZED;
    }

    if (driver == NULL) {
        return UWB_HAL_ERROR_INVALID_PARAM;
    }

    /* 查找并移除 */
    for (uint8_t i = 0; i < g_driver_mgr.driver_count; i++) {
        if (g_driver_mgr.drivers[i] == driver) {
            /* 停止测距并反初始化 */
            if (driver->initialized) {
                if (driver->ops.stop_ranging != NULL) {
                    driver->ops.stop_ranging(driver);
                }
                if (driver->ops.deinit != NULL) {
                    driver->ops.deinit(driver);
                }
            }

            /* 移动数组元素 */
            for (uint8_t j = i; j < g_driver_mgr.driver_count - 1; j++) {
                g_driver_mgr.drivers[j] = g_driver_mgr.drivers[j + 1];
            }
            g_driver_mgr.driver_count--;

            /* 更新默认驱动 */
            if (g_driver_mgr.default_driver == driver) {
                g_driver_mgr.default_driver = (g_driver_mgr.driver_count > 0) ? 
                                               g_driver_mgr.drivers[0] : NULL;
            }

            return UWB_HAL_OK;
        }
    }

    return UWB_HAL_ERROR_CHIP_NOT_FOUND;
}

/**
 * @brief 获取默认驱动
 */
uwb_chip_driver_t* uwb_driver_get_default(void)
{
    if (!g_driver_mgr.initialized) {
        return NULL;
    }
    return g_driver_mgr.default_driver;
}

/**
 * @brief 设置默认驱动
 */
int uwb_driver_set_default(uwb_chip_driver_t *driver)
{
    if (!g_driver_mgr.initialized) {
        return UWB_HAL_ERROR_NOT_INITIALIZED;
    }

    if (driver == NULL) {
        return UWB_HAL_ERROR_INVALID_PARAM;
    }

    /* 确认驱动已注册 */
    bool found = false;
    for (uint8_t i = 0; i < g_driver_mgr.driver_count; i++) {
        if (g_driver_mgr.drivers[i] == driver) {
            found = true;
            break;
        }
    }

    if (!found) {
        return UWB_HAL_ERROR_CHIP_NOT_FOUND;
    }

    g_driver_mgr.default_driver = driver;
    return UWB_HAL_OK;
}

/******************************************************************************
 * 测距操作 API
 ******************************************************************************/

/**
 * @brief 开始 UWB 测距
 */
int uwb_start_ranging(uwb_chip_driver_t *driver)
{
    if (!g_driver_mgr.initialized) {
        return UWB_HAL_ERROR_NOT_INITIALIZED;
    }

    /* 使用默认驱动 */
    if (driver == NULL) {
        driver = g_driver_mgr.default_driver;
    }

    if (driver == NULL) {
        return UWB_HAL_ERROR_INVALID_CHIP;
    }

    if (!driver->initialized) {
        return UWB_HAL_ERROR_NOT_INITIALIZED;
    }

    /* 检查当前状态 */
    if (driver->state == UWB_CHIP_STATE_TX || driver->state == UWB_CHIP_STATE_RX) {
        return UWB_HAL_OK;  /* 已经在测距 */
    }

    if (driver->ops.start_ranging == NULL) {
        return UWB_HAL_ERROR_NOT_SUPPORTED;
    }

    g_driver_mgr.total_ranging_count++;

    int ret = driver->ops.start_ranging(driver);
    if (ret != UWB_HAL_OK) {
        g_driver_mgr.failed_ranging_count++;
        driver->state = UWB_CHIP_STATE_ERROR;
    } else {
        driver->state = (driver->config.role == CCC_UWB_ROLE_INITIATOR) ? 
                        UWB_CHIP_STATE_TX : UWB_CHIP_STATE_RX;
    }

    return ret;
}

/**
 * @brief 停止 UWB 测距
 */
int uwb_stop_ranging(uwb_chip_driver_t *driver)
{
    if (!g_driver_mgr.initialized) {
        return UWB_HAL_ERROR_NOT_INITIALIZED;
    }

    /* 使用默认驱动 */
    if (driver == NULL) {
        driver = g_driver_mgr.default_driver;
    }

    if (driver == NULL) {
        return UWB_HAL_ERROR_INVALID_CHIP;
    }

    if (!driver->initialized) {
        return UWB_HAL_ERROR_NOT_INITIALIZED;
    }

    if (driver->ops.stop_ranging == NULL) {
        return UWB_HAL_ERROR_NOT_SUPPORTED;
    }

    int ret = driver->ops.stop_ranging(driver);
    if (ret == UWB_HAL_OK) {
        driver->state = UWB_CHIP_STATE_READY;
    }

    return ret;
}

/**
 * @brief 获取测距结果
 */
int uwb_get_measurement(uwb_chip_driver_t *driver, uwb_hal_measurement_t *result)
{
    if (!g_driver_mgr.initialized) {
        return UWB_HAL_ERROR_NOT_INITIALIZED;
    }

    if (result == NULL) {
        return UWB_HAL_ERROR_INVALID_PARAM;
    }

    /* 使用默认驱动 */
    if (driver == NULL) {
        driver = g_driver_mgr.default_driver;
    }

    if (driver == NULL) {
        return UWB_HAL_ERROR_INVALID_CHIP;
    }

    if (!driver->initialized) {
        return UWB_HAL_ERROR_NOT_INITIALIZED;
    }

    if (driver->ops.get_measurement == NULL) {
        return UWB_HAL_ERROR_NOT_SUPPORTED;
    }

    memset(result, 0, sizeof(uwb_hal_measurement_t));
    
    int ret = driver->ops.get_measurement(driver, result);
    
    /* 计算质量分数 */
    if (ret == UWB_HAL_OK && result->is_valid) {
        /* 基于 RSSI 和精度计算质量 */
        int quality = 100;
        
        /* RSSI 影响 (>= -60dBm 为最佳) */
        if (result->rssi_dbm < -80) {
            quality -= 40;
        } else if (result->rssi_dbm < -70) {
            quality -= 20;
        } else if (result->rssi_dbm < -60) {
            quality -= 10;
        }
        
        /* 精度影响 */
        if (result->accuracy_m > 0.5f) {
            quality -= 30;
        } else if (result->accuracy_m > 0.3f) {
            quality -= 15;
        } else if (result->accuracy_m > 0.1f) {
            quality -= 5;
        }
        
        if (quality < 0) quality = 0;
        result->quality = (uint8_t)quality;
    }

    return ret;
}

/**
 * @brief 执行单次测距
 */
int uwb_do_single_ranging(uwb_chip_driver_t *driver, uwb_hal_measurement_t *result, uint32_t timeout_ms)
{
    int ret;

    if (!g_driver_mgr.initialized) {
        return UWB_HAL_ERROR_NOT_INITIALIZED;
    }

    if (result == NULL) {
        return UWB_HAL_ERROR_INVALID_PARAM;
    }

    /* 使用默认驱动 */
    if (driver == NULL) {
        driver = g_driver_mgr.default_driver;
    }

    if (driver == NULL) {
        return UWB_HAL_ERROR_INVALID_CHIP;
    }

    if (!driver->initialized) {
        return UWB_HAL_ERROR_NOT_INITIALIZED;
    }

    if (driver->ops.do_single_ranging != NULL) {
        /* 使用驱动的单次测距函数 */
        return driver->ops.do_single_ranging(driver, result);
    }

    /* 使用 start + get + stop 模式 */
    ret = uwb_start_ranging(driver);
    if (ret != UWB_HAL_OK) {
        return ret;
    }

    /* 等待测距完成 */
    uint32_t wait_time = 0;
    uint32_t check_interval = 10;  /* 10ms 检查一次 */
    
    while (wait_time < timeout_ms) {
        ret = driver->ops.get_measurement(driver, result);
        if (ret == UWB_HAL_OK && result->is_valid) {
            break;
        }
        
        /* 小延迟等待 */
        /* HAL_Delay(check_interval); 需要平台特定实现 */
        wait_time += check_interval;
    }

    uwb_stop_ranging(driver);

    if (wait_time >= timeout_ms) {
        return UWB_HAL_ERROR_TIMEOUT;
    }

    return UWB_HAL_OK;
}

/******************************************************************************
 * 配置操作 API
 ******************************************************************************/

/**
 * @brief 配置 STS 密钥
 */
int uwb_configure_sts(uwb_chip_driver_t *driver, const uwb_sts_config_t *sts_config)
{
    if (!g_driver_mgr.initialized) {
        return UWB_HAL_ERROR_NOT_INITIALIZED;
    }

    if (sts_config == NULL) {
        return UWB_HAL_ERROR_INVALID_PARAM;
    }

    /* 使用默认驱动 */
    if (driver == NULL) {
        driver = g_driver_mgr.default_driver;
    }

    if (driver == NULL) {
        return UWB_HAL_ERROR_INVALID_CHIP;
    }

    if (!driver->initialized) {
        return UWB_HAL_ERROR_NOT_INITIALIZED;
    }

    if (driver->ops.configure_sts == NULL) {
        return UWB_HAL_ERROR_NOT_SUPPORTED;
    }

    /* 验证 STS 配置 */
    if (sts_config->enable_sts) {
        /* STS 密钥不能为全零 */
        bool key_all_zero = true;
        for (int i = 0; i < 16; i++) {
            if (sts_config->sts_key[i] != 0) {
                key_all_zero = false;
                break;
            }
        }
        if (key_all_zero) {
            return UWB_HAL_ERROR_STS_CONFIG;
        }

        /* STS IV 不能为全零 */
        bool iv_all_zero = true;
        for (int i = 0; i < 8; i++) {
            if (sts_config->sts_iv[i] != 0) {
                iv_all_zero = false;
                break;
            }
        }
        if (iv_all_zero) {
            return UWB_HAL_ERROR_STS_CONFIG;
        }
    }

    int ret = driver->ops.configure_sts(driver, sts_config);
    if (ret == UWB_HAL_OK) {
        /* 保存到驱动配置 */
        memcpy(&driver->config.sts, sts_config, sizeof(uwb_sts_config_t));
    }

    return ret;
}

/**
 * @brief 配置 UWB 参数
 */
int uwb_configure(uwb_chip_driver_t *driver, const uwb_hal_config_t *config)
{
    int ret;

    if (!g_driver_mgr.initialized) {
        return UWB_HAL_ERROR_NOT_INITIALIZED;
    }

    if (config == NULL) {
        return UWB_HAL_ERROR_INVALID_PARAM;
    }

    /* 验证配置 */
    ret = uwb_driver_validate_config(config);
    if (ret != UWB_HAL_OK) {
        return ret;
    }

    /* 使用默认驱动 */
    if (driver == NULL) {
        driver = g_driver_mgr.default_driver;
    }

    if (driver == NULL) {
        return UWB_HAL_ERROR_INVALID_CHIP;
    }

    if (!driver->initialized) {
        return UWB_HAL_ERROR_NOT_INITIALIZED;
    }

    if (driver->ops.configure == NULL) {
        return UWB_HAL_ERROR_NOT_SUPPORTED;
    }

    ret = driver->ops.configure(driver, config);
    if (ret == UWB_HAL_OK) {
        memcpy(&driver->config, config, sizeof(uwb_hal_config_t));
    }

    return ret;
}

/******************************************************************************
 * 芯片操作 API
 ******************************************************************************/

/**
 * @brief 初始化特定芯片驱动
 */
int uwb_chip_init(uwb_chip_driver_t *driver, void *spi_handle, void *gpio_handle)
{
    int ret;

    if (!g_driver_mgr.initialized) {
        return UWB_HAL_ERROR_NOT_INITIALIZED;
    }

    if (driver == NULL) {
        return UWB_HAL_ERROR_INVALID_PARAM;
    }

    if (driver->ops.init == NULL) {
        return UWB_HAL_ERROR_NOT_SUPPORTED;
    }

    ret = driver->ops.init(driver, spi_handle, gpio_handle);
    if (ret == UWB_HAL_OK) {
        driver->initialized = true;
        driver->state = UWB_CHIP_STATE_IDLE;
        driver->spi_handle = spi_handle;
        driver->gpio_handle = gpio_handle;
    }

    return ret;
}

/**
 * @brief 反初始化特定芯片驱动
 */
int uwb_chip_deinit(uwb_chip_driver_t *driver)
{
    int ret;

    if (!g_driver_mgr.initialized) {
        return UWB_HAL_ERROR_NOT_INITIALIZED;
    }

    if (driver == NULL) {
        driver = g_driver_mgr.default_driver;
    }

    if (driver == NULL) {
        return UWB_HAL_ERROR_INVALID_CHIP;
    }

    if (!driver->initialized) {
        return UWB_HAL_OK;  /* 已经未初始化 */
    }

    if (driver->ops.deinit == NULL) {
        return UWB_HAL_ERROR_NOT_SUPPORTED;
    }

    /* 先停止测距 */
    if (driver->state == UWB_CHIP_STATE_TX || driver->state == UWB_CHIP_STATE_RX) {
        uwb_stop_ranging(driver);
    }

    ret = driver->ops.deinit(driver);
    if (ret == UWB_HAL_OK) {
        driver->initialized = false;
        driver->state = UWB_CHIP_STATE_UNINIT;
    }

    return ret;
}

/**
 * @brief 复位 UWB 芯片
 */
int uwb_chip_reset(uwb_chip_driver_t *driver)
{
    if (!g_driver_mgr.initialized) {
        return UWB_HAL_ERROR_NOT_INITIALIZED;
    }

    if (driver == NULL) {
        driver = g_driver_mgr.default_driver;
    }

    if (driver == NULL) {
        return UWB_HAL_ERROR_INVALID_CHIP;
    }

    if (driver->ops.reset == NULL) {
        return UWB_HAL_ERROR_NOT_SUPPORTED;
    }

    return driver->ops.reset(driver);
}

/**
 * @brief 获取芯片信息
 */
int uwb_chip_get_info(uwb_chip_driver_t *driver, uwb_chip_info_t *info)
{
    if (!g_driver_mgr.initialized) {
        return UWB_HAL_ERROR_NOT_INITIALIZED;
    }

    if (info == NULL) {
        return UWB_HAL_ERROR_INVALID_PARAM;
    }

    if (driver == NULL) {
        driver = g_driver_mgr.default_driver;
    }

    if (driver == NULL) {
        return UWB_HAL_ERROR_INVALID_CHIP;
    }

    if (driver->ops.get_info == NULL) {
        return UWB_HAL_ERROR_NOT_SUPPORTED;
    }

    return driver->ops.get_info(driver, info);
}

/******************************************************************************
 * 中断处理 API
 ******************************************************************************/

/**
 * @brief 注册中断回调
 */
int uwb_register_irq_callback(uwb_chip_driver_t *driver, uwb_irq_callback_t callback, void *user_data)
{
    if (!g_driver_mgr.initialized) {
        return UWB_HAL_ERROR_NOT_INITIALIZED;
    }

    if (driver == NULL) {
        driver = g_driver_mgr.default_driver;
    }

    if (driver == NULL) {
        return UWB_HAL_ERROR_INVALID_CHIP;
    }

    if (driver->ops.register_irq_callback == NULL) {
        return UWB_HAL_ERROR_NOT_SUPPORTED;
    }

    driver->irq_callback = callback;
    driver->irq_user_data = user_data;

    return driver->ops.register_irq_callback(driver, callback, user_data);
}

/**
 * @brief 启用/禁用中断
 */
int uwb_enable_irq(uwb_chip_driver_t *driver, bool enable)
{
    if (!g_driver_mgr.initialized) {
        return UWB_HAL_ERROR_NOT_INITIALIZED;
    }

    if (driver == NULL) {
        driver = g_driver_mgr.default_driver;
    }

    if (driver == NULL) {
        return UWB_HAL_ERROR_INVALID_CHIP;
    }

    if (driver->ops.enable_irq == NULL) {
        return UWB_HAL_ERROR_NOT_SUPPORTED;
    }

    return driver->ops.enable_irq(driver, enable);
}

/**
 * @brief 中断处理函数
 */
int uwb_irq_handler(uwb_chip_driver_t *driver)
{
    if (!g_driver_mgr.initialized) {
        return UWB_HAL_ERROR_NOT_INITIALIZED;
    }

    if (driver == NULL) {
        driver = g_driver_mgr.default_driver;
    }

    if (driver == NULL) {
        return UWB_HAL_ERROR_INVALID_CHIP;
    }

    if (driver->ops.irq_handler == NULL) {
        return UWB_HAL_ERROR_NOT_SUPPORTED;
    }

    return driver->ops.irq_handler(driver);
}

/******************************************************************************
 * 辅助函数
 ******************************************************************************/

/**
 * @brief 验证配置参数
 */
static int uwb_driver_validate_config(const uwb_hal_config_t *config)
{
    /* 验证信道 */
    if (config->phy.channel != 5 && config->phy.channel != 6 && 
        config->phy.channel != 8 && config->phy.channel != 9) {
        return UWB_HAL_ERROR_INVALID_PARAM;
    }

    /* 验证前导码索引 */
    if (config->phy.preamble_code < 1 || config->phy.preamble_code > 24) {
        return UWB_HAL_ERROR_INVALID_PARAM;
    }

    /* 验证测距间隔 */
    if (config->ranging_interval_ms < CCC_UWB_MIN_RANGING_INTERVAL_MS) {
        return UWB_HAL_ERROR_INVALID_PARAM;
    }

    return UWB_HAL_OK;
}

/**
 * @brief 等待芯片状态
 */
static int uwb_driver_wait_for_state(uwb_chip_driver_t *driver, uwb_chip_state_t state, uint32_t timeout_ms)
{
    (void)driver;
    (void)state;
    (void)timeout_ms;
    /* 需要平台特定实现 - 使用延时或事件等待 */
    return UWB_HAL_OK;
}

/**
 * @brief 计算距离
 */
static float uwb_calculate_distance(uint32_t timestamp_tx, uint32_t timestamp_rx, uint32_t resp_delay)
{
    /* 光速 (m/s) */
    const float SPEED_OF_LIGHT = 299702547.0f;
    /* UWB 时钟频率 (Hz) - DW3000 使甈 499.2MHz 基准 */
    const float UWB_CLOCK_FREQ = 499.2e6f;
    const float CLOCK_PERIOD = 1.0f / (128.0f * UWB_CLOCK_FREQ);  /* ~15.65ps */

    /* 计算飞行时间 */
    int64_t round_trip_time;
    
    if (timestamp_rx >= timestamp_tx) {
        round_trip_time = (int64_t)(timestamp_rx - timestamp_tx);
    } else {
        /* 处理溢出 */
        round_trip_time = (int64_t)(timestamp_rx + 0xFFFFFFFFFFULL - timestamp_tx);
    }

    /* 减去响应延迟 */
    int64_t tof = round_trip_time - (int64_t)resp_delay;
    
    /* 转换为秒 */
    float tof_sec = (float)tof * CLOCK_PERIOD;
    
    /* 计算距离 (双程) */
    float distance = tof_sec * SPEED_OF_LIGHT / 2.0f;

    return distance;
}

/**
 * @brief 获取错误码字符串
 */
const char* uwb_error_string(int error)
{
    switch (error) {
        case UWB_HAL_OK:
            return "OK";
        case UWB_HAL_ERROR_INIT_FAILED:
            return "Init failed";
        case UWB_HAL_ERROR_NOT_INITIALIZED:
            return "Not initialized";
        case UWB_HAL_ERROR_ALREADY_INITIALIZED:
            return "Already initialized";
        case UWB_HAL_ERROR_INVALID_CHIP:
            return "Invalid chip";
        case UWB_HAL_ERROR_SPI_FAILURE:
            return "SPI failure";
        case UWB_HAL_ERROR_GPIO_FAILURE:
            return "GPIO failure";
        case UWB_HAL_ERROR_TIMEOUT:
            return "Timeout";
        case UWB_HAL_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        case UWB_HAL_ERROR_RANGING_FAILED:
            return "Ranging failed";
        case UWB_HAL_ERROR_STS_CONFIG:
            return "STS configuration error";
        case UWB_HAL_ERROR_INTERRUPT:
            return "Interrupt error";
        case UWB_HAL_ERROR_NOT_SUPPORTED:
            return "Not supported";
        case UWB_HAL_ERROR_CHIP_NOT_FOUND:
            return "Chip not found";
        default:
            return "Unknown error";
    }
}

/**
 * @brief 获取驱动管理器统计信息
 */
void uwb_driver_get_stats(uint32_t *total, uint32_t *failed)
{
    if (total != NULL) {
        *total = g_driver_mgr.total_ranging_count;
    }
    if (failed != NULL) {
        *failed = g_driver_mgr.failed_ranging_count;
    }
}

/**
 * @brief 清除统计信息
 */
void uwb_driver_clear_stats(void)
{
    g_driver_mgr.total_ranging_count = 0;
    g_driver_mgr.failed_ranging_count = 0;
}
