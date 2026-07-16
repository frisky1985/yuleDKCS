/**
 * @file ble_kw47a.c
 * @brief BLE Driver for NXP KW47A with Low Power Support
 */

#include "ccc_digital_key.h"
#include "../../../system_architecture/sys_time.h"

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

/* Global state — all IRQ-accessible globals are volatile to prevent
 * compiler reordering/caching across ISR boundaries [EMB-P1-01] */
static ble_power_state_e volatile g_ble_pwr_state = BLE_PWR_OFF;
static ble_lp_config_t g_ble_lp_cfg;
static uint16_t volatile g_conn_handle = 0xFFFF;
static bool volatile g_adv_active = false;
static bool volatile g_lp_mode = false;

/* Callbacks — may be invoked from ISR context, must be volatile */
static ble_recv_cb_t volatile    g_recv_cb    = NULL;
static ble_conn_cb_t volatile    g_conn_cb    = NULL;
static ble_disconn_cb_t volatile g_disconn_cb = NULL;
static void (* volatile g_uwb_wake_cb)(uwb_wake_reason_e reason) = NULL;

/* External dependencies */
extern int32_t spi_transfer(uint8_t dev, const uint8_t *tx, uint8_t *rx, uint16_t len);
extern void    gpio_write(uint8_t port, uint8_t pin, uint8_t val);
extern void    gpio_write_wake(uint8_t pin, uint8_t val);  /* UWB wake GPIO */
extern void    delay_ms(uint32_t ms);

/* ========================================================================
 *  [EMB-P1-10] BLE Bonding Cache — 限制大小防止资源耗尽
 * ======================================================================== */
#define MAX_BONDING_CACHE_ENTRIES  16  /* 最多缓存16个配对设备 */

typedef struct {
    uint8_t  addr[6];         /* 蓝牙设备地址 */
    uint8_t  addr_type;       /* 地址类型 */
    uint8_t  ltk[16];         /* 长期密钥 */
    uint8_t  irk[16];         /* 身份解析密钥 */
    uint32_t last_used;       /* 最后使用时间 (系统tick) */
    uint8_t  valid;           /* 是否有效 */
} bonding_cache_entry_t;

static bonding_cache_entry_t g_bonding_cache[MAX_BONDING_CACHE_ENTRIES];
static uint32_t g_bonding_count = 0;

/* [EMB-P1-10 FIX] Bonding 缓存清理: 移除最久未用的条目 */
static void bonding_cache_evict_lru(void)
{
    if (g_bonding_count == 0) return;

    uint32_t oldest_idx = 0;
    uint32_t oldest_time = g_bonding_cache[0].last_used;

    for (uint32_t i = 1; i < g_bonding_count; i++) {
        if (g_bonding_cache[i].last_used < oldest_time) {
            oldest_idx = i;
            oldest_time = g_bonding_cache[i].last_used;
        }
    }

    memset(&g_bonding_cache[oldest_idx], 0, sizeof(bonding_cache_entry_t));

    /* 压缩数组: 将后续元素前移 */
    for (uint32_t i = oldest_idx; i < g_bonding_count - 1; i++) {
        memcpy(&g_bonding_cache[i], &g_bonding_cache[i + 1], sizeof(bonding_cache_entry_t));
    }
    memset(&g_bonding_cache[g_bonding_count - 1], 0, sizeof(bonding_cache_entry_t));
    g_bonding_count--;
}

/* [EMB-P1-10 FIX] 添加 BLE bonding 条目 */
static ccc_status_t ble_bonding_cache_add(const uint8_t addr[6], uint8_t addr_type,
                                    const uint8_t ltk[16], const uint8_t irk[16])
{
    if (!addr || !ltk) return CCC_ERR_INVALID_PARAM;

    /* 检查是否已存在 */
    for (uint32_t i = 0; i < g_bonding_count; i++) {
        if (memcmp(g_bonding_cache[i].addr, addr, 6) == 0) {
            /* 更新已有条目 */
            memcpy(g_bonding_cache[i].ltk, ltk, 16);
            if (irk) memcpy(g_bonding_cache[i].irk, irk, 16);
            g_bonding_cache[i].addr_type = addr_type;
            g_bonding_cache[i].last_used = sys_tick_get_ms();
            g_bonding_cache[i].valid = 1;
            return CCC_OK;
        }
    }

    /* [EMB-P1-10 FIX] 检查缓存是否已满, 满则执行LRU淘汰 */
    if (g_bonding_count >= MAX_BONDING_CACHE_ENTRIES) {
        bonding_cache_evict_lru();
    }

    /* 添加新条目 */
    memcpy(g_bonding_cache[g_bonding_count].addr, addr, 6);
    g_bonding_cache[g_bonding_count].addr_type = addr_type;
    memcpy(g_bonding_cache[g_bonding_count].ltk, ltk, 16);
    if (irk) memcpy(g_bonding_cache[g_bonding_count].irk, irk, 16);
    g_bonding_cache[g_bonding_count].last_used = sys_tick_get_ms();
    g_bonding_cache[g_bonding_count].valid = 1;
    g_bonding_count++;

    return CCC_OK;
}

