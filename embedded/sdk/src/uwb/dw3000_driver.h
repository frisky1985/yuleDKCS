/******************************************************************************
 * @file    dw3000_driver.h
 * @brief   Qorvo DW3000 UWB 芯片驱动头文件
 * @author  YuleTech
 * @version 2.0.0
 * @date    2026-05-09
 ******************************************************************************/

#ifndef DW3000_DRIVER_H
#define DW3000_DRIVER_H

#include <stdint.h>
#include <stddef.h>
#include "dkcs_error.h"
#include "dkcs_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * 定义和宏
 ******************************************************************************/

/* DW3000 APDU 最大长度 */
#define DW3000_APDU_MAX_LEN     256

/* UWB 频率通道 */
#define UWB_CHANNEL_5           5
#define UWB_CHANNEL_9           9

/* UWB 脉冲重复频率 (PRF) */
#define UWB_PRF_16M             0
#define UWB_PRF_64M             1

/* UWB 数据速率 */
#define UWB_DATARATE_6M8        0   /* 6.8 Mbps */
#define UWB_DATARATE_850K       1   /* 850 Kbps */
#define UWB_DATARATE_110K       2   /* 110 Kbps */

/* UWB 前导码长度 */
#define UWB_PLEN_64             0x04
#define UWB_PLEN_128            0x14
#define UWB_PLEN_256            0x24
#define UWB_PLEN_512            0x34
#define UWB_PLEN_1024           0x44
#define UWB_PLEN_1536           0x54
#define UWB_PLEN_2048           0x64
#define UWB_PLEN_4096           0x74

/* UWB PAC 大小 */
#define UWB_PAC8                0
#define UWB_PAC16               1
#define UWB_PAC32               2
#define UWB_PAC64               3

/* 发送模式 */
#define DWT_START_TX_IMMEDIATE  0
#define DWT_START_TX_DELAYED    1

/* 接收模式 */
#define DWT_RX_NORMAL           0
#define DWT_RX_ON_DLY           1

/******************************************************************************
 * 数据类型
 ******************************************************************************/

/* UWB 工作模式 */
typedef enum {
    UWB_MODE_TWR_INITIATOR = 0,     /* 双向测距 - 启动方 */
    UWB_MODE_TWR_RESPONDER,          /* 双向测距 - 响应方 */
    UWB_MODE_TDOA_TAG,               /* TDOA - 标签 */
    UWB_MODE_TDOA_ANCHOR,            /* TDOA - 基站 */
    UWB_MODE_RANGING_MASTER,         /* 测距主机 */
    UWB_MODE_RANGING_SLAVE           /* 测距从机 */
} uwb_mode_t;

/* DW3000 配置 */
typedef struct {
    uint8_t  channel;                /* 通道: 5 或 9 */
    uint8_t  prf;                    /* PRF: 16M 或 64M */
    uint8_t  data_rate;              /* 数据速率 */
    uint8_t  preamble_len;           /* 前导码长度 */
    uint8_t  pac_size;               /* PAC 大小 */
    uint16_t rx_timeout;             /* 接收超时 (ms) */
    uint32_t tx_power;               /* 发射功率 */
} dw3000_config_t;

/* 接收信息 */
typedef struct {
    uint64_t rx_stamp;               /* 接收时间戳 */
    uint16_t rx_len;                 /* 接收长度 */
    uint16_t rx_pacc;                /* 前导码累加器计数 */
    uint16_t fp_index;               /* 第一路径索引 */
    int16_t  fp_ampl1;               /* 第一路径振幅 1 */
    int16_t  fp_ampl2;               /* 第一路径振幅 2 */
    int16_t  fp_ampl3;               /* 第一路径振幅 3 */
    int16_t  cfo;                    /* 载波频偏 */
} dw3000_rx_info_t;

/* 测距结果 */
typedef struct {
    float distance;                  /* 距离 (米) */
    float rssi;                      /* 信号强度 (dBm) */
    float precision;                 /* 精度 (厘米) */
    uint8_t quality;                 /* 质量因子 (0-100) */
    uint64_t timestamp;              /* 测量时间戳 */
} uwb_range_result_t;

