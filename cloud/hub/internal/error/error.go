// error.go - 统一错误码定义
// 三端统一错误码: Cloud / SDK / Vehicle

package error

import (
	"fmt"
)

// 错误码类别
type Category uint8

const (
	CategorySuccess   Category = 0x00
	CategoryRequest   Category = 0x01
	CategoryAuth      Category = 0x02
	CategoryKey       Category = 0x03
	CategoryVehicle   Category = 0x04
	CategoryShare     Category = 0x05
	CategoryDevice    Category = 0x06
	CategoryVendor    Category = 0x07
	CategoryTransport Category = 0x08
	CategorySystem    Category = 0x09
	CategoryTCU       Category = 0xAC
)

// 错误码
const (
	// 成功类 (0x0000-0x00FF)
	SUCCESS          ErrorCode = 0x0000
	SUCCESS_ASYNC    ErrorCode = 0x0001
	SUCCESS_PARTIAL  ErrorCode = 0x0002

	// 请求错误类 (0x0100-0x01FF)
	ERR_INVALID_REQUEST    ErrorCode = 0x0101
	ERR_INVALID_PARAMETER  ErrorCode = 0x0102
	ERR_MISSING_PARAMETER  ErrorCode = 0x0103
	ERR_INVALID_FORMAT     ErrorCode = 0x0104
	ERR_INVALID_LENGTH     ErrorCode = 0x0105
	ERR_INVALID_SIGNATURE  ErrorCode = 0x0106
	ERR_RATE_LIMIT         ErrorCode = 0x0107
	ERR_QUOTA_EXCEEDED     ErrorCode = 0x0108
	ERR_VERSION_MISMATCH   ErrorCode = 0x0109

	// 认证鉴权类 (0x0200-0x02FF)
	ERR_UNAUTHORIZED      ErrorCode = 0x0201
	ERR_INVALID_TOKEN      ErrorCode = 0x0202
	ERR_TOKEN_EXPIRED      ErrorCode = 0x0203
	ERR_FORBIDDEN         ErrorCode = 0x0204
	ERR_ACCESS_DENIED     ErrorCode = 0x0205
	ERR_USER_DISABLED     ErrorCode = 0x0206
	ERR_ACCOUNT_LOCKED    ErrorCode = 0x0207
	ERR_SESSION_EXPIRED   ErrorCode = 0x0208

	// 密钥管理类 (0x0300-0x03FF)
	ERR_KEY_NOT_FOUND              ErrorCode = 0x0301
	ERR_KEY_EXISTS                 ErrorCode = 0x0302
	ERR_KEY_EXPIRED                ErrorCode = 0x0303
	ERR_KEY_REVOKED                ErrorCode = 0x0304
	ERR_KEY_SUSPENDED              ErrorCode = 0x0305
	ERR_KEY_PENDING                ErrorCode = 0x0306
	ERR_KEY_MAX_REACHED            ErrorCode = 0x0307
	ERR_KEY_MAX_USES               ErrorCode = 0x0308
	ERR_DISTANCE_EXCEEDED          ErrorCode = 0x0309
	ERR_KEY_TYPE_NOT_ALLOWED       ErrorCode = 0x030A
	ERR_KEY_NOT_ACTIVE             ErrorCode = 0x030B
	ERR_KEY_VALIDITY_NOT_STARTED   ErrorCode = 0x030C
	ERR_KEY_BIND_FAILED            ErrorCode = 0x030D
	ERR_KEY_UNBIND_FAILED          ErrorCode = 0x030E

	// 车辆控制类 (0x0400-0x04FF)
	ERR_VEHICLE_NOT_FOUND       ErrorCode = 0x0401
	ERR_VEHICLE_OFFLINE         ErrorCode = 0x0402
	ERR_VEHICLE_NOT_BOUND       ErrorCode = 0x0403
	ERR_TCU_OFFLINE             ErrorCode = 0x0404
	ERR_TCU_TIMEOUT             ErrorCode = 0x0405
	ERR_TCU_ERROR               ErrorCode = 0x0406
	ERR_COMMAND_FAILED          ErrorCode = 0x0407
	ERR_COMMAND_TIMEOUT         ErrorCode = 0x0408
	ERR_COMMAND_REJECTED        ErrorCode = 0x0409
	ERR_COMMAND_INVALID         ErrorCode = 0x040A
	ERR_COMMAND_IN_PROGRESS     ErrorCode = 0x040B
	ERR_VEHICLE_NOT_SUPPORTED   ErrorCode = 0x040C
	ERR_BATTERY_LOW             ErrorCode = 0x040D
	ERR_ENGINE_RUNNING          ErrorCode = 0x040E
	ERR_VEHICLE_MOVING          ErrorCode = 0x040F
	ERR_DOOR_OPEN               ErrorCode = 0x0410

	// 分享业务类 (0x0500-0x05FF)
	ERR_SHARE_NOT_FOUND       ErrorCode = 0x0501
	ERR_SHARE_EXPIRED         ErrorCode = 0x0502
	ERR_SHARE_ACCEPTED        ErrorCode = 0x0503
	ERR_SHARE_CANCELLED       ErrorCode = 0x0504
	ERR_SHARE_MAX_USES        ErrorCode = 0x0505
	ERR_SHARE_NOT_ALLOWED     ErrorCode = 0x0506
	ERR_CODE_INVALID          ErrorCode = 0x0507
	ERR_CODE_EXPIRED          ErrorCode = 0x0508
	ERR_VENDOR_NOT_MATCH      ErrorCode = 0x0509
	ERR_SHARE_TO_SELF         ErrorCode = 0x050A

	// 设备硬件类 (0x0600-0x06FF)
	ERR_DEVICE_NOT_FOUND       ErrorCode = 0x0601
	ERR_DEVICE_NOT_BOUND       ErrorCode = 0x0602
	ERR_DEVICE_DISABLED        ErrorCode = 0x0603
	ERR_DEVICE_NOT_SUPPORTED   ErrorCode = 0x0604
	ERR_BLE_DISABLED           ErrorCode = 0x0605
	ERR_NFC_DISABLED           ErrorCode = 0x0606
	ERR_UWB_DISABLED           ErrorCode = 0x0607
	ERR_SE_NOT_AVAILABLE       ErrorCode = 0x0608
	ERR_SE_ERROR               ErrorCode = 0x0609
	ERR_STORAGE_FULL           ErrorCode = 0x060A
	ERR_BLE_SCAN_FAILED        ErrorCode = 0x060B
	ERR_BLE_CONNECT_FAILED     ErrorCode = 0x060C
	ERR_NFC_READ_FAILED        ErrorCode = 0x060D
	ERR_NFC_WRITE_FAILED       ErrorCode = 0x060E
	ERR_UWB_RANGING_FAILED     ErrorCode = 0x060F

	// 厂商适配类 (0x0700-0x07FF)
	ERR_VENDOR_NOT_SUPPORTED  ErrorCode = 0x0701
	ERR_ADAPTER_NOT_FOUND     ErrorCode = 0x0702
	ERR_VENDOR_API_ERROR      ErrorCode = 0x0703
	ERR_VENDOR_API_TIMEOUT   ErrorCode = 0x0704
	ERR_VENDOR_AUTH_FAILED   ErrorCode = 0x0705
	ERR_VENDOR_BIND_FAILED   ErrorCode = 0x0706
	ERR_PROTOCOL_ERROR        ErrorCode = 0x0707

	// 通信链路类 (0x0800-0x08FF)
	ERR_NETWORK_ERROR          ErrorCode = 0x0801
	ERR_NETWORK_TIMEOUT        ErrorCode = 0x0802
	ERR_SERVER_UNREACHABLE     ErrorCode = 0x0803
	ERR_CONNECTION_REFUSED     ErrorCode = 0x0804
	ERR_MQTT_DISCONNECTED      ErrorCode = 0x0805
	ERR_MQTT_PUBLISH_FAILED    ErrorCode = 0x0806
	ERR_GRPC_UNAVAILABLE       ErrorCode = 0x0807
	ERR_GRPC_DEADLINE          ErrorCode = 0x0808

	// 系统内部类 (0x0900-0x09FF)
	ERR_INTERNAL_ERROR          ErrorCode = 0x0901
	ERR_SERVICE_UNAVAILABLE    ErrorCode = 0x0902
	ERR_DATABASE_ERROR         ErrorCode = 0x0903
	ERR_CACHE_ERROR            ErrorCode = 0x0904
	ERR_QUEUE_ERROR            ErrorCode = 0x0905
	ERR_CONFIG_ERROR           ErrorCode = 0x0906
	ERR_CRYPTO_ERROR           ErrorCode = 0x0907
	ERR_SIGN_ERROR             ErrorCode = 0x0908
	ERR_VERIFY_ERROR           ErrorCode = 0x0909
	ERR_FEATURE_NOT_SUPPORTED  ErrorCode = 0x090A
	ERR_MAINTENANCE            ErrorCode = 0x090B
	ERR_CAPACITY_EXCEEDED      ErrorCode = 0x090C

	// TCU车端专用类 (0xAC00-0xACFF)
	ERR_TCU_BLE_ERROR         ErrorCode = 0xAC01
	ERR_TCU_NFC_ERROR         ErrorCode = 0xAC02
	ERR_TCU_UWB_ERROR         ErrorCode = 0xAC03
	ERR_TCU_SE_ERROR          ErrorCode = 0xAC04
	ERR_TCU_PAIRING_FAILED    ErrorCode = 0xAC05
	ERR_TCU_AUTH_FAILED       ErrorCode = 0xAC06
	ERR_TCU_CRYPTO_FAILED     ErrorCode = 0xAC07
	ERR_TCU_STORAGE_ERROR     ErrorCode = 0xAC08
	ERR_TCU_POWER_LOW         ErrorCode = 0xAC09
	ERR_TCU_WATCHDOG_RESET    ErrorCode = 0xAC0A
	ERR_TCU_ANTENNA_FAULT     ErrorCode = 0xAC0B
	ERR_TCU_MSG_QUEUE_FULL    ErrorCode = 0xAC0C
)

