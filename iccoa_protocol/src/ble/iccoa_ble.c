/**
 * @file iccoa_ble.c
 * @brief ICCOA BLE Communication Layer (NXP KW47A)
 */

#include "iccoa_digital_key.h"

/* KW47A SPI command interface (shared with CCC BLE driver) */
#define KW47A_SPI_CMD_WRITE    0x0A
#define KW47A_SPI_CMD_READ     0x0B
#define KW47A_IRQ_PIN          GPIO_PIN_3
#define KW47A_CS_PORT          GPIOB
#define KW47A_CS_PIN           GPIO_PIN_6

static iccoa_ble_recv_cb_t g_ble_cb = NULL;
static uint8_t g_rx_buf[260];
static volatile bool g_connected = false;

int32_t iccoa_ble_init(void)
{
    /* SPI2 init @ 8MHz, Mode 0 */
    /* KW47A reset sequence */
    /* Configure IRQ pin (falling edge) */
    g_ble_cb = NULL;
    g_connected = false;
    return ICCOA_OK;
}

int32_t iccoa_ble_deinit(void)
{
    g_ble_cb = NULL;
    g_connected = false;
    return ICCOA_OK;
}

int32_t iccoa_ble_start_adv(void)
{
    /* KW47A: start BLE advertising with ICCOA service UUID 0xFEF5 */
    /* Adv data: Flags + Complete 16bit Service UUID + Local Name */
    uint8_t adv_cmd[] = { KW47A_SPI_CMD_WRITE, 0x01, 0x00 };
    /* TODO: SPI transfer */
    (void)adv_cmd;
    return ICCOA_OK;
}

int32_t iccoa_ble_stop_adv(void)
{
    uint8_t stop_cmd[] = { KW47A_SPI_CMD_WRITE, 0x02, 0x00 };
    /* TODO: SPI transfer */
    (void)stop_cmd;
    return ICCOA_OK;
}

int32_t iccoa_ble_send(const uint8_t *data, uint16_t len)
{
    if (!data || len == 0 || len > 260) return ICCOA_ERR_PARAM;
    if (!g_connected) return ICCOA_ERR_BUSY;

    /* Write to KW47A TX buffer via SPI, then trigger send */
    /* TODO: SPI transfer with CS control */
    (void)data; (void)len;
    return ICCOA_OK;
}

int32_t iccoa_ble_register_cb(iccoa_ble_recv_cb_t cb)
{
    if (!cb) return ICCOA_ERR_PARAM;
    g_ble_cb = cb;
    return ICCOA_OK;
}

/* Called from GPIO IRQ handler */
void iccoa_ble_irq_handler(void)
{
    /* Read KW47A RX buffer via SPI */
    /* Dispatch: connection event vs data received */
    /* If data: call g_ble_cb(g_rx_buf, len) */
    if (g_ble_cb) {
        /* TODO: parse event type */
        /* g_ble_cb(g_rx_buf, rx_len); */
    }
}
