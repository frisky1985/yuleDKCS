/**
 * @file iccoa_dk40.c
 * @brief ICCOA DK 4.0 Protocol Implementation - UWB Enhanced Digital Key
 * @version 1.0
 * @date 2026-05-08
 *
 * DK 4.0 特性:
 * - UWB 安全测距 (FiRa 兼容)
 * - BLE 数据通道
 * - Session Token 认证
 * - HMAC-SHA256 消息完整性
 * - 多设备并发支持
 */

#include "iccoa_digital_key.h"
#include <stdlib.h>

/* ========================================================================
 *  Constants
 * ======================================================================== */
#define DK40_VERSION        0x01
#define DK40_FLAG_ENCRYPT   (1 << 0)
#define DK40_FLAG_UWB       (1 << 1)
#define DK40_FLAG_RESPONSE  (1 << 2)
#define DK40_FLAG_ERROR     (1 << 3)

#define DK40_MAX_SESSIONS   4
#define DK40_HMAC_LEN       16
#define DK40_SESSION_TK_LEN 4
#define DK40_NONCE_LEN      12

/* ========================================================================
 *  Types
 * ======================================================================== */

/**
 * @brief UWB 测距配置
 */
typedef struct {
    uint8_t  channel;           /**< UWB 通道 (5/6/8/9) */
    uint8_t  preamble_code;     /**< 前导码索引 */
    uint16_t slot_duration;     /**< 时隙时长 (ms) */
    uint16_t slots_per_round;   /**< 每轮测距时隙数 */
    uint8_t  responder_num;     /**< 响应者数量 */
    uint8_t  ranging_mode;      /**< 0=SS-TWR, 1=DS-TWR */
    int16_t  distance_min;      /**< 最小距离阈值 (cm) */
    int16_t  distance_max;      /**< 最大距离阈值 (cm) */
} dk40_uwb_config_t;

/**
 * @brief 会话状态
 */
typedef enum {
    SESSION_STATE_IDLE = 0,
    SESSION_STATE_BIND_PENDING,
    SESSION_STATE_AUTHENTICATED,
    SESSION_STATE_RANGING_ACTIVE,
    SESSION_STATE_EXPIRED
} dk40_session_state_e;

/**
 * @brief 活动会话
 */
typedef struct {
    uint8_t  token[DK40_SESSION_TK_LEN];
    uint8_t  peer_id[16];           /**< 手机设备 ID */
    uint32_t last_activity;
    dk40_session_state_e state;
    uint16_t last_msg_id;
    uint8_t  permissions;
    uint8_t  uwb_session_id;
    int16_t  last_distance;         /**< 上次测距结果 (cm) */
    uint8_t  zone;                  /**< 当前区域 (INSIDE/UNLOCK/APPROACH) */
} dk40_session_t;

/**
 * @brief 密钥管理上下文
 */
typedef struct {
    uint8_t  master_key[32];        /**< 主密钥 (SE050 保护) */
    uint8_t  session_key[32];       /**< 当前会话密钥 */
    uint8_t  hmac_key[32];          /**< HMAC 密钥 */
    uint32_t key_id;
} dk40_key_ctx_t;

/* ========================================================================
 *  Globals
 * ======================================================================== */
static dk40_session_t g_sessions[DK40_MAX_SESSIONS];
static dk40_uwb_config_t g_uwb_cfg;
static dk40_key_ctx_t g_key_ctx;
static uint16_t g_msg_id_counter = 0;
static bool g_initialized = false;

/* ========================================================================
 *  Internal Functions - HMAC
 * ======================================================================== */

/**
 * @brief 计算 HMAC-SHA256 截断到 16 字节
 */
static void dk40_compute_hmac(const uint8_t *data, uint16_t len, uint8_t *hmac_out)
{
    /* 外部实现: SE050 HMAC-SHA256 硬件加速 */
    extern int se050_hmac_sha256(const uint8_t *key, uint16_t key_len,
                                  const uint8_t *data, uint16_t data_len,
                                  uint8_t *mac_out);

    se050_hmac_sha256(g_key_ctx.hmac_key, 32, data, len, hmac_out);
    /* 截断到 16 字节 */
}

/**
 * @brief 验证接收帧 HMAC
 */
static int32_t dk40_verify_hmac(const iccoa_dk40_frame_t *frame)
{
    uint8_t computed_hmac[32];
    uint16_t data_len = 12 + frame->payload_len; /* magic 到 payload 结束 */

    dk40_compute_hmac((const uint8_t *)frame, data_len, computed_hmac);

    if (memcmp(frame->hmac, computed_hmac, DK40_HMAC_LEN) != 0) {
        return ICCOA_ERR_SECURITY;
    }
    return ICCOA_OK;
}

/* ========================================================================
 *  Internal Functions - Session Management
 * ======================================================================== */

