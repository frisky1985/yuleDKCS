/*
 * hal_kw47a.c — NXP KW47A HAL implementation
 *
 * Platform-specific implementation of hal_interface.h
 * Uses NXP MCUXpresso SDK drivers.
 */

#include "hal_interface.h"
#include "kw47a_board.h"

/* MCUXpresso SDK headers (included via SDK download) */
#include "fsl_clock.h"
#include "fsl_gpio.h"
#include "fsl_lpuart.h"
#include "fsl_lpspi.h"
#include "fsl_lpi2c.h"

/* ── UART Debug ──────────────────────────────────────────── */
static LPUART_Type *s_debug_uart = LPUART0;

void hal_debug_printf(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    LPUART_WriteBlocking(s_debug_uart, (uint8_t *)buf, strlen(buf));
}

/* ── Time ────────────────────────────────────────────────── */
uint64_t hal_time_us(void)
{
    return (uint64_t)SysTick->VAL;  /* Simplified, use systick or RTC */
}

uint32_t hal_time_ms(void)
{
    return xTaskGetTickCount();     /* FreeRTOS ticks */
}

void hal_delay_ms(uint32_t ms)
{
    SDK_DelayAtLeastUs(ms * 1000, CLOCK_GetCoreSysClkFreq());
}

/* ── BLE (KW47A integrated BLE) ──────────────────────────── */
int hal_ble_init(void)
{
    /* KW47A BLE is initialized via ble_controller_init() in MCUX SDK */
    return ble_controller_init(kBLE_LLParams);
}

int hal_ble_advertise_start(const uint8_t *adv_data, uint16_t len)
{
    return ble_adapter_advertise_start(adv_data, len);
}

int hal_ble_send_notify(uint16_t conn_handle, uint16_t char_handle,
                         const uint8_t *data, uint16_t len)
{
    return ble_adapter_send_notification(conn_handle, char_handle,
                                          (uint8_t *)data, len);
}

/* ── NFC (ST25R501 via LPSPI1) ───────────────────────────── */
static LPSPI_Type *s_nfc_spi = LPSPI1;

int hal_nfc_init(void)
{
    lpspi_master_config_t config;
    LPSPI_MasterGetDefaultConfig(&config);
    config.baudRate = NFC_SPI_BAUDRATE;
    config.pcs = kLPSPI_Pcs1;
    LPSPI_MasterInit(s_nfc_spi, &config, CLOCK_GetLpspiClkFreq(1));
    return 0;
}

int hal_nfc_transceive(const uint8_t *tx, uint16_t tx_len,
                         uint8_t *rx, uint16_t *rx_len)
{
    lpspi_transfer_t xfer = {
        .txData = (uint8_t *)tx,
        .rxData = rx,
        .dataSize = tx_len,
    };
    status_t status = LPSPI_MasterTransferBlocking(s_nfc_spi, &xfer);
    *rx_len = (status == kStatus_Success) ? tx_len : 0;
    return (status == kStatus_Success) ? 0 : -1;
}

/* ── UWB (NCJ29D6 via LPSPI2) ────────────────────────────── */
static LPSPI_Type *s_uwb_spi = LPSPI2;

int hal_uwb_init(void)
{
    lpspi_master_config_t config;
    LPSPI_MasterGetDefaultConfig(&config);
    config.baudRate = UWB_SPI_BAUDRATE;
    LPSPI_MasterInit(s_uwb_spi, &config, CLOCK_GetLpspiClkFreq(2));
    return 0;
}

/* ── SE050 (via LPI2C0) ──────────────────────────────────── */
static LPI2C_Type *s_se050_i2c = LPI2C0;

int hal_se050_init(void)
{
    lpi2c_master_config_t config;
    LPI2C_MasterGetDefaultConfig(&config);
    config.baudRate_Hz = SE050_I2C_BAUDRATE;
    LPI2C_MasterInit(s_se050_i2c, &config, CLOCK_GetLpi2cClkFreq(0));
    return se05x_init(s_se050_i2c, SE050_I2C_ADDR);
}

int hal_se050_ecdsa_sign(const uint8_t *key_id, const uint8_t *hash,
                           uint8_t *signature)
{
    return se05x_ecdsa_sign(s_se050_i2c, key_id, hash, 32, signature, 64);
}

int hal_se050_generate_random(uint8_t *buffer, uint16_t len)
{
    return se05x_random(s_se050_i2c, buffer, len);
}

/* ── NV partition / OTP（驱动骨架）────────────────────────────
 * offset 相对 NV 分区基址（kw47a_nv.h: 0x7C000），非绝对地址。
 * 目标实现：MCUXpresso fsl_flash 的 FLASH_Erase/FLASH_Program，
 * 每次操作前按页对齐；OTP 使用 KW47A eFuse 驱动。
 * 在厂商驱动接入前保持返回 -1：kw47a_nv_init() 会判定不可用，
 * sec_nv 不绑定，系统降级为 RAM-only，不会误判为已锁定。
 */
int hal_nv_flash_read(uint32_t offset, void *data, uint32_t len)
{
    (void)offset; (void)data; (void)len;
    return -1; /* TODO(target): flash_cache/FLASH_Read + memcpy */
}

int hal_nv_flash_write(uint32_t offset, const void *data, uint32_t len)
{
    (void)offset; (void)data; (void)len;
    return -1; /* TODO(target): FLASH_Program，按 4/8 字节对齐分块 */
}

int hal_nv_flash_erase(uint32_t offset, uint32_t len)
{
    (void)offset; (void)len;
    return -1; /* TODO(target): FLASH_Erase(Page/Sector)，按扇区对齐 */
}

int hal_otp_lock_read(void)
{
    return 0; /* TODO(target): 读 eFuse PROD_LOCK 位 */
}

int hal_otp_lock_set(void)
{
    return -1; /* TODO(target): 烧写 eFuse PROD_LOCK（不可逆） */
}

int hal_otp_epoch_read(uint32_t *epoch)
{
    if (epoch) *epoch = 0;
    return -1; /* TODO(target): 读 eFuse CRL_EPOCH 水位 */
}

int hal_otp_epoch_raise(uint32_t epoch)
{
    (void)epoch;
    return -1; /* TODO(target): 按位烧写 eFuse，仅允许单调递增 */
}