// ErrorCode 错误码类型
type ErrorCode uint16

// GetCategory 获取错误类别
func (e ErrorCode) GetCategory() Category {
	return Category(e >> 8)
}

// GetCode 获取具体错误码
func (e ErrorCode) GetCode() uint8 {
	return uint8(e & 0xFF)
}

// IsSuccess 是否成功
func (e ErrorCode) IsSuccess() bool {
	return e.GetCategory() == CategorySuccess
}

// String 获取错误码名称
func (e ErrorCode) String() string {
	if name, ok := errorNames[e]; ok {
		return name
	}
	return fmt.Sprintf("ERR_UNKNOWN_0x%04X", e)
}

// Error 错误接口实现
func (e ErrorCode) Error() string {
	return fmt.Sprintf("[0x%04X] %s", e, e.String())
}

// ToMap 转换为Map格式
func (e ErrorCode) ToMap() map[string]interface{} {
	return map[string]interface{}{
		"code":    e,
		"code_hex": fmt.Sprintf("0x%04X", e),
		"name":    e.String(),
		"category": e.GetCategory().String(),
	}
}

// Category String
func (c Category) String() string {
	switch c {
	case CategorySuccess:
		return "SUCCESS"
	case CategoryRequest:
		return "REQUEST"
	case CategoryAuth:
		return "AUTH"
	case CategoryKey:
		return "KEY"
	case CategoryVehicle:
		return "VEHICLE"
	case CategoryShare:
		return "SHARE"
	case CategoryDevice:
		return "DEVICE"
	case CategoryVendor:
		return "VENDOR"
	case CategoryTransport:
		return "TRANSPORT"
	case CategorySystem:
		return "SYSTEM"
	case CategoryTCU:
		return "TCU"
	default:
		return "UNKNOWN"
	}
}