/* DW3000 上下文 */
typedef struct {
    dw3000_config_t config;          /* 当前配置 */
    uwb_mode_t mode;                 /* 当前模式 */
    
#if defined(STM32_PLATFORM)
    SPI_HandleTypeDef spi_handle;    /* SPI 句柄 */
#elif defined(NXP_PLATFORM)
    dspi_master_handle_t spi_handle; /* DSPI 句柄 */
#endif
    
    void (*rx_callback)(const uint8_t *data, size_t len, const dw3000_rx_info_t *info);
    void (*tx_callback)(void);
    void (*range_callback)(const uwb_range_result_t *result);
} dw3000_context_t;

/* UWB 会话 */
typedef struct {
    uint16_t session_id;             /* 会话ID */
    uint64_t local_addr;             /* 本地地址 */
    uint64_t peer_addr;              /* 对端地址 */
    uwb_mode_t mode;                 /* 工作模式 */
    uint8_t is_active;               /* 是否活动 */
} uwb_session_t;

/* 回调函数类型 */
typedef void (*uwb_rx_callback_t)(const uint8_t *data, size_t len, const dw3000_rx_info_t *info);
typedef void (*uwb_tx_callback_t)(void);
typedef void (*uwb_range_callback_t)(const uwb_range_result_t *result);

/******************************************************************************
 * API 声明
 ******************************************************************************/

/* 初始化和配置 */
error_t dw3000_init(const dw3000_config_t *config);
void dw3000_deinit(void);
error_t dw3000_configure(const dw3000_config_t *config);
error_t dw3000_check_device(void);
uint32_t dw3000_get_version(void);

/* 设备身份 */
error_t dw3000_set_eui(uint64_t eui);
error_t dw3000_set_address(uint16_t pan_id, uint16_t short_addr);

/* 发送和接收 */
error_t dw3000_tx(const uint8_t *data, size_t len, uint8_t tx_mode);
error_t dw3000_rx_enable(uint8_t rx_mode);
error_t dw3000_rx_disable(void);
error_t dw3000_rx_read(uint8_t *data, size_t *len, dw3000_rx_info_t *info);

/* 时间戳 */
uint64_t dw3000_read_tx_timestamp(void);
uint64_t dw3000_read_rx_timestamp(void);

/* 信号质量 */
float dw3000_get_rssi(void);
float dw3000_get_precision(void);

/* 测距功能 */
error_t dw3000_measure_distance(float *distance, uint32_t timeout_ms);
error_t dw3000_send_poll(uint64_t *tx_timestamp);
error_t dw3000_wait_response(uint32_t timeout_ms, uint64_t *rx_timestamp);
error_t dw3000_send_final(uint64_t *tx_timestamp);
float dw3000_calculate_distance(
    uint64_t poll_tx_ts,
    uint64_t resp_rx_ts,
    uint64_t final_tx_ts,
    uint64_t resp_tx_ts,
    uint64_t final_rx_ts
);

/* 会话管理 */
error_t uwb_session_init(uwb_session_t *session, uwb_mode_t mode);
error_t uwb_session_start(uwb_session_t *session);
error_t uwb_session_stop(uwb_session_t *session);

/* 模式设置 */
error_t dw3000_set_mode(uwb_mode_t mode);

/* 低功耗管理 */
error_t dw3000_enter_sleep(void);
error_t dw3000_wakeup(void);

/* 回调设置 */
void dw3000_set_rx_callback(uwb_rx_callback_t callback);
void dw3000_set_tx_callback(uwb_tx_callback_t callback);
void dw3000_set_range_callback(uwb_range_callback_t callback);

/******************************************************************************
 * 平台特定配置
 ******************************************************************************/

#if defined(STM32_PLATFORM)
    #define UWB_SPI_INSTANCE    SPI1
    #define UWB_CS_PORT         GPIOA
    #define UWB_CS_PIN          GPIO_PIN_4
    #define UWB_RST_PORT        GPIOA
    #define UWB_RST_PIN         GPIO_PIN_3
    #define UWB_IRQ_PORT        GPIOA
    #define UWB_IRQ_PIN         GPIO_PIN_2
    #define UWB_WAKEUP_PORT     GPIOA
    #define UWB_WAKEUP_PIN      GPIO_PIN_1
#endif

#ifdef __cplusplus
}
#endif

#endif /* DW3000_DRIVER_H */
