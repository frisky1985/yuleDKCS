/**
 * @file dk_unified.h
 * @brief Digital Key Unified Protocol Interface
 * @version 1.0
 * @date 2026-05-08
 *
 * 统一接口层 - 整合 CCC / ICCOA / ICCE 三套协议
 * 设计原则：
 *   1. 设备类型驱动协议选择
 *   2. 统一API，内部路由到具体协议实现
 *   3. 能力配置动态决定支持的功能集
 *   4. 状态机统一管理连接、认证、定位
 */

#ifndef DK_UNIFIED_H
#define DK_UNIFIED_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ========================================================================
 *  统一返回码
 * ======================================================================== */
#define DK_OK                   (0)
#define DK_ERR_INVALID_PARAM    (-1)
#define DK_ERR_NO_MEM           (-2)
#define DK_ERR_TIMEOUT          (-3)
#define DK_ERR_BUSY             (-4)
#define DK_ERR_NOT_INIT         (-5)
#define DK_ERR_HARDWARE         (-6)
#define DK_ERR_SECURITY         (-7)
#define DK_ERR_NOT_FOUND        (-8)
#define DK_ERR_ALREADY_EXISTS   (-9)
#define DK_ERR_DENIED           (-10)
#define DK_ERR_UNSUPPORTED      (-11)   /* 协议不支持该操作 */
#define DK_ERR_PROTOCOL         (-12)   /* 协议层错误 */

typedef int32_t dk_status_t;

/* ========================================================================
 *  协议类型定义
 * ======================================================================== */
typedef enum {
    DK_PROTOCOL_CCC    = 0x01,   /* CCC Digital Key 3.0 */
    DK_PROTOCOL_ICCOA  = 0x02,   /* ICCOA DK 3.0/4.0 */
    DK_PROTOCOL_ICCE   = 0x03    /* ICCE Edge Computing */
} dk_protocol_e;

typedef enum {
    DK_VERSION_CCC_30       = 0x0300,   /* CCC 3.0 */
    DK_VERSION_ICCOA_30     = 0x0300,   /* ICCOA DK 3.0 */
    DK_VERSION_ICCOA_40     = 0x0400,   /* ICCOA DK 4.0 */
    DK_VERSION_ICCE_10      = 0x0100    /* ICCE 1.0 */
} dk_protocol_version_e;

/* ========================================================================
 *  设备类型定义 - 核心参数
 * ======================================================================== */
typedef enum {
    DK_DEVICE_SMARTPHONE    = 0x01,   /* 智能手机 */
    DK_DEVICE_KEY_FOB       = 0x02,   /* 智能钥匙 */
    DK_DEVICE_CARD          = 0x03,   /* NFC 卡片 */
    DK_DEVICE_TABLET        = 0x04,   /* 平板电脑 */
    DK_DEVICE_WEARABLE      = 0x05,   /* 可穿戴设备 */
    DK_DEVICE_VEHICLE_TCU   = 0x10,   /* 车端 TCU */
    DK_DEVICE_VEHICLE_CE    = 0x11    /* 车端 CE 模块 */
} dk_device_type_e;

/* ========================================================================
 *  设备能力配置
 * ======================================================================== */
#define DK_CAP_NFC             (1 << 0)   /* 支持 NFC */
#define DK_CAP_BLE             (1 << 1)   /* 支持 BLE */
#define DK_CAP_UWB             (1 << 2)   /* 支持 UWB */
#define DK_CAP_UWB_ANGULAR     (1 << 3)   /* UWB 支持到达角 */
#define DK_CAP_SE              (1 << 4)   /* 安全元件 */
#define DK_CAP_LPCD            (1 << 5)   /* NFC 低功耗检测 */
#define DK_CAP_BLE_Coded       (1 << 6)   /* BLE Coded PHY (长距离) */
#define DK_CAP_BLE_2M          (1 << 7)   /* BLE 2M PHY */
#define DK_CAP_EDGE_COMPUTE    (1 << 8)   /* 边缘计算能力 */
#define DK_CAP_REMOTE_CTRL     (1 << 9)   /* 远程控制 */
#define DK_CAP_KEY_SHARE       (1 << 10)  /* 钥匙分享 */
#define DK_CAP_MULTI_KEY       (1 << 11)  /* 多钥匙支持 */

