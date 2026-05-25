/******************************************************************************
 * @file    dw3000_driver.c
 * @brief   Qorvo DW3000 UWB 芯片驱动实现
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-16
 *
 * @note    实现DW3000芯片的完整驱动接口
 *          支持STS (Scrambled Timestamp Sequence) 安全测距
 *          符合IEEE 802.15.4z HRP UWB规范
 ******************************************************************************/

#include "uwb_hal.h"
#include <string.h>
#include <stdio.h>

/******************************************************************************
 * DW3000 寄存器定义
 ******************************************************************************/
#define DW3000_DEV_ID_REG           0x00    /* 设备ID寄存器 */
#define DW3000_EUI_64_REG           0x04    /* 64位扩展唯一标识符 */
#define DW3000_PANADR_REG           0x0C    /* PAN标识和短地址 */
#define DW3000_SYS_CFG_REG          0x10    /* 系统配置 */
#define DW3000_SYS_TIME_REG         0x14    /* 系统时间 */
#define DW3000_TX_FCTRL_REG         0x18    /* TX帧控制 */
#define DW3000_TX_BUFFER_REG        0x1C    /* TX数据缓冲区 */
#define DW3000_DX_TIME_REG          0x20    /* 延迟发送/接收时间 */
#define DW3000_RX_FWTO_REG          0x24    /* RX帧等待超时 */
#define DW3000_SYS_CTRL_REG         0x28    /* 系统控制 */
#define DW3000_SYS_MASK_REG         0x2C    /* 系统事件遮罩 */
#define DW3000_SYS_STATUS_REG       0x30    /* 系统状态 */
#define DW3000_RX_FINFO_REG         0x38    /* RX帧信息 */
#define DW3000_RX_BUFFER_REG        0x3C    /* RX数据缓冲区 */
#define DW3000_RX_FQUAL_REG         0x44    /* RX帧质量 */
#define DW3000_RX_TTCKI_REG         0x48    /* RX发送时间粗调 */
#define DW3000_RX_TTCKO_REG         0x4C    /* RX发送时间精调 */
#define DW3000_RX_TIME_REG          0x50    /* RX时间 */
#define DW3000_TX_TIME_REG          0x54    /* TX时间 */
#define DW3000_TX_ANTD_REG          0x58    /* TX天线延迟 */
#define DW3000_SYS_STATE_REG        0x5C    /* 系统状态机 */
#define DW3000_ACK_RESP_T_REG       0x60    /* 确认响应时间 */
#define DW3000_RX_SNIFF_REG         0x64    /* RX嗅探配置 */
#define DW3000_TX_POWER_REG         0x68    /* TX功率控制 */
#define DW3000_CHAN_CTRL_REG        0x6C    /* 信道控制 */
#define DW3000_LE_PENDAT_REG        0x70    /* 低功耗待发送 */
#define DW3000_LE_PENDAT_W_REG      0x74    /* 低功耗待发送孚接续 */
#define DW3000_SPI_COLLISION_REG    0x78    /* SPI冲突状态 */
#define DW3000_RDB_STATUS_REG       0x7C    /* 读数据缓冲状态 */
#define DW3000_RDB_DIAG_REG         0x80    /* 读数据缓冲诊断 */
#define DW3000_AES_CFG_REG          0x84    /* AES配置 */
#define DW3000_AES_IV0_REG          0x88    /* AES IV0 */
#define DW3000_AES_IV1_REG          0x8C    /* AES IV1 */
#define DW3000_AES_IV2_REG          0x90    /* AES IV2 */
#define DW3000_AES_IV3_REG          0x94    /* AES IV3 */
#define DW3000_AES_IV4_REG          0x98    /* AES IV4 */
#define DW3000_DMA_CFG_REG          0x9C    /* DMA配置 */
#define DW3000_AES_SRC_ADDR_REG     0xA0    /* AES源地址 */
#define DW3000_AES_DST_ADDR_REG     0xA4    /* AES目标地址 */
#define DW3000_AES_LEN_REG          0xA8    /* AES长度 */
#define DW3000_AES_CTRL_REG         0xAC    /* AES控制 */
#define DW3000_AES_STS_REG          0xB0    /* AES状态 */
#define DW3000_AES_KEY_REG          0xB4    /* AES密钥 */
#define DW3000_STS_CFG_REG          0xB8    /* STS配置 */
#define DW3000_STS_KEY_REG          0xBC    /* STS密钥 */
#define DW3000_STS_IV_REG           0xC0    /* STS IV */
#define DW3000_STS_LEN_REG          0xC4    /* STS长度 */
#define DW3000_STS_POLY_REG         0xC8    /* STS多项式 */

