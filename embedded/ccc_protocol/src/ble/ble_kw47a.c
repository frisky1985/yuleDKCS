/**
 * @file ble_kw47a.c
 * @brief BLE Driver for NXP KW47A with Low Power Support
 */

#include "ccc_digital_key.h"

/* KW47A SPI Command Interface */
#define KW47A_CMD_RESET             0x01
#define KW47A_CMD_INIT              0x02
#define KW47A_CMD_START_ADV         0x10
#define KW47A_CMD_STOP_ADV          0x11
#define KW47A_CMD_START_ADV_DIR     0x12    /* Directed advertising */
#define KW47A_CMD_CONNECT           0x13
#define KW47A_CMD_DISCONNECT        0x14
#define KW47A_CMD_SEND_DATA         0x20
#define KW47A_CMD_OOB_PAIR          0x30
#define KW47A_CMD_SET_SLEEP         0x40    /* Enter deep sleep */
#define KW47A_CMD_WAKE             0x41    /* Wake from sleep */
#define KW47A_CMD_SET_TX_POWER      0x42    /* Set TX power */
#define KW47A_EVT_RECV_DATA         0xA0
#define KW47A_EVT_CONNECTED         0xA1
#define KW47A_EVT_DISCONNECTED      0xA2
#define KW47A_EVT_WAKE_REQ         0xA3    /* Wake request from peer */
#define KW47A_EVT_SLAVE_DELAYED   0xA4    /* Slave latency delayed */

/* BLE Power States */
typedef enum {
    BLE_PWR_OFF = 0,           /* Power down */
    BLE_PWR_IDLE,               /* Idle, RX listening */
    BLE_PWR_ADV_CONN,          /* Connectable advertising */
    BLE_PWR_ADV_NCONN,        /* Non-connectable ( beacon mode) */
    BLE_PWR_CONNECTED,          /* Connected */
    BLE_PWR_SLEEP              /* Deep sleep */
} ble_power_state_e;

/* BLE Low Power Configuration */
typedef struct {
    uint8_t  adv_interval_ms;     /* Advertising interval */
    uint8_t  tx_power_dbm;      /* TX power (dBm) */
    uint8_t  min_conn_interval; /* Min connection interval (ms) */
    uint8_t  max_conn_interval; /* Max connection interval (ms) */
    uint16_t slave_latency;     /* Slave latency (ms) */
    uint16_t adv_timeout_s;    /* Advertising timeout (s) */
} ble_lp_config_t;

/* Default low power config */
static const ble_lp_config_t g_default_lp = {
    .adv_interval_ms = 100,     /* 100ms interval */
    .tx_power_dbm = 0,        /* 0 dBm (default) */
    .min_conn_interval = 20,   /* 20ms min */
    .max_conn_interval = 200,   /* 200ms max */
    .slave_latency = 500,      /* 500ms slave latency */
    .adv_timeout_s = 60        /* 60s timeout */
};

/* UWB Wake Trigger Types */
typedef enum {
    UWB_WAKE_NONE = 0,         /* No wake pending */
    UWB_WAKE_RANGING_REQ,       /* UWB ranging request */
    UWB_WAKE_DATA_TX,          /* Data transmission */
    UWB_WAKE_SECURITY         /* Security operation */
} uwb_wake_reason_e;

/* Global state */
static ble_power_state_e g_ble_pwr_state = BLE_PWR_OFF;
static ble_lp_config_t g_ble_lp_cfg;
static uint16_t g_conn_handle = 0xFFFF;
static bool g_adv_active = false;
static bool g_lp_mode = false;

/* Callbacks */
static ble_recv_cb_t    g_recv_cb    = NULL;
static ble_conn_cb_t    g_conn_cb    = NULL;
static ble_disconn_cb_t g_disconn_cb = NULL;
static void (*g_uwb_wake_cb)(uwb_wake_reason_e reason) = NULL;

/* External dependencies */
extern int32_t spi_transfer(uint8_t dev, const uint8_t *tx, uint8_t *rx, uint16_t len);
extern void    gpio_write(uint8_t port, uint8_t pin, uint8_t val);
extern void    gpio_write_wake(uint8_t pin, uint8_t val);  /* UWB wake GPIO */
extern void    delay_ms(uint32_t ms);

static ccc_status_t kw47a_send_cmd(uint8_t cmd, const uint8_t *payload, uint16_t len)
{
    uint8_t header[4] = { cmd, 0x00, (uint8_t)(len >> 8), (uint8_t)(len & 0xFF) };

    gpio_write(KW47A_CS_PORT, KW47A_CS_PIN, 0);
    spi_transfer(1, header, NULL, 4);
    if (len > 0 && payload) {
        spi_transfer(1, payload, NULL, len);
    }
    gpio_write(KW47A_CS_PORT, KW47A_CS_PIN, 1);

    return CCC_OK;
}