/* [EMB-P1-11] PAN ID 变化状态跟踪 */
#define PAN_ID_CACHE_SIZE  8
static struct {
    uint8_t  pan_id[4];       /* 上一次记录的 PAN ID */
    uint16_t conn_handle;     /* 关联的连接句柄 */
    uint32_t last_seen;       /* 最后更新时间 */
    uint8_t  valid;
} g_pan_id_cache[PAN_ID_CACHE_SIZE];

/* [EMB-P1-11 FIX] 检查 PAN ID 是否发生变化, 发生变化则触发重连 */
static ccc_status_t ble_check_pan_id_change(uint16_t conn_handle, const uint8_t new_pan_id[4])
{
    if (!new_pan_id) return CCC_ERR_INVALID_PARAM;

    for (int i = 0; i < PAN_ID_CACHE_SIZE; i++) {
        if (g_pan_id_cache[i].conn_handle == conn_handle && g_pan_id_cache[i].valid) {
            if (memcmp(g_pan_id_cache[i].pan_id, new_pan_id, 4) != 0) {
                /* [EMB-P1-11 FIX] PAN ID 变化: 断开当前连接, 触发重连 */
                uint16_t handle = conn_handle;
                ble_disconnect(handle);
                delay_ms(50);  /* 等待断开完成 */

                /* 更新为新 PAN ID */
                memcpy(g_pan_id_cache[i].pan_id, new_pan_id, 4);
                g_pan_id_cache[i].last_seen = sys_tick_get_ms();

                /* 重新开始广播以便重连 */
                ble_adv_param_t adv = {0};
                adv.interval_min = 100;
                adv.interval_max = 200;
                ble_start_adv(&adv);

                return CCC_OK;
            }
            return CCC_OK;
        }
    }

    /* 未找到记录: 添加新记录 */
    for (int i = 0; i < PAN_ID_CACHE_SIZE; i++) {
        if (!g_pan_id_cache[i].valid) {
            memcpy(g_pan_id_cache[i].pan_id, new_pan_id, 4);
            g_pan_id_cache[i].conn_handle = conn_handle;
            g_pan_id_cache[i].last_seen = sys_tick_get_ms();
            g_pan_id_cache[i].valid = 1;
            break;
        }
    }

    return CCC_OK;
}

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
    static uint8_t buf[244];
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
    static uint8_t evt_buf[260];
    uint8_t header[4] = {0};
    memset(evt_buf, 0, sizeof(evt_buf));

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
        /* [EMB-P1-11 FIX] 连接后检查 PAN ID 变化 (扩展数据从evt_buf中提取) */
        if (payload_len >= 10) {
            ble_check_pan_id_change(g_conn_handle, &evt_buf[2]);
        }
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

/* ========================================================================
 *  CCC GATT Service Registration — [P0-2]
 * ========================================================================
 * 注册 CCC Digital Key Service (UUID 0xFFD1) 及其 6 个 Characteristic。
 * 参考: CCC Digital Key 技术规范 R3.0 §4.3.1
 */

/* CCC Characteristic UUIDs (16-bit 自定义) */
#define CCC_CHAR_BOND_MGMT_UUID     0xFFD2   /**< 配对管理: 读/写/Notify */
#define CCC_CHAR_KEY_DELIVERY_UUID  0xFFD3   /**< 密钥下发: 写/Indicate */
#define CCC_CHAR_AUTH_CONTROL_UUID  0xFFD4   /**< 认证控制: 写/Notify */
#define CCC_CHAR_VEHICLE_STATUS_UUID 0xFFD5  /**< 车辆状态: 读/Notify */
#define CCC_CHAR_UWB_PARAMS_UUID    0xFFD6   /**< UWB 参数: 写/Indicate */
#define CCC_CHAR_RSSI_UUID          0xFFD7   /**< RSSI 数据: 读/Notify */