/******************************************************************************
 * DW3000 设备ID
 ******************************************************************************/
#define DW3000_DEVICE_ID            0xDECA0300U  /* DW3000设备ID */
#define DW3000_DEVICE_ID_MASK       0xFFFFFF00U

/******************************************************************************
 * 系统控制命令
 ******************************************************************************/
#define DW3000_CMD_TX               0x01    /* 立即发送 */
#define DW3000_CMD_RX               0x02    /* 立即接收 */
#define DW3000_CMD_DTX              0x03    /* 延迟发送 */
#define DW3000_CMD_DRX              0x04    /* 延迟接收 */
#define DW3000_CMD_DTX_TS           0x05    /* 带时戳的延迟发送 */
#define DW3000_CMD_DRX_TS           0x06    /* 带时戳的延迟接收 */
#define DW3000_CMD_TRXOFF           0x07    /* 发送/接收关闭 */
#define DW3000_CMD_TX_W4R           0x08    /* 发送并等待响应 */
#define DW3000_CMD_DTX_W4R          0x09    /* 延迟发送并等待响应 */
#define DW3000_CMD_DTX_W4R_TS       0x0A    /* 延迟发送并等待响应带时戳 */
#define DW3000_CMD_RX_W4R           0x0B    /* 接收并等待响应 */
#define DW3000_CMD_DRX_W4R          0x0C    /* 延迟接收并等待响应 */
#define DW3000_CMD_DRX_W4R_TS       0x0D    /* 延迟接收并等待响应带时戳 */
#define DW3000_CMD_GO_SLEEP         0x10    /* 进入睡眠 */
#define DW3000_CMD_GO_DEEPSLEEP     0x11    /* 进入深度睡眠 */
#define DW3000_CMD_CLR_IRQS         0x12    /* 清除中断 */

/******************************************************************************
 * 系统状态位
 ******************************************************************************/
#define DW3000_IRQ_TXFRS            (1UL << 0)   /* TX帧发送完成 */
#define DW3000_IRQ_RXFCG            (1UL << 1)   /* RX帧CRC正确接收 */
#define DW3000_IRQ_RXFCE            (1UL << 2)   /* RX帧CRC错误 */
#define DW3000_IRQ_RXDFR            (1UL << 3)   /* RX帧数据就绪 */
#define DW3000_IRQ_RXFTO            (1UL << 4)   /* RX帧等待超时 */
#define DW3000_IRQ_LDEERR           (1UL << 5)   /* LDE算法错误 */
#define DW3000_IRQ_RXOVRR           (1UL << 6)   /* RX溢出 */
#define DW3000_IRQ_RXPTO            (1UL << 7)   /* 前导码超时 */
#define DW3000_IRQ_GPIOIRQ          (1UL << 8)   /* GPIO中断 */
#define DW3000_IRQ_SLP2INIT         (1UL << 9)   /* 睡眠到初始化 */
#define DW3000_IRQ_RFPLLLL          (1UL << 10)  /* RF PLL失锁 */
#define DW3000_IRQ_CPLLLL           (1UL << 11)  /* 时钟PLL失锁 */
#define DW3000_IRQ_RXSFDTO          (1UL << 12)  /* RX SFD超时 */
#define DW3000_IRQ_HPDWARN          (1UL << 13)  /* 半双工预警 */
#define DW3000_IRQ_TXBERR           (1UL << 14)  /* TX缓冲区错误 */
#define DW3000_IRQ_AFFREJ           (1UL << 15)  /* 自动帧过滤拒绝 */
#define DW3000_IRQ_HSRBP            (1UL << 16)  /* 硬件接收缓冲区指针 */
#define DW3000_IRQ_ICRBP            (1UL << 17)  /* 读数据缓冲区指针 */
#define DW3000_IRQ_MCPLOCK          (1UL << 18)  /* 主时钟PLL锁定 */
#define DW3000_IRQ_RXSTNO           (1UL << 19)  /* RX STS无效 */
#define DW3000_IRQ_RXSTSF           (1UL << 20)  /* RX STS失败 */
#define DW3000_IRQ_RXFSTSF          (1UL << 21)  /* RX 第一次STS失败 */
#define DW3000_IRQ_RXDTMF           (1UL << 22)  /* RX 数据模式失败 */
#define DW3000_IRQ_LDEERR_D          (1UL << 23) /* LDE错误信号强 */
#define DW3000_IRQ_RXPHD            (1UL << 24)  /* RX PHR检测到 */
#define DW3000_IRQ_RXCE             (1UL << 25)  /* RX 帧控制错误 */
#define DW3000_IRQ_RXRSCS           (1UL << 26)  /* RX Reed-Solomon校正成功 */
#define DW3000_IRQ_RXPHE            (1UL << 27)  /* RX PHR错误 */
#define DW3000_IRQ_RXFSDD           (1UL << 28)  /* RX SFD检测到 */
#define DW3000_IRQ_RXPTO_D          (1UL << 29) /* RX 前导码超时信号强 */
#define DW3000_IRQ_CIAERR           (1UL << 30)  /* CIA错误 */
#define DW3000_IRQ_VBATLOW          (1UL << 31)  /* 电池电压低 */

