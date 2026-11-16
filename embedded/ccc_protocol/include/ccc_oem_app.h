/**
 * @file ccc_oem_app.h
 * @brief CCC v4.0 Message Type 4 (Vehicle OEM App) — OEM 子协议契约 v1
 *
 * 定位
 * ----
 * CCC-TS-101 v4.0 Table 19-21 将 Message Type 4 保留为 Vehicle OEM App:
 * 载荷完全由 OEM 自定义, 不属于 CCC 一致性测试范围. 本文件是该载荷的
 * **唯一契约来源**; 任何车控指令不得借用 SE 通道的私有 INS (如 0xD2).
 *
 * 封装 (复用既有 DK 帧, 见 ccc_digital_key.h)
 * ----
 *   [0]    header      msg_type = CCC_MSG_TYPE_VEHICLE_OEM_APP(4), RFU=0
 *   [1]    message_id  CCC_OEM_MSG_ID_RQ / CCC_OEM_MSG_ID_RS
 *   [2-3]  length      载荷长度 (大端), 必须与实际载荷精确一致
 *   [4...] payload     本文件定义的 OEM 载荷
 *
 * OEM 载荷 (请求与响应同构, 12 字节头 + 可选 body)
 * ----
 *   off  size  字段
 *   0    1     version      协议版本, 当前 0x01; 不一致即拒绝
 *   1    1     command      请求=命令字; 响应=原命令字回显
 *   2    1     flags/result 请求=flags, 必须全 0; 响应=ccc_oem_result_e
 *   3    1     reserved     必须全 0 (双向)
 *   4    4     session_id   大端; 认证会话标识, 见下方会话规则
 *   8    2     sequence     大端; 会话内严格递增
 *   10   2     body_len     大端; 必须 == payload_len - 12
 *   12   N     body
 *
 * 会话与防重放规则
 * ----
 *   session_id 由车端在完整认证通过时以 TRNG 生成, 经 ccc_auth_info_t 暴露;
 *   手机通过 STATUS 命令读取. 断开连接/重新认证即作废.
 *
 *   命令            需要认证  session_id 要求        sequence 要求
 *   CAPABILITY      否        必须 0                必须 0
 *   STATUS          是        请求必须为 0           必须 0
 *   VEHICLE_CONTROL 是        必须等于当前 session_id 必须 > 上一已接受值
 *
 *   响应恒回显请求中的 session_id/sequence, 便于手机匹配 outstanding 请求.
 *
 * 权限
 * ----
 *   VEHICLE_CONTROL 除认证外, 还要求 ccc_auth_info_t.key_bound == true,
 *   且 action 对应的 ACCESS_* 位在 access_rights 中置位.
 *   key_bound == false (多钥匙歧义) 时一律 ERR_PERMISSION_DENIED.
 *
 * 完整性说明 (v1 的明确边界)
 * ----
 *   v1 不含应用层 MAC: 依赖 BLE LE Secure Connections 链路加密 +
 *   session_id/sequence 会话绑定. 应用层 CMAC 需要会话密钥, 而当前
 *   INTERNAL AUTHENTICATE 只做签名验证、未建立共享密钥; 在 sec 层提供
 *   ECDH 之前不得声称具备应用层完整性. 升级路径: version=0x02 引入
 *   SESSION_BEGIN 握手与 CMAC, 届时本文件同步修订.
 *
 * 通用 fail-closed 不变式
 * ----
 *   1. version 不符              -> ERR_UNSUPPORTED_VERSION, 不执行
 *   2. flags/reserved 非 0       -> ERR_MALFORMED
 *   3. body_len 与实际不符/超长  -> ERR_MALFORMED / ERR_PAYLOAD_TOO_LARGE
 *   4. 未知 command / message_id -> ERR_UNKNOWN_COMMAND, 不执行
 *   5. 未认证访问受保护命令      -> ERR_AUTH_REQUIRED
 *   6. session_id 不符           -> ERR_SESSION (不执行, 不改变状态)
 *   7. sequence 非递增           -> ERR_REPLAY
 *   8. 权限不足 / 钥匙未绑定     -> ERR_PERMISSION_DENIED
 *   9. 未注册执行器              -> ERR_UNSUPPORTED_COMMAND (不假成功)
 *  10. 任何错误路径均不得触发车控执行器, 不得改变车辆状态.
 */

#ifndef CCC_OEM_APP_H
#define CCC_OEM_APP_H

#include <stdint.h>
#include "ccc_digital_key.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ─── 封装层 ─── */

#define CCC_OEM_MSG_TYPE        CCC_MSG_TYPE_VEHICLE_OEM_APP  /* = 4 */
#define CCC_OEM_MSG_ID_RQ       0x01u
#define CCC_OEM_MSG_ID_RS       0x02u
/* 其余 message_id 全部保留, 收到即拒绝 */

/* ─── 载荷布局 ─── */

#define CCC_OEM_APP_VERSION        0x01u

#define CCC_OEM_OFF_VERSION        0u
#define CCC_OEM_OFF_COMMAND        1u
#define CCC_OEM_OFF_FLAGS_RESULT   2u
#define CCC_OEM_OFF_RESERVED       3u
#define CCC_OEM_OFF_SESSION_ID     4u
#define CCC_OEM_OFF_SEQUENCE       8u
#define CCC_OEM_OFF_BODY_LEN       10u
#define CCC_OEM_OFF_BODY           12u

