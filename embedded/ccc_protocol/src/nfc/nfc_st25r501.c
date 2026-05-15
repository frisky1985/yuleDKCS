/**
 * @file nfc_st25r501.c
 * @brief NFC Driver for ST ST25R501 with LPCD Support
 */

#include "ccc_digital_key.h"

/* ST25R501 Register Map */
#define ST25R501_REG_ID             0x01
#define ST25R501_REG_IO_CFG1        0x02
#define ST25R501_REG_OP_CTRL        0x00
#define ST25R501_REG_MODE_DEF       0x04
#define ST25R501_REG_FIELD_ON       0x10
#define ST25R501_REG_FIELD_OFF      0x11
#define ST25R501_REG_AUX_DISPLAY    0x18
#define ST25R501_REG_RX_START      0x19
#define ST25R501_REG_TX_CRC        0x23
#define ST25R501_REG_NFCIP1        0x24

/* LPCD Registers (ST25R501 specific) */
#define ST25R501_REG_LPCD_CFG        0x40    /* LPCD Configuration */
#define ST25R501_REG_LPCD_TIMER     0x41    /* LPCD Timing */
#define ST25R501_REG_LPCD_STATUS   0x42    /* LPCD Status */
#define ST25R501_REG_RF_UPPER       0x43    /* RF field upper threshold */
#define ST25R501_REG_RF_LOWER       0x44    /* RF field lower threshold */
#define ST25R501_REG_EMD_SUP        0x45    /* EMD suppression */

/* SPI commands */
#define ST25R501_CMD_READ           0x02
#define ST25R501_CMD_WRITE          0x01
#define ST25R501_CMD_FIFO_READ      0x03
#define ST25R501_CMD_FIFO_WRITE    0x04
#define ST25R501_CMD_SET_RF        0x05
#define ST25R501_CMD_REQ_RF         0x06

/* LPCD Configuration Bits */
#define ST25R501_LPCD_EN            0x01    /* LPCD Enable */
#define ST25R501_LPCD_AUTO_WAKE    0x02    /* Auto wake on field detect */
#define ST25R501_LPCD_RSSI_MEAS    0x04    /* RSSI measurement enable */
#define ST25R501_LPCD_DUAL         0x08    /* Dual interval mode */

/* NFC Power States */
typedef enum {
    NFC_PWR_OFF = 0,        /* Power down */
    NFC_PWR_LPCD,           /* Low Power Card Detection (~10uA) */
    NFC_PWR_LISTEN,         /* Listen mode (~5mA) */
    NFC_PWR_ACTIVE          /* Full power active (~50mA) */
} nfc_power_state_e;

/* LPCD Configuration */
typedef struct {
    uint8_t  enable;            /* LPCD enable */
    uint8_t  poll_interval_ms;   /* Poll interval (16-256ms) */
    uint8_t  sense_dur_ms;      /* Sense duration (1-15ms) */
    uint8_t  rssi_thresh;       /* RSSI threshold */
    uint16_t field_upper_mv;    /* Field upper threshold */
    uint16_t field_lower_mv;    /* Field lower threshold */
} nfc_lpcd_config_t;

/* Default LPCD config */
static const nfc_lpcd_config_t g_default_lpcd = {
    .enable = 1,
    .poll_interval_ms = 64,     /* 64ms poll interval */
    .sense_dur_ms = 4,         /* 4ms sense duration */
    .rssi_thresh = 20,         /* RSSI threshold */
    .field_upper_mv = 250,     /* 250mV upper */
    .field_lower_mv = 50       /* 50mV lower */
};

/* Global state */
static nfc_state_e g_nfc_state = NFC_STATE_IDLE;
static nfc_power_state_e g_nfc_pwr_state = NFC_PWR_OFF;
static nfc_lpcd_config_t g_lpcd_cfg;
static bool g_lpcd_active = false;

/* Internal helpers */
extern int32_t spi_transfer(uint8_t dev, const uint8_t *tx, uint8_t *rx, uint16_t len);
extern void    gpio_write(uint8_t port, uint8_t pin, uint8_t val);
extern uint8_t gpio_read(uint8_t port, uint8_t pin);
extern void    delay_ms(uint32_t ms);

static int32_t st25r501_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t tx[3] = { ST25R501_CMD_WRITE, reg, val };
    return spi_transfer(2, tx, NULL, 3);
}

static int32_t st25r501_read_reg(uint8_t reg, uint8_t *val)
{
    uint8_t tx[2] = { ST25R501_CMD_READ, reg };
    uint8_t rx[3] = {0};
    int32_t ret = spi_transfer(2, tx, rx, 3);
    if (ret == 0 && val) *val = rx[2];
    return ret;
}

static int32_t st25r501_write_reg16(uint8_t reg, uint16_t val)
{
    st25r501_write_reg(reg, (uint8_t)(val >> 8));
    return st25r501_write_reg(reg + 1, (uint8_t)(val & 0xFF));
}

/**
 * @brief Initialize NFC driver with LPCD support
 */
ccc_status_t nfc_st25r501_init(void)
{
    /* Hardware reset */
    gpio_write(ST25R501_RST_PORT, ST25R501_RST_PIN, 0);
    delay_ms(10);
    gpio_write(ST25R501_RST_PORT, ST25R501_RST_PIN, 1);
    delay_ms(10);

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

    /* Initialize default LPCD config */
    memcpy(&g_lpcd_cfg, &g_default_lpcd, sizeof(g_lpcd_cfg));

    g_nfc_state = NFC_STATE_IDLE;
    g_nfc_pwr_state = NFC_PWR_OFF;
    g_lpcd_active = false;
    
    return CCC_OK;
}