typedef struct {
    uint32_t capabilities;          /* 能力位掩码 */
    uint8_t  max_keys;              /* 最大钥匙数 */
    uint8_t  max_sessions;          /* 最大并发会话数 */
    uint8_t  nfc_channels;          /* NFC 通道数 */
    uint8_t  uwb_channels;          /* UWB 通道数 */
    uint16_t ble_mtu;               /* BLE MTU */
    uint16_t uwb_max_range_cm;      /* UWB 最大测距范围 */
    uint8_t  reserved[8];           /* 保留 */
} dk_capabilities_t;

/* ========================================================================
 *  设备类型结构 - 主参数
 * ======================================================================== */
typedef struct {
    /* 基本信息 */
    uint8_t           device_id[16];        /* 设备唯一标识 */
    dk_device_type_e  device_type;          /* 设备类型 */
    dk_protocol_e     protocol;             /* 协议类型 */
    uint16_t          protocol_version;     /* 协议版本 */
    
    /* 能力配置 */
    dk_capabilities_t capabilities;
    
    /* 硬件信息 */
    uint8_t           ble_mac[6];           /* BLE MAC 地址 */
    uint8_t           uwb_id[8];            /* UWB 设备 ID */
    uint8_t           nfc_uid[10];          /* NFC UID */
    uint8_t           se_id[16];            /* 安全元件 ID */
    
    /* 厂商扩展 */
    uint8_t           vendor_id[4];         /* 厂商 ID */
    uint8_t           product_id[4];        /* 产品 ID */
    uint8_t           firmware_ver[4];      /* 固件版本 */
    uint8_t           reserved[16];         /* 保留 */
} dk_device_type_t;

/* ========================================================================
 *  连接状态
 * ======================================================================== */
typedef enum {
    DK_CONN_DISCONNECTED   = 0x00,   /* 未连接 */
    DK_CONN_NFC_FIELD      = 0x01,   /* NFC 场检测 */
    DK_CONN_NFC_ACTIVE     = 0x02,   /* NFC 激活 */
    DK_CONN_BLE_ADV        = 0x10,   /* BLE 广播中 */
    DK_CONN_BLE_CONNECTING = 0x11,   /* BLE 连接中 */
    DK_CONN_BLE_CONNECTED  = 0x12,   /* BLE 已连接 */
    DK_CONN_UWB_IDLE       = 0x20,   /* UWB 空闲 */
    DK_CONN_UWB_RANGING    = 0x21,   /* UWB 测距中 */
    DK_CONN_UWB_ACTIVE     = 0x22    /* UWB 激活 */
} dk_conn_state_e;

typedef struct {
    dk_conn_state_e  nfc_state;
    dk_conn_state_e  ble_state;
    dk_conn_state_e  uwb_state;
    uint16_t         ble_conn_handle;     /* BLE 连接句柄 */
    uint32_t         uwb_session_id;      /* UWB 会话 ID */
    int8_t           ble_rssi;            /* BLE RSSI */
    uint16_t         ble_mtu;             /* 协商后的 MTU */
    uint32_t         conn_duration_ms;    /* 连接持续时间 */
} dk_conn_status_t;

/* ========================================================================
 *  认证状态
 * ======================================================================== */
typedef enum {
    DK_AUTH_NONE        = 0x00,   /* 未认证 */
    DK_AUTH_BINDING     = 0x01,   /* 绑定中 */
    DK_AUTH_BOUND       = 0x02,   /* 已绑定 */
    DK_AUTH_CHALLENGING = 0x03,   /* 挑战验证中 */
    DK_AUTH_VERIFIED    = 0x04,   /* 已验证 */
    DK_AUTH_EXPIRED     = 0x05,   /* 认证过期 */
    DK_AUTH_FAILED      = 0x06    /* 认证失败 */
} dk_auth_state_e;