// DigitalKeyError 统一错误结构
type DigitalKeyError struct {
	Code     ErrorCode
	Message  string
	Details  map[string]interface{}
	TraceID  string
	Cause    error
}

// NewError 创建错误
func NewError(code ErrorCode, message string) *DigitalKeyError {
	return &DigitalKeyError{
		Code:    code,
		Message: message,
		Details: make(map[string]interface{}),
	}
}

// NewErrorWithDetails 创建带详情的错误
func NewErrorWithDetails(code ErrorCode, message string, details map[string]interface{}) *DigitalKeyError {
	return &DigitalKeyError{
		Code:    code,
		Message: message,
		Details: details,
	}
}

// NewErrorWithCause 创建带原因的错误的
func NewErrorWithCause(code ErrorCode, message string, cause error) *DigitalKeyError {
	return &DigitalKeyError{
		Code:    code,
		Message: message,
		Details: make(map[string]interface{}),
		Cause:   cause,
	}
}

// Error 实现error接口
func (e *DigitalKeyError) Error() string {
	if e.Cause != nil {
		return fmt.Sprintf("[0x%04X] %s: %v", e.Code, e.Message, e.Cause)
	}
	return fmt.Sprintf("[0x%04X] %s", e.Code, e.Message)
}

// ToMap 转换为Map
func (e *DigitalKeyError) ToMap() map[string]interface{} {
	result := map[string]interface{}{
		"code":     e.Code,
		"code_hex": fmt.Sprintf("0x%04X", e.Code),
		"name":     e.Code.String(),
		"message":  e.Message,
		"category": e.Code.GetCategory().String(),
	}
	if e.TraceID != "" {
		result["trace_id"] = e.TraceID
	}
	if e.Details != nil {
		result["details"] = e.Details
	}
	return result
}