/**
 * @brief 查找会话
 */
static int32_t dk40_find_session(const uint8_t *token)
{
    for (int i = 0; i < DK40_MAX_SESSIONS; i++) {
        if (g_sessions[i].state != SESSION_STATE_IDLE &&
            memcmp(g_sessions[i].token, token, DK40_SESSION_TK_LEN) == 0) {
            return i;
        }
    }
    return ICCOA_ERR_NOT_FOUND;
}

/**
 * @brief 分配新会话
 */
static int32_t dk40_alloc_session(const uint8_t *peer_id)
{
    for (int i = 0; i < DK40_MAX_SESSIONS; i++) {
        if (g_sessions[i].state == SESSION_STATE_IDLE) {
            /* 生成会话 token */
            extern int se050_rng(uint8_t *out, uint16_t len);
            se050_rng(g_sessions[i].token, DK40_SESSION_TK_LEN);

            memcpy(g_sessions[i].peer_id, peer_id, 16);
            g_sessions[i].state = SESSION_STATE_BIND_PENDING;
            g_sessions[i].last_activity = 0; /* TODO: 获取时间戳 */
            g_sessions[i].last_msg_id = 0;
            g_sessions[i].permissions = 0;
            g_sessions[i].last_distance = -1;
            g_sessions[i].zone = 0;
            return i;
        }
    }
    return ICCOA_ERR_NO_MEM;
}

/**
 * @brief 释放会话
 */
static void dk40_free_session(int idx)
{
    if (idx >= 0 && idx < DK40_MAX_SESSIONS) {
        memset(&g_sessions[idx], 0, sizeof(dk40_session_t));
        g_sessions[idx].state = SESSION_STATE_IDLE;
    }
}

/* ========================================================================
 *  Internal Functions - UWB Integration
 * ======================================================================== */

/**
 * @brief 启动 UWB 测距会话
 */
static int32_t dk40_start_uwb_ranging(uint8_t session_idx, const dk40_uwb_config_t *cfg)
{
    extern int uwb_ncj29d6_start_ranging(uint8_t channel, uint8_t mode);
    extern int uwb_ncj29d6_set_threshold(int16_t min, int16_t max);

    int ret = uwb_ncj29d6_start_ranging(cfg->channel, cfg->ranging_mode);
    if (ret != 0) return ICCOA_ERR_HARDWARE;

    uwb_ncj29d6_set_threshold(cfg->distance_min, cfg->distance_max);

    g_sessions[session_idx].uwb_session_id = session_idx;
    g_sessions[session_idx].state = SESSION_STATE_RANGING_ACTIVE;

    return ICCOA_OK;
}

/**
 * @brief 处理 UWB 测距结果回调
 */
void dk40_uwb_result_cb(uint8_t session_id, int16_t distance_cm, uint8_t zone)
{
    if (session_id < DK40_MAX_SESSIONS) {
        g_sessions[session_id].last_distance = distance_cm;
        g_sessions[session_id].zone = zone;

        /* 触发状态通知 */
        uint8_t notify[8] = {0};
        notify[0] = ICCOA_V4_NOTIFY;
        notify[1] = zone;
        *(int16_t *)(notify + 2) = distance_cm;
        iccoa_dk40_send_response(ICCOA_V4_NOTIFY, notify, 4);
    }
}

/* ========================================================================
 *  Protocol Handlers
 * ======================================================================== */

/**
 * @brief 处理会话打开请求
 */
static int32_t handle_session_open(const iccoa_dk40_frame_t *req, uint8_t *rsp_payload, uint16_t *rsp_len)
{
    /* Payload: [peer_id(16)] + [device_type(1)] + [capabilities(4)] */
    if (req->payload_len < 21) return ICCOA_ERR_PARAM;

    const uint8_t *peer_id = req->payload;
    int idx = dk40_alloc_session(peer_id);
    if (idx < 0) return idx;

    /* Response: [session_token(4)] + [server_nonce(12)] */
    memcpy(rsp_payload, g_sessions[idx].token, DK40_SESSION_TK_LEN);
    extern int se050_rng(uint8_t *out, uint16_t len);
    se050_rng(rsp_payload + 4, DK40_NONCE_LEN);
    *rsp_len = 4 + DK40_NONCE_LEN;

    return ICCOA_OK;
}

/**
 * @brief 处理绑定请求
 */