/* Characteristic 属性定义 (兼容 BLE 4.2 GATT) */
#define CHAR_PROP_READ       0x02
#define CHAR_PROP_WRITE      0x08
#define CHAR_PROP_WRITE_NO_RSP 0x04
#define CHAR_PROP_NOTIFY     0x10
#define CHAR_PROP_INDICATE   0x20

/* Characteristic 安全权限 */
#define CHAR_PERM_NONE       0x00
#define CHAR_PERM_READ_AUTH  0x01   /* 认证后读 */
#define CHAR_PERM_WRITE_AUTH 0x02   /* 认证后写 */
#define CHAR_PERM_READ_ENC   0x04   /* 加密读 */
#define CHAR_PERM_WRITE_ENC  0x08   /* 加密写 */

/**
 * @brief GATT Characteristic 描述符 (平台无关抽象)
 */
typedef struct {
    uint16_t uuid;              /**< 16-bit UUID */
    uint8_t  properties;        /**< BLE 属性 (read/write/notify/indicate) */
    uint8_t  perm_read;         /**< 读权限 */
    uint8_t  perm_write;        /**< 写权限 */
    uint16_t max_len;           /**< 最大值长度 */
    uint8_t  is_variable_len;   /**< 是否变长 */
} ccc_gatt_char_t;

/**
 * @brief GATT Service 描述符
 */
typedef struct {
    uint16_t          uuid;              /**< Service UUID */
    uint8_t           char_count;        /**< Characteristic 数量 */
    const ccc_gatt_char_t *chars;        /**< Characteristic 数组 */
} ccc_gatt_service_t;

/* CCC Digital Key Service 的 6 个 Characteristic 定义 */
static const ccc_gatt_char_t g_ccc_chars[] = {
    /* [0] CCC_CHAR_BOND_MGMT */
    {
        .uuid        = CCC_CHAR_BOND_MGMT_UUID,
        .properties  = CHAR_PROP_READ | CHAR_PROP_WRITE | CHAR_PROP_NOTIFY,
        .perm_read   = CHAR_PERM_READ_AUTH,
        .perm_write  = CHAR_PERM_WRITE_AUTH,
        .max_len     = 64,
        .is_variable_len = 1
    },
    /* [1] CCC_CHAR_KEY_DELIVERY */
    {
        .uuid        = CCC_CHAR_KEY_DELIVERY_UUID,
        .properties  = CHAR_PROP_WRITE | CHAR_PROP_INDICATE,
        .perm_read   = CHAR_PERM_NONE,
        .perm_write  = CHAR_PERM_WRITE_AUTH | CHAR_PERM_WRITE_ENC,
        .max_len     = 512,
        .is_variable_len = 1
    },
    /* [2] CCC_CHAR_AUTH_CONTROL */
    {
        .uuid        = CCC_CHAR_AUTH_CONTROL_UUID,
        .properties  = CHAR_PROP_WRITE | CHAR_PROP_NOTIFY,
        .perm_read   = CHAR_PERM_NONE,
        .perm_write  = CHAR_PERM_WRITE_AUTH,
        .max_len     = 64,
        .is_variable_len = 1
    },
    /* [3] CCC_CHAR_VEHICLE_STATUS */
    {
        .uuid        = CCC_CHAR_VEHICLE_STATUS_UUID,
        .properties  = CHAR_PROP_READ | CHAR_PROP_NOTIFY,
        .perm_read   = CHAR_PERM_READ_AUTH,
        .perm_write  = CHAR_PERM_NONE,
        .max_len     = 32,
        .is_variable_len = 0
    },
    /* [4] CCC_CHAR_UWB_PARAMS */
    {
        .uuid        = CCC_CHAR_UWB_PARAMS_UUID,
        .properties  = CHAR_PROP_WRITE | CHAR_PROP_INDICATE,
        .perm_read   = CHAR_PERM_NONE,
        .perm_write  = CHAR_PERM_WRITE_AUTH | CHAR_PERM_WRITE_ENC,
        .max_len     = 128,
        .is_variable_len = 1
    },
    /* [5] CCC_CHAR_RSSI */
    {
        .uuid        = CCC_CHAR_RSSI_UUID,
        .properties  = CHAR_PROP_READ | CHAR_PROP_NOTIFY,
        .perm_read   = CHAR_PERM_READ_AUTH,
        .perm_write  = CHAR_PERM_NONE,
        .max_len     = 4,
        .is_variable_len = 0
    }
};

