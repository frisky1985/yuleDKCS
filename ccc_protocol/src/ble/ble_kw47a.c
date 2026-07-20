/**
 * @file ble_kw47a.c
 * @brief BLE Driver for NXP KW47A
 */

#include "ccc_digital_key.h"

/* KW47A SPI Command Interface */
#define KW47A_CMD_RESET             0x01
#define KW47A_CMD_INIT              0x02
#define KW47A_CMD_START_ADV         0x10
#define KW47A_CMD_STOP_ADV          0x11
#define KW47A_CMD_CONNECT           0x12
#define KW47A_CMD_DISCONNECT        0x13
#define KW47A_CMD_SEND_DATA         0x20
#define KW47A_CMD_OOB_PAIR          0x30
#define KW47A_EVT_RECV_DATA         0xA0
#define KW47A_EVT_CONNECTED         0xA1
#define KW47A_EVT_DISCONNECTED      0xA2

/* Callback storage */
static ble_recv_cb_t    g_recv_cb    = NULL;
static ble_conn_cb_t    g_conn_cb    = NULL;
static ble_disconn_cb_t g_disconn_cb = NULL;

static uint16_t g_conn_handle = 0xFFFF;

/* SPI transfer helpers */
extern int32_t spi_transfer(uint8_t dev, const uint8_t *tx, uint8_t *rx, uint16_t len);
extern void    gpio_write(uint8_t port, uint8_t pin, uint8_t val);
extern uint8_t gpio_read(uint8_t port, uint8_t pin);

static ccc_status_t kw47a_send_cmd(uint8_t cmd, const uint8_t *payload, uint16_t len)
{
    uint8_t header[4] = { cmd, 0x00, (uint8_t)(len >> 8), (uint8_t)(len & 0xFF) };

    /* Assert CS */
    gpio_write(KW47A_CS_PORT, KW47A_CS_PIN, 0);
    spi_transfer(1, header, NULL, 4);
    if (len > 0 && payload) {
        spi_transfer(1, payload, NULL, len);
    }
    gpio_write(KW47A_CS_PORT, KW47A_CS_PIN, 1);

    return CCC_OK;
}

ccc_status_t ble_kw47a_init(void)
{
    /* Hardware reset */
    gpio_write(KW47A_RST_PORT, KW47A_RST_PIN, 0);
    gpio_write(KW47A_RST_PORT, KW47A_RST_PIN, 1);

    /* Send init command */
    ccc_status_t ret = kw47a_send_cmd(KW47A_CMD_INIT, NULL, 0);
    if (ret != CCC_OK) return ret;

    g_conn_handle = 0xFFFF;
    return CCC_OK;
}

ccc_status_t ble_kw47a_deinit(void)
{
    kw47a_send_cmd(KW47A_CMD_RESET, NULL, 0);
    g_conn_handle = 0xFFFF;
    return CCC_OK;
}

ccc_status_t ble_start_adv(const ble_adv_param_t *param)
{
    if (!param) return CCC_ERR_INVALID_PARAM;
    return kw47a_send_cmd(KW47A_CMD_START_ADV, param->data, param->len);
}

ccc_status_t ble_stop_adv(void)
{
    return kw47a_send_cmd(KW47A_CMD_STOP_ADV, NULL, 0);
}

ccc_status_t ble_connect(const ble_addr_t *addr, const ble_conn_param_t *param)
{
    (void)param;
    if (!addr) return CCC_ERR_INVALID_PARAM;
    return kw47a_send_cmd(KW47A_CMD_CONNECT, addr->addr, 6);
}

ccc_status_t ble_disconnect(uint16_t conn_handle)
{
    uint8_t buf[2] = { (uint8_t)(conn_handle >> 8), (uint8_t)(conn_handle & 0xFF) };
    return kw47a_send_cmd(KW47A_CMD_DISCONNECT, buf, 2);
}

ccc_status_t ble_send(uint16_t conn_handle, const uint8_t *data, uint16_t len)
{
    (void)conn_handle;
    if (!data || len == 0) return CCC_ERR_INVALID_PARAM;
    if (len > BLE_MAX_PAYLOAD) return CCC_ERR_INVALID_PARAM;
    return kw47a_send_cmd(KW47A_CMD_SEND_DATA, data, len);
}

ccc_status_t ble_register_recv_cb(ble_recv_cb_t cb)
{
    g_recv_cb = cb;
    return CCC_OK;
}

ccc_status_t ble_register_conn_cb(ble_conn_cb_t cb)
{
    g_conn_cb = cb;
    return CCC_OK;
}

ccc_status_t ble_register_disconn_cb(ble_disconn_cb_t cb)
{
    g_disconn_cb = cb;
    return CCC_OK;
}

ccc_status_t ble_oob_pair(const ccc_nfc_oob_data_t *oob)
{
    if (!oob) return CCC_ERR_INVALID_PARAM;
    return kw47a_send_cmd(KW47A_CMD_OOB_PAIR, (const uint8_t *)oob, sizeof(*oob));
}

bool ble_is_connected(uint16_t conn_handle)
{
    return (g_conn_handle == conn_handle && g_conn_handle != 0xFFFF);
}

/* IRQ handler - called from platform ISR */
void kw47a_irq_handler(void)
{
    uint8_t evt_buf[260] = {0};
    uint8_t header[4] = {0};

    gpio_write(KW47A_CS_PORT, KW47A_CS_PIN, 0);
    spi_transfer(1, NULL, header, 4);
    uint16_t payload_len = ((uint16_t)header[2] << 8) | header[3];
    if (payload_len > 0 && payload_len <= 256) {
        spi_transfer(1, NULL, evt_buf, payload_len);
    }
    gpio_write(KW47A_CS_PORT, KW47A_CS_PIN, 1);

    uint8_t evt_type = header[0];

    switch (evt_type) {
    case KW47A_EVT_CONNECTED:
        g_conn_handle = ((uint16_t)evt_buf[0] << 8) | evt_buf[1];
        if (g_conn_cb) g_conn_cb(g_conn_handle, 0);
        break;

    case KW47A_EVT_DISCONNECTED:
        if (g_disconn_cb) g_disconn_cb(g_conn_handle, evt_buf[0]);
        g_conn_handle = 0xFFFF;
        break;

    case KW47A_EVT_RECV_DATA: {
        uint16_t ch = ((uint16_t)evt_buf[0] << 8) | evt_buf[1];
        if (g_recv_cb) g_recv_cb(ch, &evt_buf[2], payload_len - 2);
        break;
    }

    default:
        break;
    }
}