#define CCC_OEM_APP_HDR_LEN        12u
/** 单帧载荷上限与 APDU 路径一致: 235B, 减去 12B 头 */
#define CCC_OEM_APP_MAX_BODY       (235u - CCC_OEM_APP_HDR_LEN)   /* 223 */

#define CCC_OEM_SESSION_NONE       0u   /* 未建立会话时的 session_id */
#define CCC_OEM_SEQ_INITIAL        0u   /* CAPABILITY/STATUS 请求的 sequence */

/* ─── 命令字 ─── */

typedef enum {
    CCC_OEM_CMD_CAPABILITY      = 0x01,  /**< 能力查询 (免认证) */
    CCC_OEM_CMD_STATUS          = 0x02,  /**< 会话参数查询 (需认证) */
    CCC_OEM_CMD_VEHICLE_CONTROL = 0x03   /**< 车辆控制 (需认证+权限) */
    /* 新增命令必须同步更新 CAPABILITY 位图与本文件的权限表 */
} ccc_oem_cmd_e;

/** CAPABILITY 响应中的命令位图 */
#define CCC_OEM_CAP_CAPABILITY      (1u << 0)
#define CCC_OEM_CAP_STATUS          (1u << 1)
#define CCC_OEM_CAP_VEHICLE_CONTROL (1u << 2)

/* ─── 车辆动作 (VEHICLE_CONTROL body 第 1 字节) ─── */

typedef enum {
    CCC_OEM_ACT_LOCK         = 0x01,  /**< 需要 ACCESS_LOCK_UNLOCK */
    CCC_OEM_ACT_UNLOCK       = 0x02,  /**< 需要 ACCESS_LOCK_UNLOCK */
    CCC_OEM_ACT_TRUNK        = 0x03,  /**< 需要 ACCESS_TRUNK */
    CCC_OEM_ACT_ENGINE_START = 0x04   /**< 需要 ACCESS_ENGINE_START */
} ccc_oem_action_e;

/** VEHICLE_CONTROL body: action(1) + params(N), 当前 params 长度必须为 0 */
#define CCC_OEM_ACT_BODY_LEN   1u

/* ─── 结果码 (响应 offset 2) ─── */

typedef enum {
    CCC_OEM_OK                    = 0x00,
    CCC_OEM_ERR_UNSUPPORTED_VERSION = 0x01,
    CCC_OEM_ERR_UNKNOWN_COMMAND   = 0x02,
    CCC_OEM_ERR_MALFORMED         = 0x03,
    CCC_OEM_ERR_AUTH_REQUIRED     = 0x04,
    CCC_OEM_ERR_PERMISSION_DENIED = 0x05,
    CCC_OEM_ERR_REPLAY            = 0x06,
    CCC_OEM_ERR_EXEC_FAILED       = 0x07,
    CCC_OEM_ERR_PAYLOAD_TOO_LARGE = 0x08,
    CCC_OEM_ERR_SESSION           = 0x09,
    CCC_OEM_ERR_UNSUPPORTED_CMD   = 0x0A   /**< 已定义但未注册执行器 */
} ccc_oem_result_e;

/* ─── 标准响应 body ─── */

/** CAPABILITY: version(1) + max_body(2,BE) + cmd_bitmap(4,BE) + features(1,=0) */
#define CCC_OEM_CAPABILITY_BODY_LEN  8u
/** STATUS: session_id(4,BE) + next_sequence(2,BE) + reserved(2,=0) */
#define CCC_OEM_STATUS_BODY_LEN      8u

/* ─── 实现接口契约 (本版仅声明, 实现落地前不得注册到消息路由) ─── */

/**
 * 处理一个 OEM App 载荷, 产出响应载荷.
 * @param rq      请求载荷 (不含 DK 帧头)
 * @param rq_len  请求载荷长度
 * @param rs      响应缓冲
 * @param rs_cap  缓冲容量, 至少 CCC_OEM_APP_HDR_LEN + CCC_OEM_APP_MAX_BODY
 * @return 响应载荷长度; 永不为 0 (错误也以结果码形式返回)
 *
 * 约定: 任何输入都必须产生合法响应, 不得断言/越界/部分执行.
 */
uint16_t ccc_oem_app_handle(const uint8_t *rq, uint16_t rq_len,
                            uint8_t *rs, uint16_t rs_cap);

/** 会话复位: 断开连接/重新认证时由 core 调用, 清空 sequence 跟踪 */
void ccc_oem_app_reset_session(void);

/** 车控执行器回调; 返回 CCC_OK 表示动作已完成, 其余一律映射为 ERR_EXEC_FAILED */
typedef ccc_status_t (*ccc_oem_vehicle_exec_cb)(uint8_t action,
                                                const uint8_t *params,
                                                uint16_t len);
void ccc_oem_app_register_exec(ccc_oem_vehicle_exec_cb cb);

#ifdef __cplusplus
}
#endif

#endif /* CCC_OEM_APP_H */