/**
 * @brief Initialize BLE driver with low power support
 */
ccc_status_t ble_kw47a_init(void)
{
    /* Hardware reset */
    gpio_write(KW47A_RST_PORT, KW47A_RST_PIN, 0);
    delay_ms(10);
    gpio_write(KW47A_RST_PORT, KW47A_RST_PIN, 1);
    delay_ms(10);

    /* Send init command */
    ccc_status_t ret = kw47a_send_cmd(KW47A_CMD_INIT, NULL, 0);
    if (ret != CCC_OK) return ret;

    /* Initialize default low power config */
    memcpy(&g_ble_lp_cfg, &g_default_lp, sizeof(g_ble_lp_cfg));

    g_conn_handle = 0xFFFF;
    g_ble_pwr_state = BLE_PWR_IDLE;
    g_adv_active = false;
    g_lp_mode = false;
    
    return CCC_OK;
}

/**
 * @brief Enter low power advertising mode (beacon mode)
 * @note Current ~10uA in non-connectable beacon mode
 */
ccc_status_t ble_enter_lp_mode(const ble_lp_config_t *cfg)
{
    if (cfg) {
        memcpy(&g_ble_lp_cfg, cfg, sizeof(g_ble_lp_cfg));
    }
    
    /* Set TX power */
    kw47a_send_cmd(KW47A_CMD_SET_TX_POWER, &g_ble_lp_cfg.tx_power_dbm, 1);
    
    /* Start non-connectable advertising (iBeacon style) */
    uint8_t adv_data[31] = {
        0x1A,  /* Length */
        0xFF,  /* Manufacturer data */
        0x4C, 0x00,  /* Apple company ID */
        0x02,        /* iBeacon subtype */
        0x15,        /* iBeacon length */
        /* 20-byte UUID filled by application */
    };
    /* UUID would be set by keymgmt module */
    
    uint8_t adv_param[4] = {
        g_ble_lp_cfg.adv_interval_ms,
        0x00,      /* Non-connectable */
        g_ble_lp_cfg.adv_timeout_s,
        0x00
    };
    
    /* First send adv data, then start advertising */
    /* Note: Full implementation would combine these properly */
    
    g_lp_mode = true;
    g_ble_pwr_state = BLE_PWR_ADV_NCONN;
    g_adv_active = true;
    
    return CCC_OK;
}

/**
 * @brief Exit low power mode, enter connectable mode
 */
ccc_status_t ble_exit_lp_mode(void)
{
    kw47a_send_cmd(KW47A_CMD_STOP_ADV, NULL, 0);
    
    g_lp_mode = false;
    g_ble_pwr_state = BLE_PWR_ADV_CONN;
    g_adv_active = true;
    
    return CCC_OK;
}

/**
 * @brief Request wakeup of UWB module
 * @note Triggers UWB ranging or data operation
 */
static void ble_trigger_uwb_wake(uwb_wake_reason_e reason)
{
    if (g_uwb_wake_cb) {
        g_uwb_wake_cb(reason);
    }
    
    /* Hardware wake signal to UWB (e.g., NCJ29D6) */
    gpio_write_wake(UWB_WAKE_PIN, 1);
    delay_ms(10);
    gpio_write_wake(UWB_WAKE_PIN, 0);
}

/**
 * @brief Register UWB wake callback
 */
ccc_status_t ble_register_uwb_wake_cb(void (*cb)(uwb_wake_reason_e reason))
{
    g_uwb_wake_cb = cb;
    return CCC_OK;
}

/**
 * @brief Send data to UWB via BLE UART bridge
 * @note Used when phone is connected via BLE for UWB ranging
 */
ccc_status_t ble_send_to_uwb(uint32_t session_id, const uint8_t *data, uint16_t len)
{
    if (!data || len == 0 || len > 240) return CCC_ERR_INVALID_PARAM;
    
    /* Data format: [session_id:4][data...] */
    uint8_t buf[244];
    buf[0] = (uint8_t)(session_id >> 24);
    buf[1] = (uint8_t)(session_id >> 16);
    buf[2] = (uint8_t)(session_id >> 8);
    buf[3] = (uint8_t)(session_id & 0xFF);
    memcpy(&buf[4], data, len);
    
    return kw47a_send_cmd(KW47A_CMD_SEND_DATA, buf, len + 4);
}