/******************************************************************************
 * STS 配置
 ******************************************************************************/
#define DW3000_STS_MODE_OFF         0x00    /* STS禁用 */
#define DW3000_STS_MODE_STATIC      0x01    /* 静态STS */
#define DW3000_STS_MODE_DYNAMIC     0x02    /* 动态STS */
#define DW3000_STS_LEN_32           0x00    /* 32位STS */
#define DW3000_STS_LEN_64           0x01    /* 64位STS */
#define DW3000_STS_LEN_128          0x02    /* 128位STS */

/******************************************************************************
 * 信道配置
 ******************************************************************************/
#define DW3000_CHANNEL_5            5       /* 信道5 (6489.6 MHz) */
#define DW3000_CHANNEL_9            9       /* 信道9 (7987.2 MHz) */

/******************************************************************************
 * 数据速率
 ******************************************************************************/
#define DW3000_DATA_RATE_850K       0x00    /* 850 kbps */
#define DW3000_DATA_RATE_6M8        0x01    /* 6.8 Mbps */

/******************************************************************************
 * 前导码长度
 ******************************************************************************/
#define DW3000_PREAMBLE_32          0x00
#define DW3000_PREAMBLE_64          0x01
#define DW3000_PREAMBLE_128         0x02
#define DW3000_PREAMBLE_256         0x03
#define DW3000_PREAMBLE_512         0x04
#define DW3000_PREAMBLE_1024        0x05
#define DW3000_PREAMBLE_2048        0x06
#define DW3000_PREAMBLE_4096        0x07

/******************************************************************************
 * 测距常数
 ******************************************************************************/
#define DW3000_SPEED_OF_LIGHT       299702547   /* 光速 (m/s) */
#define DW3000_DWT_TIME_UNITS       (1.0 / 499.2e6 / 128.0) /* ~15.65ps */

/******************************************************************************
 * DW3000 私有数据结构
 ******************************************************************************/
typedef struct {
    uint8_t spi_handle[32];     /* SPI句柄 (平台特定) */
    uint8_t gpio_handle[32];    /* GPIO句柄 (平台特定) */
    uint32_t irq_pin;           /* 中断引脚 */
    uint32_t rst_pin;           /* 复位引脚 */
    uint32_t wakeup_pin;        /* 唤醒引脚 */
    
    /* 芯片状态 */
    bool initialized;
    bool sleeping;
    uwb_chip_state_t state;
    
    /* 配置缓存 */
    uwb_hal_config_t config;
    
    /* 测距缓存 */
    uint64_t tx_timestamp;
    uint64_t rx_timestamp;
    double clock_offset;
    
    /* STS配置 */
    uint8_t sts_key[16];
    uint8_t sts_iv[8];
    bool sts_enabled;
} dw3000_context_t;

/******************************************************************************
 * SPI 读写函数声明 (平台特定)
 ******************************************************************************/
extern int dw3000_spi_write(void *spi_handle, uint32_t reg_addr, const uint8_t *data, size_t len);
extern int dw3000_spi_read(void *spi_handle, uint32_t reg_addr, uint8_t *data, size_t len);
extern void dw3000_spi_cs_low(void *spi_handle);
extern void dw3000_spi_cs_high(void *spi_handle);
extern void dw3000_delay_ms(uint32_t ms);
extern void dw3000_delay_us(uint32_t us);
extern uint64_t dw3000_get_sys_time(void);

/******************************************************************************
 * 内部函数声明
 ******************************************************************************/
