/**
 * @file ccc_oem_app.c
 * @brief CCC Message Type 4 (Vehicle OEM App) 分发器 — 契约见 ccc_oem_app.h
 *
 * 只负责: 载荷解析、会话绑定、防重放、权限判定、结果码封装.
 * 车辆动作由外部注册的执行器完成; 未注册时一律 ERR_UNSUPPORTED_CMD.
 */

#include "ccc_oem_app.h"

#include <string.h>

/* ─── 会话内状态 (仅 sequence 跟踪) ─── */

static ccc_oem_vehicle_exec_cb s_exec_cb;
static uint32_t s_session_id;
static uint16_t s_last_seq;

/* ─── 大端读写 ─── */

static uint16_t rd_u16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }

static uint32_t rd_u32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}

static void wr_u16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }

static void wr_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}

/* ─── 响应构造 ─── */

static uint16_t respond(uint8_t *rs, uint16_t cap,
                        uint8_t cmd, uint8_t result,
                        uint32_t sid, uint16_t seq,
                        const uint8_t *body, uint16_t body_len)
{
    if (rs == NULL || cap < CCC_OEM_APP_HDR_LEN) return 0;
    if (body_len > (uint16_t)(cap - CCC_OEM_APP_HDR_LEN)) {
        body = NULL;
        body_len = 0;
    }

    rs[CCC_OEM_OFF_VERSION]      = CCC_OEM_APP_VERSION;
    rs[CCC_OEM_OFF_COMMAND]      = cmd;
    rs[CCC_OEM_OFF_FLAGS_RESULT] = result;
    rs[CCC_OEM_OFF_RESERVED]     = 0;
    wr_u32(rs + CCC_OEM_OFF_SESSION_ID, sid);
    wr_u16(rs + CCC_OEM_OFF_SEQUENCE, seq);
    wr_u16(rs + CCC_OEM_OFF_BODY_LEN, body_len);
    if (body_len > 0 && body != NULL) {
        (void)memcpy(rs + CCC_OEM_OFF_BODY, body, body_len);
    }
    return (uint16_t)(CCC_OEM_APP_HDR_LEN + body_len);
}

/* ─── 会话跟踪 ─── */

void ccc_oem_app_reset_session(void)
{
    s_session_id = CCC_OEM_SESSION_NONE;
    s_last_seq   = 0;
}

void ccc_oem_app_register_exec(ccc_oem_vehicle_exec_cb cb)
{
    s_exec_cb = cb;
}

/** 与认证快照对齐: 会话失效或切换即清空 sequence 窗口 */
static void sync_session(const ccc_auth_info_t *auth)
{
    if (!auth->authenticated || auth->session_id == CCC_OEM_SESSION_NONE) {
        s_session_id = CCC_OEM_SESSION_NONE;
        s_last_seq   = 0;
        return;
    }
    if (auth->session_id != s_session_id) {
        s_session_id = auth->session_id;
        s_last_seq   = 0;
    }
}

/* ─── 动作 -> 权限位 ─── */

static uint8_t action_required_right(uint8_t action)
{
    switch (action) {
    case CCC_OEM_ACT_LOCK:
    case CCC_OEM_ACT_UNLOCK:       return ACCESS_LOCK_UNLOCK;
    case CCC_OEM_ACT_TRUNK:        return ACCESS_TRUNK;
    case CCC_OEM_ACT_ENGINE_START: return ACCESS_ENGINE_START;
    default:                       return 0;
    }
}

/* ─── 各命令 ─── */

static uint16_t do_capability(uint8_t cmd, uint32_t sid, uint16_t seq,
                              uint8_t *rs, uint16_t cap)
{
    /* session_id/sequence 为固定取值字段, 非 0 即格式错误 */
    if (sid != CCC_OEM_SESSION_NONE || seq != CCC_OEM_SEQ_INITIAL) {
        return respond(rs, cap, cmd, CCC_OEM_ERR_MALFORMED, sid, seq, NULL, 0);
    }

    uint8_t body[CCC_OEM_CAPABILITY_BODY_LEN];
    const uint32_t bitmap = CCC_OEM_CAP_CAPABILITY | CCC_OEM_CAP_STATUS
                          | CCC_OEM_CAP_VEHICLE_CONTROL;

    body[0] = CCC_OEM_APP_VERSION;
    wr_u16(body + 1, CCC_OEM_APP_MAX_BODY);
    wr_u32(body + 3, bitmap);
    body[7] = 0;   /* features: 无应用层 MAC, 见契约完整性说明 */

    return respond(rs, cap, cmd, CCC_OEM_OK, CCC_OEM_SESSION_NONE,
                   CCC_OEM_SEQ_INITIAL, body, sizeof(body));
}

static uint16_t do_status(const ccc_auth_info_t *auth,
                          uint8_t cmd, uint32_t sid, uint16_t seq,
                          uint8_t *rs, uint16_t cap)
{
    if (!auth->authenticated) {
        return respond(rs, cap, cmd, CCC_OEM_ERR_AUTH_REQUIRED, sid, seq, NULL, 0);
    }
    if (sid != CCC_OEM_SESSION_NONE || seq != CCC_OEM_SEQ_INITIAL) {
        return respond(rs, cap, cmd, CCC_OEM_ERR_MALFORMED, sid, seq, NULL, 0);
    }

    sync_session(auth);

    uint8_t body[CCC_OEM_STATUS_BODY_LEN];
    uint16_t next_seq = (uint16_t)(s_last_seq + 1u);

    wr_u32(body, s_session_id);
    wr_u16(body + 4, next_seq);
    body[6] = 0;
    body[7] = 0;

    return respond(rs, cap, cmd, CCC_OEM_OK, CCC_OEM_SESSION_NONE,
                   CCC_OEM_SEQ_INITIAL, body, sizeof(body));
}

