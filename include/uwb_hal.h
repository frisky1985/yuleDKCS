/******************************************************************************
 * @file    uwb_hal.h
 * @brief   UWB 芯片驱动硬件抽象层头文件
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-09
 *
 * @note    支持多种 UWB 芯片 (Qorvo DW3000, NXP NCJ29D5 等)
 *          符合 CCC Digital Key R3 UWB 规范
 ******************************************************************************/

#ifndef UWB_HAL_H
#define UWB_HAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "dkcs.h"
#include "ccc_uwb.h"

/******************************************************************************
 * UWB HAL 错误码
 ******************************************************************************/
typedef enum {
    UWB_HAL_OK = 0,
    UWB_HAL_ERROR_INIT_FAILED = -300,
    UWB_HAL_ERROR_NOT_INITIALIZED = -301,
    UWB_HAL_ERROR_ALREADY_INITIALIZED = -302,
    UWB_HAL_ERROR_INVALID_CHIP = -303,
    UWB_HAL_ERROR_SPI_FAILURE = -304,
    UWB_HAL_ERROR_GPIO_FAILURE = -305,
    UWB_HAL_ERROR_TIMEOUT = -306,
    UWB_HAL_ERROR_INVALID_PARAM = -307,
    UWB_HAL_ERROR_RANGING_FAILED = -308,
    UWB_HAL_ERROR_STS_CONFIG = -309,
    UWB_HAL_ERROR_INTERRUPT = -310,
    UWB_HAL_ERROR_NOT_SUPPORTED = -311,
    UWB_HAL_ERROR_CHIP_NOT_FOUND = -312,
} uwb_hal_error_t;

/******************************************************************************
 * UWB 芯片类型定义
 ******************************************************************************/
typedef enum {
    UWB_CHIP_UNKNOWN = 0,
    UWB_CHIP_QORVO_DW3000 = 1,      /**< Qorvo DW3000 */
    UWB_CHIP_QORVO_DW3720 = 2,      /**< Qorvo DW3720 */
    UWB_CHIP_NXP_NCJ29D5 = 3,       /**< NXP NCJ29D5 */
    UWB_CHIP_NXP_SR040 = 4,         /**< NXP SR040 */
    UWB_CHIP_MAX
} uwb_chip_type_t;

/******************************************************************************
 * UWB 芯片状态
 ******************************************************************************/
typedef enum {
    UWB_CHIP_STATE_UNINIT = 0,
    UWB_CHIP_STATE_IDLE = 1,
    UWB_CHIP_STATE_READY = 2,
    UWB_CHIP_STATE_TX = 3,
    UWB_CHIP_STATE_RX = 4,
    UWB_CHIP_STATE_SLEEP = 5,
    UWB_CHIP_STATE_ERROR = 6,
} uwb_chip_state_t;

/******************************************************************************
 * UWB 测距模式
 ******************************************************************************/
typedef enum {
    UWB_MODE_TWO_WAY_RANGING = 0,   /**< 双向测距 (TWR) */
    UWB_MODE_DS_TWR = 1,            /**< 双边双向测距 */
    UWB_MODE_SS_TWR = 2,            /**< 单边双向测距 */
    UWB_MODE_PDOA = 3,              /**< 相位差测距 (AoA) */
} uwb_ranging_mode_t;

/******************************************************************************
 * UWB 数据速率
 ******************************************************************************/
typedef enum {
    UWB_DATA_RATE_850K = 0,
    UWB_DATA_RATE_6M8 = 1,
} uwb_data_rate_t;

/******************************************************************************
 * UWB PRF (Pulse Repetition Frequency)
 ******************************************************************************/
typedef enum {
    UWB_PRF_16M = 0,
    UWB_PRF_64M = 1,
} uwb_prf_t;

/******************************************************************************
 * UWB Preamble Length
 ******************************************************************************/
typedef enum {
    UWB_PREAMBLE_64 = 0,
    UWB_PREAMBLE_128 = 1,
    UWB_PREAMBLE_256 = 2,
    UWB_PREAMBLE_512 = 3,
    UWB_PREAMBLE_1024 = 4,
    UWB_PREAMBLE_2048 = 5,
    UWB_PREAMBLE_4096 = 6,
} uwb_preamble_length_t;

/******************************************************************************
 * UWB 物理层配置
 ******************************************************************************/
typedef struct {
    uint8_t                 channel;            /**< UWB 信道 (5, 6, 8, 9) */
    uwb_data_rate_t         data_rate;          /**< 数据速率 */
    uwb_prf_t               prf;                /**< 脉冲重复频率 */
    uwb_preamble_length_t   preamble_length;    /**< 前导码长度 */
    uint8_t                 preamble_code;      /**< 前导码索引 (1-24) */
    uint8_t                 sfd_type;           /**< SFD 类型 */
    bool                    phr_ext_mode;       /**< PHR 扩展模式 */
} uwb_phy_config_t;

/******************************************************************************
 * UWB STS 配置 (用于 HAL 层)
 ******************************************************************************/
typedef struct {
    uint8_t  sts_key[16];           /**< 128-bit STS 密钥 */
    uint8_t  sts_iv[8];             /**< 64-bit STS IV */
    uint8_t  sts_index;             /**< STS 索引 (0-3) */
    bool     enable_sts;            /**< 启用 STS */
    bool     dynamic_sts;           /**< 动态 STS 模式 */
} uwb_sts_config_t;

/******************************************************************************
 * UWB 测距配置 (HAL 层)
 ******************************************************************************/