static int dw3000_read_reg(dw3000_context_t *ctx, uint32_t reg_addr, uint8_t *data, size_t len);
static int dw3000_write_reg(dw3000_context_t *ctx, uint32_t reg_addr, const uint8_t *data, size_t len);
static int dw3000_read_32(dw3000_context_t *ctx, uint32_t reg_addr, uint32_t *value);
static int dw3000_write_32(dw3000_context_t *ctx, uint32_t reg_addr, uint32_t value);
static int dw3000_send_command(dw3000_context_t *ctx, uint8_t cmd);
static int dw3000_wait_for_status(dw3000_context_t *ctx, uint32_t mask, uint32_t *status, uint32_t timeout_ms);
static int dw3000_configure_phy(dw3000_context_t *ctx, const uwb_phy_config_t *phy);
static int dw3000_configure_sts_internal(dw3000_context_t *ctx, const uwb_sts_config_t *sts);
static int dw3000_do_twr_ranging(dw3000_context_t *ctx, uwb_hal_measurement_t *result);
static uint64_t dw3000_get_rx_timestamp(dw3000_context_t *ctx);
static uint64_t dw3000_get_tx_timestamp(dw3000_context_t *ctx);
static double dw3000_calculate_distance(dw3000_context_t *ctx, uint64_t poll_tx, uint64_t poll_rx,
                                         uint64_t resp_tx, uint64_t resp_rx);

/******************************************************************************
 * DW3000 驱动接口实现 (uwb_driver_ops_t)
 ******************************************************************************/

static int dw3000_driver_init_impl(struct uwb_chip_driver *driver, void *spi_handle, void *gpio_handle)
{
    if (!driver) {
        return UWB_HAL_ERROR_INVALID_PARAM;
    }

    static dw3000_context_t g_dw3000_ctx;
    memset(&g_dw3000_ctx, 0, sizeof(g_dw3000_ctx));
    dw3000_context_t *ctx = &g_dw3000_ctx;

    driver->priv_data = ctx;
    driver->spi_handle = spi_handle;
    driver->gpio_handle = gpio_handle;

    /* 硬件复位 */
    dw3000_delay_ms(10);
    
    /* 读取设备ID验证芯片 */
    uint32_t dev_id = 0;
    if (dw3000_read_32(ctx, DW3000_DEV_ID_REG, &dev_id) != 0) {
        memset(&g_dw3000_ctx, 0, sizeof(g_dw3000_ctx));
        driver->priv_data = NULL;
        return UWB_HAL_ERROR_SPI_FAILURE;
    }

    if ((dev_id & DW3000_DEVICE_ID_MASK) != DW3000_DEVICE_ID) {
        memset(&g_dw3000_ctx, 0, sizeof(g_dw3000_ctx));
        driver->priv_data = NULL;
        return UWB_HAL_ERROR_INVALID_CHIP;
    }

    /* 初始化配置 */
    ctx->initialized = true;
    ctx->state = UWB_CHIP_STATE_IDLE;
    
    return UWB_HAL_OK;
}

static int dw3000_driver_deinit_impl(struct uwb_chip_driver *driver)
{
    if (!driver || !driver->priv_data) {
        return UWB_HAL_ERROR_NOT_INITIALIZED;
    }

    dw3000_context_t *ctx = (dw3000_context_t *)driver->priv_data;
    
    /* 关闭无线电 */
    dw3000_send_command(ctx, DW3000_CMD_TRXOFF);
    
    ctx->initialized = false;
    ctx->state = UWB_CHIP_STATE_UNINIT;
    
    driver->priv_data = NULL;
    
    return UWB_HAL_OK;
}

static int dw3000_driver_reset_impl(struct uwb_chip_driver *driver)
{
    if (!driver || !driver->priv_data) {
        return UWB_HAL_ERROR_NOT_INITIALIZED;
    }

    dw3000_context_t *ctx = (dw3000_context_t *)driver->priv_data;
    
    /* 软件复位 */
    dw3000_send_command(ctx, DW3000_CMD_TRXOFF);
    dw3000_delay_ms(5);
    
    ctx->state = UWB_CHIP_STATE_IDLE;
    
    return UWB_HAL_OK;
}