typedef struct {
    dk_auth_state_e  state;
    uint8_t          key_id[16];          /* 当前钥匙 ID */
    uint8_t          key_type;            /* 钥匙类型 */
    uint8_t          access_rights[4];    /* 访问权限 */
    uint32_t         auth_time;           /* 认证时间戳 */
    uint32_t         expire_time;         /* 过期时间 */
    uint8_t          auth_level;          /* 认证级别 */
    uint8_t          reserved[7];
} dk_auth_status_t;

/* ========================================================================
 *  定位数据
 * ======================================================================== */
typedef enum {
    DK_ZONE_LOCKED    = 0,   /* 锁定区 (>10m) */
    DK_ZONE_APPROACH  = 1,   /* 接近区 (5-10m) */
    DK_ZONE_UNLOCK    = 2,   /* 解锁区 (2-5m) */
    DK_ZONE_ENTRY     = 3,   /* 进入区 (0-2m) */
    DK_ZONE_INSIDE    = 4,   /* 车内 (<0.5m) */
    DK_ZONE_UNKNOWN   = 0xFF /* 未知 */
} dk_zone_e;

typedef struct {
    uint32_t    distance_mm;         /* 距离 (毫米) */
    int16_t     angle_azimuth;       /* 方位角 (0.1度) */
    int16_t     angle_elevation;     /* 仰角 (0.1度) */
    dk_zone_e   zone;                /* 区域 */
    uint8_t     quality;             /* 质量 0-100 */
    uint8_t     method;              /* 定位方式: 0=UWB, 1=BLE RSSI, 2=NFC */
    int8_t      rssi;                /* BLE RSSI */
    uint8_t     reserved[5];
} dk_location_t;

/* ========================================================================
 *  综合状态字
 * ======================================================================== */
typedef struct {
    dk_device_type_t   device;        /* 设备类型 */
    dk_conn_status_t   conn;          /* 连接状态 */
    dk_auth_status_t   auth;          /* 认证状态 */
    dk_location_t      location;      /* 定位数据 */
    uint32_t           timestamp;     /* 状态时间戳 */
    uint32_t           flags;         /* 状态标志位 */
} dk_device_status_t;

/* 状态标志位定义 */
#define DK_FLAG_LOCKED        (1 << 0)   /* 车辆锁定 */
#define DK_FLAG_ENGINE_RUN    (1 << 1)   /* 引擎运行 */
#define DK_FLAG_ALARM_ACTIVE  (1 << 2)   /* 报警激活 */
#define DK_FLAG_LOW_BATTERY   (1 << 3)   /* 低电量 */
#define DK_FLAG_KEY_PRESENT   (1 << 4)   /* 钥匙在场 */
#define DK_FLAG_IN_VEHICLE    (1 << 5)   /* 钥匙在车内 */

/* ========================================================================
 *  钥匙管理
 * ======================================================================== */
typedef enum {
    DK_KEY_OWNER     = 0x01,   /* 车主钥匙 */
    DK_KEY_FRIEND    = 0x02,   /* 朋友钥匙 */
    DK_KEY_SERVICE   = 0x03,   /* 服务钥匙 */
    DK_KEY_TEMPORARY = 0x04,   /* 临时钥匙 */
    DK_KEY_VALET     = 0x05    /* 代客钥匙 */
} dk_key_type_e;

typedef enum {
    DK_KEY_STATE_INACTIVE  = 0x00,
    DK_KEY_STATE_ACTIVE    = 0x01,
    DK_KEY_STATE_SUSPENDED = 0x02,
    DK_KEY_STATE_EXPIRED   = 0x03,
    DK_KEY_STATE_REVOKED   = 0x04
} dk_key_state_e;

/* 访问权限位定义 */
#define DK_ACCESS_LOCK         (1 << 0)
#define DK_ACCESS_UNLOCK       (1 << 1)
#define DK_ACCESS_ENGINE_START (1 << 2)
#define DK_ACCESS_TRUNK        (1 << 3)
#define DK_ACCESS_WINDOWS      (1 << 4)
#define DK_ACCESS_SUNROOF      (1 << 5)
#define DK_ACCESS_CLIMATE      (1 << 6)
#define DK_ACCESS_SEAT         (1 << 7)
#define DK_ACCESS_FUEL         (1 << 8)
#define DK_ACCESS_FIND         (1 << 9)