typedef struct {
    ccc_uwb_role_t      role;               /**< 测距角色 */
    uwb_ranging_mode_t  mode;               /**< 测距模式 */
    uwb_phy_config_t    phy;                /**< 物理层配置 */
    uwb_sts_config_t    sts;                /**< STS 配置 */
    uint32_t            ranging_interval_ms; /**< 测距间隔 */
    uint16_t            max_attempts;       /**< 最大尝试次数 */
    uint32_t            timeout_ms;         /**< 超时时间 */
    bool                enable_aoa;         /**< 启用 AoA */
} uwb_hal_config_t;

/******************************************************************************
 * UWB 测距结果 (HAL 层)
 ******************************************************************************/
typedef struct {
    uint64_t    timestamp;          /**< 时间戳 */
    float       distance_m;         /**< 距离 (米) */
    float       accuracy_m;         /**< 精度估计 (米) */
    int8_t      rssi_dbm;           /**< 信号强度 */
    float       aoa_azimuth;        /**< 方位角 (度) */
    float       aoa_elevation;      /**< 仰角 (度) */
    bool        aoa_valid;          /**< AoA 数据有效 */
    uint8_t     quality;            /**< 测量质量 (0-100) */
    bool        is_valid;           /**< 测量是否有效 */
    uint32_t    raw_timestamp_tx;   /**< 原始发送时间戳 */
    uint32_t    raw_timestamp_rx;   /**< 原始接收时间戳 */
} uwb_hal_measurement_t;

/******************************************************************************
 * UWB 芯片信息
 ******************************************************************************/
typedef struct {
    uwb_chip_type_t chip_type;
    uint32_t        chip_id;
    uint32_t        lot_id;
    uint8_t         dev_id;
    char            version_str[32];
    bool            supports_sts;
    bool            supports_aoa;
    bool            supports_pdoa;
} uwb_chip_info_t;

/******************************************************************************
 * UWB 中断回调类型
 ******************************************************************************/
typedef void (*uwb_irq_callback_t)(void *user_data);

/******************************************************************************
 * UWB 芯片驱动接口 (VTable)
 ******************************************************************************/
struct uwb_chip_driver;

typedef struct {
    /* 生命周期管理 */
    int (*init)(struct uwb_chip_driver *driver, void *spi_handle, void *gpio_handle);
    int (*deinit)(struct uwb_chip_driver *driver);
    int (*reset)(struct uwb_chip_driver *driver);
    
    /* 芯片信息 */
    int (*get_info)(struct uwb_chip_driver *driver, uwb_chip_info_t *info);
    
    /* 配置 */
    int (*configure)(struct uwb_chip_driver *driver, const uwb_hal_config_t *config);
    int (*configure_sts)(struct uwb_chip_driver *driver, const uwb_sts_config_t *sts_config);
    
    /* 测距控制 */
    int (*start_ranging)(struct uwb_chip_driver *driver);
    int (*stop_ranging)(struct uwb_chip_driver *driver);
    int (*do_single_ranging)(struct uwb_chip_driver *driver, uwb_hal_measurement_t *result);
    
    /* 数据获取 */
    int (*get_measurement)(struct uwb_chip_driver *driver, uwb_hal_measurement_t *result);
    int (*clear_measurement)(struct uwb_chip_driver *driver);
    
    /* 中断处理 */
    int (*enable_irq)(struct uwb_chip_driver *driver, bool enable);
    int (*register_irq_callback)(struct uwb_chip_driver *driver, uwb_irq_callback_t callback, void *user_data);
    int (*irq_handler)(struct uwb_chip_driver *driver);
    
    /* 低功耗 */
    int (*sleep)(struct uwb_chip_driver *driver);
    int (*wakeup)(struct uwb_chip_driver *driver);
    
    /* 寄存器访问 (调试) */
    int (*read_reg)(struct uwb_chip_driver *driver, uint32_t addr, uint8_t *data, size_t len);
    int (*write_reg)(struct uwb_chip_driver *driver, uint32_t addr, const uint8_t *data, size_t len);
} uwb_driver_ops_t;

/******************************************************************************
 * UWB 芯片驱动上下文
 ******************************************************************************/
typedef struct uwb_chip_driver {
    uwb_chip_type_t     chip_type;
    uwb_chip_state_t    state;
    uwb_hal_config_t    config;
    uwb_driver_ops_t    ops;
    void               *priv_data;      /**< 驱动私有数据 */
    void               *spi_handle;     /**< SPI 句柄 */
    void               *gpio_handle;    /**< GPIO 句柄 */
    uwb_irq_callback_t  irq_callback;
    void               *irq_user_data;
    bool                initialized;
} uwb_chip_driver_t;

/******************************************************************************
 * UWB HAL API 函数声明
 ******************************************************************************/

/**
 * @brief 初始化 UWB HAL 层
 */
int uwb_hal_init(void);

/**
 * @brief 反初始化 UWB HAL 层
 */
int uwb_hal_deinit(void);

/**
 * @brief 注册芯片驱动
 */
int uwb_hal_register_chip(uwb_chip_driver_t *driver);

/**
 * @brief 注销芯片驱动
 */
int uwb_hal_unregister_chip(uwb_chip_driver_t *driver);

/**
 * @brief 获取默认芯片驱动
 */
uwb_chip_driver_t* uwb_hal_get_default_driver(void);

/**
 * @brief 设置默认芯片驱动
 */
int uwb_hal_set_default_driver(uwb_chip_driver_t *driver);

/**
 * @brief 检测并初始化 UWB 芯片
 */
int uwb_hal_detect_chip(void);

/**
 * @brief 获取芯片错误码字符串
 */
const char* uwb_hal_error_string(int error);

#ifdef __cplusplus
}
#endif

#endif /* UWB_HAL_H */