// WithTraceID 添加追踪ID
func (e *DigitalKeyError) WithTraceID(traceID string) *DigitalKeyError {
	e.TraceID = traceID
	return e
}

// WithDetail 添加详情
func (e *DigitalKeyError) WithDetail(key string, value interface{}) *DigitalKeyError {
	if e.Details == nil {
		e.Details = make(map[string]interface{})
	}
	e.Details[key] = value
	return e
}

// WrappedError 错误码名称映射
var errorNames = map[ErrorCode]string{
	// 成功类
	SUCCESS:         "SUCCESS",
	SUCCESS_ASYNC:   "SUCCESS_ASYNC",
	SUCCESS_PARTIAL: "SUCCESS_PARTIAL",

	// 请求错误类
	ERR_INVALID_REQUEST:   "ERR_INVALID_REQUEST",
	ERR_INVALID_PARAMETER: "ERR_INVALID_PARAMETER",
	ERR_MISSING_PARAMETER: "ERR_MISSING_PARAMETER",
	ERR_INVALID_FORMAT:    "ERR_INVALID_FORMAT",
	ERR_INVALID_LENGTH:    "ERR_INVALID_LENGTH",
	ERR_INVALID_SIGNATURE: "ERR_INVALID_SIGNATURE",
	ERR_RATE_LIMIT:        "ERR_RATE_LIMIT",
	ERR_QUOTA_EXCEEDED:    "ERR_QUOTA_EXCEEDED",
	ERR_VERSION_MISMATCH:  "ERR_VERSION_MISMATCH",

	// 认证鉴权类
	ERR_UNAUTHORIZED:     "ERR_UNAUTHORIZED",
	ERR_INVALID_TOKEN:     "ERR_INVALID_TOKEN",
	ERR_TOKEN_EXPIRED:     "ERR_TOKEN_EXPIRED",
	ERR_FORBIDDEN:         "ERR_FORBIDDEN",
	ERR_ACCESS_DENIED:     "ERR_ACCESS_DENIED",
	ERR_USER_DISABLED:     "ERR_USER_DISABLED",
	ERR_ACCOUNT_LOCKED:    "ERR_ACCOUNT_LOCKED",
	ERR_SESSION_EXPIRED:   "ERR_SESSION_EXPIRED",

	// 密钥管理类
	ERR_KEY_NOT_FOUND:             "ERR_KEY_NOT_FOUND",
	ERR_KEY_EXISTS:                "ERR_KEY_EXISTS",
	ERR_KEY_EXPIRED:               "ERR_KEY_EXPIRED",
	ERR_KEY_REVOKED:               "ERR_KEY_REVOKED",
	ERR_KEY_SUSPENDED:             "ERR_KEY_SUSPENDED",
	ERR_KEY_PENDING:               "ERR_KEY_PENDING",
	ERR_KEY_MAX_REACHED:           "ERR_KEY_MAX_REACHED",
	ERR_KEY_MAX_USES:              "ERR_KEY_MAX_USES",
	ERR_DISTANCE_EXCEEDED:         "ERR_DISTANCE_EXCEEDED",
	ERR_KEY_TYPE_NOT_ALLOWED:      "ERR_KEY_TYPE_NOT_ALLOWED",
	ERR_KEY_NOT_ACTIVE:            "ERR_KEY_NOT_ACTIVE",
	ERR_KEY_VALIDITY_NOT_STARTED:  "ERR_KEY_VALIDITY_NOT_STARTED",
	ERR_KEY_BIND_FAILED:           "ERR_KEY_BIND_FAILED",
	ERR_KEY_UNBIND_FAILED:         "ERR_KEY_UNBIND_FAILED",

	// 车辆控制类
	ERR_VEHICLE_NOT_FOUND:        "ERR_VEHICLE_NOT_FOUND",
	ERR_VEHICLE_OFFLINE:          "ERR_VEHICLE_OFFLINE",
	ERR_VEHICLE_NOT_BOUND:        "ERR_VEHICLE_NOT_BOUND",
	ERR_TCU_OFFLINE:              "ERR_TCU_OFFLINE",
	ERR_TCU_TIMEOUT:              "ERR_TCU_TIMEOUT",
	ERR_TCU_ERROR:                "ERR_TCU_ERROR",
	ERR_COMMAND_FAILED:           "ERR_COMMAND_FAILED",
	ERR_COMMAND_TIMEOUT:          "ERR_COMMAND_TIMEOUT",
	ERR_COMMAND_REJECTED:         "ERR_COMMAND_REJECTED",
	ERR_COMMAND_INVALID:          "ERR_COMMAND_INVALID",
	ERR_COMMAND_IN_PROGRESS:      "ERR_COMMAND_IN_PROGRESS",
	ERR_VEHICLE_NOT_SUPPORTED:    "ERR_VEHICLE_NOT_SUPPORTED",
	ERR_BATTERY_LOW:              "ERR_BATTERY_LOW",
	ERR_ENGINE_RUNNING:           "ERR_ENGINE_RUNNING",
	ERR_VEHICLE_MOVING:           "ERR_VEHICLE_MOVING",
	ERR_DOOR_OPEN:                "ERR_DOOR_OPEN",

	// 分享业务类
	ERR_SHARE_NOT_FOUND:    "ERR_SHARE_NOT_FOUND",
	ERR_SHARE_EXPIRED:      "ERR_SHARE_EXPIRED",
	ERR_SHARE_ACCEPTED:     "ERR_SHARE_ACCEPTED",
	ERR_SHARE_CANCELLED:    "ERR_SHARE_CANCELLED",
	ERR_SHARE_MAX_USES:     "ERR_SHARE_MAX_USES",
	ERR_SHARE_NOT_ALLOWED:  "ERR_SHARE_NOT_ALLOWED",
	ERR_CODE_INVALID:       "ERR_CODE_INVALID",
	ERR_CODE_EXPIRED:       "ERR_CODE_EXPIRED",
	ERR_VENDOR_NOT_MATCH:   "ERR_VENDOR_NOT_MATCH",
	ERR_SHARE_TO_SELF:      "ERR_SHARE_TO_SELF",

	// 设备硬件类
	ERR_DEVICE_NOT_FOUND:     "ERR_DEVICE_NOT_FOUND",
	ERR_DEVICE_NOT_BOUND:     "ERR_DEVICE_NOT_BOUND",
	ERR_DEVICE_DISABLED:      "ERR_DEVICE_DISABLED",
	ERR_DEVICE_NOT_SUPPORTED: "ERR_DEVICE_NOT_SUPPORTED",
	ERR_BLE_DISABLED:         "ERR_BLE_DISABLED",
	ERR_NFC_DISABLED:         "ERR_NFC_DISABLED",
	ERR_UWB_DISABLED:         "ERR_UWB_DISABLED",
	ERR_SE_NOT_AVAILABLE:     "ERR_SE_NOT_AVAILABLE",
	ERR_SE_ERROR:             "ERR_SE_ERROR",
	ERR_STORAGE_FULL:         "ERR_STORAGE_FULL",
	ERR_BLE_SCAN_FAILED:      "ERR_BLE_SCAN_FAILED",
	ERR_BLE_CONNECT_FAILED:   "ERR_BLE_CONNECT_FAILED",
	ERR_NFC_READ_FAILED:      "ERR_NFC_READ_FAILED",
	ERR_NFC_WRITE_FAILED:     "ERR_NFC_WRITE_FAILED",
	ERR_UWB_RANGING_FAILED:   "ERR_UWB_RANGING_FAILED",

	// 厂商适配类
	ERR_VENDOR_NOT_SUPPORTED: "ERR_VENDOR_NOT_SUPPORTED",
	ERR_ADAPTER_NOT_FOUND:    "ERR_ADAPTER_NOT_FOUND",
	ERR_VENDOR_API_ERROR:     "ERR_VENDOR_API_ERROR",
	ERR_VENDOR_API_TIMEOUT:   "ERR_VENDOR_API_TIMEOUT",
	ERR_VENDOR_AUTH_FAILED:   "ERR_VENDOR_AUTH_FAILED",
	ERR_VENDOR_BIND_FAILED:   "ERR_VENDOR_BIND_FAILED",
	ERR_PROTOCOL_ERROR:       "ERR_PROTOCOL_ERROR",

	// 通信链路类
	ERR_NETWORK_ERROR:         "ERR_NETWORK_ERROR",
	ERR_NETWORK_TIMEOUT:       "ERR_NETWORK_TIMEOUT",
	ERR_SERVER_UNREACHABLE:   "ERR_SERVER_UNREACHABLE",
	ERR_CONNECTION_REFUSED:   "ERR_CONNECTION_REFUSED",
	ERR_MQTT_DISCONNECTED:     "ERR_MQTT_DISCONNECTED",
	ERR_MQTT_PUBLISH_FAILED:  "ERR_MQTT_PUBLISH_FAILED",
	ERR_GRPC_UNAVAILABLE:     "ERR_GRPC_UNAVAILABLE",
	ERR_GRPC_DEADLINE:        "ERR_GRPC_DEADLINE",

	// 系统内部类
	ERR_INTERNAL_ERROR:         "ERR_INTERNAL_ERROR",
	ERR_SERVICE_UNAVAILABLE:   "ERR_SERVICE_UNAVAILABLE",
	ERR_DATABASE_ERROR:        "ERR_DATABASE_ERROR",
	ERR_CACHE_ERROR:            "ERR_CACHE_ERROR",
	ERR_QUEUE_ERROR:            "ERR_QUEUE_ERROR",
	ERR_CONFIG_ERROR:           "ERR_CONFIG_ERROR",
	ERR_CRYPTO_ERROR:           "ERR_CRYPTO_ERROR",
	ERR_SIGN_ERROR:             "ERR_SIGN_ERROR",
	ERR_VERIFY_ERROR:           "ERR_VERIFY_ERROR",
	ERR_FEATURE_NOT_SUPPORTED: "ERR_FEATURE_NOT_SUPPORTED",
	ERR_MAINTENANCE:            "ERR_MAINTENANCE",
	ERR_CAPACITY_EXCEEDED:     "ERR_CAPACITY_EXCEEDED",

	// TCU车端专用类
	ERR_TCU_BLE_ERROR:        "ERR_TCU_BLE_ERROR",
	ERR_TCU_NFC_ERROR:        "ERR_TCU_NFC_ERROR",
	ERR_TCU_UWB_ERROR:        "ERR_TCU_UWB_ERROR",
	ERR_TCU_SE_ERROR:         "ERR_TCU_SE_ERROR",
	ERR_TCU_PAIRING_FAILED:   "ERR_TCU_PAIRING_FAILED",
	ERR_TCU_AUTH_FAILED:      "ERR_TCU_AUTH_FAILED",
	ERR_TCU_CRYPTO_FAILED:   "ERR_TCU_CRYPTO_FAILED",
	ERR_TCU_STORAGE_ERROR:    "ERR_TCU_STORAGE_ERROR",
	ERR_TCU_POWER_LOW:        "ERR_TCU_POWER_LOW",
	ERR_TCU_WATCHDOG_RESET:  "ERR_TCU_WATCHDOG_RESET",
	ERR_TCU_ANTENNA_FAULT:   "ERR_TCU_ANTENNA_FAULT",
	ERR_TCU_MSG_QUEUE_FULL:  "ERR_TCU_MSG_QUEUE_FULL",
}