typedef struct {
    uint8_t      key_id[16];
    uint8_t      vehicle_id[16];
    uint8_t      owner_id[16];
    dk_key_type_e   key_type;
    dk_key_state_e  state;
    uint8_t      access_rights[4];
    uint8_t      restrictions[4];
    uint32_t     valid_from;
    uint32_t     valid_until;
    uint8_t      se_key_ref;          /* 安全元件密钥引用 */
    uint8_t      reserved[7];
} dk_key_t;

/* ========================================================================
 *  车辆控制
 * ======================================================================== */
typedef enum {
    DK_CTRL_LOCK         = 0x01,
    DK_CTRL_UNLOCK       = 0x02,
    DK_CTRL_UNLOCK_ALL   = 0x03,
    DK_CTRL_ENGINE_START = 0x04,
    DK_CTRL_ENGINE_STOP  = 0x05,
    DK_CTRL_TRUNK_OPEN   = 0x06,
    DK_CTRL_TRUNK_CLOSE  = 0x07,
    DK_CTRL_WINDOW_UP    = 0x08,
    DK_CTRL_WINDOW_DOWN  = 0x09,
    DK_CTRL_SUNROOF_OPEN = 0x0A,
    DK_CTRL_SUNROOF_CLOSE= 0x0B,
    DK_CTRL_CLIMATE_ON   = 0x0C,
    DK_CTRL_CLIMATE_OFF  = 0x0D,
    DK_CTRL_HORN         = 0x0E,
    DK_CTRL_LIGHTS       = 0x0F,
    DK_CTRL_FIND         = 0x10
} dk_ctrl_cmd_e;

typedef struct {
    uint8_t  lock_status;
    uint8_t  engine_status;
    uint8_t  door_status;
    uint8_t  window_status;
    int8_t   battery_pct;
    int16_t  interior_temp;
    uint16_t odometer_km;
    uint8_t  fuel_level;
    uint8_t  alarm_status;
    uint8_t  reserved[5];
} dk_vehicle_status_t;

/* ========================================================================
 *  回调函数类型
 * ======================================================================== */
typedef void (*dk_conn_cb_t)(const dk_device_status_t *status, void *user_data);
typedef void (*dk_auth_cb_t)(const dk_auth_status_t *auth, void *user_data);
typedef void (*dk_location_cb_t)(const dk_location_t *loc, void *user_data);
typedef void (*dk_zone_cb_t)(dk_zone_e zone, uint32_t distance_mm, void *user_data);
typedef void (*dk_vehicle_cb_t)(const dk_vehicle_status_t *status, void *user_data);

/* ========================================================================
 *  统一 API - 初始化与配置
 * ======================================================================== */

/**
 * @brief 初始化统一协议层
 * @param device 设备类型配置
 * @return 状态码
 */
dk_status_t dk_init(const dk_device_type_t *device);

/**
 * @brief 反初始化
 */
dk_status_t dk_deinit(void);

/**
 * @brief 获取当前设备状态
 */
dk_status_t dk_get_status(dk_device_status_t *status);

/**
 * @brief 主循环 tick (非阻塞)
 */
dk_status_t dk_run(void);

/* ========================================================================
 *  统一 API - 连接管理
 * ======================================================================== */

/**
 * @brief 开始 NFC 监听 (LPCD 模式)
 */
dk_status_t dk_nfc_start_listen(void);

/**
 * @brief 停止 NFC 监听
 */
dk_status_t dk_nfc_stop_listen(void);

/**
 * @brief 开始 BLE 广播
 * @param protocol 指定协议 (用于选择广播格式)
 */
dk_status_t dk_ble_start_adv(dk_protocol_e protocol);

/**
 * @brief 停止 BLE 广播
 */
dk_status_t dk_ble_stop_adv(void);

/**
 * @brief 断开 BLE 连接
 */
dk_status_t dk_ble_disconnect(void);

/**
 * @brief 开始 UWB 测距
 * @param session_id 会话 ID (0 表示自动分配)
 */
dk_status_t dk_uwb_start_ranging(uint32_t *session_id);

/**
 * @brief 停止 UWB 测距
 */
