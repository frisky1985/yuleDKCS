/**
 * @file nfc_st25r501.c
 * @brief NFC Driver for ST ST25R501
 */

#include "ccc_digital_key.h"

/* ST25R501 Register Map (key registers) */
#define ST25R501_REG_ID             0x01
#define ST25R501_REG_IO_CFG1        0x02
#define ST25R501_REG_OP_CTRL        0x00
#define ST25R501_REG_MODE_DEF       0x04
#define ST25R501_REG_FIELD_ON       0x10
#define ST25R501_REG_FIELD_OFF      0x11
#define ST25R501_REG_AUX_DISPLAY    0x18

/* SPI commands */
#define ST25R501_CMD_READ           0x02
#define ST25R501_CMD_WRITE          0x01
#define ST25R501_CMD_FIFO_READ      0x03
#define ST25R501_CMD_FIFO_WRITE     0x04

static nfc_state_e g_nfc_state = NFC_STATE_IDLE;

/* Internal SPI helpers - platform-specific implementation required */
extern int32_t spi_transfer(uint8_t dev, const uint8_t *tx, uint8_t *rx, uint16_t len);
extern void    gpio_write(uint8_t port, uint8_t pin, uint8_t val);
extern uint8_t gpio_read(uint8_t port, uint8_t pin);

static int32_t st25r501_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t tx[3] = { ST25R501_CMD_WRITE, reg, val };
    return spi_transfer(2, tx, NULL, 3);  /* SPI2 */
}

static int32_t st25r501_read_reg(uint8_t reg, uint8_t *val)
{
    uint8_t tx[2] = { ST25R501_CMD_READ, reg };
    uint8_t rx[3] = {0};
    int32_t ret = spi_transfer(2, tx, rx, 3);
    if (ret == 0 && val) *val = rx[2];
    return ret;
}

ccc_status_t nfc_st25r501_init(void)
{
    /* Hardware reset */
    gpio_write(ST25R501_RST_PORT, ST25R501_RST_PIN, 0);
    /* delay 10ms */
    gpio_write(ST25R501_RST_PORT, ST25R501_RST_PIN, 1);
    /* delay 10ms */

    /* Verify chip ID */
    uint8_t chip_id = 0;
    if (st25r501_read_reg(ST25R501_REG_ID, &chip_id) != 0) {
        return CCC_ERR_HARDWARE;
    }

    /* Configure: NFC-F (FeliCa) mode for CCC */
    st25r501_write_reg(ST25R501_REG_MODE_DEF, 0x03);  /* Passive NFC-F */
    st25r501_write_reg(ST25R501_REG_IO_CFG1, 0x08);   /* SPI + IRQ mode */

    /* Enable field detection interrupt */
    st25r501_write_reg(ST25R501_REG_OP_CTRL, 0x20);

    g_nfc_state = NFC_STATE_IDLE;
    return CCC_OK;
}

ccc_status_t nfc_st25r501_deinit(void)
{
    /* Turn off RF field */
    st25r501_write_reg(ST25R501_REG_FIELD_OFF, 0x00);
    g_nfc_state = NFC_STATE_IDLE;
    return CCC_OK;
}

bool nfc_field_detect(void)
{
    /* Read field detect status from IRQ or register */
    return gpio_read(ST25R501_IRQ_PORT, ST25R501_IRQ_PIN) != 0;
}

nfc_state_e nfc_get_state(void)
{
    return g_nfc_state;
}

ccc_status_t nfc_start_listen(void)
{
    /* Enable RF field in listen mode */
    st25r501_write_reg(ST25R501_REG_FIELD_ON, 0x01);
    g_nfc_state = NFC_STATE_FIELD_DETECT;
    return CCC_OK;
}

ccc_status_t nfc_stop_listen(void)
{
    st25r501_write_reg(ST25R501_REG_FIELD_OFF, 0x00);
    g_nfc_state = NFC_STATE_IDLE;
    return CCC_OK;
}

ccc_status_t nfc_send(const uint8_t *data, uint16_t len)
{
    if (!data || len == 0) return CCC_ERR_INVALID_PARAM;

    /* Write data to FIFO */
    uint8_t header[2] = { ST25R501_CMD_FIFO_WRITE, (uint8_t)len };
    spi_transfer(2, header, NULL, 2);
    spi_transfer(2, data, NULL, len);

    return CCC_OK;
}

ccc_status_t nfc_recv(uint8_t *buf, uint16_t *len, uint32_t timeout_ms)
{
    if (!buf || !len) return CCC_ERR_INVALID_PARAM;
    /* FIFO read - platform timeout implementation needed */
    (void)timeout_ms;
    uint8_t header[2] = { ST25R501_CMD_FIFO_READ, 0x00 };
    uint8_t rx_len = 0;
    spi_transfer(2, header, &rx_len, 1);
    if (rx_len > 0) {
        *len = rx_len;
        spi_transfer(2, NULL, buf, rx_len);
    }
    return CCC_OK;
}

ccc_status_t nfc_oob_exchange(ccc_nfc_oob_data_t *oob_out, ccc_nfc_oob_data_t *oob_in)
{
    if (!oob_out || !oob_in) return CCC_ERR_INVALID_PARAM;

    g_nfc_state = NFC_STATE_DATA_EXCHANGE;

    /* Send our OOB data */
    ccc_status_t ret = nfc_send((const uint8_t *)oob_out, sizeof(ccc_nfc_oob_data_t));
    if (ret != CCC_OK) return ret;

    /* Receive phone OOB data */
    uint16_t recv_len = 0;
    ret = nfc_recv((uint8_t *)oob_in, &recv_len, 2000);
    if (ret != CCC_OK) return ret;

    if (recv_len != sizeof(ccc_nfc_oob_data_t)) {
        return CCC_ERR_SECURITY;
    }

    g_nfc_state = NFC_STATE_IDLE;
    return CCC_OK;
}