static int32_t handle_bind(const iccoa_dk40_frame_t *req, uint8_t *rsp_payload, uint16_t *rsp_len)
{
    int idx = dk40_find_session(req->session_token);
    if (idx < 0) return ICCOA_ERR_DENIED;

    /* Payload: [public_key(64)] + [signature(72)] + [permissions(1)] */
    if (req->payload_len < 137) return ICCOA_ERR_PARAM;

    const uint8_t *pub_key = req->payload;
    const uint8_t *sig = req->payload + 64;
    uint8_t requested_perm = req->payload[136];

    /* 验证签名 (SE050) */
    extern int se050_verify_ecdsa(const uint8_t *pub_key, const uint8_t *hash,
                                   const uint8_t *sig, uint16_t sig_len);
    /* TODO: 计算 hash = SHA256(peer_id || pub_key || session_token) */

    /* 存储用户公钥和权限 */
    g_sessions[idx].permissions = requested_perm;
    g_sessions[idx].state = SESSION_STATE_AUTHENTICATED;

    /* Response: [result(1)] + [vehicle_pub_key(64)] */
    rsp_payload[0] = 0x00; /* 成功 */
    /* TODO: 填充车辆公钥 */
    *rsp_len = 65;

    return ICCOA_OK;
}

/**
 * @brief 处理认证请求
 */
static int32_t handle_auth(const iccoa_dk40_frame_t *req, uint8_t *rsp_payload, uint16_t *rsp_len)
{
    int idx = dk40_find_session(req->session_token);
    if (idx < 0) return ICCOA_ERR_DENIED;

    /* Payload: [challenge_response(32)] */
    if (req->payload_len < 32) return ICCOA_ERR_PARAM;

    /* 验证挑战响应 */
    int32_t ret = iccoa_auth_verify(req->payload, req->payload_len);
    if (ret != ICCOA_OK) {
        rsp_payload[0] = 0x01; /* 认证失败 */
        *rsp_len = 1;
        return ICCOA_OK;
    }

    g_sessions[idx].state = SESSION_STATE_AUTHENTICATED;
    rsp_payload[0] = 0x00;
    *rsp_len = 1;

    return ICCOA_OK;
}

/**
 * @brief 处理车辆控制请求
 */
static int32_t handle_ctrl(const iccoa_dk40_frame_t *req, uint8_t *rsp_payload, uint16_t *rsp_len)
{
    int idx = dk40_find_session(req->session_token);
    if (idx < 0) return ICCOA_ERR_DENIED;

    if (g_sessions[idx].state != SESSION_STATE_AUTHENTICATED &&
        g_sessions[idx].state != SESSION_STATE_RANGING_ACTIVE) {
        return ICCOA_ERR_DENIED;
    }

    /* Payload: [ctrl_cmd(1)] + [param(1)] */
    if (req->payload_len < 2) return ICCOA_ERR_PARAM;

    iccoa_ctrl_cmd_e cmd = (iccoa_ctrl_cmd_e)req->payload[0];
    uint8_t param = req->payload[1];

    /* 权限检查 */
    if (cmd == CTRL_UNLOCK || cmd == CTRL_LOCK) {
        if (!(g_sessions[idx].permissions & ICCOA_PERM_UNLOCK)) {
            rsp_payload[0] = 0x02; /* 无权限 */
            *rsp_len = 1;
            return ICCOA_OK;
        }
    }
    if (cmd == CTRL_ENGINE_ON || cmd == CTRL_ENGINE_OFF) {
        if (!(g_sessions[idx].permissions & ICCOA_PERM_ENGINE)) {
            rsp_payload[0] = 0x02;
            *rsp_len = 1;
            return ICCOA_OK;
        }
    }

    /* 执行控制命令 */
    int32_t ret = iccoa_ctrl_execute(cmd, param);

    rsp_payload[0] = (ret == ICCOA_OK) ? 0x00 : 0x01;
    *rsp_len = 1;

    return ICCOA_OK;
}

/**
 * @brief 处理 UWB 配置请求
 */
static int32_t handle_uwb_config(const iccoa_dk40_frame_t *req, uint8_t *rsp_payload, uint16_t *rsp_len)
{
    int idx = dk40_find_session(req->session_token);
    if (idx < 0) return ICCOA_ERR_DENIED;

    /* Payload: [uwb_config(10)] */
    if (req->payload_len < 10) return ICCOA_ERR_PARAM;

    /* 解析 UWB 配置 */
    dk40_uwb_config_t cfg = {0};
    cfg.channel = req->payload[0];
    cfg.preamble_code = req->payload[1];
    cfg.slot_duration = *(uint16_t *)(req->payload + 2);
    cfg.slots_per_round = *(uint16_t *)(req->payload + 4);
    cfg.responder_num = req->payload[6];
    cfg.ranging_mode = req->payload[7];
    cfg.distance_min = *(int16_t *)(req->payload + 8);
    cfg.distance_max = *(int16_t *)(req->payload + 10);

    /* 启动 UWB 测距 */
    int32_t ret = dk40_start_uwb_ranging(idx, &cfg);

    rsp_payload[0] = (ret == ICCOA_OK) ? 0x00 : 0x01;
    *rsp_len = 1;

    return ICCOA_OK;
}