/**
 * @brief 平台层回调: 通知 GATT Characteristic 值变化
 */
static void (*g_gatt_value_change_cb)(uint16_t char_uuid, const uint8_t *data, uint16_t len) = NULL;

/**
 * @brief 注册 CCC GATT Service 到 BLE 协议栈  — [P0-2]
 *
 * 实现步骤:
 *   1. 创建 CCC Digital Key Service (0xFFD1)
 *   2. 依次注册 6 个 Characteristic
 *   3. 配置每个 Characteristic 的读写回调
 *   4. 使能 Service (可见可连接)
 *
 * @return CCC_OK 成功, 否则错误码
 */
ccc_status_t ble_register_gatt_service(void)  /* [P0-2] */
{
    ccc_status_t ret;

    /* Step 1: 创建 Primary Service */
    /* 通过 SPI 命令通知 KW47A 创建 GATT Primary Service */
    uint8_t svc_decl[2];
    svc_decl[0] = (uint8_t)(CCC_SERVICE_UUID >> 8);
    svc_decl[1] = (uint8_t)(CCC_SERVICE_UUID & 0xFF);
    ret = kw47a_send_cmd(/* KW47A_CMD_ADD_SERVICE */ 0x50, svc_decl, 2);
    if (ret != CCC_OK) {
        return CCC_ERR_HARDWARE;
    }

    /* Step 2: 注册所有 Characteristic */
    for (uint8_t i = 0; i < sizeof(g_ccc_chars) / sizeof(g_ccc_chars[0]); i++) {
        uint8_t gatt_data[16];
        uint8_t idx = 0;

        /* Characteristic 声明: [properties(1)][uuid(2)] */
        gatt_data[idx++] = g_ccc_chars[i].properties;
        gatt_data[idx++] = (uint8_t)(g_ccc_chars[i].uuid >> 8);
        gatt_data[idx++] = (uint8_t)(g_ccc_chars[i].uuid & 0xFF);

        /* Characteristic 值配置: [max_len(2)][perms(2)] */
        gatt_data[idx++] = (uint8_t)(g_ccc_chars[i].max_len >> 8);
        gatt_data[idx++] = (uint8_t)(g_ccc_chars[i].max_len & 0xFF);
        gatt_data[idx++] = g_ccc_chars[i].perm_read;
        gatt_data[idx++] = g_ccc_chars[i].perm_write;

        ret = kw47a_send_cmd(/* KW47A_CMD_ADD_CHAR */ 0x51, gatt_data, idx);
        if (ret != CCC_OK) {
            return CCC_ERR_HARDWARE;
        }
    }

    /* Step 3: 启动 Service (使能特征) */
    uint8_t start_cmd[2];
    start_cmd[0] = (uint8_t)(CCC_SERVICE_UUID >> 8);
    start_cmd[1] = (uint8_t)(CCC_SERVICE_UUID & 0xFF);
    ret = kw47a_send_cmd(/* KW47A_CMD_START_SERVICE */ 0x52, start_cmd, 2);
    if (ret != CCC_OK) {
        return CCC_ERR_HARDWARE;
    }

    return CCC_OK;
}

/**
 * @brief 注册 GATT Characteristic 值变化回调 — [P0-2]
 */
ccc_status_t ble_register_gatt_value_change_cb(void (*cb)(uint16_t char_uuid,
                                                           const uint8_t *data,
                                                           uint16_t len))
{
    if (!cb) return CCC_ERR_INVALID_PARAM;
    g_gatt_value_change_cb = cb;
    return CCC_OK;
}

/**
 * @brief 通过 GATT Notify 上报 Characteristic 值变化 — [P0-2]
 */
ccc_status_t ble_gatt_notify(uint16_t char_uuid, const uint8_t *data, uint16_t len)
{
    if (!data || len == 0) return CCC_ERR_INVALID_PARAM;

    static uint8_t buf[260]; /* 固定最大: 8 + 252 */
    if ((uint16_t)(8 + len) > sizeof(buf)) return CCC_ERR_INVALID_PARAM;
    memset(buf, 0, sizeof(buf));
    buf[0] = (uint8_t)(char_uuid >> 8);
    buf[1] = (uint8_t)(char_uuid & 0xFF);
    buf[2] = (uint8_t)(len >> 8);
    buf[3] = (uint8_t)(len & 0xFF);
    memcpy(buf + 4, data, len);

    return kw47a_send_cmd(/* KW47A_CMD_GATT_NOTIFY */ 0x53, buf, 4 + len);
}