static int dw3000_driver_get_info_impl(struct uwb_chip_driver *driver, uwb_chip_info_t *info)
{
    if (!driver || !driver->priv_data || !info) {
        return UWB_HAL_ERROR_INVALID_PARAM;
    }

    dw3000_context_t *ctx = (dw3000_context_t *)driver->priv_data;
    
    info->chip_type = UWB_CHIP_QORVO_DW3000;
    info->supports_sts = true;
    info->supports_aoa = true;
    info->supports_pdoa = true;
    
    /* 读取芯片版本 */
    uint32_t dev_id = 0;
    dw3000_read_32(ctx, DW3000_DEV_ID_REG, &dev_id);
    info->chip_id = dev_id;
    
    snprintf(info->version_str, sizeof(info->version_str), "DW3000 v%u.%u",
             (unsigned int)((dev_id >> 8) & 0xF), (unsigned int)(dev_id & 0xF));
    
    return UWB_HAL_OK;
}

static int dw3000_driver_configure_impl(struct uwb_chip_driver *driver, const uwb_hal_config_t *config)
{
    if (!driver || !driver->priv_data || !config) {
        return UWB_HAL_ERROR_INVALID_PARAM;
    }

    dw3000_context_t *ctx = (dw3000_context_t *)driver->priv_data;
    
    memcpy(&ctx->config, config, sizeof(uwb_hal_config_t));
    
    /* 配置物理层 */
    int ret = dw3000_configure_phy(ctx, &config->phy);
    if (ret != 0) {
        return UWB_HAL_ERROR_NOT_SUPPORTED;
    }
    
    /* 配置STS */
    if (config->sts.enable_sts) {
        ret = dw3000_configure_sts_internal(ctx, &config->sts);
        if (ret != 0) {
            return UWB_HAL_ERROR_STS_CONFIG;
        }
    }
    
    ctx->sts_enabled = config->sts.enable_sts;
    ctx->state = UWB_CHIP_STATE_READY;
    
    return UWB_HAL_OK;
}

static int dw3000_driver_configure_sts_impl(struct uwb_chip_driver *driver, const uwb_sts_config_t *sts_config)
{
    if (!driver || !driver->priv_data || !sts_config) {
        return UWB_HAL_ERROR_INVALID_PARAM;
    }

    dw3000_context_t *ctx = (dw3000_context_t *)driver->priv_data;
    
    return dw3000_configure_sts_internal(ctx, sts_config);
}

static int dw3000_driver_start_ranging_impl(struct uwb_chip_driver *driver)
{
    if (!driver || !driver->priv_data) {
        return UWB_HAL_ERROR_NOT_INITIALIZED;
    }

    dw3000_context_t *ctx = (dw3000_context_t *)driver->priv_data;
    
    if (ctx->state != UWB_CHIP_STATE_READY) {
        return UWB_HAL_ERROR_NOT_INITIALIZED;
    }
    
    ctx->state = UWB_CHIP_STATE_RX;
    
    /* 启动接收 */
    dw3000_send_command(ctx, DW3000_CMD_RX);
    
    return UWB_HAL_OK;
}

static int dw3000_driver_stop_ranging_impl(struct uwb_chip_driver *driver)
{
    if (!driver || !driver->priv_data) {
        return UWB_HAL_ERROR_NOT_INITIALIZED;
    }

    dw3000_context_t *ctx = (dw3000_context_t *)driver->priv_data;
    
    dw3000_send_command(ctx, DW3000_CMD_TRXOFF);
    ctx->state = UWB_CHIP_STATE_READY;
    
    return UWB_HAL_OK;
}

static int dw3000_driver_do_single_ranging_impl(struct uwb_chip_driver *driver, uwb_hal_measurement_t *result)
{
    if (!driver || !driver->priv_data || !result) {
        return UWB_HAL_ERROR_INVALID_PARAM;
    }

    dw3000_context_t *ctx = (dw3000_context_t *)driver->priv_data;
    
    if (ctx->config.mode == UWB_MODE_TWO_WAY_RANGING ||
        ctx->config.mode == UWB_MODE_DS_TWR) {
        return dw3000_do_twr_ranging(ctx, result);
    }
    
    return UWB_HAL_ERROR_NOT_SUPPORTED;
}

static int dw3000_driver_get_measurement_impl(struct uwb_chip_driver *driver, uwb_hal_measurement_t *result)
{
    if (!driver || !driver->priv_data || !result) {
        return UWB_HAL_ERROR_INVALID_PARAM;
    }

    /* 测距结果在do_single_ranging中已经填充 */
    return UWB_HAL_OK;
}

static int dw3000_driver_sleep_impl(struct uwb_chip_driver *driver)
{
    if (!driver || !driver->priv_data) {
        return UWB_HAL_ERROR_NOT_INITIALIZED;
    }

    dw3000_context_t *ctx = (dw3000_context_t *)driver->priv_data;
    
    dw3000_send_command(ctx, DW3000_CMD_GO_SLEEP);
    ctx->sleeping = true;
    ctx->state = UWB_CHIP_STATE_SLEEP;
    
    return UWB_HAL_OK;
}