/**
 * @brief 处理钥匙分享请求
 */
static int32_t handle_share(const iccoa_dk40_frame_t *req, uint8_t *rsp_payload, uint16_t *rsp_len)
{
    int idx = dk40_find_session(req->session_token);
    if (idx < 0) return ICCOA_ERR_DENIED;

    /* Payload: [friend_id(16)] + [permissions(1)] + [valid_hours(2)] */
    if (req->payload_len < 19) return ICCOA_ERR_PARAM;

    /* TODO: 生成分享钥匙并上传云端 */
    rsp_payload[0] = 0x00;
    *rsp_len = 1;

    return ICCOA_OK;
}

/* ========================================================================
 *  Public API
 * ======================================================================== */

int32_t iccoa_dk40_init(void)
{
    if (g_initialized) return ICCOA_OK;

    memset(g_sessions, 0, sizeof(g_sessions));
    memset(&g_uwb_cfg, 0, sizeof(g_uwb_cfg));
    memset(&g_key_ctx, 0, sizeof(g_key_ctx));

    /* 初始化主密钥 (从 SE050 加载) */
    extern int se050_key_get_master_key(uint8_t *key_out);
    se050_key_get_master_key(g_key_ctx.master_key);

    g_msg_id_counter = 0;
    g_initialized = true;

    return ICCOA_OK;
}

int32_t iccoa_dk40_process(const uint8_t *raw, uint16_t len)
{
    if (!raw || len < sizeof(iccoa_dk40_frame_t)) return ICCOA_ERR_PARAM;
    if (!g_initialized) return ICCOA_ERR_NOT_INIT;

    const iccoa_dk40_frame_t *req = (const iccoa_dk40_frame_t *)raw;

    /* 验证 Magic */
    if (req->magic != DK40_MAGIC) return ICCOA_ERR_PARAM;

    /* 验证版本 */
    if (req->version != DK40_VERSION) return ICCOA_ERR_PARAM;

    /* 验证 HMAC */
    int32_t ret = dk40_verify_hmac(req);
    if (ret != ICCOA_OK) return ret;

    /* 准备响应 */
    uint8_t rsp_payload[ICCOA_MAX_PAYLOAD];
    uint16_t rsp_len = 0;

    /* 分发处理 */
    switch (req->msg_type) {
    case ICCOA_V4_SESSION_OPEN:
        ret = handle_session_open(req, rsp_payload, &rsp_len);
        break;
    case ICCOA_V4_SESSION_CLOSE: {
        int idx = dk40_find_session(req->session_token);
        if (idx >= 0) dk40_free_session(idx);
        rsp_payload[0] = 0x00;
        rsp_len = 1;
        break;
    }
    case ICCOA_V4_BIND:
        ret = handle_bind(req, rsp_payload, &rsp_len);
        break;
    case ICCOA_V4_AUTH:
        ret = handle_auth(req, rsp_payload, &rsp_len);
        break;
    case ICCOA_V4_CTRL:
        ret = handle_ctrl(req, rsp_payload, &rsp_len);
        break;
    case ICCOA_V4_UWB_CONFIG:
        ret = handle_uwb_config(req, rsp_payload, &rsp_len);
        break;
    case ICCOA_V4_SHARE:
        ret = handle_share(req, rsp_payload, &rsp_len);
        break;
    default:
        rsp_payload[0] = 0xFF; /* 未知命令 */
        rsp_len = 1;
        break;
    }

    if (ret != ICCOA_OK) {
        rsp_payload[0] = (uint8_t)(-ret);
        rsp_len = 1;
    }

    /* 发送响应 */
    return iccoa_dk40_send_response((iccoa_v4_cmd_e)(req->msg_type | 0x80),
                                     rsp_payload, rsp_len);
}

int32_t iccoa_dk40_send_response(iccoa_v4_cmd_e cmd, const uint8_t *payload, uint16_t len)
{
    iccoa_dk40_frame_t frame = {0};

    frame.magic = DK40_MAGIC;
    frame.version = DK40_VERSION;
    frame.msg_type = (uint8_t)cmd;
    frame.msg_id = g_msg_id_counter++;
    frame.flags = DK40_FLAG_RESPONSE;
    frame.payload_len = len;

    if (len > 0 && payload) {
        memcpy(frame.payload, payload, len);
    }

    /* 计算 HMAC */
    uint16_t data_len = 12 + len;
    dk40_compute_hmac((const uint8_t *)&frame, data_len, frame.hmac);

    /* 通过 BLE 发送 */
    uint16_t total_len = sizeof(iccoa_dk40_frame_t) - ICCOA_MAX_PAYLOAD + len;
    return iccoa_ble_send((const uint8_t *)&frame, total_len);
}