// GetErrorMessage 获取错误信息
func GetErrorMessage(code ErrorCode) string {
	return errorMessages[code]
}

// errorMessages 错误信息映射
var errorMessages = map[ErrorCode]string{
	SUCCESS:                   "成功",
	SUCCESS_ASYNC:             "异步成功，已接收",
	SUCCESS_PARTIAL:            "部分成功",
	ERR_INVALID_REQUEST:       "无效请求",
	ERR_INVALID_PARAMETER:      "参数错误",
	ERR_MISSING_PARAMETER:     "缺少参数",
	ERR_INVALID_FORMAT:        "格式错误",
	ERR_INVALID_LENGTH:         "长度错误",
	ERR_INVALID_SIGNATURE:     "签名无效",
	ERR_RATE_LIMIT:             "请求过于频繁",
	ERR_QUOTA_EXCEEDED:         "配额超限",
	ERR_VERSION_MISMATCH:       "版本不匹配",
	ERR_UNAUTHORIZED:           "未授权",
	ERR_INVALID_TOKEN:          "Token无效",
	ERR_TOKEN_EXPIRED:          "Token过期",
	ERR_FORBIDDEN:              "禁止访问",
	ERR_ACCESS_DENIED:          "权限不足",
	ERR_USER_DISABLED:          "用户已禁用",
	ERR_ACCOUNT_LOCKED:         "账户已锁定",
	ERR_SESSION_EXPIRED:        "会话已过期",
	ERR_KEY_NOT_FOUND:          "密钥不存在",
	ERR_KEY_EXISTS:             "密钥已存在",
	ERR_KEY_EXPIRED:            "密钥已过期",
	ERR_KEY_REVOKED:            "密钥已撤销",
	ERR_KEY_SUSPENDED:          "密钥已挂起",
	ERR_KEY_PENDING:            "密钥待激活",
	ERR_KEY_MAX_REACHED:        "密钥数量达上限",
	ERR_KEY_MAX_USES:           "使用次数已用完",
	ERR_DISTANCE_EXCEEDED:      "距离超出限制",
	ERR_KEY_TYPE_NOT_ALLOWED:   "密钥类型不允许",
	ERR_KEY_NOT_ACTIVE:         "密钥未激活",
	ERR_KEY_VALIDITY_NOT_STARTED: "密钥未到生效时间",
	ERR_KEY_BIND_FAILED:        "密钥绑定失败",
	ERR_KEY_UNBIND_FAILED:      "密钥解绑失败",
	ERR_VEHICLE_NOT_FOUND:      "车辆不存在",
	ERR_VEHICLE_OFFLINE:        "车辆离线",
	ERR_VEHICLE_NOT_BOUND:      "车辆未绑定",
	ERR_TCU_OFFLINE:            "TCU离线",
	ERR_TCU_TIMEOUT:            "TCU超时",
	ERR_TCU_ERROR:              "TCU内部错误",
	ERR_COMMAND_FAILED:          "指令执行失败",
	ERR_COMMAND_TIMEOUT:         "指令执行超时",
	ERR_COMMAND_REJECTED:        "指令被拒绝",
	ERR_COMMAND_INVALID:         "指令无效",
	ERR_COMMAND_IN_PROGRESS:     "指令执行中",
	ERR_VEHICLE_NOT_SUPPORTED:   "车辆不支持该操作",
	ERR_BATTERY_LOW:             "电瓶电量低",
	ERR_ENGINE_RUNNING:          "引擎运行中",
	ERR_VEHICLE_MOVING:          "车辆正在移动",
	ERR_DOOR_OPEN:               "车门未关闭",
	ERR_SHARE_NOT_FOUND:         "分享不存在",
	ERR_SHARE_EXPIRED:           "分享已过期",
	ERR_SHARE_ACCEPTED:          "分享已被接受",
	ERR_SHARE_CANCELLED:         "分享已取消",
	ERR_SHARE_MAX_USES:          "分享次数已用完",
	ERR_SHARE_NOT_ALLOWED:       "不允许分享",
	ERR_CODE_INVALID:            "分享码无效",
	ERR_CODE_EXPIRED:            "分享码过期",
	ERR_VENDOR_NOT_MATCH:        "厂商不匹配",
	ERR_SHARE_TO_SELF:           "不能分享给自己",
	ERR_DEVICE_NOT_FOUND:        "设备不存在",
	ERR_DEVICE_NOT_BOUND:        "设备未绑定",
	ERR_DEVICE_DISABLED:         "设备已禁用",
	ERR_DEVICE_NOT_SUPPORTED:    "设备不支持",
	ERR_BLE_DISABLED:            "蓝牙未开启",
	ERR_NFC_DISABLED:            "NFC未开启",
	ERR_UWB_DISABLED:            "UWB未开启",
	ERR_SE_NOT_AVAILABLE:        "安全元件不可用",
	ERR_SE_ERROR:                "SE通信错误",
	ERR_STORAGE_FULL:            "存储满",
	ERR_BLE_SCAN_FAILED:         "BLE扫描失败",
	ERR_BLE_CONNECT_FAILED:      "BLE连接失败",
	ERR_NFC_READ_FAILED:         "NFC读取失败",
	ERR_NFC_WRITE_FAILED:        "NFC写入失败",
	ERR_UWB_RANGING_FAILED:      "UWB测距失败",
	ERR_VENDOR_NOT_SUPPORTED:    "厂商不支持",
	ERR_ADAPTER_NOT_FOUND:       "适配器未找到",
	ERR_VENDOR_API_ERROR:        "厂商API错误",
	ERR_VENDOR_API_TIMEOUT:      "厂商API超时",
	ERR_VENDOR_AUTH_FAILED:      "厂商认证失败",
	ERR_VENDOR_BIND_FAILED:      "厂商绑定失败",
	ERR_PROTOCOL_ERROR:          "协议错误",
	ERR_NETWORK_ERROR:           "网络错误",
	ERR_NETWORK_TIMEOUT:         "网络超时",
	ERR_SERVER_UNREACHABLE:      "服务器不可达",
	ERR_CONNECTION_REFUSED:      "连接被拒绝",
	ERR_MQTT_DISCONNECTED:       "MQTT断开",
	ERR_MQTT_PUBLISH_FAILED:     "MQTT发布失败",
	ERR_GRPC_UNAVAILABLE:        "gRPC不可用",
	ERR_GRPC_DEADLINE:           "gRPC超时",
	ERR_INTERNAL_ERROR:          "内部错误",
	ERR_SERVICE_UNAVAILABLE:      "服务不可用",
	ERR_DATABASE_ERROR:           "数据库错误",
	ERR_CACHE_ERROR:              "缓存错误",
	ERR_QUEUE_ERROR:              "队列错误",
	ERR_CONFIG_ERROR:             "配置错误",
	ERR_CRYPTO_ERROR:             "加密错误",
	ERR_SIGN_ERROR:               "签名错误",
	ERR_VERIFY_ERROR:             "验签错误",
	ERR_FEATURE_NOT_SUPPORTED:    "功能不支持",
	ERR_MAINTENANCE:              "系统维护",
	ERR_CAPACITY_EXCEEDED:        "容量超限",
	ERR_TCU_BLE_ERROR:           "BLE通信错误",
	ERR_TCU_NFC_ERROR:           "NFC通信错误",
	ERR_TCU_UWB_ERROR:           "UWB通信错误",
	ERR_TCU_SE_ERROR:            "SE通信错误",
	ERR_TCU_PAIRING_FAILED:      "配对失败",
	ERR_TCU_AUTH_FAILED:         "认证失败",
	ERR_TCU_CRYPTO_FAILED:       "加密失败",
	ERR_TCU_STORAGE_ERROR:       "存储错误",
	ERR_TCU_POWER_LOW:           "电量低",
	ERR_TCU_WATCHDOG_RESET:      "看门狗复位",
	ERR_TCU_ANTENNA_FAULT:       "天线故障",
	ERR_TCU_MSG_QUEUE_FULL:      "消息队列满",
}