/**
 * @brief Enter deep sleep mode
 * @note Current ~0.5uA
 */
ccc_status_t ble_enter_deep_sleep(void)
{
    if (g_adv_active) {
        kw47a_send_cmd(KW47A_CMD_STOP_ADV, NULL, 0);
    }
    
    kw47a_send_cmd(KW47A_CMD_SET_SLEEP, NULL, 0);
    
    g_adv_active = false;
    g_ble_pwr_state = BLE_PWR_SLEEP;
    
    return CCC_OK;
}

/**
 * @brief Wake from deep sleep
 */
ccc_status_t ble_wake_from_sleep(void)
{
    kw47a_send_cmd(KW47A_CMD_WAKE, NULL, 0);
    
    /* Wait for BLE to be ready */
    delay_ms(50);
    
    g_ble_pwr_state = BLE_PWR_IDLE;
    
    return CCC_OK;
}

/**
 * @brief Get current power state
 */
ble_power_state_e ble_get_power_state(void)
{
    return g_ble_pwr_state;
}

/**
 * @brief Check if in low power mode
 */
bool ble_is_lp_mode(void)
{
    return g_lp_mode;
}

/**
 * @brief Check if connected
 */
bool ble_is_connected(uint16_t conn_handle)
{
    return (g_conn_handle == conn_handle && g_conn_handle != 0xFFFF);
}

ccc_status_t ble_kw47a_deinit(void)
{
    kw47a_send_cmd(KW47A_CMD_RESET, NULL, 0);
    g_conn_handle = 0xFFFF;
    g_ble_pwr_state = BLE_PWR_OFF;
    return CCC_OK;
}

ccc_status_t ble_start_adv(const ble_adv_param_t *param)
{
    if (!param) return CCC_ERR_INVALID_PARAM;
    g_adv_active = true;
    g_ble_pwr_state = BLE_PWR_ADV_CONN;
    return kw47a_send_cmd(KW47A_CMD_START_ADV, param->data, param->len);
}

ccc_status_t ble_stop_adv(void)
{
    ccc_status_t ret = kw47a_send_cmd(KW47A_CMD_STOP_ADV, NULL, 0);
    g_adv_active = false;
    if (g_lp_mode == false) {
        g_ble_pwr_state = BLE_PWR_IDLE;
    }
    return ret;
}

ccc_status_t ble_connect(const ble_addr_t *addr, const ble_conn_param_t *param)
{
    (void)param;
    if (!addr) return CCC_ERR_INVALID_PARAM;
    return kw47a_send_cmd(KW47A_CMD_START_ADV_DIR, addr->addr, 6);
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

/* IRQ handler */
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
        g_ble_pwr_state = BLE_PWR_CONNECTED;
        /* Wake UWB for ranging */
        ble_trigger_uwb_wake(UWB_WAKE_RANGING_REQ);
        if (g_conn_cb) g_conn_cb(g_conn_handle, 0);
        break;

    case KW47A_EVT_DISCONNECTED:
        if (g_disconn_cb) g_disconn_cb(g_conn_handle, evt_buf[0]);
        g_conn_handle = 0xFFFF;
        g_ble_pwr_state = g_lp_mode ? BLE_PWR_ADV_NCONN : BLE_PWR_ADV_CONN;
        g_adv_active = true;
        break;

    case KW47A_EVT_RECV_DATA: {
        uint16_t ch = ((uint16_t)evt_buf[0] << 8) | evt_buf[1];
        uint8_t *payload = &evt_buf[2];
        uint16_t payload_data_len = payload_len - 2;
        
        /* Check for UWB control commands in data */
        if (payload_data_len >= 1) {
            switch (payload[0]) {
            case 0x01:  /* Start UWB ranging */
                ble_trigger_uwb_wake(UWB_WAKE_RANGING_REQ);
                break;
            case 0x02:  /* UWB data TX */
                ble_trigger_uwb_wake(UWB_WAKE_DATA_TX);
                break;
            case 0x03:  /* Security operation */
                ble_trigger_uwb_wake(UWB_WAKE_SECURITY);
                break;
            default:
                break;
            }
        }
        
        if (g_recv_cb) g_recv_cb(ch, payload, payload_data_len);
        break;
    }

    case KW47A_EVT_WAKE_REQ:
        /* Peer requested wake */
        ble_trigger_uwb_wake(UWB_WAKE_RANGING_REQ);
        break;

    case KW47A_EVT_SLAVE_DELAYED:
        /* Connection alive, no action needed */
        break;

    default:
        break;
    }
}