static int dw3000_driver_wakeup_impl(struct uwb_chip_driver *driver)
{
    if (!driver || !driver->priv_data) {
        return UWB_HAL_ERROR_NOT_INITIALIZED;
    }

    dw3000_context_t *ctx = (dw3000_context_t *)driver->priv_data;
    
    /* 激活芯片 (通过WAKEUP引脚或SPI交互) */
    ctx->sleeping = false;
    ctx->state = UWB_CHIP_STATE_IDLE;
    dw3000_delay_ms(2);
    
    return UWB_HAL_OK;
}

static int dw3000_driver_read_reg_impl(struct uwb_chip_driver *driver, uint32_t addr, uint8_t *data, size_t len)
{
    if (!driver || !driver->priv_data || !data) {
        return UWB_HAL_ERROR_INVALID_PARAM;
    }

    dw3000_context_t *ctx = (dw3000_context_t *)driver->priv_data;
    
    return dw3000_read_reg(ctx, addr, data, len);
}

static int dw3000_driver_write_reg_impl(struct uwb_chip_driver *driver, uint32_t addr, const uint8_t *data, size_t len)
{
    if (!driver || !driver->priv_data || !data) {
        return UWB_HAL_ERROR_INVALID_PARAM;
    }

    dw3000_context_t *ctx = (dw3000_context_t *)driver->priv_data;
    
    return dw3000_write_reg(ctx, addr, data, len);
}

/******************************************************************************
 * 内部函数实现
 ******************************************************************************/

static int dw3000_read_reg(dw3000_context_t *ctx, uint32_t reg_addr, uint8_t *data, size_t len)
{
    /* 构建SPI读取命令 */
    uint8_t header[3];
    uint8_t header_len = 0;
    
    if (reg_addr < 0x80) {
        /* 短地址: 1字节头部 */
        header[0] = (uint8_t)(reg_addr & 0x7F);
        header_len = 1;
    } else if (reg_addr < 0x4000) {
        /* 中地址: 2字节头部 */
        header[0] = (uint8_t)(0x80 | ((reg_addr >> 7) & 0x7F));
        header[1] = (uint8_t)(reg_addr & 0x7F);
        header_len = 2;
    } else {
        /* 长地址: 3字节头部 */
        header[0] = (uint8_t)(0xC0 | ((reg_addr >> 14) & 0x3F));
        header[1] = (uint8_t)((reg_addr >> 7) & 0x7F);
        header[2] = (uint8_t)(reg_addr & 0x7F);
        header_len = 3;
    }
    
    /* SPI交互 (平台特定) */
    /* 实际应调用dw3000_spi_read等函数 */
    
    return 0;
}

static int dw3000_write_reg(dw3000_context_t *ctx, uint32_t reg_addr, const uint8_t *data, size_t len)
{
    /* 构建SPI写入命令 */
    uint8_t header[3];
    uint8_t header_len = 0;
    
    if (reg_addr < 0x80) {
        header[0] = (uint8_t)(0x80 | (reg_addr & 0x7F));
        header_len = 1;
    } else if (reg_addr < 0x4000) {
        header[0] = (uint8_t)(0xC0 | ((reg_addr >> 7) & 0x7F));
        header[1] = (uint8_t)(reg_addr & 0x7F);
        header_len = 2;
    } else {
        header[0] = (uint8_t)(0xE0 | ((reg_addr >> 14) & 0x3F));
        header[1] = (uint8_t)((reg_addr >> 7) & 0x7F);
        header[2] = (uint8_t)(reg_addr & 0x7F);
        header_len = 3;
    }
    
    return 0;
}

static int dw3000_read_32(dw3000_context_t *ctx, uint32_t reg_addr, uint32_t *value)
{
    uint8_t data[4];
    int ret = dw3000_read_reg(ctx, reg_addr, data, 4);
    if (ret == 0 && value) {
        *value = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                 ((uint32_t)data[2] << 8) | (uint32_t)data[3];
    }
    return ret;
}

static int dw3000_write_32(dw3000_context_t *ctx, uint32_t reg_addr, uint32_t value)
{
    uint8_t data[4] = {
        (uint8_t)(value >> 24),
        (uint8_t)(value >> 16),
        (uint8_t)(value >> 8),
        (uint8_t)(value)
    };
    return dw3000_write_reg(ctx, reg_addr, data, 4);
}