static uint16_t do_vehicle_control(const ccc_auth_info_t *auth,
                                   const uint8_t *body, uint16_t body_len,
                                   uint32_t sid, uint16_t seq,
                                   uint8_t *rs, uint16_t cap)
{
    const uint8_t cmd = CCC_OEM_CMD_VEHICLE_CONTROL;

    if (!auth->authenticated) {
        return respond(rs, cap, cmd, CCC_OEM_ERR_AUTH_REQUIRED, sid, seq, NULL, 0);
    }

    sync_session(auth);

    /* 会话绑定: 认证后必须存在有效 session_id, 且与请求一致 */
    if (s_session_id == CCC_OEM_SESSION_NONE || sid != s_session_id) {
        return respond(rs, cap, cmd, CCC_OEM_ERR_SESSION, sid, seq, NULL, 0);
    }
    /* 防重放: 严格递增, 0 非法, 回绕视同重放 */
    if (seq == 0 || seq <= s_last_seq) {
        return respond(rs, cap, cmd, CCC_OEM_ERR_REPLAY, sid, seq, NULL, 0);
    }
    if (body_len != CCC_OEM_ACT_BODY_LEN) {
        return respond(rs, cap, cmd, CCC_OEM_ERR_MALFORMED, sid, seq, NULL, 0);
    }

    const uint8_t action = body[0];
    const uint8_t need   = action_required_right(action);
    if (need == 0) {
        return respond(rs, cap, cmd, CCC_OEM_ERR_UNKNOWN_COMMAND, sid, seq, NULL, 0);
    }

    /* 权限: 钥匙未绑定 = 权限不可判定, 一律拒绝 */
    if (!auth->key_bound || (auth->access_rights[0] & need) == 0) {
        return respond(rs, cap, cmd, CCC_OEM_ERR_PERMISSION_DENIED, sid, seq, NULL, 0);
    }
    if (s_exec_cb == NULL) {
        return respond(rs, cap, cmd, CCC_OEM_ERR_UNSUPPORTED_CMD, sid, seq, NULL, 0);
    }

    /* 校验全部通过, 接受该序号后再执行; 执行失败也不允许同序号重试 */
    s_last_seq = seq;

    ccc_status_t st = s_exec_cb(action, NULL, 0);
    return respond(rs, cap, cmd,
                   (st == CCC_OK) ? CCC_OEM_OK : CCC_OEM_ERR_EXEC_FAILED,
                   sid, seq, NULL, 0);
}

/* ─── 入口 ─── */

uint16_t ccc_oem_app_handle(const ccc_auth_info_t *auth,
                            const uint8_t *rq, uint16_t rq_len,
                            uint8_t *rs, uint16_t rs_cap)
{
    if (rs == NULL || rs_cap < CCC_OEM_APP_HDR_LEN) return 0;

    if (auth == NULL || rq == NULL || rq_len < CCC_OEM_APP_HDR_LEN) {
        return respond(rs, rs_cap, 0, CCC_OEM_ERR_MALFORMED,
                       CCC_OEM_SESSION_NONE, CCC_OEM_SEQ_INITIAL, NULL, 0);
    }

    const uint8_t  version = rq[CCC_OEM_OFF_VERSION];
    const uint8_t  cmd     = rq[CCC_OEM_OFF_COMMAND];
    const uint8_t  flags   = rq[CCC_OEM_OFF_FLAGS_RESULT];
    const uint8_t  rsvd    = rq[CCC_OEM_OFF_RESERVED];
    const uint32_t sid     = rd_u32(rq + CCC_OEM_OFF_SESSION_ID);
    const uint16_t seq     = rd_u16(rq + CCC_OEM_OFF_SEQUENCE);
    const uint16_t blen    = rd_u16(rq + CCC_OEM_OFF_BODY_LEN);
    const uint8_t *body    = rq + CCC_OEM_OFF_BODY;

    if (version != CCC_OEM_APP_VERSION) {
        return respond(rs, rs_cap, cmd, CCC_OEM_ERR_UNSUPPORTED_VERSION,
                       sid, seq, NULL, 0);
    }
    if (flags != 0 || rsvd != 0) {
        return respond(rs, rs_cap, cmd, CCC_OEM_ERR_MALFORMED, sid, seq, NULL, 0);
    }
    if (rq_len > (uint16_t)(CCC_OEM_APP_HDR_LEN + CCC_OEM_APP_MAX_BODY)) {
        return respond(rs, rs_cap, cmd, CCC_OEM_ERR_PAYLOAD_TOO_LARGE,
                       sid, seq, NULL, 0);
    }
    if (blen != (uint16_t)(rq_len - CCC_OEM_APP_HDR_LEN)) {
        return respond(rs, rs_cap, cmd, CCC_OEM_ERR_MALFORMED, sid, seq, NULL, 0);
    }

    switch (cmd) {
    case CCC_OEM_CMD_CAPABILITY:
        return do_capability(cmd, sid, seq, rs, rs_cap);
    case CCC_OEM_CMD_STATUS:
        return do_status(auth, cmd, sid, seq, rs, rs_cap);
    case CCC_OEM_CMD_VEHICLE_CONTROL:
        return do_vehicle_control(auth, body, blen, sid, seq, rs, rs_cap);
    default:
        return respond(rs, rs_cap, cmd, CCC_OEM_ERR_UNKNOWN_COMMAND,
                       sid, seq, NULL, 0);
    }
}