/**
 * @brief Enter Low Power Card Detection mode
 * @note Average current ~10uA with 64ms polling interval
 */
ccc_status_t nfc_enter_lpcd(const nfc_lpcd_config_t *cfg)
{
    if (cfg) {
        memcpy(&g_lpcd_cfg, cfg, sizeof(g_lpcd_cfg));
    }
    
    /* Configure LPCD timing */
    uint8_t timing = ((g_lpcd_cfg.poll_interval_ms / 16) << 4) | 
                    g_lpcd_cfg.sense_dur_ms;
    st25r501_write_reg(ST25R501_REG_LPCD_TIMER, timing);
    
    /* Configure field thresholds */
    st25r501_write_reg16(ST25R501_REG_RF_UPPER, g_lpcd_cfg.field_upper_mv * 4);
    st25r501_write_reg16(ST25R501_REG_RF_LOWER, g_lpcd_cfg.field_lower_mv * 4);
    
    /* Configure EMD suppression for noise immunity */
    st25r501_write_reg(ST25R501_REG_EMD_SUP, 0x03);  /* Enable EMD suppression */
    
    /* Enable LPCD mode */
    st25r501_write_reg(ST25R501_REG_LPCD_CFG, ST25R501_LPCD_EN | 
                     ST25R501_LPCD_AUTO_WAKE);
    
    g_lpcd_active = true;
    g_nfc_pwr_state = NFC_PWR_LPCD;
    g_nfc_state = NFC_STATE_FIELD_DETECT;
    
    return CCC_OK;
}

/**
 * @brief Exit LPCD mode, enter full power
 */
ccc_status_t nfc_exit_lpcd(void)
{
    /* Disable LPCD */
    st25r501_write_reg(ST25R501_REG_LPCD_CFG, 0x00);
    
    /* Resume normal NFC-F operation */
    st25r501_write_reg(ST25R501_REG_MODE_DEF, 0x03);
    st25r501_write_reg(ST25R501_REG_FIELD_ON, 0x01);
    
    g_lpcd_active = false;
    g_nfc_pwr_state = NFC_PWR_ACTIVE;
    g_nfc_state = NFC_STATE_FIELD_DETECT;
    
    return CCC_OK;
}

/**
 * @brief Check if field detected in LPCD mode
 * @note Call periodically in LPCD poll loop
 * @return true if NFC field detected
 */
bool nfc_lpcd_field_detect(void)
{
    uint8_t status = 0;
    st25r501_read_reg(ST25R501_REG_LPCD_STATUS, &status);
    return (status & 0x01) != 0;  /* Bit 0: Field detected */
}

/**
 * @brief Get current RSSI in LPCD mode
 * @return RSSI value (0-63)
 */
uint8_t nfc_lpcd_get_rssi(void)
{
    uint8_t rssi = 0;
    st25r501_read_reg(ST25R501_REG_LPCD_STATUS, &rssi);
    return (rssi >> 1) & 0x3F;
}

/**
 * @brief Power down NFC
 */
ccc_status_t nfc_power_down(void)
{
    st25r501_write_reg(ST25R501_REG_LPCD_CFG, 0x00);
    st25r501_write_reg(ST25R501_REG_FIELD_OFF, 0x00);
    
    g_lpcd_active = false;
    g_nfc_pwr_state = NFC_PWR_OFF;
    g_nfc_state = NFC_STATE_IDLE;
    
    return CCC_OK;
}

/**
 * @brief Get power state
 */
nfc_power_state_e nfc_get_power_state(void)
{
    return g_nfc_pwr_state;
}

/**
 * @brief Check if in LPCD mode
 */
bool nfc_is_lpcd_active(void)
{
    return g_lpcd_active;
}

ccc_status_t nfc_st25r501_deinit(void)
{
    nfc_power_down();
    return CCC_OK;
}

bool nfc_field_detect(void)
{
    if (g_lpcd_active) {
        return nfc_lpcd_field_detect();
    }
    return gpio_read(ST25R501_IRQ_PORT, ST25R501_IRQ_PIN) != 0;
}

nfc_state_e nfc_get_state(void)
{
    return g_nfc_state;
}

ccc_status_t nfc_start_listen(void)
{
    if (g_lpcd_active) {
        nfc_exit_lpcd();
    }
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
    if (g_lpcd_active) return CCC_ERR_BUSY;

    uint8_t header[2] = { ST25R501_CMD_FIFO_WRITE, (uint8_t)len };
    spi_transfer(2, header, NULL, 2);
    spi_transfer(2, data, NULL, len);

    return CCC_OK;
}

ccc_status_t nfc_recv(uint8_t *buf, uint16_t *len, uint32_t timeout_ms)
{
    if (!buf || !len) return CCC_ERR_INVALID_PARAM;
    if (g_lpcd_active) return CCC_ERR_BUSY;

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
    if (g_lpcd_active) return CCC_ERR_BUSY;

    g_nfc_state = NFC_STATE_DATA_EXCHANGE;

    ccc_status_t ret = nfc_send((const uint8_t *)oob_out, sizeof(ccc_nfc_oob_data_t));
    if (ret != CCC_OK) return ret;

    uint16_t recv_len = 0;
    ret = nfc_recv((uint8_t *)oob_in, &recv_len, 2000);
    if (ret != CCC_OK) return ret;

    if (recv_len != sizeof(ccc_nfc_oob_data_t)) {
        return CCC_ERR_SECURITY;
    }

    g_nfc_state = NFC_STATE_IDLE;
    return CCC_OK;
}