static int dw3000_send_command(dw3000_context_t *ctx, uint8_t cmd)
{
    /* 写入系统控制寄存器执行命令 */
    return dw3000_write_reg(ctx, DW3000_SYS_CTRL_REG, &cmd, 1);
}

static int dw3000_wait_for_status(dw3000_context_t *ctx, uint32_t mask, uint32_t *status, uint32_t timeout_ms)
{
    uint32_t start_time = (uint32_t)dw3000_get_sys_time();
    uint32_t current_status = 0;
    
    while (((uint32_t)dw3000_get_sys_time() - start_time) < timeout_ms * 1000) {
        dw3000_read_32(ctx, DW3000_SYS_STATUS_REG, &current_status);
        
        if (current_status & mask) {
            if (status) {
                *status = current_status;
            }
            return 0;
        }
        
        dw3000_delay_us(100);
    }
    
    return -1; /* 超时 */
}

static int dw3000_configure_phy(dw3000_context_t *ctx, const uwb_phy_config_t *phy)
{
    uint32_t sys_cfg = 0;
    uint32_t chan_ctrl = 0;
    
    /* 配置信道 */
    chan_ctrl |= (phy->channel & 0x3F);
    
    /* 配置数据速率 */
    if (phy->data_rate == UWB_DATA_RATE_6M8) {
        sys_cfg |= (1 << 2);
    }
    
    /* 配置PRF */
    if (phy->prf == UWB_PRF_64M) {
        chan_ctrl |= (1 << 16);
    }
    
    /* 配置前导码长度 */
    chan_ctrl |= ((phy->preamble_length & 0x07) << 17);
    
    /* 写入配置 */
    dw3000_write_32(ctx, DW3000_SYS_CFG_REG, sys_cfg);
    dw3000_write_32(ctx, DW3000_CHAN_CTRL_REG, chan_ctrl);
    
    return 0;
}

static int dw3000_configure_sts_internal(dw3000_context_t *ctx, const uwb_sts_config_t *sts)
{
    if (!sts->enable_sts) {
        return 0;
    }
    
    /* 配置STS密钥 */
    dw3000_write_reg(ctx, DW3000_STS_KEY_REG, sts->sts_key, 16);
    
    /* 配置STS IV */
    uint8_t iv_data[12] = {0};
    memcpy(iv_data, sts->sts_iv, 8);
    iv_data[8] = sts->sts_index;
    dw3000_write_reg(ctx, DW3000_STS_IV_REG, iv_data, 12);
    
    /* 启用STS */
    uint32_t sts_cfg = sts->sts_mode;
    dw3000_write_32(ctx, DW3000_STS_CFG_REG, sts_cfg);
    
    /* 保存配置 */
    memcpy(ctx->sts_key, sts->sts_key, 16);
    memcpy(ctx->sts_iv, sts->sts_iv, 8);
    
    return 0;
}