dk_status_t dk_uwb_stop_ranging(uint32_t session_id);

/**
 * @brief 注册连接状态回调
 */
dk_status_t dk_register_conn_cb(dk_conn_cb_t cb, void *user_data);

/* ========================================================================
 *  统一 API - 认证管理
 * ======================================================================== */

/**
 * @brief 开始配对绑定
 * @param protocol 使用协议
 */
dk_status_t dk_auth_bind(dk_protocol_e protocol);

/**
 * @brief 取消配对
 */
dk_status_t dk_auth_unbind(const uint8_t *key_id);

/**
 * @brief 执行认证
 */
dk_status_t dk_auth_verify(void);

/**
 * @brief 检查权限
 * @param permission 权限位 (DK_ACCESS_*)
 */
bool dk_auth_check_permission(uint32_t permission);

/**
 * @brief 注册认证状态回调
 */
dk_status_t dk_register_auth_cb(dk_auth_cb_t cb, void *user_data);

/* ========================================================================
 *  统一 API - 钥匙管理
 * ======================================================================== */

/**
 * @brief 创建钥匙
 */
dk_status_t dk_key_create(dk_key_t *key);

/**
 * @brief 删除钥匙
 */
dk_status_t dk_key_delete(const uint8_t *key_id);

/**
 * @brief 获取钥匙信息
 */
dk_status_t dk_key_get(const uint8_t *key_id, dk_key_t *key);

/**
 * @brief 列出所有钥匙
 */
dk_status_t dk_key_list(dk_key_t *keys, uint8_t *count);

/**
 * @brief 分享钥匙
 */
dk_status_t dk_key_share(const uint8_t *key_id, dk_key_type_e type, 
                         uint32_t duration_s);

/**
 * @brief 撤销钥匙
 */
dk_status_t dk_key_revoke(const uint8_t *key_id);

/**
 * @brief 暂停钥匙
 */
dk_status_t dk_key_suspend(const uint8_t *key_id);

/**
 * @brief 恢复钥匙
 */
dk_status_t dk_key_resume(const uint8_t *key_id);

/* ========================================================================
 *  统一 API - 定位与区域
 * ======================================================================== */

/**
 * @brief 获取当前位置
 */
dk_status_t dk_location_get(dk_location_t *loc);

/**
 * @brief 设置区域阈值
 */
dk_status_t dk_zone_set_threshold(uint16_t approach_cm,
                                  uint16_t unlock_cm,
                                  uint16_t entry_cm,
                                  uint16_t inside_cm);

/**
 * @brief 注册定位回调
 */
dk_status_t dk_register_location_cb(dk_location_cb_t cb, void *user_data);

/**
 * @brief 注册区域变化回调
 */
dk_status_t dk_register_zone_cb(dk_zone_cb_t cb, void *user_data);

/* ========================================================================
 *  统一 API - 车辆控制
 * ======================================================================== */

/**
 * @brief 执行车辆控制命令
 * @param cmd 控制命令
 * @param param 参数 (如温度、窗户位置等)
 */
dk_status_t dk_vehicle_ctrl(dk_ctrl_cmd_e cmd, uint8_t param);

/**
 * @brief 获取车辆状态
 */
dk_status_t dk_vehicle_get_status(dk_vehicle_status_t *status);

/**
 * @brief 注册车辆状态回调
 */
dk_status_t dk_register_vehicle_cb(dk_vehicle_cb_t cb, void *user_data);

/* ========================================================================
 *  统一 API - 协议扩展
 * ======================================================================== */

/**
 * @brief 发送协议特定数据
 * @param protocol 目标协议
 * @param data 原始数据
 * @param len 数据长度
 * @note 用于发送协议扩展命令或厂商自定义数据
 */
dk_status_t dk_protocol_send_raw(dk_protocol_e protocol, 
                                  const uint8_t *data, 
                                  uint16_t len);

/**
 * @brief 获取协议版本信息
 */
dk_status_t dk_protocol_get_info(dk_protocol_e protocol,
                                  uint16_t *version,
                                  const char **name);

#ifdef __cplusplus
}
#endif

#endif /* DK_UNIFIED_H */
