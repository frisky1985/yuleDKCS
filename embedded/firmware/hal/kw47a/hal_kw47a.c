/*
 * hal_kw47a.c — NXP KW47A HAL implementation
 *
 * Platform-specific implementation of hal_interface.h
 * Uses NXP MCUXpresso SDK drivers.
 */

#include "hal_interface.h"
#include "kw47a_board.h"
#include "kw47a_flash_ll.h"
#include "kw47a_otp_ll.h"
#include "sec_nv.h"

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

/* ── NV partition / OTP（经平台无关 LL 层）────────────────────
 * offset 相对 NV 分区基址（SEC_NV_FLASH_BASE: 0x74000），非绝对地址。
 * Flash 操作由 kw47a_flash_ll_* 完成（MCUX 适配层在 kw47a_flash_mcux.c，
 * 由 kw47a_nv_flash_hw_init() 绑定）；OTP 转发 kw47a_otp_ll_*。
 * 未绑定时 LL 层返回 -1，kw47a_nv_init() 判定不可用并降级 RAM-only。
 */
static int nv_check_range(uint32_t offset, uint32_t len)
{
    if (offset >= SEC_NV_FLASH_SIZE || len > (SEC_NV_FLASH_SIZE - offset)) {
        return -1;
    }
    return 0;
}

int hal_nv_flash_read(uint32_t offset, void *data, uint32_t len)
{
    if (data == NULL || len == 0u || nv_check_range(offset, len) != 0) {
        return -1;
    }
    return kw47a_flash_ll_read(SEC_NV_FLASH_BASE + offset,
                               (uint8_t *)data, len);
}

int hal_nv_flash_write(uint32_t offset, const void *data, uint32_t len)
{
    const uint8_t *src = (const uint8_t *)data;
    uint32_t done = 0;

    if (src == NULL || len == 0u || nv_check_range(offset, len) != 0) {
        return -1;
    }
    if ((offset % 4u) != 0u) {
        return -1;
    }

    /* LL 层要求 4 字节对齐的编程单元；尾字节用 0xFF 补齐（擦除态无副作用）。 */
    while ((len - done) >= 4u) {
        if (kw47a_flash_ll_program(SEC_NV_FLASH_BASE + offset + done,
                                   src + done, 4u) != 0) {
            return -1;
        }
        done += 4u;
    }
    if (done < len) {
        uint8_t tail[4] = { 0xFFu, 0xFFu, 0xFFu, 0xFFu };
        uint32_t i;

        if (nv_check_range(offset + done, 4u) != 0) {
            return -1;
        }
        for (i = 0; i < (len - done); i++) {
            tail[i] = src[done + i];
        }
        if (kw47a_flash_ll_program(SEC_NV_FLASH_BASE + offset + done,
                                   tail, 4u) != 0) {
            return -1;
        }
    }
    return 0;
}

int hal_nv_flash_erase(uint32_t offset, uint32_t len)
{
    uint32_t page = kw47a_flash_ll_page_size();
    uint32_t done;

    if (len == 0u || nv_check_range(offset, len) != 0) {
        return -1;
    }
    if (page == 0u || (offset % page) != 0u || (len % page) != 0u) {
        return -1;
    }
    for (done = 0; done < len; done += page) {
        if (kw47a_flash_ll_erase(SEC_NV_FLASH_BASE + offset + done) != 0) {
            return -1;
        }
    }
    return 0;
}

static void otp_bind_once(void)
{
    static int bound;

    if (!bound) {
        bound = 1;
        (void)kw47a_otp_ll_bind_default();
    }
}

int hal_otp_lock_read(void)
{
    otp_bind_once();
    return kw47a_otp_ll_lock_read() ? 1 : 0;
}

int hal_otp_lock_set(void)
{
    otp_bind_once();
    return kw47a_otp_ll_lock_set();
}

int hal_otp_epoch_read(uint32_t *epoch)
{
    if (epoch == NULL) {
        return -1;
    }
    otp_bind_once();
    *epoch = kw47a_otp_ll_epoch_read();
    return 0;
}

int hal_otp_epoch_raise(uint32_t epoch)
{
    otp_bind_once();
    return kw47a_otp_ll_epoch_raise(epoch);
}