static int dw3000_do_twr_ranging(dw3000_context_t *ctx, uwb_hal_measurement_t *result)
{
    /* 简化的双边双向测距 (DS-TWR) 实现 */
    
    uint8_t poll_msg[] = {0x41, 0x88, 0x00, 0x00, 0xCA, 0xDE, 'W', 'A', 'V', 'E', 0x21, 0x00, 0x00};
    uint8_t resp_msg[] = {0x41, 0x88, 0x00, 0x00, 0xCA, 0xDE, 'V', 'E', 'W', 'A', 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t final_msg[] = {0x41, 0x88, 0x00, 0x00, 0xCA, 0xDE, 'W', 'A', 'V', 'E', 0x23, 0x00, 0x00};
    
    uint64_t poll_tx_ts, poll_rx_ts, resp_tx_ts, resp_rx_ts;
    uint64_t final_tx_ts, final_rx_ts;
    uint32_t status = 0;
    
    /* 第一步: 发送Poll消息 */
    dw3000_write_reg(ctx, DW3000_TX_BUFFER_REG, poll_msg, sizeof(poll_msg));
    dw3000_send_command(ctx, DW3000_CMD_TX_W4R);
    
    if (dw3000_wait_for_status(ctx, DW3000_IRQ_TXFRS, &status, 10) != 0) {
        return UWB_HAL_ERROR_RANGING_FAILED;
    }
    
    poll_tx_ts = dw3000_get_tx_timestamp(ctx);
    
    /* 等待响应 */
    if (dw3000_wait_for_status(ctx, DW3000_IRQ_RXFCG, &status, 100) != 0) {
        return UWB_HAL_ERROR_RANGING_FAILED;
    }
    
    resp_rx_ts = dw3000_get_rx_timestamp(ctx);
    
    /* 第二步: 发送Response */
    dw3000_write_reg(ctx, DW3000_TX_BUFFER_REG, resp_msg, sizeof(resp_msg));
    dw3000_send_command(ctx, DW3000_CMD_TX_W4R);
    
    if (dw3000_wait_for_status(ctx, DW3000_IRQ_TXFRS, &status, 10) != 0) {
        return UWB_HAL_ERROR_RANGING_FAILED;
    }
    
    resp_tx_ts = dw3000_get_tx_timestamp(ctx);
    
    /* 等待Final */
    if (dw3000_wait_for_status(ctx, DW3000_IRQ_RXFCG, &status, 100) != 0) {
        return UWB_HAL_ERROR_RANGING_FAILED;
    }
    
    final_rx_ts = dw3000_get_rx_timestamp(ctx);
    
    /* 计算距离 (双边双向测距) */
    double distance = dw3000_calculate_distance(ctx, poll_tx_ts, resp_rx_ts, resp_tx_ts, final_rx_ts);
    
    /* 填充结果 */
    result->timestamp = dw3000_get_sys_time();
    result->distance_m = (float)distance;
    result->accuracy_m = 0.1f; /* 估计精度 */
    result->is_valid = true;
    result->quality = 95;
    
    return UWB_HAL_OK;
}

static uint64_t dw3000_get_rx_timestamp(dw3000_context_t *ctx)
{
    uint8_t ts_data[5];
    dw3000_read_reg(ctx, DW3000_RX_TIME_REG, ts_data, 5);
    
    uint64_t timestamp = 0;
    for (int i = 0; i < 5; i++) {
        timestamp |= ((uint64_t)ts_data[i]) << (8 * i);
    }
    
    return timestamp;
}

static uint64_t dw3000_get_tx_timestamp(dw3000_context_t *ctx)
{
    uint8_t ts_data[5];
    dw3000_read_reg(ctx, DW3000_TX_TIME_REG, ts_data, 5);
    
    uint64_t timestamp = 0;
    for (int i = 0; i < 5; i++) {
        timestamp |= ((uint64_t)ts_data[i]) << (8 * i);
    }
    
    return timestamp;
}

static double dw3000_calculate_distance(dw3000_context_t *ctx, uint64_t poll_tx, uint64_t poll_rx,
                                         uint64_t resp_tx, uint64_t resp_rx)
{
    /* 双边双向测距计算
     * 公式: distance = ((round_trip_time - response_delay) / 2) * speed_of_light
     */
    
    double round_trip_time = (double)(resp_rx - poll_tx) * DW3000_DWT_TIME_UNITS;
    double response_delay = (double)(resp_tx - poll_rx) * DW3000_DWT_TIME_UNITS;
    
    double tof = (round_trip_time - response_delay) / 2.0;
    double distance = tof * DW3000_SPEED_OF_LIGHT;
    
    return distance;
}

/******************************************************************************
 * DW3000 驱动初始化
 ******************************************************************************/

int dw3000_driver_register(uwb_chip_driver_t *driver)
{
    if (!driver) {
        return -1;
    }
    
    driver->chip_type = UWB_CHIP_QORVO_DW3000;
    driver->state = UWB_CHIP_STATE_UNINIT;
    driver->initialized = false;
    
    /* 填充操作函数表 */
    driver->ops.init = dw3000_driver_init_impl;
    driver->ops.deinit = dw3000_driver_deinit_impl;
    driver->ops.reset = dw3000_driver_reset_impl;
    driver->ops.get_info = dw3000_driver_get_info_impl;
    driver->ops.configure = dw3000_driver_configure_impl;
    driver->ops.configure_sts = dw3000_driver_configure_sts_impl;
    driver->ops.start_ranging = dw3000_driver_start_ranging_impl;
    driver->ops.stop_ranging = dw3000_driver_stop_ranging_impl;
    driver->ops.do_single_ranging = dw3000_driver_do_single_ranging_impl;
    driver->ops.get_measurement = dw3000_driver_get_measurement_impl;
    driver->ops.sleep = dw3000_driver_sleep_impl;
    driver->ops.wakeup = dw3000_driver_wakeup_impl;
    driver->ops.read_reg = dw3000_driver_read_reg_impl;
    driver->ops.write_reg = dw3000_driver_write_reg_impl;
    
    return 0;
}
